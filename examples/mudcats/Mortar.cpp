/**
 * Mortar.cpp  -  Mud Cats
 *
 * Their mortars. A tube on a baseplate and the two men who serve it. It
 * drops its bombs on whatever their riflemen can see, walking them on
 * round by round, and every round's flash gives it away: a radioman can
 * put the cursor on the mark and call the guns down on it.
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
    sf::Vector2f dir(float a) { return {std::cos(a), std::sin(a)}; }
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

        // Out of bombs: they pack up and go.
        bool const done = m.rounds <= 0;
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
        if(!fighting_started()) continue;

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
            hint(tr("hints.mortar"), 5.0f);
        for(auto& r : m_soldiers)
        {
            if(r.team != Team::MONTERAN || !r.fighting() || r.role != Role::RADIOMAN || r.missions <= 0) continue;
            if(r.is_player && !m_autoplay) continue;
            if(length(r.pos - m.pos) > 1100.0f) continue;
            bool busy = false;
            for(const auto& ms : m_missions) if(ms.caller == r.id) busy = true;
            if(busy) continue;
            call_fire_mission(r, m.pos + sf::Vector2f{Random::real(-12, 12), Random::real(-12, 12)});
            bark(r, tr("barks.monteran.mortar_seen"), true);
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

}
