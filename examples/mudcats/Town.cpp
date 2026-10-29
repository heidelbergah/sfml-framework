/**
 * Town.cpp  -  Mud Cats
 *
 * Kalvo, at dusk: house to house up the main street to the church.
 *
 * B Platoon comes in from the fields to the west. The Rechivii hold the
 * town in depth: the first block of houses, with a machine gun behind a
 * barricade where the cross-street meets the main one; the second block
 * past it, with another barricade where the street opens into the square;
 * the town hall and the houses round the square; and the church at the far
 * end, its tower looking straight back down the street with a gun in it.
 * Behind the church, in the graveyard, a mortar. Behind that, east of the
 * town, a reserve company that comes in once the church falls.
 *
 * Each part wakes when we get to it. When a block is lost, whoever is left
 * of the men holding it falls back to the next.
 *
 * Played from either side. As a Monteran you go in with 2 Section. As a
 * Rechivii you're one of the men in the first houses, and you fall back
 * with them. The Monterans win when the church is theirs and the
 * counter-attack has been beaten off; the Rechivii when the attack has
 * cost the Monterans too much to go on, or when it has run out of time.
 */

#include "Battle.hpp"

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

    constexpr float PI = 3.14159f;
    constexpr float ATTACK_TIME = 840.0f;           // the attack is called off after this long
    constexpr float COUNTER_HOLD = 150.0f;          // the church held this long against the counter-attack is enough
    constexpr float BREAK_AT = 0.12f;               // what's left of the platoon when it gives up
    constexpr float BREAK_AT_NO_OFFICER = 0.18f;

    const char* const NAMES[] = {
        "Oslev", "Brannik", "Dvorek", "Halst", "Mirov", "Tesk", "Vadim", "Kosar", "Rudan", "Zelny",
        "Pavek", "Strom", "Ilko", "Gerd", "Yaro", "Lutz", "Borsk", "Kalen", "Matvo", "Resk",
        "Dravo", "Imre", "Sobek", "Varn", "Tolya", "Kresh", "Andrik", "Pellan", "Gosta", "Vey",
        "Hollin", "Radek", "Siro", "Mavik", "Darko", "Lesh", "Orvan", "Tibor", "Zavic", "Nemo",
        "Kuzma", "Bodan", "Elek", "Fedko", "Grisha", "Hrin", "Isko", "Jurek", "Kolya", "Lev",
        "Milo", "Nikolai", "Oleg", "Pyotr", "Rostik", "Semyon", "Taras", "Ustin", "Vlas", "Yegor",
        "Arkady", "Bogdan", "Danil", "Efim", "Foma", "Gleb", "Ignat", "Kirill", "Luka", "Makar"};

    const char* const MONTERANS[] = {
        "Ilie Brandt", "Oren Mazur", "Paolo Serra", "Viktor Ames", "Luca Fenn", "Radu Pell", "Ion Carver",
        "Mihai Stoll", "Andrei Holm", "Simon Lark", "Dario Kesh", "Nico Varro", "Teo Danes", "Emeric Volt",
        "Gaspar Wren", "Iosif Brann", "Matei Lusk", "Felix Arno", "Grigore Pask", "Adrian Kell", "Sorin Vey",
        "Petre Lund", "Cosmin Dral"};

    // Inside a window: where a man stands to fight from it.
    // Block A, the first houses: their front windows on the street, and the
    // windows in the end walls that look west, the way we come.
    const sf::Vector2f WEST_POSTS[] = {
        {263, 381}, {332, 457}, {542, 457}, {698, 457}, {263, 571}, {468, 543}, {708, 543}, {802, 457}};
    // Block B, past the cross-street.
    const sf::Vector2f EAST_POSTS[] = {
        {913, 381}, {1138, 457}, {1388, 457}, {913, 571}, {1258, 543}, {1342, 543}};
    // The square: the town hall's windows, the houses on the south side, the fountain.
    const sf::Vector2f SQUARE_POSTS[] = {
        {1580, 257}, {1870, 257}, {1678, 753}, {1786, 500}};
    // The church: the tower windows down the street, the nave windows onto the square.
    const sf::Vector2f CHURCH_POSTS[] = {{2023, 523}, {2079, 430}, {2079, 570}, {2130, 470}};
}

void Battle::spawn_town()
{
    m_soldiers.reserve(MAX_SOLDIERS);
    Campaign const& c = Campaign::current();

    // --- B Platoon, at the edge of the fields -------------------------------------------------
    int const one   = add_section(Team::MONTERAN, "1 Section", 115.0f);
    int const two   = add_section(Team::MONTERAN, "2 Section", -110.0f);
    int const three = add_section(Team::MONTERAN, "3 Section", -225.0f);
    std::size_t name = 0;
    auto place = [](int section, int i) { return sf::Vector2f{40.0f + static_cast<float>(i % 4) * 22.0f, 500.0f + (section == 0 ? 90.0f : (section == 1 ? -40.0f : -150.0f)) + static_cast<float>(i / 4) * 22.0f}; };
    auto make_section = [&](int id, bool medic)
    {
        for(int i = 0; i < 9; ++i)
        {
            Role role = i == 1 ? Role::GUNNER : (medic && i == 2 ? Role::MEDIC : Role::RIFLEMAN);
            if(i == 3) role = medic ? Role::SAPPER : Role::STORMTROOPER;
            if(i == 5) role = Role::STORMTROOPER;
            add(Team::MONTERAN, role, i == 0 ? "Cpl." : "Pvt.", MONTERANS[name++ % 23], place(id, i), id).leader = i == 0;
        }
    };
    make_section(one, true);
    struct M { Role role; const char* rank; const char* name; };
    const M yours[] = {
        {Role::RIFLEMAN, "Cpl.", "Petru Orban"}, {Role::RIFLEMAN, "Pvt.", "Emil Rell"},
        {Role::GUNNER, "Cpl.", "Anselm Kray"},  {Role::FLAMER, "Pvt.", "Dorian Vosk"},
        {Role::MEDIC, "Pvt.", "Teodor Halm"},   {Role::RADIOMAN, "Pvt.", "Tomas Dacre"},
        {Role::STORMTROOPER, "Pvt.", "Lio Marchetti"}, {Role::RIFLEMAN, "Pvt.", "Benedek Sorn"}};
    for(int i = 0; i < 8; ++i) add(Team::MONTERAN, yours[i].role, yours[i].rank, yours[i].name, place(two, i), two).leader = i == 0;
    add(Team::MONTERAN, Role::STORMTROOPER, "Pvt.", MONTERANS[name++ % 23], place(two, 8), two);
    make_section(three, false);
    add(Team::MONTERAN, Role::OFFICER, "Lt.", "Aurel Varga", {130.0f, 470.0f}, -1);
    for(auto& s : m_soldiers)
    {
        s.aim = 0.0f;
        s.goal = s.pos;
        s.home = {6.0f, std::clamp(s.pos.y, 330.0f, 670.0f)};
        if(s.leader) s.smokes = std::max(s.smokes, 1);           // a smoke grenade for every corporal: streets have to be crossed
    }

    // --- the Rechivii -----------------------------------------------------------------------------
    int n = 0;
    auto their_name = [&]() { return std::string(NAMES[n++ % 70]); };
    int const west    = add_section(Team::RECHIVII, "the first houses", 0.0f);
    int const east    = add_section(Team::RECHIVII, "the second block", 0.0f);
    int const square  = add_section(Team::RECHIVII, "the square", 0.0f);
    int const church  = add_section(Team::RECHIVII, "the church", 0.0f);
    int const reserve = add_section(Team::RECHIVII, "reserve", 0.0f);
    m_town_sections[0] = west; m_town_sections[1] = east; m_town_sections[2] = square;
    m_town_sections[3] = church; m_town_sections[4] = reserve;
    auto enemy = [&](Role role, sf::Vector2f post, int section, bool active, std::string rank = "Pvt.") -> Soldier&
    {
        Soldier& s = add(Team::RECHIVII, role, role == Role::OFFICER && rank == "Pvt." ? "Lt." : rank, their_name(), post, section);
        s.post = post;
        s.has_post = true;
        s.active = active;
        s.alerted = true;
        s.stance = Stance::CROUCH;
        s.aim = PI;
        return s;
    };

    // The first houses, and the gun at the barricade. Their lieutenant first,
    // so a Rechivii officer is him.
    enemy(Role::OFFICER, {604, 410}, -1, true);
    for(std::size_t i = 0; i < std::size(WEST_POSTS); ++i)
    {
        Role role = Role::RIFLEMAN;
        if(i == 7) role = Role::SNIPER;
        if(i == 3) role = Role::STORMTROOPER;
        if(i == 6) role = Role::MEDIC;
        enemy(role, WEST_POSTS[i], west, true).leader = i == 0;
    }
    Soldier& mg1 = enemy(Role::GUNNER, {864, 492}, west, true);
    mg1.deploy_dir = PI;
    mg1.deployed = true;
    mg1.stance = Stance::PRONE;
    mg1.body_angle = PI;
    enemy(Role::RIFLEMAN, {864, 514}, west, true);

    // The second block, and the barricade into the square.
    for(std::size_t i = 0; i < std::size(EAST_POSTS); ++i)
        enemy(i == 2 ? Role::SNIPER : (i == 5 ? Role::FLAMER : Role::RIFLEMAN), EAST_POSTS[i], east, false).leader = i == 0;
    enemy(Role::RIFLEMAN, {1522, 486}, east, false);

    // The square, and a gun behind the barricade where the street comes into it.
    for(std::size_t i = 0; i < std::size(SQUARE_POSTS); ++i)
        enemy(i == 1 ? Role::SNIPER : Role::RIFLEMAN, SQUARE_POSTS[i], square, false).leader = i == 0;
    Soldier& mg2 = enemy(Role::GUNNER, {1522, 512}, square, false);
    mg2.deploy_dir = PI;
    mg2.stance = Stance::PRONE;
    mg2.body_angle = PI;

    // The church: the captain, the gun in the tower, a medic.
    enemy(Role::OFFICER, {2170, 500}, -1, false, "Capt.");
    Soldier& tower = enemy(Role::GUNNER, {2023, 477}, church, false);
    tower.deploy_dir = PI;
    for(std::size_t i = 0; i < std::size(CHURCH_POSTS); ++i)
        enemy(Role::RIFLEMAN, CHURCH_POSTS[i], church, false).leader = i == 0;
    enemy(Role::MEDIC, {2210, 520}, church, false);

    // The mortar among the graves.
    {
        Soldier& g = enemy(Role::RIFLEMAN, {2332, 452}, -1, false);
        Soldier& l = enemy(Role::RIFLEMAN, {2342, 462}, -1, false);
        place_mortar({2340, 450}, PI, g.id, l.id);
        g.serving = l.serving = static_cast<int>(m_mortars.size()) - 1;
        m_mortars.back().rounds = 8;
    }

    // The reserve, east of the town, who come when the church falls.
    for(int i = 0; i < 10; ++i)
    {
        Role const role = i == 2 ? Role::GUNNER : (i == 4 || i == 8 ? Role::STORMTROOPER : (i == 6 ? Role::FLAMER : Role::RIFLEMAN));
        Soldier& r = enemy(role, {2392.0f, 340.0f + 34.0f * static_cast<float>(i)}, reserve, false);
        r.has_post = false;
        r.reserve = true;
        r.stance = Stance::STAND;
        r.leader = i == 0;
    }

    // --- you ----------------------------------------------------------------------------------------
    if(c.playing == Team::RECHIVII)
    {
        fill_roles(west);
        pick_player(west);
    }
    else
    {
        fill_roles(two);
        pick_player(two);
    }
    m_town_start = 0;
    for(const auto& s : m_soldiers) if(s.team == Team::MONTERAN) ++m_town_start;
}

// ---------------------------------------------------------------- reinforcements

void Battle::town_reinforce(int men)
{
    // A few men from the company's next platoon, sent up the street behind
    // us as each block falls. Each joins whichever section is shortest.
    static const char* const names[] = {
        "Anton Bors", "Silviu Rand", "Marek Haas", "Tudor Ivo", "Cyril Moss", "Eugen Falk", "Horia Dunn",
        "Laszlo Penn", "Ovidiu Crane", "Remus Tal", "Stefan Ward", "Vasile Orr", "Bogdan Leif", "Costin Hale"};
    for(int k = 0; k < men && m_soldiers.size() < MAX_SOLDIERS; ++k)
    {
        int fewest = -1, fewest_n = 99;
        for(const auto& sec : m_sections)
        {
            if(sec.team != Team::MONTERAN) continue;
            int n = 0;
            for(const auto& o : m_soldiers) if(o.section == sec.id && o.fighting()) ++n;
            if(n < fewest_n) { fewest_n = n; fewest = sec.id; }
        }
        if(fewest < 0) return;
        Role const role = k % 3 == 1 ? Role::STORMTROOPER : Role::RIFLEMAN;
        sf::Vector2f const from{14.0f + static_cast<float>(k) * 10.0f, 492.0f + static_cast<float>(k % 3) * 8.0f};
        Soldier& s = add(Team::MONTERAN, role, "Pvt.", names[(m_town_start + k) % 14], from, fewest);
        s.home = {6.0f, from.y};
        s.order = Order::ADVANCE;
        const Section& sec = m_sections[static_cast<std::size_t>(fewest)];
        set_goal(s, m_map.waypoints[static_cast<std::size_t>(std::clamp(sec.at, 0, static_cast<int>(m_map.waypoints.size()) - 1))] + sf::Vector2f{-30.0f, sec.band}, 40.0f);
        if(!section_leader(fewest)) s.leader = true;
    }
    m_town_start += men;
    if(me().team == Team::MONTERAN) notice("Men from C Platoon coming up behind you", 3.0f);
    else notice("More of them, coming up the street", 3.0f);
}

// ---------------------------------------------------------------- falling back

void Battle::town_fall_back(int from, int to)
{
    // Whoever is left of one part goes to the posts of the next, filling the
    // ones nobody is at. Those who don't get a post find cover near the rest.
    std::vector<sf::Vector2f> spots;
    if(to == m_town_sections[1]) spots.assign(std::begin(EAST_POSTS), std::end(EAST_POSTS));
    else if(to == m_town_sections[2]) spots.assign(std::begin(SQUARE_POSTS), std::end(SQUARE_POSTS));
    else spots.assign(std::begin(CHURCH_POSTS), std::end(CHURCH_POSTS));
    std::erase_if(spots, [this](sf::Vector2f p)
    {
        for(const auto& o : m_soldiers) if(o.fighting() && o.has_post && length(o.post - p) < 6.0f) return true;
        return false;
    });
    sf::Vector2f const rally = to == m_town_sections[1] ? sf::Vector2f{1180.0f, 500.0f} : (to == m_town_sections[2] ? sf::Vector2f{1800.0f, 500.0f} : sf::Vector2f{2150.0f, 500.0f});
    bool any = false;
    for(auto& s : m_soldiers)
    {
        bool const officer_here = s.role == Role::OFFICER && s.section < 0 && s.team == Team::RECHIVII && s.active && s.pos.x < rally.x - 200.0f;
        if(s.team != Team::RECHIVII || !s.fighting() || (s.section != from && !officer_here)) continue;
        s.section = s.role == Role::OFFICER ? -1 : to;
        s.leader = false;
        s.active = true;
        s.deployed = false;
        s.serving = -1;
        if(!spots.empty())
        {
            s.post = spots.back();
            spots.pop_back();
        }
        else
        {
            int const cp = pick_cover(s, rally + sf::Vector2f{Random::real(-60, 60), Random::real(-140, 140)}, 90.0f, m_map.waypoints.front());
            s.post = cp >= 0 ? m_map.cover[static_cast<std::size_t>(cp)].pos : rally;
        }
        s.has_post = true;
        s.path.clear();
        s.suppression = std::min(s.suppression, 0.4f);
        if(!s.is_player && Random::chance(0.4f)) bark(s, "Nazad! Nazad!", true);
        any = true;
    }
    promote(to);
    if(any && me().team == Team::RECHIVII && me().section == to)
    {
        notice("Fall back!", 3.0f);
        say("Lt.", to == m_town_sections[1] ? "Back! Over the cross-street, into the next houses!" : (to == m_town_sections[2] ? "Back to the square! The town hall, the fountain, go!" : "Into the church! Hold the church!"), Team::RECHIVII, 4.0f);
    }
}

// ---------------------------------------------------------------- the script

void Battle::script_town(float dt)
{
    Soldier& p = *player();
    bool const theirs = p.team == Team::RECHIVII;
    auto enemies_in = [this](const sf::FloatRect& r)
    {
        int k = 0;
        for(const auto& s : m_soldiers) if(s.team == Team::RECHIVII && s.fighting() && s.active && r.contains(s.pos)) ++k;
        return k;
    };
    auto friends_in = [this](const sf::FloatRect& r)
    {
        int k = 0;
        for(const auto& s : m_soldiers) if(s.team == Team::MONTERAN && s.alive() && r.contains(s.pos)) ++k;
        return k;
    };
    auto sections_at = [this](int at)
    {
        for(const auto& sec : m_sections) if(sec.team == Team::MONTERAN && section_leader(sec.id) && sec.at < at) return false;
        return true;
    };
    sf::FloatRect const zone_a({240.0f, 330.0f}, {640.0f, 340.0f});
    sf::FloatRect const zone_b({890.0f, 330.0f}, {640.0f, 340.0f});

    // The church bell, before it starts: somebody in the tower ringing it to warn them.
    if(m_stage == Stage::TOWN_ENTER)
    {
        m_bell_timer -= dt;
        if(m_bell_timer <= 0.0f && m_bells < 6)
        {
            m_bell_timer = 2.2f;
            ++m_bells;
            SoundBank::get().play_at("church_bell", {2040.0f, 500.0f}, 1.0f, 0.0f);
            SoundBank::get().play("church_bell", 0.18f, 0.0f);
        }
    }

    switch(m_stage)
    {
        case Stage::TOWN_ENTER:
        {
            bool in_town = false;
            for(const auto& s : m_soldiers) if(s.team == Team::MONTERAN && s.alive() && s.pos.x > 232.0f) in_town = true;
            if(m_stage_time > 16.0f || in_town || m_combat_heat > 0.4f) set_stage(Stage::TOWN_STREET);
            break;
        }
        case Stage::TOWN_STREET:
            if((enemies_in(zone_a) == 0 || (sections_at(3) && enemies_in(zone_a) <= 4 && m_stage_time > 45.0f))
               && friends_in(sf::FloatRect({560.0f, 330.0f}, {300.0f, 340.0f})) > 0 && you_there(sf::FloatRect({560.0f, 330.0f}, {300.0f, 340.0f}), 160.0f))
                set_stage(Stage::TOWN_CROSS);
            break;
        case Stage::TOWN_CROSS:
            if((enemies_in(zone_b) == 0 || (sections_at(6) && enemies_in(zone_b) <= 5 && m_stage_time > 40.0f))
               && friends_in(sf::FloatRect({1240.0f, 330.0f}, {290.0f, 340.0f})) > 0 && you_there(sf::FloatRect({1240.0f, 330.0f}, {290.0f, 340.0f}), 160.0f))
                set_stage(Stage::TOWN_SQUARE);
            break;
        case Stage::TOWN_SQUARE:
        {
            // Theirs once nobody holds it: with men of ours inside, or close round it and the square empty.
            sf::FloatRect const round_it(m_map.church_area.position - sf::Vector2f{120.0f, 120.0f}, m_map.church_area.size + sf::Vector2f{240.0f, 240.0f});
            bool const empty = enemies_in(m_map.church_area) == 0;
            bool const ours_now = friends_in(m_map.church_area) >= 2 || (friends_in(round_it) >= 1 && enemies_in(m_map.square_area) == 0 && m_stage_time > 30.0f);
            if(empty && ours_now && you_there(m_map.church_area, 60.0f)) set_stage(Stage::COUNTER);
        }
            break;
        case Stage::COUNTER:
        {
            // The counter-attack is beaten when it's down to a handful, or when the church has
            // been held a good while with it all committed: whoever is left of them gives up
            // the town and goes. Not every last man in every cellar has to be found.
            bool waiting = false;
            int left = 0;
            for(const auto& s : m_soldiers)
            {
                if(s.reserve && !s.active && s.fighting()) waiting = true;
                if(s.team == Team::RECHIVII && s.fighting() && s.morale > 0.0f) ++left;
            }
            bool const held = m_stage_time > COUNTER_HOLD && friends_in(m_map.church_area) >= 1;
            if(!waiting && !m_town_broken && (left <= 3 || held))
            {
                for(auto& s : m_soldiers)
                    if(s.team == Team::RECHIVII && s.fighting() && !s.is_player)
                    {
                        s.morale = 0.0f;
                        s.path.clear();
                    }
                log("Kalvo was taken");
                set_stage(theirs ? Stage::LOST : Stage::WON);
            }
            break;
        }
        default:
            break;
    }

    // --- the attack breaking ------------------------------------------------------------------
    // When it has cost the platoon too much, or gone on too long, the
    // Monterans pull back, from whichever side you're playing it.
    if(fighting_started() && !m_town_broken && m_stage < Stage::WON)
    {
        m_town_check -= dt;
        if(m_town_check <= 0.0f)
        {
            m_town_check = 2.0f;
            int left = 0;
            for(const auto& s : m_soldiers) if(s.team == Team::MONTERAN && s.fighting()) ++left;
            float const share = static_cast<float>(left) / static_cast<float>(std::max(1, m_town_start));
            bool const no_officer = officer(Team::MONTERAN) == nullptr;
            bool const spent = share <= BREAK_AT || (no_officer && share <= BREAK_AT_NO_OFFICER);
            bool const late = m_time > ATTACK_TIME && m_stage < Stage::COUNTER;
            if(spent || late)
            {
                m_town_broken = true;
                m_town_break_time = m_time;
                for(auto& s : m_soldiers)
                    if(s.team == Team::MONTERAN && s.fighting() && !s.is_player)
                    {
                        s.morale = 0.0f;
                        s.path.clear();
                        if(Random::chance(0.3f)) bark(s, "Back! Get back!", true);
                    }
                if(Soldier* lt = officer(Team::MONTERAN)) say(*lt, late ? "That's enough. Pull back, all of you. We'll come again tomorrow." : "Back! Back out of it! Get the wounded out!", 4.0f);
                SoundBank::get().play_at("whistle", m_map.waypoints.front(), 0.8f);
                notice(late ? "The attack is called off" : (theirs ? "They're pulling back!" : "Pull back! Pull back!"), 3.5f);
                log(late ? "The attack on Kalvo was called off at dusk" : "The attack on Kalvo broke");
            }
        }
    }
    if(m_town_broken && m_stage < Stage::WON && m_time - m_town_break_time > 6.0f) set_stage(theirs ? Stage::WON : Stage::LOST);
}

}
