/**
 * Combat.cpp  -  Mud Cats
 *
 * Weapons, bullets, wounds, gore, grenades, fire and shells.
 *
 * The rules, in short:
 *
 *   - Every round that passes close to a man frightens him (suppression),
 *     hit or miss. Frightened men aim worse and want to lie down.
 *   - Cover is checked when a round reaches its target: a trench, a
 *     sandbag just in front of him or a shell hole he is lying in may
 *     stop it, more likely the lower he is.
 *   - A hit lands somewhere: head, body, an arm or a leg. Heavy rounds and
 *     explosions can take a limb off. A man who goes down bleeds out
 *     unless a medic reaches him.
 *   - Explosions do not care about sandbags, only walls; fire does not
 *     care about either.
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
    sf::Vector2f unit(sf::Vector2f v) { float const l = length(v); return l > 1e-5f ? v / l : sf::Vector2f{1.0f, 0.0f}; }

    bool segment_circle(sf::Vector2f a, sf::Vector2f b, sf::Vector2f c, float r, sf::Vector2f* at)
    {
        sf::Vector2f const d = b - a, f = a - c;
        float const A = d.x * d.x + d.y * d.y;
        if(A < 1e-8f) return false;
        float const B = 2.0f * (f.x * d.x + f.y * d.y);
        float const C = f.x * f.x + f.y * f.y - r * r;
        float const disc = B * B - 4.0f * A * C;
        if(disc < 0.0f) return false;
        float const s = std::sqrt(disc);
        float t = (-B - s) / (2.0f * A);
        if(t < 0.0f || t > 1.0f) t = (-B + s) / (2.0f * A);
        if(t < 0.0f || t > 1.0f) return false;
        if(at) *at = a + d * t;
        return true;
    }

    const char* cause_name(int cause)
    {
        switch(cause)
        {
            case 0:  return "head";
            case 1:  return "body";
            default: return "limb";
        }
    }
}

// ---------------------------------------------------------------- shooting

void Battle::reload(Soldier& s)
{
    const WeaponSpec& w = weapon_of(s.role);
    if(w.flame || s.reloading > 0.0f || s.ammo >= w.magazine || s.spare <= 0) return;
    s.reloading = w.reload * (s.lost & (ARM_L | ARM_R) ? 2.0f : 1.0f);
    SoundBank::get().play_at(w.reload_sound, s.pos, s.is_player ? 0.8f : 0.5f);
    if(!s.is_player && Random::chance(0.4f)) bark(s, s.team == Team::MONTERAN ? "Reloading!" : "Vash!", false);
}

void Battle::fire(Soldier& s, float dt)
{
    const WeaponSpec& w = weapon_of(s.role);

    if(w.flame)
    {
        if(!s.firing || s.fuel <= 0.0f)
        {
            if(s.flash > 0.0f && s.is_player) SoundBank::get().play_at("flamer_stop", s.pos, 0.7f);
            s.flash = 0.0f;
            return;
        }
        if(s.flash <= 0.0f) SoundBank::get().play_at("flamer_start", s.pos, 0.9f);
        s.flash = 0.2f;
        s.fuel = std::max(0.0f, s.fuel - 13.0f * dt);
        s.cooldown -= dt;
        while(s.cooldown <= 0.0f)
        {
            s.cooldown += w.interval;
            float const a = s.aim + Random::normal(0.0f, w.spread);
            Flame f;
            f.pos = s.pos + dir(s.aim) * 8.0f;
            f.vel = dir(a) * w.speed * Random::real(0.8f, 1.2f) + s.vel * 0.5f;
            f.max_life = f.life = Random::real(0.4f, 0.55f);
            f.owner = s.id;
            f.team = s.team;
            m_flames.push_back(f);
        }
        m_lights.push_back({s.pos + dir(s.aim) * 20.0f, 34.0f, 0.06f, 0.06f, FIRE_ORANGE});
        hear_shot(s);
        if(length(s.pos - player()->pos) < 300.0f) m_combat_heat = std::min(1.0f, m_combat_heat + dt * 0.3f);
        return;
    }

    if(!s.firing) return;
    if(s.reloading > 0.0f || s.cooldown > 0.0f) return;
    if(s.role == Role::GUNNER && (!s.deployed || s.overheated > 0.0f)) return;

    if(s.ammo <= 0)
    {
        if(s.is_player) SoundBank::get().play("click_empty", 0.6f);
        s.cooldown = 0.35f;
        if(!s.is_player || s.spare > 0) reload(s);
        return;
    }

    // How steady is he?
    float spread = w.spread;
    if(s.stance == Stance::CROUCH) spread *= 0.75f;
    if(s.stance == Stance::PRONE)  spread *= 0.55f;
    if(length(s.vel) > 8.0f) spread *= 2.4f;
    spread *= 1.0f + 2.2f * s.suppression + 1.5f * s.shock;
    if(s.is_player && m_aiming) spread *= 0.45f;
    if(s.lost & (ARM_L | ARM_R)) spread *= 2.0f;
    if(s.team == Team::RECHIVII) spread *= 1.25f;     // defenders shooting at men who keep moving
    spread += s.kick;

    fire_bullet(s, s.aim + Random::normal(0.0f, spread));

    --s.ammo;
    ++s.shots_fired;
    s.cooldown = w.interval;
    s.kick += w.kick;
    s.flash = 0.06f;
    s.revealed = 2.5f;
    if(s.role == Role::GUNNER)
    {
        s.heat += 0.03f;
        if(s.heat >= 1.0f)
        {
            s.overheated = 4.0f;
            SoundBank::get().play_at("overheat", s.pos, 0.8f);
            if(s.is_player) hint("The barrel is too hot. Wait for it to cool. Fire in bursts.", 3.5f);
        }
    }

    m_lights.push_back({s.pos + dir(s.aim) * 9.0f, s.role == Role::GUNNER ? 26.0f : 20.0f, 0.05f, 0.05f, FIRE_YELLOW});
    for(int i = 0; i < 3; ++i)
        m_sparks.push_back({s.pos + dir(s.aim) * 9.0f, dir(s.aim + Random::real(-0.5f, 0.5f)) * Random::real(40.0f, 90.0f), 0.06f, FIRE_YELLOW});
    m_puffs.push_back({s.pos + dir(s.aim) * 10.0f, dir(s.aim) * 12.0f, 0.5f, 0.5f, 2.0f, 6.0f, with_alpha(SMOKE, 0.35f)});

    SoundBank::get().shot_at(w.sound, w.far_sound, s.pos, s.is_player ? 0.9f : 0.75f);
    if(s.role == Role::RIFLEMAN && s.is_player) m_later.push_back({0.45f, "rifle_bolt", s.pos, 0.6f});
    hear_shot(s);

    if(s.is_player) m_camera.shake(s.role == Role::GUNNER ? 0.05f : 0.09f);
    if(length(s.pos - player()->pos) < 380.0f) m_combat_heat = std::min(1.0f, m_combat_heat + 0.03f);

    // Bolt actions and pistols need a fresh pull each time; let go of it.
    if(!w.automatic) s.firing = false;
}

void Battle::fire_bullet(Soldier& s, float angle)
{
    const WeaponSpec& w = weapon_of(s.role);
    Bullet b;
    b.pos = b.origin = s.pos + dir(s.aim) * 7.0f;
    b.vel = dir(angle) * w.speed;
    b.range = w.range * Random::real(0.9f, 1.05f);
    b.damage = w.damage;
    b.suppress = w.suppress;
    b.owner = s.id;
    b.team = s.team;
    b.heavy = s.role == Role::GUNNER;
    b.tracer = w.tracer_every > 0 && s.shots_fired % w.tracer_every == 0;
    m_bullets.push_back(b);
}

void Battle::hear_shot(const Soldier& shooter)
{
    for(auto& s : m_soldiers)
    {
        if(s.team == shooter.team || !s.alive()) continue;
        float const d = length(s.pos - shooter.pos);
        if(d > 420.0f) continue;
        if(s.threat_age > 1.5f || d < length(s.threat - s.pos))
        {
            s.threat = shooter.pos + sf::Vector2f{Random::real(-1.0f, 1.0f), Random::real(-1.0f, 1.0f)} * (d * 0.06f);
            s.threat_age = 0.0f;
        }
    }
}

void Battle::throw_grenade(Soldier& s, sf::Vector2f at)
{
    sf::Vector2f d = at - s.pos;
    float const dist = std::min(length(d), 150.0f);
    d = unit(d) * dist;
    float const t = std::clamp(dist / 130.0f, 0.35f, 1.1f);

    Grenade g;
    g.pos = s.pos + unit(d) * 4.0f;
    g.vel = d / t;
    g.height = 5.0f;
    g.vz = 300.0f * t * 0.5f;
    g.fuse = Random::real(3.0f, 3.6f);
    g.owner = s.id;
    g.team = s.team;
    g.spin = Random::real(-15.0f, 15.0f);
    m_grenades.push_back(g);
    --s.grenades;
    s.cooldown = std::max(s.cooldown, 0.8f);

    SoundBank::get().play_at("grenade_pin", s.pos, 0.6f);
    SoundBank::get().play_at("throw", s.pos, 0.7f);
    bark(s, s.team == Team::MONTERAN ? "Grenade out!" : "Vrak!", true);
}

// ---------------------------------------------------------------- bullets

void Battle::update_bullets(float dt)
{
    Soldier const& me = *player();

    for(auto& b : m_bullets)
    {
        sf::Vector2f const a = b.pos;
        sf::Vector2f c = b.pos + b.vel * dt;
        float seg = length(c - a);
        if(seg > b.range)
        {
            c = a + unit(b.vel) * b.range;
            seg = b.range;
        }

        // Walls.
        float end = 1.0f;
        if(auto t = m_map.wall_hit(a, c)) end = *t;

        // Bottles on the range.
        for(auto& bottle : m_map.bottles)
        {
            sf::Vector2f at;
            if(!bottle.broken && segment_circle(a, a + (c - a) * end, bottle.pos, 2.5f, &at))
            {
                bottle.broken = true;
                --m_bottles_left;
                SoundBank::get().play_at("bottle_break", at, 0.9f);
                for(int i = 0; i < 12; ++i)
                    m_sparks.push_back({at, dir(Random::real(0, 6.283f)) * Random::real(20, 70), Random::real(0.2f, 0.5f), lerp(GRASS, MON_WHITE, 0.6f)});
                b.dead = true;
                break;
            }
        }
        if(b.dead) continue;

        // Men: the nearest one along the path.
        Soldier* victim = nullptr;
        sf::Vector2f victim_at;
        float best = 1e9f;
        for(auto& s : m_soldiers)
        {
            if(s.id == b.owner || s.team == b.team) continue;
            if(s.life != Life::OK && s.life != Life::DOWNED) continue;
            sf::Vector2f at;
            float const r = s.life == Life::DOWNED ? 2.5f : s.radius();
            if(segment_circle(a, a + (c - a) * end, s.pos, r, &at))
            {
                float const d = length(at - a);
                // Rounds aimed at the living mostly pass over the fallen.
                if(s.life == Life::DOWNED && length(at - b.origin) > 30.0f && Random::chance(0.8f)) continue;
                if(d < best) { best = d; victim = &s; victim_at = at; }
            }
        }

        // Near misses frighten whoever they pass.
        sf::Vector2f const stop = victim ? victim_at : a + (c - a) * end;
        for(auto& s : m_soldiers)
        {
            if(s.team == b.team || !s.alive() || s.id >= 64) continue;
            std::uint64_t const bit = std::uint64_t{1} << s.id;
            if(b.passed & bit) continue;
            float const d = distance_to_segment(s.pos, a, stop);
            if(d > 24.0f) continue;
            b.passed |= bit;
            bool const covered = m_map.in_trench(s.pos) || (s.stance == Stance::PRONE && m_map.in_crater(s.pos));
            s.suppression = std::min(1.0f, s.suppression + b.suppress * (1.0f - d / 24.0f) * (covered ? 0.55f : 1.0f));
            s.threat = b.origin;
            s.threat_age = 0.0f;
            if(s.is_player && victim != &s)
            {
                if(d < 12.0f) SoundBank::get().play("snap", 0.75f, 0.1f);
                else SoundBank::get().play("whiz", 0.5f, 0.1f);
                m_combat_heat = std::min(1.0f, m_combat_heat + 0.08f);
            }
        }

        if(victim)
        {
            // Cover gets a chance to stop it.
            Soldier& s = *victim;
            bool const low = s.stance == Stance::PRONE || s.life == Life::DOWNED;
            float block = 0.0f;
            float const travelled = length(victim_at - b.origin);
            if(m_map.in_trench(s.pos) && (!m_map.in_trench(b.origin) || travelled > 45.0f))
            {
                block = low ? 0.94f : (s.stance == Stance::CROUCH ? 0.85f : 0.45f);
                // Standing right over the lip, you can see down into it.
                if(!m_map.in_trench(b.origin) && travelled < 70.0f) block *= travelled / 70.0f;
            }
            else if(m_map.sandbag_between(b.origin, s.pos, 20.0f))
                block = low ? 0.9f : (s.stance == Stance::CROUCH ? 0.8f : 0.3f);
            else if(m_map.in_crater(s.pos))
                block = low ? 0.78f : (s.stance == Stance::CROUCH ? 0.45f : 0.0f);

            if(Random::chance(block))
            {
                dust(victim_at - unit(b.vel) * 3.0f, 4, MUD_PALE);
                SoundBank::get().play_at(m_map.in_trench(s.pos) ? "impact_dirt" : "impact_sandbag", victim_at, 0.5f);
                if(s.is_player) m_camera.shake(0.12f);
            }
            else
            {
                hit(s, b, victim_at);
            }
            b.dead = true;
            continue;
        }

        if(end < 1.0f)
        {
            sf::Vector2f const at = a + (c - a) * end;
            dust(at - unit(b.vel) * 1.5f, 3, lerp(CONCRETE, BRICK, 0.5f));
            if(length(at - me.pos) < 200.0f && Random::chance(0.35f)) SoundBank::get().play_at("ricochet", at, 0.5f);
            else SoundBank::get().play_at("impact_wood", at, 0.3f);
            b.dead = true;
            continue;
        }

        b.pos = c;
        b.range -= seg;
        if(b.range <= 0.5f)
        {
            dust(b.pos, 2, MUD_PALE);
            if(length(b.pos - me.pos) < 120.0f) SoundBank::get().play_at("impact_dirt", b.pos, 0.35f);
            b.dead = true;
        }
    }
    std::erase_if(m_bullets, [](const Bullet& b) { return b.dead; });
}

// ---------------------------------------------------------------- wounds

void Battle::hit(Soldier& s, const Bullet& b, sf::Vector2f at)
{
    float const damage = b.damage * Random::real(0.85f, 1.15f);
    sf::Vector2f const d = unit(b.vel);

    float const r = Random::real(0.0f, 1.0f);
    int cause = 1;                          // 0 head, 1 body, 2 arm, 3 leg
    float head_chance = s.stance == Stance::PRONE ? 0.22f : (s.stance == Stance::CROUCH ? 0.16f : 0.11f);
    if(r < head_chance) cause = 0;
    else if(r < head_chance + 0.45f) cause = 1;
    else if(r < head_chance + 0.67f) cause = 2;
    else cause = 3;

    spray_blood(at, d, cause == 0 ? 14 : 8, 70.0f);
    SoundBank::get().play_at("impact_flesh", at, 0.8f);

    if(s.life == Life::DOWNED)
    {
        kill(s, b.owner, b.heavy ? "Cut down by machine-gun fire where he lay" : "Shot again where he lay");
        return;
    }
    wound(s, damage, d, b.owner, cause, b.heavy);
}

void Battle::wound(Soldier& s, float damage, sf::Vector2f from_dir, int attacker, int cause, bool heavy)
{
    if(s.life != Life::OK) return;
    if(s.is_player)
    {
        m_hurt = 1.0f;
        m_camera.shake(0.45f);
    }

    s.suppression = std::min(1.0f, s.suppression + 0.5f);
    s.morale = std::max(0.0f, s.morale - 0.15f);

    if(cause == 0)
    {
        if(damage >= 40.0f)
        {
            if(Random::chance(heavy ? 0.5f : 0.2f)) sever(s, HEAD, from_dir);
            kill(s, attacker, heavy ? "Cut down by machine-gun fire" : "Shot through the head");
            return;
        }
        s.hp -= damage * 1.5f;
    }
    else if(cause == 1)
    {
        s.hp -= damage;
        s.bleeding += 1.4f;
        // A flamethrower's tanks are on his back.
        if(s.role == Role::FLAMER && s.fuel > 5.0f && Random::chance(0.14f))
        {
            explode(s.pos, 1.1f, attacker, s.team == Team::MONTERAN ? Team::RECHIVII : Team::MONTERAN, true, "Burned to death when his tanks burst");
            return;
        }
    }
    else
    {
        bool const arm = cause == 2;
        bool const left = Random::chance(0.5f);
        Limb const limb = arm ? (left ? ARM_L : ARM_R) : (left ? LEG_L : LEG_R);
        s.hp -= damage * 0.55f;
        s.bleeding += 0.8f;
        if(!(s.lost & limb) && damage >= 40.0f && Random::chance(heavy ? 0.4f : 0.15f))
        {
            sever(s, limb, from_dir);
            s.bleeding += 3.5f;
        }
    }

    if(s.hp <= 0.0f)
    {
        if(s.hp < -45.0f) kill(s, attacker, heavy ? "Cut down by machine-gun fire" : "Shot dead");
        else go_down(s, attacker);
        return;
    }

    if(!s.can_walk()) s.stance = Stance::PRONE;
    if(Random::chance(0.6f)) SoundBank::get().play_at(Random::chance(0.5f) ? "scream" : "groan", s.pos, 0.7f, 0.08f);
    bark(s, s.team == Team::MONTERAN ? (Random::chance(0.5f) ? "I'm hit!" : "Agh! My god!") : "Ahh!", false);
    if(s.is_player) hint(s.bleeding > 0.0f && s.bandages > 0 ? "You are bleeding. F to bandage yourself, somewhere safe." : "You are hit.", 4.0f);
    (void)cause_name;
}

void Battle::sever(Soldier& s, Limb limb, sf::Vector2f push)
{
    if(s.lost & limb) return;
    s.lost |= limb;
    Gib g;
    g.pos = s.pos;
    g.vel = unit(push + sf::Vector2f{Random::real(-0.5f, 0.5f), Random::real(-0.5f, 0.5f)}) * Random::real(50.0f, 110.0f);
    g.vz = Random::real(40.0f, 90.0f);
    g.height = 4.0f;
    g.angle = Random::real(0.0f, 6.283f);
    g.spin = Random::real(-18.0f, 18.0f);
    g.kind = limb == HEAD ? 3 : ((limb == ARM_L || limb == ARM_R) ? 1 : 2);
    g.team = s.team;
    g.charred = s.charred;
    m_gibs.push_back(g);

    if(limb == HEAD && s.role != Role::OFFICER && !(s.role == Role::MEDIC && s.team == Team::MONTERAN))
    {
        Gib helmet = g;
        helmet.kind = 4;
        helmet.vel = g.vel * 1.3f;
        helmet.vz = g.vz * 1.2f;
        m_gibs.push_back(helmet);
    }

    spray_blood(s.pos, push, 22, 90.0f);
    SoundBank::get().play_at("gore", s.pos, 0.9f, 0.08f);
    if(limb != HEAD && s.life == Life::OK) SoundBank::get().play_at("scream", s.pos, 0.9f, 0.06f);
}

void Battle::gib_body(Soldier& s, sf::Vector2f push)
{
    for(Limb l : {ARM_L, ARM_R, LEG_L, LEG_R, HEAD})
        if(Random::chance(l == HEAD ? 0.7f : 0.65f)) sever(s, l, push + sf::Vector2f{Random::real(-1, 1), Random::real(-1, 1)});
    for(int i = 0; i < 8; ++i)
    {
        Gib g;
        g.pos = s.pos;
        g.vel = unit(push + sf::Vector2f{Random::real(-1, 1), Random::real(-1, 1)}) * Random::real(40.0f, 140.0f);
        g.vz = Random::real(50.0f, 140.0f);
        g.kind = 0;
        g.team = s.team;
        g.spin = Random::real(-20.0f, 20.0f);
        m_gibs.push_back(g);
    }
    spray_blood(s.pos, push, 40, 130.0f);
}

void Battle::go_down(Soldier& s, int attacker)
{
    (void)attacker;
    s.life = Life::DOWNED;
    s.stance = Stance::PRONE;
    s.body_angle = Random::real(0.0f, 6.283f);
    s.vel = {};
    s.deployed = false;
    s.deploying = 0.0f;
    s.firing = false;
    s.burning = std::min(s.burning, 0.0f);
    s.bleed_out = (s.lost & (ARM_L | ARM_R | LEG_L | LEG_R)) ? 16.0f : 32.0f;
    if(s.dragging >= 0) { m_soldiers[static_cast<std::size_t>(s.dragging)].dragged_by = -1; s.dragging = -1; }
    s.treating = -1;

    SoundBank::get().play_at("body_fall", s.pos, 0.7f);
    SoundBank::get().play_at("scream", s.pos, 0.85f, 0.05f);
    if(s.team == Team::MONTERAN)
    {
        s.bark_cooldown = 0.0f;
        bark(s, "Medic! Medic!", false);
        if(!s.is_player)
            for(auto& o : m_soldiers)
                if(o.team == s.team && o.alive() && !o.is_player && length(o.pos - s.pos) < 120.0f) { bark(o, "Man down!", true); break; }
    }
    if(s.is_player)
    {
        hint("You are down and bleeding. E calls for a medic, WASD crawls. Space gives up.", 10.0f);
        m_hurt = 1.0f;
    }
}

void Battle::kill(Soldier& s, int attacker, const std::string& fate)
{
    if(s.life == Life::DEAD) return;
    bool const was_down = s.life == Life::DOWNED;
    s.life = Life::DEAD;
    if(s.fate.empty()) s.fate = fate;
    s.vel = {};
    s.firing = false;
    s.deployed = false;
    if(!was_down) s.body_angle = s.aim + Random::real(-0.6f, 0.6f) + 3.14159f;
    if(s.dragging >= 0) { m_soldiers[static_cast<std::size_t>(s.dragging)].dragged_by = -1; s.dragging = -1; }
    if(s.dragged_by >= 0) { m_soldiers[static_cast<std::size_t>(s.dragged_by)].dragging = -1; s.dragged_by = -1; }
    s.treating = -1;

    if(attacker >= 0 && attacker < static_cast<int>(m_soldiers.size()) && m_soldiers[static_cast<std::size_t>(attacker)].team != s.team)
        ++m_soldiers[static_cast<std::size_t>(attacker)].kills;

    if(!was_down) SoundBank::get().play_at("body_fall", s.pos, 0.7f);
    stamp_corpse(s);

    if(s.team == Team::MONTERAN && !s.is_player)
        for(auto& o : m_soldiers)
            if(o.team == s.team && o.alive() && length(o.pos - s.pos) < 90.0f)
            {
                o.morale = std::max(0.0f, o.morale - 0.1f);
                if(o.role == Role::OFFICER || Random::chance(0.3f)) bark(o, Random::chance(0.5f) ? "No! No..." : "He's gone.", false);
                break;
            }
    if(s.is_player) m_hurt = 1.0f;
}

void Battle::stamp_corpse(Soldier& s)
{
    // A pool first, then the body on top of it, into the ground for good.
    draw::disc(m_stamps, s.pos.x + Random::real(-1, 1), s.pos.y + Random::real(-1, 1), s.charred ? 3.0f : Random::real(4.0f, 6.0f),
               s.charred ? CHAR : BLOOD_DK);
    if(!s.charred) draw::disc(m_stamps, s.pos.x, s.pos.y, 3.0f, BLOOD);
    draw_soldier(m_stamps, s, m_time);
}

// ---------------------------------------------------------------- effects

void Battle::spray_blood(sf::Vector2f at, sf::Vector2f d, int count, float speed)
{
    d = unit(d);
    for(int i = 0; i < count; ++i)
    {
        Drop drop;
        drop.pos = at;
        drop.vel = unit(d + sf::Vector2f{Random::real(-0.6f, 0.6f), Random::real(-0.6f, 0.6f)}) * speed * Random::real(0.2f, 1.0f);
        drop.vz = Random::real(10.0f, 60.0f);
        drop.height = 3.0f;
        drop.size = Random::chance(0.3f) ? 2.0f : 1.0f;
        m_drops.push_back(drop);
    }
}

void Battle::dust(sf::Vector2f at, int count, sf::Color colour)
{
    for(int i = 0; i < count; ++i)
    {
        m_puffs.push_back({at, {Random::real(-12, 12), Random::real(-18, -4)}, 0.5f, 0.5f, 1.5f, 5.0f, with_alpha(colour, 0.6f)});
        Drop d;
        d.pos = at;
        d.vel = {Random::real(-30, 30), Random::real(-30, 30)};
        d.vz = Random::real(20.0f, 50.0f);
        d.dirt = true;
        m_drops.push_back(d);
    }
}

void Battle::ignite(Soldier& s, int attacker)
{
    if(s.life != Life::OK) return;
    if(s.burning <= 0.0f)
    {
        SoundBank::get().play_at("scream", s.pos, 1.0f, 0.04f);
        bark(s, s.team == Team::MONTERAN ? "I'm burning! Help me!" : "AAAH!", false);
        if(s.is_player) hint("You are on fire. Z to drop and roll -- it puts it out faster.", 4.0f);
    }
    s.burning = std::max(s.burning, 4.5f);
    s.suppression = 1.0f;
    s.target = attacker;        // remembered for the kill
}

void Battle::explode(sf::Vector2f at, float power, int owner, Team team, bool fire, const std::string& cause)
{
    (void)team;
    bool const ours = owner == -2;          // our own guns on their line: it only finds them
    Soldier& me = *player();
    float const to_me = length(at - me.pos);

    SoundBank::get().play_at(fire ? "tank_burst" : "explosion", at, std::min(1.0f, 0.6f + power * 0.3f), 0.08f);
    m_lights.push_back({at, 60.0f * power, 0.25f, 0.25f, fire ? FIRE_ORANGE : FIRE_YELLOW});
    m_camera.shake(std::clamp(1.0f - to_me / 500.0f, 0.0f, 1.0f) * 0.75f * power);
    if(to_me < 110.0f * power)
    {
        float const k = 1.0f - to_me / (110.0f * power);
        SoundBank::get().deafen(1.0f + 3.0f * k);
        m_flash = std::max(m_flash, k);
        me.shock = std::max(me.shock, k);
    }

    // The crater, the scorch and the earth thrown up.
    float const r = 6.0f + 6.0f * power;
    draw::disc(m_stamps, at.x, at.y, r + 3.0f, lerp(MUD_PALE, MUD_DARK, 0.3f));
    draw::disc(m_stamps, at.x, at.y, r, MUD_DARK);
    draw::disc(m_stamps, at.x + 1, at.y + 1, r * 0.6f, fire ? CHAR : lerp(MUD_DARK, INK, 0.4f));
    for(int i = 0; i < 40; ++i)
    {
        Drop d;
        d.pos = at;
        float const a = Random::real(0.0f, 6.283f);
        d.vel = dir(a) * Random::real(20.0f, 120.0f) * power;
        d.vz = Random::real(60.0f, 180.0f);
        d.dirt = true;
        d.size = Random::chance(0.3f) ? 2.0f : 1.0f;
        m_drops.push_back(d);
    }
    for(int i = 0; i < 14; ++i)
        m_puffs.push_back({at + sf::Vector2f{Random::real(-6, 6), Random::real(-6, 6)}, {Random::real(-15, 15), Random::real(-25, -5)},
                           Random::real(1.5f, 3.5f), 3.5f, Random::real(4.0f, 8.0f), Random::real(4.0f, 9.0f),
                           with_alpha(i < 4 ? FIRE_ORANGE : SMOKE, i < 4 ? 0.9f : 0.6f)});
    for(int i = 0; i < 16; ++i)
        m_sparks.push_back({at, dir(Random::real(0, 6.283f)) * Random::real(60.0f, 200.0f), Random::real(0.1f, 0.35f), FIRE_YELLOW});

    if(fire)
    {
        for(int i = 0; i < 6; ++i)
            m_fires.push_back({at + sf::Vector2f{Random::real(-20, 20), Random::real(-20, 20)}, Random::real(5, 9), Random::real(5, 9), 0.0f});
        for(int i = 0; i < 30; ++i)
        {
            Flame f;
            f.pos = at;
            f.vel = dir(Random::real(0, 6.283f)) * Random::real(30.0f, 110.0f);
            f.max_life = f.life = Random::real(0.4f, 0.8f);
            f.owner = owner;
            f.team = Team::MONTERAN;     // burns everyone: see the flame update
            m_flames.push_back(f);
        }
    }


    for(auto& s : m_soldiers)
    {
        if(s.life != Life::OK && s.life != Life::DOWNED) continue;
        if(ours && s.team == Team::MONTERAN) continue;
        float const d = length(s.pos - at);
        if(d > 120.0f * power) continue;
        if(!m_map.line_of_sight(at, s.pos)) continue;           // walls take it

        float shelter = 1.0f;
        if(m_map.in_trench(s.pos) && d > 10.0f) shelter = 0.45f;
        if(s.stance == Stance::PRONE && d > 14.0f) shelter *= 0.7f;

        sf::Vector2f const push = unit(s.pos - at);
        s.suppression = std::min(1.0f, s.suppression + (1.0f - d / (120.0f * power)) * 0.9f);
        s.shock = std::max(s.shock, (1.0f - d / (120.0f * power)) * 0.8f);

        if(d < 12.0f * power && shelter > 0.5f)
        {
            gib_body(s, push);
            kill(s, owner, "Blown apart by " + cause);
            continue;
        }
        float const reach = 46.0f * power;
        if(d < reach)
        {
            float const dmg = 150.0f * (1.0f - d / reach) * shelter;
            if(s.life == Life::DOWNED) { if(dmg > 30.0f) kill(s, owner, "Killed by " + cause + " as he lay wounded"); continue; }
            // Fragments find arms and legs.
            for(Limb l : {ARM_L, ARM_R, LEG_L, LEG_R})
                if(dmg > 55.0f && Random::chance(0.3f)) sever(s, l, push);
            s.hp -= dmg;
            s.bleeding += dmg * 0.03f;
            spray_blood(s.pos, push, 10, 80.0f);
            if(s.is_player) { m_hurt = 1.0f; }
            if(s.hp <= 0.0f)
            {
                if(s.hp < -40.0f) kill(s, owner, "Killed by " + cause);
                else go_down(s, owner);
            }
            else if(!s.can_walk()) s.stance = Stance::PRONE;
            if(fire && s.life == Life::OK) ignite(s, owner);
        }
    }
}

// ---------------------------------------------------------------- bodies

void Battle::update_soldier_body(Soldier& s, float dt)
{
    s.bark_time     = std::max(0.0f, s.bark_time - dt);
    s.bark_cooldown = std::max(0.0f, s.bark_cooldown - dt);
    s.stance_change = std::max(0.0f, s.stance_change - dt);
    s.flash         = std::max(0.0f, s.flash - (weapon_of(s.role).flame ? 0.0f : dt));
    s.threat_age   += dt;
    s.shock         = std::max(0.0f, s.shock - dt * 0.35f);
    s.revealed      = std::max(0.0f, s.revealed - dt);

    if(s.life == Life::DEAD || s.life == Life::FLED) return;

    if(s.life == Life::DOWNED)
    {
        s.bleed_out -= dt * (s.dragged_by >= 0 ? 0.8f : 1.0f);
        s.idle_anim += dt;
        if(s.idle_anim > 0.8f)
        {
            s.idle_anim = 0.0f;
            draw::disc(m_stamps, s.pos.x + Random::real(-2, 2), s.pos.y + Random::real(-2, 2), Random::real(1.5f, 3.0f), BLOOD_DK);
        }
        if(Random::chance(dt / 5.0f))
            SoundBank::get().play_at(s.team == Team::MONTERAN && Random::chance(0.5f) ? "medic_call" : "groan", s.pos, 0.55f, 0.06f);
        if(s.dragged_by >= 0)
        {
            Soldier& by = m_soldiers[static_cast<std::size_t>(s.dragged_by)];
            sf::Vector2f const behind = by.pos - unit(by.vel.x * by.vel.x + by.vel.y * by.vel.y > 1.0f ? by.vel : dir(by.aim)) * 8.0f;
            s.pos += (behind - s.pos) * std::min(1.0f, dt * 6.0f);
            m_map.collide(s.pos, 3.0f);
            if(length(by.vel) > 3.0f && Random::chance(dt * 8.0f)) draw::rect(m_stamps, std::floor(s.pos.x), std::floor(s.pos.y), 2, 1, BLOOD_DK);
            SoundBank::get().set_loop("drag_loop", by.is_player && length(by.vel) > 3.0f ? 0.5f : 0.0f);
        }
        else if(s.is_player)
        {
            s.pos += s.vel * dt;
            m_map.collide(s.pos, 3.0f);
            if(length(s.vel) > 1.0f && Random::chance(dt * 6.0f)) draw::rect(m_stamps, std::floor(s.pos.x), std::floor(s.pos.y), 2, 1, BLOOD_DK);
        }
        if(s.bleed_out <= 0.0f) kill(s, -1, s.lost ? "Bled to death from his wounds" : "Bled out waiting for a medic");
        return;
    }

    // --- alive --------------------------------------------------------------
    const WeaponSpec& w = weapon_of(s.role);
    s.cooldown = std::max(0.0f, s.cooldown - (w.flame ? 0.0f : dt));
    s.kick *= std::exp(-dt * 5.0f);
    s.heat = std::max(0.0f, s.heat - dt * (s.overheated > 0.0f ? 0.25f : 0.18f));
    s.overheated = std::max(0.0f, s.overheated - dt);

    if(s.reloading > 0.0f)
    {
        s.reloading -= dt;
        if(s.reloading <= 0.0f)
        {
            int const want = w.magazine - s.ammo;
            int const take = std::min(want, s.spare);
            s.ammo += take;
            s.spare -= take;
        }
    }
    if(s.deploying > 0.0f)
    {
        s.deploying -= dt;
        if(s.deploying <= 0.0f) s.deployed = true;
    }
    if(s.deployed && length(s.vel) > 6.0f) s.deployed = false;

    // Fear fades, faster in cover and near the officer.
    float calm = 0.18f;
    if(m_map.in_trench(s.pos) || s.stance == Stance::PRONE) calm *= 1.3f;
    if(Soldier* lt = officer(s.team); lt && lt->id != s.id && length(lt->pos - s.pos) < 80.0f) calm *= 1.6f;
    if(s.team == Team::MONTERAN && !officer(Team::MONTERAN)) calm *= 0.8f;
    s.suppression = std::max(0.0f, s.suppression - calm * dt);

    // --- movement ---------------------------------------------------------------
    s.pos += s.vel * dt;
    m_map.collide(s.pos, s.radius());
    float const speed = length(s.vel);
    if(speed > 2.0f)
    {
        float const before = s.walk_phase;
        s.walk_phase += speed * dt * 0.32f;
        if(std::floor(before / 3.14159f) != std::floor(s.walk_phase / 3.14159f) && s.stance != Stance::PRONE)
            SoundBank::get().play_at("footstep", s.pos, s.is_player ? 0.35f : 0.18f, 0.1f);
    }

    // --- bleeding, burning --------------------------------------------------------
    if(s.bleeding > 0.0f)
    {
        s.hp -= s.bleeding * dt;
        if(Random::chance(dt * s.bleeding * 1.5f)) draw::rect(m_stamps, std::floor(s.pos.x + Random::real(-2, 2)), std::floor(s.pos.y + Random::real(-2, 2)), 1, 1, BLOOD);
        if(s.hp <= 0.0f) { go_down(s, -1); return; }
    }
    if(s.burning > 0.0f)
    {
        s.burning -= dt * (s.stance == Stance::PRONE ? 2.5f : 1.0f);
        s.hp -= 21.0f * dt;
        if(Random::chance(dt * 30.0f))
            m_puffs.push_back({s.pos + sf::Vector2f{Random::real(-3, 3), Random::real(-3, 3)}, {Random::real(-5, 5), Random::real(-25, -10)},
                               0.4f, 0.4f, 2.0f, 3.0f, with_alpha(Random::chance(0.5f) ? FIRE_ORANGE : FIRE_YELLOW, 0.9f)});
        if(Random::chance(dt * 1.5f)) SoundBank::get().play_at("scream", s.pos, 0.9f, 0.1f);
        m_lights.push_back({s.pos, 18.0f, dt * 1.5f, dt * 1.5f, FIRE_ORANGE});
        if(s.hp <= 0.0f)
        {
            s.charred = true;
            kill(s, s.target, "Burned to death");
            return;
        }
    }

    // --- dragging, treating ---------------------------------------------------------
    if(s.dragging >= 0)
    {
        Soldier& d = m_soldiers[static_cast<std::size_t>(s.dragging)];
        if(d.life != Life::DOWNED) { d.dragged_by = -1; s.dragging = -1; SoundBank::get().set_loop("drag_loop", 0.0f); }
    }
    if(s.treating >= 0)
    {
        Soldier& patient = m_soldiers[static_cast<std::size_t>(s.treating)];
        bool const ok = (patient.life == Life::DOWNED || (patient.alive() && patient.bleeding > 0.0f))
                     && length(patient.pos - s.pos) < 16.0f && length(s.vel) < 5.0f && s.medkits > 0;
        if(!ok) { s.treating = -1; }
        else
        {
            s.treat_time -= dt;
            if(s.treat_time <= 0.0f)
            {
                --s.medkits;
                patient.bleeding = 0.0f;
                patient.bandaged = true;
                if(patient.life == Life::DOWNED)
                {
                    patient.life = Life::OK;
                    patient.hp = 35.0f;
                    patient.bleed_out = 0.0f;
                    patient.stance = patient.can_walk() ? Stance::CROUCH : Stance::PRONE;
                    patient.suppression = 0.5f;
                    if(patient.dragged_by >= 0) { m_soldiers[static_cast<std::size_t>(patient.dragged_by)].dragging = -1; patient.dragged_by = -1; }
                    if(patient.is_player) hint(s.name + " patched you up. Back into it.", 4.0f);
                }
                bark(s, "You'll live. Go!", false);
                s.treating = -1;
            }
        }
    }
}

void Battle::update_effects(float dt)
{
    // --- separation: men do not stand inside each other ---------------------------
    for(std::size_t i = 0; i < m_soldiers.size(); ++i)
        for(std::size_t j = i + 1; j < m_soldiers.size(); ++j)
        {
            Soldier& a = m_soldiers[i];
            Soldier& b = m_soldiers[j];
            if(!a.alive() || !b.alive()) continue;
            sf::Vector2f const d = b.pos - a.pos;
            float const l = length(d);
            if(l < 6.0f && l > 0.01f)
            {
                sf::Vector2f const push = d / l * ((6.0f - l) * 0.5f);
                if(!a.is_player) a.pos -= push;
                if(!b.is_player) b.pos += push;
            }
        }

    // --- sounds held back a moment ------------------------------------------------------
    for(auto& l : m_later)
    {
        l.t -= dt;
        if(l.t <= 0.0f) SoundBank::get().play_at(l.name, l.pos, l.volume);
    }
    std::erase_if(m_later, [](const Later& l) { return l.t <= 0.0f; });

    // --- grenades --------------------------------------------------------------------------
    for(auto& g : m_grenades)
    {
        sf::Vector2f next = g.pos + g.vel * dt;
        if(m_map.wall_hit(g.pos, next) || (!m_map.walkable(next) && g.height < 6.0f))
        {
            g.vel = -g.vel * 0.3f;
            next = g.pos;
        }
        g.pos = next;
        g.height += g.vz * dt;
        g.vz -= 300.0f * dt;
        if(g.height <= 0.0f)
        {
            g.height = 0.0f;
            if(std::abs(g.vz) > 30.0f) SoundBank::get().play_at("grenade_bounce", g.pos, 0.5f);
            g.vz = -g.vz * 0.25f;
            g.vel *= m_map.in_trench(g.pos) ? 0.2f : 0.45f;
            if(std::abs(g.vz) < 10.0f) g.vz = 0.0f;
        }
        g.spin *= std::exp(-dt * 2.0f);
        g.fuse -= dt;
        if(g.fuse <= 0.0f)
            explode(g.pos, 1.0f, g.owner, g.team, false, "a grenade");
    }
    std::erase_if(m_grenades, [](const Grenade& g) { return g.fuse <= 0.0f; });

    // --- shells -------------------------------------------------------------------------------
    for(auto& sh : m_shells)
    {
        sh.time -= dt;
        if(!sh.whistled && sh.time < 1.8f)
        {
            sh.whistled = true;
            float const d = length(sh.target - player()->pos);
            SoundBank::get().play("shell_incoming", std::clamp(1.0f - d / 700.0f, 0.15f, 0.9f) * (sh.ours ? 0.5f : 1.0f), 0.05f);
        }
        if(sh.time <= 0.0f) explode(sh.target, sh.ours ? 1.3f : 1.5f, sh.ours ? -2 : -1, Team::RECHIVII, false, "a shell");
    }
    std::erase_if(m_shells, [](const Shell& s) { return s.time <= 0.0f; });

    // --- flames ---------------------------------------------------------------------------------
    float flame_near = 0.0f;
    for(auto& f : m_flames)
    {
        sf::Vector2f const next = f.pos + f.vel * dt;
        if(m_map.wall_hit(f.pos, next)) { f.vel *= -0.2f; f.life = std::min(f.life, 0.12f); }
        else f.pos = next;
        f.vel *= std::exp(-dt * 2.2f);
        f.life -= dt;
        float const age = 1.0f - f.life / f.max_life;

        for(auto& s : m_soldiers)
        {
            if(s.life != Life::OK && s.life != Life::DOWNED) continue;
            if(s.id == f.owner) continue;
            if(s.team == m_soldiers[static_cast<std::size_t>(std::max(0, f.owner))].team && f.owner >= 0 && age < 0.6f) continue;
            if(length(s.pos - f.pos) < 5.0f)
            {
                if(s.life == Life::DOWNED) { s.charred = true; kill(s, f.owner, "Burned where he lay"); }
                else ignite(s, f.owner);
            }
        }
        if(f.life <= 0.0f && Random::chance(0.12f))
        {
            m_fires.push_back({f.pos, Random::real(4, 7), Random::real(4, 8), 0.0f});
            draw::disc(m_stamps, f.pos.x, f.pos.y, Random::real(2, 4), with_alpha(CHAR, 0.7f));
        }
        flame_near = std::max(flame_near, SoundBank::get().attenuation(f.pos));
    }
    std::erase_if(m_flames, [](const Flame& f) { return f.life <= 0.0f; });
    SoundBank::get().set_loop("flamer_loop", m_flames.empty() ? 0.0f : 0.25f + 0.7f * flame_near);

    // --- fires on the ground -----------------------------------------------------------------------
    float burning_near = 0.0f;
    for(auto& fire : m_fires)
    {
        fire.life -= dt;
        fire.spawn -= dt;
        if(fire.spawn <= 0.0f)
        {
            fire.spawn = 0.05f;
            m_puffs.push_back({fire.pos + sf::Vector2f{Random::real(-fire.radius, fire.radius), Random::real(-fire.radius, fire.radius) * 0.6f},
                               {Random::real(-4, 4), Random::real(-22, -10)}, 0.45f, 0.45f, 2.0f, 2.0f,
                               with_alpha(Random::chance(0.5f) ? FIRE_ORANGE : FIRE_YELLOW, 0.9f)});
            if(Random::chance(0.2f))
                m_puffs.push_back({fire.pos, {Random::real(-4, 4), Random::real(-14, -6)}, 1.6f, 1.6f, 3.0f, 5.0f, with_alpha(SMOKE, 0.4f)});
        }
        for(auto& s : m_soldiers)
            if(s.life == Life::OK && length(s.pos - fire.pos) < fire.radius) ignite(s, -1);
        m_lights.push_back({fire.pos, fire.radius * 3.0f, dt * 1.5f, dt * 1.5f, FIRE_ORANGE});
        burning_near = std::max(burning_near, SoundBank::get().attenuation(fire.pos));
    }
    std::erase_if(m_fires, [](const Fire& f) { return f.life <= 0.0f; });
    SoundBank::get().set_loop("burning_loop", burning_near * 0.7f);

    // --- gibs ----------------------------------------------------------------------------------------
    for(auto& g : m_gibs)
    {
        g.pos += g.vel * dt;
        g.height += g.vz * dt;
        g.vz -= 260.0f * dt;
        g.angle += g.spin * dt;
        g.trail -= dt;
        if(g.trail <= 0.0f && g.kind != 4)
        {
            g.trail = 0.03f;
            draw::rect(m_stamps, std::floor(g.pos.x), std::floor(g.pos.y), 1, 1, BLOOD);
        }
        if(m_map.wall_hit(g.pos - g.vel * dt, g.pos)) { g.pos -= g.vel * dt; g.vel = -g.vel * 0.3f; }
        if(g.height <= 0.0f)
        {
            g.height = 0.0f;
            g.vz = -g.vz * 0.3f;
            g.vel *= 0.4f;
            g.spin *= 0.4f;
            if(length(g.vel) < 6.0f && std::abs(g.vz) < 12.0f)
            {
                // Settled: into the ground it goes.
                sf::Vector2f const u = dir(g.angle);
                sf::Color const coat = g.charred ? CHAR : (g.team == Team::MONTERAN ? MON_BLUE : RCH_TAN);
                if(g.kind != 4) draw::disc(m_stamps, g.pos.x, g.pos.y, g.kind == 0 ? 1.5f : 2.5f, BLOOD_DK);
                switch(g.kind)
                {
                    case 1:
                        draw::thick_line(m_stamps, g.pos - u * 2.0f, g.pos + u * 2.0f, 1.6f, coat);
                        draw::rect(m_stamps, std::floor(g.pos.x + u.x * 2.5f), std::floor(g.pos.y + u.y * 2.5f), 1, 1, g.charred ? CHAR : SKIN);
                        draw::rect(m_stamps, std::floor(g.pos.x - u.x * 2.5f), std::floor(g.pos.y - u.y * 2.5f), 1, 1, BLOOD_BRIGHT);
                        break;
                    case 2:
                        draw::thick_line(m_stamps, g.pos - u * 2.5f, g.pos + u * 2.5f, 2.0f, g.charred ? CHAR : (g.team == Team::MONTERAN ? MON_BLUE_DK : RCH_TAN_DK));
                        draw::rect(m_stamps, std::floor(g.pos.x + u.x * 3.0f), std::floor(g.pos.y + u.y * 3.0f), 2, 2, LEATHER);
                        draw::rect(m_stamps, std::floor(g.pos.x - u.x * 3.0f), std::floor(g.pos.y - u.y * 3.0f), 1, 1, BONE);
                        break;
                    case 3:
                        draw::disc(m_stamps, g.pos.x, g.pos.y, 2.0f, g.charred ? CHAR : SKIN_DK);
                        draw::rect(m_stamps, std::floor(g.pos.x - u.x * 2.0f), std::floor(g.pos.y - u.y * 2.0f), 1, 1, BLOOD_BRIGHT);
                        break;
                    case 4:
                        draw::disc(m_stamps, g.pos.x, g.pos.y, 2.0f, g.team == Team::MONTERAN ? MON_HELMET : RCH_HELMET);
                        break;
                    default:
                        draw::rect(m_stamps, std::floor(g.pos.x), std::floor(g.pos.y), 2, 1, g.charred ? CHAR : GORE);
                        break;
                }
                g.kind = -1;
            }
        }
    }
    std::erase_if(m_gibs, [](const Gib& g) { return g.kind < 0; });

    // --- drops of blood and clods of earth ---------------------------------------------------------
    for(auto& d : m_drops)
    {
        d.pos += d.vel * dt;
        d.height += d.vz * dt;
        d.vz -= 260.0f * dt;
        if(d.height <= 0.0f)
        {
            if(d.dirt) draw::rect(m_stamps, std::floor(d.pos.x), std::floor(d.pos.y), d.size, 1, Random::chance(0.5f) ? MUD_PALE : MUD_DARK);
            else draw::rect(m_stamps, std::floor(d.pos.x), std::floor(d.pos.y), d.size, d.size, Random::chance(0.5f) ? BLOOD : BLOOD_DK);
            d.height = -1.0f;
        }
    }
    std::erase_if(m_drops, [](const Drop& d) { return d.height < 0.0f; });

    for(auto& p : m_puffs)
    {
        p.pos += p.vel * dt;
        p.vel *= std::exp(-dt * 1.5f);
        p.size += p.grow * dt;
        p.life -= dt;
    }
    std::erase_if(m_puffs, [](const Puff& p) { return p.life <= 0.0f; });
    if(m_puffs.size() > 1500) m_puffs.erase(m_puffs.begin(), m_puffs.begin() + 300);

    for(auto& s : m_sparks)
    {
        s.pos += s.vel * dt;
        s.vel *= std::exp(-dt * 4.0f);
        s.life -= dt;
    }
    std::erase_if(m_sparks, [](const Spark& s) { return s.life <= 0.0f; });

    for(auto& l : m_lights) l.life -= dt;
    std::erase_if(m_lights, [](const Light& l) { return l.life <= 0.0f; });

    for(auto& p : m_popups) { p.life -= dt; p.pos.y -= dt * 8.0f; }
    std::erase_if(m_popups, [](const Popup& p) { return p.life <= 0.0f; });
}

}
