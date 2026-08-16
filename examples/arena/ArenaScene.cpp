/**
 * ArenaScene.cpp  -  Ironvale Arena
 */

#include "ArenaScene.hpp"

#include <cstdio>

namespace arena
{

namespace
{
    const std::string FONT_PATH = "assets/fonts/default.ttf";

    constexpr float VIEW_W = static_cast<float>(WINDOW_WIDTH);
    constexpr float VIEW_H = static_cast<float>(WINDOW_HEIGHT);

    const std::vector<sf::Color> SPARK_COLORS = {
        sf::Color(255, 236, 170),
        sf::Color(255, 190,  90),
        sf::Color(255, 128,  64),
        sf::Color(255, 255, 255)
    };
}

ArenaScene::ArenaScene()
    : m_camera({VIEW_W, VIEW_H}, {ARENA_W / 2.0f, ARENA_H / 2.0f})
{
    m_camera.set_bounds(sf::FloatRect{{0.0f, 0.0f}, {ARENA_W, ARENA_H}});
    m_camera.set_max_shake_offset(18.0f);
    m_camera.set_trauma_decay(1.9f);
    m_camera.set_shake_frequency(26.0f);

    build_hud();
    build_particles();
}

void ArenaScene::build_particles()
{
    // Muzzle flash: fast, short-lived, no gravity.
    ParticleSystem muzzle;
    muzzle.set_lifespan(sf::seconds(0.22f));
    muzzle.set_drag(0.02f);
    muzzle.set_fade(true);
    m_particles.add_particle_system("muzzle", std::move(muzzle));

    // Sparks from bullet impacts.
    ParticleSystem sparks;
    sparks.set_lifespan(sf::seconds(0.5f));
    sparks.set_drag(0.1f);
    sparks.set_fade(true);
    m_particles.add_particle_system("sparks", std::move(sparks));

    // Debris from kills: heavier, falls, lingers.
    ParticleSystem debris;
    debris.set_lifespan(sf::seconds(1.5f));
    debris.set_drag(0.4f);
    debris.set_fade(true);
    debris.set_gravity(sf::Vector2f{0.0f, 210.0f});
    debris.set_gravity_enabled(true);
    m_particles.add_particle_system("debris", std::move(debris));

    // Tread dust, kicked up while driving.
    ParticleSystem dust;
    dust.set_lifespan(sf::seconds(0.9f));
    dust.set_drag(0.05f);
    dust.set_fade(true);
    m_particles.add_particle_system("dust", std::move(dust));
}

void ArenaScene::build_hud()
{
    const sf::Font* font = Assets::font(FONT_PATH);
    if(!font) return;

    // Health, left. Gauges now map their range onto their boundary
    // correctly, so 0-100 spans -130 to 130 degrees as written.
    m_health_gauge = std::make_shared<Gauge>(
        sf::Vector2f{64.0f, 64.0f}, sf::Color(126, 214, 176),
        sf::Vector2f{0.0f, 100.0f}, sf::Vector2f{-130.0f, 130.0f});
    m_health_gauge->set_dial_color(sf::Color(26, 30, 44));
    m_health_gauge->set_arm_transition(TransitionFunction::EaseOutBack);
    m_health_gauge->set_arm_transition_duration(0.3f);
    m_hud.add_widget("health", m_health_gauge);

    m_heat_gauge = std::make_shared<Gauge>(
        sf::Vector2f{64.0f, 64.0f}, sf::Color(240, 176, 96),
        sf::Vector2f{0.0f, 100.0f}, sf::Vector2f{-130.0f, 130.0f});
    m_heat_gauge->set_dial_color(sf::Color(26, 30, 44));
    m_heat_gauge->set_arm_transition(TransitionFunction::EaseOutQuad);
    m_heat_gauge->set_arm_transition_duration(0.12f);
    m_hud.add_widget("heat", m_heat_gauge);

    m_score_text = std::make_shared<TextBox>(
        sf::Vector2f{220.0f, 26.0f}, sf::Color::Transparent,
        "0", *font, sf::Color(226, 230, 240));
    m_score_text->set_character_size(12);
    m_score_text->set_alignment(Alignment::RIGHT);
    m_hud.add_widget("score", m_score_text);

    m_wave_text = std::make_shared<TextBox>(
        sf::Vector2f{220.0f, 22.0f}, sf::Color::Transparent,
        "WAVE 1", *font, sf::Color(150, 160, 186));
    m_wave_text->set_character_size(12);
    m_wave_text->set_alignment(Alignment::RIGHT);
    m_hud.add_widget("wave", m_wave_text);

    m_status_text = std::make_shared<TextBox>(
        sf::Vector2f{VIEW_W, 40.0f}, sf::Color::Transparent,
        "", *font, sf::Color(255, 226, 150));
    m_status_text->set_character_size(12);
    m_status_text->set_position({0.0f, VIEW_H / 2.0f - 60.0f}, true);
    m_hud.add_widget("status", m_status_text);
}

std::shared_ptr<ParticleSystem> ArenaScene::system(const std::string& key)
{
    return m_particles.get_particle_system(key);
}

void ArenaScene::on_enter()
{
    RunState& state = run_state();
    state.begin_run();

    // A fresh callsign per run, in the style of the corpus.
    static Markov names("assets/names.txt", 3);
    if(names.is_trained()) state.callsign = names.generate_name(5, 12);

    m_tank = Tank();
    m_bullets.clear();
    m_enemies.clear();
    m_pickups.clear();

    m_dead        = false;
    m_death_timer = -1.0f;
    m_firing      = false;

    m_camera.set_center(m_tank.pos, true);
    m_camera.clear_shake();

    GameTime::resume();

    start_wave(1);

    /*
     * HUD slides in from off-screen. These tweens are marked unscaled so
     * they still run if the scene is entered while time is stopped.
     */
    if(m_health_gauge)
    {
        m_health_gauge->set_position({-80.0f, VIEW_H - 76.0f}, true);
        m_health_gauge->set_transition(TransitionFunction::EaseOutBack);
        m_health_gauge->set_transition_duration(0.5f);
        m_health_gauge->set_position({16.0f, VIEW_H - 76.0f});
    }
    if(m_heat_gauge)
    {
        m_heat_gauge->set_position({-80.0f, VIEW_H - 76.0f}, true);
        m_heat_gauge->set_transition(TransitionFunction::EaseOutBack);
        m_heat_gauge->set_transition_duration(0.6f);
        m_heat_gauge->set_position({88.0f, VIEW_H - 76.0f});
    }
    if(m_score_text)
    {
        m_score_text->set_position({VIEW_W - 236.0f, -30.0f}, true);
        m_score_text->set_transition(TransitionFunction::EaseOutBack);
        m_score_text->set_transition_duration(0.5f);
        m_score_text->set_position({VIEW_W - 236.0f, 14.0f});
    }
    if(m_wave_text)
    {
        m_wave_text->set_position({VIEW_W - 236.0f, -30.0f}, true);
        m_wave_text->set_transition(TransitionFunction::EaseOutBack);
        m_wave_text->set_transition_duration(0.62f);
        m_wave_text->set_position({VIEW_W - 236.0f, 38.0f});
    }
}

void ArenaScene::on_exit()
{
    GameTime::resume();
}

void ArenaScene::start_wave(int wave)
{
    RunState& state = run_state();
    state.wave = wave;

    m_alive_target = 3 + wave * 2;
    m_spawn_timer  = 0.0f;
    m_wave_timer   = 0.0f;

    if(m_status_text)
    {
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "WAVE %d", wave);
        m_status_text->set_string(buffer);
        m_status_text->add_text_scroll(24.0f);
    }
}

void ArenaScene::spawn_enemy()
{
    // Spawn off the edge of the visible area but inside the arena, so
    // enemies walk in rather than appearing in the player's face.
    sf::FloatRect const view = m_camera.get_viewport_bounds();

    sf::Vector2f spawn;
    for(int attempt = 0; attempt < 24; ++attempt)
    {
        spawn = { Random::real(40.0f, ARENA_W - 40.0f),
                  Random::real(40.0f, ARENA_H - 40.0f) };

        bool const off_screen =
            spawn.x < view.position.x - 20.0f ||
            spawn.y < view.position.y - 20.0f ||
            spawn.x > view.position.x + view.size.x + 20.0f ||
            spawn.y > view.position.y + view.size.y + 20.0f;

        if(off_screen && distance(spawn, m_tank.pos) > 320.0f) break;
    }

    float const difficulty = static_cast<float>(run_state().wave);
    Enemy::Kind const kind = Random::chance(0.32f) ? Enemy::Kind::RUNNER
                                                   : Enemy::Kind::DRONE;

    m_enemies.emplace_back(spawn, kind, difficulty);
}

void ArenaScene::fire()
{
    if(!m_tank.can_fire()) return;

    sf::Vector2f const dir    = m_tank.aim_direction();
    sf::Vector2f const muzzle = m_tank.muzzle_position();

    Bullet bullet;
    bullet.pos      = muzzle;
    bullet.velocity = dir * Bullet::SPEED;
    m_bullets.push_back(bullet);

    m_tank.on_fired();

    if(auto flash = system("muzzle"))
    {
        flash->set_position(muzzle);
        float const aim = m_tank.turret_degrees.get_value();
        flash->add_particles_varied(9, sf::Color(255, 226, 150),
                                    Vector{380.0f, sf::degrees(aim)}, 14.0f, 90.0f);
    }
}

void ArenaScene::kill_enemy(std::size_t index)
{
    Enemy const& enemy = m_enemies[index];

    RunState& state = run_state();
    state.score += enemy.score_value();
    state.kills += 1;

    m_camera.shake(enemy.kind == Enemy::Kind::DRONE ? 0.3f : 0.2f);

    if(auto debris = system("debris"))
    {
        debris->set_position(enemy.pos);
        for(int i = 0; i < 26; ++i)
        {
            debris->add_particles_varied(1, enemy.base_color(),
                                         Vector{Random::real(90.0f, 320.0f),
                                                Random::angle()}, 0.0f, 40.0f);
        }
    }
    if(auto sparks = system("sparks"))
    {
        sparks->set_position(enemy.pos);
        for(int i = 0; i < 14; ++i)
        {
            sparks->add_particles_varied(1, Random::pick(SPARK_COLORS),
                                         Vector{Random::real(150.0f, 460.0f),
                                                Random::angle()}, 0.0f, 60.0f);
        }
    }

    // Occasional repair pickup, so a long run stays survivable.
    if(Random::chance(0.13f))
    {
        Pickup pickup;
        pickup.pos = enemy.pos;
        m_pickups.push_back(pickup);
    }

    m_enemies.erase(m_enemies.begin() + static_cast<std::ptrdiff_t>(index));
}

void ArenaScene::handle_collisions()
{
    // Bullets against enemies.
    for(std::size_t b = 0; b < m_bullets.size(); ++b)
    {
        for(std::size_t e = 0; e < m_enemies.size(); ++e)
        {
            if(!circles_overlap(m_bullets[b].pos, Bullet::RADIUS,
                                m_enemies[e].pos, m_enemies[e].radius))
                continue;

            if(auto sparks = system("sparks"))
            {
                sparks->set_position(m_bullets[b].pos);
                sparks->add_particles_varied(6, sf::Color(255, 236, 170),
                                             Vector{200.0f, Random::angle()},
                                             180.0f, 60.0f);
            }

            m_enemies[e].health   -= 1.0f;
            m_enemies[e].hit_flash = 1.0f;

            // Knockback, so hits have weight.
            m_enemies[e].velocity += normalized(m_bullets[b].velocity) * 90.0f;

            m_bullets[b].life = 0.0f;

            if(m_enemies[e].health <= 0.0f) kill_enemy(e);
            break;
        }
    }

    if(m_dead) return;

    // Enemies against the tank.
    for(std::size_t e = 0; e < m_enemies.size(); ++e)
    {
        if(!circles_overlap(m_tank.pos, Tank::RADIUS,
                            m_enemies[e].pos, m_enemies[e].radius))
            continue;

        if(m_tank.invuln <= 0.0f)
        {
            m_tank.damage(m_enemies[e].kind == Enemy::Kind::DRONE ? 14.0f : 9.0f);
            m_camera.shake(0.45f);
        }

        kill_enemy(e);
        break;
    }

    // Pickups.
    for(std::size_t p = 0; p < m_pickups.size(); ++p)
    {
        if(!circles_overlap(m_tank.pos, Tank::RADIUS,
                            m_pickups[p].pos, Pickup::RADIUS))
            continue;

        m_tank.health = std::min(m_tank.max_health, m_tank.health + 22.0f);

        if(auto sparks = system("sparks"))
        {
            sparks->set_position(m_pickups[p].pos);
            sparks->add_particles_varied(20, COL_PICKUP,
                                         Vector{190.0f, Random::angle()},
                                         180.0f, 50.0f);
        }

        m_pickups.erase(m_pickups.begin() + static_cast<std::ptrdiff_t>(p));
        break;
    }
}

void ArenaScene::die()
{
    if(m_dead) return;

    m_dead        = true;
    m_death_timer = 1.6f;

    m_camera.shake(1.0f);

    // Slow motion on death. One call, and every tween, particle and
    // camera movement in the game slows with it.
    GameTime::set_scale(0.25f);

    if(auto debris = system("debris"))
    {
        debris->set_position(m_tank.pos);
        for(int i = 0; i < 90; ++i)
        {
            debris->add_particles_varied(1, Random::pick(SPARK_COLORS),
                                         Vector{Random::real(120.0f, 520.0f),
                                                Random::angle()}, 0.0f, 90.0f);
        }
    }

    if(m_status_text)
    {
        m_status_text->set_string("DESTROYED");
        m_status_text->add_text_scroll(18.0f);
    }
}

void ArenaScene::begin_frame()
{
    m_hud.begin_frame();
}

void ArenaScene::handle_event(const sf::Event& event, sf::Vector2f mouse_pos)
{
    // Aim through the settled camera centre, not the shaking one --
    // otherwise the crosshair drifts by however much the screen is
    // being thrown around that frame.
    if(m_manager)
    {
        sf::View aim = m_camera.get_view();
        aim.setCenter(m_camera.get_target_center());
        aim.setRotation(sf::Angle::Zero);
        m_mouse_world = m_manager->map_to_world(mouse_pos, aim);
    }

    if(const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if(pressed->button == sf::Mouse::Button::Left) m_firing = true;
    }
    else if(const auto* released = event.getIf<sf::Event::MouseButtonReleased>())
    {
        if(released->button == sf::Mouse::Button::Left) m_firing = false;
    }
    else if(const auto* key = event.getIf<sf::Event::KeyPressed>())
    {
        switch(key->code)
        {
            case sf::Keyboard::Key::Escape:
                EventBus::publish(Event(EventType::SWITCH_TO_MAIN_MENU));
                break;

            case sf::Keyboard::Key::P:
                if(!m_dead)
                {
                    GameTime::is_paused() ? GameTime::resume() : GameTime::pause();
                    if(m_status_text)
                        m_status_text->set_string(GameTime::is_paused() ? "PAUSED" : "");
                }
                break;

            default:
                break;
        }
    }
}

void ArenaScene::update(sf::Time delta_time)
{
    // Zero while paused, so everything below simply stops without a
    // single explicit pause check.
    float const dt = delta_time.asSeconds();
    if(dt <= 0.0f) return;

    m_tank.aim_at(m_mouse_world);

    if(!m_dead)
    {
        // Held keys are polled once per frame here, not per event.
        float turn = 0.0f;
        if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) ||
           sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))  turn -= 1.0f;
        if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) ||
           sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)) turn += 1.0f;

        float drive = 0.0f;
        if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) ||
           sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))    drive += 1.0f;
        if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S) ||
           sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))  drive -= 1.0f;

        m_tank.hull_angle += sf::degrees(turn * Tank::TURN_SPEED * dt);

        if(drive != 0.0f)
        {
            float const heading = m_tank.hull_angle.asDegrees();
            sf::Vector2f const forward = to_cartesian({1.0f, sf::degrees(-heading)});

            m_tank.velocity += forward * drive * Tank::DRIVE_ACCEL * dt;

            if(length(m_tank.velocity) > Tank::MAX_SPEED)
                m_tank.velocity = normalized(m_tank.velocity) * Tank::MAX_SPEED;

            // Dust from the treads, rate-limited so it does not depend
            // on framerate.
            m_dust_timer -= dt;
            if(m_dust_timer <= 0.0f)
            {
                m_dust_timer = 0.04f;
                if(auto dust = system("dust"))
                {
                    dust->set_position(m_tank.pos - forward * 18.0f);
                    dust->add_particles_varied(2, sf::Color(78, 84, 96),
                                               Vector{60.0f, Random::angle()},
                                               180.0f, 25.0f);
                }
            }
        }

        // Continuous fire is rate-limited by the tank's own cooldown,
        // rather than by how fast the OS repeats a held key.
        if(m_firing)
        {
            fire();

            /*
             * Held fire is a sustained rumble, refreshed every frame,
             * rather than a stream of impulses. Explosions still spike
             * above it and settle back down to it.
             */
            if(m_tank.can_fire() || m_tank.fire_cooldown > 0.0f)
                m_camera.set_sustained_shake(m_tank.overheated ? 0.0f : 0.26f);
        }
    }

    m_tank.update(delta_time);

    for(Bullet& bullet : m_bullets) bullet.update(delta_time);
    std::erase_if(m_bullets, [](const Bullet& b) { return b.dead(); });

    for(Enemy& enemy : m_enemies) enemy.update(delta_time, m_tank.pos);

    for(Pickup& pickup : m_pickups) pickup.update(delta_time);
    std::erase_if(m_pickups, [](const Pickup& p) { return p.dead(); });

    handle_collisions();

    if(!m_dead)
    {
        // Wave pacing: trickle enemies in until the wave's quota is met,
        // then advance once the field is clear.
        m_wave_timer += dt;

        if(m_alive_target > 0)
        {
            m_spawn_timer -= dt;
            if(m_spawn_timer <= 0.0f && m_enemies.size() < 16)
            {
                m_spawn_timer = std::max(0.25f, 1.1f - run_state().wave * 0.06f);
                spawn_enemy();
                --m_alive_target;
            }
        }
        else if(m_enemies.empty() && m_wave_timer > 1.0f)
        {
            start_wave(run_state().wave + 1);
        }

        if(m_tank.health <= 0.0f) die();
    }
    else
    {
        m_death_timer -= dt;
        if(m_death_timer <= 0.0f)
        {
            run_state().finish_run();
            GameTime::resume();
            EventBus::publish(Event(EventType::SWITCH_TO_GAME_OVER));
        }
    }

    m_particles.update(delta_time);

    /*
     * Camera follows the tank, offset toward the cursor so you can see
     * where you are shooting. Offset is capped so the tank never leaves
     * the middle of the screen.
     */
    sf::Vector2f look_ahead = (m_mouse_world - m_tank.pos) * 0.28f;
    if(length(look_ahead) > 150.0f) look_ahead = normalized(look_ahead) * 150.0f;

    m_camera.follow(m_tank.pos + look_ahead, delta_time, 4.5f);
    m_camera.update(delta_time);

    update_hud();
}

void ArenaScene::update_realtime(sf::Time delta_time)
{
    // The HUD keeps animating while paused, so the pause banner can
    // type itself out and the gauges finish settling.
    m_hud.update(delta_time);

    if(GameTime::is_paused()) m_camera.update(delta_time);
}

void ArenaScene::update_hud()
{
    RunState const& state = run_state();

    if(m_health_gauge) m_health_gauge->set_value(m_tank.health);
    if(m_heat_gauge)   m_heat_gauge->set_value(m_tank.heat);

    if(m_score_text)
    {
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "%s   %d",
                      state.callsign.c_str(), state.score);
        if(m_score_text->get_string() != buffer) m_score_text->set_string(buffer);
    }

    if(m_wave_text)
    {
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "WAVE %d   x%d", state.wave, state.kills);
        if(m_wave_text->get_string() != buffer) m_wave_text->set_string(buffer);
    }

    // Clear the wave banner a couple of seconds in.
    if(m_status_text && !m_dead && !GameTime::is_paused() &&
       m_wave_timer > 2.2f && !m_status_text->get_string().empty())
    {
        m_status_text->set_string("");
    }
}

void ArenaScene::render(sf::RenderTarget& target)
{
    // ---- world ----
    target.setView(m_camera.get_view());

    target.draw(m_floor);

    for(const Pickup& pickup : m_pickups) pickup.draw(target);
    for(const Enemy& enemy : m_enemies)   enemy.draw(target);

    if(!m_dead) m_tank.draw(target);

    for(const Bullet& bullet : m_bullets)
    {
        sf::CircleShape shape(Bullet::RADIUS);
        shape.setOrigin({Bullet::RADIUS, Bullet::RADIUS});
        shape.setPosition(bullet.pos);
        shape.setFillColor(COL_BULLET);
        target.draw(shape);
    }

    target.draw(m_particles);

    // ---- HUD ----
    // get_ui_view() keeps the HUD pinned to the screen while still
    // letting the shake reach it, so firing the cannon jolts everything.
    target.setView(m_camera.get_ui_view());
    target.draw(m_hud);
}

sf::Color ArenaScene::get_clear_color() const
{
    return sf::Color(16, 18, 26);
}

}   // namespace arena
