/**
 * PlayScene.cpp  -  Cinder
 */

#include "PlayScene.hpp"

#include "Palette.hpp"
#include "Run.hpp"
#include "Sound.hpp"

#include "../../include/core/Random.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace cinder
{

namespace
{
    // --- the spark ------------------------------------------------------
    constexpr float RADIUS        = 4.0f;
    constexpr float SPEED         = 105.0f;
    constexpr float RESPONSE      = 9.0f;      // how quickly velocity follows input
    constexpr float DASH_SPEED    = 330.0f;
    constexpr float DASH_TIME     = 0.17f;
    constexpr float DASH_COOLDOWN = 0.45f;
    constexpr float DASH_COST     = 0.05f;

    // --- heat -------------------------------------------------------------
    constexpr float WATER_DRAIN   = 0.16f;     // per second, on top of the level's
    constexpr float WIND_DRAIN    = 0.02f;
    constexpr float FLAKE_COST    = 0.025f;
    constexpr float EMBER_HEAT    = 0.35f;
    constexpr float FIRE_WARMING  = 0.35f;     // per second, standing by a lit brazier
    constexpr float FIRE_REACH    = 46.0f;
    constexpr float LOW_HEAT      = 0.25f;
    constexpr float EMBER_RESPAWN = 25.0f;

    // --- light --------------------------------------------------------------
    constexpr float BRAZIER_RADIUS = 150.0f;
    constexpr float TRAIL_SPACING  = 5.0f;
    constexpr float TRAIL_LIFE     = 5.0f;
    constexpr std::size_t MAX_TRAIL = 400;

    constexpr float FADE_TIME = 0.6f;

    float length(sf::Vector2f v) { return std::sqrt(v.x * v.x + v.y * v.y); }

    void rect(sf::RenderTarget& t, sf::Vector2f p, sf::Vector2f s, sf::Color c)
    {
        sf::RectangleShape r(s);
        r.setPosition({std::round(p.x), std::round(p.y)});
        r.setFillColor(c);
        t.draw(r);
    }
}

PlayScene::PlayScene()
    : m_camera(FRAME_SIZE_F, FRAME_SIZE_F * 0.5f)
{
    m_camera.set_trauma_decay(1.8f);
    m_camera.set_max_shake_offset(6.0f);

    auto sparks = std::make_shared<ParticleSystem>();
    sparks->set_fade(true);
    sparks->set_drag(0.15f);
    sparks->set_lifespan(sf::seconds(0.7f));
    m_sparks.add_particle_system("sparks", sparks);

    m_flood.set_transition(TransitionFunction::EaseInOutQuad);
    m_flood.set_duration(3.0f);
    m_fade.set_unscaled(true);
    m_fade.set_transition(TransitionFunction::Linear);
    m_fade.set_duration(FADE_TIME);
}

sf::Color PlayScene::get_clear_color() const
{
    return BACKGROUND;
}

// ---------------------------------------------------------------- lifecycle

void PlayScene::on_enter()
{
    m_font = font();
    m_reveal.initialize(m_manager->get_shaders());
    m_manager->set_bloom_enabled(true);
    GameTime::resume();

    Run::current() = Run{};

    // CINDER_LEVEL=2 (or 3) starts further in, for working on later levels.
    std::size_t first = 0;
    if(const char* env = std::getenv("CINDER_LEVEL"))
        first = static_cast<std::size_t>(std::clamp(std::atoi(env), 1, static_cast<int>(levels().size())) - 1);
    start_level(first);

    m_fade.set_instant(1.0f);
    m_fade.set_value(0.0f);

    SoundBank::get().set_music(true);
}

void PlayScene::on_exit()
{
    GameTime::resume();
    SoundBank::get().stop_loops();
}

void PlayScene::start_level(std::size_t index)
{
    m_level = index;
    const LevelDef& def = levels()[index];
    m_world.build(def);

    m_braziers.clear();
    for(auto p : def.braziers)
    {
        Brazier b;
        b.pos = p;
        b.warmth.set_transition(TransitionFunction::EaseOutBack);
        b.warmth.set_duration(1.3f);
        m_braziers.push_back(b);
    }

    m_embers.clear();
    for(auto p : def.embers) m_embers.push_back({p, false, 0.0f, Random::real(0.0f, 6.0f)});

    m_flakes.clear();
    if(def.snow)
        for(int i = 0; i < 48; ++i)
            m_flakes.push_back({{Random::real(0.0f, FRAME_SIZE_F.x), Random::real(0.0f, FRAME_SIZE_F.y)},
                                Random::real(16.0f, 32.0f), Random::real(0.0f, 6.0f)});

    m_checkpoint = def.start;
    m_flood.set_instant(0.0f);
    m_phase      = Phase::PLAYING;
    m_level_real = 0.0f;
    m_phase_real = 0.0f;
    m_moved      = false;

    m_camera.set_bounds(sf::FloatRect({0.0f, 0.0f}, def.size));
    respawn();
}

void PlayScene::respawn()
{
    m_pos  = m_checkpoint;
    m_vel  = {};
    m_heat = 1.0f;
    m_last_trail = m_pos;
    m_dash_time = m_dash_cooldown = 0.0f;
    m_trail.clear();
    m_motes.clear();
    m_sparks.get_particle_system("sparks")->clear();
    m_camera.set_center(m_pos, true);
    m_camera.clear_shake();
    m_phase = Phase::PLAYING;
    GameTime::set_scale(1.0f);
}

// ---------------------------------------------------------------- input

void PlayScene::handle_event(const sf::Event& event, sf::Vector2f mouse_pos)
{
    (void)mouse_pos;

    auto set_key = [this](sf::Keyboard::Scancode code, bool down)
    {
        using S = sf::Keyboard::Scancode;
        switch(code)
        {
            case S::W: case S::Up:    m_up    = down; break;
            case S::S: case S::Down:  m_down  = down; break;
            case S::A: case S::Left:  m_left  = down; break;
            case S::D: case S::Right: m_right = down; break;
            default: break;
        }
    };

    if(const auto* key = event.getIf<sf::Event::KeyPressed>())
    {
        set_key(key->scancode, true);

        switch(key->scancode)
        {
            case sf::Keyboard::Scancode::Space:
                m_dash_requested = true;
                break;
            case sf::Keyboard::Scancode::P:
                if(m_phase == Phase::PLAYING)
                {
                    EventBus::publish(Event(EventType::TOGGLE_PAUSE));
                    SoundBank::get().play("ui_select", 0.5f);
                }
                break;
            case sf::Keyboard::Scancode::M:
                SoundBank::get().toggle_music_mute();
                m_mute_notice = 2.0f;
                break;
            case sf::Keyboard::Scancode::Escape:
                SoundBank::get().play("ui_select", 0.5f);
                m_manager->request_scene_switch("menu", SceneTransitionType::FADE);
                break;
            default:
                break;
        }
    }
    else if(const auto* key_up = event.getIf<sf::Event::KeyReleased>())
    {
        set_key(key_up->scancode, false);
    }
    else if(event.is<sf::Event::FocusLost>())
    {
        m_up = m_down = m_left = m_right = false;
    }
}

// ---------------------------------------------------------------- update

void PlayScene::update(sf::Time delta_time)
{
    float const dt = delta_time.asSeconds();
    if(dt <= 0.0f) return;          // paused

    m_time += dt;
    Run::current().time += dt;

    if(m_phase == Phase::PLAYING) update_spark(dt);
    update_world(dt);

    m_camera.follow(m_pos, delta_time, 6.0f);
    m_camera.update(delta_time);
}

void PlayScene::update_spark(float dt)
{
    const LevelDef& def = m_world.def();

    sf::Vector2f input{(m_right ? 1.0f : 0.0f) - (m_left ? 1.0f : 0.0f),
                       (m_down  ? 1.0f : 0.0f) - (m_up   ? 1.0f : 0.0f)};
    if(float const l = length(input); l > 0.0f)
    {
        input /= l;
        m_facing = input;
        m_moved  = true;
    }

    bool const in_water = m_world.in_water(m_pos);
    sf::Vector2f const wind = m_world.wind_at(m_pos);

    // --- dash -------------------------------------------------------------
    m_dash_cooldown = std::max(0.0f, m_dash_cooldown - dt);
    if(m_dash_requested && m_dash_cooldown <= 0.0f && m_heat > DASH_COST)
    {
        m_dash_time     = DASH_TIME;
        m_dash_cooldown = DASH_COOLDOWN;
        m_vel           = m_facing * DASH_SPEED;
        m_heat         -= DASH_COST;
        SoundBank::get().play("dash", 0.7f, 0.06f);
        m_motes.burst(m_pos, 10, 60.0f, 2.0f);
    }
    m_dash_requested = false;

    // --- movement -----------------------------------------------------------
    if(m_dash_time > 0.0f)
    {
        m_dash_time -= dt;
        m_motes.flame(m_pos, 2.0f, 2.5f, 10.0f);
    }
    else
    {
        float const top = SPEED * (in_water ? 0.6f : 1.0f);
        m_vel += (input * top - m_vel) * std::min(1.0f, RESPONSE * dt);
    }

    m_pos += (m_vel + wind) * dt;
    m_world.collide(m_pos, RADIUS);

    // --- heat -----------------------------------------------------------------
    float drain = def.drain;
    if(in_water)
    {
        drain += WATER_DRAIN;
        if(Random::chance(dt * 30.0f)) m_motes.steam(m_pos, 1);
        if(!m_was_in_water)
        {
            SoundBank::get().play("sizzle", 0.8f, 0.08f);
            m_motes.steam(m_pos, 8);
        }
    }
    if(wind.x != 0.0f || wind.y != 0.0f) drain += WIND_DRAIN;
    m_was_in_water = in_water;

    for(const auto& b : m_braziers)
        if(b.lit && length(b.pos - m_pos) < FIRE_REACH) drain -= FIRE_WARMING;

    m_heat = std::clamp(m_heat - drain * dt, 0.0f, 1.0f);

    // --- embers -----------------------------------------------------------------
    for(auto& e : m_embers)
    {
        if(e.taken || length(e.pos - m_pos) > RADIUS + 6.0f) continue;
        e.taken   = true;
        e.respawn = EMBER_RESPAWN;
        m_heat    = std::min(1.0f, m_heat + EMBER_HEAT);
        ++Run::current().embers;
        SoundBank::get().play("ember_pickup", 0.8f, 0.08f);
        m_motes.burst(e.pos, 14, 50.0f, 2.0f);
    }

    // --- braziers -----------------------------------------------------------------
    for(auto& b : m_braziers)
        if(!b.lit && length(b.pos - m_pos) < 18.0f) light(b);

    // --- snow ---------------------------------------------------------------------
    for(auto& f : m_flakes)
    {
        if(length(f.pos - m_pos) > RADIUS + 2.0f) continue;
        m_heat = std::max(0.0f, m_heat - FLAKE_COST);
        m_low_flash = 1.0f;
        SoundBank::get().play("sizzle", 0.45f, 0.15f);
        m_motes.steam(f.pos, 5);
        f.pos.y -= FRAME_SIZE_F.y + 10.0f;       // gone; it will fall in again from above
    }

    // --- the trail ----------------------------------------------------------------
    if(length(m_pos - m_last_trail) > TRAIL_SPACING)
    {
        m_last_trail = m_pos;
        if(m_trail.size() >= MAX_TRAIL) m_trail.erase(m_trail.begin());
        float const life = TRAIL_LIFE * (0.4f + 0.6f * m_heat);
        m_trail.push_back({m_pos, life, life, 18.0f + 16.0f * m_heat});
    }

    // The flame itself.
    m_flame_timer += dt;
    float const rate = 1.0f / (25.0f + 35.0f * m_heat);
    while(m_flame_timer > rate)
    {
        m_flame_timer -= rate;
        m_motes.flame(m_pos + sf::Vector2f{0.0f, -1.0f}, 1.5f + m_heat, 1.5f + m_heat * 1.5f, 20.0f + 20.0f * m_heat);
    }

    // --- heartbeat --------------------------------------------------------------
    if(m_heat < LOW_HEAT)
    {
        m_heartbeat_timer -= dt;
        if(m_heartbeat_timer <= 0.0f)
        {
            SoundBank::get().play("heartbeat", 0.4f + (LOW_HEAT - m_heat) * 2.0f);
            m_heartbeat_timer = 0.55f + 0.6f * (m_heat / LOW_HEAT);
        }
    }
    else
    {
        m_heartbeat_timer = 0.0f;
    }

    if(m_heat <= 0.0f) go_out();
}

void PlayScene::update_world(float dt)
{
    for(auto& t : m_trail) t.life -= dt;
    std::erase_if(m_trail, [](const Trail& t) { return t.life <= 0.0f; });

    for(auto& e : m_embers)
    {
        e.phase += dt;
        if(e.taken && (e.respawn -= dt) <= 0.0f) e.taken = false;
        if(!e.taken && Random::chance(dt * 3.0f)) m_motes.flame(e.pos, 1.0f, 1.0f, 12.0f);
    }

    for(const auto& b : m_braziers)
    {
        if(b.lit)
        {
            // Fire: a steady stream of motes, more while it is catching.
            int const n = Random::integer(2, 3);
            for(int i = 0; i < n; ++i) m_motes.flame(b.pos + sf::Vector2f{0.0f, -6.0f}, 4.0f, 3.0f, 34.0f);
        }
        else if(Random::chance(dt * 4.0f))
        {
            m_motes.smoke(b.pos + sf::Vector2f{0.0f, -5.0f}, 1);    // a wisp, so cold fires can be found
        }
    }

    // Snow drifts down through the camera's view and wraps around it, so it
    // always looks like it is falling everywhere.
    if(!m_flakes.empty())
    {
        sf::FloatRect const view = m_camera.get_viewport_bounds();
        for(auto& f : m_flakes)
        {
            f.sway += dt;
            sf::Vector2f const wind = m_world.wind_at(f.pos) * 0.5f;
            f.pos += sf::Vector2f{std::sin(f.sway * 1.3f) * 8.0f + 6.0f, f.speed} * dt + wind * dt;

            float const x0 = view.position.x - 8.0f, y0 = view.position.y - 8.0f;
            float const w  = view.size.x + 16.0f,    h  = view.size.y + 16.0f;
            f.pos.x = x0 + std::fmod(std::fmod(f.pos.x - x0, w) + w, w);
            f.pos.y = y0 + std::fmod(std::fmod(f.pos.y - y0, h) + h, h);
        }
    }

    m_motes.update(dt);
    m_sparks.update(sf::seconds(dt));

    if(m_phase == Phase::DYING) m_motes.smoke(m_pos, Random::chance(0.5f) ? 1 : 0);
}

void PlayScene::light(Brazier& b)
{
    b.lit = true;
    b.warmth.set_value(BRAZIER_RADIUS);
    m_checkpoint = b.pos + sf::Vector2f{0.0f, 14.0f};
    m_heat = std::max(m_heat, 0.6f);

    SoundBank::get().play("ignite", 0.9f, 0.04f);
    m_camera.shake(0.45f);
    m_motes.burst(b.pos + sf::Vector2f{0.0f, -4.0f}, 50, 120.0f, 3.0f);

    auto sparks = m_sparks.get_particle_system("sparks");
    sparks->set_position(b.pos + sf::Vector2f{0.0f, -4.0f});
    sparks->add_particles_varied(40, GOLDEN,      Vector{140.0f, sf::degrees(90.0f)}, 70.0f, 50.0f);
    sparks->add_particles_varied(30, ORANGE_GLOW, Vector{90.0f,  sf::degrees(90.0f)}, 180.0f, 40.0f);

    if(lit_count() == static_cast<int>(m_braziers.size()))
    {
        m_phase      = Phase::COMPLETE;
        m_phase_real = 0.0f;
        m_flood_centre = b.pos;
        m_flood.set_value(2600.0f);
        m_vel = {};
        SoundBank::get().play("level_complete", 0.9f);
        m_camera.shake(0.3f);
    }
}

void PlayScene::go_out()
{
    m_phase      = Phase::DYING;
    m_phase_real = 0.0f;
    ++Run::current().deaths;
    GameTime::set_scale(0.35f);
    SoundBank::get().play("extinguish", 0.9f);
    m_motes.smoke(m_pos, 16);
    m_camera.shake(0.2f);
}

void PlayScene::update_realtime(sf::Time delta_time)
{
    float const dt = delta_time.asSeconds();
    m_level_real  += dt;
    m_phase_real  += dt;
    m_mute_notice  = std::max(0.0f, m_mute_notice - dt);
    m_low_flash    = std::max(0.0f, m_low_flash - dt * 3.0f);

    update_audio();
    SoundBank::get().update(dt);

    // Phase changes run on real time, so slow motion does not stretch them.
    if(m_pending == Pending::NONE)
    {
        if(m_phase == Phase::DYING && m_phase_real > 1.8f)
        {
            m_pending = Pending::RESPAWN;
            m_fade.set_value(1.0f);
        }
        else if(m_phase == Phase::COMPLETE && m_phase_real > 5.0f)
        {
            if(m_level + 1 >= levels().size())
            {
                Run::current().finished = true;
                m_manager->request_scene_switch("end", SceneTransitionType::FADE);
                m_pending = Pending::NEXT_LEVEL;   // nothing more to do here
            }
            else
            {
                m_pending = Pending::NEXT_LEVEL;
                m_fade.set_value(1.0f);
            }
        }
    }
    else if(m_fade.is_transition_over() && m_fade.get_target() >= 1.0f)
    {
        if(m_pending == Pending::RESPAWN)    respawn();
        if(m_pending == Pending::NEXT_LEVEL && !Run::current().finished) start_level(m_level + 1);
        if(!Run::current().finished)
        {
            m_pending = Pending::NONE;
            m_fade.set_value(0.0f);
        }
    }
}

void PlayScene::update_audio()
{
    SoundBank& sound = SoundBank::get();

    if(GameTime::is_paused())
    {
        sound.stop_loops();
        return;
    }

    float fire = m_phase == Phase::DYING ? 0.0f : 0.10f + 0.10f * m_heat;
    for(const auto& b : m_braziers)
        if(b.lit) fire = std::max(fire, 0.75f * (1.0f - std::min(1.0f, length(b.pos - m_pos) / 260.0f)));
    sound.set_loop("fire_loop", fire);

    bool const playing = m_phase == Phase::PLAYING;
    sound.set_loop("hiss_loop", playing && m_world.in_water(m_pos) ? 0.55f : 0.0f);

    sf::Vector2f const wind = m_world.wind_at(m_pos);
    float const gust = (wind.x != 0.0f || wind.y != 0.0f) ? 0.75f : (m_world.def().winds.empty() ? 0.0f : 0.15f);
    sound.set_loop("wind_loop", m_phase == Phase::COMPLETE ? 0.0f : gust);
}

int PlayScene::lit_count() const
{
    return static_cast<int>(std::count_if(m_braziers.begin(), m_braziers.end(),
                                          [](const Brazier& b) { return b.lit; }));
}

// ---------------------------------------------------------------- render

void PlayScene::render(sf::RenderTarget& target)
{
    if(!m_reveal.ready()) return;

    sf::View const view = m_camera.get_view();
    sf::FloatRect const visible = m_camera.get_viewport_bounds();

    // 1. The living world, in colour.
    sf::RenderTexture& world = m_reveal.world(view, m_world.ground_colour());
    m_world.draw_living(world, visible);
    draw_bowls(world);

    // 2. Heat.
    float const flicker = 0.5f + 0.5f * std::sin(m_time * 11.0f) * std::sin(m_time * 3.7f);
    if(m_phase != Phase::DYING || m_phase_real < 1.0f)
    {
        float const fade_out = m_phase == Phase::DYING ? 1.0f - m_phase_real : 1.0f;
        float const r = (34.0f + 72.0f * m_heat) * (1.0f + 0.03f * flicker) * fade_out;
        m_reveal.warm(m_pos, r);
    }
    for(const auto& t : m_trail)
    {
        float const k = t.life / t.max_life;
        m_reveal.warm(t.pos, t.radius * std::sqrt(k), std::min(1.0f, k * 1.4f));
    }
    for(const auto& b : m_braziers)
        if(b.lit) m_reveal.warm(b.pos, b.warmth.get_value() * (1.0f + 0.02f * flicker));
    for(const auto& e : m_embers)
        if(!e.taken) m_reveal.warm(e.pos, 16.0f + 2.0f * std::sin(e.phase * 4.0f), 0.7f);
    if(m_flood.get_value() > 1.0f) m_reveal.warm(m_flood_centre, m_flood.get_value());

    // 3. Burn the cold parts to ash; draw what is always in colour on top.
    sf::RenderTexture& frame = m_reveal.composite(view, m_time, flicker);
    draw_cold_things(frame, visible);
    m_motes.draw(frame);
    frame.draw(m_sparks);
    draw_spark(frame);

    // 4. Up to full size, then the HUD at full resolution.
    m_reveal.present(target);
    target.setView(target.getDefaultView());
    draw_pointers(target);
    draw_hud(target);

    if(float const a = m_fade.get_value(); a > 0.0f)
    {
        sf::RectangleShape veil(sf::Vector2f{BASE_RESOLUTION});
        veil.setFillColor(with_alpha(BACKGROUND, a));
        target.draw(veil);
    }
}

void PlayScene::draw_bowls(sf::RenderTarget& t) const
{
    for(const auto& b : m_braziers)
    {
        sf::Vector2f const p{std::round(b.pos.x), std::round(b.pos.y)};
        // Legs, bowl, rim -- stone that warms to brick once it has burned.
        sf::Color const stone = b.lit ? RED_BRICK : G6;
        sf::Color const dark  = b.lit ? RED_DEEP  : G7;
        rect(t, p + sf::Vector2f{-6, 4}, {2, 4}, dark);
        rect(t, p + sf::Vector2f{4, 4},  {2, 4}, dark);
        rect(t, p + sf::Vector2f{-7, 0}, {14, 4}, stone);
        rect(t, p + sf::Vector2f{-8, -2}, {16, 2}, b.lit ? RED_KINDLING : G5);
        rect(t, p + sf::Vector2f{-5, -3}, {10, 1}, b.lit ? RED_ASHEN : G8);   // old ash in the bowl
        rect(t, p + sf::Vector2f{-9, 8}, {18, 2}, sf::Color(0x10, 0x0E, 0x0C));
    }
}

void PlayScene::draw_cold_things(sf::RenderTarget& t, const sf::FloatRect& visible)
{
    m_world.draw_cold(t, visible, m_time);

    // Snowflakes: always visible, the same way water is.
    if(!m_flakes.empty())
    {
        sf::VertexArray va(sf::PrimitiveType::Triangles);
        sf::Color const c = lerp(AQUA, G1, 0.55f);
        for(const auto& f : m_flakes)
        {
            sf::Vector2f const p{std::round(f.pos.x), std::round(f.pos.y)};
            for(sf::Vector2f o : {sf::Vector2f{0, 0}, sf::Vector2f{1, 0}, sf::Vector2f{0, 1}, sf::Vector2f{1, 1}})
            {
                sf::Vector2f const q = p + o;
                sf::Color const k = (o.x + o.y == 1.0f) ? c : with_alpha(c, 0.5f);
                va.append({q, k}); va.append({q + sf::Vector2f{1, 0}, k}); va.append({q + sf::Vector2f{1, 1}, k});
                va.append({q, k}); va.append({q + sf::Vector2f{1, 1}, k}); va.append({q + sf::Vector2f{0, 1}, k});
            }
        }
        t.draw(va);
    }

    // Embers lying on the ground: a pulsing coal.
    for(const auto& e : m_embers)
    {
        if(e.taken) continue;
        float const pulse = 0.5f + 0.5f * std::sin(e.phase * 4.0f);
        sf::Vector2f const p{std::round(e.pos.x), std::round(e.pos.y)};
        rect(t, p + sf::Vector2f{-2, -1}, {4, 3}, lerp(RED_EMBER, ORANGE_GLOW, pulse));
        rect(t, p + sf::Vector2f{-1, -2}, {2, 1}, RED_EMBER);
        rect(t, p + sf::Vector2f{-1, 0},  {2, 1}, lerp(GOLDEN, YELLOW, pulse));
    }

    // Fire in the lit bowls: a hot core under the motes.
    for(const auto& b : m_braziers)
    {
        if(!b.lit) continue;
        sf::Vector2f const p{std::round(b.pos.x), std::round(b.pos.y)};
        float const f = std::sin(m_time * 13.0f + b.pos.x) * 0.5f + 0.5f;
        rect(t, p + sf::Vector2f{-5, -4}, {10, 2}, ORANGE_GLOW);
        rect(t, p + sf::Vector2f{-3, -6 - std::round(f)}, {6, 3}, GOLDEN);
        rect(t, p + sf::Vector2f{-1, -7 - std::round(f * 2.0f)}, {2, 3}, YELLOW);
    }
}

void PlayScene::draw_spark(sf::RenderTarget& t)
{
    if(m_phase == Phase::DYING && m_phase_real > 1.0f) return;

    float const k = m_phase == Phase::DYING ? 1.0f - m_phase_real : 1.0f;
    float const glow = (5.0f + 6.0f * m_heat) * k;

    sf::CircleShape halo(glow, 20);
    halo.setOrigin({glow, glow});
    halo.setPosition(m_pos);
    halo.setFillColor(with_alpha(ORANGE_GLOW, 0.22f * k));
    t.draw(halo, sf::BlendAdd);

    sf::Vector2f const p{std::round(m_pos.x), std::round(m_pos.y)};
    sf::Color const body = m_heat < LOW_HEAT ? lerp(RED_FLAME, ORANGE_GLOW, m_heat / LOW_HEAT) : ORANGE_GLOW;
    rect(t, p + sf::Vector2f{-2, -2}, {4, 4}, body);
    rect(t, p + sf::Vector2f{-1, -3}, {2, 6}, body);
    rect(t, p + sf::Vector2f{-3, -1}, {6, 2}, body);
    rect(t, p + sf::Vector2f{-1, -2}, {2, 3}, m_heat > 0.15f ? YELLOW : GOLDEN);
    if(m_heat > 0.4f) rect(t, p + sf::Vector2f{0, -1}, {1, 1}, G1);
}

void PlayScene::draw_pointers(sf::RenderTarget& t)
{
    // A small grey marker at the screen edge for each brazier still cold
    // and out of sight, so the dark never becomes a maze.
    if(m_phase == Phase::COMPLETE) return;

    sf::FloatRect const view = m_camera.get_viewport_bounds();
    sf::Vector2f const centre = view.position + view.size * 0.5f;
    float const pulse = 0.55f + 0.25f * std::sin(m_level_real * 3.0f);

    for(const auto& b : m_braziers)
    {
        if(b.lit || view.contains(b.pos)) continue;

        sf::Vector2f const d = b.pos - centre;
        // Scale the direction to hit the edge of an inset screen rectangle.
        float const hx = view.size.x * 0.5f - 10.0f, hy = view.size.y * 0.5f - 10.0f;
        float const s  = std::min(hx / std::max(0.001f, std::abs(d.x)), hy / std::max(0.001f, std::abs(d.y)));
        sf::Vector2f const edge = (sf::Vector2f{hx + 10.0f, hy + 10.0f} + d * s) * 2.0f;   // to HUD pixels

        float const angle = std::atan2(d.y, d.x);
        sf::ConvexShape arrow(3);
        arrow.setPoint(0, {7.0f, 0.0f});
        arrow.setPoint(1, {-4.0f, -5.0f});
        arrow.setPoint(2, {-4.0f, 5.0f});
        arrow.setPosition({std::round(edge.x), std::round(edge.y)});
        arrow.setRotation(sf::radians(angle));
        arrow.setFillColor(with_alpha(G5, pulse));
        t.draw(arrow);
    }
}

void PlayScene::draw_hud(sf::RenderTarget& t)
{
    if(!m_font) return;
    const LevelDef& def = m_world.def();

    // --- heat --------------------------------------------------------------
    float const blink = (m_heat < LOW_HEAT && std::fmod(m_level_real, 0.5f) < 0.2f) ? 0.5f : 1.0f;
    sf::Color const fill = with_alpha(m_heat < LOW_HEAT ? RED_FLAME : lerp(ORANGE_BLAZE, GOLDEN, m_heat), blink);

    rect(t, {22, 22}, {164, 12}, G9);
    rect(t, {24, 24}, {160, 8}, G11);
    rect(t, {24, 24}, {std::round(160.0f * m_heat), 8}, fill);
    rect(t, {24, 24}, {std::round(160.0f * m_heat), 2}, with_alpha(YELLOW, 0.6f * blink));
    if(m_low_flash > 0.0f) rect(t, {22, 22}, {164, 12}, with_alpha(AQUA, m_low_flash * 0.5f));
    t.draw(centred_text(*m_font, "heat", 16, {207, 27}, G6));

    // --- braziers, top right ------------------------------------------------------
    float x = static_cast<float>(BASE_RESOLUTION.x) - 26.0f - 22.0f * static_cast<float>(m_braziers.size() - 1);
    for(const auto& b : m_braziers)
    {
        rect(t, {x - 7, 26}, {14, 6}, b.lit ? RED_BRICK : G8);
        rect(t, {x - 8, 24}, {16, 2}, b.lit ? RED_KINDLING : G7);
        if(b.lit)
        {
            rect(t, {x - 4, 18}, {8, 6}, ORANGE_GLOW);
            rect(t, {x - 2, 15}, {4, 6}, GOLDEN);
            rect(t, {x - 1, 13}, {2, 4}, YELLOW);
        }
        x += 22.0f;
    }

    // --- level name, as it begins -------------------------------------------------
    float const intro = m_level_real < 0.6f ? m_level_real / 0.6f
                      : m_level_real < 3.4f ? 1.0f
                      : std::max(0.0f, 1.0f - (m_level_real - 3.4f) / 1.2f);
    if(intro > 0.0f)
    {
        sf::Vector2f const c{BASE_RESOLUTION.x * 0.5f, 150.0f};
        std::string const number = std::to_string(m_level + 1) + " / " + std::to_string(levels().size());
        t.draw(centred_text(*m_font, number, 16, c + sf::Vector2f{0, -38}, with_alpha(G6, intro)));
        t.draw(centred_text(*m_font, def.name, 32, c, with_alpha(G2, intro)));
        t.draw(centred_text(*m_font, def.line, 16, c + sf::Vector2f{0, 34}, with_alpha(G5, intro)));
    }

    // --- how to play, on the first level until you have moved a while ----------------
    if(m_level == 0 && m_phase == Phase::PLAYING)
    {
        float const a = m_moved ? std::max(0.0f, 1.0f - (m_level_real - 10.0f) / 2.0f) : 1.0f;
        if(a > 0.0f)
        {
            float const y = static_cast<float>(BASE_RESOLUTION.y) - 32.0f;
            t.draw(centred_text(*m_font, "wasd / arrows  move      space  dash      p  pause      m  music",
                                16, {BASE_RESOLUTION.x * 0.5f, y}, with_alpha(G5, a)));
            t.draw(centred_text(*m_font, "relight every brazier.  embers keep you warm.",
                                16, {BASE_RESOLUTION.x * 0.5f, y - 22.0f}, with_alpha(G6, a)));
        }
    }

    // --- the level coming back ------------------------------------------------------
    if(m_phase == Phase::COMPLETE)
    {
        float const a = std::clamp((m_phase_real - 1.0f) / 0.8f, 0.0f, 1.0f);
        sf::Vector2f const c{BASE_RESOLUTION.x * 0.5f, 120.0f};
        t.draw(centred_text(*m_font, def.name + " is warm again", 32, c + sf::Vector2f{2, 2}, with_alpha(RED_ASHEN, a)));
        t.draw(centred_text(*m_font, def.name + " is warm again", 32, c, with_alpha(GOLDEN, a)));
    }

    if(m_phase == Phase::DYING)
    {
        float const a = std::clamp(m_phase_real / 0.6f, 0.0f, 1.0f);
        t.draw(centred_text(*m_font, "gone out", 32, {BASE_RESOLUTION.x * 0.5f, 220.0f}, with_alpha(G4, a)));
    }

    if(m_mute_notice > 0.0f)
    {
        t.draw(centred_text(*m_font, SoundBank::get().music_muted() ? "music off" : "music on", 16,
                            {static_cast<float>(BASE_RESOLUTION.x) - 70.0f, static_cast<float>(BASE_RESOLUTION.y) - 28.0f},
                            with_alpha(G5, std::min(1.0f, m_mute_notice))));
    }

    // --- pause -------------------------------------------------------------------------
    if(GameTime::is_paused())
    {
        sf::RectangleShape veil(sf::Vector2f{BASE_RESOLUTION});
        veil.setFillColor(with_alpha(BACKGROUND, 0.7f));
        t.draw(veil);
        sf::Vector2f const c{BASE_RESOLUTION.x * 0.5f, BASE_RESOLUTION.y * 0.5f};
        t.draw(centred_text(*m_font, "paused", 32, c + sf::Vector2f{0, -16}, G2));
        t.draw(centred_text(*m_font, "p  resume      esc  title", 16, c + sf::Vector2f{0, 22}, G5));
    }
}

}
