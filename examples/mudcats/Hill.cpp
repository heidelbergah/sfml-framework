/**
 * Hill.cpp  -  Mud Cats
 *
 * Hill 312: sixteen of us on a hilltop, and the Rechivii coming up it in
 * waves until nobody is left. Also the two things the hill is built
 * around but that work anywhere: digging in, and their mortars.
 *
 * A wave is planned all at once and then arrives over half a minute, a
 * group at a time from each side it comes from. Each wave is bigger than
 * the last, and from the second on it brings things with it that hang
 * back: a machine gun to keep our heads down, a sniper, later mortars.
 * When the men in the rush are all down, whatever is left of the wave
 * goes back where it came from, and there is a lull of ten seconds: just
 * time to fill up at the dump and get back to the trench. Every third lull
 * a few new men come up the back slope.
 *
 * Slots: the soldier list has room for MAX_SOLDIERS and no more (bullets
 * keep a bit per man). The dead are drawn into the ground when they die,
 * so a new man can take a dead Rechivii's slot.
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
    sf::Vector2f dir(float a) { return {std::cos(a), std::sin(a)}; }

    const char* const THEIR_NAMES[] = {
        "Oslev", "Brannik", "Dvorek", "Halst", "Mirov", "Tesk", "Vadim", "Kosar", "Rudan", "Zelny",
        "Pavek", "Strom", "Ilko", "Gerd", "Yaro", "Lutz", "Borsk", "Kalen", "Matvo", "Resk",
        "Dravo", "Imre", "Sobek", "Varn", "Tolya", "Kresh", "Andrik", "Pellan", "Gosta", "Vey",
        "Hollin", "Radek", "Siro", "Mavik", "Darko", "Lesh", "Orvan", "Tibor", "Zavic", "Nemo",
        "Kuzma", "Bodan", "Elek", "Fedko", "Grisha", "Hrin", "Isko", "Jurek", "Kolya", "Lev"};

    // Directions they can come from, as angles from the crest.
    constexpr float EAST = 0.0f, NORTH_EAST = -0.75f, SOUTH_EAST = 0.75f, NORTH = -1.5708f, SOUTH = 1.5708f;

    const char* direction_name(float a)
    {
        if(std::abs(a - EAST) < 0.3f) return "east";
        if(std::abs(a - NORTH_EAST) < 0.3f) return "north-east";
        if(std::abs(a - SOUTH_EAST) < 0.3f) return "south-east";
        if(a < 0.0f) return "north";
        return "south";
    }
}

// ---------------------------------------------------------------- slots

Soldier* Battle::spawn_soldier(Team team, Role role, std::string rank, std::string name, sf::Vector2f pos, int section)
{
    // A dead or departed Rechivii's slot, if there is one: he's in the ground already.
    int slot = -1;
    for(const auto& s : m_soldiers)
        if(s.team == Team::RECHIVII && !s.is_player && (s.life == Life::DEAD || s.life == Life::FLED)) { slot = s.id; break; }
    if(slot < 0 && m_soldiers.size() >= MAX_SOLDIERS) return nullptr;

    Soldier s;
    s.id      = slot >= 0 ? slot : static_cast<int>(m_soldiers.size());
    s.team    = team;
    s.role    = role;
    s.rank    = std::move(rank);
    s.name    = std::move(name);
    s.pos     = pos;
    s.goal    = pos;
    s.section = section;
    s.aim     = team == Team::MONTERAN ? 0.0f : 3.14159f;
    kit_out(s);
    s.think   = Random::real(0.0f, 0.3f);
    s.morale  = Random::real(0.8f, 1.0f);
    s.order   = Order::HOLD;

    if(slot < 0)
    {
        m_soldiers.push_back(s);
        return &m_soldiers.back();
    }

    // Tally the man whose place this was, and make sure nobody still
    // means him.
    Soldier& old = m_soldiers[static_cast<std::size_t>(slot)];
    if(old.life == Life::FLED && !old.surrendered) ++m_hill_fled;
    else if(old.life == Life::DEAD && old.active) ++m_hill_dead;
    for(auto& o : m_soldiers)
    {
        if(o.target == slot) o.target = -1;
        if(o.treating == slot) o.treating = -1;
        if(o.dragging == slot) o.dragging = -1;
        if(o.dragged_by == slot) o.dragged_by = -1;
    }
    for(auto& m : m_mortars)
    {
        if(m.gunner == slot) m.gunner = -1;
        if(m.loader == slot) m.loader = -1;
    }
    for(auto& g : m_grenades) if(g.owner == slot) g.owner = -1;
    for(auto& b : m_bullets) { if(b.owner == slot) b.owner = -1; b.passed.reset(static_cast<std::size_t>(slot)); }
    old = s;
    return &old;
}

// ---------------------------------------------------------------- the defenders

void Battle::spawn_hill()
{
    m_soldiers.reserve(MAX_SOLDIERS);
    sf::Vector2f const c = m_map.crest;

    int const one = add_section(Team::MONTERAN, "1 Section", 0.0f);
    int const two = add_section(Team::MONTERAN, "2 Section", 0.0f);
    add(Team::MONTERAN, Role::OFFICER, "Lt.", "Aurel Varga", c + sf::Vector2f{-42.0f, -4.0f}, -1);

    struct M { Role role; const char* rank; const char* name; };
    const M first[] = {
        {Role::RIFLEMAN, "Cpl.", "Petru Orban"}, {Role::RIFLEMAN, "Pvt.", "Emil Rell"}, {Role::GUNNER, "Cpl.", "Anselm Kray"},
        {Role::FLAMER, "Pvt.", "Dorian Vosk"},   {Role::MEDIC, "Pvt.", "Teodor Halm"},  {Role::RADIOMAN, "Pvt.", "Tomas Dacre"},
        {Role::STORMTROOPER, "Pvt.", "Lio Marchetti"}, {Role::RIFLEMAN, "Pvt.", "Benedek Sorn"}};
    const M second[] = {
        {Role::RIFLEMAN, "Cpl.", "Ilie Brandt"}, {Role::GUNNER, "Pvt.", "Oren Mazur"}, {Role::SNIPER, "Pvt.", "Paolo Serra"},
        {Role::RIFLEMAN, "Pvt.", "Viktor Ames"}, {Role::STORMTROOPER, "Pvt.", "Luca Fenn"}, {Role::SAPPER, "Pvt.", "Radu Pell"},
        {Role::RIFLEMAN, "Pvt.", "Ion Carver"}};
    for(int i = 0; i < 8; ++i) add(Team::MONTERAN, first[i].role, first[i].rank, first[i].name, c, one).leader = i == 0;
    for(int i = 0; i < 7; ++i) add(Team::MONTERAN, second[i].role, second[i].rank, second[i].name, c, two).leader = i == 0;
    fill_roles(one);
    pick_player(one);

    hill_positions(EAST, EAST);
    for(auto& s : m_soldiers)
    {
        s.pos = s.goal;
        s.aim = std::atan2(s.pos.y - c.y, s.pos.x - c.x);
        s.stance = Stance::CROUCH;
    }
    // You start at the dump, where the lieutenant can talk to you.
    Soldier& p = m_soldiers[static_cast<std::size_t>(m_player)];
    if(p.role != Role::OFFICER) p.pos = p.goal = c + sf::Vector2f{-6.0f, 14.0f};
    m_last_threat = EAST;
}

void Battle::hill_positions(float facing, float second)
{
    // Both sections along the ring, the side they're coming from; the
    // guns in the two pits that face them best; the medic and the radio
    // by the dump, in the middle.
    sf::Vector2f const c = m_map.crest;
    auto ring_spot = [this, c](float a)
    {
        sf::Vector2f const want = c + sf::Vector2f{std::cos(a) * 143.0f, std::sin(a) * 132.0f};
        sf::Vector2f best = want;
        float best_d = 40.0f;
        for(const auto& cp : m_map.cover)
        {
            if(!cp.trench) continue;
            float const d = length(cp.pos - want);
            if(d >= best_d) continue;
            bool taken = false;
            for(const auto& o : m_soldiers) if(o.team == Team::MONTERAN && o.alive() && o.has_post && length(o.post - cp.pos) < 7.0f) taken = true;
            if(taken) continue;
            best_d = d;
            best = cp.pos;
        }
        return best;
    };

    const sf::Vector2f pits[] = {c + sf::Vector2f{166, -46}, c + sf::Vector2f{166, 50}, c + sf::Vector2f{-6, -156}, c + sf::Vector2f{0, 158}};
    bool pit_used[4] = {false, false, false, false};
    auto best_pit = [&](float toward)
    {
        int best = -1;
        float score = -2.0f;
        for(int i = 0; i < 4; ++i)
        {
            if(pit_used[i]) continue;
            float const k = std::cos(std::atan2(pits[i].y - c.y, pits[i].x - c.x) - toward);
            if(k > score) { score = k; best = i; }
        }
        if(best >= 0) pit_used[best] = true;
        return best;
    };

    for(auto& s : m_soldiers) if(s.team == Team::MONTERAN) s.has_post = false;     // free the spots before taking them again
    int pit_next = 0;
    int k_by_section[2] = {0, 0};
    for(auto& s : m_soldiers)
    {
        if(s.team != Team::MONTERAN || !s.alive() || (s.is_player && !m_autoplay)) continue;
        sf::Vector2f spot;
        if(s.role == Role::OFFICER) spot = c + sf::Vector2f{std::cos(facing), std::sin(facing)} * 40.0f;
        else if(s.role == Role::MEDIC) spot = c + sf::Vector2f{-8.0f, 18.0f};
        else if(s.role == Role::RADIOMAN) spot = c + sf::Vector2f{std::cos(facing), std::sin(facing)} * 30.0f + sf::Vector2f{-12.0f, 8.0f};
        else if(s.role == Role::GUNNER && pit_next < 4)
        {
            int const pit = best_pit(pit_next++ % 2 == 0 ? facing : second);
            sf::Vector2f const pc = pits[std::max(0, pit)];
            spot = pc - unit(pc - c) * 3.0f;
            s.deploy_dir = std::atan2(pc.y - c.y, pc.x - c.x);
        }
        else
        {
            int const sec = std::clamp(s.section, 0, 1);
            int const k = k_by_section[sec]++;
            float a;
            if(std::abs(second - facing) > 0.3f)
            {
                // Two ways at once: a section to each, spread either side of it.
                float const centre = sec == 0 ? facing : second;
                a = centre + (k % 2 == 0 ? 1.0f : -1.0f) * 0.17f * static_cast<float>((k + 1) / 2);
            }
            else
            {
                // 1 Section on the near side of the threat, 2 Section on the far
                // side, each man a little round the ring from the last.
                a = facing + (sec == 0 ? -0.14f - 0.17f * static_cast<float>(k) : 0.14f + 0.17f * static_cast<float>(k));
            }
            spot = ring_spot(a);
        }
        s.post = spot;
        s.has_post = true;
        s.order = Order::HOLD;
        s.goal = spot;
        s.path.clear();
        s.cover_index = -1;
        s.deployed = false;
    }
}

// ---------------------------------------------------------------- the waves

void Battle::plan_wave()
{
    ++m_wave;
    int const n = m_wave;
    m_pending.clear();
    m_wave_broke = false;
    m_wave_go = 0.0f;

    // Where from.
    std::vector<float> dirs;
    switch(n)
    {
        case 1: case 2: case 3: dirs = {EAST}; break;
        case 4:         dirs = {EAST, NORTH_EAST}; break;
        case 5:         dirs = {EAST, SOUTH_EAST}; break;
        case 6:         dirs = {NORTH_EAST, SOUTH_EAST}; break;
        case 7:         dirs = {EAST, NORTH}; break;
        default:
        {
            std::vector<float> all{NORTH, NORTH_EAST, EAST, SOUTH_EAST, SOUTH};
            for(std::size_t i = all.size() - 1; i > 0; --i) std::swap(all[i], all[static_cast<std::size_t>(Random::integer(0, static_cast<int>(i)))]);
            dirs.assign(all.begin(), all.begin() + (n % 2 == 0 ? 3 : 2));
            break;
        }
    }
    m_wave_dirs = dirs;
    m_last_threat = dirs.front();

    sf::Vector2f const c = m_map.crest;
    sf::Vector2f const size = m_map.size;
    auto edge_point = [&](float a)
    {
        // Where a line out from the crest meets the edge of the map.
        sf::Vector2f const d = dir(a);
        float t = 1e9f;
        if(d.x > 1e-3f) t = std::min(t, (size.x - 12.0f - c.x) / d.x);
        if(d.x < -1e-3f) t = std::min(t, (12.0f - c.x) / d.x);
        if(d.y > 1e-3f) t = std::min(t, (size.y - 12.0f - c.y) / d.y);
        if(d.y < -1e-3f) t = std::min(t, (12.0f - c.y) / d.y);
        return c + d * t;
    };
    auto out_at = [&](float a, float r)
    {
        sf::Vector2f p = c + sf::Vector2f{std::cos(a) * r * 1.05f, std::sin(a) * r};
        p.x = std::clamp(p.x, 30.0f, size.x - 30.0f);
        p.y = std::clamp(p.y, 30.0f, size.y - 30.0f);
        return p;
    };
    auto cover_near = [&](sf::Vector2f p, float radius)
    {
        sf::Vector2f best = p;
        float best_d = radius;
        for(const auto& cp : m_map.cover)
            if(!cp.trench && length(cp.pos - p) < best_d) { best_d = length(cp.pos - p); best = cp.pos; }
        return best;
    };

    // What with.
    // Twenty-five men in the first, five more in each after. More than the
    // slots can hold at once come on as the dead make room for them.
    int const infantry = std::min(25 + 5 * (n - 1), 90);
    int storm   = n >= 3 ? std::min(1 + (n - 3), 6) : 0;
    int flamers = n >= 9 ? 2 : (n >= 6 ? 1 : 0);
    int const guns    = n >= 6 ? 2 : (n >= 2 ? 1 : 0);
    int const snipers = n >= 8 ? 2 : (n >= 3 ? 1 : 0);
    int const mortars = n >= 7 ? 2 : (n >= 4 ? 1 : 0);

    int name = Random::integer(0, 49);
    auto next_name = [&]() { return std::string(THEIR_NAMES[name++ % 50]); };
    std::size_t const groups = dirs.size();
    for(std::size_t g = 0; g < groups; ++g)
    {
        float const a = dirs[g];
        int const sec = add_section(Team::RECHIVII, std::string("wave ") + std::to_string(n) + " " + direction_name(a), 0.0f);
        sf::Vector2f const from = edge_point(a);
        sf::Vector2f const along = sf::Vector2f{-dir(a).y, dir(a).x};
        int const men = infantry / static_cast<int>(groups) + (g == 0 ? infantry % static_cast<int>(groups) : 0);
        float const start = 2.0f + static_cast<float>(g) * 6.0f;
        // They gather at the foot of the hill first, in what cover there is.
        sf::Vector2f const form_up = out_at(a, 600.0f);
        for(int k = 0; k < men; ++k)
        {
            Pending pd;
            pd.at = start + 0.45f * static_cast<float>(k);
            pd.role = Role::RIFLEMAN;
            if(g == 0 && k == 1) pd.role = Role::OFFICER;
            else if(k > 1 && storm > 0 && k % 3 == 2) { pd.role = Role::STORMTROOPER; --storm; }
            else if(k > 2 && flamers > 0 && k % 5 == 4) { pd.role = Role::FLAMER; --flamers; }
            pd.pos = from + along * Random::real(-110.0f, 110.0f) - dir(a) * Random::real(0.0f, 16.0f);
            pd.home = from;
            pd.section = sec;
            pd.leader = k == 0;
            pd.post = cover_near(form_up + along * (static_cast<float>(k) - static_cast<float>(men) * 0.5f) * 16.0f, 30.0f);
            pd.name = next_name();
            m_pending.push_back(pd);
            m_wave_go = std::max(m_wave_go, pd.at + 12.0f);
        }
    }
    // The ones who hang back, shared out between the directions.
    for(int k = 0; k < guns; ++k)
    {
        float const a = dirs[static_cast<std::size_t>(k) % groups];
        Pending pd;
        pd.at = 6.0f + 4.0f * static_cast<float>(k);
        pd.role = Role::GUNNER;
        pd.kind = 1;
        pd.post = cover_near(out_at(a + Random::real(-0.25f, 0.25f), Random::real(390.0f, 440.0f)), 50.0f);
        pd.pos = edge_point(a) + sf::Vector2f{-dir(a).y, dir(a).x} * Random::real(-60.0f, 60.0f);
        pd.home = edge_point(a);
        pd.name = next_name();
        m_pending.push_back(pd);
    }
    for(int k = 0; k < snipers; ++k)
    {
        float const a = dirs[static_cast<std::size_t>(k + 1) % groups] + (k % 2 ? 0.35f : -0.35f);
        Pending pd;
        pd.at = 4.0f + 7.0f * static_cast<float>(k);
        pd.role = Role::SNIPER;
        pd.kind = 2;
        pd.post = cover_near(out_at(a, Random::real(470.0f, 540.0f)), 60.0f);
        pd.pos = edge_point(a);
        pd.home = pd.pos;
        pd.name = next_name();
        m_pending.push_back(pd);
    }
    for(int k = 0; k < mortars; ++k)
    {
        float const a = dirs[static_cast<std::size_t>(k) % groups] + (k % 2 ? 0.2f : -0.2f);
        sf::Vector2f const at = out_at(a, Random::real(640.0f, 700.0f));
        for(int crew = 0; crew < 2; ++crew)
        {
            Pending pd;
            pd.at = 1.0f + 3.0f * static_cast<float>(k) + 0.6f * static_cast<float>(crew);
            pd.role = Role::RIFLEMAN;
            pd.kind = crew == 0 ? 3 : 4;
            pd.post = at;
            pd.pos = edge_point(a) + sf::Vector2f{static_cast<float>(crew) * 6.0f, 0.0f};
            pd.home = edge_point(a);
            pd.name = next_name();
            m_pending.push_back(pd);
        }
    }

    // Tell everyone.
    std::string from_where = direction_name(dirs.front());
    for(std::size_t g = 1; g < groups; ++g) from_where += (g + 1 == groups ? " and " : ", ") + std::string(direction_name(dirs[g]));
    notice("Wave " + std::to_string(n), 3.5f);
    Soldier* lt = officer(Team::MONTERAN);
    if(lt && !lt->is_player) say(*lt, "Here they come! " + std::string(n == 1 ? "Up the spur, from the " : "From the ") + from_where + "!", 4.0f);
    else say("", "They're coming again. From the " + from_where + ".", Team::MONTERAN, 4.0f);
    log("Wave " + std::to_string(n) + " came up from the " + from_where);

    // The sections turn to meet it, if there's an officer to turn them.
    if(lt && !lt->is_player && !player_leads()) hill_positions(dirs.front(), dirs.size() > 1 ? dirs[1] : dirs.front());
}

void Battle::spawn_pending()
{
    for(auto& pd : m_pending)
    {
        if(pd.at > m_stage_time || pd.at < 0.0f) continue;
        bool const ours = pd.kind == 5;
        std::string rank = ours ? pd.name.substr(0, pd.name.find(' ')) : (pd.role == Role::OFFICER ? "Lt." : "");
        std::string who  = ours ? pd.name.substr(pd.name.find(' ') + 1) : pd.name;
        Soldier* sp = spawn_soldier(ours ? Team::MONTERAN : Team::RECHIVII, pd.role, rank, who, pd.pos, pd.section);
        if(!sp) continue;                   // no room yet: he comes when someone else has gone
        pd.at = -1.0f;
        Soldier& s = *sp;
        s.active = true;
        s.alerted = true;
        s.leader = pd.leader;
        s.home = pd.home;
        s.stance = Stance::STAND;
        if(ours)
        {
            s.aim = 0.0f;
            s.has_post = true;
            s.post = pd.post;
            s.goal = pd.post;
            s.order = Order::HOLD;
            s.wave = -1;
            log(s.rank + " " + s.name + " came up with the replacements");
            continue;
        }
        s.wave = m_wave;
        s.aim = std::atan2(m_map.crest.y - s.pos.y, m_map.crest.x - s.pos.x);
        s.morale = Random::real(0.75f, 1.0f);
        if(pd.role == Role::OFFICER) s.smokes = 2;
        if(pd.leader) s.smokes = std::max(s.smokes, 1);
        switch(pd.kind)
        {
            case 1:     // a gun to give the rush covering fire
                s.has_post = true;
                s.support = true;
                s.post = pd.post;
                s.goal = pd.post;
                s.deploy_dir = std::atan2(m_map.crest.y - pd.post.y, m_map.crest.x - pd.post.x);
                break;
            case 2:     // a sniper, to lie up somewhere he can see the top
                s.has_post = true;
                s.support = true;
                s.post = pd.post;
                s.goal = pd.post;
                break;
            case 3:
            case 4:
            {
                // The mortar's crew: the first man brings the tube, the second the bombs.
                int index = -1;
                for(std::size_t i = 0; i < m_mortars.size(); ++i)
                    if(length(m_mortars[i].pos - pd.post) < 1.0f && !m_mortars[i].silenced) index = static_cast<int>(i);
                if(index < 0)
                {
                    place_mortar(pd.post, std::atan2(m_map.crest.y - pd.post.y, m_map.crest.x - pd.post.x), -1, -1);
                    index = static_cast<int>(m_mortars.size()) - 1;
                }
                Mortar& m = m_mortars[static_cast<std::size_t>(index)];
                if(pd.kind == 3) m.gunner = s.id; else m.loader = s.id;
                s.serving = index;
                s.support = true;
                s.has_post = true;
                sf::Vector2f const back = -dir(m.facing);
                s.post = m.pos + back * 6.0f + sf::Vector2f{-back.y, back.x} * (pd.kind == 3 ? 4.0f : -6.0f);
                s.goal = s.post;
                break;
            }
            default:
                // The rush: to the foot of the hill, and wait for the whistle.
                s.has_post = true;
                s.post = pd.post;
                s.goal = pd.post;
                break;
        }
    }
    std::erase_if(m_pending, [](const Pending& p) { return p.at < 0.0f; });
}

void Battle::script_hill(float dt)
{
    spawn_pending();
    sf::Vector2f const c = m_map.crest;

    // The afternoon goes, then the evening.
    m_night_target = std::clamp((static_cast<float>(m_wave) - 3.0f) / 4.0f, 0.0f, 1.0f);
    m_night += (m_night_target - m_night) * std::min(1.0f, dt * 0.04f);

    auto of_this_wave = [this](const Soldier& s) { return s.team == Team::RECHIVII && s.wave == m_wave && s.active; };

    switch(m_stage)
    {
        case Stage::HILL_LULL:
        {
            float const length_of = m_wave == 0 ? FIRST_LULL : LULL;
            // Some of them dig in, toward where the last lot came from.
            if(m_stage_time > 4.0f && m_stage_time - dt <= 4.0f)
            {
                int diggers = 0;
                for(auto& s : m_soldiers)
                {
                    if(diggers >= 4) break;
                    if(s.team != Team::MONTERAN || !s.fighting() || s.is_player || s.bags <= 0 || s.role == Role::MEDIC || s.role == Role::GUNNER) continue;
                    if(!s.has_post || length(s.pos - s.post) > 10.0f) continue;
                    sf::Vector2f const out = unit(s.pos - c);
                    if(std::cos(std::atan2(out.y, out.x) - m_last_threat) < 0.5f) continue;
                    start_dig(s, false, s.pos + out * 20.0f);
                    if(s.digging > 0.0f) ++diggers;
                }
            }
            if(m_stage_time > length_of)
            {
                plan_wave();
                set_stage(Stage::HILL_WAVE);
            }
            break;
        }
        case Stage::HILL_WAVE:
        {
            int rush = 0, left = 0, lost = 0, total = 0;
            bool officer_up = false;
            for(const auto& s : m_soldiers)
            {
                if(!of_this_wave(s)) continue;
                ++total;
                if(!s.fighting()) { ++lost; continue; }
                ++left;
                if(!s.support && s.serving < 0) ++rush;
                if(s.role == Role::OFFICER) officer_up = true;
            }
            bool queued = false;
            for(const auto& pd : m_pending) if(pd.kind != 5) queued = true;

            // Everyone at the foot of the hill: the whistles, and up they come.
            if(!queued && m_stage_time > m_wave_go && m_wave_go > 0.0f)
            {
                m_wave_go = -1.0f;
                bool whistled = false;
                for(auto& s : m_soldiers)
                    if(of_this_wave(s) && s.fighting() && !s.support && s.serving < 0)
                    {
                        s.has_post = false;
                        s.goal = s.pos;
                        s.idle_anim = 10.0f;            // off at once
                        if(s.leader || s.role == Role::OFFICER)
                        {
                            bark(s, "Vperyod! Za mnoy!", true);
                            if(!whistled) { SoundBank::get().play_at("whistle", s.pos, 1.0f); whistled = true; }
                        }
                    }
                if(Soldier* any = officer(Team::MONTERAN); any && !any->is_player) say(*any, "Whistles! Here they come! Stand to!", 3.0f);
                // Their mortars put smoke down on the slope in front of them.
                for(const auto& m : m_mortars)
                {
                    if(m.silenced || m.gunner < 0) continue;
                    float const a = std::atan2(m.pos.y - c.y, m.pos.x - c.x);
                    for(int k = 0; k < 3; ++k)
                    {
                        Shell sh;
                        sh.target = c + dir(a + Random::real(-0.25f, 0.25f)) * Random::real(230.0f, 320.0f);
                        sh.time = 3.0f + static_cast<float>(k) * 0.8f;
                        sh.kind = 4;
                        sh.smoke = true;
                        m_shells.push_back(sh);
                    }
                    SoundBank::get().play_at("mortar_fire", m.pos, 1.0f, 0.06f);
                }
            }

            // A wave that has gone to ground on the slope gets whistled up for
            // one last push, and if that doesn't do it, it goes back.
            if(!queued && !m_wave_broke && m_stage_time > 150.0f && m_stage_time - dt <= 150.0f)
                for(auto& s : m_soldiers)
                    if(of_this_wave(s) && s.fighting() && !s.support && s.serving < 0)
                    {
                        s.charge_until = m_time + 14.0f;
                        s.goal = c + sf::Vector2f{Random::real(-40, 40), Random::real(-40, 40)};
                        s.path.clear();
                        s.cover_index = -1;
                        bark(s, "Urraaa!", true);
                    }
            // Their nerve: a wave that has lost most of its men goes back.
            if(!queued && total > 0 && !m_wave_broke
               && (rush == 0 || lost * 100 >= total * 70 || (!officer_up && lost * 2 >= total) || m_stage_time > 200.0f))
            {
                m_wave_broke = true;
                for(auto& s : m_soldiers)
                    if(of_this_wave(s) && s.fighting()) { s.morale = 0.0f; s.has_post = false; s.deployed = false; s.serving = -1; }
                for(auto& m : m_mortars) if(!m.silenced) m.silenced = true;
                if(Soldier* any = officer(Team::MONTERAN); any && !any->is_player) say(*any, "They're going back! Let them go, save your rounds!", 3.5f);
                else notice("They're going back", 3.0f);
            }
            if(!queued && left == 0)
            {
                ++m_waves_held;
                Campaign::current().waves = m_waves_held;
                log("Wave " + std::to_string(m_wave) + " was beaten off");
                set_stage(Stage::HILL_LULL);
                break;
            }

            // At night, white stars over whoever is coming.
            if(m_night > 0.35f)
            {
                m_illum_timer -= dt;
                if(m_illum_timer <= 0.0f)
                {
                    m_illum_timer = Random::real(8.0f, 13.0f);
                    Soldier* shooter = officer(Team::MONTERAN);
                    if(!shooter) for(auto& s : m_soldiers) if(s.team == Team::MONTERAN && s.fighting()) { shooter = &s; break; }
                    sf::Vector2f over = c + dir(m_last_threat) * 260.0f;
                    float best = 1e9f;
                    for(const auto& s : m_soldiers)
                        if(of_this_wave(s) && s.fighting() && !s.support && length(s.pos - c) < best) { best = length(s.pos - c); over = s.pos; }
                    if(shooter) fire_flare(*shooter, over + sf::Vector2f{Random::real(-50, 50), Random::real(-50, 50)}, true);
                }
            }

            // Their leaders put smoke down in front of them for the last of the climb.
            for(auto& s : m_soldiers)
                if(of_this_wave(s) && s.fighting() && s.smokes > 0 && !s.support && s.morale > 0.0f)
                {
                    float const d = length(s.pos - c);
                    if(d < 290.0f && d > 170.0f && (s.suppression > 0.2f || s.leader || s.role == Role::OFFICER) && Random::chance(dt * 0.6f))
                    {
                        throw_grenade(s, s.pos + unit(c - s.pos) * 70.0f, true);
                        bark(s, "Dym! Dym!", true);
                    }
                }
            break;
        }
        default:
            break;
    }
}

// ---------------------------------------------------------------- mortars

void Battle::place_mortar(sf::Vector2f at, float facing, int gunner, int loader)
{
    Mortar m;
    m.pos = at;
    m.facing = facing;
    m.gunner = gunner;
    m.loader = loader;
    m.cooldown = Random::real(10.0f, 16.0f);
    m_mortars.push_back(m);
    // A shallow scrape for it, into the ground.
    draw::disc(m_stamps, at.x, at.y, 7.0f, MUD_DARK);
    draw::disc(m_stamps, at.x, at.y, 4.5f, lerp(MUD_DARK, INK, 0.3f));
}

void Battle::update_mortars(float dt)
{
    for(std::size_t i = 0; i < m_mortars.size(); ++i)
    {
        Mortar& m = m_mortars[i];
        m.seen = std::max(0.0f, m.seen - dt);
        m.kick = std::max(0.0f, m.kick - dt * 4.0f);
        if(m.silenced) continue;

        // Out of bombs, or the fight has moved past them: they pack up and go.
        bool const done = m.rounds <= 0 || (m_level == LevelId::TRENCHES && m_stage >= Stage::HOLD && m_stage < Stage::WON)
                       || (m_level == LevelId::FIELDS && m_stage >= Stage::COUNTER && m_stage < Stage::WON);
        if(done)
        {
            m.silenced = true;
            for(int id : {m.gunner, m.loader})
                if(id >= 0 && m_soldiers[static_cast<std::size_t>(id)].fighting())
                {
                    Soldier& c = m_soldiers[static_cast<std::size_t>(id)];
                    c.morale = 0.0f;
                    c.serving = -1;
                    c.has_post = false;
                }
            continue;
        }
        auto ok = [this](int id) { return id >= 0 && m_soldiers[static_cast<std::size_t>(id)].fighting() && m_soldiers[static_cast<std::size_t>(id)].serving >= 0; };
        if(!ok(m.gunner)) { m.gunner = ok(m.loader) ? m.loader : -1; m.loader = -1; }
        if(m.gunner < 0) { m.silenced = true; continue; }
        Soldier& g = m_soldiers[static_cast<std::size_t>(m.gunner)];
        if(length(g.pos - g.post) > 10.0f || !g.active) continue;     // not at the tube yet

        // When it works: once the fighting has started where it is.
        bool live = fighting_started();
        if(m_level == LevelId::FIELDS) live = live && m_stage >= Stage::STREAM;
        if(m_level == LevelId::HILL) live = m_stage == Stage::HILL_WAVE;
        if(!live) continue;

        m.cooldown -= dt;
        if(m.cooldown > 0.0f || m.rounds <= 0) continue;

        // What their riflemen can see: a man someone has a bead on, the more
        // of ours round him the better.
        sf::Vector2f target;
        float best = -1.0f;
        for(const auto& s : m_soldiers)
        {
            if(s.team != Team::RECHIVII || !s.fighting() || s.target < 0) continue;
            const Soldier& t = m_soldiers[static_cast<std::size_t>(s.target)];
            if(!t.fighting() || t.team != Team::MONTERAN) continue;
            float const d = length(t.pos - m.pos);
            if(d < 160.0f || d > 900.0f) continue;
            float score = 1.0f;
            for(const auto& o : m_soldiers) if(o.team == Team::MONTERAN && o.fighting() && length(o.pos - t.pos) < 40.0f) score += 1.0f;
            if(score > best) { best = score; target = t.pos; }
        }
        if(best < 0.0f) { m.cooldown = 2.0f; continue; }

        // Walk them on: the first bomb a long way off, each one after closer.
        if(length(target - m.aim_at) > 70.0f) m.error = 85.0f;
        else m.error = std::max(20.0f, m.error * 0.7f);
        m.aim_at = target;
        sf::Vector2f const impact = target + dir(Random::real(0.0f, 6.283f)) * m.error * Random::real(0.35f, 1.0f);
        Shell sh;
        sh.target = impact;
        sh.time = Random::real(3.2f, 3.8f);
        sh.kind = 4;
        m_shells.push_back(sh);
        --m.rounds;
        static const float pace[] = {10.0f, 8.0f, 6.5f};
        m.cooldown = pace[std::clamp(Campaign::current().difficulty, 0, 2)] + Random::real(0.0f, 2.5f);
        m.facing = std::atan2(target.y - m.pos.y, target.x - m.pos.x);
        m.kick = 1.0f;

        // The flash and the cough of it: anyone looking that way sees where it is.
        SoundBank::get().play_at("mortar_fire", m.pos, 1.0f, 0.06f);
        m_lights.push_back({m.pos, 70.0f, 0.18f, 0.18f, FIRE_YELLOW});
        sf::Vector2f const up = dir(m.facing) * 3.0f;
        for(int k = 0; k < 6; ++k)
            puff(m.pos + up, dir(m.facing + Random::real(-0.6f, 0.6f)) * Random::real(8.0f, 22.0f), Random::real(2.0f, 3.5f), 6.0f, Random::real(0.8f, 1.4f),
                 with_alpha(SMOKE, 0.5f));
        sparks(m.pos + up, 6, 60.0f, FIRE_YELLOW, 0.12f);
        m.seen = 8.0f;
        g.revealed = std::max(g.revealed, 6.0f);
        if(m.loader >= 0) m_soldiers[static_cast<std::size_t>(m.loader)].revealed = 6.0f;

        // Our radiomen answer it, if they've a call left.
        float const to_me = length(m.pos - player()->pos);
        if(to_me < 900.0f && m.rounds % 4 == 0 && player()->role == Role::RADIOMAN && player()->alive() && !m_autoplay)
            hint("Mortar! The flash marks it. Put the cursor on it and V to call the guns on it.", 5.0f);
        for(auto& r : m_soldiers)
        {
            if(r.team != Team::MONTERAN || !r.fighting() || r.role != Role::RADIOMAN || r.missions <= 0) continue;
            if(r.is_player && !m_autoplay) continue;
            if(length(r.pos - m.pos) > 1100.0f) continue;
            bool busy = false;
            for(const auto& ms : m_missions) if(ms.caller == r.id) busy = true;
            if(busy) continue;
            call_fire_mission(r, m.pos + sf::Vector2f{Random::real(-12, 12), Random::real(-12, 12)});
            bark(r, "Mortar, there! Calling it in!", true);
            break;
        }
    }

    // A mission still in step 2 is waiting for its correction: an AI
    // radioman moves it onto the mortar if one has fired since.
    for(auto& ms : m_missions)
    {
        if(ms.step != 2 || ms.caller < 0) continue;
        Soldier& r = m_soldiers[static_cast<std::size_t>(ms.caller)];
        if(r.is_player && !m_autoplay) continue;
        for(const auto& m : m_mortars)
            if(!m.silenced && m.seen > 0.0f && length(m.pos - ms.target) < 160.0f) { ms.target = m.pos; break; }
    }
}

// ---------------------------------------------------------------- digging in

void Battle::start_dig(Soldier& s, bool hole, sf::Vector2f toward)
{
    if(s.digging > 0.0f || !s.fighting()) return;
    sf::FloatRect r;
    if(hole)
    {
        if(m_map.in_crater(s.pos) || m_map.in_trench(s.pos))
        {
            if(s.is_player) notice("You're in a hole already", 1.5f);
            return;
        }
        r = sf::FloatRect(s.pos - sf::Vector2f{8.0f, 8.0f}, {16.0f, 16.0f});
    }
    else
    {
        if(s.bags <= 0)
        {
            if(s.is_player) notice("No sandbags left. Fill some at the dump", 2.0f);
            return;
        }
        // A row of bags across the way you're facing, just in front of you.
        sf::Vector2f const d = unit(toward - s.pos);
        sf::Vector2f const at = s.pos + d * (length(toward - s.pos) > 1.0f ? std::min(length(toward - s.pos), 20.0f) : 12.0f);
        bool const across_x = std::abs(d.x) > std::abs(d.y);    // facing east or west: the row runs north-south
        r = across_x ? sf::FloatRect({at.x - 3.0f, at.y - 9.0f}, {6.0f, 18.0f}) : sf::FloatRect({at.x - 9.0f, at.y - 3.0f}, {18.0f, 6.0f});
        r.position = {std::floor(r.position.x), std::floor(r.position.y)};
    }
    bool room = m_map.can_dig(r);
    for(const auto& o : m_soldiers)
        if(o.id != s.id && (o.life == Life::OK || o.life == Life::DOWNED) && r.contains(o.pos)) room = false;
    if(!hole && r.contains(s.pos)) room = false;
    if(!room)
    {
        if(s.is_player) notice(hole ? "No room to dig here" : "No room for bags there", 1.5f);
        return;
    }
    s.dig_rect = r;
    s.dig_hole = hole;
    s.dig_total = s.digging = hole ? 5.0f : 3.5f;
    s.vel = {};
    if(s.stance == Stance::STAND && !hole) s.stance = Stance::CROUCH;
    if(!s.is_player) bark(s, hole ? "Digging in." : "Filling bags. Cover me.", false);
    else hint(hole ? "Digging. Keep still. Moving stops it." : "Filling sandbags. Keep still. Moving stops it.", 2.5f);
}

void Battle::finish_dig(Soldier& s)
{
    if(s.dig_hole)
    {
        sf::Vector2f const at = s.dig_rect.getCenter();
        m_map.add_foxhole(at, 8.0f);
        m_map.draw_foxhole(m_stamps, at, 8.0f);
        if(s.is_player) notice("Foxhole dug", 1.8f);
    }
    else
    {
        --s.bags;
        m_map.add_bags(s.dig_rect);
        m_map.draw_bags(m_stamps, s.dig_rect);
        if(s.is_player) notice(std::to_string(s.bags) + (s.bags == 1 ? " sandbag left" : " sandbags left"), 1.8f);
    }
    dust(s.dig_rect.getCenter(), 10, MUD_LIGHT);
    SoundBank::get().play_at("shovel", s.pos, 0.6f, 0.1f);
}

}
