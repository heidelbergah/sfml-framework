/**
 * Battle.cpp  -  Mud Cats
 *
 * Setup, input, the platoon and its sections, and the levels' scripts. The
 * hill's waves, digging in and the mortars are in Hill.cpp.
 *
 * The platoon works the way a real one did. The lieutenant gives orders to
 * sections, not to men. A section's corporal turns "move up to that
 * hedge" into a place for each of his men, in cover, spread out. And the
 * sections leapfrog: one moves while the others keep the enemy's heads
 * down, then the next one goes.
 */

#include "Battle.hpp"

#include "Palette.hpp"
#include "Sound.hpp"

#include "../../include/core/Random.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace mudcats
{

namespace
{
    float length(sf::Vector2f v) { return std::sqrt(v.x * v.x + v.y * v.y); }
    sf::Vector2f dir(float a) { return {std::cos(a), std::sin(a)}; }

    const char* const RECHIVII_NAMES[] = {
        "Oslev", "Brannik", "Dvorek", "Halst", "Mirov", "Tesk", "Vadim", "Kosar", "Rudan", "Zelny",
        "Pavek", "Strom", "Ilko", "Gerd", "Yaro", "Lutz", "Borsk", "Kalen", "Matvo", "Resk",
        "Dravo", "Imre", "Sobek", "Varn", "Tolya", "Kresh", "Andrik", "Pellan", "Gosta", "Vey",
        "Hollin", "Radek", "Siro", "Mavik", "Darko", "Lesh", "Orvan", "Tibor", "Zavic", "Nemo"};

    const char* const MONTERAN_RIFLEMEN[] = {
        "Ilie Brandt", "Oren Mazur", "Paolo Serra", "Viktor Ames", "Luca Fenn", "Radu Pell", "Ion Carver",
        "Mihai Stoll", "Andrei Holm", "Simon Lark", "Dario Kesh", "Nico Varro", "Teo Danes", "Emeric Volt",
        "Gaspar Wren", "Iosif Brann", "Matei Lusk"};

    // The Rechivii sections on the fields, in the order spawn_fields makes
    // them, after our three.
    constexpr int FIELDS_PATROL = 3;
    constexpr int FIELDS_MILL   = 4;

    // How a hint reads for each weapon, the first time you are on the range.
    const char* range_hint(Gun gun)
    {
        switch(gun)
        {
            case Gun::MG:     return "Right click sets the gun up; then left click fires. R reloads. Watch the barrel heat.";
            case Gun::FLAMER: return "Hold left click to burn. Short bursts: the fuel doesn't last.";
            case Gun::SNIPER: return "Hold right click to look a long way out and steady the scope. Each shot gives you away.";
            default:          return "Left click fires. Hold right click to steady your aim and look further. R reloads.";
        }
    }
}

const char* difficulty_name(int d)
{
    return d <= 0 ? "Recruit" : (d == 1 ? "Regular" : "Veteran");
}

const char* level_name(LevelId level)
{
    switch(level)
    {
        case LevelId::TRENCHES: return "The Ochre line";
        case LevelId::FIELDS:   return "Halvek fields";
        case LevelId::RAID:     return "The Dravo raid";
        default:                return "Hill 312";
    }
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

    Campaign& c = Campaign::current();
    m_level = c.level;
    c.won = false;
    c.time = 0.0f;
    c.prisoners = c.prisoners_shot = 0;
    c.log.clear();

    build_world();
    m_soldiers.clear();
    m_sections.clear();
    if(m_level == LevelId::TRENCHES) spawn_trenches();
    else if(m_level == LevelId::FIELDS) spawn_fields();
    else if(m_level == LevelId::RAID) spawn_raid();
    else spawn_hill();

    m_stage_time = m_time = m_real = 0.0f;
    m_waypoint = 0;
    m_bounding = 0;
    m_waypoint_time = 0.0f;
    m_end_timer = 0.0f;
    m_combat_heat = 0.0f;
    m_bottles_left = 3;
    m_officer_dead_noted = false;
    m_hurt = m_flash = m_takeover = m_notice = 0.0f;
    m_up = m_down = m_left = m_right = m_sprint = m_trigger = m_aiming = false;
    m_selected = -1;
    m_mist = m_level == LevelId::FIELDS ? 0.9f : 0.0f;
    m_night = m_level == LevelId::RAID ? 1.0f : 0.0f;
    m_alarm = m_level != LevelId::RAID;
    m_creep = 0;
    m_illum_timer = 6.0f;
    m_missions.clear();
    m_kick = {};
    m_bound_wait = -1;
    m_cover_until = 0.0f;
    m_assault = false;
    m_keep_up = 0.0f;
    m_smoke_mission = false;
    m_look = {};
    m_wave = m_waves_held = 0;
    m_wave_dirs.clear();
    m_hill_dead = m_hill_fled = m_new_men = 0;
    m_night_target = 0.0f;
    m_wave_broke = false;
    c.waves = 0;
    // For recording and testing: your man is driven like the others.
    m_autoplay = std::getenv("MUDCATS_AUTOPLAY") != nullptr;
    set_stage(m_level == LevelId::TRENCHES ? Stage::REAR : (m_level == LevelId::FIELDS ? Stage::MARCH : (m_level == LevelId::RAID ? Stage::RAID_CREEP : Stage::HILL_LULL)));

    // Shortcuts for working on later parts of a level.
    if(const char* env = std::getenv("MUDCATS_STAGE"))
    {
        int const s = std::atoi(env);
        if(m_level == LevelId::TRENCHES && s >= 1)
        {
            for(auto& f : m_soldiers)
                if(f.team == Team::MONTERAN && f.pos.x < 800.0f)
                {
                    f.pos = f.goal = {850.0f, 120.0f + 26.0f * static_cast<float>(f.id)};
                    f.sitting = false;
                }
            set_stage(s >= 2 ? Stage::BARRAGE : Stage::FRONT);
            if(s >= 3)
            {
                sf::Vector2f const at = s >= 4 ? sf::Vector2f{2320.0f, 380.0f} : sf::Vector2f{1716.0f, 420.0f};
                for(auto& f : m_soldiers)
                {
                    if(f.team == Team::MONTERAN) { f.pos = f.goal = at + sf::Vector2f{Random::real(-25, 25), static_cast<float>(f.id * 9) - 110.0f}; f.order = Order::HOLD; }
                    else if(!f.reserve && (f.post.x < 1800 || s >= 4)) { f.life = Life::DEAD; f.active = false; }
                }
                set_stage(s >= 4 ? Stage::HOLD : Stage::TRENCH);
                m_waypoint = s >= 4 ? 8 : 5;
            }
        }
        if(m_level == LevelId::FIELDS && s >= 1)
        {
            for(auto& f : m_soldiers)
                if(f.team == Team::MONTERAN) f.pos = f.goal = {Random::real(880, 980), Random::real(490, 550)};
            set_stage(Stage::CONTACT);
            if(s >= 2)
            {
                for(auto& f : m_soldiers)
                {
                    if(f.team == Team::MONTERAN) f.pos = f.goal = {Random::real(1500, 1600), Random::real(470, 600)};
                    else if(f.section == FIELDS_PATROL) { f.life = Life::DEAD; f.active = false; }
                }
                m_waypoint = 3;
                set_stage(Stage::STREAM);
            }
            if(s >= 3)
            {
                for(auto& f : m_soldiers)
                {
                    if(f.team == Team::MONTERAN) f.pos = f.goal = {Random::real(1760, 1870), Random::real(420, 545)};
                    else if(!f.reserve) { f.life = Life::DEAD; f.active = false; }
                }
                m_waypoint = 5;
                set_stage(Stage::COUNTER);
            }
        }
    }

    if(const char* env = std::getenv("MUDCATS_STAGE"); env && m_level == LevelId::RAID)
    {
        int const s = std::atoi(env);
        if(s >= 1)
        {
            // Up to the wire, unseen.
            for(auto& f : m_soldiers)
                if(f.team == Team::MONTERAN) f.pos = f.goal = {Random::real(600, 630), 450.0f + static_cast<float>(f.id % 6 - 3) * 14.0f};
            for(auto& sec : m_sections) if(sec.team == Team::MONTERAN) sec.at = 2;
        }
        if(s >= 2)
        {
            // In their trench, the alarm already up.
            for(auto& f : m_soldiers)
            {
                if(f.team == Team::MONTERAN) f.pos = f.goal = {Random::real(808, 822), 400.0f + static_cast<float>(f.id % 8) * 16.0f};
                else if(f.section >= 0 && f.section <= 3 && f.post.x < 900.0f) { f.life = Life::DEAD; f.active = false; }
            }
            m_map.cut_wire({671, 450}, 20.0f);
            m_map.cut_wire({731, 450}, 20.0f);
            for(auto& sec : m_sections) if(sec.team == Team::MONTERAN) sec.at = 4;
            raise_alarm("");
            set_stage(Stage::RAID_HAMLET);
        }
    }

    if(const char* env = std::getenv("MUDCATS_STAGE"); env && m_level == LevelId::HILL)
    {
        // Straight to wave N, the lull before it nearly over.
        int const wave = std::max(1, std::atoi(env));
        m_wave = m_waves_held = wave - 1;
        m_night = std::clamp((static_cast<float>(m_wave) - 2.0f) / 4.0f, 0.0f, 1.0f);
        m_stage_time = 40.0f;
    }

    if(const char* speed = std::getenv("MUDCATS_SPEED")) GameTime::set_scale(static_cast<float>(std::atof(speed)));

    m_camera.set_bounds(sf::FloatRect({0.0f, 0.0f}, m_map.size));
    m_camera.set_center(player()->pos, true);

    SoundBank& sound = SoundBank::get();
    sound.set_dread(m_level == LevelId::HILL);
    sound.start_music();
    sound.set_intensity(0.0f);
    sound.set_front(m_level == LevelId::TRENCHES ? 0.55f : (m_level == LevelId::RAID || m_level == LevelId::HILL ? 0.3f : 0.0f));
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
        if(!m_frame.resize(FRAME) || !m_shade.resize(FRAME) || !m_light.resize(FRAME)) return;
        m_light.setSmooth(true);
        m_frame.setSmooth(false);
        m_shade.setSmooth(false);
        m_grade = m_manager->get_shaders().load_fragment("mudcats_grade", "shaders/mudcats/grade.frag");
        m_ready = true;
    }
    m_map.build(m_level);
    sf::Vector2u const want{static_cast<unsigned>(m_map.size.x), static_cast<unsigned>(m_map.size.y)};
    if(!m_built_once || m_ground.getSize() != want)
    {
        if(!m_ground.resize(want)) { m_ready = false; return; }
        m_built_once = true;
    }
    m_map.render(m_ground);

    m_bullets.clear(); m_grenades.clear(); m_flames.clear(); m_fires.clear(); m_smokes.clear(); m_flares.clear();
    m_gibs.clear(); m_dropped.clear(); m_lights.clear(); m_shells.clear(); m_lines.clear(); m_stamps.clear(); m_later.clear();
    m_mortars.clear(); m_pending.clear();
    m_fx.clear();
    setup_effects();
}

int Battle::add_section(Team team, std::string name, float band)
{
    Section s;
    s.id = static_cast<int>(m_sections.size());
    s.team = team;
    s.name = std::move(name);
    s.band = band;
    m_sections.push_back(s);
    return s.id;
}

void Battle::kit_out(Soldier& s)
{
    Role const role = s.role;
    s.gun = default_gun(role);
    const WeaponSpec& w = s.weapon();
    s.ammo     = w.magazine;
    s.spare    = w.spare;
    s.fuel     = w.flame ? 100.0f : 0.0f;
    s.grenades = 0;
    s.smokes = s.flares = s.satchels = s.missions = s.medkits = 0;
    switch(role)
    {
        case Role::RIFLEMAN:     s.grenades = 2; break;
        case Role::OFFICER:      s.grenades = 2; s.smokes = 2; s.flares = s.team == Team::MONTERAN ? 2 : 0; break;
        case Role::MEDIC:        s.medkits = 8; break;
        case Role::RADIOMAN:     s.grenades = 1; s.missions = 3; break;
        case Role::STORMTROOPER: s.grenades = 5; s.smokes = 1; break;
        case Role::SAPPER:       s.grenades = 1; s.satchels = 2; break;
        default: break;
    }
    // On the hill everyone has a shovel and some empty sandbags.
    s.bags = m_level == LevelId::HILL && s.team == Team::MONTERAN ? 3 : 0;
}

void Battle::fill_roles(int section)
{
    // Whatever you chose, there is one of him in your section: if the
    // section doesn't have one, a rifleman becomes one.
    Role const chosen = Campaign::current().chosen;
    if(chosen == Role::OFFICER) return;
    for(const auto& s : m_soldiers) if(s.section == section && s.role == chosen) return;
    Soldier* swap = nullptr;
    for(auto& s : m_soldiers)
        if(s.section == section && !s.leader && (s.role == Role::RIFLEMAN || s.role == Role::STORMTROOPER)) swap = &s;
    if(!swap) return;
    swap->role = chosen;
    kit_out(*swap);
}

void Battle::pick_player(int section)
{
    Role const chosen = Campaign::current().chosen;
    m_player = -1;
    // Your man: one of that role in your section, the corporal only if
    // there is no one else. The officer is the officer.
    for(int pass = 0; pass < 2 && m_player < 0; ++pass)
        for(auto& s : m_soldiers)
        {
            if(s.team != Team::MONTERAN || s.role != chosen) continue;
            if(chosen != Role::OFFICER && s.section != section) continue;
            if(pass == 0 && s.leader && chosen != Role::OFFICER) continue;
            m_player = s.id;
            break;
        }
    if(m_player < 0)
        for(auto& s : m_soldiers) if(s.team == Team::MONTERAN && s.section == section && !s.leader) { m_player = s.id; break; }
    if(m_player < 0) m_player = 0;
    m_soldiers[static_cast<std::size_t>(m_player)].is_player = true;
}

Soldier& Battle::add(Team team, Role role, std::string rank, std::string name, sf::Vector2f pos, int section)
{
    Soldier s;
    s.id      = static_cast<int>(m_soldiers.size());
    s.team    = team;
    s.role    = role;
    s.gun     = default_gun(role);
    s.rank    = std::move(rank);
    s.name    = std::move(name);
    s.pos     = pos;
    s.goal    = pos;
    s.section = section;
    s.aim     = team == Team::MONTERAN ? 0.0f : 3.14159f;

    kit_out(s);
    s.think    = Random::real(0.0f, 0.3f);
    s.morale   = Random::real(0.8f, 1.0f);
    s.order    = Order::HOLD;
    m_soldiers.push_back(s);
    return m_soldiers.back();
}

namespace
{
    // A section's men, in order: who leads it, then the rest.
    struct Member { Role role; const char* rank; const char* name; };
}

void Battle::spawn_trenches()
{
    Role const chosen = Campaign::current().chosen;

    // --- the platoon -----------------------------------------------------------
    int const one   = add_section(Team::MONTERAN, "1 Section", -150.0f);
    int const two   = add_section(Team::MONTERAN, "2 Section", 0.0f);
    int const three = add_section(Team::MONTERAN, "3 Section", 150.0f);

    Soldier& lt = add(Team::MONTERAN, Role::OFFICER, "Lt.", "Aurel Varga", {852, 440}, -1);

    // 2 Section, yours, waits behind the lines.
    const Member yours[] = {
        {Role::RIFLEMAN, "Cpl.", "Petru Orban"}, {Role::RIFLEMAN, "Pvt.", "Emil Rell"},
        {Role::GUNNER, "Cpl.", "Anselm Kray"},  {Role::FLAMER, "Pvt.", "Dorian Vosk"},
        {Role::MEDIC, "Pvt.", "Teodor Halm"},   {Role::RADIOMAN, "Pvt.", "Tomas Dacre"},
        {Role::RIFLEMAN, "Pvt.", "Lio Marchetti"}, {Role::RIFLEMAN, "Pvt.", "Benedek Sorn"}};
    const sf::Vector2f rear[] = {{360, 276}, {110, 262}, {300, 282}, {245, 262}, {190, 226}, {330, 280}, {70, 262}, {275, 274}};
    for(int i = 0; i < 8; ++i)
    {
        Soldier& s = add(Team::MONTERAN, yours[i].role, yours[i].rank, yours[i].name, rear[i], two);
        s.leader = i == 0;
        s.sitting = i != 1;
    }

    // 1 and 3 Sections are already up in the front trench.
    std::size_t name = 0;
    auto front_section = [&](int section, float y0, bool medic)
    {
        for(int i = 0; i < 8; ++i)
        {
            Role role = i == 1 ? Role::GUNNER : (medic && i == 2 ? Role::MEDIC : Role::RIFLEMAN);
            if(i == 3) role = medic ? Role::SAPPER : Role::SNIPER;
            std::string const who = MONTERAN_RIFLEMEN[name++ % 17];
            Soldier& s = add(Team::MONTERAN, role, i == 0 ? "Cpl." : "Pvt.", who, {850.0f, y0 + 36.0f * static_cast<float>(i)}, section);
            s.leader = i == 0;
        }
    };
    front_section(one, 130.0f, false);
    front_section(three, 480.0f, true);

    // Put the player in his man's boots.
    (void)chosen;
    (void)lt;
    fill_roles(two);
    pick_player(two);
    Soldier& p = m_soldiers[static_cast<std::size_t>(m_player)];
    p.is_player = true;
    p.sitting = false;
    // Whoever you are, you start at the back with 2 Section.
    if(p.role == Role::OFFICER) std::swap(p.pos, m_soldiers[2].pos);
    else if(p.name != "Emil Rell") std::swap(p.pos, m_soldiers[2].pos);
    for(auto& s : m_soldiers) s.goal = s.pos;

    // --- the Rechivii ----------------------------------------------------------
    int n = 0;
    int const ra = add_section(Team::RECHIVII, "north", 0.0f);
    int const rb = add_section(Team::RECHIVII, "south", 0.0f);
    int const rc = add_section(Team::RECHIVII, "bunker", 0.0f);
    int const rd = add_section(Team::RECHIVII, "reserve", 0.0f);

    auto enemy = [&](Role role, sf::Vector2f post, int section, bool active, bool reserve) -> Soldier&
    {
        Soldier& s = add(Team::RECHIVII, role, role == Role::OFFICER ? "Lt." : "", RECHIVII_NAMES[n++ % 40], post, section);
        s.post = post;
        s.has_post = true;
        s.active = active;
        s.reserve = reserve;
        s.stance = Stance::CROUCH;
        return s;
    };
    auto trench_spot = [this](float y)
    {
        sf::Vector2f best{1712.0f, y};
        float best_d = 1e9f;
        for(const auto& c : m_map.cover)
            if(c.trench && c.pos.x > 1690 && c.pos.x < 1745 && std::abs(c.pos.y - y) < best_d)
            {
                best_d = std::abs(c.pos.y - y);
                best = c.pos;
            }
        return best;
    };

    Soldier& scout = enemy(Role::RIFLEMAN, {1500, 470}, ra, false, false);
    scout.has_post = false;
    scout.stance = Stance::PRONE;
    scout.body_angle = 3.14159f;

    Soldier& nest = enemy(Role::GUNNER, {1668, 334}, ra, true, false);
    nest.deployed = true;
    nest.deploy_dir = 3.14159f;
    nest.stance = Stance::PRONE;
    nest.body_angle = 3.14159f;
    enemy(Role::RIFLEMAN, {1708, 326}, ra, true, false).leader = true;
    for(float y : {150.0f, 200.0f, 250.0f, 295.0f, 380.0f, 405.0f})
        enemy(y == 200.0f ? Role::SNIPER : Role::RIFLEMAN, trench_spot(y), ra, true, false);     // a sniper among them

    enemy(Role::OFFICER, trench_spot(430.0f), -1, true, false);
    enemy(Role::RIFLEMAN, trench_spot(470.0f), rb, true, false).leader = true;
    Soldier& south_mg = enemy(Role::GUNNER, trench_spot(600.0f), rb, true, false);
    south_mg.deploy_dir = 3.14159f;
    enemy(Role::MEDIC, trench_spot(540.0f), rb, true, false);
    for(float y : {500.0f, 560.0f, 650.0f, 700.0f, 740.0f, 775.0f})
        enemy(y == 650.0f ? Role::FLAMER : Role::RIFLEMAN, trench_spot(y), rb, true, false);

    Soldier& bmg = enemy(Role::GUNNER, {2014, 414}, rc, true, false);
    bmg.deployed = true;
    bmg.deploy_dir = 3.14159f;
    enemy(Role::FLAMER, {2042, 428}, rc, true, false);
    enemy(Role::RIFLEMAN, {2040, 405}, rc, true, false).leader = true;
    for(sf::Vector2f at : {sf::Vector2f{1846, 290}, {1862, 232}, {1940, 612}, {1966, 532}, {1850, 262}, {1952, 578}, {2026, 440}})
        enemy(Role::RIFLEMAN, at, rc, true, false);

    // A mortar behind the ruins, which opens up once we're over the top.
    {
        Soldier& g = enemy(Role::RIFLEMAN, {1944, 336}, -1, true, false);
        Soldier& l = enemy(Role::RIFLEMAN, {1950, 346}, -1, true, false);
        place_mortar({1952, 334}, 3.14159f, g.id, l.id);
        g.serving = l.serving = static_cast<int>(m_mortars.size()) - 1;
    }

    // The counter-attack, with stormtroopers and a flamethrower in it.
    for(int i = 0; i < 11; ++i)
    {
        Role const role = i == 3 ? Role::GUNNER : (i == 5 || i == 6 ? Role::STORMTROOPER : (i == 8 ? Role::FLAMER : Role::RIFLEMAN));
        Soldier& r = enemy(role, {2690.0f, 270.0f + 20.0f * static_cast<float>(i)}, rd, false, true);
        r.has_post = false;
        r.stance = Stance::STAND;
        r.leader = i == 0;
    }
}

void Battle::spawn_fields()
{
    Role const chosen = Campaign::current().chosen;

    int const one   = add_section(Team::MONTERAN, "1 Section", -150.0f);
    int const two   = add_section(Team::MONTERAN, "2 Section", 0.0f);
    int const three = add_section(Team::MONTERAN, "3 Section", 150.0f);

    // In file up the lane: 1 Section leading, the lieutenant, 2 Section, 3 Section.
    auto lane_y = [this](float x)
    {
        const auto& pts = m_map.trenches.front().points;
        for(std::size_t i = 0; i + 1 < pts.size(); ++i)
            if(x >= pts[i].x && x <= pts[i + 1].x)
                return pts[i].y + (pts[i + 1].y - pts[i].y) * (x - pts[i].x) / (pts[i + 1].x - pts[i].x);
        return 520.0f;
    };
    float x = 640.0f;
    auto place = [&]() { sf::Vector2f const p{x, lane_y(x) + Random::real(-5, 5)}; x -= 19.0f; return p; };

    std::size_t name = 0;
    auto make_section = [&](int id, bool medic)
    {
        for(int i = 0; i < 8; ++i)
        {
            Role role = i == 1 ? Role::GUNNER : (medic && i == 2 ? Role::MEDIC : Role::RIFLEMAN);
            if(i == 3) role = medic ? Role::SAPPER : Role::SNIPER;
            Soldier& s = add(Team::MONTERAN, role, i == 0 ? "Cpl." : "Pvt.", MONTERAN_RIFLEMEN[name++ % 17], place(), id);
            s.leader = i == 0;
        }
    };
    make_section(one, false);
    add(Team::MONTERAN, Role::OFFICER, "Lt.", "Aurel Varga", place(), -1);
    const Member yours[] = {
        {Role::RIFLEMAN, "Cpl.", "Petru Orban"}, {Role::RIFLEMAN, "Pvt.", "Emil Rell"},
        {Role::GUNNER, "Cpl.", "Anselm Kray"},  {Role::FLAMER, "Pvt.", "Dorian Vosk"},
        {Role::MEDIC, "Pvt.", "Teodor Halm"},   {Role::RADIOMAN, "Pvt.", "Tomas Dacre"},
        {Role::RIFLEMAN, "Pvt.", "Lio Marchetti"}, {Role::RIFLEMAN, "Pvt.", "Benedek Sorn"}};
    for(int i = 0; i < 8; ++i) add(Team::MONTERAN, yours[i].role, yours[i].rank, yours[i].name, place(), two).leader = i == 0;
    make_section(three, true);

    (void)chosen;
    fill_roles(two);
    pick_player(two);
    for(auto& s : m_soldiers) s.aim = 0.0f;

    // --- the Rechivii -------------------------------------------------------------
    int n = 0;
    int const patrol = add_section(Team::RECHIVII, "patrol", 0.0f);
    int const mill   = add_section(Team::RECHIVII, "mill", 0.0f);
    int const main   = add_section(Team::RECHIVII, "main body", 0.0f);
    int const counter = add_section(Team::RECHIVII, "reserve", 0.0f);

    auto enemy = [&](Role role, sf::Vector2f post, int section, bool active, bool reserve) -> Soldier&
    {
        Soldier& s = add(Team::RECHIVII, role, role == Role::OFFICER ? "Lt." : "", RECHIVII_NAMES[n++ % 40], post, section);
        s.post = post;
        s.has_post = true;
        s.active = active;
        s.reserve = reserve;
        s.stance = Stance::CROUCH;
        return s;
    };

    // A patrol in the wood, which we walk into.
    for(sf::Vector2f p : {sf::Vector2f{1180, 400}, {1230, 372}, {1290, 410}, {1340, 360}, {1400, 395}, {1260, 330}})
        enemy(p.x == 1340.0f ? Role::SNIPER : Role::RIFLEMAN, p, patrol, false, false);
    m_soldiers.back().leader = true;

    // The mill and the far bank.
    Soldier& window = enemy(Role::GUNNER, {1764, 486}, mill, false, false);
    window.deploy_dir = 3.14159f;
    for(sf::Vector2f p : {sf::Vector2f{1840, 440}, {1850, 530}, {1922, 455}, {1930, 520},
                          {1800, 372}, {1840, 590}, {1764, 430}})
        enemy(Role::RIFLEMAN, p, mill, false, false);
    m_soldiers.back().leader = true;

    // Their main body, marching in from the east to meet us.
    enemy(Role::OFFICER, {1990, 500}, -1, false, true);
    enemy(Role::MEDIC, {2000, 520}, main, false, true);
    Soldier& main_mg = enemy(Role::GUNNER, {1962, 462}, main, false, true);
    main_mg.deploy_dir = 3.14159f;
    for(sf::Vector2f p : {sf::Vector2f{1962, 540}, {2140, 420}, {2140, 620}, {1975, 445}, {2060, 460}, {2060, 545}})
        enemy(Role::RIFLEMAN, p, main, false, true);
    m_soldiers.back().leader = true;
    for(auto& s : m_soldiers)
        if(s.section == main || (s.team == Team::RECHIVII && s.role == Role::OFFICER))
        {
            s.pos = {2780.0f, 440.0f + Random::real(0, 120)};
            s.goal = s.pos;
        }

    // A mortar in the orchard, ranged on the fields in front of it.
    {
        Soldier& g = enemy(Role::RIFLEMAN, {2236, 302}, -1, true, false);
        Soldier& l = enemy(Role::RIFLEMAN, {2246, 312}, -1, true, false);
        place_mortar({2244, 300}, 3.14159f, g.id, l.id);
        g.serving = l.serving = static_cast<int>(m_mortars.size()) - 1;
        m_mortars.back().rounds = 16;
    }

    // And a reserve section for later, with stormtroopers and a flamethrower.
    for(int i = 0; i < 13; ++i)
    {
        Role const role = i == 2 ? Role::GUNNER : (i == 4 ? Role::STORMTROOPER : (i == 7 ? Role::FLAMER : Role::RIFLEMAN));
        Soldier& r = enemy(role, {2780.0f, 260.0f + 34.0f * static_cast<float>(i)}, counter, false, true);
        r.has_post = false;
        r.stance = Stance::STAND;
        r.leader = i == 0;
    }
}

void Battle::spawn_raid()
{
    // --- the raiding party: the lieutenant and two assault groups ---------------------------
    int const one = add_section(Team::MONTERAN, "1 Group", -250.0f);
    int const two = add_section(Team::MONTERAN, "2 Group", 270.0f);
    add(Team::MONTERAN, Role::OFFICER, "Lt.", "Aurel Varga", {126, 452}, -1);
    const Member first[] = {
        {Role::STORMTROOPER, "Cpl.", "Ilie Brandt"}, {Role::STORMTROOPER, "Pvt.", "Emil Rell"},
        {Role::STORMTROOPER, "Pvt.", "Paolo Serra"}, {Role::FLAMER, "Pvt.", "Dorian Vosk"}, {Role::SAPPER, "Pvt.", "Radu Pell"}};
    const Member second[] = {
        {Role::STORMTROOPER, "Cpl.", "Oren Mazur"}, {Role::STORMTROOPER, "Pvt.", "Luca Fenn"},
        {Role::GUNNER, "Pvt.", "Anselm Kray"}, {Role::MEDIC, "Pvt.", "Teodor Halm"}, {Role::RADIOMAN, "Pvt.", "Tomas Dacre"}};
    for(int i = 0; i < 5; ++i)
    {
        add(Team::MONTERAN, first[i].role, first[i].rank, first[i].name, {124.0f + (i % 2) * 6.0f, 384.0f + 13.0f * static_cast<float>(i)}, one).leader = i == 0;
        add(Team::MONTERAN, second[i].role, second[i].rank, second[i].name, {128.0f - (i % 2) * 6.0f, 470.0f + 13.0f * static_cast<float>(i)}, two).leader = i == 0;
    }
    fill_roles(one);
    pick_player(one);
    for(auto& s : m_soldiers) { s.aim = 0.0f; s.stance = Stance::CROUCH; s.goal = s.pos; }

    // --- the Rechivii: most of them asleep ------------------------------------------------------
    int n = 0;
    int const watch  = add_section(Team::RECHIVII, "watch", 0.0f);
    int const line   = add_section(Team::RECHIVII, "trench", 0.0f);
    int const hamlet = add_section(Team::RECHIVII, "hamlet", 0.0f);
    int const hq     = add_section(Team::RECHIVII, "headquarters", 0.0f);
    int const reserve = add_section(Team::RECHIVII, "reserve", 0.0f);
    auto enemy = [&](Role role, sf::Vector2f post, int section, bool awake, bool in_reserve) -> Soldier&
    {
        Soldier& s = add(Team::RECHIVII, role, role == Role::OFFICER ? "Lt." : "", RECHIVII_NAMES[n++ % 40], post, section);
        s.post = post;
        s.has_post = true;
        s.active = awake;
        s.alerted = awake;
        s.reserve = in_reserve;
        s.stance = Stance::CROUCH;
        return s;
    };

    // The watch: two in the listening post, two on the fire step.
    for(sf::Vector2f p : {sf::Vector2f{688, 478}, {702, 486}, {806, 350}, {814, 566}})
        enemy(Role::RIFLEMAN, p, watch, true, false);
    m_soldiers.back().leader = true;
    // The machine guns, their crews dozing beside them.
    for(sf::Vector2f p : {sf::Vector2f{782, 300}, {796, 620}})
    {
        Soldier& g = enemy(Role::GUNNER, p, line, false, false);
        g.deploy_dir = 3.14159f;
    }
    // The off-watch, asleep in the two dugouts. When the alarm goes they
    // come out to the fire step.
    float const steps[] = {170, 210, 250, 380, 520, 680, 720, 760};
    for(int i = 0; i < 8; ++i)
    {
        sf::Vector2f const bed = i < 4 ? sf::Vector2f{858.0f + static_cast<float>(i % 2) * 14.0f, 210.0f + static_cast<float>(i / 2) * 14.0f}
                                       : sf::Vector2f{862.0f + static_cast<float>(i % 2) * 14.0f, 704.0f + static_cast<float>((i - 4) / 2) * 14.0f};
        Soldier& s = enemy(Role::RIFLEMAN, bed, line, false, false);
        s.post = {i == 3 ? 800.0f : 812.0f, steps[i]};
        s.sitting = true;
    }
    m_soldiers.back().leader = true;
    // The hamlet.
    for(sf::Vector2f p : {sf::Vector2f{1030, 300}, {1060, 330}, {1060, 590}, {1095, 616}, {1210, 410}, {1270, 450}, {1290, 400}})
        enemy(Role::RIFLEMAN, p, hamlet, false, false);
    m_soldiers.back().leader = true;
    // Their headquarters, in the dugout.
    enemy(Role::OFFICER, {1440, 470}, -1, false, false);
    for(sf::Vector2f p : {sf::Vector2f{1420, 460}, {1424, 486}, {1446, 488}})
        enemy(Role::RIFLEMAN, p, hq, false, false);
    m_soldiers.back().leader = true;
    // And a reserve section in billets to the east, who will come.
    for(int i = 0; i < 6; ++i)
    {
        Soldier& r = enemy(i == 1 ? Role::GUNNER : (i == 3 ? Role::STORMTROOPER : Role::RIFLEMAN), {1690.0f, 380.0f + 26.0f * static_cast<float>(i)}, reserve, false, true);
        r.has_post = false;
        r.stance = Stance::STAND;
        r.leader = i == 0;
    }
}

// ---------------------------------------------------------------- helpers

Soldier* Battle::player()
{
    if(m_player < 0 || m_player >= static_cast<int>(m_soldiers.size())) return &m_soldiers.front();
    return &m_soldiers[static_cast<std::size_t>(m_player)];
}

const Soldier& Battle::me() const
{
    return m_soldiers[static_cast<std::size_t>(std::clamp(m_player, 0, static_cast<int>(m_soldiers.size()) - 1))];
}

Soldier* Battle::officer(Team team)
{
    for(auto& s : m_soldiers)
        if(s.team == team && s.role == Role::OFFICER && s.fighting()) return &s;
    return nullptr;
}

Soldier* Battle::section_leader(int section)
{
    if(section < 0) return nullptr;
    for(auto& s : m_soldiers)
        if(s.section == section && s.leader && s.fighting()) return &s;
    return nullptr;
}

int Battle::count(Team team) const
{
    int n = 0;
    for(const auto& s : m_soldiers)
        if(s.team == team && s.fighting()) ++n;
    return n;
}

bool Battle::player_leads() const
{
    if(m_autoplay) return false;
    const Soldier& p = me();
    if(!p.alive()) return false;
    if(p.role == Role::OFFICER) return true;
    for(const auto& s : m_soldiers)
        if(s.team == Team::MONTERAN && s.role == Role::OFFICER && s.fighting()) return false;
    return true;
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

void Battle::notice(const std::string& text, float time)
{
    m_notice_text = text;
    m_notice = time;
}

void Battle::bark(Soldier& s, const std::string& text, bool voice)
{
    if(s.bark_cooldown > 0.0f || !s.alive()) return;
    s.bark = text;
    s.bark_time = 1.6f;
    s.bark_cooldown = Random::real(3.5f, 6.0f);
    if(voice) SoundBank::get().play_at("bark", s.pos, 0.55f, 0.08f);
}

void Battle::log(const std::string& line)
{
    int const secs = static_cast<int>(m_time);
    char stamp[16];
    std::snprintf(stamp, sizeof stamp, "%d:%02d  ", secs / 60, secs % 60);
    Campaign::current().log.push_back(stamp + line);
}

// ---------------------------------------------------------------- orders

void Battle::order_section(int section, Order order, sf::Vector2f point, sf::Vector2f suppress_at)
{
    if(section < 0 || section >= static_cast<int>(m_sections.size())) return;
    Section& sec = m_sections[static_cast<std::size_t>(section)];
    sec.order = order;
    sec.point = point;
    sec.since = 0.0f;
    sec.suppress_at = suppress_at;
    sec.suppress_until = m_time + 14.0f;

    // The corporal first, then his men in order.
    std::vector<Soldier*> men;
    for(auto& s : m_soldiers)
        if(s.section == section && s.fighting() && s.leader && (!s.is_player || m_autoplay)) men.push_back(&s);
    for(auto& s : m_soldiers)
        if(s.section == section && s.fighting() && !s.leader && (!s.is_player || m_autoplay)) men.push_back(&s);
    float const n = static_cast<float>(men.size());
    // Which way is forward, for the shape of the group.
    sf::Vector2f forward = sec.team == Team::MONTERAN ? sf::Vector2f{1.0f, 0.0f} : sf::Vector2f{-1.0f, 0.0f};
    if(!men.empty() && length(point - men.front()->pos) > 30.0f) forward = (point - men.front()->pos) / length(point - men.front()->pos);
    sf::Vector2f const across{-forward.y, forward.x};
    for(std::size_t k = 0; k < men.size(); ++k)
    {
        Soldier& s = *men[k];
        s.order = order;
        s.path.clear();
        s.repath = 0.0f;
        s.charge_until = order == Order::CHARGE ? m_time + 9.0f : 0.0f;
        // A loose line abreast, every other man a step back: a group, not
        // a parade, and not the whole platoon in one long line.
        float const slot = (static_cast<float>(k) - (n - 1.0f) * 0.5f) * (order == Order::CHARGE ? 11.0f : 13.0f);
        float const depth = (k % 2 == 1) ? -9.0f : 0.0f;
        sf::Vector2f const loose = order == Order::CHARGE ? sf::Vector2f{} : sf::Vector2f{Random::real(-6.0f, 6.0f), Random::real(-4.0f, 4.0f)};
        sf::Vector2f const spot = point + across * slot + forward * depth + loose;
        // They move off one after another, following the man in front.
        if(order == Order::ADVANCE || order == Order::CHARGE) s.hold_until = m_time + 0.22f * static_cast<float>(k);
        switch(order)
        {
            case Order::ADVANCE:
                set_goal(s, spot, 38.0f);
                break;
            case Order::CHARGE:
                s.goal = spot;
                s.cover_index = -1;
                break;
            case Order::HOLD:
                s.goal = s.pos;
                break;
            case Order::SUPPRESS:
                s.suppress_at = suppress_at;
                s.suppress_until = m_time + 14.0f;
                if(length(point) > 1.0f) set_goal(s, spot, 38.0f);
                else s.goal = s.pos;
                break;
            case Order::FOLLOW:
                break;
        }
        // On the hill a man holds where he was last sent, and goes back
        // there after he has been for ammunition or a wounded mate.
        if(m_level == LevelId::HILL && s.team == Team::MONTERAN)
        {
            s.has_post = order != Order::FOLLOW && order != Order::CHARGE;
            s.post = s.goal;
        }
    }
}

void Battle::order_from_player(Order order)
{
    Soldier& p = *player();
    static const char* const words[] = {"On me!", "Hold here!", "Move up! Go!", "Covering fire, there!", "Fix bayonets! Charge!"};
    for(auto& sec : m_sections)
    {
        if(sec.team != Team::MONTERAN) continue;
        if(m_selected >= 0 && sec.id != m_selected) continue;
        // Each section takes its own share of the line around the cursor.
        float const band = m_selected >= 0 ? 0.0f : sec.band * 0.5f;
        order_section(sec.id, order, m_mouse_world + sf::Vector2f{0.0f, band}, m_mouse_world);
    }
    // The lieutenant's own runners, if he is not you.
    bark(p, words[static_cast<int>(order)], true);
    p.bark_cooldown = 0.0f;
    if(order == Order::ADVANCE || order == Order::CHARGE) SoundBank::get().play_at("whistle", p.pos, 0.7f);
    if(order == Order::CHARGE) SoundBank::get().play_at("charge_cry", p.pos, 0.9f);
}

void Battle::promote(int section)
{
    if(section < 0 || section_leader(section)) return;
    // The most senior man left takes over: a corporal if there is one.
    Soldier* best = nullptr;
    for(auto& s : m_soldiers)
        if(s.section == section && s.fighting() && (!best || (s.rank == "Cpl." && best->rank != "Cpl."))) best = &s;
    if(!best) return;
    best->leader = true;
    if(best->team == Team::MONTERAN)
    {
        std::string const who = best->rank + " " + best->name.substr(best->name.find(' ') + 1);
        notice(who + " takes " + m_sections[static_cast<std::size_t>(section)].name, 3.0f);
    }
}

void Battle::platoon_orders(float dt)
{
    for(auto& sec : m_sections) sec.since += dt;

    // Losing a corporal: someone steps up.
    for(auto& sec : m_sections) promote(sec.id);

    Soldier* lt = officer(Team::MONTERAN);
    if(!lt && m_autoplay && player()->alive()) lt = player();
    if(!lt || player_leads()) return;
    if(!fighting_started() || m_stage == Stage::RAID_WITHDRAW || m_level == LevelId::HILL) return;

    m_waypoint_time += dt;
    int limit = static_cast<int>(m_map.waypoints.size()) - 1;
    if(m_level == LevelId::TRENCHES)
    {
        limit = 5;
        if(m_stage == Stage::TRENCH) limit = 7;
        if(m_stage >= Stage::BUNKER) limit = 8;
    }
    else if(m_level == LevelId::FIELDS)
    {
        limit = 2;
        if(m_stage == Stage::STREAM) limit = 4;
        if(m_stage >= Stage::MILL) limit = 5;
    }
    else
    {
        limit = 4;
        if(m_stage == Stage::RAID_HAMLET) limit = 6;
        if(m_stage == Stage::RAID_DUGOUT) limit = 7;
    }
    if(m_stage == Stage::HOLD || m_stage == Stage::COUNTER) return;
    leapfrog(*lt, limit);
}

void Battle::leapfrog(Soldier& lt, int limit)
{
    /*
     * The sections leapfrog. One moves; the others put fire on whatever is
     * shooting at it, and they start that fire before it moves, not after.
     * When it has got where it was going, the next goes. A section that
     * is behind catches up; when they are all level, it is your section
     * that goes forward first, so nothing is taken without you.
     */
    std::vector<Section*> secs;
    for(auto& sec : m_sections)
        if(sec.team == Team::MONTERAN && section_leader(sec.id)) secs.push_back(&sec);
    if(secs.empty()) return;

    auto men_of = [this](int section)
    {
        std::vector<Soldier*> out;
        for(auto& s : m_soldiers)
            if(s.section == section && s.fighting() && (!s.is_player || m_autoplay)) out.push_back(&s);
        return out;
    };
    auto pinned_share = [&](int section)
    {
        auto const men = men_of(section);
        if(men.empty()) return 0.0f;
        int pinned = 0;
        for(const auto* m : men) if(m->suppression > 0.6f) ++pinned;
        return static_cast<float>(pinned) / static_cast<float>(men.size());
    };
    bool const rush = (m_level == LevelId::TRENCHES && m_stage == Stage::ASSAULT) || (m_level == LevelId::RAID && m_stage == Stage::RAID_TRENCH);
    auto point_of = [&](int at, const Section& sec)
    {
        bool const inside = (m_level == LevelId::TRENCHES && at == 8) || (m_level == LevelId::FIELDS && at == 5) || (m_level == LevelId::RAID && at == 7);
        return m_map.waypoints[static_cast<std::size_t>(std::clamp(at, 0, static_cast<int>(m_map.waypoints.size()) - 1))]
             + sf::Vector2f{0.0f, sec.band * (inside ? 0.2f : 1.0f)};
    };
    sf::Vector2f const forward = m_level == LevelId::RAID ? sf::Vector2f{120.0f, 0.0f} : sf::Vector2f{160.0f, 0.0f};

    // --- a bound under way: has it got there? --------------------------------------------
    for(auto* sec : secs)
    {
        if(!sec->bounding) continue;
        auto const men = men_of(sec->id);
        int there = 0;
        for(const auto* m : men) if(length(m->goal - m->pos) < 16.0f || (m->suppression > 0.75f && length(m->vel) < 3.0f)) ++there;
        float const took = m_time - sec->bound_start;
        if((static_cast<float>(there) >= 0.7f * static_cast<float>(men.size()) && took > 2.0f) || took > 18.0f || men.empty())
        {
            sec->bounding = false;
            sec->at = sec->heading;
            m_waypoint = std::max(m_waypoint, sec->at);
            m_waypoint_time = 0.0f;
            m_cover_until = m_time + (rush ? 0.6f : 2.0f);
            if(Soldier* cpl = section_leader(sec->id)) bark(*cpl, "In position!", false);
        }
        return;
    }

    // --- a bound waiting for its covering fire ---------------------------------------------
    if(m_bound_wait >= 0)
    {
        Section& sec = m_sections[static_cast<std::size_t>(m_bound_wait)];
        if(!section_leader(sec.id)) { m_bound_wait = -1; return; }
        if(pinned_share(sec.id) > 0.5f) { m_cover_until = std::max(m_cover_until, m_time + 1.0f); return; }
        if(m_time < m_cover_until) return;

        Soldier* cpl = section_leader(sec.id);
        if(m_assault)
        {
            float const d = cpl ? length(m_assault_at - cpl->pos) : 999.0f;
            order_section(sec.id, d < 90.0f ? Order::CHARGE : Order::ADVANCE, m_assault_at);
            bark(lt, d < 90.0f ? "Get in there! Go!" : "Work round them! Go!", true);
        }
        else
        {
            bool const charge = (m_level == LevelId::TRENCHES && sec.heading == 5) || (m_level == LevelId::FIELDS && sec.heading == 5)
                             || (m_level == LevelId::RAID && (sec.heading == 4 || sec.heading == 7));
            order_section(sec.id, charge ? Order::CHARGE : Order::ADVANCE, point_of(sec.heading, sec));
            if(charge)
            {
                bark(lt, "Fix bayonets! Charge!", true);
                SoundBank::get().play_at("whistle", lt.pos, 0.9f);
                SoundBank::get().play_at("charge_cry", lt.pos, 0.9f);
            }
            else if(cpl)
            {
                static const char* const calls[] = {"Up! Next cover, go!", "Move! Move!", "On me! Keep low!", "Go, go! Don't stop in the open!"};
                bark(*cpl, calls[m_bounding % 4], true);
            }
            // The lieutenant keeps with the rearmost.
            int rear = sec.heading;
            for(const auto* o : secs) rear = std::min(rear, o->at);
            if(lt.id != player()->id || m_autoplay) set_goal(lt, point_of(rear, sec) + sf::Vector2f{-10.0f, -sec.band}, 40.0f);
            // Across open ground under a gun, smoke first.
            if(lt.smokes > 0 && pinned_share(sec.id) > 0.2f)
                throw_grenade(lt, point_of(sec.heading, sec) + sf::Vector2f{30.0f, 0.0f}, true);
        }
        sec.bounding = true;
        sec.bound_start = m_time;
        m_bound_wait = -1;
        m_assault = false;
        ++m_bounding;
        return;
    }
    if(m_time < m_cover_until) return;

    // --- what next ------------------------------------------------------------------------------
    int front = 0, back = 1 << 20;
    for(const auto* sec : secs) { front = std::max(front, sec->at); back = std::min(back, sec->at); }
    Section* mine = nullptr;
    for(auto* sec : secs) if(sec->id == player_section()) mine = sec;

    Section* go = nullptr;
    int heading = front;
    if(back < front)
    {
        // Someone is behind: the rearmost catches up.
        for(auto* sec : secs) if(sec->at == back && (!go || sec == mine)) go = sec;
        heading = front;
    }
    else if(front < limit)
    {
        // All level: the next bound forward is yours.
        go = mine ? mine : secs[static_cast<std::size_t>(m_bounding) % secs.size()];
        heading = front + 1;
    }
    else
    {
        // At the last bound and still held up: go in at whoever is left.
        if(m_waypoint_time < 14.0f) return;
        sf::Vector2f const goal = m_map.waypoints[static_cast<std::size_t>(limit)];
        Soldier* holdout = nullptr;
        float best = 400.0f;
        for(auto& e : m_soldiers)
            if(e.team == Team::RECHIVII && e.fighting() && e.active && length(e.pos - goal) < best) { best = length(e.pos - goal); holdout = &e; }
        if(!holdout) return;
        m_waypoint_time = 0.0f;

        // If he's far enough from any of us, the guns can have him.
        float nearest = 1e9f;
        for(const auto& o : m_soldiers) if(o.team == Team::MONTERAN && o.alive()) nearest = std::min(nearest, length(o.pos - holdout->pos));
        if(nearest > 140.0f && m_missions.empty())
            for(auto& r : m_soldiers)
                if(r.team == Team::MONTERAN && r.alive() && r.role == Role::RADIOMAN && r.missions > 0 && (!r.is_player || m_autoplay))
                {
                    call_fire_mission(r, holdout->pos);
                    m_waypoint_time = -8.0f;
                    return;
                }
        go = mine ? mine : secs[static_cast<std::size_t>(m_bounding) % secs.size()];
        heading = go->at;
        m_assault = true;
        m_assault_at = holdout->pos;
    }
    if(!go) return;

    // Your section waits for you.
    if(go == mine && !m_autoplay && player()->alive())
    {
        sf::Vector2f centre;
        auto const men = men_of(mine->id);
        for(const auto* m : men) centre += m->pos;
        if(!men.empty()) centre /= static_cast<float>(men.size());
        if(!men.empty() && length(player()->pos - centre) > 110.0f)
        {
            m_keep_up += 0.25f;
            if(m_keep_up > 4.0f)
            {
                m_keep_up = 0.0f;
                if(Soldier* cpl = section_leader(mine->id)) { cpl->bark_cooldown = 0.0f; bark(*cpl, "Come on! Keep up with us!", true); }
            }
            m_cover_until = m_time + 0.25f;
            m_assault = false;
            return;
        }
        m_keep_up = 0.0f;
    }

    // Start the covering fire, then move once it's going.
    sf::Vector2f threat = lt.threat_age < 15.0f ? lt.threat : point_of(heading, *go) + forward;
    if(m_assault) threat = m_assault_at;
    for(auto* sec : secs)
        if(sec != go)
        {
            order_section(sec->id, Order::SUPPRESS, {}, threat);
            if(Soldier* cpl = section_leader(sec->id); cpl && Random::chance(0.5f)) bark(*cpl, "Covering fire! Keep their heads down!", true);
        }
    go->heading = heading;
    m_bound_wait = go->id;
    m_cover_until = m_time + (rush ? 1.2f : 2.5f);
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
            if(key->scancode == S::M) SoundBank::get().toggle_music_mute();
            return;
        }
        Soldier* p = player();
        bool const leading = player_leads();
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
            case S::T:
                if(p->alive() && p->smokes > 0) throw_grenade(*p, m_mouse_world, true);
                else if(p->alive()) notice("No smoke grenades", 1.5f);
                break;
            case S::X:
                if(p->alive() && p->flares > 0) fire_flare(*p, m_mouse_world);
                else if(p->alive() && p->role == Role::OFFICER) notice("No flares left", 1.5f);
                break;
            case S::H:
                // Digging in: lying down, a foxhole where you are; otherwise a row of bags in front.
                if(p->alive() && m_level == LevelId::HILL)
                {
                    if(p->digging > 0.0f) p->digging = 0.0f;
                    else start_dig(*p, p->stance == Stance::PRONE, m_mouse_world);
                }
                break;
            case S::V:
                // Each man's own trick: the radio, the satchel charge.
                if(!p->alive()) break;
                if(p->role == Role::RADIOMAN)
                {
                    // A mortar's flash, marked on the screen: the cursor on the mark calls the guns on the mortar itself.
                    sf::Vector2f at = m_mouse_world;
                    for(const auto& m : m_mortars)
                        if(!m.silenced && m.seen > 0.0f)
                        {
                            sf::Vector2f const mark = edge_marker(m.pos);
                            if(std::hypot(mark.x - m_mouse_screen.x, mark.y - m_mouse_screen.y) < 30.0f) { at = m.pos; break; }
                        }
                    call_fire_mission(*p, at);
                }
                else if(p->satchels > 0) place_satchel(*p, m_mouse_world);
                else if(p->role == Role::OFFICER && p->flares > 0) fire_flare(*p, m_mouse_world);
                else if(p->role == Role::SAPPER) notice("No satchel charges left", 1.5f);
                break;
            case S::B:
                if(p->alive() && p->role == Role::RADIOMAN)
                {
                    m_smoke_mission = !m_smoke_mission;
                    notice(m_smoke_mission ? "Next call: smoke" : "Next call: high explosive", 1.8f);
                    SoundBank::get().play("ui_hover", 0.6f);
                }
                break;
            case S::E:
                if(p->life == Life::DOWNED)
                {
                    SoundBank::get().play_at("medic_call", p->pos, 0.8f, 0.03f);
                    bark(*p, "Medic!", false);
                    for(auto& o : m_soldiers)                   // a medic hears it
                        if(o.team == p->team && o.role == Role::MEDIC && o.alive() && length(o.pos - p->pos) < 400.0f) o.think = 0.0f;
                    break;
                }
                if(p->alive()) interact(*p);
                break;
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
            case S::Tab:
                if(leading)
                {
                    int const sections = static_cast<int>(std::count_if(m_sections.begin(), m_sections.end(), [](const Section& s) { return s.team == Team::MONTERAN; }));
                    m_selected = m_selected + 1 >= sections ? -1 : m_selected + 1;
                    notice(m_selected < 0 ? "Orders to: the whole platoon" : "Orders to: " + m_sections[static_cast<std::size_t>(m_selected)].name, 1.8f);
                    SoundBank::get().play("ui_hover", 0.6f);
                }
                break;
            case S::Num1: case S::Num2: case S::Num3: case S::Num4: case S::Num5:
                if(leading)
                {
                    Order const o = key->scancode == S::Num1 ? Order::FOLLOW : key->scancode == S::Num2 ? Order::HOLD
                                  : key->scancode == S::Num3 ? Order::ADVANCE : key->scancode == S::Num4 ? Order::SUPPRESS : Order::CHARGE;
                    order_from_player(o);
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
        Soldier* p = player();
        if(press->button == sf::Mouse::Button::Left) { m_trigger = true; m_trigger_pressed = true; }
        if(press->button == sf::Mouse::Button::Middle && p->alive()) bayonet(*p);
        if(press->button == sf::Mouse::Button::Right)
        {
            m_aiming = true;
            if(p->has_mg() && p->alive())
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

void Battle::interact(Soldier& p)
{
    if(p.dragging >= 0)
    {
        m_soldiers[static_cast<std::size_t>(p.dragging)].dragged_by = -1;
        p.dragging = -1;
        SoundBank::get().set_loop("drag_loop", 0.0f);
        return;
    }

    if(p.cutting > 0.0f) { p.cutting = 0.0f; return; }

    // A wounded friend comes first.
    Soldier* near = nullptr;
    float best = 22.0f;
    for(auto& o : m_soldiers)
    {
        if(o.id == p.id || o.team != p.team) continue;
        bool const needs = o.life == Life::DOWNED || (o.alive() && o.bleeding > 0.0f);
        float const d = length(o.pos - p.pos);
        if(needs && d < best) { best = d; near = &o; }
    }
    if(near)
    {
        if(p.role == Role::MEDIC && p.medkits > 0)
        {
            p.treating = near->id;
            p.treat_time = near->life == Life::DOWNED ? 3.0f : 1.5f;
            p.vel = {};
            if(p.stance == Stance::STAND) p.stance = Stance::CROUCH;
            SoundBank::get().play_at("bandage", p.pos, 0.8f);
            hint(std::string("Treating ") + near->name + ". Keep still.", 3.0f);
        }
        else if(near->life == Life::DOWNED && near->dragged_by < 0)
        {
            p.dragging = near->id;
            near->dragged_by = p.id;
            p.stance = Stance::CROUCH;
            hint("Dragging. E again to let go. Get him to a medic, or to cover.", 4.0f);
        }
        return;
    }

    // Wire, if you have the cutters for it.
    sf::Vector2f wire_at;
    if(p.cuts_wire() && m_map.near_wire(p.pos, 12.0f, &wire_at))
    {
        p.cutting = p.role == Role::SAPPER ? 2.5f : 4.0f;
        p.cut_at = wire_at;
        p.vel = {};
        if(p.stance == Stance::STAND) p.stance = Stance::CROUCH;
        hint("Cutting. Keep still. E again to stop.", 3.0f);
        return;
    }

    // Then an ammunition crate.
    for(auto c : m_map.crates)
        if(length(c - p.pos) < 18.0f)
        {
            const WeaponSpec& w = p.weapon();
            p.spare = std::max(p.spare, w.spare);
            if(w.flame) p.fuel = 100.0f;
            p.grenades = std::max(p.grenades, 2);
            if(m_level == LevelId::HILL) p.bags = std::max(p.bags, 3);
            SoundBank::get().play_at("ammo_pickup", p.pos, 0.8f);
            notice("Filled your pouches", 2.0f);
            return;
        }

    // Then whatever is lying on the ground.
    Dropped* nearest = nullptr;
    best = 16.0f;
    for(auto& d : m_dropped)
        if(!d.taken && length(d.pos - p.pos) < best) { best = length(d.pos - p.pos); nearest = &d; }
    if(nearest && pick_up(p, *nearest)) return;
}

std::string Battle::interact_prompt() const
{
    // What E would do, said over your head, so you never have to guess.
    const Soldier& p = me();
    if(!p.alive()) return "";
    if(p.dragging >= 0) return "E  let go";
    if(p.cutting > 0.0f) return "cutting...";
    if(p.digging > 0.0f) return p.dig_hole ? "digging..." : "filling bags...";
    if(p.treating >= 0) return "treating...";
    const Soldier* near = nullptr;
    float best = 22.0f;
    for(const auto& o : m_soldiers)
    {
        if(o.id == p.id || o.team != p.team) continue;
        bool const needs = o.life == Life::DOWNED || (o.alive() && o.bleeding > 0.0f);
        float const d = std::hypot(o.pos.x - p.pos.x, o.pos.y - p.pos.y);
        if(needs && d < best) { best = d; near = &o; }
    }
    std::string const who = near ? near->name.substr(near->name.find(' ') + 1) : "";
    if(near && p.role == Role::MEDIC && p.medkits > 0) return "E  treat " + who;
    if(near && near->life == Life::DOWNED && near->dragged_by < 0) return "E  drag " + who;
    if(p.cuts_wire() && m_map.near_wire(p.pos, 12.0f)) return "E  cut the wire";
    for(auto c : m_map.crates) if(std::hypot(c.x - p.pos.x, c.y - p.pos.y) < 18.0f) return "E  fill up";
    for(const auto& d : m_dropped)
        if(!d.taken && std::hypot(d.pos.x - p.pos.x, d.pos.y - p.pos.y) < 16.0f)
            return d.gun == p.gun ? "E  take his rounds" : std::string("E  take the ") + gun_name(d.gun);
    return "";
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
    if(length(to_mouse) > 2.0f && p.lunge <= 0.0f)
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

    // Shift gets you up and running, from wherever you were.
    bool const wants_run = m_sprint && length(in) > 0.0f && p.can_walk() && p.dragging < 0 && !p.deployed && p.deploying <= 0.0f;
    if(wants_run && p.stance != Stance::STAND && p.stance_change <= 0.0f)
    {
        p.stance_change = p.stance == Stance::PRONE ? 0.45f : 0.2f;
        p.stance = Stance::STAND;
    }

    float speed = 46.0f;
    if(p.stance == Stance::CROUCH) speed = 28.0f;
    if(p.stance == Stance::PRONE)  speed = 11.0f;
    bool const sprinting = wants_run && p.stance == Stance::STAND && !m_aiming;
    if(sprinting) speed = 74.0f;
    if(m_aiming && !p.has_mg()) speed *= 0.55f;
    if(p.dragging >= 0) speed = 20.0f;
    if(p.hp < 50.0f) speed *= 0.75f;
    if(!p.can_walk()) { speed = 8.0f; p.stance = Stance::PRONE; }
    if(m_map.in_wire(p.pos)) speed *= 0.35f;
    if(m_map.in_water(p.pos)) speed *= 0.5f;
    if(p.deployed || p.deploying > 0.0f || p.treating >= 0 || m_bandage_time > 0.0f) speed = 0.0f;
    // On the radio or at the wire, moving gives it up.
    bool calling = false;
    for(const auto& m : m_missions) if(m.caller == p.id && m.step == 0) calling = true;
    if((calling || p.cutting > 0.0f || p.digging > 0.0f) && length(in) > 0.0f) { p.cutting = 0.0f; p.digging = 0.0f; }
    if(calling || p.cutting > 0.0f || p.digging > 0.0f) speed = 0.0f;
    // Up the hill is slower than down it.
    if(m_level == LevelId::HILL && length(in) > 0.0f)
    {
        float const climb = m_map.height(p.pos + in / length(in) * 6.0f) - m_map.height(p.pos);
        if(climb > 0.0f) speed *= 1.0f - std::min(0.22f, climb * 18.0f);
    }
    // Creeping up on them in the dark, you go slow.
    if(m_level == LevelId::RAID && !m_alarm && !sprinting) speed *= 0.85f;
    if(p.stance_change > 0.0f) speed *= 0.3f;

    sf::Vector2f const want = length(in) > 0.0f ? in / length(in) * speed : sf::Vector2f{};
    if(p.lunge <= 0.0f) p.vel += (want - p.vel) * std::min(1.0f, dt * 12.0f);
    if(length(want) > 0.0f && p.stance == Stance::PRONE)
    {
        float const bd = std::remainder(std::atan2(want.y, want.x) - p.body_angle, 6.2831853f);
        p.body_angle += bd * std::min(1.0f, dt * 3.0f);
    }

    // --- shoot -----------------------------------------------------------------
    const WeaponSpec& w = p.weapon();
    bool const can = !sprinting && p.reloading <= 0.0f && p.treating < 0 && p.dragging < 0 && p.lunge <= 0.0f
                  && (!p.has_mg() || p.deployed) && (p.can_shoot() || p.has_flamer());
    // The weapon comes up when you aim or fire, and goes down when you run.
    bool const want_up = can && (m_aiming || m_trigger || p.flash > 0.0f || p.cooldown > 0.0f);
    p.ready += ((want_up ? 1.0f : (sprinting ? 0.0f : 0.15f)) - p.ready) * std::min(1.0f, dt * (want_up ? 22.0f : 5.0f));
    // A click while it's still coming up fires as soon as it's there.
    if(can && p.ready > 0.6f && (w.automatic ? m_trigger : m_trigger_pressed)) { p.firing = true; m_trigger_pressed = false; }
    if(m_trigger_pressed && p.has_mg() && !p.deployed && p.deploying <= 0.0f)
        hint("Right click to set the gun up first. It fires lying or kneeling, not from the hip.", 3.0f);
    if(!can) m_trigger_pressed = false;

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
    else m_mouse_world = player()->pos + dir(player()->aim) * 60.0f;

    for(auto& s : m_soldiers)
        if((!s.is_player || m_autoplay) && s.active && s.life == Life::OK) think(s, dt);
    for(auto& s : m_soldiers)
        if((!s.is_player || m_autoplay) && s.active && s.life == Life::OK) act(s, dt);
    for(auto& s : m_soldiers) update_soldier_body(s, dt);
    for(auto& s : m_soldiers) if(s.life == Life::OK) update_bayonet(s, dt);
    for(auto& s : m_soldiers) if(s.life == Life::OK) fire(s, dt);

    update_bullets(dt);
    update_world(dt);
    update_missions(dt);
    update_mortars(dt);
    update_effects(dt);
    update_script(dt);
    platoon_orders(dt);

    // Who's in your section, for the armbands.
    int const mine = player()->section;
    for(auto& s : m_soldiers) s.yours = s.team == Team::MONTERAN && mine >= 0 && s.section == mine && !s.is_player;

    // The camera leans toward where you are looking. Holding right click
    // leans it much further, so you can see what you are aiming at.
    Soldier& p = *player();
    sf::Vector2f look = m_mouse_world - p.pos;
    bool const far = (m_aiming && p.alive()) || p.deployed;
    float const reach = far ? (p.gun == Gun::SNIPER && m_aiming ? 300.0f : 165.0f) : 45.0f;
    float const share = far ? 0.75f : 0.4f;
    if(length(look) > 1.0f) look = look / length(look) * std::min(length(look) * share, reach);
    if(m_autoplay) look = {};
    m_look += (look - m_look) * std::min(1.0f, dt * 6.0f);
    m_kick *= std::exp(-dt * 10.0f);
    m_camera.follow(p.pos + m_look + m_kick, delta_time, 5.0f);
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
    m_notice = std::max(0.0f, m_notice - dt);
    if(!m_lines.empty())
    {
        m_lines.front().time -= dt;
        if(m_lines.front().time <= 0.0f) m_lines.pop_front();
    }

    // Music follows the fighting: recent shots near you and the stage.
    m_combat_heat = std::max(0.0f, m_combat_heat - dt * 0.1f);
    float target = std::clamp(m_combat_heat * 1.3f, 0.0f, 1.0f);
    // On the hill, once the first wave has come, the dread doesn't let go: not even in the lulls.
    bool const battle = m_level == LevelId::HILL ? (m_wave > 0 && m_stage < Stage::WON) : fighting_started();
    if(m_stage == Stage::WON || m_stage == Stage::LOST) target = 0.0f;
    else if(battle) target = std::max(target, 0.5f);
    SoundBank& sound = SoundBank::get();
    sound.set_intensity(target);
    if(m_level == LevelId::TRENCHES) sound.set_front(battle ? 0.3f : 0.55f);

    // The weather.
    if(!GameTime::is_paused())
    {
        if(m_level == LevelId::TRENCHES) sound.set_loop("rain_loop", 0.45f);
        else if(m_level == LevelId::FIELDS) sound.set_loop("dawn_loop", 0.35f * (1.0f - std::min(1.0f, m_combat_heat * 2.0f)) + 0.1f);
        else if(m_level == LevelId::HILL)
        {
            sound.set_loop("dawn_loop", (0.3f * (1.0f - std::min(1.0f, m_combat_heat * 2.0f)) + 0.08f) * (1.0f - m_night));
            sound.set_loop("night_loop", 0.4f * m_night);
        }
        else sound.set_loop("night_loop", 0.4f);
        bool on_set = false;
        for(const auto& m : m_missions) if(m.caller == m_player && m.step == 0) on_set = true;
        sound.set_loop("radio_loop", on_set ? 0.5f : 0.0f);
    }

    if(GameTime::is_paused()) sound.stop_loops();
    sound.update(dt);

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
    m_waypoint_time = std::max(m_waypoint_time, 5.0f);     // a new stage, a fresh look at what to do
    m_stage = stage;
    m_stage_time = 0.0f;
    Soldier* lt = officer(Team::MONTERAN);
    Soldier& p = *player();
    bool const you_lead = p.role == Role::OFFICER && p.alive();
    bool const ai_lt = lt && !lt->is_player;
    auto all_rechivii = [this](auto&& f) { for(auto& s : m_soldiers) if(s.team == Team::RECHIVII) f(s); };
    std::string const lead_hint = "Tab picks a section.  1 follow  2 hold  3 advance  4 covering fire  5 charge, all at the cursor";

    switch(stage)
    {
        // ======================================================== the Ochre line
        case Stage::REAR:
            m_objective = "Go to the front trench";
            break;
        case Stage::RANGE:
            m_objective = "Break the bottles on the range";
            hint(range_hint(p.gun), 9.0f);
            break;
        case Stage::FRONT:
            m_objective = "Wait in the front trench";
            if(ai_lt) say(*lt, "There you are. Keep your head below the parapet.");
            hint("C kneels, Z goes prone. In a trench you're safe from almost everything but a shell.", 8.0f);
            for(auto& s : m_soldiers)
                if(s.team == Team::MONTERAN && !s.is_player && s.alive() && s.pos.x < 800)
                {
                    s.sitting = false;
                    s.order = Order::ADVANCE;
                    set_goal(s, m_map.waypoints[0] + sf::Vector2f{0.0f, (static_cast<float>(s.id % 8) - 3.5f) * 30.0f});
                }
            break;
        case Stage::SCOUT:
        {
            all_rechivii([](Soldier& s) { if(!s.has_post && !s.reserve) s.active = true; });
            Soldier* gunner = nullptr;
            for(auto& s : m_soldiers) if(s.team == Team::MONTERAN && s.role == Role::GUNNER && s.alive() && !s.is_player) gunner = &s;
            if(gunner) say(*gunner, "Movement! Out by the wire!");
            m_objective = "Kill the scout";
            m_combat_heat = 0.6f;
            break;
        }
        case Stage::BARRAGE:
            m_objective = "Wait for the whistle";
            if(ai_lt) say(*lt, "Our guns. When they lift, we go. Fix bayonets.", 4.5f);
            else say("Runner", "Guns are going in, Lieutenant. Go on the lift.", Team::MONTERAN, 4.0f);
            for(int i = 0; i < 12; ++i)
            {
                sf::Vector2f const t{Random::real(1690, 1760), Random::real(130, 770)};
                m_shells.push_back({t, 1.5f + static_cast<float>(i) * 0.75f + Random::real(0.0f, 0.3f), false, 1});
            }
            break;
        case Stage::ASSAULT:
            m_objective = you_lead ? "Lead them across" : "Cross no man's land. Take their trench";
            SoundBank::get().play_at("whistle", lt ? lt->pos : p.pos, 1.0f);
            SoundBank::get().play("charge_cry", 0.6f);
            if(ai_lt) say(*lt, "Over! Over the top! Go, go, go!", 3.0f);
            if(you_lead) hint(lead_hint, 14.0f);
            else hint("Move from cover to cover. Shift runs. Middle click lunges with the bayonet.", 8.0f);
            all_rechivii([](Soldier& s) { if(s.has_post) { s.active = true; s.suppression = 1.0f; } });
            m_waypoint = 0;
            m_waypoint_time = 100.0f;
            for(auto& sec : m_sections) sec.at = 0;
            m_combat_heat = 1.0f;
            break;
        case Stage::TRENCH:
            m_objective = "Take the bunker";
            if(ai_lt) say(*lt, "Their trench is ours! The bunker next, keep moving!");
            else say(p, "Trench is ours! Bunker, next!");
            m_barrage_timer = 3.0f;
            all_rechivii([](Soldier& s)
            {
                if(s.fighting() && s.has_post && s.post.x < 1800)
                {
                    s.post = {Random::real(2120, 2380), Random::real(320, 440)};
                    s.path.clear();
                }
            });
            break;
        case Stage::BUNKER:
            m_objective = "Take the farmhouse";
            if(ai_lt) say(*lt, "Bunker's done. The farmhouse! Get inside!");
            else say(p, "Bunker's clear. To the farm!");
            break;
        case Stage::HOLD:
            m_objective = "Hold the farmhouse";
            if(ai_lt) say(*lt, "Into the windows. They'll want it back. Make them pay for it.", 4.5f);
            else say(p, "Get to the walls. They'll come back.");
            break;

        // ======================================================== Halvek fields
        case Stage::MARCH:
        {
            m_objective = "March up the lane";
            say("Cpl. Orban", "Keep closed up. Mist like this, you lose a man and never find him.", Team::MONTERAN, 5.0f);
            hint("WASD to walk, the mouse to look. Stay with the platoon.", 8.0f);
            // The whole column walks up to the end of the lane.
            for(auto& s : m_soldiers)
                if(s.team == Team::MONTERAN && !s.is_player)
                {
                    s.order = Order::ADVANCE;
                    s.goal = s.pos + sf::Vector2f{std::min(330.0f, 960.0f - s.pos.x), 0.0f};
                    s.goal.y = 520.0f + static_cast<float>(s.id % 3 - 1) * 8.0f;
                    if(s.goal.x > 900.0f) s.goal.y += (s.goal.x - 900.0f) * 0.25f;
                }
            break;
        }
        case Stage::CONTACT:
            m_objective = "Get off the lane. Find the shooters";
            all_rechivii([](Soldier& s) { if(s.section == FIELDS_PATROL) s.active = true; });
            if(ai_lt) say(*lt, "Down! Get down! In the wood, left!", 3.0f);
            hint(you_lead ? lead_hint : "Get into cover. The hedges hide you, but they won't stop much.", 9.0f);
            m_waypoint = 0;
            m_waypoint_time = 3.0f;
            for(auto& sec : m_sections) sec.at = 0;
            m_combat_heat = 0.8f;
            // Their main body is on the road too. Neither side knew.
            all_rechivii([](Soldier& s)
            {
                if(s.reserve && s.has_post) { s.active = true; s.path.clear(); }
            });
            break;
        case Stage::WOOD:
            m_objective = "Clear the wood";
            if(ai_lt) say(*lt, "Into the trees! Clear them out!");
            break;
        case Stage::STREAM:
            m_objective = "Push on to the stream";
            if(ai_lt) say(*lt, "The mill, by the bridge. That's where they'll be. Hedge to hedge, go!", 4.5f);
            all_rechivii([](Soldier& s) { if(s.section == FIELDS_MILL) s.active = true; });
            break;
        case Stage::MILL:
            m_objective = "Take the mill";
            if(ai_lt) say(*lt, "Over the bridge and into the mill! Bombs through the door first!", 4.5f);
            break;

        // ======================================================== the Dravo raid
        case Stage::RAID_CREEP:
            m_objective = "Get through their wire. Quietly";
            say("Lt. Varga", "Nobody fires till they know we're here. Sappers to the wire. The rest of you, low and slow.", Team::MONTERAN, 6.0f);
            hint(p.cuts_wire() ? "Crawl up to the wire and press E to cut it. Z to lie down, C to kneel." : "Stay low: Z lies down, C kneels. Keep to the dark between the shell holes.", 9.0f);
            for(auto& sec : m_sections)
                if(sec.team == Team::MONTERAN)
                {
                    // Up to the wire together, in the dark, before any of it starts.
                    order_section(sec.id, Order::ADVANCE, m_map.waypoints[2] + sf::Vector2f{0.0f, sec.band});
                    sec.at = 2;
                }
            break;
        case Stage::RAID_TRENCH:
            m_objective = "Clear their trench";
            for(auto& sec : m_sections)
                if(sec.team == Team::MONTERAN)
                {
                    sec.at = std::max(sec.at, m_creep >= 2 ? 3 : 2);
                    // A group with its lane through the wire doesn't stop to
                    // think: it goes straight in while they're still waking.
                    if(sec.belts_cut >= 2 && section_leader(sec.id))
                    {
                        order_section(sec.id, Order::CHARGE, m_map.waypoints[4] + sf::Vector2f{0.0f, sec.band});
                        sec.at = 4;
                        sec.bounding = true;
                        sec.heading = 4;
                        sec.bound_start = m_time;
                    }
                }
            if(ai_lt) say(*lt, "In! Into the trench! Bombs round every traverse!", 3.5f);
            break;
        case Stage::RAID_HAMLET:
            m_objective = "Clear the cottages and the barn";
            if(ai_lt) say(*lt, "The farm! House by house. Grenade first, then in.", 4.0f);
            break;
        case Stage::RAID_DUGOUT:
            m_objective = "Destroy the command dugout";
            if(ai_lt) say(*lt, "Their headquarters, behind the barn. Satchel it if you can. Go!", 4.5f);
            break;
        case Stage::RAID_WITHDRAW:
        {
            m_objective = "Get back to our ditch before it's light";
            if(ai_lt) say(*lt, "That's it! Out! Back the way we came, now!", 4.0f);
            else say(p, "Out! Everyone back!", 3.0f);
            SoundBank::get().play_at("whistle", lt ? lt->pos : p.pos, 1.0f);
            all_rechivii([](Soldier& s) { if(s.reserve) s.active = true; });
            for(auto& sec : m_sections)
                if(sec.team == Team::MONTERAN)
                    order_section(sec.id, Order::ADVANCE, m_map.waypoints[0] + sf::Vector2f{0.0f, sec.band * 0.6f});
            if(lt && !lt->is_player) set_goal(*lt, m_map.waypoints[0], 40.0f);
            m_barrage_timer = 4.0f;
            log("The dugout was taken. The raiders fell back");
            break;
        }

        // ======================================================== Hill 312
        case Stage::HILL_LULL:
        {
            if(m_wave == 0)
            {
                m_objective = "Hold Hill 312";
                if(ai_lt) say(*lt, "This is where we stop them. Dig in, fill your bags, and fill your pouches at the dump.", 6.0f);
                hint("H digs in: a row of sandbags in front of you, or lying down (Z), a foxhole. E at the dump fills bags and pouches.", 10.0f);
                break;
            }
            m_objective = "Hold the hill. Next wave soon";
            // The ones who gave themselves up are walked to the rear.
            for(auto& s : m_soldiers)
                if(s.team == Team::RECHIVII && s.surrendered && s.life == Life::OK) { s.life = Life::FLED; s.fate = "Taken prisoner"; }
            // The battery has a little more for us, and the dump has dressings.
            for(auto& s : m_soldiers)
            {
                if(s.team != Team::MONTERAN || !s.fighting()) continue;
                if(s.role == Role::RADIOMAN) s.missions = std::max(s.missions, 2);
                if(s.role == Role::MEDIC) s.medkits = std::max(s.medkits, 8);
                s.bandages = std::max(s.bandages, 1);
            }
            if(ai_lt)
            {
                static const char* const lines[] = {"Count off! Who's hit? Medic, see to them.", "Ammunition from the dump, now, while it's quiet.",
                                                    "Get those bags filled. They'll be back.", "Good. Good. Same again, lads."};
                say(*lt, lines[m_waves_held % 4], 4.0f);
                hill_positions(m_last_threat, m_wave_dirs.size() > 1 ? m_wave_dirs[1] : m_last_threat);
            }
            // Every third lull, a few new men up the back slope.
            if(m_waves_held % 3 == 0)
            {
                int const count = 3;
                for(int k = 0; k < count && m_new_men < 15; ++k)
                {
                    Pending pd;
                    pd.at = 3.0f + 1.2f * static_cast<float>(k);
                    pd.role = (m_new_men % 4 == 3) ? Role::STORMTROOPER : Role::RIFLEMAN;
                    pd.kind = 5;
                    pd.pos = {16.0f, 640.0f + static_cast<float>(k) * 10.0f};
                    pd.post = m_map.crest + sf::Vector2f{-60.0f + static_cast<float>(k) * 14.0f, 40.0f};
                    pd.home = pd.pos;
                    int fewest = -1, fewest_n = 99;
                    for(const auto& sec : m_sections)
                    {
                        if(sec.team != Team::MONTERAN) continue;
                        int men = 0;
                        for(const auto& o : m_soldiers) if(o.section == sec.id && o.fighting()) ++men;
                        if(men < fewest_n) { fewest_n = men; fewest = sec.id; }
                    }
                    pd.section = fewest;
                    static const char* const names[] = {
                        "Pvt. Anton Bors", "Pvt. Silviu Rand", "Pvt. Marek Haas", "Pvt. Tudor Ivo", "Pvt. Cyril Moss",
                        "Pvt. Eugen Falk", "Pvt. Horia Dunn", "Pvt. Laszlo Penn", "Pvt. Ovidiu Crane", "Pvt. Remus Tal",
                        "Pvt. Stefan Ward", "Pvt. Vasile Orr", "Pvt. Bogdan Leif", "Pvt. Costin Hale", "Pvt. Dan Rowe"};
                    pd.name = names[m_new_men % 15];
                    ++m_new_men;
                    m_pending.push_back(pd);
                }
                if(count > 0) notice("Replacements coming up from the rear", 3.0f);
            }
            break;
        }
        case Stage::HILL_WAVE:
            m_objective = "Hold the hill. Wave " + std::to_string(m_wave);
            m_combat_heat = 0.6f;
            m_illum_timer = 2.0f;
            break;

        // ======================================================== both
        case Stage::COUNTER:
        {
            m_objective = m_level == LevelId::TRENCHES ? "Hold the farmhouse" : "Hold the mill";
            all_rechivii([](Soldier& s) { if(s.reserve && !s.has_post) s.active = true; });
            // Everyone into cover around the objective, facing the way they'll come.
            if(!player_leads())
                for(auto& sec : m_sections)
                    if(sec.team == Team::MONTERAN)
                        order_section(sec.id, Order::ADVANCE, objective_point() + sf::Vector2f{-10.0f, sec.band * 0.35f});
            Soldier* any = nullptr;
            for(auto& s : m_soldiers) if(s.team == Team::MONTERAN && s.alive() && !s.is_player) any = &s;
            if(any) say(*any, m_level == LevelId::TRENCHES ? "Here they come! East, in the field!" : "Movement on the road! They're coming back!");
            m_combat_heat = 1.0f;
            break;
        }
        case Stage::WON:
            m_objective = "";
            Campaign::current().won = true;
            if(lt) say(*lt, m_level == LevelId::TRENCHES ? "It's ours. God help us, it's ours."
                          : (m_level == LevelId::FIELDS ? "The mill's ours. Count the men." : "We're in. Count heads. Who's missing?"), 5.0f);
            else say(p, "It's ours.", 5.0f);
            SoundBank::get().play_at("whistle", p.pos, 0.6f);
            log(m_level == LevelId::RAID ? "The raiders were back in their own lines" : "The objective was taken");
            m_end_timer = 0.0f;
            break;
        case Stage::LOST:
            m_objective = "";
            if(m_level == LevelId::HILL)
            {
                say("", "Hill 312 has fallen.", Team::MONTERAN, 6.0f);
                log("Hill 312 fell, after " + std::to_string(m_waves_held) + (m_waves_held == 1 ? " wave" : " waves") + " beaten off");
                Campaign::current().waves = m_waves_held;
            }
            else
            {
                say("", m_level == LevelId::RAID ? "The raid has failed." : "The attack has failed.", Team::MONTERAN, 6.0f);
                log(m_level == LevelId::RAID ? "None of the raiders came back" : "The platoon was destroyed");
            }
            m_end_timer = 0.0f;
            break;
    }
}

void Battle::update_script(float dt)
{
    Soldier& p = *player();

    if(m_level == LevelId::TRENCHES)
    {
        // Far-off guns, the whole time.
        m_far_guns -= dt;
        if(m_far_guns <= 0.0f)
        {
            m_far_guns = Random::real(4.0f, 11.0f);
            SoundBank::get().play("artillery_far", Random::real(0.25f, 0.5f), 0.1f);
            if(Random::chance(0.5f)) m_camera.shake(0.08f);
        }
        script_trenches(dt);
    }
    else if(m_level == LevelId::FIELDS)
    {
        // The mist burns off as the morning goes on.
        m_mist = std::max(0.0f, m_mist - dt / 260.0f);
        script_fields(dt);
    }
    else if(m_level == LevelId::RAID) script_raid(dt);
    else script_hill(dt);

    // Victory and defeat, once there is fighting. (The raid is won by
    // getting home, not by killing everyone, and the hill isn't won at all.)
    bool const fighting = fighting_started() || (m_level == LevelId::FIELDS && m_stage >= Stage::CONTACT && m_stage < Stage::WON);
    if(fighting && m_stage < Stage::WON && m_level != LevelId::RAID && m_level != LevelId::HILL)
    {
        bool reserve_waiting = false;
        for(const auto& s : m_soldiers) if(s.reserve && !s.active && s.fighting()) reserve_waiting = true;
        if(count(Team::RECHIVII) == 0 && !reserve_waiting) set_stage(Stage::WON);
    }
    if(m_stage < Stage::WON)
    {
        bool anyone = false;
        for(const auto& s : m_soldiers) if(s.team == Team::MONTERAN && (s.alive() || s.life == Life::DOWNED)) anyone = true;
        if(!anyone) set_stage(Stage::LOST);
    }

    // Their nerve. Men who have had enough run; men caught with the enemy on
    // top of them and nowhere to go put their hands up.
    if(fighting)
    {
        m_rout_check -= dt;
        if(m_rout_check <= 0.0f)
        {
            m_rout_check = 3.0f;
            int lost = 0, total = 0;
            for(const auto& s : m_soldiers) if(s.team == Team::RECHIVII) { ++total; if(!s.fighting()) ++lost; }
            bool const no_officer = officer(Team::RECHIVII) == nullptr;
            for(auto& s : m_soldiers)
            {
                if(s.team != Team::RECHIVII || !s.fighting() || !s.active) continue;
                float nearest = 1e9f;
                for(const auto& o : m_soldiers)
                    if(o.team == Team::MONTERAN && o.fighting()) nearest = std::min(nearest, length(o.pos - s.pos));

                if(nearest < 45.0f && (s.morale < 0.45f || s.suppression > 0.8f) && Random::chance(0.3f))
                {
                    surrender(s);
                    continue;
                }
                if(m_level != LevelId::HILL && no_officer && lost * 2 >= total && Random::chance(0.18f + 0.4f * s.suppression))
                {
                    s.morale = 0.0f;
                    bark(s, "Hesh! Hesh!", true);
                    continue;
                }
                if(m_level == LevelId::TRENCHES && s.has_post && s.post.x < 1800 && nearest < 70.0f && s.suppression > 0.5f && Random::chance(0.3f))
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
    if(!m_officer_dead_noted && fighting && !officer(Team::MONTERAN) && p.role != Role::OFFICER)
    {
        m_officer_dead_noted = true;
        say("", "Lt. Varga is down. The platoon looks to you now.", Team::MONTERAN, 4.0f);
        log("Lt. Varga fell. The platoon looked to " + p.name);
        for(auto& sec : m_sections)
            if(sec.team == Team::MONTERAN) order_section(sec.id, Order::HOLD, {});
        hint("Tab picks a section.  1 follow  2 hold  3 advance  4 covering fire  5 charge, all at the cursor", 12.0f);
    }

    // Your man has died: someone else picks up the fight.
    if(p.life == Life::DEAD || p.life == Life::FLED)
    {
        m_dead_wait += dt;
        if(m_dead_wait > 2.0f) { m_dead_wait = 0.0f; take_over(); }
    }
}

void Battle::script_trenches(float dt)
{
    Soldier& p = *player();
    auto enemies_in = [this](const sf::FloatRect& r)
    {
        int n = 0;
        for(const auto& s : m_soldiers) if(s.team == Team::RECHIVII && s.fighting() && r.contains(s.pos)) ++n;
        return n;
    };
    auto friends_in = [this](const sf::FloatRect& r)
    {
        int n = 0;
        for(const auto& s : m_soldiers) if(s.team == Team::MONTERAN && s.alive() && r.contains(s.pos)) ++n;
        return n;
    };
    // Rechivii still fighting who aren't the reserve waiting off the map.
    int holding_out = 0;
    for(const auto& s : m_soldiers) if(s.team == Team::RECHIVII && s.fighting() && s.active && !s.reserve) ++holding_out;
    auto stand_up = [this](float after)
    {
        for(auto& s : m_soldiers)
            if(s.team == Team::MONTERAN && (!s.is_player || m_autoplay) && s.order == Order::HOLD && s.pos.x < 800.0f
               && m_stage_time > after + static_cast<float>(s.id % 8) * 2.5f)
            {
                s.sitting = false;
                s.order = Order::ADVANCE;
                set_goal(s, m_map.waypoints[0] + sf::Vector2f{0.0f, (static_cast<float>(s.id % 8) - 3.5f) * 30.0f});
            }
    };

    switch(m_stage)
    {
        case Stage::REAR:
            if(m_stage_time > 1.5f && m_stage_time - dt <= 1.5f)
            {
                say("Cpl. Orban", p.name.substr(p.name.find(' ') + 1) + "! Up to the front trench, the Lieutenant wants everyone.", Team::MONTERAN, 4.5f);
                say("Cpl. Orban", "Check your weapon on the range on the way. Bottles on the crates.", Team::MONTERAN, 4.5f);
                hint("WASD to walk. Aim with the mouse. Follow the trench east.", 9.0f);
            }
            stand_up(4.0f);
            if(m_map.range_area.contains(p.pos)) set_stage(Stage::RANGE);
            if(m_map.front_trench_area.contains(p.pos)) set_stage(Stage::FRONT);
            break;
        case Stage::RANGE:
            if(m_bottles_left == 0 && m_objective != "Go to the front trench")
            {
                m_objective = "Go to the front trench";
                say("Cpl. Orban", "Good enough. Now get up there.", Team::MONTERAN, 3.0f);
            }
            stand_up(0.0f);
            if(m_map.front_trench_area.contains(p.pos)) set_stage(Stage::FRONT);
            break;
        case Stage::FRONT:
            if(m_stage_time > 9.0f) set_stage(Stage::SCOUT);
            break;
        case Stage::SCOUT:
        {
            bool scout_gone = true;
            for(const auto& s : m_soldiers)
                if(s.team == Team::RECHIVII && !s.has_post && !s.reserve && s.fighting()) scout_gone = false;
            if(scout_gone || m_stage_time > 30.0f) set_stage(Stage::BARRAGE);
            break;
        }
        case Stage::BARRAGE:
            if(m_stage_time > 11.5f) set_stage(Stage::ASSAULT);
            break;
        case Stage::ASSAULT:
            // The barrage creeps on ahead of us for the first part of the
            // crossing, keeping their heads down.
            m_barrage_timer -= dt;
            if(m_stage_time < 26.0f && m_barrage_timer <= 0.0f)
            {
                m_barrage_timer = Random::real(0.7f, 1.1f);
                m_shells.push_back({{Random::real(1700, 1790), Random::real(130, 770)}, 1.9f, false, 1});
            }
            if(enemies_in(m_map.enemy_trench_area) == 0 && friends_in(m_map.enemy_trench_area) + friends_in(sf::FloatRect({1640, 100}, {200, 700})) > 0
               && you_there(m_map.enemy_trench_area, 160.0f))
                set_stage(Stage::TRENCH);
            break;
        case Stage::TRENCH:
        {
            // Their guns answer: shells walk across no man's land and the trench.
            m_barrage_timer -= dt;
            if(m_barrage_timer <= 0.0f && m_stage_time < 28.0f)
            {
                m_barrage_timer = Random::real(3.5f, 5.5f);
                std::vector<sf::Vector2f> near;
                for(const auto& s : m_soldiers) if(s.team == Team::MONTERAN && s.alive()) near.push_back(s.pos);
                sf::Vector2f t = near.empty() ? sf::Vector2f{1600, 450} : near[static_cast<std::size_t>(Random::integer(0, static_cast<int>(near.size()) - 1))];
                t += dir(Random::real(0.0f, 6.283f)) * Random::real(35.0f, 110.0f);
                m_shells.push_back({t, 1.8f, false, 0});
            }
            if(enemies_in(m_map.bunker_area) == 0 && (friends_in(sf::FloatRect({1950, 260}, {170, 320})) > 0 || holding_out == 0)
               && you_there(m_map.bunker_area, 200.0f))
                set_stage(Stage::BUNKER);
            break;
        }
        case Stage::BUNKER:
            if(friends_in(m_map.farm_area) >= 1 && you_there(m_map.farm_area, 40.0f))
                set_stage(Stage::HOLD);
            break;
        case Stage::HOLD:
            if(m_stage_time > 14.0f) set_stage(Stage::COUNTER);
            break;
        default:
            break;
    }

    // If every enemy in the way is gone, skip ahead.
    if(holding_out == 0 && m_stage_time > 3.0f)
    {
        if(m_stage == Stage::ASSAULT && you_there(m_map.enemy_trench_area, 200.0f)) set_stage(Stage::TRENCH);
        else if(m_stage == Stage::TRENCH && you_there(m_map.bunker_area, 260.0f)) set_stage(Stage::BUNKER);
        else if(m_stage == Stage::BUNKER && you_there(m_map.farm_area, 80.0f)) set_stage(Stage::HOLD);
    }
}

void Battle::script_fields(float dt)
{
    (void)dt;
    Soldier& p = *player();
    auto alive_in = [this](int section)
    {
        int n = 0;
        for(const auto& s : m_soldiers) if(s.team == Team::RECHIVII && s.section == section && s.fighting()) ++n;
        return n;
    };
    auto friends_in = [this](const sf::FloatRect& r)
    {
        int n = 0;
        for(const auto& s : m_soldiers) if(s.team == Team::MONTERAN && s.alive() && r.contains(s.pos)) ++n;
        return n;
    };
    auto enemies_in = [this](const sf::FloatRect& r)
    {
        int n = 0;
        for(const auto& s : m_soldiers) if(s.team == Team::RECHIVII && s.fighting() && r.contains(s.pos)) ++n;
        return n;
    };

    switch(m_stage)
    {
        case Stage::MARCH:
            if(m_stage_time > 7.0f && m_stage_time - dt <= 7.0f)
            {
                Soldier* lt = officer(Team::MONTERAN);
                if(lt && !lt->is_player) say(*lt, "Halvek's past the stream. We hold the mill by noon, then we eat.", 5.0f);
            }
            if(m_stage_time > 15.0f && m_stage_time - dt <= 15.0f)
                say("Pvt. Dacre", "Listen. Birds have stopped.", Team::MONTERAN, 3.5f);
            if(m_stage_time > 22.0f || p.pos.x > 820.0f)
            {
                // The first shot, from the wood.
                SoundBank::get().play_at("rifle", {1200, 400}, 1.0f);
                set_stage(Stage::CONTACT);
            }
            break;
        case Stage::CONTACT:
            if(m_stage_time > 10.0f) set_stage(Stage::WOOD);
            break;
        case Stage::WOOD:
            if((alive_in(FIELDS_PATROL) <= 1 || m_stage_time > 70.0f) && you_there(m_map.wood_area, 120.0f)) set_stage(Stage::STREAM);
            break;
        case Stage::STREAM:
            if((friends_in(sf::FloatRect({1560, 380}, {170, 260})) >= 3 || m_stage_time > 90.0f) && you_there(sf::FloatRect({1560, 380}, {170, 260}), 160.0f))
                set_stage(Stage::MILL);
            break;
        case Stage::MILL:
            if(enemies_in(m_map.mill_area) == 0 && friends_in(m_map.mill_area) >= 2 && you_there(m_map.mill_area, 60.0f))
            {
                set_stage(Stage::COUNTER);
                // The ones on the far bank pull back to the road.
                for(auto& s : m_soldiers)
                    if(s.team == Team::RECHIVII && s.fighting() && s.has_post && !s.reserve && s.post.x < 2000)
                    {
                        s.post = {Random::real(2120, 2160), Random::real(380, 660)};
                        s.path.clear();
                    }
            }
            break;
        default:
            break;
    }
    if(count(Team::RECHIVII) == 0 && m_stage >= Stage::CONTACT && m_stage < Stage::COUNTER)
    {
        bool reserve_waiting = false;
        for(const auto& s : m_soldiers) if(s.reserve && !s.active && s.fighting()) reserve_waiting = true;
        if(reserve_waiting) set_stage(Stage::COUNTER);
    }
}

bool Battle::you_there(const sf::FloatRect& area, float reach) const
{
    // The objective doesn't fall without you: you, or your man if he is
    // driving himself for a recording.
    if(m_autoplay) return true;
    const Soldier& p = me();
    if(!p.alive()) return true;
    float const dx = std::max({area.position.x - p.pos.x, 0.0f, p.pos.x - (area.position.x + area.size.x)});
    float const dy = std::max({area.position.y - p.pos.y, 0.0f, p.pos.y - (area.position.y + area.size.y)});
    return std::hypot(dx, dy) <= reach;
}

void Battle::raise_alarm(const std::string& why)
{
    if(m_alarm) return;
    m_alarm = true;
    m_alarm_at = m_time;
    m_illum_timer = 1.5f;
    for(auto& s : m_soldiers)
        if(s.team == Team::RECHIVII && !s.reserve)
        {
            // Waking up takes a moment: the watch is quick, a man asleep in
            // a dugout or beside his gun much less so.
            bool const asleep = !s.alerted;
            s.active = true;
            s.alerted = true;
            s.sitting = false;
            s.reaction = asleep ? Random::real(4.0f, 8.0f) : Random::real(0.8f, 2.5f);
            s.path.clear();
        }
    if(!why.empty())
    {
        notice(why, 3.0f);
        log("The raiders were discovered: " + why);
    }
    SoundBank::get().play("alarm_bell", 0.7f);
    m_combat_heat = 1.0f;
}

void Battle::script_raid(float dt)
{
    Soldier& p = *player();
    auto enemies_in = [this](const sf::FloatRect& r)
    {
        int n = 0;
        for(const auto& s : m_soldiers) if(s.team == Team::RECHIVII && s.fighting() && r.contains(s.pos)) ++n;
        return n;
    };
    auto friends_in = [this](const sf::FloatRect& r)
    {
        int n = 0;
        for(const auto& s : m_soldiers) if(s.team == Team::MONTERAN && s.alive() && r.contains(s.pos)) ++n;
        return n;
    };
    sf::FloatRect const their_line({660.0f, 110.0f}, {230.0f, 700.0f});      // the wire, the LP, the trench and its dugouts

    // Once they know, white stars go up over the slope every few seconds.
    if(m_alarm && m_stage < Stage::WON)
    {
        m_illum_timer -= dt;
        if(m_illum_timer <= 0.0f)
        {
            m_illum_timer = Random::real(7.0f, 11.0f);
            Soldier* shooter = nullptr;
            for(auto& s : m_soldiers)
                if(s.team == Team::RECHIVII && s.fighting() && s.active && (!shooter || s.pos.x > 760.0f)) shooter = &s;
            sf::Vector2f const from = shooter ? shooter->pos : sf::Vector2f{1000.0f, 450.0f};
            // Over wherever we are.
            sf::Vector2f over = p.pos;
            if(Random::chance(0.5f))
                for(const auto& s : m_soldiers) if(s.team == Team::MONTERAN && s.alive() && Random::chance(0.3f)) { over = s.pos; break; }
            (void)from;
            fire_flare(*(shooter ? shooter : &m_soldiers.back()), over + sf::Vector2f{Random::real(-80, 80), Random::real(-80, 80)}, true);
        }
    }
    // The reserve comes once they've had time to wake.
    if(m_alarm && m_time - m_alarm_at > 100.0f)
        for(auto& s : m_soldiers) if(s.reserve && !s.active && s.fighting()) s.active = true;

    switch(m_stage)
    {
        case Stage::RAID_CREEP:
        {
            if(m_alarm || friends_in(m_map.enemy_trench_area) > 0) { set_stage(Stage::RAID_TRENCH); break; }
            // Before they know: form up short of the wire, cut a lane through
            // both belts for each group, go through it, and go in together.
            std::vector<Section*> groups;
            for(auto& sec : m_sections) if(sec.team == Team::MONTERAN && section_leader(sec.id)) groups.push_back(&sec);
            // Nothing moves on without you: you're with your group, or near the point it is making for.
            auto with_us = [&](int wp, float reach)
            {
                if(m_autoplay || !p.alive()) return true;
                float band = 0.0f;
                for(auto* g : groups) if(g->id == p.section) band = g->band;
                sf::Vector2f const point = m_map.waypoints[static_cast<std::size_t>(wp)];
                // Yours, or (the lieutenant, or your group gone) anywhere along the line.
                return length(p.pos - (point + sf::Vector2f{0.0f, band})) < reach
                    || (band == 0.0f && std::abs(p.pos.x - point.x) < reach * 0.6f);
            };
            if(m_creep == 0)
            {
                bool formed = m_stage_time > 45.0f && with_us(2, 140.0f);
                if(!formed)
                {
                    formed = true;
                    for(auto* g : groups)
                        if(Soldier* cpl = section_leader(g->id); cpl && length(cpl->pos - (m_map.waypoints[2] + sf::Vector2f{0.0f, g->band})) > 50.0f) formed = false;
                }
                if(formed && with_us(2, 140.0f))
                {
                    m_creep = 1;
                    hint(p.cuts_wire() ? "Cut your group's lane: crawl to the wire and hold E. Or wait for the sapper." : "Lie still (Z) while the cutters work. Then through the gap after your corporal.", 7.0f);
                    if(Soldier* lt = officer(Team::MONTERAN); lt && !lt->is_player) say(*lt, "Cutters forward. Everyone else, down and wait.", 4.0f);
                    for(auto& s : m_soldiers) if(s.team == Team::MONTERAN && !s.is_player) s.stance = Stance::PRONE;
                }
            }
            else if(m_creep == 1)
            {
                bool all_through = true;
                for(auto* g : groups)
                {
                    if(g->belts_cut >= 2) continue;
                    all_through = false;
                    // The group's cutter: its sapper, or its corporal.
                    Soldier* cutter = nullptr;
                    for(auto& s : m_soldiers)
                        if(s.section == g->id && s.fighting() && (!s.is_player || m_autoplay) && (s.role == Role::SAPPER || (!cutter && s.leader))) cutter = &s;
                    if(!cutter) { g->belts_cut = 2; continue; }
                    float const y = m_map.waypoints[2].y + g->band;
                    float const wire_x = g->belts_cut == 0 ? 660.0f : 720.0f;
                    sf::Vector2f const at{wire_x, y};
                    // Has this belt already gone here (an old gap, or someone else's)?
                    if(!m_map.near_wire(at, 6.0f)) { ++g->belts_cut; continue; }
                    if(cutter->cutting > 0.0f) continue;
                    if(length(cutter->pos - at) < 13.0f)
                    {
                        cutter->cutting = cutter->role == Role::SAPPER ? 2.5f : 4.0f;
                        cutter->cut_at = at;
                        cutter->stance = Stance::CROUCH;
                    }
                    else if(length(cutter->goal - (at - sf::Vector2f{8.0f, 0.0f})) > 2.0f)
                    {
                        cutter->goal = at - sf::Vector2f{8.0f, 0.0f};
                        cutter->path.clear();
                        cutter->cover_index = -1;
                        cutter->order = Order::HOLD;
                    }
                }
                if(all_through || m_stage_time > 150.0f)
                {
                    m_creep = 2;
                    for(auto* g : groups) order_section(g->id, Order::ADVANCE, m_map.waypoints[3] + sf::Vector2f{0.0f, g->band});
                    // The lieutenant goes through with the first group, clear of their listening post.
                    if(Soldier* lt = officer(Team::MONTERAN); lt && !lt->is_player && !groups.empty())
                        set_goal(*lt, m_map.waypoints[3] + sf::Vector2f{-30.0f, groups.front()->band + 20.0f}, 20.0f);
                }
            }
            else if(m_creep == 2)
            {
                bool there = true;
                for(auto* g : groups)
                    if(Soldier* cpl = section_leader(g->id); cpl && length(cpl->pos - (m_map.waypoints[3] + sf::Vector2f{0.0f, g->band})) > 45.0f) there = false;
                if((there || m_stage_time > 160.0f) && with_us(3, 90.0f))
                {
                    // Over the lip together.
                    m_creep = 3;
                    for(auto* g : groups) order_section(g->id, Order::CHARGE, m_map.waypoints[4] + sf::Vector2f{0.0f, g->band});
                    if(Soldier* lt = officer(Team::MONTERAN); lt && !lt->is_player) bark(*lt, "Now! In!", true);
                }
            }
            break;
        }
        case Stage::RAID_TRENCH:
            // In unseen. A breath, then bombs down both dugout stairs, which
            // is how they find out. You're there for it.
            if(!m_alarm && m_creep < 4 && m_stage_time > 4.0f && you_there(m_map.enemy_trench_area, 50.0f))
            {
                // Both dugouts, and both guns before their crews are on them.
                sf::Vector2f const beds[] = {{864.0f, 216.0f}, {868.0f, 710.0f}, {782.0f, 300.0f}, {796.0f, 620.0f}};
                std::vector<int> used;
                bool thrown = false;
                for(sf::Vector2f bed : beds)
                {
                    Soldier* bomber = nullptr;
                    for(auto& s : m_soldiers)
                        if(s.team == Team::MONTERAN && s.fighting() && s.grenades > 0 && (!s.is_player || m_autoplay) && length(s.pos - bed) < 130.0f
                           && std::find(used.begin(), used.end(), s.id) == used.end()
                           && (!bomber || length(s.pos - bed) < length(bomber->pos - bed)))
                            bomber = &s;
                    if(!bomber) continue;
                    throw_grenade(*bomber, bed);
                    used.push_back(bomber->id);
                    if(!thrown) bark(*bomber, "Bombs down the stairs! Now!", true);
                    thrown = true;
                }
                if(!thrown && m_stage_time > 12.0f) raise_alarm("");
                else if(thrown) m_creep = 4;     // once is enough
            }
            if(enemies_in(their_line) == 0 && you_there(m_map.enemy_trench_area, 140.0f)) set_stage(Stage::RAID_HAMLET);
            break;
        case Stage::RAID_HAMLET:
            if(enemies_in(m_map.farm_area) == 0 && you_there(m_map.farm_area, 100.0f)) set_stage(Stage::RAID_DUGOUT);
            break;
        case Stage::RAID_DUGOUT:
        {
            // Nobody goes in the door of a concrete dugout. A charge against
            // its wall from the blind side, or a bomb in through the door.
            sf::FloatRect const& b = m_map.bunker_area;
            sf::Vector2f const centre = b.position + b.size * 0.5f;
            if(enemies_in(b) > 0 && you_there(b, 220.0f))
            {
                bool live = false;
                for(const auto& g : m_grenades) if(g.team == Team::MONTERAN && length(g.pos - centre) < 90.0f) live = true;
                Soldier* bomber = nullptr;
                bool satchel = false;
                for(auto& s : m_soldiers)
                {
                    if(s.team != Team::MONTERAN || !s.fighting() || (s.is_player && !m_autoplay) || s.role == Role::OFFICER) continue;
                    bool const has = s.satchels > 0;
                    if(!has && s.grenades <= 0) continue;
                    if(!bomber || (has && !satchel) || (has == satchel && length(s.pos - centre) < length(bomber->pos - centre))) { bomber = &s; satchel = has; }
                }
                if(bomber && !live)
                {
                    m_waypoint_time = 0.0f;      // the rest wait for it
                    // Which wall: the side with more of them close behind it.
                    int north = 0, south = 0;
                    for(const auto& e : m_soldiers)
                        if(e.team == Team::RECHIVII && e.fighting() && b.contains(e.pos)) ++(e.pos.y < centre.y ? north : south);
                    bool const top = north >= south;
                    sf::Vector2f const spot = satchel ? sf::Vector2f{centre.x, top ? b.position.y - 10.0f : b.position.y + b.size.y + 10.0f}
                                                      : sf::Vector2f{b.position.x - 16.0f, 473.0f};
                    if(length(bomber->pos - spot) < 8.0f)
                    {
                        if(satchel) place_satchel(*bomber, {centre.x, top ? b.position.y - 2.0f : b.position.y + b.size.y + 2.0f});
                        else { throw_grenade(*bomber, {centre.x, 473.0f}); bark(*bomber, "Bomb in the door!", true); }
                        bomber->goal = spot + sf::Vector2f{-46.0f, satchel ? (top ? -36.0f : 36.0f) : -40.0f};
                        bomber->path.clear();
                        bomber->cover_index = -1;
                    }
                    else if(length(bomber->goal - spot) > 3.0f)
                    {
                        bomber->goal = spot;
                        bomber->path.clear();
                        bomber->cover_index = -1;
                        bomber->order = Order::HOLD;
                        bomber->stance = Stance::CROUCH;
                        bark(*bomber, satchel ? "Cover me! I'll put a charge on it!" : "Cover me! Going for the door!", false);
                    }
                }
            }
            if(enemies_in(m_map.bunker_area) == 0 && friends_in(m_map.bunker_area) + friends_in(sf::FloatRect({1370, 420}, {120, 110})) > 0
               && you_there(m_map.bunker_area, 140.0f))
                set_stage(Stage::RAID_WITHDRAW);
            break;
        }
        case Stage::RAID_WITHDRAW:
        {
            // Their guns, on the slope we have to cross.
            m_barrage_timer -= dt;
            if(m_barrage_timer <= 0.0f)
            {
                m_barrage_timer = Random::real(2.5f, 4.5f);
                m_shells.push_back({{Random::real(240, 640), Random::real(180, 720)}, 1.8f, false, 0});
            }
            // Home: you in the ditch, and nobody else still on his way.
            if(m_map.front_trench_area.contains(p.pos) || (m_autoplay && friends_in(m_map.front_trench_area) > 0))
            {
                bool still_out = false;
                for(const auto& s : m_soldiers)
                    if(s.team == Team::MONTERAN && s.alive() && !s.is_player && !m_map.front_trench_area.contains(s.pos) && s.pos.x < 1000.0f) still_out = true;
                if(!still_out || m_stage_time > 60.0f) set_stage(Stage::WON);
            }
            break;
        }
        default:
            break;
    }

    // Nobody left to raid: on with it.
    int holding_out = 0;
    for(const auto& s : m_soldiers) if(s.team == Team::RECHIVII && s.fighting() && s.active && !s.reserve) ++holding_out;
    if(m_alarm && holding_out == 0 && m_stage_time > 3.0f && m_stage >= Stage::RAID_TRENCH && m_stage < Stage::RAID_WITHDRAW)
        set_stage(Stage::RAID_WITHDRAW);
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
    c.platoon.clear();
    c.enemy_dead = c.enemy_fled = 0;
    for(auto& s : m_soldiers)
    {
        if(s.team == Team::MONTERAN)
        {
            Soldier copy = s;
            bool const left = m_level == LevelId::RAID && !m_map.front_trench_area.contains(copy.pos) && copy.life != Life::DEAD;
            if(copy.fate.empty() && left)
            {
                // Not back by first light: nobody knows.
                copy.fate = copy.life == Life::DOWNED ? "Left wounded in the dark. Missing" : "Not back by first light. Missing";
                copy.life = Life::DEAD;
            }
            if(copy.fate.empty())
            {
                if(copy.life == Life::DOWNED) copy.fate = "Carried back to the aid post";
                else if(copy.lost & (ARM_L | ARM_R)) copy.fate = "Survived, lost an arm";
                else if(copy.lost & (LEG_L | LEG_R)) copy.fate = "Survived, lost a leg";
                else if(copy.hp < 60.0f) copy.fate = "Wounded, survived";
                else copy.fate = "Survived";
            }
            c.platoon.push_back(copy);
        }
        else if(s.life == Life::FLED && !s.surrendered) ++c.enemy_fled;
        else if(!s.alive() && s.active && !s.surrendered) ++c.enemy_dead;
    }
    c.enemy_dead += m_hill_dead;
    c.enemy_fled += m_hill_fled;
    if(m_level == LevelId::HILL) c.waves = m_waves_held;
}

}
