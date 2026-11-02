/**
 * Forest.cpp  -  Mud Cats
 *
 * The Lesna road, at midday. Six wagons of shells and rations going up to
 * the front, with B Platoon walking beside them: a section out ahead, the
 * lieutenant and a section with the wagons, a section at the back.
 *
 * Thirty Rechivii lie in the bracken north of the road where the trees
 * come closest, a machine gun among them and another at the far end,
 * behind a pine they've felled across the track. A mine under the road
 * at the head of the stretch is the signal. Unless somebody in the
 * column sees them first, or somebody fires early.
 *
 * Once it's sprung the wagons stop where they are and the escort gets
 * down behind them. A little later an assault group comes out of the
 * trees at the wagons with grenades, satchels and a flamethrower. Burn
 * four of the six and the ambush has done what it came for; lose too
 * many men before that, and it breaks and goes back into the forest. The
 * escort wins by beating it off with at least three wagons left to go on
 * with.
 *
 * Played from either side. As a Monteran you walk with the wagons in
 * 2 Section. As a Rechivii you lie in the ambush line, or, as a
 * stormtrooper, sapper or flamethrower, with the assault group behind it.
 */

#include "Battle.hpp"

#include "Draw.hpp"
#include "Palette.hpp"
#include "Sound.hpp"

#include "../../include/core/Random.hpp"

#include <algorithm>
#include <cmath>

namespace mudcats
{

namespace
{
    float length(sf::Vector2f v) { return std::sqrt(v.x * v.x + v.y * v.y); }
    sf::Vector2f unit(sf::Vector2f v) { float const l = length(v); return l > 1e-5f ? v / l : sf::Vector2f{1.0f, 0.0f}; }

    constexpr float PI = 3.14159f;
    constexpr int   WAGONS = 6;
    constexpr float WAGON_GAP = 58.0f;
    constexpr float COLUMN_START = 900.0f;      // the lead wagon, at the start
    constexpr float MARCH_SPEED = 24.0f;
    constexpr float MINE_AT = 1640.0f;          // along the road: where the lead wagon finds it
    constexpr float PUSH_AFTER = 25.0f;         // seconds of fire before the assault group goes in
    constexpr float BREAK_OFF_AFTER = 330.0f;   // they can't stay all day: our guns will find them
    constexpr int   WAGONS_TO_WIN = 4;
}

// ---------------------------------------------------------------- the two sides

void Battle::spawn_forest()
{
    m_soldiers.reserve(MAX_SOLDIERS);
    Campaign const& c = Campaign::current();
    m_column = COLUMN_START;

    // --- the wagons ----------------------------------------------------------------------------
    m_wagons.clear();
    for(int k = 0; k < WAGONS; ++k)
    {
        Wagon w;
        w.along = m_column - WAGON_GAP * static_cast<float>(k);
        w.pos = m_map.road_at(w.along);
        w.dir = m_map.road_dir(w.along);
        m_wagons.push_back(w);
    }

    // --- the escort: a section ahead, one with the wagons, one behind ---------------------------
    int const one   = add_section(Team::MONTERAN, item("sections.monteran", 0), 170.0f);
    int const two   = add_section(Team::MONTERAN, item("sections.monteran", 1), 0.0f);
    int const three = add_section(Team::MONTERAN, item("sections.monteran", 2), -170.0f);
    auto place = [&](Soldier& s, float at, float side)
    {
        s.march_at = at;
        s.march_side = side;
        sf::Vector2f const d = m_map.road_dir(m_column + at);
        s.pos = s.goal = m_map.road_at(m_column + at) + sf::Vector2f{-d.y, d.x} * side;
        s.aim = std::atan2(d.y, d.x);
        s.home = {6.0f, std::clamp(s.pos.y, 300.0f, 700.0f)};
    };
    auto make_section = [&](int id, float from, bool medic, bool gun)
    {
        for(int i = 0; i < 8; ++i)
        {
            Role role = i == 1 && gun ? Role::GUNNER : (medic && i == 2 ? Role::MEDIC : Role::RIFLEMAN);
            if(i == 3) role = medic ? Role::SAPPER : Role::STORMTROOPER;
            Soldier& s = add(Team::MONTERAN, role, i == 0 ? Rank::CORPORAL : Rank::PRIVATE, next_name(Team::MONTERAN), {}, id);
            s.leader = i == 0;
            place(s, from - 17.0f * static_cast<float>(i), i % 2 ? 15.0f : -15.0f);
        }
    };
    make_section(one, 150.0f, false, true);
    // Your own section: its men are named in the text file, in this order.
    struct M { Role role; Rank rank; };
    const M yours[] = {
        {Role::RIFLEMAN, Rank::CORPORAL}, {Role::RIFLEMAN, Rank::PRIVATE},
        {Role::GUNNER, Rank::CORPORAL},   {Role::FLAMER, Rank::PRIVATE},
        {Role::MEDIC, Rank::PRIVATE},     {Role::RADIOMAN, Rank::PRIVATE},
        {Role::STORMTROOPER, Rank::PRIVATE}, {Role::RIFLEMAN, Rank::PRIVATE}};
    for(int i = 0; i < 8; ++i)
    {
        Soldier& s = add(Team::MONTERAN, yours[i].role, yours[i].rank, item("names.monteran.your_section", static_cast<std::size_t>(i)), {}, two);
        s.leader = i == 0;
        // Spread down the wagons, one each side.
        place(s, -10.0f - 34.0f * static_cast<float>(i), i % 2 ? 17.0f : -17.0f);
    }
    Soldier& lt = add(Team::MONTERAN, Role::OFFICER, Rank::LIEUTENANT, tr("names.monteran.lieutenant"), {}, -1);
    place(lt, -130.0f, -15.0f);
    make_section(three, -WAGON_GAP * WAGONS - 30.0f, true, true);

    // --- the ambush -----------------------------------------------------------------------------
    auto their_name = [&]() { return next_name(Team::RECHIVII); };
    int const line  = add_section(Team::RECHIVII, tr("sections.forest.line"), 0.0f);
    int const block = add_section(Team::RECHIVII, tr("sections.forest.block"), 0.0f);
    int const assault = add_section(Team::RECHIVII, tr("sections.forest.assault"), 0.0f);
    auto spot_near = [this](sf::Vector2f p)
    {
        // The nearest place a man can lie: not in a trunk, not in a log.
        for(float r = 0.0f; r < 40.0f; r += 4.0f)
            for(int k = 0; k < 8; ++k)
            {
                sf::Vector2f const q = p + sf::Vector2f{std::cos(static_cast<float>(k) * 0.785f), std::sin(static_cast<float>(k) * 0.785f)} * r;
                if(m_map.walkable(q)) return q;
            }
        return p;
    };
    auto enemy = [&](Role role, sf::Vector2f at, int section, bool active, Rank rank = Rank::PRIVATE) -> Soldier&
    {
        at = spot_near(at);
        Soldier& s = add(Team::RECHIVII, role, role == Role::OFFICER && rank == Rank::PRIVATE ? Rank::LIEUTENANT : rank, their_name(), at, section);
        s.post = at;
        s.has_post = true;
        s.active = active;
        s.alerted = true;
        s.stance = Stance::PRONE;
        s.body_angle = PI * 0.5f;
        s.aim = PI * 0.5f;
        s.home = {at.x, 6.0f};
        return s;
    };
    // Their lieutenant first, so a Rechivii officer is him.
    enemy(Role::OFFICER, m_map.road_at(1560.0f) + sf::Vector2f{0.0f, -150.0f}, -1, true);
    // The line, in the bracken along the north side.
    for(int i = 0; i < 12; ++i)
    {
        if(i == 6) continue;                    // the gun's place
        float const x = 1260.0f + 44.0f * static_cast<float>(i);
        float const y = m_map.road_at(x).y - 120.0f - (i % 3 == 1 ? 14.0f : 0.0f);
        Role const role = i == 4 ? Role::SNIPER : (i == 8 ? Role::MEDIC : Role::RIFLEMAN);
        enemy(role, {x, y}, line, true).leader = i == 0;
    }
    Soldier& mg = enemy(Role::GUNNER, m_map.road_at(1520.0f) + sf::Vector2f{0.0f, -116.0f}, line, true);
    mg.deploy_dir = PI * 0.5f;
    mg.stance = Stance::CROUCH;
    // The block: a few riflemen behind the felled pine, for anyone who tries to drive on through.
    sf::Vector2f const pine = m_map.road_at(1880.0f);
    for(sf::Vector2f o : {sf::Vector2f{30.0f, -10.0f}, {28.0f, -48.0f}, {40.0f, 40.0f}, {70.0f, -20.0f}})
        enemy(Role::RIFLEMAN, pine + o, block, true).aim = PI;
    m_soldiers.back().leader = true;
    // The assault group, further back in the trees, waiting for the word.
    const Role group[] = {Role::STORMTROOPER, Role::SAPPER, Role::FLAMER, Role::STORMTROOPER, Role::SAPPER, Role::RIFLEMAN, Role::RIFLEMAN, Role::RIFLEMAN};
    for(int i = 0; i < 8; ++i)
    {
        Soldier& s = enemy(group[i], {1450.0f + 20.0f * static_cast<float>(i), 200.0f + (i % 2) * 22.0f}, assault, false);
        s.leader = i == 0;
        s.reserve = true;
        s.has_post = false;
        s.raider = group[i] != Role::RIFLEMAN || i == 5;
        s.stance = Stance::CROUCH;
    }

    // --- you ------------------------------------------------------------------------------------
    if(c.playing == Team::RECHIVII)
    {
        bool const raider = c.chosen == Role::STORMTROOPER || c.chosen == Role::SAPPER || c.chosen == Role::FLAMER;
        int const sec = raider ? assault : line;
        fill_roles(sec);
        pick_player(sec);
        Soldier& p = *player();
        p.active = true;
        p.raider = raider;
    }
    else
    {
        fill_roles(two);
        pick_player(two);
    }
    m_forest_rch = m_forest_mon = 0;
    for(const auto& s : m_soldiers) (s.team == Team::RECHIVII ? m_forest_rch : m_forest_mon) += 1;
    m_forest_outcome = 0;
    m_forest_check = 0.0f;
}

void Battle::forest_shortcut(int stage)
{
    // 1: the column a few seconds short of the mine. 2: right on it.
    if(stage < 1) return;
    m_column = stage >= 2 ? MINE_AT - 2.0f : MINE_AT - 70.0f;
    for(int k = 0; k < static_cast<int>(m_wagons.size()); ++k)
    {
        Wagon& w = m_wagons[static_cast<std::size_t>(k)];
        w.along = m_column - WAGON_GAP * static_cast<float>(k);
        w.pos = m_map.road_at(w.along);
        w.dir = m_map.road_dir(w.along);
    }
    for(auto& s : m_soldiers)
    {
        if(s.team != Team::MONTERAN) continue;
        sf::Vector2f const d = m_map.road_dir(m_column + s.march_at);
        s.pos = s.goal = m_map.road_at(m_column + s.march_at) + sf::Vector2f{-d.y, d.x} * s.march_side;
        s.path.clear();
    }
}

// ---------------------------------------------------------------- the wagons

int Battle::wagons_lost() const
{
    int n = 0;
    for(const auto& w : m_wagons) if(w.destroyed) ++n;
    return n;
}

void Battle::ignite_wagon(Wagon& w)
{
    if(w.destroyed || w.burning > 0.0f) return;
    w.burning = 0.01f;
    SoundBank::get().play_at("flamer_start", w.pos, 0.8f);
    if(me().team == Team::MONTERAN) notice(tr("levels.forest.wagons.burning"), 2.5f);
}

void Battle::damage_wagons(sf::Vector2f at, float power, bool fire)
{
    for(auto& w : m_wagons)
    {
        if(w.destroyed) continue;
        float const d = length(w.pos - at);
        if(d > 34.0f) continue;
        w.hp -= 75.0f * power * (1.0f - d / 34.0f);
        if(fire || power > 1.6f || Random::chance(0.35f * power)) ignite_wagon(w);
        if(w.hp <= 0.0f && w.burning <= 0.0f) ignite_wagon(w);
    }
}

void Battle::halt_column()
{
    // Where they stand is where they stay: from now on each wagon is something to get behind.
    for(auto& w : m_wagons)
    {
        if(w.halted) continue;
        w.halted = true;
        sf::Vector2f const c = w.pos - w.dir * 3.0f;
        float const hx = std::abs(w.dir.x) * 12.0f + std::abs(w.dir.y) * 6.0f;
        float const hy = std::abs(w.dir.y) * 12.0f + std::abs(w.dir.x) * 6.0f;
        m_map.add_low({{c.x - hx, c.y - hy}, {hx * 2.0f, hy * 2.0f}});
        w.horses_down = m_time + Random::real(0.5f, 4.0f);
    }
}

void Battle::update_wagons(float dt)
{
    for(auto& w : m_wagons)
    {
        if(!w.halted)
        {
            w.pos = m_map.road_at(w.along);
            w.dir = m_map.road_dir(w.along);
        }
        if(w.horses_down > 0.0f && m_time > w.horses_down && w.horses_down < 1e8f)
        {
            w.horses_down = 1e9f;                       // down for good
            SoundBank::get().play_at("body_fall", w.pos + w.dir * 14.0f, 0.7f);
        }
        if(w.burning <= 0.0f) continue;
        // Burning: the load goes up bit by bit, and when the shells in it go, they go.
        w.burning += dt;
        w.hp -= dt * 7.0f;
        if(Random::chance(dt * 3.0f))
        {
            bool found = false;
            for(auto& f : m_fires) if(length(f.pos - w.pos) < 6.0f) { f.life = std::max(f.life, 2.0f); found = true; }
            if(!found) m_fires.push_back({w.pos - w.dir * 3.0f, 5.0f, 3.0f, 0.0f});
        }
        if(!w.destroyed && w.hp <= 0.0f)
        {
            w.destroyed = true;
            explode(w.pos, 1.0f, -1, Team::RECHIVII, false);
            m_camera.shake(0.25f);
            int const lost = wagons_lost();
            if(me().team == Team::MONTERAN) notice(tr(lost == 1 ? "levels.forest.wagons.lost_one" : "levels.forest.wagons.lost_many", {{"n", std::to_string(lost)}}), 3.0f);
            else notice(tr("levels.forest.wagons.of_needed", {{"n", std::to_string(lost)}, {"needed", std::to_string(WAGONS_TO_WIN)}}), 3.0f);
        }
        if(w.burning > 40.0f) w.burning = 40.0f;        // a smouldering wreck by now
    }
}

void Battle::draw_wagons(sf::VertexArray& va) const
{
    for(const auto& w : m_wagons)
    {
        sf::Vector2f const d = w.dir, n{-d.y, d.x};
        float const a = std::atan2(d.y, d.x);
        bool const down = w.horses_down > 1e8f;
        float const bob = w.halted ? 0.0f : std::sin(m_time * 6.0f + w.along) * 0.6f;
        // The horses, two abreast in front: on their feet, or down in the shafts.
        for(float side : {-4.0f, 4.0f})
        {
            sf::Vector2f const h = w.pos + d * 15.0f + n * side;
            sf::Color const coat = side < 0.0f ? sf::Color(92, 64, 44) : sf::Color(72, 52, 38);
            if(down)
            {
                draw::rotated_rect(va, h + n * (side * 0.8f), 11.0f, 6.0f, a + 1.2f * (side < 0 ? -1.0f : 1.0f), lerp(coat, INK, 0.2f));
                draw::disc(va, h.x + n.x * side * 1.4f, h.y + n.y * side * 1.4f, 2.0f, BLOOD_DK);
                continue;
            }
            draw::rotated_rect(va, h + sf::Vector2f{1.0f, 1.0f}, 11.0f, 5.0f, a, SHADOW);
            draw::rotated_rect(va, h + d * bob, 11.0f, 5.0f, a, coat);
            draw::rotated_rect(va, h + d * (7.0f + bob), 4.0f, 3.0f, a, lerp(coat, INK, 0.25f));     // head
        }
        // The shafts and the wagon: a plank bed, wheels at the corners, the load under canvas.
        sf::Vector2f const c = w.pos - d * 3.0f;
        draw::thick_line(va, c + d * 10.0f, w.pos + d * 10.0f, 1.0f, PLANK_DARK);
        draw::rotated_rect(va, c + sf::Vector2f{2.0f, 2.0f}, 24.0f, 13.0f, a, SHADOW);
        for(float along : {-8.0f, 7.0f})
            for(float side : {-6.5f, 6.5f})
                draw::rotated_rect(va, c + d * along + n * side, 6.0f, 2.0f, a, w.destroyed ? CHAR : lerp(PLANK_DARK, INK, 0.3f));
        sf::Color const bed = w.destroyed ? CHAR : PLANK;
        sf::Color const load = w.destroyed ? lerp(CHAR, MUD_DARK, 0.4f) : (w.burning > 0.0f ? lerp(SANDBAG, CHAR, std::min(1.0f, w.burning / 20.0f)) : lerp(SANDBAG, PAPER_DK, 0.25f));
        draw::rotated_rect(va, c, 22.0f, 11.0f, a, bed);
        draw::rotated_rect(va, c - d * 1.0f, 17.0f, 9.0f, a, load);
        if(!w.destroyed)
            for(float k : {-5.0f, 0.0f, 5.0f})
                draw::rotated_rect(va, c + d * k, 1.0f, 9.0f, a, lerp(load, INK, 0.2f));         // the canvas's ribs
    }
}

// ---------------------------------------------------------------- the assault group

bool Battle::raider_think(Soldier& s)
{
    // Go in at the nearest wagon that isn't already burning, and set it alight:
    // a flamethrower from close, a satchel against it, grenades from a little further.
    Wagon* best = nullptr;
    float best_d = 1e9f;
    for(auto& w : m_wagons)
    {
        if(w.destroyed || w.burning > 0.0f) continue;
        float const d = length(w.pos - s.pos);
        if(d < best_d) { best_d = d; best = &w; }
    }
    if(!best) return false;
    Wagon& w = *best;
    sf::Vector2f const side = w.pos + sf::Vector2f{0.0f, -16.0f};
    if(s.has_flamer() && best_d < 34.0f && s.fuel > 5.0f)
    {
        // A long burst into the load.
        sf::Vector2f const to = unit(w.pos - s.pos);
        for(int k = 0; k < 6; ++k)
            m_flames.push_back({s.pos + to * 4.0f, (to + sf::Vector2f{Random::real(-0.15f, 0.15f), Random::real(-0.15f, 0.15f)}) * 160.0f, 0.4f, 0.4f, s.id, s.team});
        s.fuel = std::max(0.0f, s.fuel - 6.0f);
        SoundBank::get().play_at("flamer_start", s.pos, 0.7f);
        s.aim = std::atan2(to.y, to.x);
        return true;
    }
    if(s.satchels > 0 && best_d < 22.0f)
    {
        place_satchel(s, w.pos);
        s.goal = s.pos + unit(s.pos - w.pos) * 50.0f;
        s.path.clear();
        bark(s, tr("barks.rechivii.fire"), true);
        return true;
    }
    if(s.grenades > 0 && best_d < 90.0f && best_d > 30.0f && Random::chance(0.25f))
    {
        throw_grenade(s, w.pos + sf::Vector2f{Random::real(-6, 6), Random::real(-6, 6)});
        return true;
    }
    if(length(s.goal - side) > 10.0f)
    {
        s.goal = side;
        s.path.clear();
        s.cover_index = -1;
        if(Random::chance(0.2f)) bark(s, tr("barks.rechivii.to_the_wagons"), true);
    }
    return true;
}

// ---------------------------------------------------------------- where things are

sf::Vector2f Battle::forest_objective() const
{
    if(m_stage == Stage::FOREST_MARCH)
        return me().team == Team::MONTERAN ? m_map.road_at(m_column + 40.0f) : m_map.road_at(MINE_AT);
    if(me().team == Team::RECHIVII)
    {
        // The nearest wagon still whole.
        sf::Vector2f best = m_map.road_area.getCenter();
        float best_d = 1e9f;
        for(const auto& w : m_wagons)
            if(!w.destroyed && length(w.pos - me().pos) < best_d) { best_d = length(w.pos - me().pos); best = w.pos; }
        return best;
    }
    return m_map.waypoints[static_cast<std::size_t>(std::clamp(forest_limit(), 0, static_cast<int>(m_map.waypoints.size()) - 1))];
}

int Battle::forest_limit() const
{
    // Down behind the wagons first; then up into the trees at them; then through.
    if(m_stage == Stage::FOREST_AMBUSH) return m_stage_time < 40.0f ? 0 : 1;
    if(m_stage == Stage::FOREST_PUSH)
    {
        int left = 0;
        for(const auto& s : m_soldiers) if(s.team == Team::RECHIVII && s.fighting() && s.active) ++left;
        return left * 2 < m_forest_rch ? 3 : 2;
    }
    return 0;
}

sf::Vector2f Battle::forest_forward() const
{
    return {0.0f, -160.0f};
}

// ---------------------------------------------------------------- the script

void Battle::script_forest(float dt)
{
    Soldier& p = *player();
    bool const theirs = p.team == Team::RECHIVII;
    update_wagons(dt);

    if(m_stage == Stage::FOREST_MARCH)
    {
        // The column walks on, everyone keeping his place by the wagons.
        m_column += MARCH_SPEED * dt;
        for(int k = 0; k < static_cast<int>(m_wagons.size()); ++k)
            m_wagons[static_cast<std::size_t>(k)].along = m_column - WAGON_GAP * static_cast<float>(k);
        for(auto& s : m_soldiers)
        {
            if(s.team != Team::MONTERAN || (s.is_player && !m_autoplay) || !s.alive()) continue;
            sf::Vector2f const d = m_map.road_dir(m_column + s.march_at);
            s.goal = m_map.road_at(m_column + s.march_at) + sf::Vector2f{-d.y, d.x} * s.march_side;
            s.path = {s.goal};
            s.repath = 1.0f;
            s.order = Order::FOLLOW;
        }
        if(m_stage_time > 4.0f && m_stage_time - dt <= 4.0f && !theirs)
            say(tr("speakers.varga"), tr("levels.forest.march.varga"), Team::MONTERAN, 5.0f);
        if(m_stage_time > 14.0f && m_stage_time - dt <= 14.0f)
            say(theirs ? "Lt." : tr("speakers.dacre"), theirs ? tr("levels.forest.march.rechivii_wait") : tr("levels.forest.march.dacre"), theirs ? Team::RECHIVII : Team::MONTERAN, 4.0f);

        // Somebody walks right onto one of them.
        for(const auto& s : m_soldiers)
        {
            if(s.team != Team::MONTERAN || !s.alive()) continue;
            for(const auto& e : m_soldiers)
                if(e.team == Team::RECHIVII && e.fighting() && !e.reserve && length(e.pos - s.pos) < 42.0f)
                {
                    raise_alarm(s.is_player ? tr("alarm.walked_into") : tr("alarm.in_the_trees"));
                    break;
                }
            if(m_alarm) break;
        }
        // The lead wagon finds the mine.
        if(!m_alarm && !m_wagons.empty() && m_wagons.front().along >= MINE_AT)
        {
            Wagon& lead = m_wagons.front();
            explode(lead.pos, 1.6f, -1, Team::RECHIVII, false);
            lead.hp = 35.0f;            // it burns a little while before the shells in it go
            ignite_wagon(lead);
            raise_alarm("");
        }
        if(m_alarm) set_stage(Stage::FOREST_AMBUSH);
        return;
    }

    // --- sprung ---------------------------------------------------------------------------------
    if(m_stage == Stage::FOREST_AMBUSH)
    {
        // A little while to shoot the column up, then the assault group goes in.
        if(m_stage_time > PUSH_AFTER) set_stage(Stage::FOREST_PUSH);
    }

    if(m_forest_outcome == 0 && fighting_started())
    {
        m_forest_check -= dt;
        if(m_forest_check <= 0.0f)
        {
            m_forest_check = 1.0f;
            int rch = 0, mon = 0;
            for(const auto& s : m_soldiers)
            {
                if(!s.fighting()) continue;
                if(s.team == Team::RECHIVII && s.morale > 0.0f) ++rch;
                if(s.team == Team::MONTERAN) ++mon;
            }
            bool const no_officer = officer(Team::RECHIVII) == nullptr;
            float const rch_share = static_cast<float>(rch) / static_cast<float>(std::max(1, m_forest_rch));
            float const mon_share = static_cast<float>(mon) / static_cast<float>(std::max(1, m_forest_mon));
            float const since = m_time - m_alarm_at;
            int outcome = 0;
            if(wagons_lost() >= WAGONS_TO_WIN) outcome = 2;
            else if(mon_share <= 0.14f) outcome = 2;
            else if(rch_share <= 0.4f || (no_officer && rch_share <= 0.55f)) outcome = 1;
            else if(since > BREAK_OFF_AFTER) outcome = wagons_lost() >= 3 ? 2 : 1;
            if(outcome != 0)
            {
                m_forest_outcome = outcome;
                m_forest_outcome_at = m_time;
                // Either way the ambushers melt back into the trees: they never meant to stay.
                for(auto& s : m_soldiers)
                    if(s.team == Team::RECHIVII && s.fighting() && !s.is_player)
                    {
                        s.morale = 0.0f;
                        s.path.clear();
                        if(Random::chance(0.3f)) bark(s, outcome == 2 ? tr("barks.rechivii.done_back") : tr("barks.rechivii.flee"), true);
                    }
                SoundBank::get().play_at("whistle", m_map.road_at(1560.0f) + sf::Vector2f{0.0f, -180.0f}, 0.8f);
                notice(outcome == 1 ? (theirs ? tr("levels.forest.outcome.rechivii_beaten") : tr("levels.forest.outcome.monteran_won")) : (theirs ? tr("levels.forest.outcome.rechivii_done") : tr("levels.forest.outcome.monteran_lost")), 3.5f);
            }
        }
    }
    if(m_forest_outcome != 0 && m_stage < Stage::WON && m_time - m_forest_outcome_at > 6.0f)
    {
        bool const ours_won = (m_forest_outcome == 1) == !theirs;
        set_stage(ours_won ? Stage::WON : Stage::LOST);
    }
}

}
