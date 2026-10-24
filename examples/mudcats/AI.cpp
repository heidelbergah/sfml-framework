/**
 * AI.cpp  -  Mud Cats
 *
 * Everyone who is not you. Each man thinks a few times a second and acts
 * every frame.
 *
 * Thinking picks a target and a place to be. The place is usually a cover
 * point: behind sandbags on the side away from the enemy, in a trench, in
 * a shell hole, behind a tree or a hedge. Acting walks him there, turns
 * him, sets his stance and pulls the trigger.
 *
 * A Monteran's place comes from his section: the corporal is told where
 * the section goes (Battle.cpp), and his men find cover around that spot.
 * A Rechivii mostly holds where he was put, until he is told to fall back
 * or counter-attack.
 *
 * What makes it play like a firefight rather than a shooting gallery is
 * mostly suppression: a man who has had rounds snapping past him lies
 * down and stops moving, and fires blind if at all. So the way to take a
 * position is the old one: pin it with fire, and move someone else up
 * while its head is down.
 *
 * And what makes the fields different is seeing. In mist, in tall grass,
 * under trees, a man lying still can't be picked out until he gives
 * himself away by firing.
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

// ---------------------------------------------------------------- seeing

bool Battle::fighting_started() const
{
    if(m_level == LevelId::TRENCHES) return m_stage >= Stage::ASSAULT && m_stage <= Stage::COUNTER;
    if(m_level == LevelId::RAID) return m_alarm && m_stage >= Stage::RAID_CREEP && m_stage <= Stage::COUNTER;
    if(m_level == LevelId::HILL) return m_stage == Stage::HILL_WAVE || (m_stage == Stage::HILL_LULL && m_wave > 0);
    return (m_stage >= Stage::CONTACT && m_stage <= Stage::MILL) || m_stage == Stage::COUNTER;
}

int Battle::player_section() const
{
    return me().section;
}

float Battle::lit(sf::Vector2f p) const
{
    // How well a spot is lit, 0 to 1: flares, fires, muzzle flashes,
    // explosions. Only matters at night.
    float best = 0.0f;
    for(const auto& l : m_lights)
    {
        float const d = std::hypot(l.pos.x - p.x, l.pos.y - p.y);
        if(d < l.radius) best = std::max(best, (1.0f - d / l.radius) * std::min(1.0f, l.life / std::max(0.001f, l.max_life) * 1.5f));
    }
    return std::min(1.0f, best);
}

sf::Vector2f Battle::objective_point() const
{
    if(m_level == LevelId::HILL) return m_map.crest;
    return m_level == LevelId::TRENCHES ? sf::Vector2f{2320.0f, 380.0f} : sf::Vector2f{1815.0f, 482.0f};
}

bool Battle::through_smoke(sf::Vector2f a, sf::Vector2f b) const
{
    for(const auto& sm : m_smokes)
        if(sm.radius > 10.0f && distance_to_segment(sm.pos, a, b) < sm.radius * 0.85f) return true;
    return false;
}

bool Battle::sight(sf::Vector2f a, sf::Vector2f b) const
{
    return m_map.line_of_sight(a, b) && !through_smoke(a, b);
}

float Battle::vision_range() const
{
    if(m_level == LevelId::TRENCHES) return 420.0f;             // rain
    if(m_level == LevelId::RAID) return m_alarm ? 150.0f : 95.0f;   // the dark, and men not yet looking
    if(m_level == LevelId::HILL) return 360.0f - 210.0f * m_night;  // the afternoon going
    return 170.0f + 300.0f * (1.0f - m_mist);                   // the mist lifts
}

bool Battle::can_spot(const Soldier& viewer, const Soldier& target, float d) const
{
    float range = vision_range() * (target.revealed > 0.0f ? 1.35f : 1.0f);
    // From higher up you see further, and a man below you has less to hide behind.
    if(m_level == LevelId::HILL) range *= std::clamp(1.0f + 0.35f * (m_map.height(viewer.pos) - m_map.height(target.pos)), 0.88f, 1.2f);
    if(m_level == LevelId::RAID || (m_level == LevelId::HILL && m_night > 0.25f))
    {
        // At night what matters is light: a man under a flare, or behind
        // his own muzzle flash, can be seen from a long way off.
        float const light = std::max(lit(target.pos), target.revealed > 0.0f ? 0.8f : 0.0f);
        // On the hill the dark comes on slowly, and the slope is long: less in it.
        range *= 1.0f + (m_level == LevelId::HILL ? 1.0f * std::min(1.0f, m_night * 1.4f) : 1.8f) * light;
        // Men keeping low in the dark are hard to make out; stormtroopers,
        // who are trained to it, harder still.
        if(!m_alarm && target.stance != Stance::STAND) range *= target.role == Role::STORMTROOPER ? 0.6f : 0.75f;
    }
    if(d > range) return false;
    // A sniper lying still is a heap of rag and grass.
    if(target.role == Role::SNIPER && target.stance == Stance::PRONE && target.revealed <= 0.0f && d > 90.0f) return false;

    if(target.revealed <= 0.0f && target.alive())
    {
        bool const low = target.stance == Stance::PRONE;
        bool const hidden = m_map.in_grass(target.pos) || m_map.under_canopy(target.pos);
        if(hidden && low && d > 55.0f) return false;
        if(hidden && target.stance == Stance::CROUCH && d > 130.0f) return false;
        // From outside the trees, you can't see far into them.
        if(m_map.under_canopy(target.pos) && !m_map.under_canopy(viewer.pos) && d > 160.0f) return false;
        if(d > 230.0f && (low || (target.stance == Stance::CROUCH && m_map.in_trench(target.pos)))) return false;
        if(d > 300.0f && target.stance == Stance::CROUCH) return false;
    }
    return sight(viewer.pos, target.pos);
}

float Battle::enemy_accuracy() const
{
    static const float scale[] = {1.7f, 1.35f, 1.05f};
    return scale[std::clamp(Campaign::current().difficulty, 0, 2)];
}

float Battle::spread_of(const Soldier& s) const
{
    const WeaponSpec& w = s.weapon();
    float spread = w.spread;
    if(s.stance == Stance::CROUCH) spread *= 0.75f;
    if(s.stance == Stance::PRONE)  spread *= 0.55f;
    if(length(s.vel) > 8.0f) spread *= 2.4f;
    spread *= 1.0f + 2.2f * s.suppression + 1.5f * s.shock;
    if(s.is_player && m_aiming) spread *= s.gun == Gun::SNIPER ? 0.25f : 0.45f;
    if(s.lost & (ARM_L | ARM_R)) spread *= 2.0f;
    if(s.team == Team::RECHIVII) spread *= enemy_accuracy();    // defenders shooting at men who keep moving
    return spread + s.kick;
}

// ---------------------------------------------------------------- choosing

int Battle::find_target(const Soldier& s) const
{
    const WeaponSpec& w = s.weapon();
    if(s.gun == Gun::NONE) return -1;
    float const range = w.flame ? 200.0f : std::min(w.range, 430.0f);

    int best = -1;
    float best_score = 1e9f;
    for(const auto& e : m_soldiers)
    {
        if(e.team == s.team || !e.fighting() || (!e.active && !e.is_player)) continue;
        float const d = length(e.pos - s.pos);
        if(d > range) continue;
        if(!can_spot(s, e, d)) continue;
        float score = d;
        if(e.has_mg() || e.has_flamer()) score *= 0.75f;
        if(e.role == Role::RADIOMAN) score *= 0.6f;             // shoot the man with the aerial
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
        if(std::abs(c.pos.x - around.x) > radius || std::abs(c.pos.y - around.y) > radius) continue;
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
        if(m_map.in_water(c.pos)) score += 60.0f;

        if(score < best_score) { best_score = score; best = static_cast<int>(i); }
    }
    return best;
}

void Battle::set_goal(Soldier& s, sf::Vector2f goal, float radius)
{
    sf::Vector2f const forward = s.team == Team::MONTERAN ? sf::Vector2f{260.0f, 0.0f} : sf::Vector2f{-260.0f, 0.0f};
    sf::Vector2f const threat = s.threat_age < 8.0f ? s.threat : goal + forward;
    int const c = pick_cover(s, goal, radius, threat);
    s.cover_index = c;
    s.goal = c >= 0 ? m_map.cover[static_cast<std::size_t>(c)].pos : goal;
    s.path.clear();
    s.repath = 0.0f;
}

void Battle::section_goal(Soldier& s)
{
    // Who he follows: his corporal, or if he is the corporal, whoever leads.
    Soldier* lead = section_leader(s.section);
    if(!lead || lead->id == s.id)
    {
        lead = officer(Team::MONTERAN);
        if(!lead || lead->id == s.id) lead = player();
    }
    // "On me!" from you means you.
    if(player_leads()) lead = player();
    if(!lead || lead->id == s.id || !lead->alive()) return;

    // A loose file behind him.
    int slot = 0;
    for(const auto& o : m_soldiers)
    {
        if(o.id == s.id) break;
        if(o.team == Team::MONTERAN && o.alive() && o.id != lead->id && (o.section == s.section || lead == player())) ++slot;
    }
    float const face = length(lead->vel) > 5.0f ? angle_of(lead->vel) : lead->aim;
    sf::Vector2f const back = -dir(face), side{-std::sin(face), std::cos(face)};
    sf::Vector2f const spot = lead->pos + back * (16.0f + 10.0f * static_cast<float>(slot / 2)) + side * ((slot % 2 ? 1.0f : -1.0f) * 12.0f);
    if(length(spot - s.goal) > 26.0f)
    {
        if(s.threat_age < 5.0f) set_goal(s, spot, 40.0f);
        else { s.goal = spot; s.path.clear(); s.cover_index = -1; }
    }
}

void Battle::medic_logic(Soldier& s)
{
    Soldier* patient = nullptr;
    float best = 320.0f;
    for(auto& o : m_soldiers)
    {
        if(o.team != s.team || o.id == s.id || o.surrendered) continue;
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

bool Battle::scavenge(Soldier& s)
{
    const WeaponSpec& w = s.weapon();
    bool const empty = s.gun == Gun::NONE;
    bool const low = !empty && !w.flame && s.ammo + s.spare < w.magazine;
    bool const dry = w.flame && s.fuel < 10.0f;
    if(!empty && !low && !dry) { s.want_pickup = -1; return false; }
    if(s.suppression > 0.7f) return false;

    // A weapon, or rounds for his own, lying near.
    int best = -1;
    float best_d = 130.0f;
    for(std::size_t i = 0; i < m_dropped.size(); ++i)
    {
        const Dropped& d = m_dropped[i];
        if(d.taken) continue;
        bool const useful = empty ? (d.gun != Gun::FLAMER && d.gun != Gun::MG) || s.role == Role::GUNNER
                                  : d.gun == s.gun && (d.ammo + d.spare > 0 || d.fuel > 5.0f);
        if(!useful) continue;
        if(!(d.gun == Gun::PISTOL || d.gun == Gun::SMG) && !s.can_use_rifle()) continue;
        float const l = length(d.pos - s.pos);
        if(l < best_d) { best_d = l; best = static_cast<int>(i); }
    }
    if(best >= 0)
    {
        Dropped& d = m_dropped[static_cast<std::size_t>(best)];
        if(best_d < 8.0f)
        {
            if(pick_up(s, d))
                bark(s, s.team == Team::MONTERAN ? "Got some! Got some rounds!" : "Est!", false);
            s.want_pickup = -1;
            return false;
        }
        if(s.want_pickup != best)
        {
            s.want_pickup = best;
            s.goal = d.pos;
            s.path.clear();
            s.cover_index = -1;
            if(s.team == Team::MONTERAN) bark(s, "I'm out! Need ammunition!", true);
        }
        return true;
    }

    // Or a crate.
    if(s.team == Team::MONTERAN && !empty)
        for(auto c : m_map.crates)
        {
            float const l = length(c - s.pos);
            if(l > 220.0f) continue;
            if(l < 10.0f)
            {
                s.spare = std::max(s.spare, w.spare);
                if(w.flame) s.fuel = 100.0f;
                SoundBank::get().play_at("ammo_pickup", s.pos, 0.6f);
                return false;
            }
            s.goal = c;
            s.path.clear();
            s.cover_index = -1;
            return true;
        }
    return false;
}

// ---------------------------------------------------------------- thinking

void Battle::think(Soldier& s, float dt)
{
    s.think -= dt;
    s.reaction -= dt;
    s.repath -= dt;
    if(s.surrendered)
    {
        s.goal = s.pos;
        s.target = -1;
        return;
    }
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
            {
                static const char* const ours[] = {"Contact!", "There! Front!", "Movement, there!", "I see one!"};
                bark(s, s.team == Team::MONTERAN ? ours[Random::integer(0, 3)] : "Kesh! Kesh!", true);
            }
        }
        s.target = target;
    }
    if(target >= 0)
    {
        s.threat = m_soldiers[static_cast<std::size_t>(target)].pos;
        s.threat_age = 0.0f;
    }
    if(target >= 0 && !m_alarm && s.team == Team::RECHIVII)
    {
        bark(s, "Tam! Tam! Trevoga!", true);
        raise_alarm("A sentry saw us!");
    }

    // --- grenades landing nearby: get away from them --------------------------------
    for(const auto& g : m_grenades)
    {
        if(g.height > 3.0f || g.smoke) continue;
        float const d = length(g.pos - s.pos);
        if(d < 36.0f && !(s.has_mg() && s.deployed))
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
        // To the water, if there is any close; otherwise just run.
        sf::Vector2f wet{};
        float best = 90.0f;
        for(const auto& w : m_map.water)
        {
            sf::Vector2f const c{std::clamp(s.pos.x, w.position.x, w.position.x + w.size.x), std::clamp(s.pos.y, w.position.y, w.position.y + w.size.y)};
            if(length(c - s.pos) < best) { best = length(c - s.pos); wet = c; }
        }
        s.goal = best < 90.0f ? wet : s.pos + dir(Random::real(0.0f, 6.283f)) * 40.0f;
        s.path.clear();
        return;
    }
    if(s.team == Team::RECHIVII && s.morale <= 0.0f)
    {
        bool const homeward = s.home.x != 0.0f || s.home.y != 0.0f;
        s.goal = homeward ? s.home : sf::Vector2f{m_map.size.x - 4.0f, std::clamp(s.pos.y, 150.0f, m_map.size.y - 150.0f)};
        s.deployed = false;
        s.serving = -1;
        if(s.path.empty() || s.repath <= 0.0f) { s.path = m_map.find_path(s.pos, s.goal); s.repath = 3.0f; }
        if(homeward ? length(s.pos - s.home) < 24.0f : s.pos.x > m_map.size.x - 20.0f) { s.life = Life::FLED; s.fate = "Ran"; }
        return;
    }

    // --- grenades of their own -----------------------------------------------------------
    if(target >= 0 && s.grenades > 0 && s.suppression < 0.6f && Random::chance(s.role == Role::STORMTROOPER ? 0.14f : 0.07f))
    {
        const Soldier& t = m_soldiers[static_cast<std::size_t>(target)];
        float const d = length(t.pos - s.pos);
        bool const dug_in = m_map.in_trench(t.pos) || m_map.sandbag_between(s.pos, t.pos, 20.0f) || m_map.in_crater(t.pos) || m_map.under_roof(t.pos);
        if(d > 45.0f && d < 140.0f && dug_in)
            throw_grenade(s, t.pos + sf::Vector2f{Random::real(-10, 10), Random::real(-10, 10)});
    }

    // --- wire in the way: cut it ------------------------------------------------------------------
    if(s.cutting > 0.0f || s.digging > 0.0f) return;
    if(s.team == Team::MONTERAN && s.role == Role::SAPPER && !(m_level == LevelId::RAID && m_stage == Stage::RAID_CREEP))
    {
        sf::Vector2f pt;
        bool const ahead = s.goal.x > s.pos.x + 20.0f;
        if(ahead && m_map.near_wire(s.pos, 46.0f, &pt) && pt.x > s.pos.x - 4.0f && s.suppression < 0.7f)
        {
            if(length(pt - s.pos) < 10.0f)
            {
                s.cutting = s.role == Role::SAPPER ? 2.5f : 4.0f;
                s.cut_at = pt;
                s.vel = {};
                bark(s, "Cutting! Cover me!", false);
            }
            else if(length(s.goal - pt) > 6.0f && s.path.size() < 2)
            {
                s.goal = pt - sf::Vector2f{6.0f, 0.0f};
                s.path.clear();
                s.cover_index = -1;
            }
            return;
        }
    }

    // --- a weapon, or rounds for it ----------------------------------------------------------
    if(scavenge(s)) return;

    // --- a mate down beside you, and no medic near him: a field dressing ------------------------
    if(s.team == Team::MONTERAN && s.role != Role::MEDIC && s.charge_until <= m_time)
    {
        if(s.treating >= 0) { s.goal = s.pos; return; }
        if(s.bandages > 0 && s.suppression < 0.5f)
            for(auto& o : m_soldiers)
            {
                if(o.team != s.team || o.life != Life::DOWNED || o.bleed_out < 2.0f || o.dragged_by >= 0) continue;
                float const d = length(o.pos - s.pos);
                if(d > 40.0f) continue;
                bool covered = false;
                for(const auto& m : m_soldiers)
                    if(m.id != s.id && (m.treating == o.id || (m.role == Role::MEDIC && m.alive() && m.medkits > 0 && length(m.pos - o.pos) < 120.0f))) covered = true;
                if(covered) continue;
                if(d < 10.0f)
                {
                    s.treating = o.id;
                    s.treat_time = 6.0f;
                    s.goal = s.pos;
                    s.stance = Stance::CROUCH;
                    SoundBank::get().play_at("bandage", s.pos, 0.6f);
                    bark(s, "Hold on. Dressing on you, hold on.", false);
                }
                else
                {
                    s.goal = o.pos;
                    s.path.clear();
                    s.cover_index = -1;
                }
                return;
            }
    }

    // --- where to be --------------------------------------------------------------------------
    if(s.role == Role::MEDIC)
    {
        medic_logic(s);
        if(s.treating >= 0 || length(s.goal - s.pos) > 12.0f) return;
    }

    // In a charge, straight at them.
    if(s.charge_until > m_time)
    {
        if(target >= 0)
        {
            const Soldier& t = m_soldiers[static_cast<std::size_t>(target)];
            if(length(t.pos - s.pos) < 90.0f) { s.goal = t.pos; s.path.clear(); s.cover_index = -1; }
        }
        return;
    }

    if(s.team == Team::MONTERAN)
    {
        if(s.order == Order::FOLLOW) section_goal(s);
        // Back to where he was put, once he has been to the dump or to a mate.
        else if(m_level == LevelId::HILL && s.has_post && length(s.goal - s.post) > 1.0f && s.want_pickup < 0)
        {
            s.goal = s.post;
            s.path.clear();
            s.cover_index = -1;
        }
        return;
    }

    // --- Rechivii ---------------------------------------------------------------------------------
    // A sniper who has given himself away moves: shoot, and somewhere else.
    if(s.has_post && s.role == Role::SNIPER && s.revealed > 3.0f && s.revealed < 3.4f && Random::chance(0.5f))
    {
        int const c = pick_cover(s, s.post, 70.0f, s.threat_age < 10.0f ? s.threat : objective_point());
        if(c >= 0 && length(m_map.cover[static_cast<std::size_t>(c)].pos - s.post) > 20.0f) s.post = m_map.cover[static_cast<std::size_t>(c)].pos;
    }
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
    if(!s.reserve && m_level == LevelId::TRENCHES)
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

    // The counter-attack: bound from cover to cover toward us.
    if(length(s.goal - s.pos) < 6.0f || s.goal == s.pos)
    {
        s.idle_anim += 0.3f;
        if(s.idle_anim > Random::real(2.0f, 5.0f) || s.goal == sf::Vector2f{})
        {
            s.idle_anim = 0.0f;
            sf::Vector2f aim_for = objective_point();
            float best = 1e9f;
            for(const auto& o : m_soldiers)
                if(o.team == Team::MONTERAN && o.alive() && length(o.pos - s.pos) < best) { best = length(o.pos - s.pos); aim_for = o.pos; }
            bool const storm = s.role == Role::STORMTROOPER || s.role == Role::FLAMER;
            if(best > (storm ? 50.0f : 70.0f))
            {
                sf::Vector2f const hop = s.pos + unit(aim_for - s.pos) * std::min(storm ? 120.0f : 90.0f, best - (storm ? 25.0f : 40.0f));
                int const c = pick_cover(s, hop, 60.0f, aim_for);
                s.cover_index = c;
                s.goal = c >= 0 ? m_map.cover[static_cast<std::size_t>(c)].pos : hop;
                s.path.clear();
                if(Random::chance(0.3f)) bark(s, "Toven! Toven!", true);
            }
            else if((best < 30.0f && s.gun == Gun::RIFLE && Random::chance(0.5f)) || (storm && best < 50.0f && Random::chance(0.6f)))
            {
                s.charge_until = m_time + 4.0f;
                s.goal = aim_for;
                s.path.clear();
                bark(s, "Urraaa!", true);
            }
        }
    }
}

// ---------------------------------------------------------------- acting

void Battle::steer(Soldier& s, float dt)
{
    sf::Vector2f const to_goal = s.goal - s.pos;
    float const dist = length(to_goal);

    bool const charging = s.charge_until > m_time;
    bool const pinned = s.suppression > 0.75f && s.burning <= 0.0f && s.morale > 0.0f && !charging;
    bool const busy = s.treating >= 0 || (s.has_mg() && (s.deployed || s.deploying > 0.0f));
    bool const waiting = m_time < s.hold_until || s.cutting > 0.0f || s.digging > 0.0f;
    if(dist < 3.0f || pinned || busy || waiting || s.sitting || s.surrendered || s.lunge > 0.0f)
    {
        if(s.lunge <= 0.0f) s.vel *= std::exp(-dt * 10.0f);
        return;
    }

    if(s.path.empty() || s.repath <= 0.0f)
    {
        if(dist > 20.0f || !m_map.walkable(s.goal)) s.path = m_map.find_path(s.pos, s.goal);
        else s.path = {s.goal};
        s.repath = charging ? 0.8f : 2.5f;
        if(s.path.empty()) s.path = {s.goal};
    }

    sf::Vector2f next = s.path.front();
    if(length(next - s.pos) < 4.0f && s.path.size() > 1)
    {
        s.path.erase(s.path.begin());
        next = s.path.front();
    }

    float speed = 40.0f;
    bool const danger = s.threat_age < 6.0f || fighting_started();
    if(danger) speed = 60.0f;
    if(s.burning > 0.0f || s.morale <= 0.0f) speed = 70.0f;
    if(s.stance == Stance::PRONE) speed = 11.0f;
    if(s.stance == Stance::CROUCH) speed = 30.0f;
    if(charging) speed = 72.0f;
    if(!s.can_walk()) speed = 7.0f;
    if(s.hp < 50.0f) speed *= 0.75f;
    if(m_map.in_wire(s.pos)) speed *= 0.35f;
    if(m_map.in_water(s.pos)) speed *= 0.5f;
    if(s.role == Role::MEDIC && s.goal != s.post) speed = std::max(speed, 55.0f);
    if(s.dragging >= 0) speed = 20.0f;
    if(!fighting_started() && m_level == LevelId::FIELDS && s.team == Team::MONTERAN) speed = std::min(speed, 30.0f);    // on the march
    if(m_level == LevelId::HILL)
    {
        // Climbing is slow work.
        float const climb = m_map.height(s.pos + unit(next - s.pos) * 6.0f) - m_map.height(s.pos);
        if(climb > 0.0f) speed *= 1.0f - std::min(0.22f, climb * 18.0f);
    }

    sf::Vector2f const want = unit(next - s.pos) * std::min(speed, length(next - s.pos) * 6.0f);
    s.vel += (want - s.vel) * std::min(1.0f, dt * 8.0f);
}

void Battle::act(Soldier& s, float dt)
{
    const WeaponSpec& w = s.weapon();
    s.firing = false;
    if(s.surrendered)
    {
        s.vel *= std::exp(-dt * 10.0f);
        s.stance = Stance::STAND;
        return;
    }
    if(s.lunge > 0.0f) return;

    // --- stance ------------------------------------------------------------------
    bool const charging = s.charge_until > m_time;
    bool const moving = length(s.goal - s.pos) > 3.0f;
    bool const pinned = s.suppression > 0.75f && s.burning <= 0.0f && !charging;
    bool const danger = s.threat_age < 6.0f;
    Stance want = Stance::STAND;
    if(s.sitting) want = Stance::CROUCH;
    else if(!s.can_walk()) want = Stance::PRONE;
    else if(charging) want = Stance::STAND;
    else if(pinned) want = Stance::PRONE;
    else if(s.burning > 0.0f || s.morale <= 0.0f) want = Stance::STAND;
    else if(moving && !(s.target >= 0 && s.gun == Gun::RIFLE && s.team == Team::RECHIVII)) want = (s.target < 0 && !danger) || s.suppression < 0.35f ? Stance::STAND : Stance::CROUCH;
    else if(s.has_mg() && s.deployed) want = m_map.in_trench(s.pos) || m_map.under_roof(s.pos) ? Stance::CROUCH : Stance::PRONE;
    else if(danger || s.target >= 0)
    {
        // In cover he can kneel to shoot over it. In the open, he lies down.
        bool const covered = s.cover_index >= 0 || m_map.in_trench(s.pos) || m_map.in_crater(s.pos) || m_map.under_roof(s.pos);
        want = (s.suppression > 0.4f || !covered) ? Stance::PRONE : Stance::CROUCH;
    }
    // Up the hill under fire, they keep low.
    if(m_level == LevelId::HILL && s.team == Team::RECHIVII && s.wave >= 0 && moving && want == Stance::STAND && !charging && s.morale > 0.0f
       && length(s.pos - m_map.crest) < 470.0f)
        want = Stance::CROUCH;
    // Creeping up in the dark: low when moving, flat when not.
    if(m_level == LevelId::RAID && !m_alarm && s.team == Team::MONTERAN) want = moving ? Stance::CROUCH : Stance::PRONE;
    if(s.cutting > 0.0f) want = Stance::CROUCH;
    if(s.digging > 0.0f) want = s.dig_hole ? Stance::PRONE : Stance::CROUCH;
    // A sniper lies up; so do a mortar's crew, by the tube.
    if(s.team == Team::RECHIVII && s.has_post && !moving && s.morale > 0.0f && (s.role == Role::SNIPER || s.serving >= 0)) want = s.serving >= 0 ? Stance::CROUCH : Stance::PRONE;
    if(m_level == LevelId::TRENCHES && !s.has_post && !s.reserve && s.team == Team::RECHIVII && m_stage == Stage::SCOUT && s.suppression < 0.5f) want = Stance::PRONE;
    // The patrol in the wood waits lying in the trees.
    if(m_level == LevelId::FIELDS && s.team == Team::RECHIVII && !s.active) want = Stance::PRONE;
    if(want != s.stance && s.stance_change <= 0.0f)
    {
        if(want == Stance::PRONE) s.body_angle = moving ? angle_of(s.goal - s.pos) : s.aim;
        s.stance = want;
        s.stance_change = want == Stance::PRONE ? 0.5f : 0.25f;
    }

    // A gun that has been told to move has to be packed up first.
    if(s.has_mg() && (s.deployed || s.deploying > 0.0f) && length(s.goal - s.pos) > 10.0f && s.target < 0)
    {
        s.deployed = false;
        s.deploying = 0.0f;
    }

    steer(s, dt);

    // --- the machine gun goes down where it stops ------------------------------------
    if(s.has_mg() && !moving && !s.deployed && s.deploying <= 0.0f && (danger || s.has_post || s.order == Order::SUPPRESS))
    {
        s.deploying = 1.2f;
        s.deploy_dir = s.target >= 0 ? angle_of(m_soldiers[static_cast<std::size_t>(s.target)].pos - s.pos)
                     : (s.order == Order::SUPPRESS && s.team == Team::MONTERAN ? angle_of(s.suppress_at - s.pos)
                     : (s.threat_age < 8.0f ? angle_of(s.threat - s.pos) : (s.team == Team::MONTERAN ? 0.0f : 3.14159f)));
        if(m_level == LevelId::TRENCHES && s.has_post && s.post.x > 1990 && s.post.x < 2070) s.deploy_dir = 3.14159f;   // the bunker gun covers its slit
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
        aim_at = s.suppress_at + sf::Vector2f{std::sin(m_time * 1.3f + static_cast<float>(s.id)) * 14.0f, std::cos(m_time * 1.7f) * 14.0f};
        has_aim = suppressing = true;
    }
    else if(s.threat_age < 4.0f && (s.has_mg() || Random::chance(0.002f)) && length(s.threat - s.pos) < w.range)
    {
        aim_at = s.threat + sf::Vector2f{std::sin(m_time * 2.1f + static_cast<float>(s.id)) * 12.0f, std::cos(m_time * 1.4f + static_cast<float>(s.id)) * 12.0f};
        has_aim = suppressing = s.has_mg();
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

    // --- cold steel ----------------------------------------------------------------------------
    if(s.target >= 0 && s.bayonet_cd <= 0.0f && s.burning <= 0.0f)
    {
        const Soldier& t = m_soldiers[static_cast<std::size_t>(s.target)];
        float const d = length(t.pos - s.pos);
        bool const can = !s.has_mg() && !s.has_flamer() && (s.can_use_rifle() || s.one_handed());
        bool const close = d < 13.0f && std::abs(std::remainder(angle_of(t.pos - s.pos) - s.aim, 6.2831853f)) < 0.5f;
        if(can && close && (charging || s.ammo <= 0 || s.gun == Gun::RIFLE || Random::chance(0.4f)))
        {
            bayonet(s);
            return;
        }
    }

    // --- trigger ---------------------------------------------------------------------------------
    // The weapon comes up when there is something to shoot at, and goes
    // down again when there isn't. It has to be up to fire.
    {
        bool const want_up = (has_aim && (s.target >= 0 || suppressing)) || s.deployed || s.deploying > 0.0f || (s.threat_age < 2.5f && !moving);
        s.ready += ((want_up ? 1.0f : 0.0f) - s.ready) * std::min(1.0f, dt * 6.0f);
    }
    if(!has_aim || s.reaction > 0.0f || s.reloading > 0.0f || s.treating >= 0 || s.sitting || s.cutting > 0.0f || s.digging > 0.0f) return;
    // A mortar's crew serves the tube, and only reaches for a rifle if we get close.
    if(s.serving >= 0 && length(aim_at - s.pos) > 140.0f) return;
    if(s.ready < 0.75f) return;
    // On the raid, nobody fires until they know we're there.
    if(m_level == LevelId::RAID && !m_alarm && s.team == Team::MONTERAN) return;
    if(s.burning > 0.0f || s.morale <= 0.0f) return;
    if(!s.can_shoot() && !s.has_flamer()) return;
    if(s.target < 0 && !suppressing) return;
    if(charging && length(aim_at - s.pos) > 45.0f && s.gun == Gun::RIFLE) return;     // keep running

    float const d = length(aim_at - s.pos);
    float const off = std::abs(std::remainder(angle_of(aim_at - s.pos) - s.aim, 6.2831853f));
    if(off > 0.12f || d > w.range * 0.95f) return;
    if(pinned && !Random::chance(0.04f)) return;                    // heads down; the odd blind shot
    if(s.ammo <= 0 && !w.flame) { reload(s); return; }

    // Don't shoot through your own men.
    for(const auto& o : m_soldiers)
        if(o.team == s.team && o.id != s.id && o.alive() && std::abs(o.pos.x - s.pos.x) < d + 5.0f && length(o.pos - s.pos) < d
           && distance_to_segment(o.pos, s.pos, aim_at) < 5.0f) return;

    if(w.flame)
    {
        s.firing = d < 72.0f;
        return;
    }
    if(s.has_mg())
    {
        if(!s.deployed) return;
        if(s.heat > 0.78f) s.burst_left = 0;
        if(s.burst_left <= 0)
        {
            if(s.cooldown > 0.0f || s.heat > 0.45f) return;
            s.burst_left = Random::integer(7, 14);
            float const night = m_level == LevelId::RAID && s.target >= 0 ? 1.8f - 0.8f * lit(m_soldiers[static_cast<std::size_t>(s.target)].pos) : 1.0f;
            // A machine gun on its bipod is a steady thing: the first rounds
            // of a burst are a little off, and the gunner walks the rest on.
            s.sway = Random::normal(0.0f, (0.02f + 0.04f * s.suppression + d / 14000.0f) * (s.team == Team::RECHIVII ? enemy_accuracy() : 1.0f) * night);
        }
        int const before = s.shots_fired;
        s.firing = true;
        if(s.cooldown <= 0.0f)
        {
            float const keep = s.aim;
            s.aim += s.sway + Random::normal(0.0f, 0.006f);
            fire(s, dt);
            s.aim = keep;
            s.firing = false;
            if(s.shots_fired != before) s.sway *= 0.82f;
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
    if(s.cooldown <= 0.0f && length(s.vel) < 10.0f && Random::chance(dt * (d > 220.0f ? 1.2f : 2.0f)))
    {
        // Their aim is not ours: a little wander on every shot.
        float const keep = s.aim;
        float const mover = s.target >= 0 && length(m_soldiers[static_cast<std::size_t>(s.target)].vel) > 20.0f ? 0.03f : 0.0f;
        // Hard to hit a man at range, harder still a man down in cover.
        const Soldier* t = s.target >= 0 ? &m_soldiers[static_cast<std::size_t>(s.target)] : nullptr;
        float const low = t && t->stance != Stance::STAND ? 0.02f : 0.0f;
        float const sniper = s.gun == Gun::SNIPER ? 0.3f : 1.0f;
        // In the dark, you shoot at a shape, unless a flare is over him.
        float const night = m_level == LevelId::RAID && t ? 1.7f - 0.7f * lit(t->pos) : 1.0f;
        // Men just shaken awake shoot wild for the first few seconds.
        float const groggy = m_level == LevelId::RAID && s.team == Team::RECHIVII && m_alarm
                           ? 1.0f + 0.8f * std::max(0.0f, 1.0f - (m_time - m_alarm_at) / 20.0f) : 1.0f;
        s.aim += Random::normal(0.0f, (0.03f + 0.07f * s.suppression + d / 5000.0f + mover + low) * sniper * night * groggy);
        s.firing = true;
        fire(s, dt);
        s.aim = keep;
        s.firing = false;
        s.cooldown += Random::real(0.2f, 1.1f);
    }
}

}
