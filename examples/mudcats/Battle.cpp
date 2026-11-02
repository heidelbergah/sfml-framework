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
#include "Screens.hpp"
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

    constexpr float RUN_FROM_SHORTCUT = 900.0f;     // MUDCATS_STAGE=2 on the charge: the lines put here

}

std::string difficulty_name(int d)
{
    return tr("difficulty." + std::to_string(std::clamp(d, 0, 2)));
}

const char* level_key(LevelId level)
{
    switch(level)
    {
        case LevelId::TOWN:     return "town";
        case LevelId::FOREST:   return "forest";
        default:                return "charge";
    }
}

const char* side_key(Team team)
{
    return team == Team::MONTERAN ? "monteran" : "rechivii";
}

std::string level_name(LevelId level)
{
    return tr(std::string("levels.") + level_key(level) + ".name");
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
    m_heard.clear();

    Campaign& c = Campaign::current();
    m_level = c.level;
    c.won = false;
    c.time = 0.0f;
    c.wagons_lost = 0;
    m_next_name[0] = m_next_name[1] = 0;

    c.playing = c.side;
    build_world();
    m_soldiers.clear();
    m_sections.clear();
    if(m_level == LevelId::TOWN) spawn_town();
    else if(m_level == LevelId::FOREST) spawn_forest();
    else spawn_charge();
    // The town: houses already burning when we get there.
    for(sf::Vector2f b : m_map.burning) m_fires.push_back({b, 7.0f, 1.0e9f, 0.0f});

    m_stage_time = m_time = m_real = 0.0f;
    m_waypoint = 0;
    m_bounding = 0;
    m_waypoint_time = 0.0f;
    m_end_timer = 0.0f;
    m_combat_heat = 0.0f;
    m_officer_dead_noted = false;
    m_section_hint_given = false;
    m_hurt = m_flash = m_takeover = m_notice = 0.0f;
    m_up = m_down = m_left = m_right = m_sprint = m_trigger = m_aiming = false;
    m_selected = -1;
    m_night = m_level == LevelId::TOWN ? 0.36f : 0.0f;
    m_alarm = m_level != LevelId::FOREST;
    m_alarm_at = 0.0f;
    m_illum_timer = 6.0f;
    m_missions.clear();
    m_kick = {};
    m_bound_wait = -1;
    m_cover_until = 0.0f;
    m_assault = false;
    m_keep_up = 0.0f;
    m_smoke_mission = false;
    m_look = {};
    m_second_off = m_colours_down = m_roared = false;
    m_colours_taken = 0;
    m_town_broken = false;
    m_town_check = 0.0f;
    m_bell_timer = 1.5f;
    m_bells = 0;
    m_far_guns = 3.0f;
    m_barrage_timer = 1.0f;
    // For recording and testing: your man is driven like the others.
    m_autoplay = std::getenv("MUDCATS_AUTOPLAY") != nullptr;
    set_stage(m_level == LevelId::TOWN ? Stage::TOWN_ENTER : (m_level == LevelId::FOREST ? Stage::FOREST_MARCH : Stage::CHARGE_FORM));

    // Shortcuts for working on later parts of a level.
    if(const char* env = std::getenv("MUDCATS_STAGE"); env && m_level == LevelId::CHARGE)
    {
        // 1: the lines just stepping off. 2: the whistles, a little way out.
        int const s = std::atoi(env);
        if(s >= 1) set_stage(Stage::CHARGE_WALK);
        if(s >= 2)
        {
            for(auto& f : m_soldiers)
                if(f.team == Team::RECHIVII && f.line >= 0) f.pos = f.goal = {RUN_FROM_SHORTCUT + 140.0f * static_cast<float>(f.line) + Random::real(-10, 10), f.file_y};
            m_second_off = true;
            set_stage(Stage::CHARGE_RUN);
        }
    }

    if(const char* env = std::getenv("MUDCATS_STAGE"); env && m_level == LevelId::FOREST) forest_shortcut(std::atoi(env));

    if(const char* env = std::getenv("MUDCATS_STAGE"); env && m_level == LevelId::TOWN)
    {
        // 1: at the cross-street. 2: at the square. 3: in the church, the counter-attack coming.
        int const s = std::atoi(env);
        float const x = s >= 3 ? 2140.0f : (s >= 2 ? 1420.0f : 760.0f);
        for(auto& f : m_soldiers)
        {
            if(f.team == Team::MONTERAN)
            {
                float const y = f.section < 0 ? 500.0f : 500.0f + m_sections[static_cast<std::size_t>(f.section)].band * (s >= 3 ? 0.2f : 1.0f);
                f.pos = f.goal = {x + Random::real(-40, 20), y + Random::real(-24, 24)};
                if(!m_map.walkable(f.pos)) f.pos = f.goal = {x, 500.0f + Random::real(-20, 20)};
            }
            else if(f.has_post && f.post.x < x - 20.0f && !f.is_player) { f.life = Life::DEAD; f.active = false; }
        }
        for(auto& sec : m_sections) if(sec.team == Team::MONTERAN) sec.at = s >= 3 ? 8 : (s >= 2 ? 6 : 3);
        set_stage(Stage::TOWN_STREET);
        if(s >= 1) set_stage(Stage::TOWN_CROSS);
        if(s >= 2) set_stage(Stage::TOWN_SQUARE);
        if(s >= 3) set_stage(Stage::COUNTER);
    }

    if(const char* speed = std::getenv("MUDCATS_SPEED")) GameTime::set_scale(static_cast<float>(std::atof(speed)));

    m_camera.set_bounds(sf::FloatRect({0.0f, 0.0f}, m_map.size));
    m_camera.set_center(player()->pos, true);

    SoundBank& sound = SoundBank::get();
    sound.set_combat_track(m_level == LevelId::CHARGE ? SoundBank::Track::CHARGE : (m_level == LevelId::TOWN ? SoundBank::Track::TOWN : SoundBank::Track::FOREST));
    sound.set_calm(false);
    sound.lock_to_music(m_level == LevelId::CHARGE ? "drums_loop" : "");
    sound.start_music();
    sound.set_intensity(0.0f);
    sound.set_front(m_level == LevelId::FOREST ? 0.12f : 0.3f);
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
    m_mortars.clear();
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
            if(s.team != Campaign::current().playing || s.role != chosen) continue;
            if(chosen != Role::OFFICER && s.section != section) continue;
            if(pass == 0 && s.leader && chosen != Role::OFFICER) continue;
            m_player = s.id;
            break;
        }
    if(m_player < 0)
        for(auto& s : m_soldiers) if(s.team == Campaign::current().playing && s.section == section && !s.leader) { m_player = s.id; break; }
    if(m_player < 0) m_player = 0;
    m_soldiers[static_cast<std::size_t>(m_player)].is_player = true;
}

Soldier& Battle::add(Team team, Role role, Rank rank, std::string name, sf::Vector2f pos, int section)
{
    Soldier s;
    s.id      = static_cast<int>(m_soldiers.size());
    s.team    = team;
    s.role    = role;
    s.gun     = default_gun(role);
    s.rank    = rank;
    s.name    = std::move(name);
    s.pos     = pos;
    s.goal    = pos;
    s.section = section;
    s.aim     = team == Team::MONTERAN ? 0.0f : 3.14159f;

    kit_out(s);
    s.think    = Random::real(0.0f, 0.3f);
    s.morale   = Random::real(0.8f, 1.0f);
    // His voice: most men in the middle, some deep, some lighter. Never a boy's.
    {
        float const r = Random::real(0.0f, 1.0f);
        s.voice = r < 0.3f ? Random::real(0.84f, 0.93f) : (r < 0.8f ? Random::real(0.93f, 1.04f) : Random::real(1.04f, 1.12f));
    }
    s.order    = Order::HOLD;
    m_soldiers.push_back(s);
    return m_soldiers.back();
}

std::string Battle::next_name(Team team)
{
    // Down the list for that side, skipping anyone already on the field
    // (your section and the lieutenant are named by hand).
    const auto& names = list(team == Team::MONTERAN ? "names.monteran.men" : "names.rechivii.men");
    if(names.empty()) return "?";
    std::size_t& next = m_next_name[team == Team::MONTERAN ? 0 : 1];
    for(std::size_t tries = 0; tries < names.size(); ++tries)
    {
        const std::string& name = names[next++ % names.size()];
        bool taken = false;
        for(const auto& s : m_soldiers) if(s.team == team && s.name == name) taken = true;
        if(!taken) return name;
    }
    return names[next++ % names.size()];
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
    // Orders are for a Monteran platoon. On the other side you're one man in the charge.
    if(p.team != Team::MONTERAN) return false;
    return p.role == Role::OFFICER;
}

bool Battle::player_commands_section() const
{
    // A corporal, or whoever stepped up when the corporal fell: his own section, nothing more.
    if(m_autoplay) return false;
    const Soldier& p = me();
    return p.alive() && p.team == Team::MONTERAN && p.role != Role::OFFICER && p.leader && p.section >= 0;
}

void Battle::say(const Soldier& who, const std::string& text, float time)
{
    if(!who.alive()) return;
    say(short_name(who), text, who.team, time);
    SoundBank::get().play_at("bark", who.pos, 0.6f, 0.04f, who.voice);
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
    if(voice) SoundBank::get().play_at("bark", s.pos, 0.55f, 0.04f, s.voice);
}

void Battle::shout(Soldier& s, std::string_view key, bool voice)
{
    std::string const own = std::string("barks.") + side_key(s.team) + "." + std::string(key);
    bark(s, pick(has_text(own) ? own : "barks.monteran." + std::string(key)), voice);
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
    }
}

void Battle::order_from_player(Order order)
{
    Soldier& p = *player();
    // A section leader orders his own section; only the lieutenant has the others.
    int const only = player_leads() ? m_selected : p.section;
    for(auto& sec : m_sections)
    {
        if(sec.team != Team::MONTERAN) continue;
        if(only >= 0 && sec.id != only) continue;
        // Each section takes its own share of the line around the cursor.
        float const band = only >= 0 ? 0.0f : sec.band * 0.5f;
        order_section(sec.id, order, m_mouse_world + sf::Vector2f{0.0f, band}, m_mouse_world);
    }
    // The lieutenant's own runners, if he is not you.
    bark(p, item("orders.words", static_cast<std::size_t>(order)), true);
    p.bark_cooldown = 0.0f;
    if(order == Order::ADVANCE || order == Order::CHARGE) SoundBank::get().play_at("whistle", p.pos, 0.7f);
    if(order == Order::CHARGE) SoundBank::get().play_at("charge_cry", p.pos, 0.9f, 0.0f, p.voice);
}

void Battle::promote(int section)
{
    if(section < 0 || section_leader(section)) return;
    // The most senior man left takes over: a corporal if there is one.
    Soldier* best = nullptr;
    for(auto& s : m_soldiers)
        if(s.section == section && s.fighting() && (!best || (s.rank == Rank::CORPORAL && best->rank != Rank::CORPORAL))) best = &s;
    if(!best) return;
    best->leader = true;
    if(best->team == Team::MONTERAN)
    {
        notice(tr("hud.notice.takes_section", {{"name", short_name(*best)}, {"section", m_sections[static_cast<std::size_t>(section)].name}}), 3.0f);
    }
}

void Battle::platoon_orders(float dt)
{
    for(auto& sec : m_sections) sec.since += dt;

    // Losing a corporal: someone steps up.
    for(auto& sec : m_sections) promote(sec.id);

    // Who runs the platoon: the lieutenant, or with him gone, the senior
    // corporal still on his feet. Never you, unless you are the lieutenant:
    // a private who finds himself leading has his own section, and that's all.
    Soldier* lt = officer(Team::MONTERAN);
    if(lt && lt->is_player && !m_autoplay) lt = nullptr;
    if(!lt) lt = acting_commander();
    if(m_level == LevelId::TOWN && m_town_broken) return;
    if(!lt || player_leads()) return;
    if(!fighting_started() || m_level == LevelId::CHARGE) return;

    m_waypoint_time += dt;
    int limit = static_cast<int>(m_map.waypoints.size()) - 1;
    if(m_level == LevelId::TOWN)
    {
        limit = 3;
        if(m_stage == Stage::TOWN_CROSS) limit = 6;
        if(m_stage == Stage::TOWN_SQUARE) limit = 8;
    }
    else if(m_level == LevelId::FOREST) limit = forest_limit();
    if(m_stage == Stage::COUNTER) return;
    leapfrog(*lt, limit);
}

Soldier* Battle::acting_commander()
{
    // The senior corporal: the leader of the lowest-numbered section that
    // still has one, and who isn't you.
    for(auto& sec : m_sections)
    {
        if(sec.team != Team::MONTERAN) continue;
        Soldier* cpl = section_leader(sec.id);
        if(cpl && (!cpl->is_player || m_autoplay)) return cpl;
    }
    return nullptr;
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
    {
        if(sec.team != Team::MONTERAN || !section_leader(sec.id)) continue;
        // A section you are leading goes where you take it; the rest of the platoon gets on without it.
        if(player_commands_section() && sec.id == player()->section) continue;
        secs.push_back(&sec);
    }
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
    auto point_of = [&](int at, const Section& sec)
    {
        bool const inside = m_level == LevelId::TOWN && at == 8;
        bool const square = m_level == LevelId::TOWN && at == 7;      // across the square: keep away from the town hall's windows
        return m_map.waypoints[static_cast<std::size_t>(std::clamp(at, 0, static_cast<int>(m_map.waypoints.size()) - 1))]
             + (m_level == LevelId::FOREST ? sf::Vector2f{sec.band, 0.0f}        // the forest: abreast along the road
                                           : sf::Vector2f{0.0f, sec.band * (inside ? 0.2f : (square ? 0.5f : 1.0f))});
    };
    sf::Vector2f const forward = m_level == LevelId::FOREST ? forest_forward() : sf::Vector2f{160.0f, 0.0f};

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
            m_cover_until = m_time + 2.0f;
            if(Soldier* cpl = section_leader(sec->id)) bark(*cpl, tr("barks.monteran.in_position"), false);
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
            bark(lt, tr(d < 90.0f ? "barks.monteran.get_in" : "barks.monteran.work_round"), true);
        }
        else
        {
            bool const charge = (m_level == LevelId::TOWN && (sec.heading == 1 || sec.heading == 4 || sec.heading == 8))
                             || (m_level == LevelId::FOREST && sec.heading == forest_limit());
            order_section(sec.id, charge ? Order::CHARGE : Order::ADVANCE, point_of(sec.heading, sec));
            if(charge)
            {
                bark(lt, tr("barks.monteran.fix_bayonets"), true);
                SoundBank::get().play_at("whistle", lt.pos, 0.9f);
                SoundBank::get().play_at("charge_cry", lt.pos, 0.9f, 0.0f, lt.voice);
            }
            else if(cpl)
            {
                bark(*cpl, item("barks.monteran.bound", static_cast<std::size_t>(m_bounding)), true);
            }
            // In the town, open ground is a street to cross: the corporal smokes it first.
            if(m_level == LevelId::TOWN && cpl && cpl->smokes > 0 && (sec.heading == 4 || sec.heading == 7) && (!cpl->is_player || m_autoplay))
            {
                sf::Vector2f const across = point_of(sec.heading, sec);
                throw_grenade(*cpl, cpl->pos + (across - cpl->pos) * 0.55f + sf::Vector2f{0.0f, sec.band > 0.0f ? -30.0f : 30.0f}, true);
                bark(*cpl, tr("barks.monteran.smoke_wait"), true);
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
                if(Soldier* cpl = section_leader(mine->id)) { cpl->bark_cooldown = 0.0f; bark(*cpl, tr("barks.monteran.keep_up"), true); }
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
            if(Soldier* cpl = section_leader(sec->id); cpl && Random::chance(0.5f)) bark(*cpl, tr("barks.monteran.covering_fire"), true);
        }
    go->heading = heading;
    m_bound_wait = go->id;
    m_cover_until = m_time + 2.5f;
}

// ---------------------------------------------------------------- input

void Battle::handle_event(const sf::Event& event, sf::Vector2f mouse_pos)
{
    m_mouse_screen = mouse_pos;
    using S = sf::Keyboard::Scancode;
    if(global_key(event, *m_manager)) return;

    // The pause screen's sound levels: dragged with the mouse, or picked and nudged with the arrows.
    if(m_paused)
    {
        if(const auto* press = event.getIf<sf::Event::MouseButtonPressed>(); press && press->button == sf::Mouse::Button::Left)
            for(int i = 0; i < SoundBank::BUSES; ++i)
            {
                sf::FloatRect const bar = sound_bar(i);
                if(sf::FloatRect{bar.position - sf::Vector2f{6.0f, 10.0f}, bar.size + sf::Vector2f{12.0f, 20.0f}}.contains(mouse_pos)) { m_sound_drag = i; m_sound_row = i; }
            }
        if(event.is<sf::Event::MouseButtonReleased>() && m_sound_drag >= 0) { m_sound_drag = -1; SoundBank::get().save_settings(); }
        if(m_sound_drag >= 0)
        {
            sf::FloatRect const bar = sound_bar(m_sound_drag);
            set_sound_level(m_sound_drag, (mouse_pos.x - bar.position.x) / bar.size.x);
        }
    }

    if(const auto* key = event.getIf<sf::Event::KeyPressed>())
    {
        if(m_paused)
        {
            auto& bank = SoundBank::get();
            auto const bus = static_cast<SoundBank::Bus>(m_sound_row);
            switch(key->scancode)
            {
                case S::Escape: case S::P: m_paused = false; m_sound_drag = -1; GameTime::resume(); bank.save_settings(); break;
                case S::Q: GameTime::resume(); bank.save_settings(); m_manager->request_scene_switch("briefing", SceneTransitionType::FADE); break;
                case S::M: bank.toggle_music_mute(); break;
                case S::Up: case S::W: m_sound_row = (m_sound_row + SoundBank::BUSES - 1) % SoundBank::BUSES; bank.play("ui_hover", 0.5f); break;
                case S::Down: case S::S: m_sound_row = (m_sound_row + 1) % SoundBank::BUSES; bank.play("ui_hover", 0.5f); break;
                case S::Left: case S::A: set_sound_level(m_sound_row, bank.volume(bus) - 0.05f); break;
                case S::Right: case S::D: set_sound_level(m_sound_row, bank.volume(bus) + 0.05f); break;
                default: break;
            }
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
                else if(p->alive()) notice(tr("hud.notice.no_smoke"), 1.5f);
                break;
            case S::X:
                if(p->alive() && p->flares > 0) fire_flare(*p, m_mouse_world);
                else if(p->alive() && p->role == Role::OFFICER) notice(tr("hud.notice.no_flares"), 1.5f);
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
                else if(p->role == Role::SAPPER) notice(tr("hud.notice.no_satchels"), 1.5f);
                break;
            case S::B:
                if(p->alive() && p->role == Role::RADIOMAN)
                {
                    m_smoke_mission = !m_smoke_mission;
                    notice(tr(m_smoke_mission ? "hud.notice.next_smoke" : "hud.notice.next_shells"), 1.8f);
                    SoundBank::get().play("ui_hover", 0.6f);
                }
                break;
            case S::E:
                if(p->life == Life::DOWNED)
                {
                    SoundBank::get().play_at("medic_call", p->pos, 0.8f, 0.02f, p->voice);
                    bark(*p, tr("barks.monteran.medic"), false);
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
                if(p->life == Life::DOWNED) { kill(*p, -1); take_over(); }
                break;
            case S::Tab:
                if(leading)
                {
                    int const sections = static_cast<int>(std::count_if(m_sections.begin(), m_sections.end(), [](const Section& s) { return s.team == Team::MONTERAN; }));
                    m_selected = m_selected + 1 >= sections ? -1 : m_selected + 1;
                    notice(m_selected < 0 ? tr("hud.notice.orders_all") : tr("hud.notice.orders_to", {{"section", m_sections[static_cast<std::size_t>(m_selected)].name}}), 1.8f);
                    SoundBank::get().play("ui_hover", 0.6f);
                }
                break;
            case S::Num1: case S::Num2: case S::Num3: case S::Num4: case S::Num5:
                if(leading || player_commands_section())
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

void Battle::set_sound_level(int row, float value)
{
    auto& bank = SoundBank::get();
    auto const bus = static_cast<SoundBank::Bus>(row);
    float const before = bank.volume(bus);
    bank.set_volume(bus, value);
    if(bank.volume(bus) == before) return;
    // A sample of what it sounds like now.
    if(bus == SoundBank::Bus::VOICES) bank.play("grunt", 0.7f);
    else if(bus != SoundBank::Bus::MUSIC) bank.play("ui_hover", 0.8f);
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
            hint(tr("hints.treating", {{"name", near->name}}), 3.0f);
        }
        else if(near->life == Life::DOWNED && near->dragged_by < 0)
        {
            p.dragging = near->id;
            near->dragged_by = p.id;
            p.stance = Stance::CROUCH;
            hint(tr("hints.dragging"), 4.0f);
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
        hint(tr("hints.cutting"), 3.0f);
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
            SoundBank::get().play_at("ammo_pickup", p.pos, 0.8f);
            notice(tr("hud.notice.filled_up"), 2.0f);
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
    if(p.dragging >= 0) return tr("hud.prompt.let_go");
    if(p.cutting > 0.0f) return tr("hud.prompt.cutting");
    if(p.treating >= 0) return tr("hud.prompt.treating");
    const Soldier* near = nullptr;
    float best = 22.0f;
    for(const auto& o : m_soldiers)
    {
        if(o.id == p.id || o.team != p.team) continue;
        bool const needs = o.life == Life::DOWNED || (o.alive() && o.bleeding > 0.0f);
        float const d = std::hypot(o.pos.x - p.pos.x, o.pos.y - p.pos.y);
        if(needs && d < best) { best = d; near = &o; }
    }
    std::string const who = near ? surname(*near) : "";
    if(near && p.role == Role::MEDIC && p.medkits > 0) return tr("hud.prompt.treat", {{"name", who}});
    if(near && near->life == Life::DOWNED && near->dragged_by < 0) return tr("hud.prompt.drag", {{"name", who}});
    if(p.cuts_wire() && m_map.near_wire(p.pos, 12.0f)) return tr("hud.prompt.cut_wire");
    for(auto c : m_map.crates) if(std::hypot(c.x - p.pos.x, c.y - p.pos.y) < 18.0f) return tr("hud.prompt.fill_up");
    for(const auto& d : m_dropped)
        if(!d.taken && std::hypot(d.pos.x - p.pos.x, d.pos.y - p.pos.y) < 16.0f)
            return d.gun == p.gun ? tr("hud.prompt.take_rounds") : tr("hud.prompt.take_weapon", {{"weapon", gun_name(d.gun)}});
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
    if((calling || p.cutting > 0.0f) && length(in) > 0.0f) p.cutting = 0.0f;
    if(calling || p.cutting > 0.0f) speed = 0.0f;
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
        hint(tr("hints.set_up_gun"), 3.0f);
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
            hint(tr("hints.bandaged"), 2.5f);
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
    // How long the engagement lasted: from the first shots to the end.
    float& fought = Campaign::current().time;
    if(m_stage != Stage::WON && m_stage != Stage::LOST && (fought > 0.0f || fighting_started())) fought += dt;

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
    Team const side = player()->team;
    for(auto& s : m_soldiers) s.yours = s.team == side && mine >= 0 && s.section == mine && !s.is_player;

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
    bool const battle = fighting_started();
    if(m_stage == Stage::WON || m_stage == Stage::LOST) target = 0.0f;
    else if(battle) target = std::max(target, 0.5f);
    SoundBank& sound = SoundBank::get();
    sound.set_intensity(target);
    sound.set_music_pitch(m_level != LevelId::CHARGE ? 1.0f : (m_stage == Stage::CHARGE_WALK ? 0.95f : (m_stage == Stage::CHARGE_RUN ? 1.07f : 1.0f)));

    // The weather.
    if(!GameTime::is_paused())
    {
        if(m_level == LevelId::CHARGE) sound.set_loop("dawn_loop", 0.3f * (1.0f - std::min(1.0f, m_combat_heat * 2.0f)) + 0.08f);
        else if(m_level == LevelId::TOWN) sound.set_loop("night_loop", 0.22f);
        else sound.set_loop("forest_loop", 0.36f * (1.0f - std::min(1.0f, m_combat_heat * 1.5f)) + 0.08f);
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
    std::string const lead_hint = tr("hints.lead_orders");

    switch(stage)
    {
        // ======================================================== the Sava meadow
        case Stage::CHARGE_FORM:
            if(p.team == Team::MONTERAN)
            {
                m_objective = tr("levels.charge.objective.monteran");
                if(ai_lt) say(*lt, tr("levels.charge.form.monteran_lt"), 5.0f);
                hint(tr("levels.charge.form.monteran_hint"), 8.0f);
            }
            else
            {
                m_objective = tr("levels.charge.objective.rechivii");
                say(tr("speakers.rch_captain"), tr("levels.charge.form.rechivii_captain"), Team::RECHIVII, 5.0f);
                hint(tr("levels.charge.form.rechivii_hint"), 8.0f);
            }
            break;
        case Stage::CHARGE_WALK:
            SoundBank::get().play_at("whistle", {2380.0f, 550.0f}, 1.0f);
            if(p.team == Team::MONTERAN)
            {
                m_objective = tr("levels.charge.objective.monteran");
                if(ai_lt) say(*lt, tr("levels.charge.walk.monteran_lt"), 4.0f);
            }
            else say(tr("speakers.rch_captain"), tr("levels.charge.walk.rechivii_captain"), Team::RECHIVII, 3.0f);
            m_combat_heat = 0.7f;
            break;
        case Stage::CHARGE_RUN:
        {
            // The whistles, and the whole line roars.
            sf::Vector2f centre;
            int count = 0;
            for(const auto& s : m_soldiers) if(s.team == Team::RECHIVII && s.line >= 0 && s.fighting()) { centre += s.pos; ++count; }
            if(count) centre /= static_cast<float>(count);
            SoundBank& sound = SoundBank::get();
            sound.play_at("whistle", centre, 1.0f);
            sound.play_at("war_roar", centre, 1.0f, 0.02f);
            sound.play("war_roar", 0.3f, 0.03f);
            for(int k = 0; k < 18; ++k)
            {
                const Soldier* who = nullptr;
                for(int tries = 0; tries < 8 && !who; ++tries)
                {
                    const Soldier& o = m_soldiers[static_cast<std::size_t>(Random::integer(0, static_cast<int>(m_soldiers.size()) - 1))];
                    if(o.team == Team::RECHIVII && o.line >= 0 && o.fighting()) who = &o;
                }
                if(who) m_later.push_back({Random::real(0.1f, 4.0f), "war_cry", who->pos, Random::real(0.5f, 0.9f), who->voice});
            }
            for(auto& s : m_soldiers) if(s.team == Team::RECHIVII && s.line >= 0 && s.fighting() && Random::chance(0.25f)) bark(s, pick("barks.rechivii.urra"), false);
            if(p.team == Team::MONTERAN && ai_lt) say(*lt, tr("levels.charge.run.monteran_lt"), 3.0f);
            else if(p.team == Team::RECHIVII) say(tr("speakers.rch_captain"), tr("levels.charge.run.rechivii_captain"), Team::RECHIVII, 3.0f);
            m_combat_heat = 1.0f;
            break;
        }

        // ======================================================== Kalvo
        case Stage::TOWN_ENTER:
            if(p.team == Team::MONTERAN)
            {
                m_objective = tr("levels.town.enter.monteran_objective");
                if(ai_lt) say(*lt, tr("levels.town.enter.monteran_lt"), 5.5f);
                hint(you_lead ? lead_hint : tr("levels.town.enter.monteran_hint"), 9.0f);
                for(auto& sec : m_sections)
                    if(sec.team == Team::MONTERAN)
                    {
                        order_section(sec.id, Order::ADVANCE, m_map.waypoints[0] + sf::Vector2f{0.0f, sec.band * 0.8f});
                        sec.at = 0;
                    }
            }
            else
            {
                m_objective = tr("levels.town.enter.rechivii_objective");
                say(tr("speakers.rch_lieutenant"), tr("levels.town.enter.rechivii_lt"), Team::RECHIVII, 5.0f);
                hint(tr("levels.town.enter.rechivii_hint"), 9.0f);
                for(auto& sec : m_sections)
                    if(sec.team == Team::MONTERAN)
                    {
                        order_section(sec.id, Order::ADVANCE, m_map.waypoints[0] + sf::Vector2f{0.0f, sec.band * 0.8f});
                        sec.at = 0;
                    }
            }
            break;
        case Stage::TOWN_STREET:
            m_objective = tr(p.team == Team::MONTERAN ? "levels.town.street.monteran_objective" : "levels.town.street.rechivii_objective");
            if(p.team == Team::MONTERAN && ai_lt) say(*lt, tr("levels.town.street.monteran_lt"), 4.0f);
            m_waypoint = 0;
            m_waypoint_time = 3.0f;
            m_combat_heat = std::max(m_combat_heat, 0.7f);
            break;
        case Stage::TOWN_CROSS:
            m_objective = tr(p.team == Team::MONTERAN ? "levels.town.cross.monteran_objective" : "levels.town.cross.rechivii_objective");
            if(p.team == Team::MONTERAN && ai_lt) say(*lt, tr("levels.town.cross.monteran_lt"), 4.5f);
            all_rechivii([this](Soldier& s) { if(s.section == m_town_sections[1]) s.active = true; });
            town_fall_back(m_town_sections[0], m_town_sections[1]);
            town_reinforce(8);
            break;
        case Stage::TOWN_SQUARE:
            m_objective = tr(p.team == Team::MONTERAN ? "levels.town.square.monteran_objective" : "levels.town.square.rechivii_objective");
            if(p.team == Team::MONTERAN && ai_lt) say(*lt, tr("levels.town.square.monteran_lt"), 5.0f);
            all_rechivii([this](Soldier& s)
            {
                if(s.section == m_town_sections[2] || s.section == m_town_sections[3] || s.serving >= 0 || (s.role == Role::OFFICER && !s.reserve)) s.active = true;
            });
            town_fall_back(m_town_sections[1], m_town_sections[2]);
            town_reinforce(10);
            // The battery has been given the church: a salvo on the tower and the town hall's front.
            for(int i = 0; i < 10; ++i)
            {
                sf::Vector2f const t = i % 2 == 0 ? sf::Vector2f{Random::real(2016, 2064), Random::real(470, 530)}
                                                  : sf::Vector2f{Random::real(1560, 1880), Random::real(236, 262)};
                m_shells.push_back({t, 4.0f + static_cast<float>(i) * 0.8f + Random::real(0.0f, 0.4f), false, 1});
            }
            if(p.team == Team::MONTERAN) say(tr("speakers.runner"), tr("levels.town.square.runner"), Team::MONTERAN, 4.0f);
            else say(tr("speakers.rch_captain"), tr("levels.town.square.rechivii_captain"), Team::RECHIVII, 3.5f);
            break;

        // ======================================================== the Lesna road
        case Stage::FOREST_MARCH:
            if(p.team == Team::MONTERAN)
            {
                m_objective = tr("levels.forest.march.monteran_objective");
                hint(tr("levels.forest.march.monteran_hint"), 9.0f);
            }
            else
            {
                m_objective = tr("levels.forest.march.rechivii_objective");
                say(tr("speakers.rch_lieutenant"), tr("levels.forest.march.rechivii_lt"), Team::RECHIVII, 5.5f);
                hint(tr("levels.forest.march.rechivii_hint"), 9.0f);
            }
            break;
        case Stage::FOREST_AMBUSH:
        {
            halt_column();
            m_objective = tr(p.team == Team::MONTERAN ? "levels.forest.ambush.monteran_objective" : "levels.forest.ambush.rechivii_objective");
            for(auto& s : m_soldiers)
                if(s.team == Team::MONTERAN && s.alive())
                {
                    s.order = Order::ADVANCE;
                    s.path.clear();
                    s.cover_index = -1;
                }
            for(auto& sec : m_sections)
                if(sec.team == Team::MONTERAN)
                {
                    sec.at = 0;
                    if(!(player_commands_section() && sec.id == p.section) && !player_leads())
                        order_section(sec.id, Order::ADVANCE, m_map.waypoints[0] + sf::Vector2f{sec.band, 0.0f});
                }
            m_waypoint = 0;
            m_waypoint_time = 4.0f;
            if(p.team == Team::MONTERAN)
            {
                if(ai_lt) say(*lt, tr("levels.forest.ambush.monteran_lt"), 3.5f);
                else say(tr("speakers.orban"), tr("levels.forest.ambush.orban"), Team::MONTERAN, 3.5f);
            }
            else say(tr("speakers.rch_lieutenant"), tr("levels.forest.ambush.rechivii_lt"), Team::RECHIVII, 3.0f);
            m_combat_heat = 1.0f;
            break;
        }
        case Stage::FOREST_PUSH:
            m_objective = tr(p.team == Team::MONTERAN ? "levels.forest.push.monteran_objective" : (p.raider ? "levels.forest.push.raider_objective" : "levels.forest.push.rechivii_objective"));
            all_rechivii([](Soldier& s) { if(s.reserve) { s.active = true; s.alerted = true; } });
            SoundBank::get().play_at("whistle", sf::Vector2f{1520.0f, 200.0f}, 0.8f);
            if(p.team == Team::MONTERAN)
            {
                Soldier* any = nullptr;
                for(auto& s : m_soldiers) if(s.team == Team::MONTERAN && s.fighting() && !s.is_player) any = &s;
                if(any) say(*any, tr("levels.forest.push.monteran_seen"));
            }
            else say(tr("speakers.rch_lieutenant"), tr("levels.forest.push.rechivii_lt"), Team::RECHIVII, 3.0f);
            break;

        // ======================================================== both
        case Stage::COUNTER:
        {
            m_objective = tr(std::string("levels.") + level_key(m_level) + ".counter." + side_key(p.team) + "_objective");
            if(m_level == LevelId::TOWN)
            {
                if(p.team == Team::RECHIVII) say(tr("speakers.rch_captain"), tr("levels.town.counter.rechivii_captain"), Team::RECHIVII, 4.5f);
            }
            all_rechivii([](Soldier& s) { if(s.reserve && !s.has_post) s.active = true; });
            // Everyone into cover around the objective, facing the way they'll come.
            if(!player_leads())
                for(auto& sec : m_sections)
                    if(sec.team == Team::MONTERAN)
                        order_section(sec.id, Order::ADVANCE, objective_point() + sf::Vector2f{-10.0f, sec.band * 0.35f});
            Soldier* any = nullptr;
            for(auto& s : m_soldiers) if(s.team == Team::MONTERAN && s.alive() && !s.is_player) any = &s;
            if(any && any->team == Team::MONTERAN && p.team == Team::MONTERAN)
                say(*any, tr(m_level == LevelId::TOWN ? "levels.town.counter.monteran_seen" : "levels.forest.counter.monteran_seen"));
            m_combat_heat = 1.0f;
            break;
        }
        case Stage::WON:
        {
            m_objective = "";
            Campaign::current().won = true;
            std::string const at = std::string("levels.") + level_key(m_level) + ".won.";
            if(p.team == Team::MONTERAN) say(lt && lt->alive() ? short_name(*lt) : "", tr(at + "monteran"), Team::MONTERAN, 5.5f);
            else say("", tr(at + "rechivii"), Team::RECHIVII, 5.0f);
            if(m_level == LevelId::TOWN) SoundBank::get().play("church_bell", 0.5f, 0.0f);
            else SoundBank::get().play_at("whistle", p.pos, 0.6f);
            m_end_timer = 0.0f;
            break;
        }
        case Stage::LOST:
            m_objective = "";
            say("", tr(std::string("levels.") + level_key(m_level) + ".lost." + side_key(p.team)), p.team, 6.0f);
            m_end_timer = 0.0f;
            break;
    }
}

void Battle::update_script(float dt)
{
    Soldier& p = *player();

    if(m_level == LevelId::TOWN) script_town(dt);
    else if(m_level == LevelId::FOREST) script_forest(dt);
    else script_charge(dt);

    // Victory is each place's own business; defeat, below, is the same everywhere.
    bool const fighting = fighting_started();
    if(m_stage < Stage::WON)
    {
        // Your side gone: on the charge, as a Rechivii, that's every man in it.
        bool anyone = false;
        bool const steady = m_level == LevelId::TOWN && p.team == Team::MONTERAN;      // in Kalvo, a man only gives up when they all do
        for(const auto& s : m_soldiers) if(s.team == p.team && (s.alive() || s.life == Life::DOWNED) && (s.morale > 0.0f || s.is_player || steady)) anyone = true;
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
                if(s.team != Team::RECHIVII || !s.fighting() || !s.active || s.is_player) continue;
                float nearest = 1e9f;
                for(const auto& o : m_soldiers)
                    if(o.team == Team::MONTERAN && o.fighting()) nearest = std::min(nearest, length(o.pos - s.pos));

                if(nearest < 45.0f && (s.morale < 0.45f || s.suppression > 0.8f) && Random::chance(0.3f))
                {
                    surrender(s);
                    continue;
                }
                if(m_level != LevelId::CHARGE && no_officer && lost * 2 >= total && Random::chance(0.18f + 0.4f * s.suppression))
                {
                    s.morale = 0.0f;
                    bark(s, tr("barks.rechivii.flee"), true);
                    continue;
                }
            }
        }
    }

    // Losing the lieutenant: the senior corporal takes the platoon. You keep
    // what you had, which is your own section if you lead it, and nothing more.
    if(!m_officer_dead_noted && fighting && !officer(Team::MONTERAN) && p.team == Team::MONTERAN && p.role != Role::OFFICER)
    {
        m_officer_dead_noted = true;
        if(Soldier* cpl = acting_commander()) say("", tr("lieutenant_down.has_platoon", {{"name", short_name(*cpl)}}), Team::MONTERAN, 4.5f);
        else say("", tr("lieutenant_down.down"), Team::MONTERAN, 4.0f);
    }

    // The section yours to lead, the moment it is: tell the player once.
    if(player_commands_section() && !m_section_hint_given)
    {
        m_section_hint_given = true;
        m_selected = p.section;
        notice(tr("hud.notice.you_lead", {{"section", m_sections[static_cast<std::size_t>(p.section)].name}}), 3.0f);
        hint(tr("hints.section_orders"), 12.0f);
    }

    // Your man has died: someone else picks up the fight.
    if(p.life == Life::DEAD || p.life == Life::FLED)
    {
        m_dead_wait += dt;
        if(m_dead_wait > 2.0f) { m_dead_wait = 0.0f; take_over(); }
    }
}

bool Battle::you_there(const sf::FloatRect& area, float reach) const
{
    // The objective doesn't fall without you: you, or your man if he is
    // driving himself for a recording.
    if(m_autoplay) return true;
    const Soldier& p = me();
    if(!p.alive() || p.team != Team::MONTERAN) return true;
    float const dx = std::max({area.position.x - p.pos.x, 0.0f, p.pos.x - (area.position.x + area.size.x)});
    float const dy = std::max({area.position.y - p.pos.y, 0.0f, p.pos.y - (area.position.y + area.size.y)});
    return std::hypot(dx, dy) <= reach;
}

void Battle::raise_alarm(const std::string& why)
{
    // The forest: the ambush is sprung, by the signal or because someone was seen.
    if(m_alarm) return;
    m_alarm = true;
    m_alarm_at = m_time;
    for(auto& s : m_soldiers)
    {
        if(s.reserve) continue;
        s.active = true;
        s.alerted = true;
        s.sitting = false;
        // The ambushers are ready for it; the column has to take it in first.
        s.reaction = s.team == Team::RECHIVII ? Random::real(0.1f, 0.6f) : Random::real(0.8f, 2.4f);
        s.path.clear();
        // Most of the escort throw themselves flat at the first shots; the rest stand there a moment.
        if(s.team == Team::MONTERAN && !s.is_player && s.alive() && Random::chance(0.7f))
        {
            s.stance = Stance::PRONE;
            s.body_angle = s.aim;
            s.stance_change = 1.5f;
        }
    }
    if(!why.empty())
    {
        notice(why, 3.0f);
    }
    m_combat_heat = 1.0f;
    SoundBank::get().punch_in();
}

void Battle::take_over()
{
    Soldier& old = *player();
    Soldier* best = nullptr;
    float best_d = 1e9f;
    for(auto& s : m_soldiers)
    {
        if(s.team != old.team || !s.alive() || s.id == old.id || s.morale <= 0.0f) continue;
        // Someone from your own section first; the lieutenant only if there's nobody else.
        float const d = length(s.pos - old.pos) + (s.section != old.section ? 1.0e5f : 0.0f) + (s.role == Role::OFFICER ? 1.0e6f : 0.0f);
        if(d < best_d) { best_d = d; best = &s; }
    }
    if(!best) return;

    old.is_player = false;
    best->is_player = true;
    best->path.clear();
    m_player = best->id;
    m_takeover = 4.0f;
    m_takeover_text = tr("hud.you_are", {{"rank", rank_name(best->rank)}, {"name", best->name}, {"role", role_name(best->role)}});
    m_trigger = m_aiming = false;
    m_camera.set_transition(TransitionFunction::EaseInOutQuad);
}

void Battle::finish()
{
    // What the debrief shows: the losses on both sides, and how long it took.
    Campaign& c = Campaign::current();
    Team const side = me().team;
    c.ours = c.theirs = Losses{};
    c.wagons_lost = m_level == LevelId::FOREST ? wagons_lost() : 0;
    for(const auto& s : m_soldiers)
    {
        // A reserve that never came into it isn't counted.
        if(!s.active && s.alive()) continue;
        Losses& l = s.team == side ? c.ours : c.theirs;
        ++l.total;
        if(s.surrendered) ++l.captured;
        else if(s.life == Life::DEAD) ++l.killed;
        // Ambushers who did what they came for and went back into the trees didn't run.
        else if(s.life == Life::FLED && !(m_level == LevelId::FOREST && m_forest_outcome == 2 && s.team == Team::RECHIVII)) ++l.fled;
        else if(s.life == Life::DOWNED || s.lost || s.hp < 60.0f) ++l.wounded;
    }
}

}
