/**
 * Charge.cpp  -  Mud Cats
 *
 * The Sava meadow: a bayonet charge, played from either side.
 *
 * A hundred and forty Rechivii in two lines, a hundred yards apart, with
 * their officers, a corporal to every section and the regiment's colours
 * in the middle of the first line. Their guns put a few minutes of shells
 * on our trench while they form up in the long grass by the wood. Then the
 * drums, and the first line steps off at a walk, the second after it. Our
 * guns open on the field. At about four hundred yards the whistles go,
 * the whole line roars, and they run.
 *
 * Thirty of us wait for them in a trench dug last night, with two
 * machine guns and the bit of wire there was time for.
 *
 * It ends one of two ways. Enough of them fall, or their officers do, or
 * their colours go down and nobody picks them up, and the charge breaks:
 * everyone left runs back to the wood. Or they get in among us, and when
 * there are too few of us left to hold, the rest of us break instead.
 *
 * Your side is chosen on the briefing. As a Monteran you hold the trench;
 * as a Rechivii you're one of the men in the charge. Lose your man and you
 * take over the nearest one still going.
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

    constexpr float TRENCH_X = 410.0f;          // our line, roughly
    constexpr float RUN_FROM = 820.0f;          // where the whistles go: about four hundred yards out
    constexpr float FORM_TIME = 24.0f;          // their shells on us, while they form up
    constexpr float SECOND_LINE_DELAY = 9.0f;
}

Team Battle::ours() const
{
    return m_soldiers.empty() ? Team::MONTERAN : me().team;
}

// ---------------------------------------------------------------- the two sides

void Battle::spawn_charge()
{
    m_soldiers.reserve(MAX_SOLDIERS);
    Campaign const& c = Campaign::current();

    // --- us: a lieutenant and two sections in a shallow trench -----------------------------
    int const one = add_section(Team::MONTERAN, item("sections.monteran", 0), 0.0f);
    int const two = add_section(Team::MONTERAN, item("sections.monteran", 1), 0.0f);
    add(Team::MONTERAN, Role::OFFICER, Rank::LIEUTENANT, tr("names.monteran.lieutenant"), {394.0f, 560.0f}, -1);
    // Your own section's men are named in the text file, in this order; everyone else comes off the list.
    struct M { Role role; Rank rank; };
    const M first[] = {
        {Role::RIFLEMAN, Rank::CORPORAL}, {Role::RIFLEMAN, Rank::PRIVATE}, {Role::GUNNER, Rank::CORPORAL},
        {Role::FLAMER, Rank::PRIVATE},    {Role::MEDIC, Rank::PRIVATE},    {Role::RADIOMAN, Rank::PRIVATE},
        {Role::STORMTROOPER, Rank::PRIVATE}, {Role::RIFLEMAN, Rank::PRIVATE}};
    const M second[] = {
        {Role::RIFLEMAN, Rank::CORPORAL}, {Role::GUNNER, Rank::PRIVATE}, {Role::SNIPER, Rank::PRIVATE},
        {Role::STORMTROOPER, Rank::PRIVATE}, {Role::SAPPER, Rank::PRIVATE}};
    auto section = [&](int id, const M* named, int named_count, int total, bool yours)
    {
        for(int i = 0; i < total; ++i)
        {
            Role const role = i < named_count ? named[i].role : Role::RIFLEMAN;
            Rank const rank = i < named_count ? named[i].rank : Rank::PRIVATE;
            std::string name = yours && i < named_count ? item("names.monteran.your_section", static_cast<std::size_t>(i)) : next_name(Team::MONTERAN);
            add(Team::MONTERAN, role, rank, std::move(name), {}, id).leader = i == 0;
        }
    };
    section(one, first, 8, 14, true);
    section(two, second, 5, 15, false);

    // Everyone to a place on the fire step, the guns in their pits.
    auto trench_spot = [this](float y)
    {
        sf::Vector2f best{TRENCH_X, y};
        float best_d = 1e9f;
        for(const auto& cp : m_map.cover)
        {
            if(!cp.trench || cp.pos.x < 380.0f || cp.pos.x > 440.0f || std::abs(cp.pos.y - y) >= best_d) continue;
            bool taken = false;
            for(const auto& o : m_soldiers) if(o.has_post && length(o.post - cp.pos) < 6.0f) taken = true;
            if(taken) continue;
            best_d = std::abs(cp.pos.y - y);
            best = cp.pos;
        }
        return best;
    };
    int k1 = 0, k2 = 0;
    for(auto& s : m_soldiers)
    {
        if(s.team != Team::MONTERAN) continue;
        sf::Vector2f spot;
        if(s.role == Role::OFFICER)       spot = {392.0f, 560.0f};
        else if(s.role == Role::MEDIC)    spot = {372.0f, 470.0f};
        else if(s.role == Role::RADIOMAN) spot = {386.0f, 600.0f};
        else if(s.role == Role::GUNNER)   { spot = {446.0f, s.section == one ? 360.0f : 760.0f}; s.deploy_dir = 0.0f; }
        else if(s.section == one)         spot = trench_spot(110.0f + 34.0f * static_cast<float>(k1++));
        else                              spot = trench_spot(560.0f + 33.0f * static_cast<float>(k2++));
        s.pos = s.goal = s.post = spot;
        s.has_post = true;
        s.order = Order::HOLD;
        s.stance = Stance::CROUCH;
        s.aim = 0.0f;
        s.home = {12.0f, spot.y};
    }

    // --- them: two lines, the colours in the first ---------------------------------------
    auto their_name = [&]() { return next_name(Team::RECHIVII); };
    int player_section = -1;
    for(int line = 0; line < 2; ++line)
    {
        float const x0 = line == 0 ? 2370.0f : 2470.0f;
        int const sections = 5;
        int const per = line == 0 ? 14 : 13;
        for(int sec_i = 0; sec_i < sections; ++sec_i)
        {
            int const sec = add_section(Team::RECHIVII, tr("sections.charge_line", {{"line", item("sections.charge_lines", static_cast<std::size_t>(line))}, {"n", std::to_string(sec_i + 1)}}), 0.0f);
            if(line == 0 && sec_i == 2) player_section = sec;
            for(int i = 0; i < per; ++i)
            {
                int const k = sec_i * per + i;
                float const y = 110.0f + (880.0f / static_cast<float>(sections * per - 1)) * static_cast<float>(k);
                Role role = Role::RIFLEMAN;
                if(i == 4) role = Role::STORMTROOPER;
                if(line == 0 && i == 9 && (sec_i == 1 || sec_i == 3)) role = Role::FLAMER;
                if(line == 0 && i == 11 && (sec_i == 0 || sec_i == 4)) role = Role::SAPPER;
                if(line == 1 && i == 6 && sec_i % 2 == 0) role = Role::MEDIC;
                if(line == 1 && i == 8 && sec_i == 2) role = Role::RADIOMAN;
                Soldier& s = add(Team::RECHIVII, role, i == 0 ? Rank::CORPORAL : Rank::PRIVATE, their_name(), {x0 + Random::real(-8.0f, 8.0f), y}, sec);
                s.leader = i == 0;
                s.line = line;
                s.file_y = y;
                s.home = {2790.0f, y};
                s.aim = 3.14159f;
                s.stance = Stance::CROUCH;
                s.colours = line == 0 && sec_i == 2 && i == 7;
            }
        }
        // An officer to each line, and the captain with the first.
        Soldier& lt = add(Team::RECHIVII, Role::OFFICER, Rank::LIEUTENANT, their_name(), {x0 + 12.0f, 330.0f + 330.0f * static_cast<float>(line)}, -1);
        lt.line = line;
        lt.file_y = lt.pos.y;
        lt.home = {2790.0f, lt.pos.y};
        lt.aim = 3.14159f;
        lt.smokes = 1;
    }
    Soldier& captain = add(Team::RECHIVII, Role::OFFICER, Rank::CAPTAIN, their_name(), {2384.0f, 600.0f}, -1);
    captain.line = 0;
    captain.file_y = 600.0f;
    captain.home = {2790.0f, 600.0f};
    captain.aim = 3.14159f;

    for(auto& s : m_soldiers) if(s.team == Team::RECHIVII) { s.goal = s.pos; s.alerted = true; s.morale = Random::real(0.85f, 1.0f); }

    // --- you ------------------------------------------------------------------------------------
    if(c.playing == Team::RECHIVII)
    {
        // Whatever you go in as, you step off with the first line, carrying it.
        fill_roles(player_section);
        pick_player(player_section);
    }
    else
    {
        fill_roles(one);
        pick_player(one);
    }
    m_charge_men = 0;
    for(const auto& s : m_soldiers) if(s.team == Team::RECHIVII && s.line >= 0) ++m_charge_men;
}

// ---------------------------------------------------------------- how a man in the charge moves

void Battle::charge_think(Soldier& s)
{
    // Before the lines go: stay in the grass. Then a steady walk straight at
    // us, everyone keeping his place in the line; then the run. Close in, he
    // goes for the nearest of us with the bayonet.
    bool const going = m_stage == Stage::CHARGE_RUN || (m_stage == Stage::CHARGE_WALK && (s.line == 0 || m_second_off));
    if(!going)
    {
        if(length(s.goal - s.pos) > 1.0f && m_stage == Stage::CHARGE_FORM) { s.goal = s.pos; s.path.clear(); }
        return;
    }
    if(m_stage == Stage::CHARGE_WALK)
    {
        sf::Vector2f const ahead{TRENCH_X + 40.0f, s.file_y};
        if(length(s.goal - ahead) > 2.0f) { s.goal = ahead; s.path.clear(); s.cover_index = -1; }
        return;
    }

    // The run.
    s.charge_until = m_time + 60.0f;
    sf::Vector2f want{TRENCH_X - 6.0f, s.file_y};
    if(s.target >= 0)
    {
        const Soldier& t = m_soldiers[static_cast<std::size_t>(s.target)];
        if(length(t.pos - s.pos) < 110.0f) want = t.pos;
    }
    // In the trench with nobody in reach: the nearest one still fighting.
    if(s.pos.x < TRENCH_X + 30.0f && length(want - s.pos) < 8.0f)
    {
        float best = 1e9f;
        for(const auto& o : m_soldiers)
            if(o.team == Team::MONTERAN && o.fighting() && length(o.pos - s.pos) < best) { best = length(o.pos - s.pos); want = o.pos; }
    }
    if(length(s.goal - want) > 6.0f) { s.goal = want; s.path.clear(); s.cover_index = -1; }
}

// ---------------------------------------------------------------- the script

void Battle::script_charge(float dt)
{
    Team const side = ours();
    auto line_centre = [this](int line)
    {
        sf::Vector2f sum;
        int count = 0;
        for(const auto& s : m_soldiers)
            if(s.team == Team::RECHIVII && s.line == line && s.fighting() && s.morale > 0.0f) { sum += s.pos; ++count; }
        return count ? sum / static_cast<float>(count) : sf::Vector2f{2400.0f, 550.0f};
    };
    float front = 1e9f;
    for(const auto& s : m_soldiers)
        if(s.team == Team::RECHIVII && s.line >= 0 && s.fighting() && s.morale > 0.0f) front = std::min(front, s.pos.x);

    // The drums, while they walk.
    SoundBank& sound = SoundBank::get();
    if(m_stage == Stage::CHARGE_FORM || m_stage == Stage::CHARGE_WALK)
        sound.set_loop("drums_loop", std::clamp(sound.attenuation(line_centre(0)) * 1.0f + 0.1f, 0.0f, 0.5f));    // under the music, not over it
    else sound.set_loop("drums_loop", 0.0f);

    switch(m_stage)
    {
        case Stage::CHARGE_FORM:
        {
            // Their guns, on us.
            m_barrage_timer -= dt;
            if(m_barrage_timer <= 0.0f && m_stage_time < FORM_TIME - 3.0f)
            {
                m_barrage_timer = Random::real(1.0f, 2.2f);
                m_shells.push_back({{Random::real(370.0f, 520.0f), Random::real(100.0f, 1000.0f)}, 1.6f, false, 0});
            }
            if(m_stage_time > FORM_TIME) set_stage(Stage::CHARGE_WALK);
            break;
        }
        case Stage::CHARGE_WALK:
        {
            if(!m_second_off && m_stage_time > SECOND_LINE_DELAY)
            {
                m_second_off = true;
                SoundBank::get().play_at("whistle", line_centre(1), 1.0f);
            }
            if(front < RUN_FROM || m_stage_time > 75.0f) set_stage(Stage::CHARGE_RUN);
            break;
        }
        case Stage::CHARGE_RUN:
        {
            // Close in, the roar again, louder.
            if(front < 560.0f && !m_roared)
            {
                m_roared = true;
                sound.play_at("war_roar", {front + 30.0f, 550.0f}, 1.0f, 0.02f);
                sound.play("war_roar", 0.35f, 0.04f);
            }
            if(!m_second_off) m_second_off = true;
            break;
        }
        default:
            break;
    }

    // Our guns, once they're in the open.
    if(m_stage == Stage::CHARGE_WALK || m_stage == Stage::CHARGE_RUN)
    {
        m_far_guns -= dt;
        if(m_far_guns <= 0.0f)
        {
            m_far_guns = Random::real(1.3f, 2.4f);
            // On a man in one of the lines, well out from the trench.
            std::vector<sf::Vector2f> out;
            for(const auto& s : m_soldiers) if(s.team == Team::RECHIVII && s.line >= 0 && s.fighting() && s.pos.x > 680.0f) out.push_back(s.pos);
            if(!out.empty())
            {
                sf::Vector2f const at = out[static_cast<std::size_t>(Random::integer(0, static_cast<int>(out.size()) - 1))];
                m_shells.push_back({at + sf::Vector2f{Random::real(-50.0f, 50.0f), Random::real(-60.0f, 60.0f)}, 2.2f, false, 1});
            }
        }
    }

    // The colours: if they go down, someone takes them up, twice. After that
    // they stay down, and every man near sees it.
    for(auto& s : m_soldiers)
    {
        if(!s.colours || s.fighting()) continue;
        s.colours = false;
        Soldier* next = nullptr;
        float best = 45.0f;
        for(auto& o : m_soldiers)
            if(o.team == Team::RECHIVII && o.line >= 0 && o.fighting() && o.morale > 0.0f && length(o.pos - s.pos) < best) { best = length(o.pos - s.pos); next = &o; }
        if(next && !m_colours_down && m_colours_taken++ < 2)
        {
            next->colours = true;
            bark(*next, tr("barks.rechivii.colours"), true);
        }
        else
        {
            m_colours_down = true;
            for(auto& o : m_soldiers)
                if(o.team == Team::RECHIVII && o.fighting() && length(o.pos - s.pos) < 160.0f)
                {
                    o.suppression = std::min(1.0f, o.suppression + 0.35f);
                    o.morale -= 0.3f;
                }
            if(side == Team::MONTERAN) notice(tr("levels.charge.colours.theirs_down"), 2.5f);
            else notice(tr("levels.charge.colours.ours_down"), 2.5f);
        }
    }

    // How it ends.
    if(m_stage < Stage::CHARGE_WALK || m_stage >= Stage::WON) return;
    m_rout_check -= dt;
    if(m_rout_check > 0.0f) return;
    m_rout_check = 1.5f;

    int lost = 0, going = 0;
    bool officers = false;
    for(const auto& s : m_soldiers)
    {
        if(s.team != Team::RECHIVII || s.line < 0) continue;
        if(!s.fighting() || s.morale <= 0.0f) ++lost; else ++going;
        if(s.role == Role::OFFICER && s.fighting()) officers = true;
    }
    int us = 0, them_in = 0;
    for(const auto& s : m_soldiers)
    {
        if(s.team == Team::MONTERAN && s.fighting() && s.morale > 0.0f) ++us;
        if(s.team == Team::RECHIVII && s.fighting() && s.morale > 0.0f && s.pos.x < TRENCH_X + 70.0f) ++them_in;
    }

    // Their nerve.
    bool const broken = lost * 100 >= m_charge_men * 70 || (!officers && lost * 100 >= m_charge_men * 55) || (m_colours_down && lost * 100 >= m_charge_men * 60);
    if(broken && going > 0)
    {
        for(auto& s : m_soldiers)
            if(s.team == Team::RECHIVII && s.fighting() && s.morale > 0.0f && !s.is_player)
            {
                s.morale = 0.0f;
                s.charge_until = 0.0f;
                s.has_post = false;
                s.deployed = false;
                if(Random::chance(0.3f)) bark(s, tr("barks.rechivii.back"), true);
            }
        if(side == Team::RECHIVII) say("", tr("levels.charge.broke.rechivii"), Team::RECHIVII, 4.0f);
        else if(Soldier* lt = officer(Team::MONTERAN); lt && !lt->is_player) say(*lt, tr("levels.charge.broke.monteran_lt"), 3.5f);
    }
    // Ours: too few left to hold with them in the trench.
    if(us == 0 || (them_in >= 3 && us * 3 <= them_in && us <= 8))
    {
        for(auto& s : m_soldiers)
            if(s.team == Team::MONTERAN && s.fighting() && !s.is_player)
            {
                s.morale = 0.0f;
                s.has_post = false;
                s.order = Order::HOLD;
                if(Random::chance(0.4f)) bark(s, tr("barks.monteran.fall_back"), true);
            }
        set_stage(side == Team::MONTERAN ? Stage::LOST : Stage::WON);
        return;
    }
    // Theirs: nobody left coming (you, on their side, don't count: one man is not a charge).
    bool still_out = false;
    for(const auto& s : m_soldiers)
        if(s.team == Team::RECHIVII && s.fighting() && s.morale > 0.0f && !s.is_player) still_out = true;
    if(!still_out)
    {
        set_stage(side == Team::MONTERAN ? Stage::WON : Stage::LOST);
    }
}

}
