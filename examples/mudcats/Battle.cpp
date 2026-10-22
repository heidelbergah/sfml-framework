/**
 * Battle.cpp  -  Mud Cats
 *
 * Setup, input, the level script and the squad's orders.
 */

#include "Battle.hpp"

#include "Palette.hpp"
#include "Sound.hpp"

#include "../../include/core/Random.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace mudcats
{

namespace
{
    float length(sf::Vector2f v) { return std::sqrt(v.x * v.x + v.y * v.y); }

    const char* const RECHIVII_NAMES[] = {"Oslev", "Brannik", "Dvorek", "Halst", "Mirov", "Tesk", "Vadim", "Kosar",
                                          "Rudan", "Zelny", "Pavek", "Strom", "Ilko", "Gerd", "Yaro", "Lutz"};
}

Battle::Battle()
    : m_camera(FRAME_F, FRAME_F * 0.5f)
{
    m_camera.set_max_shake_offset(7.0f);
    m_camera.set_trauma_decay(1.6f);
}

sf::Color Battle::get_clear_color() const { return INK; }

// ---------------------------------------------------------------- lifecycle

void Battle::on_enter()
{
    m_font = Assets::font("assets/fonts/pixel-font-bitmap.ttf");
    m_manager->set_bloom_enabled(true);
    GameTime::resume();
    m_paused = false;

    build_world();
    spawn_squads();

    m_stage = Stage::REAR;
    m_stage_time = m_time = m_real = 0.0f;
    m_waypoint = 0;
    m_end_timer = 0.0f;
    m_intensity = m_combat_heat = 0.0f;
    m_bottles_left = 3;
    m_officer_dead_noted = false;
    m_hurt = m_flash = m_takeover = 0.0f;
    m_up = m_down = m_left = m_right = m_sprint = m_trigger = m_aiming = false;

    Campaign::current().won = false;
    Campaign::current().time = 0.0f;

    // CINDER-style shortcut for working on later parts of the level.
    if(const char* env = std::getenv("MUDCATS_STAGE"))
    {
        int const s = std::atoi(env);
        if(s >= 1)
        {
            Soldier* p = player();
            p->pos = {850.0f, 440.0f};
            for(auto& f : m_soldiers)
                if(f.team == Team::MONTERAN && !f.is_player) { f.pos = f.goal = {850.0f, 300.0f + 30.0f * static_cast<float>(f.id)}; f.sitting = false; }
            set_stage(s >= 2 ? Stage::BARRAGE : Stage::FRONT);
        }
        // 3: their trench already taken.  4: the farm reached, waiting for them.
        if(s >= 3)
        {
            sf::Vector2f const at = s >= 4 ? sf::Vector2f{2320.0f, 380.0f} : sf::Vector2f{1716.0f, 420.0f};
            for(auto& f : m_soldiers)
            {
                if(f.team == Team::MONTERAN) { f.pos = f.goal = at + sf::Vector2f{Random::real(-20, 20), static_cast<float>(f.id * 12) - 40.0f}; f.order = Order::HOLD; }
                else if(!f.reserve && (f.post.x < 1800 || s >= 4)) { f.life = Life::DEAD; f.active = false; }
            }
            set_stage(s >= 4 ? Stage::HOLD : Stage::TRENCH);
            m_waypoint = s >= 4 ? 8 : 5;
        }
    }

    // For recording and testing: your man is driven like the others.
    m_autoplay = std::getenv("MUDCATS_AUTOPLAY") != nullptr;
    if(const char* speed = std::getenv("MUDCATS_SPEED")) GameTime::set_scale(static_cast<float>(std::atof(speed)));

    m_camera.set_bounds(sf::FloatRect({0.0f, 0.0f}, m_map.size));
    m_camera.set_center(player()->pos, true);

    SoundBank& sound = SoundBank::get();
    sound.start_music();
    sound.set_intensity(0.0f);
    sound.set_front(0.55f);
    sound.set_listener(player()->pos);
}

void Battle::on_exit()
{
    GameTime::resume();
    SoundBank::get().stop_loops();
}

void Battle::build_world()
{
    if(!m_ready)
    {
        m_map.build();
        bool ok = m_ground.resize({static_cast<unsigned>(m_map.size.x), static_cast<unsigned>(m_map.size.y)})
               && m_frame.resize(FRAME) && m_shade.resize(FRAME);
        if(!ok) return;
        m_frame.setSmooth(false);
        m_shade.setSmooth(false);
        m_grade = m_manager->get_shaders().load_fragment("mudcats_grade", "shaders/mudcats/grade.frag");
        m_ready = true;
    }
    else
    {
        m_map.build();          // fresh bottles
    }
    m_map.render(m_ground);

    m_bullets.clear(); m_grenades.clear(); m_flames.clear(); m_fires.clear(); m_gibs.clear();
    m_drops.clear(); m_puffs.clear(); m_sparks.clear(); m_lights.clear(); m_shells.clear();
    m_lines.clear(); m_popups.clear(); m_stamps.clear(); m_later.clear();
}

Soldier& Battle::add(Team team, Role role, std::string rank, std::string name, sf::Vector2f pos)
{
    Soldier s;
    s.id   = static_cast<int>(m_soldiers.size());
    s.team = team;
    s.role = role;
    s.rank = std::move(rank);
    s.name = std::move(name);
    s.pos  = pos;
    s.goal = pos;
    s.aim  = team == Team::MONTERAN ? 0.0f : 3.14159f;

    const WeaponSpec& w = weapon_of(role);
    s.ammo  = w.magazine;
    s.spare = w.spare;
    s.fuel  = w.flame ? 100.0f : 0.0f;
    s.grenades = role == Role::RIFLEMAN ? 3 : (role == Role::OFFICER ? 2 : 0);
    s.medkits  = role == Role::MEDIC ? 8 : 0;
    s.think = Random::real(0.0f, 0.3f);
    s.morale = Random::real(0.85f, 1.0f);
    m_soldiers.push_back(s);
    return m_soldiers.back();
}

void Battle::spawn_squads()
{
    m_soldiers.clear();
    Role const chosen = Campaign::current().chosen;

    struct Slot { Role role; const char* rank; const char* name; sf::Vector2f pos; bool sitting; };
    const Slot squad[] = {
        {Role::RIFLEMAN, "Pvt.", "Emil Rell",      {110, 262}, false},
        {Role::OFFICER,  "Lt.",  "Aurel Varga",    {852, 440}, false},
        {Role::GUNNER,   "Cpl.", "Anselm Kray",    {300, 282}, true},
        {Role::FLAMER,   "Pvt.", "Dorian Vosk",    {245, 262}, true},
        {Role::MEDIC,    "Pvt.", "Teodor Halm",    {190, 226}, true},
        {Role::RIFLEMAN, "Pvt.", "Tomas Dacre",    {360, 276}, true},
        {Role::RIFLEMAN, "Pvt.", "Lio Marchetti",  {70, 262},  true},
        {Role::RIFLEMAN, "Pvt.", "Benedek Sorn",   {275, 274}, false},
    };

    bool assigned = false;
    for(const auto& slot : squad)
    {
        Soldier& s = add(Team::MONTERAN, slot.role, slot.rank, slot.name, slot.pos);
        s.sitting = slot.sitting;
        s.order = Order::HOLD;
        if(!assigned && slot.role == chosen)
        {
            s.is_player = true;
            s.sitting = false;
            assigned = true;
        }
    }
    // Whoever the player is starts where Rell would, at the back.
    m_player = -1;
    for(auto& s : m_soldiers)
        if(s.is_player)
        {
            m_player = s.id;
            if(s.role != Role::RIFLEMAN)
            {
                std::swap(s.pos, m_soldiers[0].pos);
                m_soldiers[0].sitting = true;
            }
            s.goal = s.pos;
        }

    // --- the Rechivii -------------------------------------------------------
    int n = 0;
    auto enemy = [&](Role role, sf::Vector2f post, bool active, bool reserve) -> Soldier&
    {
        Soldier& s = add(Team::RECHIVII, role, role == Role::OFFICER ? "Lt." : "", RECHIVII_NAMES[n++ % 16], post);
        s.post = post;
        s.has_post = true;
        s.active = active;
        s.reserve = reserve;
        s.stance = Stance::CROUCH;
        return s;
    };

    Soldier& scout = enemy(Role::RIFLEMAN, {1500, 470}, false, false);
    scout.has_post = false;
    scout.stance = Stance::PRONE;
    scout.body_angle = 3.14159f;

    Soldier& nest = enemy(Role::GUNNER, {1668, 334}, true, false);
    nest.deployed = true;
    nest.deploy_dir = 3.14159f;
    nest.stance = Stance::PRONE;
    nest.body_angle = 3.14159f;
    enemy(Role::RIFLEMAN, {1708, 326}, true, false);

    enemy(Role::OFFICER,  {1714, 418}, true, false);
    enemy(Role::MEDIC,    {1728, 560}, true, false);
    for(float y : {190.0f, 280.0f, 470.0f, 640.0f, 740.0f})
    {
        sf::Vector2f best{1712.0f, y};
        float best_d = 1e9f;
        for(const auto& c : m_map.cover)
            if(c.trench && c.pos.x > 1690 && c.pos.x < 1745 && std::abs(c.pos.y - y) < best_d)
            {
                best_d = std::abs(c.pos.y - y);
                best = c.pos;
            }
        enemy(Role::RIFLEMAN, best, true, false);
    }

    Soldier& bmg = enemy(Role::GUNNER, {2014, 414}, true, false);
    bmg.deployed = true;
    bmg.deploy_dir = 3.14159f;
    bmg.stance = Stance::CROUCH;
    enemy(Role::FLAMER, {2042, 428}, true, false);

    for(float y : {330.0f, 380.0f, 430.0f, 480.0f})
    {
        Soldier& r = enemy(y == 380.0f ? Role::GUNNER : Role::RIFLEMAN, {2685.0f, y}, false, true);
        r.has_post = false;
        r.stance = Stance::STAND;
    }
}

// ---------------------------------------------------------------- helpers

Soldier* Battle::player()
{
    if(m_player < 0 || m_player >= static_cast<int>(m_soldiers.size())) return &m_soldiers.front();
    return &m_soldiers[static_cast<std::size_t>(m_player)];
}

Soldier* Battle::officer(Team team)
{
    for(auto& s : m_soldiers)
        if(s.team == team && s.role == Role::OFFICER && s.alive()) return &s;
    return nullptr;
}

int Battle::count(Team team, bool alive_only) const
{
    int n = 0;
    for(const auto& s : m_soldiers)
        if(s.team == team && (!alive_only || s.alive())) ++n;
    return n;
}

void Battle::say(const Soldier& who, const std::string& text, float time)
{
    if(!who.alive()) return;
    say(who.rank + " " + who.name.substr(who.name.find(' ') + 1), text, who.team, time);
    SoundBank::get().play_at("bark", who.pos, 0.6f, 0.06f);
}

void Battle::say(const std::string& who, const std::string& text, Team team, float time)
{
    if(m_lines.size() > 3) m_lines.pop_front();
    m_lines.push_back({who, text, time, team});
}

void Battle::hint(const std::string& text, float time)
{
    m_hint = text;
    m_hint_time = time;
}

void Battle::bark(Soldier& s, const std::string& text, bool voice)
{
    if(s.bark_cooldown > 0.0f || !s.alive()) return;
    s.bark = text;
    s.bark_time = 1.6f;
    s.bark_cooldown = Random::real(3.5f, 6.0f);
    if(voice) SoundBank::get().play_at("bark", s.pos, 0.55f, 0.08f);
}

// ---------------------------------------------------------------- input

void Battle::handle_event(const sf::Event& event, sf::Vector2f mouse_pos)
{
    m_mouse_screen = mouse_pos;
    using S = sf::Keyboard::Scancode;

    if(const auto* key = event.getIf<sf::Event::KeyPressed>())
    {
        if(m_paused)
        {
            if(key->scancode == S::Escape || key->scancode == S::P) { m_paused = false; GameTime::resume(); }
            if(key->scancode == S::Q) { GameTime::resume(); m_manager->request_scene_switch("briefing", SceneTransitionType::FADE); }
            return;
        }
        Soldier* p = player();
        switch(key->scancode)
        {
            case S::W: case S::Up:    m_up = true; break;
            case S::S: case S::Down:  m_down = true; break;
            case S::A: case S::Left:  m_left = true; break;
            case S::D: case S::Right: m_right = true; break;
            case S::LShift: case S::RShift: m_sprint = true; break;
            case S::Escape: case S::P:
                m_paused = true;
                GameTime::pause();
                SoundBank::get().play("ui_select", 0.5f);
                break;
            case S::M: SoundBank::get().toggle_music_mute(); break;
            case S::R: if(p->alive()) reload(*p); break;
            case S::C:
                if(p->alive() && !p->deployed && p->can_walk())
                {
                    p->stance = p->stance == Stance::CROUCH ? Stance::STAND : Stance::CROUCH;
                    p->stance_change = 0.25f;
                }
                break;
            case S::Z:
                if(p->alive() && !p->deployed)
                {
                    p->stance = p->stance == Stance::PRONE ? (p->can_walk() ? Stance::CROUCH : Stance::PRONE) : Stance::PRONE;
                    p->body_angle = p->aim;
                    p->stance_change = 0.6f;
                }
                break;
            case S::G:
                if(p->alive() && p->grenades > 0 && p->can_use_rifle()) throw_grenade(*p, m_mouse_world);
                break;
            case S::E:
            {
                if(p->life == Life::DOWNED)
                {
                    SoundBank::get().play_at("medic_call", p->pos, 0.8f, 0.03f);
                    bark(*p, "Medic!", false);
                    break;
                }
                if(!p->alive()) break;
                if(p->dragging >= 0)
                {
                    m_soldiers[static_cast<std::size_t>(p->dragging)].dragged_by = -1;
                    p->dragging = -1;
                    SoundBank::get().set_loop("drag_loop", 0.0f);
                    break;
                }
                // Nearest wounded friend in reach.
                Soldier* near = nullptr;
                float best = 14.0f;
                for(auto& o : m_soldiers)
                {
                    if(o.id == p->id || o.team != p->team) continue;
                    bool const needs = o.life == Life::DOWNED || (o.alive() && o.bleeding > 0.0f);
                    float const d = length(o.pos - p->pos);
                    if(needs && d < best) { best = d; near = &o; }
                }
                if(!near) break;
                if(p->role == Role::MEDIC && p->medkits > 0)
                {
                    p->treating = near->id;
                    p->treat_time = near->life == Life::DOWNED ? 3.0f : 1.5f;
                    SoundBank::get().play_at("bandage", p->pos, 0.8f);
                }
                else if(near->life == Life::DOWNED && near->dragged_by < 0)
                {
                    p->dragging = near->id;
                    near->dragged_by = p->id;
                    p->stance = Stance::CROUCH;
                }
                break;
            }
            case S::F:
                if(p->alive() && p->bleeding > 0.0f && p->bandages > 0 && m_bandage_time <= 0.0f)
                {
                    m_bandage_time = 2.5f;
                    SoundBank::get().play_at("bandage", p->pos, 0.8f);
                }
                break;
            case S::Space:
                if(p->life == Life::DOWNED) { kill(*p, -1, "Bled out in the mud"); take_over(); }
                break;
            case S::Num1: case S::Num2: case S::Num3: case S::Num4:
                if(p->alive() && (p->role == Role::OFFICER || !officer(Team::MONTERAN)))
                {
                    Order const o = key->scancode == S::Num1 ? Order::FOLLOW : key->scancode == S::Num2 ? Order::HOLD
                                  : key->scancode == S::Num3 ? Order::ADVANCE : Order::SUPPRESS;
                    std::vector<int> taken;
                    for(auto& s : m_soldiers)
                    {
                        if(s.team != Team::MONTERAN || s.is_player || !s.alive()) continue;
                        s.order = o;
                        s.path.clear();
                        s.repath = 0.0f;
                        if(o == Order::ADVANCE) set_goal(s, m_mouse_world + sf::Vector2f{0.0f, (static_cast<float>(s.id) - 3.5f) * 22.0f});
                        if(o == Order::HOLD) s.goal = s.pos;
                        if(o == Order::SUPPRESS)
                        {
                            s.suppress_at = m_mouse_world;
                            s.suppress_until = m_time + 12.0f;
                            s.goal = s.pos;
                        }
                    }
                    static const char* const words[] = {"On me!", "Hold here!", "Move up! Go!", "Covering fire, there!"};
                    bark(*p, words[static_cast<int>(o)], true);
                    p->bark_cooldown = 0.0f;
                    if(o == Order::ADVANCE) SoundBank::get().play_at("whistle", p->pos, 0.7f);
                }
                break;
            default: break;
        }
    }
    else if(const auto* up = event.getIf<sf::Event::KeyReleased>())
    {
        switch(up->scancode)
        {
            case S::W: case S::Up:    m_up = false; break;
            case S::S: case S::Down:  m_down = false; break;
            case S::A: case S::Left:  m_left = false; break;
            case S::D: case S::Right: m_right = false; break;
            case S::LShift: case S::RShift: m_sprint = false; break;
            default: break;
        }
    }
    else if(const auto* press = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if(m_paused) return;
        if(press->button == sf::Mouse::Button::Left) { m_trigger = true; m_trigger_pressed = true; }
        if(press->button == sf::Mouse::Button::Right)
        {
            Soldier* p = player();
            if(p->role == Role::GUNNER && p->alive())
            {
                if(p->deployed || p->deploying > 0.0f) { p->deployed = false; p->deploying = 0.0f; }
                else
                {
                    p->deploying = 1.0f;
                    p->deploy_dir = p->aim;
                    if(p->stance == Stance::STAND) p->stance = Stance::CROUCH;
                    p->body_angle = p->aim;
                    SoundBank::get().play_at("mg_setup", p->pos, 0.8f);
                }
            }
            else m_aiming = true;
        }
    }
    else if(const auto* release = event.getIf<sf::Event::MouseButtonReleased>())
    {
        if(release->button == sf::Mouse::Button::Left) m_trigger = false;
        if(release->button == sf::Mouse::Button::Right) m_aiming = false;
    }
    else if(event.is<sf::Event::FocusLost>())
    {
        m_up = m_down = m_left = m_right = m_sprint = m_trigger = m_aiming = false;
    }
}

void Battle::player_input(float dt)
{
    Soldier& p = *player();
    p.firing = false;

    // The mouse, in world coordinates.
    sf::View const& view = m_camera.get_view();
    m_mouse_world = view.getCenter() - view.getSize() * 0.5f + m_mouse_screen * 0.5f;

    if(p.life == Life::DOWNED)
    {
        // You can still crawl, slowly, leaving a trail.
        sf::Vector2f in{(m_right ? 1.0f : 0.0f) - (m_left ? 1.0f : 0.0f), (m_down ? 1.0f : 0.0f) - (m_up ? 1.0f : 0.0f)};
        if(length(in) > 0.0f && p.dragged_by < 0) p.vel = in / length(in) * 6.0f;
        else p.vel = {};
        return;
    }
    if(!p.alive()) return;

    // --- aim ----------------------------------------------------------------
    sf::Vector2f const to_mouse = m_mouse_world - p.pos;
    if(length(to_mouse) > 2.0f)
    {
        float want = std::atan2(to_mouse.y, to_mouse.x);
        if(p.deployed)
        {
            float diff = std::remainder(want - p.deploy_dir, 6.2831853f);
            want = p.deploy_dir + std::clamp(diff, -0.85f, 0.85f);
        }
        float const diff = std::remainder(want - p.aim, 6.2831853f);
        float const turn = (p.stance == Stance::PRONE ? 6.0f : 14.0f) * dt;
        p.aim += std::clamp(diff, -turn, turn);
        if(p.stance == Stance::PRONE)
        {
            float const bd = std::remainder(p.aim - p.body_angle, 6.2831853f);
            if(std::abs(bd) > 0.9f) p.body_angle += bd - std::copysign(0.9f, bd);
        }
    }

    // --- move -------------------------------------------------------------------
    sf::Vector2f in{(m_right ? 1.0f : 0.0f) - (m_left ? 1.0f : 0.0f), (m_down ? 1.0f : 0.0f) - (m_up ? 1.0f : 0.0f)};
    float speed = 46.0f;
    if(p.stance == Stance::CROUCH) speed = 28.0f;
    if(p.stance == Stance::PRONE)  speed = 11.0f;
    bool const sprinting = m_sprint && p.stance == Stance::STAND && !m_aiming && p.dragging < 0 && p.can_walk();
    if(sprinting) speed = 74.0f;
    if(m_aiming) speed *= 0.55f;
    if(p.dragging >= 0) speed = 20.0f;
    if(p.hp < 50.0f) speed *= 0.75f;
    if(!p.can_walk()) { speed = 8.0f; p.stance = Stance::PRONE; }
    if(m_map.in_wire(p.pos)) speed *= 0.35f;
    if(p.deployed || p.deploying > 0.0f || p.treating >= 0 || m_bandage_time > 0.0f) speed = 0.0f;
    if(p.stance_change > 0.0f) speed *= 0.3f;

    sf::Vector2f const want = length(in) > 0.0f ? in / length(in) * speed : sf::Vector2f{};
    p.vel += (want - p.vel) * std::min(1.0f, dt * 12.0f);
    if(length(want) > 0.0f && p.stance == Stance::PRONE)
    {
        float const bd = std::remainder(std::atan2(want.y, want.x) - p.body_angle, 6.2831853f);
        p.body_angle += bd * std::min(1.0f, dt * 3.0f);
    }

    // --- shoot -----------------------------------------------------------------
    const WeaponSpec& w = weapon_of(p.role);
    bool const can = !sprinting && p.reloading <= 0.0f && p.treating < 0 && p.dragging < 0
                  && (p.role != Role::GUNNER || p.deployed)
                  && (p.can_use_rifle() || p.role == Role::MEDIC || p.role == Role::OFFICER);
    if(can)
    {
        if(w.automatic ? m_trigger : m_trigger_pressed) p.firing = true;
        else if(!w.automatic && m_trigger && m_trigger_pressed) p.firing = true;
    }
    if(m_trigger_pressed && p.role == Role::GUNNER && !p.deployed && p.deploying <= 0.0f)
        hint("Right click to set the gun up first. It fires lying or crouched, not from the hip.", 3.0f);
    m_trigger_pressed = false;

    // --- bandaging yourself ---------------------------------------------------------
    if(m_bandage_time > 0.0f)
    {
        m_bandage_time -= dt;
        if(m_bandage_time <= 0.0f && p.bandages > 0)
        {
            --p.bandages;
            p.bleeding = 0.0f;
            p.bandaged = true;
            hint("Bandaged. The bleeding has stopped.", 2.5f);
        }
    }
}

// ---------------------------------------------------------------- update

void Battle::update(sf::Time delta_time)
{
    float const dt = delta_time.asSeconds();
    if(dt <= 0.0f || !m_ready) return;

    m_time += dt;
    m_stage_time += dt;
    Campaign::current().time += dt;

    if(!m_autoplay) player_input(dt);
    else
    {
        sf::View const& v = m_camera.get_view();
        m_mouse_world = v.getCenter() - v.getSize() * 0.5f + m_mouse_screen * 0.5f;
        m_mouse_world = player()->pos + sf::Vector2f{std::cos(player()->aim), std::sin(player()->aim)} * 60.0f;
    }

    for(auto& s : m_soldiers)
        if((!s.is_player || m_autoplay) && s.active && s.life == Life::OK) think(s, dt);
    for(auto& s : m_soldiers)
        if((!s.is_player || m_autoplay) && s.active && s.life == Life::OK) act(s, dt);
    for(auto& s : m_soldiers) update_soldier_body(s, dt);
    for(auto& s : m_soldiers)
        if(s.life == Life::OK) fire(s, dt);

    update_bullets(dt);
    update_effects(dt);
    update_script(dt);
    squad_orders(dt);

    // The camera leads toward where you are looking, further when aiming.
    Soldier& p = *player();
    sf::Vector2f look = m_mouse_world - p.pos;
    float const reach = m_aiming || p.deployed ? 110.0f : 40.0f;
    if(length(look) > 1.0f) look = look / length(look) * std::min(length(look) * 0.45f, reach);
    m_camera.follow(p.pos + look, delta_time, 5.0f);
    m_camera.update(delta_time);

    SoundBank::get().set_listener(p.pos);
}

void Battle::update_realtime(sf::Time delta_time)
{
    float const dt = delta_time.asSeconds();
    m_real += dt;
    m_hurt  = std::max(0.0f, m_hurt - dt * 0.6f);
    m_flash = std::max(0.0f, m_flash - dt * 2.5f);
    m_takeover = std::max(0.0f, m_takeover - dt);
    m_hint_time = std::max(0.0f, m_hint_time - dt);
    if(!m_lines.empty())
    {
        m_lines.front().time -= dt;
        if(m_lines.front().time <= 0.0f) m_lines.pop_front();
    }

    // Music follows the fighting: recent shots and how frightened you are.
    m_combat_heat = std::max(0.0f, m_combat_heat - dt * 0.12f);
    float target = std::clamp(m_combat_heat, 0.0f, 1.0f);
    if(m_stage == Stage::WON || m_stage == Stage::LOST) target = 0.0f;
    else if(m_stage >= Stage::ASSAULT && m_stage <= Stage::COUNTER) target = std::max(target, 0.55f);
    SoundBank::get().set_intensity(target);
    SoundBank::get().set_front(m_stage >= Stage::ASSAULT ? 0.3f : 0.55f);

    if(GameTime::is_paused()) SoundBank::get().stop_loops();
    SoundBank::get().update(dt);

    if(m_stage == Stage::WON || m_stage == Stage::LOST)
    {
        m_end_timer += dt;
        if(m_end_timer > 6.5f && m_end_timer < 100.0f)
        {
            finish();
            m_end_timer = 1000.0f;
            m_manager->request_scene_switch("debrief", SceneTransitionType::FADE);
        }
    }
}

// ---------------------------------------------------------------- the script

void Battle::set_stage(Stage stage)
{
    m_stage = stage;
    m_stage_time = 0.0f;
    Soldier* lt = officer(Team::MONTERAN);
    Soldier& p = *player();
    bool const you_lead = p.role == Role::OFFICER && p.alive();

    switch(stage)
    {
        case Stage::REAR:
            break;
        case Stage::RANGE:
            m_objective = "Break the bottles on the range";
            hint(p.role == Role::GUNNER ? "Right click to set the gun up, then left click to fire. R reloads."
                 : p.role == Role::FLAMER ? "Hold left click to burn. Short bursts: the fuel does not last."
                 : "Left click to fire. Hold right click to steady your aim. R reloads.", 9.0f);
            break;
        case Stage::FRONT:
            m_objective = "Wait in the front trench";
            if(lt && !lt->is_player) say(*lt, "There you are. Keep your head below the parapet.");
            hint("C crouches, Z goes prone. In a trench you are safe from almost everything but a shell.", 8.0f);
            for(auto& s : m_soldiers)
                if(s.team == Team::MONTERAN && !s.is_player && s.alive() && s.order == Order::HOLD && s.pos.x < 800)
                {
                    s.order = Order::ADVANCE;
                    set_goal(s, m_map.waypoints[0] + sf::Vector2f{0.0f, (static_cast<float>(s.id) - 3.5f) * 34.0f});
                }
            break;
        case Stage::SCOUT:
        {
            for(auto& s : m_soldiers)
                if(s.team == Team::RECHIVII && !s.has_post && !s.reserve) s.active = true;
            Soldier* kray = nullptr;
            for(auto& s : m_soldiers) if(s.team == Team::MONTERAN && s.role == Role::GUNNER && s.alive() && !s.is_player) kray = &s;
            if(kray) say(*kray, "Movement! Out by the wire!");
            else if(lt && !lt->is_player) say(*lt, "Movement by the wire. Take him.");
            m_objective = "Kill the scout";
            m_combat_heat = 0.6f;
            break;
        }
        case Stage::BARRAGE:
            m_objective = "Wait for the whistle";
            if(lt && !lt->is_player) say(*lt, "Our guns. When they lift, we go. Fix bayonets.", 4.5f);
            else say("Runner", "Guns are going in. Go on the lift, Lieutenant.", Team::MONTERAN, 4.0f);
            m_barrage_timer = 0.0f;
            for(int i = 0; i < 10; ++i)
            {
                sf::Vector2f const t{Random::real(1690, 1760), Random::real(130, 770)};
                m_shells.push_back({t, 1.5f + static_cast<float>(i) * 0.85f + Random::real(0.0f, 0.3f), false, true});
            }
            break;
        case Stage::ASSAULT:
            m_objective = you_lead ? "Lead them across. 3 sends them forward" : "Cross no man's land. Take their trench";
            SoundBank::get().play_at("whistle", lt ? lt->pos : p.pos, 1.0f);
            if(lt && !lt->is_player) say(*lt, "Over! Over the top! Go, go, go!", 3.0f);
            if(you_lead) hint("1 follow   2 hold   3 advance to the cursor   4 covering fire on the cursor", 12.0f);
            else hint("Move from cover to cover. Shift runs. Machine guns kill men who stand in the open.", 8.0f);
            for(auto& s : m_soldiers)
                if(s.team == Team::RECHIVII && s.has_post)
                {
                    s.active = true;
                    s.suppression = 1.0f;       // the barrage has only just lifted: heads are still down
                }
            m_waypoint = 0;
            m_waypoint_time = 100.0f;           // the first move goes straight away
            m_combat_heat = 1.0f;
            break;
        case Stage::TRENCH:
            m_objective = "Take the bunker";
            if(lt && !lt->is_player) say(*lt, "Their trench is ours! The bunker next, keep moving!");
            else say(p, "Trench is ours! Bunker, next!");
            m_barrage_timer = 3.0f;
            // What is left of their line falls back on the bunker and the farm.
            for(auto& s : m_soldiers)
                if(s.team == Team::RECHIVII && s.alive() && s.has_post && s.post.x < 1800)
                {
                    s.post = {Random::real(2120, 2380), Random::real(320, 440)};
                    s.path.clear();
                }
            break;
        case Stage::BUNKER:
            m_objective = "Take the farmhouse";
            if(lt && !lt->is_player) say(*lt, "Bunker's done. The farmhouse! Get inside!");
            else say(p, "Bunker's clear. To the farm!");
            break;
        case Stage::HOLD:
            m_objective = "Hold the farmhouse";
            if(lt && !lt->is_player) say(*lt, "Into the windows. They'll want it back. Make them pay for it.", 4.5f);
            else say(p, "Get to the walls. They'll come back.");
            break;
        case Stage::COUNTER:
        {
            m_objective = "Hold the farmhouse";
            for(auto& s : m_soldiers) if(s.reserve) { s.active = true; }
            Soldier* any = nullptr;
            for(auto& s : m_soldiers) if(s.team == Team::MONTERAN && s.alive() && !s.is_player) any = &s;
            if(any) say(*any, "Here they come! East, in the field!");
            m_combat_heat = 1.0f;
            break;
        }
        case Stage::WON:
            m_objective = "";
            Campaign::current().won = true;
            if(lt) say(*lt, "It's ours. God help us, it's ours.", 5.0f);
            else say(p, "It's ours.", 5.0f);
            SoundBank::get().play_at("whistle", p.pos, 0.6f);
            m_end_timer = 0.0f;
            break;
        case Stage::LOST:
            m_objective = "";
            say("", "The attack has failed.", Team::MONTERAN, 6.0f);
            m_end_timer = 0.0f;
            break;
    }
}

void Battle::update_script(float dt)
{
    Soldier& p = *player();

    auto enemies_in = [this](const sf::FloatRect& r)
    {
        int n = 0;
        for(const auto& s : m_soldiers) if(s.team == Team::RECHIVII && s.alive() && r.contains(s.pos)) ++n;
        return n;
    };
    auto friends_in = [this](const sf::FloatRect& r)
    {
        int n = 0;
        for(const auto& s : m_soldiers) if(s.team == Team::MONTERAN && s.alive() && r.contains(s.pos)) ++n;
        return n;
    };

    // Far-off guns, the whole time.
    m_far_guns -= dt;
    if(m_far_guns <= 0.0f)
    {
        m_far_guns = Random::real(4.0f, 11.0f);
        SoundBank::get().play("artillery_far", Random::real(0.25f, 0.5f), 0.1f);
        if(Random::chance(0.5f)) m_camera.shake(0.08f);
    }

    switch(m_stage)
    {
        case Stage::REAR:
            if(m_stage_time > 1.5f && m_stage_time < 1.6f)
            {
                Soldier* lt = officer(Team::MONTERAN);
                say("Sgt. Orban", p.name.substr(p.name.find(' ') + 1) + "! Up to the front trench, the Lieutenant wants everyone.", Team::MONTERAN, 4.5f);
                if(lt && !lt->is_player) say("Sgt. Orban", "Check your weapon on the range on the way. Bottles on the crates.", Team::MONTERAN, 4.5f);
                hint("WASD to walk. Aim with the mouse. Follow the trench east.", 9.0f);
                m_objective = "Go to the front trench";
            }
            // The others get up and file forward one at a time.
            for(auto& s : m_soldiers)
                if(s.team == Team::MONTERAN && !s.is_player && s.sitting && m_stage_time > 4.0f + static_cast<float>(s.id) * 2.5f)
                {
                    s.sitting = false;
                    s.order = Order::ADVANCE;
                    set_goal(s, m_map.waypoints[0] + sf::Vector2f{0.0f, (static_cast<float>(s.id) - 3.5f) * 34.0f});
                }
            if(m_map.range_area.contains(p.pos)) set_stage(Stage::RANGE);
            if(m_map.front_trench_area.contains(p.pos)) set_stage(Stage::FRONT);
            break;
        case Stage::RANGE:
            if(m_bottles_left == 0 && m_objective != "Go to the front trench")
            {
                m_objective = "Go to the front trench";
                say("Sgt. Orban", "Good enough. Now get up there.", Team::MONTERAN, 3.0f);
            }
            for(auto& s : m_soldiers)
                if(s.team == Team::MONTERAN && !s.is_player && s.sitting) { s.sitting = false; s.order = Order::ADVANCE; set_goal(s, m_map.waypoints[0] + sf::Vector2f{0.0f, (static_cast<float>(s.id) - 3.5f) * 34.0f}); }
            if(m_map.front_trench_area.contains(p.pos)) set_stage(Stage::FRONT);
            break;
        case Stage::FRONT:
            if(m_stage_time > 9.0f) set_stage(Stage::SCOUT);
            break;
        case Stage::SCOUT:
        {
            bool scout_gone = true;
            for(const auto& s : m_soldiers)
                if(s.team == Team::RECHIVII && !s.has_post && !s.reserve && s.alive()) scout_gone = false;
            if(scout_gone || m_stage_time > 30.0f) set_stage(Stage::BARRAGE);
            break;
        }
        case Stage::BARRAGE:
            if(m_stage_time > 11.5f) set_stage(Stage::ASSAULT);
            break;
        case Stage::ASSAULT:
            if(enemies_in(m_map.enemy_trench_area) == 0 && friends_in(m_map.enemy_trench_area) + friends_in(sf::FloatRect({1640, 100}, {200, 700})) > 0)
                set_stage(Stage::TRENCH);
            break;
        case Stage::TRENCH:
        {
            // Their guns answer: shells walk across no man's land and the trench.
            m_barrage_timer -= dt;
            if(m_barrage_timer <= 0.0f && m_stage_time < 40.0f)
            {
                m_barrage_timer = Random::real(2.0f, 3.5f);
                std::vector<sf::Vector2f> near;
                for(const auto& s : m_soldiers) if(s.team == Team::MONTERAN && s.alive()) near.push_back(s.pos);
                sf::Vector2f t = near.empty() ? sf::Vector2f{1600, 450} : near[static_cast<std::size_t>(Random::integer(0, static_cast<int>(near.size()) - 1))];
                float const a = Random::real(0.0f, 6.283f);
                t += sf::Vector2f{std::cos(a), std::sin(a)} * Random::real(35.0f, 110.0f);
                m_shells.push_back({t, 1.8f, false, false});
            }
            if(enemies_in(m_map.bunker_area) == 0 && friends_in(sf::FloatRect({1950, 340}, {170, 160})) > 0)
                set_stage(Stage::BUNKER);
            break;
        }
        case Stage::BUNKER:
            if(friends_in(m_map.farm_area) >= 1 && (m_map.farm_area.contains(p.pos) || friends_in(m_map.farm_area) >= 3))
                set_stage(Stage::HOLD);
            break;
        case Stage::HOLD:
            if(m_stage_time > 14.0f) set_stage(Stage::COUNTER);
            break;
        default:
            break;
    }

    // Victory and defeat, once there is fighting.
    if(m_stage >= Stage::ASSAULT && m_stage < Stage::WON)
    {
        bool reserve_waiting = false;
        for(const auto& s : m_soldiers) if(s.reserve && !s.active && s.alive()) reserve_waiting = true;
        if(count(Team::RECHIVII) == 0 && !reserve_waiting) set_stage(Stage::WON);
        else if(count(Team::RECHIVII) == 0 && reserve_waiting && m_stage < Stage::HOLD && m_stage != Stage::BUNKER)
        {
            if(m_stage == Stage::ASSAULT) set_stage(Stage::TRENCH);
            else if(m_stage == Stage::TRENCH) set_stage(Stage::BUNKER);
        }
    }
    if(m_stage < Stage::WON)
    {
        bool anyone = false;
        for(const auto& s : m_soldiers) if(s.team == Team::MONTERAN && (s.alive() || s.life == Life::DOWNED)) anyone = true;
        if(!anyone) set_stage(Stage::LOST);
    }

    // Their nerve: once the officer is gone and half of them are down, men start to run.
    if(m_stage >= Stage::ASSAULT)
    {
        m_rout_check -= dt;
        if(m_rout_check <= 0.0f)
        {
            m_rout_check = 4.0f;
            int lost = 0;
            for(const auto& s : m_soldiers) if(s.team == Team::RECHIVII && !s.alive()) ++lost;
            bool const no_officer = officer(Team::RECHIVII) == nullptr;
            for(auto& s : m_soldiers)
            {
                if(s.team != Team::RECHIVII || !s.alive() || !s.active) continue;
                if(no_officer && lost >= 9 && Random::chance(0.22f + 0.4f * s.suppression))
                {
                    s.morale = 0.0f;
                    bark(s, "Hesh! Hesh!", true);
                    continue;
                }
                // Men who see the enemy on top of them, frightened and alone, give up the line.
                float nearest = 1e9f;
                for(const auto& o : m_soldiers)
                    if(o.team == Team::MONTERAN && o.alive()) nearest = std::min(nearest, length(o.pos - s.pos));
                if(s.has_post && s.post.x < 1800 && nearest < 70.0f && s.suppression > 0.5f && Random::chance(0.35f))
                {
                    s.post = {Random::real(2120, 2380), Random::real(320, 440)};
                    s.path.clear();
                    s.suppression = 0.3f;
                    bark(s, "Nazad! Nazad!", true);
                }
            }
        }
    }

    // Losing the lieutenant.
    if(!m_officer_dead_noted && m_stage >= Stage::SCOUT && !officer(Team::MONTERAN) && p.role != Role::OFFICER)
    {
        m_officer_dead_noted = true;
        say("", "Varga is down. The squad looks to you now.", Team::MONTERAN, 4.0f);
        for(auto& s : m_soldiers)
            if(s.team == Team::MONTERAN && s.alive() && !s.is_player) { s.order = Order::FOLLOW; s.path.clear(); }
        hint("1 follow   2 hold   3 advance to the cursor   4 covering fire on the cursor", 10.0f);
    }

    // Your man has died: someone else picks up the fight.
    if(p.life == Life::DEAD || p.life == Life::FLED)
    {
        m_dead_wait += dt;
        if(m_dead_wait > 2.0f) { m_dead_wait = 0.0f; take_over(); }
    }
}

void Battle::squad_orders(float dt)
{
    Soldier& p = *player();
    Soldier* lt = officer(Team::MONTERAN);
    if(!lt && m_autoplay && p.alive()) lt = &p;     // someone has to call the moves
    bool const player_leads = !m_autoplay && ((p.role == Role::OFFICER && p.alive()) || !lt);
    if(!lt) return;
    if(player_leads || m_stage < Stage::ASSAULT || m_stage >= Stage::WON) return;

    m_waypoint_time += dt;

    int limit = 5;
    if(m_stage == Stage::TRENCH) limit = 7;
    if(m_stage >= Stage::BUNKER) limit = 8;

    int pinned = 0, total = 0;
    for(const auto& s : m_soldiers)
        if(s.team == Team::MONTERAN && s.alive() && (!s.is_player || m_autoplay))
        {
            ++total;
            if(s.suppression > 0.6f) ++pinned;
        }

    bool const move = m_waypoint < limit && m_waypoint_time > (m_waypoint == 0 ? 0.0f : 9.0f)
                   && pinned * 2 <= total && lt->suppression < 0.65f;
    if(!move) return;

    ++m_waypoint;
    m_waypoint_time = 0.0f;
    sf::Vector2f const point = m_map.waypoints[static_cast<std::size_t>(m_waypoint)];

    // The nest pins the field; the gunner keeps its head down while the
    // others move, for the first stretch.
    Soldier* nest = nullptr;
    for(auto& s : m_soldiers) if(s.team == Team::RECHIVII && s.role == Role::GUNNER && s.alive() && s.post.x < 1800) nest = &s;

    for(auto& s : m_soldiers)
    {
        if(s.team != Team::MONTERAN || (s.is_player && !m_autoplay) || !s.alive()) continue;
        s.path.clear();
        s.repath = 0.0f;
        s.order = Order::ADVANCE;
        if(s.role == Role::GUNNER && nest && s.id != lt->id)
        {
            s.order = Order::SUPPRESS;
            s.suppress_at = nest->pos;
            s.suppress_until = m_time + 25.0f;
        }
        // Spread out across the field, one lane each: bunched men die together.
        int lane = 0;
        for(const auto& o : m_soldiers) { if(o.id == s.id) break; if(o.team == Team::MONTERAN) ++lane; }
        float const offset = (static_cast<float>(lane) - 3.5f) * (m_waypoint >= 8 ? 14.0f : 38.0f);
        set_goal(s, point + sf::Vector2f{0.0f, offset});
    }
    if(m_waypoint > 1)
    {
        static const char* const calls[] = {"Up! Next cover, go!", "Move! Move!", "Forward! Keep low!", "Go, go! Don't stop in the open!"};
        bark(*lt, calls[m_waypoint % 4], true);
    }
}

void Battle::take_over()
{
    Soldier& old = *player();
    Soldier* best = nullptr;
    float best_d = 1e9f;
    for(auto& s : m_soldiers)
    {
        if(s.team != Team::MONTERAN || !s.alive() || s.id == old.id) continue;
        float const d = length(s.pos - old.pos);
        if(d < best_d) { best_d = d; best = &s; }
    }
    if(!best) return;

    old.is_player = false;
    best->is_player = true;
    best->path.clear();
    m_player = best->id;
    m_takeover = 4.0f;
    m_takeover_text = std::string("You are ") + best->rank + " " + best->name + ", " + role_name(best->role);
    m_trigger = m_aiming = false;
    m_camera.set_transition(TransitionFunction::EaseInOutQuad);
}

void Battle::finish()
{
    Campaign& c = Campaign::current();
    c.squad.clear();
    c.enemy_dead = c.enemy_fled = 0;
    for(auto& s : m_soldiers)
    {
        if(s.team == Team::MONTERAN)
        {
            Soldier copy = s;
            if(copy.fate.empty())
            {
                if(copy.life == Life::DOWNED) copy.fate = "Carried back to the aid post, barely alive";
                else if(copy.lost & ARM_L) copy.fate = "Survived. Lost his left arm";
                else if(copy.lost & ARM_R) copy.fate = "Survived. Lost his right arm";
                else if(copy.lost & (LEG_L | LEG_R)) copy.fate = "Survived. Lost a leg";
                else if(copy.hp < 60.0f) copy.fate = "Wounded, survived";
                else copy.fate = "Survived";
            }
            c.squad.push_back(copy);
        }
        else if(s.life == Life::FLED) ++c.enemy_fled;
        else if(!s.alive() && s.active) ++c.enemy_dead;
    }
}

}
