/**
 * GameScene.cpp  -  Void Drift
 */

#include "../../include/scenes/GameScene.hpp"

// ─── Construction ─────────────────────────────────────────────────────────────

GameScene::GameScene()
    : m_ship({(float)WINDOW_WIDTH / 2.f, (float)WINDOW_HEIGHT / 2.f}),
      m_camera(sf::Vector2f(WINDOW_WIDTH, WINDOW_HEIGHT),
               sf::Vector2f(WINDOW_WIDTH / 2.f, WINDOW_HEIGHT / 2.f)),
      m_psm(m_camera.get_position())
{}

// ─── on_enter ─────────────────────────────────────────────────────────────────

void GameScene::on_enter()
{
    if(!m_font.openFromFile("assets/fonts/Windows-Regular.ttf"))
        m_font.openFromFile("././assets/fonts/Windows-Regular.ttf");
    m_font.setSmooth(false);

    // Reset ship to centre
    m_ship = Ship({(float)WINDOW_WIDTH / 2.f, (float)WINDOW_HEIGHT / 2.f});
    m_ship.m_invincible = 2.0f;   // brief grace period on spawn

    m_asteroids.clear();
    m_bullets.clear();
    m_pickups.clear();
    m_level_cleared = false;
    m_level_clear_timer = 0.f;

    // Camera
    m_camera = Camera(sf::Vector2f(WINDOW_WIDTH, WINDOW_HEIGHT),
                      sf::Vector2f(WINDOW_WIDTH / 2.f, WINDOW_HEIGHT / 2.f));
    m_camera.set_transition(TransitionFunction::EaseInOutExponential);
    m_camera.set_transition_duration(0.05f);

    // Particles
    m_psm = ParticleSystemManager(m_camera.get_position());

    ParticleSystem expl({0.f, 0.f});
    expl.toggle_fade();
    expl.set_drag(0.88f);
    expl.set_lifespan(sf::seconds(0.9f));
    m_psm.add_particle_system("explosion", std::move(expl));

    ParticleSystem thrust({0.f, 0.f});
    thrust.toggle_fade();
    thrust.set_drag(0.82f);
    thrust.set_lifespan(sf::seconds(0.3f));
    m_psm.add_particle_system("thrust", std::move(thrust));

    // HUD text
    delete m_score_text; delete m_lives_text; delete m_level_text;

    m_score_text = new sf::Text(m_font, "0", 14);
    m_score_text->setFillColor(sf::Color(200, 240, 255));
    m_score_text->setPosition({8.f, 4.f});

    m_lives_text = new sf::Text(m_font, "", 11);
    m_lives_text->setFillColor(sf::Color(80, 220, 140));
    m_lives_text->setPosition({8.f, 22.f});

    m_level_text = new sf::Text(m_font, "LEVEL " + std::to_string(RunState::level), 10);
    m_level_text->setFillColor(sf::Color(160, 200, 255));
    auto lb = m_level_text->getLocalBounds();
    m_level_text->setOrigin({lb.size.x / 2.f, 0.f});
    m_level_text->setPosition({(float)WINDOW_WIDTH / 2.f, 4.f});

    // Gauge: ship speed
    m_widgets.add_widget("speed_gauge",
        std::make_shared<Gauge>(sf::Vector2f(44.f, 44.f),
                                sf::Color(40, 160, 220),
                                sf::Vector2f(0.f, RunState::has_perk(PerkType::GHOST_ENGINE) ? 280.f : 260.f),
                                sf::Vector2f(-130.f, 130.f)));
    auto g = m_widgets.get_widget<Gauge>("speed_gauge");
    g->set_position({(float)WINDOW_WIDTH - 52.f, (float)WINDOW_HEIGHT - 52.f}, true);
    g->set_dial_color(sf::Color(15, 50, 80));
    g->set_outline(sf::Color(60, 180, 255), 1);

    m_to_menu_handle = EventBus::subscribe(EventType::SWITCH_TO_MAIN_MENU, [](const Event&){});

    spawn_level();
}

// ─── on_exit ──────────────────────────────────────────────────────────────────

void GameScene::on_exit()
{
    EventBus::unsubscribe(EventType::SWITCH_TO_MAIN_MENU, m_to_menu_handle);
    m_asteroids.clear();
    m_bullets.clear();
    m_pickups.clear();
    m_widgets.remove_widget("speed_gauge");
    delete m_score_text; m_score_text = nullptr;
    delete m_lives_text; m_lives_text = nullptr;
    delete m_level_text; m_level_text = nullptr;
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

void GameScene::spawn_level()
{
    m_asteroids.clear();
    m_level_cleared = false;

    int count = 3 + RunState::level;   // escalating asteroid count
    count = std::min(count, 10);

    std::uniform_real_distribution<float> rx(0.f, (float)WINDOW_WIDTH);
    std::uniform_real_distribution<float> ry(0.f, (float)WINDOW_HEIGHT);
    std::uniform_real_distribution<float> rv(20.f, 50.f + RunState::level * 5.f);
    std::uniform_real_distribution<float> ra(0.f, 360.f);

    for(int i = 0; i < count; ++i)
    {
        // Spawn away from player centre
        sf::Vector2f pos;
        do {
            pos = {rx(random_gen), ry(random_gen)};
        } while(std::hypot(pos.x - WINDOW_WIDTH/2.f, pos.y - WINDOW_HEIGHT/2.f) < 80.f);

        float angle = ra(random_gen) * 3.14159f / 180.f;
        float speed = rv(random_gen);
        sf::Vector2f vel = {std::cos(angle) * speed, std::sin(angle) * speed};
        m_asteroids.emplace_back(pos, vel, AsteroidSize::LARGE);
    }
}

void GameScene::spawn_explosion(sf::Vector2f pos, sf::Color col, int count, float speed)
{
    m_psm.set_particle_system_position("explosion", pos);
    auto ps = m_psm.get_particle_system("explosion");
    ps->add_particles(count,         col,                          Vector{speed,       sf::degrees(0)}, 180);
    ps->add_particles(count / 2,     sf::Color(255, 255, 200),    Vector{speed * 0.5f, sf::degrees(0)}, 180);
    ps->add_particles(count / 3,     sf::Color(255, 255, 255, 160),Vector{speed * 1.4f, sf::degrees(0)}, 180);
}

void GameScene::fire(sf::Vector2f pos, sf::Vector2f vel)
{
    m_bullets.emplace_back(pos, vel, RunState::heavy_shell);
}

void GameScene::screen_wrap(sf::Vector2f& pos)
{
    if(pos.x < -20.f)              pos.x = WINDOW_WIDTH + 20.f;
    if(pos.x > WINDOW_WIDTH + 20.f) pos.x = -20.f;
    if(pos.y < -20.f)              pos.y = WINDOW_HEIGHT + 20.f;
    if(pos.y > WINDOW_HEIGHT + 20.f) pos.y = -20.f;
}

void GameScene::handle_collisions()
{
    bool magnet = RunState::has_perk(PerkType::GHOST_ENGINE) ? false : false; // magnet perk removed in favour of ghost engine - kept for future

    // Bullet – asteroid
    for(auto& b : m_bullets)
    {
        if(!b.alive) continue;
        for(auto& a : m_asteroids)
        {
            if(!a.alive) continue;
            if(!b.get_bounds().findIntersection(a.get_bounds())) continue;

            b.alive = false;
            a.hp--;

            bool destroyed = (a.hp <= 0) || b.heavy;
            if(destroyed)
            {
                a.alive = false;
                RunState::add_score(a.points);

                // Spawn child asteroids
                auto children = a.split();
                for(auto& c : children) m_asteroids.push_back(c);

                // Colour based on size
                sf::Color col = (a.size == AsteroidSize::LARGE)  ? sf::Color(180, 180, 200) :
                                (a.size == AsteroidSize::MEDIUM) ? sf::Color(140, 160, 200) :
                                                                    sf::Color(200, 220, 255);
                int pcount = (a.size == AsteroidSize::LARGE) ? 22 :
                             (a.size == AsteroidSize::MEDIUM) ? 14 : 8;
                spawn_explosion(a.pos, col, pcount, 3.5f);

                // Drop score pickups
                int pickup_val = a.points;
                m_pickups.emplace_back(a.pos, pickup_val);

                m_camera.shake((a.size == AsteroidSize::LARGE) ? 4 : 2);
                EventBus::publish(Event(EventType::ASTEROID_DESTROYED, std::to_string(a.points)));
            }
        }
    }

    // Ship – asteroid (only if not invincible or ghost engine active)
    if(m_ship.m_invincible <= 0.f && m_ship.m_ghost_timer <= 0.f)
    {
        for(auto& a : m_asteroids)
        {
            if(!a.alive) continue;
            if(!m_ship.get_bounds().findIntersection(a.get_bounds())) continue;

            // Destroy asteroid, damage player
            a.alive = false;
            auto children = a.split();
            for(auto& c : children) m_asteroids.push_back(c);
            spawn_explosion(a.pos, sf::Color(180, 180, 200), 16, 3.f);
            spawn_explosion(m_ship.m_pos, sf::Color(80, 200, 255), 20, 4.f);

            m_camera.shake(14);
            RunState::lives--;
            m_ship.m_invincible = 2.5f;
            m_ship.m_vel = {0.f, 0.f};

            EventBus::publish(Event(EventType::PLAYER_HIT, std::to_string(RunState::lives)));

            if(RunState::lives <= 0)
            {
                m_ship.m_alive = false;
                m_camera.shake(20);
                EventBus::publish(Event(EventType::PLAYER_DIED, std::to_string(RunState::score)));
                EventBus::publish(Event(EventType::SWITCH_TO_GAME_OVER, ""));
            }
            else
            {
                // Perk select on each death while lives remain
                EventBus::publish(Event(EventType::SWITCH_TO_PERK_SELECT, ""));
            }
            break;
        }
    }

    // Ship – pickups
    for(auto& p : m_pickups)
    {
        if(!p.alive) continue;
        sf::Vector2f diff = m_ship.m_pos - p.pos;
        if(std::hypot(diff.x, diff.y) < 12.f)
        {
            p.alive = false;
            RunState::add_score(p.value);
        }
    }
}

void GameScene::update_hud()
{
    if(m_score_text)
        m_score_text->setString(std::to_string(RunState::score));

    // Lives as ship icons (unicode diamonds as stand-in)
    if(m_lives_text)
    {
        std::string lives_str = "";
        for(int i = 0; i < RunState::lives; ++i) lives_str += "<> ";
        m_lives_text->setString(lives_str);
    }

    // Speed gauge
    float spd = std::hypot(m_ship.m_vel.x, m_ship.m_vel.y);
    m_widgets.get_widget<Gauge>("speed_gauge")->set_arm_angle_based_on_value(spd);
    for(const auto& w : m_widgets.get_widgets()) w->update(sf::seconds(0.016f));
}

// ─── process_events ───────────────────────────────────────────────────────────

void GameScene::process_events()
{
    sf::Vector2i px = sf::Mouse::getPosition(*m_render_window);
    sf::Vector2f sp = {(float)px.x / m_resolution_scale.x, (float)px.y / m_resolution_scale.y};

    while(const std::optional ev = m_render_window->pollEvent())
    {
        if(ev->is<sf::Event::Closed>()) m_render_window->close();
        if(const auto* kp = ev->getIf<sf::Event::KeyPressed>())
            if(kp->code == sf::Keyboard::Key::Escape)
                EventBus::publish(Event(EventType::SWITCH_TO_MAIN_MENU, ""));
    }

    // Shooting — hold or tap Space
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space) &&
       m_ship.m_alive &&
       m_ship.m_shoot_timer >= RunState::shoot_delay)
    {
        m_ship.m_shoot_timer = 0.f;
        float   rad   = m_ship.get_angle_rad();
        float   bspd  = 320.f;
        sf::Vector2f nose = m_ship.get_nose();
        sf::Vector2f fwd  = {std::cos(rad) * bspd, std::sin(rad) * bspd};

        if(RunState::spread_shot)
        {
            float spread = 0.22f;
            for(int i = -1; i <= 1; ++i)
            {
                float a = rad + i * spread;
                fire(nose, {std::cos(a) * bspd, std::sin(a) * bspd});
            }
        }
        else
        {
            fire(nose, fwd);
        }

        if(RunState::dual_cannons)
        {
            sf::Vector2f back = {std::cos(rad + 3.14159f) * bspd,
                                 std::sin(rad + 3.14159f) * bspd};
            fire(m_ship.m_pos, back);
        }

        m_camera.shake(1);
    }
}

// ─── update ───────────────────────────────────────────────────────────────────

void GameScene::update(sf::Time dt)
{
    float s = dt.asSeconds();

    m_stars.update(dt);

    if(m_ship.m_alive)
    {
        m_ship.update(dt);
        screen_wrap(m_ship.m_pos);

        // Ghost engine: activate when thrusting with perk
        if(RunState::ghost_engine && m_ship.m_thrusting && m_ship.m_ghost_timer <= 0.f)
            m_ship.m_ghost_timer = 0.6f;

        // Thrust particle trail
        if(m_ship.m_thrusting)
        {
            float rad = m_ship.get_angle_rad() + 3.14159f;
            sf::Vector2f tail = {m_ship.m_pos.x + std::cos(rad) * 10.f,
                                 m_ship.m_pos.y + std::sin(rad) * 10.f};
            m_psm.set_particle_system_position("thrust", tail);
            sf::Color flame_col = (m_ship.m_ghost_timer > 0.f)
                                  ? sf::Color(180, 80, 255, 200)
                                  : sf::Color(255, 140, 40, 180);
            m_psm.get_particle_system("thrust")->add_particles(
                3, flame_col, Vector{2.5f, sf::degrees(rad * 180.f / 3.14159f)}, 25);
        }
    }

    // Asteroids
    for(auto& a : m_asteroids)
    {
        if(a.alive)
        {
            a.update(dt);
            screen_wrap(a.pos);
            a.shape.setPosition(a.pos);
        }
    }

    // Bullets
    for(auto& b : m_bullets) b.update(dt);

    // Pickups
    bool magnet = false; // extend here if adding MAGNET perk
    for(auto& p : m_pickups) p.update(dt, m_ship.m_pos, magnet);

    handle_collisions();

    // Prune dead objects
    auto dead = [](const auto& x){ return !x.alive; };
    m_asteroids.erase(std::remove_if(m_asteroids.begin(), m_asteroids.end(), dead), m_asteroids.end());
    m_bullets.erase(  std::remove_if(m_bullets.begin(),   m_bullets.end(),   dead), m_bullets.end());
    m_pickups.erase(  std::remove_if(m_pickups.begin(),   m_pickups.end(),   dead), m_pickups.end());

    // Level clear
    if(m_asteroids.empty() && !m_level_cleared)
    {
        m_level_cleared     = true;
        m_level_clear_timer = 1.8f;   // pause before next level
    }
    if(m_level_cleared)
    {
        m_level_clear_timer -= s;
        if(m_level_clear_timer <= 0.f)
        {
            RunState::level++;
            spawn_level();
            m_ship.m_invincible = 1.5f;
            if(m_level_text)
            {
                m_level_text->setString("LEVEL " + std::to_string(RunState::level));
                auto lb = m_level_text->getLocalBounds();
                m_level_text->setOrigin({lb.size.x / 2.f, 0.f});
            }
        }
    }

    m_psm.update(dt);
    m_psm.sync_with_view(m_camera.get_position());
    m_camera.update();
    update_hud();
}

// ─── render ───────────────────────────────────────────────────────────────────

void GameScene::render()
{
    m_render_texture->clear(sf::Color(4, 4, 16));
    m_render_texture->setView(m_camera.get_view());

    m_stars.draw(*m_render_texture);

    // Pickups
    for(const auto& p : m_pickups)
        if(p.alive) m_render_texture->draw(p.shape);

    // Asteroids
    for(const auto& a : m_asteroids)
        if(a.alive) m_render_texture->draw(a.shape);

    // Bullets
    for(const auto& b : m_bullets)
        if(b.alive) m_render_texture->draw(b.shape);

    // Ship
    m_ship.draw(*m_render_texture);

    // Particles
    m_render_texture->draw(m_psm);

    // ── HUD (fixed view) ──────────────────────────────────────────────────────
    m_render_texture->setView(m_render_texture->getDefaultView());

    if(m_score_text) m_render_texture->draw(*m_score_text);
    if(m_lives_text) m_render_texture->draw(*m_lives_text);
    if(m_level_text) m_render_texture->draw(*m_level_text);

    // Level clear banner
    if(m_level_cleared && m_level_clear_timer > 0.f)
    {
        sf::Text banner(m_font, "LEVEL CLEAR!", 18);
        banner.setFillColor(sf::Color(80, 255, 160));
        banner.setOutlineColor(sf::Color(0, 80, 40));
        banner.setOutlineThickness(1.f);
        auto bb = banner.getLocalBounds();
        banner.setOrigin({bb.size.x / 2.f, bb.size.y / 2.f});
        banner.setPosition({(float)WINDOW_WIDTH / 2.f, (float)WINDOW_HEIGHT / 2.f});
        m_render_texture->draw(banner);
    }

    // Perks row (icons at bottom-left)
    {
        float x = 8.f;
        float y = (float)WINDOW_HEIGHT - 14.f;
        for(const auto& pt : RunState::perks)
        {
            const PerkDef* def = nullptr;
            for(const auto& pd : ALL_PERKS)
                if(pd.type == pt) { def = &pd; break; }
            if(!def) continue;

            sf::RectangleShape pip({20.f, 8.f});
            pip.setFillColor(sf::Color(def->color_r, def->color_g, def->color_b, 180));
            pip.setOutlineColor(sf::Color(255, 255, 255, 60));
            pip.setOutlineThickness(0.5f);
            pip.setPosition({x, y});
            m_render_texture->draw(pip);

            sf::Text label(m_font, def->name.substr(0, 2), 6);
            label.setFillColor(sf::Color(10, 10, 20));
            label.setPosition({x + 2.f, y});
            m_render_texture->draw(label);

            x += 24.f;
        }
    }

    // ESC hint
    sf::Text esc(m_font, "ESC: Menu", 8);
    esc.setFillColor(sf::Color(80, 110, 140));
    esc.setPosition({(float)WINDOW_WIDTH - 70.f, 4.f});
    m_render_texture->draw(esc);

    // Gauge
    for(const auto& w : m_widgets.get_widgets())
        m_render_texture->draw(*w);

    // Restore camera view
    m_render_texture->setView(m_camera.get_view());
}
