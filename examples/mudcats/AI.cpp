/**
 * AI.cpp  -  Mud Cats
 *
 * Everyone who is not you. Each man thinks a few times a second and acts
 * every frame.
 *
 * Thinking picks a target and a place to be. The place is usually a cover
 * point: behind sandbags on the side away from the enemy, in a trench, in
 * a shell hole. Acting walks him there, turns him, sets his stance and
 * pulls the trigger.
 *
 * What makes it play like a firefight rather than a shooting gallery is
 * mostly suppression: a man who has had rounds snapping past him lies
 * down and stops moving, and fires blind if at all. So the way to take a
 * position is the old one -- pin it with fire, and move someone else up
 * while its head is down.
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
    sf::Vector2f unit(sf::Vector2f v) { float const l = length(v); return l > 1e-5f ? v / l : sf::Vector2f{1.0f, 0.0f}; }
    sf::Vector2f dir(float a) { return {std::cos(a), std::sin(a)}; }
    float angle_of(sf::Vector2f v) { return std::atan2(v.y, v.x); }
}

bool Battle::can_see(const Soldier& a, const Soldier& b) const
{
    return m_map.line_of_sight(a.pos, b.pos);
}

int Battle::find_target(const Soldier& s) const
{
    const WeaponSpec& w = weapon_of(s.role);
    float const range = w.flame ? 200.0f : std::min(w.range, 430.0f);

    int best = -1;
    float best_score = 1e9f;
    for(const auto& e : m_soldiers)
    {
        if(e.team == s.team || !e.alive() || (!e.active && !e.is_player)) continue;
        float const d = length(e.pos - s.pos);
        if(d > range) continue;
        // A man lying low in a trench far away is hard to pick out.
        if(e.revealed <= 0.0f)
        {
            if(d > 230.0f && (e.stance == Stance::PRONE || (e.stance == Stance::CROUCH && m_map.in_trench(e.pos)))) continue;
            if(d > 300.0f && e.stance == Stance::CROUCH) continue;
        }
        if(!can_see(s, e)) continue;
        float score = d;
        if(e.role == Role::GUNNER || e.role == Role::FLAMER) score *= 0.75f;
        if(e.id == s.target) score *= 0.7f;              // keep on the man you were shooting
        if(score < best_score) { best_score = score; best = e.id; }
    }
    return best;
}

int Battle::pick_cover(const Soldier& s, sf::Vector2f around, float radius, sf::Vector2f threat) const
{
    int best = -1;
    float best_score = 1e9f;
    for(std::size_t i = 0; i < m_map.cover.size(); ++i)
    {
        const CoverPoint& c = m_map.cover[i];
        float const d_around = length(c.pos - around);
        if(d_around > radius) continue;

        bool taken = false;
        for(const auto& o : m_soldiers)
        {
            if(o.id == s.id || !o.alive()) continue;
            if(o.cover_index == static_cast<int>(i) || length(o.goal - c.pos) < 7.0f) { taken = true; break; }
        }
        if(taken) continue;

        bool burning = false;
        for(const auto& f : m_fires) if(length(f.pos - c.pos) < f.radius + 4.0f) burning = true;
        if(burning) continue;

        float score = d_around * 0.6f + length(c.pos - s.pos) * 0.2f;
        sf::Vector2f const to_threat = unit(threat - c.pos);
        if(c.protects.x == 0.0f && c.protects.y == 0.0f) score -= c.trench ? 45.0f : 22.0f;
        else if(c.protects.x * to_threat.x + c.protects.y * to_threat.y > 0.35f) score -= 40.0f;
        else score += 35.0f;

        if(score < best_score) { best_score = score; best = static_cast<int>(i); }
    }
    return best;
}

void Battle::set_goal(Soldier& s, sf::Vector2f goal)
{
    sf::Vector2f const forward = s.team == Team::MONTERAN ? sf::Vector2f{260.0f, 0.0f} : sf::Vector2f{-260.0f, 0.0f};
    sf::Vector2f const threat = s.threat_age < 8.0f ? s.threat : goal + forward;
    int const c = pick_cover(s, goal, 60.0f, threat);
    s.cover_index = c;
    s.goal = c >= 0 ? m_map.cover[static_cast<std::size_t>(c)].pos : goal;
    s.path.clear();
    s.repath = 0.0f;
}

void Battle::medic_logic(Soldier& s)
{
    Soldier* patient = nullptr;
    float best = 320.0f;
    for(auto& o : m_soldiers)
    {
        if(o.team != s.team || o.id == s.id) continue;
        bool const needs = (o.life == Life::DOWNED && o.bleed_out > 2.0f) || (o.alive() && o.bleeding > 0.0f);
        if(!needs) continue;
        bool claimed = false;
        for(const auto& m : m_soldiers) if(m.id != s.id && m.treating == o.id) claimed = true;
        if(claimed) continue;
        float const d = length(o.pos - s.pos);
        if(d < best) { best = d; patient = &o; }
    }
    if(!patient || s.medkits <= 0 || (s.suppression > 0.65f && best > 30.0f))
    {
        if(s.treating < 0 && s.has_post) s.goal = s.post;
        return;
    }

    if(best < 10.0f)
    {
        if(s.treating != patient->id)
        {
            s.treating = patient->id;
            s.treat_time = patient->life == Life::DOWNED ? 3.2f : 1.6f;
            SoundBank::get().play_at("bandage", s.pos, 0.6f);
            bark(s, s.team == Team::MONTERAN ? "Hold still. I've got you." : "Tov, tov...", false);
        }
        s.goal = s.pos;
        s.stance = Stance::CROUCH;
    }
    else
    {
        s.goal = patient->pos;
        s.path.clear();
        s.cover_index = -1;
        if(s.bark_cooldown <= 0.0f && s.team == Team::MONTERAN) bark(s, "Coming! Stay down!", true);
    }
}

// ---------------------------------------------------------------- thinking

void Battle::think(Soldier& s, float dt)
{
    s.think -= dt;
    s.reaction -= dt;
    s.repath -= dt;
    if(s.think > 0.0f) return;
    s.think = Random::real(0.2f, 0.32f);

    // --- targets ----------------------------------------------------------------
    int const target = find_target(s);
    if(target != s.target)
    {
        if(target >= 0 && s.target < 0)
        {
            s.reaction = Random::real(0.35f, 0.9f) + s.suppression * 0.8f;
            if(s.bark_cooldown <= 0.0f && Random::chance(0.5f))
                bark(s, s.team == Team::MONTERAN ? (Random::chance(0.5f) ? "Contact!" : "There! Front!") : "Kesh! Kesh!", true);
        }
        s.target = target;
    }
    if(target >= 0)
    {
        s.threat = m_soldiers[static_cast<std::size_t>(target)].pos;
        s.threat_age = 0.0f;
    }

    // --- grenades landing nearby: get away from them --------------------------------
    for(const auto& g : m_grenades)
    {
        if(g.height > 3.0f) continue;
        float const d = length(g.pos - s.pos);
        if(d < 36.0f && !(s.role == Role::GUNNER && s.deployed))
        {
            bark(s, s.team == Team::MONTERAN ? "Grenade!" : "Vrak! Vrak!", true);
            s.goal = s.pos + unit(s.pos - g.pos) * 48.0f;
            s.path.clear();
            s.cover_index = -1;
            s.deployed = false;
            s.suppression = std::min(s.suppression, 0.5f);
            return;
        }
    }

    // --- burning or broken ------------------------------------------------------------
    if(s.burning > 0.0f)
    {
        s.goal = s.pos + dir(Random::real(0.0f, 6.283f)) * 40.0f;
        s.path.clear();
        return;
    }
    if(s.team == Team::RECHIVII && s.morale <= 0.0f)
    {
        s.goal = {m_map.size.x - 4.0f, std::clamp(s.pos.y, 150.0f, 750.0f)};
        s.deployed = false;
        if(s.path.empty() || s.repath <= 0.0f) { s.path = m_map.find_path(s.pos, s.goal); s.repath = 3.0f; }
        if(s.pos.x > m_map.size.x - 20.0f) { s.life = Life::FLED; s.fate = "Ran"; }
        return;
    }

    // --- grenades of their own -----------------------------------------------------------
    if(target >= 0 && s.grenades > 0 && s.suppression < 0.6f && Random::chance(0.07f))
    {
        const Soldier& t = m_soldiers[static_cast<std::size_t>(target)];
        float const d = length(t.pos - s.pos);
        bool const dug_in = m_map.in_trench(t.pos) || m_map.sandbag_between(s.pos, t.pos, 20.0f) || m_map.in_crater(t.pos);
        if(d > 45.0f && d < 140.0f && dug_in)
            throw_grenade(s, t.pos + sf::Vector2f{Random::real(-10, 10), Random::real(-10, 10)});
    }

    // --- where to be --------------------------------------------------------------------------
    if(s.role == Role::MEDIC)
    {
        medic_logic(s);
        if(s.treating >= 0 || length(s.goal - s.pos) > 12.0f) return;
    }

    if(s.team == Team::MONTERAN)
    {
        Soldier& p = *player();
        Soldier* lead = officer(Team::MONTERAN);
        if(!lead || lead->id == s.id) lead = &p;
        if(lead->id == s.id) return;

        if(s.order == Order::FOLLOW && lead->alive())
        {
            // A loose file behind whoever leads.
            int slot = 0;
            for(const auto& o : m_soldiers)
            {
                if(o.id == s.id) break;
                if(o.team == Team::MONTERAN && o.alive() && !o.is_player && o.id != lead->id) ++slot;
            }
            float const face = lead->aim;
            sf::Vector2f const back = -dir(face), side{-std::sin(face), std::cos(face)};
            sf::Vector2f const spot = lead->pos + back * (16.0f + 10.0f * static_cast<float>(slot / 2)) + side * ((slot % 2 ? 1.0f : -1.0f) * 12.0f);
            if(length(spot - s.goal) > 26.0f)
            {
                if(s.threat_age < 5.0f) set_goal(s, spot);
                else { s.goal = spot; s.path.clear(); s.cover_index = -1; }
            }
        }
        return;
    }

    // --- Rechivii ---------------------------------------------------------------------------------
    if(s.has_post)
    {
        if(length(s.goal - s.post) > 1.0f)
        {
            s.goal = s.post;
            s.path.clear();
        }
        return;
    }

    // The scout creeps up to the wire, then pulls back to his trench.
    if(!s.reserve)
    {
        if(m_stage_time < 16.0f && s.suppression < 0.5f && m_stage == Stage::SCOUT)
        {
            if(length(s.goal - sf::Vector2f{1180, 505}) > 1.0f) { s.goal = {1180, 505}; s.path.clear(); }
        }
        else
        {
            s.has_post = true;
            s.post = {1712, 470};
            s.goal = s.post;
            s.path.clear();
        }
        return;
    }

    // The counter-attack: bound from cover to cover toward the farm.
    if(length(s.goal - s.pos) < 6.0f || s.goal == s.pos)
    {
        s.idle_anim += 0.3f;
        if(s.idle_anim > Random::real(2.0f, 5.0f) || s.goal == sf::Vector2f{})
        {
            s.idle_anim = 0.0f;
            sf::Vector2f aim_for{2320.0f, 380.0f};
            float best = 1e9f;
            for(const auto& o : m_soldiers)
                if(o.team == Team::MONTERAN && o.alive() && length(o.pos - s.pos) < best) { best = length(o.pos - s.pos); aim_for = o.pos; }
            if(best > 70.0f)
            {
                sf::Vector2f const hop = s.pos + unit(aim_for - s.pos) * std::min(90.0f, best - 40.0f);
                int const c = pick_cover(s, hop, 60.0f, aim_for);
                s.cover_index = c;
                s.goal = c >= 0 ? m_map.cover[static_cast<std::size_t>(c)].pos : hop;
                s.path.clear();
                if(Random::chance(0.3f)) bark(s, "Toven! Toven!", true);
            }
        }
    }
}

// ---------------------------------------------------------------- acting

void Battle::steer(Soldier& s, float dt)
{
    sf::Vector2f const to_goal = s.goal - s.pos;
    float const dist = length(to_goal);

    bool const pinned = s.suppression > 0.75f && s.burning <= 0.0f && s.morale > 0.0f;
    bool const busy = s.treating >= 0 || (s.role == Role::GUNNER && (s.deployed || s.deploying > 0.0f));
    if(dist < 3.0f || pinned || busy || s.sitting)
    {
        s.vel *= std::exp(-dt * 10.0f);
        return;
    }

    if(s.path.empty() || s.repath <= 0.0f)
    {
        if(dist > 20.0f || !m_map.walkable(s.goal)) s.path = m_map.find_path(s.pos, s.goal);
        else s.path = {s.goal};
        s.repath = 2.5f;
        if(s.path.empty()) s.path = {s.goal};
    }

    sf::Vector2f next = s.path.front();
    if(length(next - s.pos) < 4.0f && s.path.size() > 1)
    {
        s.path.erase(s.path.begin());
        next = s.path.front();
    }

    float speed = 40.0f;
    bool const danger = s.threat_age < 6.0f || m_stage >= Stage::ASSAULT;
    if(danger) speed = 60.0f;
    if(s.burning > 0.0f || s.morale <= 0.0f) speed = 70.0f;
    if(s.stance == Stance::PRONE) speed = 11.0f;
    if(s.stance == Stance::CROUCH) speed = 30.0f;
    if(!s.can_walk()) speed = 7.0f;
    if(s.hp < 50.0f) speed *= 0.75f;
    if(m_map.in_wire(s.pos)) speed *= 0.35f;
    if(s.role == Role::MEDIC && s.goal != s.post) speed = std::max(speed, 55.0f);
    if(s.dragging >= 0) speed = 20.0f;

    sf::Vector2f const want = unit(next - s.pos) * std::min(speed, length(next - s.pos) * 6.0f);
    s.vel += (want - s.vel) * std::min(1.0f, dt * 8.0f);
}

void Battle::act(Soldier& s, float dt)
{
    const WeaponSpec& w = weapon_of(s.role);
    s.firing = false;

    // --- stance ------------------------------------------------------------------
    bool const moving = length(s.goal - s.pos) > 3.0f;
    bool const pinned = s.suppression > 0.75f && s.burning <= 0.0f;
    bool const danger = s.threat_age < 6.0f;
    Stance want = Stance::STAND;
    if(s.sitting) want = Stance::CROUCH;
    else if(!s.can_walk()) want = Stance::PRONE;
    else if(pinned) want = Stance::PRONE;
    else if(s.burning > 0.0f || s.morale <= 0.0f) want = Stance::STAND;
    else if(moving && !(s.target >= 0 && s.role == Role::RIFLEMAN && s.team == Team::RECHIVII)) want = (s.target < 0 && !danger) || s.suppression < 0.35f ? Stance::STAND : Stance::CROUCH;
    else if(s.role == Role::GUNNER && s.deployed) want = m_map.in_trench(s.pos) || m_map.under_roof(s.pos) ? Stance::CROUCH : Stance::PRONE;
    else if(danger || s.target >= 0) want = s.suppression > 0.4f ? Stance::PRONE : Stance::CROUCH;
    if(!s.has_post && !s.reserve && s.team == Team::RECHIVII && m_stage == Stage::SCOUT && s.suppression < 0.5f) want = Stance::PRONE;
    if(want != s.stance && s.stance_change <= 0.0f)
    {
        if(want == Stance::PRONE) s.body_angle = moving ? angle_of(s.goal - s.pos) : s.aim;
        s.stance = want;
        s.stance_change = want == Stance::PRONE ? 0.5f : 0.25f;
    }

    // A gun that has been told to move has to be packed up first.
    if(s.role == Role::GUNNER && (s.deployed || s.deploying > 0.0f) && length(s.goal - s.pos) > 10.0f && s.target < 0)
    {
        s.deployed = false;
        s.deploying = 0.0f;
    }

    steer(s, dt);

    // --- the machine gun goes down where it stops ------------------------------------
    if(s.role == Role::GUNNER && !moving && !s.deployed && s.deploying <= 0.0f && (danger || s.has_post || s.order == Order::SUPPRESS))
    {
        s.deploying = 1.2f;
        s.deploy_dir = s.target >= 0 ? angle_of(m_soldiers[static_cast<std::size_t>(s.target)].pos - s.pos)
                     : (s.order == Order::SUPPRESS ? angle_of(s.suppress_at - s.pos)
                     : (s.threat_age < 8.0f ? angle_of(s.threat - s.pos) : (s.team == Team::MONTERAN ? 0.0f : 3.14159f)));
        if(s.has_post && s.post.x > 1990 && s.post.x < 2070) s.deploy_dir = 3.14159f;   // the bunker gun covers its slit
        s.body_angle = s.deploy_dir;
    }

    // --- aim ------------------------------------------------------------------------------
    sf::Vector2f aim_at{};
    bool has_aim = false;
    bool suppressing = false;
    if(s.target >= 0)
    {
        const Soldier& t = m_soldiers[static_cast<std::size_t>(s.target)];
        aim_at = t.pos + t.vel * 0.15f;
        has_aim = true;
    }
    else if(s.order == Order::SUPPRESS && m_time < s.suppress_until && s.team == Team::MONTERAN)
    {
        aim_at = s.suppress_at + sf::Vector2f{std::sin(m_time * 1.3f + s.id) * 14.0f, std::cos(m_time * 1.7f) * 14.0f};
        has_aim = suppressing = true;
    }
    else if(s.threat_age < 4.0f && (s.role == Role::GUNNER || Random::chance(0.002f)) && length(s.threat - s.pos) < w.range)
    {
        aim_at = s.threat + sf::Vector2f{std::sin(m_time * 2.1f + s.id) * 12.0f, std::cos(m_time * 1.4f + s.id) * 12.0f};
        has_aim = suppressing = s.role == Role::GUNNER;
    }
    else if(moving && length(s.vel) > 2.0f)
    {
        aim_at = s.pos + s.vel;
        has_aim = true;
    }
    else if(s.threat_age < 10.0f)
    {
        aim_at = s.threat;
        has_aim = true;
    }

    if(has_aim)
    {
        float wantd = angle_of(aim_at - s.pos);
        if(s.deployed)
        {
            float const diff = std::remainder(wantd - s.deploy_dir, 6.2831853f);
            wantd = s.deploy_dir + std::clamp(diff, -0.85f, 0.85f);
        }
        float const diff = std::remainder(wantd - s.aim, 6.2831853f);
        float const turn = (s.stance == Stance::PRONE ? 3.5f : 6.0f) * dt;
        s.aim += std::clamp(diff, -turn, turn);
    }

    // --- trigger ---------------------------------------------------------------------------------
    if(!has_aim || s.reaction > 0.0f || s.reloading > 0.0f || s.treating >= 0 || s.sitting) return;
    if(s.burning > 0.0f || s.morale <= 0.0f) return;
    if(!(s.can_use_rifle() || s.role == Role::MEDIC || s.role == Role::OFFICER)) return;
    if(s.target < 0 && !suppressing) return;

    float const d = length(aim_at - s.pos);
    float const off = std::abs(std::remainder(angle_of(aim_at - s.pos) - s.aim, 6.2831853f));
    if(off > 0.12f || d > w.range * 0.95f) return;
    if(pinned && !Random::chance(0.04f)) return;                    // heads down; the odd blind shot
    if(s.ammo <= 0) { reload(s); return; }

    // Don't shoot through your own men.
    for(const auto& o : m_soldiers)
        if(o.team == s.team && o.id != s.id && o.alive() && length(o.pos - s.pos) < d
           && distance_to_segment(o.pos, s.pos, aim_at) < 5.0f) return;

    if(w.flame)
    {
        s.firing = d < 72.0f;
        return;
    }
    if(s.role == Role::GUNNER)
    {
        if(!s.deployed) return;
        if(s.heat > 0.78f) s.burst_left = 0;
        if(s.burst_left <= 0)
        {
            if(s.cooldown > 0.0f || s.heat > 0.45f) return;
            s.burst_left = Random::integer(7, 14);
            s.sway = Random::normal(0.0f, 0.045f + 0.06f * s.suppression + d / 9000.0f);
        }
        int const before = s.shots_fired;
        s.firing = true;
        if(s.cooldown <= 0.0f)
        {
            float const keep = s.aim;
            s.aim += s.sway + Random::normal(0.0f, 0.012f);
            fire(s, dt);
            s.aim = keep;
            s.firing = false;
            if(s.shots_fired != before && --s.burst_left <= 0) s.cooldown = Random::real(0.9f, 1.9f);
        }
        return;
    }
    if(w.automatic)
    {
        if(s.burst_left <= 0)
        {
            if(s.cooldown > 0.0f) return;
            s.burst_left = Random::integer(3, 7);
            s.sway = Random::normal(0.0f, 0.035f + 0.05f * s.suppression);
        }
        int const before = s.shots_fired;
        s.firing = true;
        if(s.cooldown <= 0.0f)
        {
            float const keep = s.aim;
            s.aim += s.sway + Random::normal(0.0f, 0.015f);
            fire(s, dt);
            s.aim = keep;
            s.firing = false;
            if(s.shots_fired != before && --s.burst_left <= 0) s.cooldown = Random::real(0.4f, 1.0f);
        }
        return;
    }
    // Bolt action and pistol: take a moment over each shot.
    if(s.cooldown <= 0.0f && length(s.vel) < 10.0f && Random::chance(dt * 2.2f))
    {
        // Their aim is not ours: a little wander on every shot.
        float const keep = s.aim;
        float const mover = s.target >= 0 && length(m_soldiers[static_cast<std::size_t>(s.target)].vel) > 20.0f ? 0.03f : 0.0f;
        s.aim += Random::normal(0.0f, 0.035f + 0.05f * s.suppression + (d > 250.0f ? 0.02f : 0.0f) + mover);
        s.firing = true;
        fire(s, dt);
        s.aim = keep;
        s.firing = false;
        s.cooldown += Random::real(0.2f, 1.1f);
    }
}

}
