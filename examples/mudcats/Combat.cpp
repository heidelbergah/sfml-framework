/**
 * Combat.cpp  -  Mud Cats
 *
 * Weapons, bullets, wounds, gore, grenades, fire, smoke, flares and shells.
 *
 * The rules, in short:
 *
 *   - Every round that passes close to a man frightens him (suppression),
 *     hit or miss. Frightened men aim worse and want to lie down.
 *   - Cover is checked when a round reaches its target: a trench, a
 *     sandbag or low wall just in front of him or a shell hole he is lying
 *     in may stop it, more likely the lower he is. Hedges hide a man but
 *     only sometimes stop the round.
 *   - A hit lands somewhere: head, body, an arm or a leg. Heavy rounds and
 *     explosions can take a limb off. A man who goes down bleeds out
 *     unless a medic reaches him.
 *   - Explosions do not care about sandbags, only walls; fire does not
 *     care about either.
 *   - Close in, a bayonet. A man with nowhere to go may put his hands up.
 *
 * What these look like is in Effects.cpp: this file only says where.
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

    std::string short_name(const Soldier& s)
    {
        std::string const last = s.name.substr(s.name.find(' ') + 1);
        return s.rank.empty() ? last : s.rank + " " + last;
    }

    const char* what_he_was(const Soldier& s)
    {
        switch(s.role)
        {
            case Role::OFFICER: return "an officer";
            case Role::GUNNER:  return "a machine gunner";
            case Role::FLAMER:  return "a flamethrower";
            case Role::MEDIC:   return "a medic";
            default:            return "a rifleman";
        }
    }
}

// ---------------------------------------------------------------- shooting

void Battle::reload(Soldier& s)
{
    const WeaponSpec& w = s.weapon();
    if(w.flame || s.gun == Gun::NONE || s.reloading > 0.0f || s.ammo >= w.magazine || s.spare <= 0) return;
    s.reloading = w.reload * (s.lost & (ARM_L | ARM_R) ? 2.0f : 1.0f);
    SoundBank::get().play_at(w.reload_sound, s.pos, s.is_player ? 0.8f : 0.5f);
    if(!s.is_player && Random::chance(0.4f)) bark(s, s.team == Team::MONTERAN ? "Reloading!" : "Vash!", false);
}

void Battle::fire(Soldier& s, float dt)
{
    const WeaponSpec& w = s.weapon();
    if(s.gun == Gun::NONE || s.lunge > 0.0f) { s.firing = false; return; }

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
        s.revealed = 3.0f;
        s.fuel = std::max(0.0f, s.fuel - 13.0f * dt);
        s.cooldown -= dt;
        while(s.cooldown <= 0.0f)
        {
            s.cooldown += w.interval;
            float const a = s.aim + Random::normal(0.0f, w.spread);
            Flame f;
            // From the nozzle, unless the nozzle is through a wall.
            f.pos = m_map.wall_hit(s.pos, s.pos + dir(s.aim) * 8.0f) ? s.pos : s.pos + dir(s.aim) * 8.0f;
            f.vel = dir(a) * w.speed * Random::real(0.8f, 1.2f) + s.vel * 0.5f;
            f.max_life = f.life = Random::real(0.4f, 0.55f);
            f.owner = s.id;
            f.team = s.team;
            m_flames.push_back(f);
        }
        // The pilot light at the nozzle, and the roar of it lighting the ground.
        flame_lick(s.pos + dir(s.aim) * 8.0f, dir(s.aim) * 30.0f, 2.0f, 0.08f);
        flame_lick(s.pos + dir(s.aim) * 10.0f, dir(s.aim) * 60.0f, 3.0f, 0.12f);
        m_lights.push_back({s.pos + dir(s.aim) * 20.0f, 34.0f, 0.06f, 0.06f, FIRE_ORANGE});
        hear_shot(s);
        if(length(s.pos - player()->pos) < 300.0f) m_combat_heat = std::min(1.0f, m_combat_heat + dt * 0.3f);
        return;
    }

    if(!s.firing) return;
    if(s.reloading > 0.0f || s.cooldown > 0.0f) return;
    if(s.has_mg() && (!s.deployed || s.overheated > 0.0f)) return;
    if(!s.can_shoot()) return;

    if(s.ammo <= 0)
    {
        if(s.is_player) SoundBank::get().play("click_empty", 0.6f);
        s.cooldown = 0.35f;
        if(s.spare > 0) reload(s);
        else if(s.is_player) notice("Out of ammunition. E on a crate, or on a fallen man's weapon", 3.0f);
        return;
    }

    fire_bullet(s, s.aim + Random::normal(0.0f, spread_of(s)));

    --s.ammo;
    ++s.shots_fired;
    // Their gun is a faster one: you can hear the difference.
    s.cooldown = w.interval * (s.has_mg() && s.team == Team::RECHIVII ? 0.75f : 1.0f);
    s.kick += w.kick;
    s.flash = 0.06f;
    s.revealed = 2.5f;
    if(s.has_mg())
    {
        s.heat += s.team == Team::RECHIVII ? 0.018f : 0.022f;
        if(s.heat >= 1.0f)
        {
            s.overheated = 4.0f;
            SoundBank::get().play_at("overheat", s.pos, 0.8f);
            if(s.is_player) hint("The barrel is too hot. Wait for it to cool. Fire in bursts.", 3.5f);
        }
    }

    muzzle(s);
    m_lights.push_back({s.pos + dir(s.aim) * 9.0f, s.has_mg() ? 26.0f : 20.0f, 0.05f, 0.05f, FIRE_YELLOW});

    SoundBank::get().shot_at(w.sound, w.far_sound, s.pos, s.is_player ? 0.9f : 0.75f);
    if((s.gun == Gun::RIFLE || s.gun == Gun::SNIPER) && s.is_player) m_later.push_back({0.45f, "rifle_bolt", s.pos, 0.6f});
    if(s.gun == Gun::SNIPER) s.revealed = 4.0f;                     // that crack carries
    // The brass, a beat later, if it is close enough to hear.
    if(length(s.pos - player()->pos) < 80.0f && Random::chance(s.has_mg() ? 0.3f : 0.6f))
        m_later.push_back({Random::real(0.25f, 0.45f), "casing", s.pos, 0.25f});
    hear_shot(s);

    if(s.is_player) m_camera.shake(s.has_mg() ? 0.05f : (s.gun == Gun::RIFLE || s.gun == Gun::SNIPER ? 0.1f : 0.04f));
    if(length(s.pos - player()->pos) < 380.0f) m_combat_heat = std::min(1.0f, m_combat_heat + 0.03f);

    // Bolt actions and pistols need a fresh pull each time; let go of it.
    if(!w.automatic) s.firing = false;
}

void Battle::fire_bullet(Soldier& s, float angle)
{
    const WeaponSpec& w = s.weapon();
    Bullet b;
    // From the man himself, not from the end of his barrel: a muzzle can
    // poke through a wall he is standing against, and a round fired from
    // there would never meet the wall.
    b.pos = b.origin = b.prev = s.pos;
    b.vel = dir(angle) * w.speed;
    b.range = w.range * Random::real(0.9f, 1.05f);
    b.damage = w.damage;
    b.suppress = w.suppress;
    b.owner = s.id;
    b.team = s.team;
    b.heavy = s.has_mg();
    b.tracer = w.tracer_every > 0 && s.shots_fired % w.tracer_every == 0;
    m_bullets.push_back(b);
}

void Battle::hear_shot(const Soldier& shooter)
{
    if(!m_alarm) raise_alarm(shooter.team == Team::MONTERAN ? "A shot! They heard it." : "They've seen us!");
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

void Battle::throw_grenade(Soldier& s, sf::Vector2f at, bool smoke)
{
    if(smoke ? s.smokes <= 0 : s.grenades <= 0) return;
    sf::Vector2f d = at - s.pos;
    float const dist = std::min(length(d), 150.0f);
    d = unit(d) * dist;
    float const t = std::clamp(dist / 130.0f, 0.35f, 1.1f);

    Grenade g;
    g.pos = s.pos;
    g.vel = d / t;
    g.height = 5.0f;
    g.vz = 300.0f * t * 0.5f;
    g.fuse = smoke ? Random::real(1.4f, 1.8f) : Random::real(3.0f, 3.6f);
    g.owner = s.id;
    g.team = s.team;
    g.spin = Random::real(-15.0f, 15.0f);
    g.smoke = smoke;
    m_grenades.push_back(g);
    if(smoke) --s.smokes;
    else --s.grenades;
    s.cooldown = std::max(s.cooldown, 0.8f);

    SoundBank::get().play_at("grenade_pin", s.pos, 0.6f);
    SoundBank::get().play_at("throw", s.pos, 0.7f);
    if(smoke) bark(s, s.team == Team::MONTERAN ? "Smoke! Smoke out!" : "Dym!", true);
    else bark(s, s.team == Team::MONTERAN ? "Grenade out!" : "Vrak!", true);
}

void Battle::fire_flare(Soldier& s, sf::Vector2f at, bool illum)
{
    if(illum)
    {
        // A white star to see by: it goes up over no man's land and hangs.
        Flare f;
        f.pos = s.pos;
        f.target = at;
        f.vel = unit(at - s.pos) * std::min(90.0f, std::hypot(at.x - s.pos.x, at.y - s.pos.y) / 1.8f);
        f.height = 4.0f;
        f.vz = 140.0f;
        f.owner = s.id;
        f.illum = true;
        m_flares.push_back(f);
        SoundBank::get().play_at("flare_fire", s.pos, 0.7f);
        return;
    }
    if(s.flares <= 0) return;
    --s.flares;
    Flare f;
    f.pos = s.pos;
    f.target = at;
    f.vel = unit(at - s.pos) * 18.0f;
    f.height = 4.0f;
    f.vz = 150.0f;
    f.owner = s.id;
    m_flares.push_back(f);
    SoundBank::get().play_at("flare_fire", s.pos, 0.9f);
    bark(s, "Red flare! Guns, on that!", true);
    if(length(at - s.pos) < 130.0f && s.is_player)
        hint("Danger close. The guns are not that accurate.", 3.0f);
    log(short_name(s) + " fired a red flare for the guns");
}

// ---------------------------------------------------------------- the bayonet

void Battle::bayonet(Soldier& s)
{
    if(!s.alive() || s.lunge > 0.0f || s.bayonet_cd > 0.0f || s.deployed || s.dragging >= 0 || s.treating >= 0) return;
    if(!s.can_use_rifle() && !s.one_handed()) return;
    if(s.has_mg() || s.has_flamer()) return;
    if(!s.can_walk()) return;
    if(s.stance != Stance::STAND) { s.stance = Stance::STAND; s.stance_change = 0.1f; }
    s.lunge = 0.36f;
    s.lunge_dir = dir(s.aim);
    s.bayonet_cd = 0.95f;
    s.firing = false;
    SoundBank::get().play_at("bayonet_swish", s.pos, s.is_player ? 0.8f : 0.5f);
    if(Random::chance(0.5f)) SoundBank::get().play_at("grunt", s.pos, 0.6f, 0.1f);
}

void Battle::update_bayonet(Soldier& s, float dt)
{
    s.bayonet_cd = std::max(0.0f, s.bayonet_cd - dt);
    if(s.lunge <= 0.0f) return;
    float const before = s.lunge;
    s.lunge -= dt;

    // The thrust: a step forward, the point leading.
    if(s.lunge > 0.16f) s.vel = s.lunge_dir * 115.0f;
    else s.vel *= std::exp(-dt * 12.0f);

    if(before > 0.2f && s.lunge <= 0.2f)
    {
        Soldier* victim = nullptr;
        float best = 13.0f;
        for(auto& o : m_soldiers)
        {
            if(o.team == s.team || (o.life != Life::OK && o.life != Life::DOWNED)) continue;
            sf::Vector2f const d = o.pos - s.pos;
            float const l = length(d);
            if(l > best) continue;
            if(l > 3.0f && (d.x * s.lunge_dir.x + d.y * s.lunge_dir.y) / l < 0.45f) continue;
            best = l;
            victim = &o;
        }
        if(!victim)
        {
            if(s.is_player) m_camera.shake(0.05f);
            return;
        }
        Soldier& v = *victim;
        bool const blade = s.gun == Gun::RIFLE;
        sf::Vector2f const push = s.lunge_dir;
        spray_blood(v.pos, push, blade ? 18 : 8, 70.0f);
        SoundBank::get().play_at(blade ? "stab" : "club_hit", v.pos, 0.95f, 0.06f);
        if(s.is_player) m_camera.shake(0.3f);
        v.vel += push * 60.0f;
        v.suppression = 1.0f;
        v.shock = std::max(v.shock, 0.6f);

        if(v.surrendered && v.team == Team::RECHIVII && s.team == Team::MONTERAN)
        {
            ++Campaign::current().prisoners_shot;
            log(short_name(s) + " bayoneted a prisoner");
        }
        if(v.life == Life::DOWNED || (blade && Random::chance(0.45f)))
        {
            SoundBank::get().play_at("death_cry", v.pos, 0.9f, 0.08f);
            kill(v, s.id, blade ? "Bayoneted" : "Beaten to death");
            return;
        }
        if(blade)
        {
            v.hp -= 70.0f;
            v.bleeding += 4.0f;
            if(v.hp <= 0.0f) go_down(v, s.id);
            else SoundBank::get().play_at("scream", v.pos, 1.0f, 0.06f);
        }
        else
        {
            v.hp -= 30.0f;
            v.shock = 1.0f;
            if(v.stance == Stance::STAND) v.stance = Stance::CROUCH;
            if(v.hp <= 0.0f) go_down(v, s.id);
        }
    }
}

// ---------------------------------------------------------------- weapons on the ground

void Battle::drop_weapon(Soldier& s)
{
    if(s.gun == Gun::NONE) return;
    Dropped d;
    d.pos = s.pos + sf::Vector2f{Random::real(-3, 3), Random::real(-3, 3)};
    d.gun = s.gun;
    d.ammo = s.ammo;
    d.spare = s.spare;
    d.fuel = s.fuel;
    d.angle = s.aim + Random::real(-0.8f, 0.8f);
    // A burst tank is no use to anyone.
    if(!(d.gun == Gun::FLAMER && (s.charred || d.fuel < 5.0f))) m_dropped.push_back(d);
    s.gun = Gun::NONE;
    s.ammo = s.spare = 0;
    s.fuel = 0.0f;
    s.deployed = false;
    s.deploying = 0.0f;

    if(m_dropped.size() > 160) m_dropped.erase(m_dropped.begin(), m_dropped.begin() + 20);
}

bool Battle::pick_up(Soldier& s, Dropped& d)
{
    if(d.taken) return false;
    const WeaponSpec& theirs = weapon_spec(d.gun);
    bool const two_hands = !(d.gun == Gun::PISTOL || d.gun == Gun::SMG);
    if(two_hands && !s.can_use_rifle())
    {
        if(s.is_player) notice("You can't hold that with one arm", 2.0f);
        return false;
    }

    if(d.gun == s.gun)
    {
        // The same weapon: take his rounds.
        if(d.gun == Gun::FLAMER) s.fuel = std::min(100.0f, s.fuel + d.fuel);
        int const rounds = d.ammo + d.spare;
        if(rounds <= 0 && d.gun != Gun::FLAMER) return false;
        s.spare = std::min(s.spare + rounds, theirs.spare * 2);
        d.taken = true;
        SoundBank::get().play_at("ammo_pickup", s.pos, 0.7f);
        if(s.is_player) notice("Took " + std::to_string(rounds) + " rounds", 2.0f);
        return true;
    }

    // Swap: put yours down, take his.
    Dropped const taking = d;
    d.taken = true;
    drop_weapon(s);
    s.gun = taking.gun;
    s.ammo = taking.ammo;
    s.spare = taking.spare;
    s.fuel = taking.fuel;
    s.reloading = 0.0f;
    s.heat = 0.0f;
    s.overheated = 0.0f;
    s.cooldown = 0.4f;
    SoundBank::get().play_at("weapon_pickup", s.pos, 0.8f);
    if(s.is_player)
    {
        notice(std::string("Picked up a ") + gun_name(s.gun), 2.5f);
        if(s.has_mg()) hint("Right click sets it up. It only fires set up.", 4.0f);
    }
    return true;
}

// ---------------------------------------------------------------- bullets

void Battle::update_bullets(float dt)
{
    Soldier const& me = *player();

    for(auto& b : m_bullets)
    {
        b.prev = b.pos;
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

        // A hedge takes some of them.
        if(end >= 1.0f && m_map.crosses_hedge(a, c) && Random::chance(0.22f))
        {
            sf::Vector2f const at = a + (c - a) * Random::real(0.3f, 0.8f);
            dust(at, 2, HEDGE_LT);
            if(length(at - me.pos) < 160.0f) SoundBank::get().play_at("impact_leaves", at, 0.4f);
            b.dead = true;
            continue;
        }

        // Bottles on the range.
        for(auto& bottle : m_map.bottles)
        {
            sf::Vector2f at;
            if(!bottle.broken && segment_circle(a, a + (c - a) * end, bottle.pos, 2.5f, &at))
            {
                bottle.broken = true;
                --m_bottles_left;
                SoundBank::get().play_at("bottle_break", at, 0.9f);
                sparks(at, 14, 60.0f, lerp(GRASS, MON_WHITE, 0.6f), 0.4f);
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
            if(s.team == b.team || !s.alive()) continue;
            auto const id = static_cast<std::size_t>(s.id);
            if(id >= MAX_SOLDIERS || b.passed.test(id)) continue;
            float const d = distance_to_segment(s.pos, a, stop);
            if(d > 24.0f) continue;
            b.passed.set(id);
            bool const covered = m_map.in_trench(s.pos) || (s.stance == Stance::PRONE && m_map.in_crater(s.pos));
            s.suppression = std::min(1.0f, s.suppression + b.suppress * (1.0f - d / 24.0f) * (covered ? 0.55f : 1.0f));
            s.threat = b.origin;
            s.threat_age = 0.0f;
            if(s.is_player && victim != &s)
            {
                if(d < 12.0f) SoundBank::get().play(b.heavy ? "snap_heavy" : "snap", 0.75f, 0.1f);
                else SoundBank::get().play("whiz", 0.5f, 0.1f);
                m_combat_heat = std::min(1.0f, m_combat_heat + 0.08f);
                // The ground around you jumps: earth kicked up where it would
                // have gone in, a spark off a stone.
                if(!m_map.in_trench(s.pos) || Random::chance(0.4f))
                {
                    sf::Vector2f const along = unit(b.vel);
                    sf::Vector2f const at = s.pos + along * Random::real(6.0f, 22.0f) + sf::Vector2f{-along.y, along.x} * Random::real(-8.0f, 8.0f);
                    dust(at, 3, m_level == LevelId::FIELDS ? MEADOW_DK : MUD_PALE);
                    if(Random::chance(0.35f)) sparks(at, 2, 60.0f, FIRE_YELLOW, 0.12f);
                }
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
            if(s.surrendered) block = 0.0f;

            if(Random::chance(block))
            {
                sf::Vector2f const at = victim_at - unit(b.vel) * 3.0f;
                dust(at, 4, m_level == LevelId::FIELDS ? STONE : MUD_PALE);
                if(Random::chance(0.3f)) sparks(at, 2, 40.0f, FIRE_YELLOW, 0.12f);
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
            dust(at - unit(b.vel) * 1.5f, 3, m_level == LevelId::FIELDS ? STONE : lerp(CONCRETE, BRICK, 0.5f));
            sparks(at - unit(b.vel) * 1.0f, Random::integer(1, 3), 50.0f, FIRE_YELLOW, 0.12f);
            if(length(at - me.pos) < 200.0f && Random::chance(0.35f)) SoundBank::get().play_at("ricochet", at, 0.5f);
            else SoundBank::get().play_at("impact_wood", at, 0.3f);
            b.dead = true;
            continue;
        }

        b.pos = c;
        b.range -= seg;
        if(b.range <= 0.5f)
        {
            if(m_map.in_water(b.pos))
            {
                sparks(b.pos, 5, 30.0f, STREAM_LT, 0.3f);
                if(length(b.pos - me.pos) < 160.0f) SoundBank::get().play_at("splash", b.pos, 0.3f);
            }
            else
            {
                dust(b.pos, 2, m_level == LevelId::FIELDS ? MEADOW_DK : MUD_PALE);
                if(length(b.pos - me.pos) < 120.0f) SoundBank::get().play_at("impact_dirt", b.pos, 0.35f);
            }
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
    float const head_chance = s.stance == Stance::PRONE ? 0.22f : (s.stance == Stance::CROUCH ? 0.16f : 0.11f);
    if(r < head_chance) cause = 0;
    else if(r < head_chance + 0.45f) cause = 1;
    else if(r < head_chance + 0.67f) cause = 2;
    else cause = 3;

    spray_blood(at, d, cause == 0 ? 16 : 9, 75.0f);
    SoundBank::get().play_at("impact_flesh", at, 0.8f);

    // Shooting a man with his hands up is remembered.
    if(s.surrendered && b.owner >= 0 && m_soldiers[static_cast<std::size_t>(b.owner)].team == Team::MONTERAN)
    {
        ++Campaign::current().prisoners_shot;
        log(short_name(m_soldiers[static_cast<std::size_t>(b.owner)]) + " shot a man who had surrendered");
        kill(s, b.owner, "Shot with his hands up");
        return;
    }
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
        static const float scale[] = {0.6f, 1.0f, 1.3f};
        damage *= scale[std::clamp(Campaign::current().difficulty, 0, 2)];
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
        if(s.has_flamer() && s.fuel > 5.0f && Random::chance(0.07f))
        {
            explode(s.pos, 1.1f, attacker, s.team == Team::MONTERAN ? Team::RECHIVII : Team::MONTERAN, true, "his own tanks bursting");
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
            // Without both arms, a rifle is no use; he drops it.
            if(arm && !s.one_handed() && s.gun != Gun::NONE) drop_weapon(s);
        }
    }

    if(s.hp <= 0.0f)
    {
        if(s.hp < -45.0f) kill(s, attacker, heavy ? "Cut down by machine-gun fire" : "Shot dead");
        else go_down(s, attacker);
        return;
    }

    if(!s.can_walk()) s.stance = Stance::PRONE;
    if(Random::chance(0.75f)) SoundBank::get().play_at(Random::chance(0.6f) ? "scream" : "groan", s.pos, 0.75f, 0.08f);
    static const char* const ours[] = {"I'm hit!", "Agh! God!", "My arm, my arm!", "Ahh, Christ!", "I'm hit, I'm hit!"};
    static const char* const theirs[] = {"Ahh!", "Mama!", "Bozhe!"};
    bark(s, s.team == Team::MONTERAN ? ours[Random::integer(0, 4)] : theirs[Random::integer(0, 2)], false);
    if(s.is_player) hint(s.bleeding > 0.0f && s.bandages > 0 ? "You are bleeding. F to bandage yourself, somewhere safe." : "You are hit.", 4.0f);
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

    spray_blood(s.pos, push, 24, 95.0f);
    SoundBank::get().play_at("gore", s.pos, 0.9f, 0.08f);
    if(limb != HEAD && s.life == Life::OK) SoundBank::get().play_at("scream", s.pos, 1.0f, 0.06f);
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
    spray_blood(s.pos, push, 44, 135.0f);
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
    s.lunge = 0.0f;
    s.burning = std::min(s.burning, 0.0f);
    s.bleed_out = (s.lost & (ARM_L | ARM_R | LEG_L | LEG_R)) ? 16.0f : 32.0f;
    if(s.dragging >= 0) { m_soldiers[static_cast<std::size_t>(s.dragging)].dragged_by = -1; s.dragging = -1; }
    s.treating = -1;

    SoundBank::get().play_at("body_fall", s.pos, 0.7f);
    SoundBank::get().play_at("scream", s.pos, 0.9f, 0.05f);
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
    s.lunge = 0.0f;
    if(!was_down) s.body_angle = s.aim + Random::real(-0.6f, 0.6f) + 3.14159f;
    if(s.dragging >= 0) { m_soldiers[static_cast<std::size_t>(s.dragging)].dragged_by = -1; s.dragging = -1; }
    if(s.dragged_by >= 0) { m_soldiers[static_cast<std::size_t>(s.dragged_by)].dragging = -1; s.dragged_by = -1; }
    s.treating = -1;

    Soldier* killer = attacker >= 0 && attacker < static_cast<int>(m_soldiers.size()) ? &m_soldiers[static_cast<std::size_t>(attacker)] : nullptr;
    if(killer && killer->team != s.team) ++killer->kills;
    // Your kill: the camera jolts toward him, and you hear it land.
    if(killer && killer->is_player && killer->team != s.team)
    {
        m_kick += unit(s.pos - killer->pos) * 5.0f;
        m_camera.shake(0.12f);
        SoundBank::get().play("kill_thud", 0.8f, 0.05f);
    }

    if(!was_down)
    {
        SoundBank::get().play_at("body_fall", s.pos, 0.7f);
        if(!s.charred && !(s.lost & HEAD) && Random::chance(0.5f)) SoundBank::get().play_at("death_cry", s.pos, 0.7f, 0.08f);
    }
    drop_weapon(s);
    stamp_corpse(s);

    // The record.
    if(s.team == Team::MONTERAN) log(short_name(s) + ": " + s.fate);
    else if(killer && killer->is_player) log("You killed " + std::string(what_he_was(s)) + (s.role == Role::OFFICER ? ", their lieutenant" : ""));
    else if(s.role == Role::OFFICER) log("Their lieutenant was killed");

    if(s.team == Team::MONTERAN && !s.is_player)
        for(auto& o : m_soldiers)
            if(o.team == s.team && o.alive() && length(o.pos - s.pos) < 90.0f)
            {
                o.morale = std::max(0.0f, o.morale - 0.1f);
                if(o.role == Role::OFFICER || Random::chance(0.3f))
                {
                    static const char* const words[] = {"No! No...", "He's gone.", "Oh God, he's dead.", "Leave him! Leave him!"};
                    bark(o, words[Random::integer(0, 3)], false);
                }
                break;
            }
    if(s.team == Team::RECHIVII)
        for(auto& o : m_soldiers)
            if(o.team == s.team && o.fighting() && length(o.pos - s.pos) < 70.0f) o.morale = std::max(0.0f, o.morale - 0.06f);
    if(s.is_player) m_hurt = 1.0f;
}

void Battle::surrender(Soldier& s)
{
    if(!s.fighting()) return;
    s.surrendered = true;
    drop_weapon(s);
    s.firing = false;
    s.vel = {};
    s.stance = Stance::STAND;
    s.path.clear();
    s.goal = s.pos;
    s.target = -1;
    s.burning = 0.0f;
    s.bark_cooldown = 0.0f;
    static const char* const words[] = {"Sdayus! Sdayus!", "Don't shoot! Don't!", "Nyet, nyet! Please!"};
    bark(s, words[Random::integer(0, 2)], true);
    ++Campaign::current().prisoners;
    log(std::string("A Rechivii ") + (s.role == Role::OFFICER ? "officer" : "soldier") + " surrendered");
    s.fate = "Taken prisoner";
}

void Battle::stamp_corpse(Soldier& s)
{
    // A pool first, then the body on top of it, into the ground for good.
    draw::disc(m_stamps, s.pos.x + Random::real(-1, 1), s.pos.y + Random::real(-1, 1), s.charred ? 3.0f : Random::real(4.0f, 6.0f),
               s.charred ? CHAR : BLOOD_DK);
    if(!s.charred) draw::disc(m_stamps, s.pos.x, s.pos.y, 3.0f, BLOOD);
    draw_soldier(m_stamps, s, m_time);
}

void Battle::ignite(Soldier& s, int attacker)
{
    if(s.life != Life::OK) return;
    if(s.burning <= 0.0f)
    {
        SoundBank::get().play_at("scream_burn", s.pos, 1.0f, 0.04f);
        bark(s, s.team == Team::MONTERAN ? "I'm burning! Help me!" : "AAAH!", false);
        if(s.is_player) hint("You are on fire. Z to drop and roll, it puts it out faster.", 4.0f);
    }
    s.burning = std::max(s.burning, 4.5f);
    s.suppression = 1.0f;
    s.target = attacker;        // remembered for the kill
}

// ---------------------------------------------------------------- explosions

void Battle::explode(sf::Vector2f at, float power, int owner, Team team, bool fire, const std::string& cause, bool spare_monterans)
{
    (void)team;
    Soldier& me = *player();
    float const to_me = length(at - me.pos);
    bool const wet = m_map.in_water(at);

    if(!m_alarm) raise_alarm("The explosion woke them.");
    SoundBank::get().play_at(fire ? "tank_burst" : "explosion", at, std::min(1.0f, 0.6f + power * 0.3f), 0.08f);
    if(to_me > 260.0f) SoundBank::get().play_at("explosion_far", at, 0.7f, 0.05f);
    m_lights.push_back({at, 60.0f * power, 0.25f, 0.25f, fire ? FIRE_ORANGE : FIRE_YELLOW});
    m_camera.shake(std::clamp(1.0f - to_me / 500.0f, 0.0f, 1.0f) * 0.75f * power);
    if(to_me < 110.0f * power)
    {
        float const k = 1.0f - to_me / (110.0f * power);
        SoundBank::get().deafen(1.0f + 3.0f * k);
        m_flash = std::max(m_flash, k);
        me.shock = std::max(me.shock, k);
    }

    // The crater and the scorch, into the ground for good.
    float const r = 6.0f + 6.0f * power;
    if(!wet)
    {
        sf::Color const rim = m_level == LevelId::FIELDS ? lerp(MEADOW_DK, MUD_PALE, 0.5f) : lerp(MUD_PALE, MUD_DARK, 0.3f);
        draw::disc(m_stamps, at.x, at.y, r + 3.0f, rim);
        draw::disc(m_stamps, at.x, at.y, r, MUD_DARK);
        draw::disc(m_stamps, at.x + 1, at.y + 1, r * 0.6f, fire ? CHAR : lerp(MUD_DARK, INK, 0.4f));
    }

    // What it looks like: Effects.cpp.
    if(wet)
    {
        sparks(at, 40, 120.0f * power, STREAM_LT, 0.6f);
        for(int i = 0; i < 10; ++i)
            puff(at + sf::Vector2f{Random::real(-6, 6), Random::real(-6, 6)}, {Random::real(-10, 10), Random::real(-20, -5)},
                 Random::real(3.0f, 5.0f), 6.0f, Random::real(0.8f, 1.6f), with_alpha(MON_WHITE, 0.5f));
    }
    else debris(at, 46, power);
    // The fireball, then the smoke rolling up out of it.
    for(int i = 0; i < 12; ++i)
        flame_lick(at + sf::Vector2f{Random::real(-5, 5), Random::real(-5, 5)} * power,
                   dir(Random::real(0, 6.283f)) * Random::real(10.0f, 45.0f) * power, Random::real(3.0f, 6.0f) * power, Random::real(0.15f, 0.35f));
    for(int i = 0; i < 16; ++i)
        puff(at + sf::Vector2f{Random::real(-7, 7), Random::real(-7, 7)}, {Random::real(-14, 14), Random::real(-24, -4)},
             Random::real(3.0f, 6.0f) * power, Random::real(5.0f, 11.0f), Random::real(2.5f, 5.0f),
             with_alpha(i < 5 ? lerp(SMOKE, INK, 0.5f) : SMOKE, Random::real(0.45f, 0.7f)));
    sparks(at, 22, 190.0f * power, FIRE_YELLOW, 0.3f);

    if(fire)
    {
        for(int i = 0; i < 5; ++i)
            m_fires.push_back({at + sf::Vector2f{Random::real(-14, 14), Random::real(-14, 14)}, Random::real(5, 9), Random::real(4, 7), 0.0f});
        for(int i = 0; i < 26; ++i)
        {
            Flame f;
            f.pos = at;
            f.vel = dir(Random::real(0, 6.283f)) * Random::real(20.0f, 75.0f);
            f.max_life = f.life = Random::real(0.4f, 0.8f);
            f.owner = -1;                 // burns everyone
            f.team = Team::MONTERAN;
            m_flames.push_back(f);
        }
    }

    for(auto& s : m_soldiers)
    {
        if(s.life != Life::OK && s.life != Life::DOWNED) continue;
        if(spare_monterans && s.team == Team::MONTERAN) continue;
        float const d = length(s.pos - at);
        if(d > 120.0f * power) continue;
        // Walls take it; a satchel charge, close up, goes through them.
        if(!m_map.line_of_sight(at, s.pos) && m_map.wall_hit(at, s.pos) && !(power >= 2.0f && d < 36.0f)) continue;

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
            float dmg = 150.0f * (1.0f - d / reach) * shelter;
            if(s.is_player)
            {
                static const float scale[] = {0.6f, 1.0f, 1.3f};
                dmg *= scale[std::clamp(Campaign::current().difficulty, 0, 2)];
            }
            if(s.life == Life::DOWNED) { if(dmg > 30.0f) kill(s, owner, "Killed by " + cause + " as he lay wounded"); continue; }
            // Fragments find arms and legs.
            for(Limb l : {ARM_L, ARM_R, LEG_L, LEG_R})
                if(dmg > 55.0f && Random::chance(0.3f)) sever(s, l, push);
            s.hp -= dmg;
            s.bleeding += dmg * 0.03f;
            spray_blood(s.pos, push, 12, 85.0f);
            if(s.is_player) m_hurt = 1.0f;
            if(s.hp <= 0.0f)
            {
                if(s.hp < -40.0f) kill(s, owner, "Killed by " + cause);
                else go_down(s, owner);
            }
            else
            {
                if(!s.can_walk()) s.stance = Stance::PRONE;
                if(!s.can_use_rifle() && !s.one_handed()) drop_weapon(s);
                if(Random::chance(0.6f)) SoundBank::get().play_at("scream", s.pos, 0.9f, 0.06f);
            }
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
    s.flash         = std::max(0.0f, s.flash - (s.has_flamer() ? 0.0f : dt));
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
        if(Random::chance(dt / 4.0f))
        {
            const char* sound = "groan";
            if(s.team == Team::MONTERAN && Random::chance(0.45f)) sound = "medic_call";
            else if(s.bleed_out < 10.0f) sound = "sob";
            SoundBank::get().play_at(sound, s.pos, 0.6f, 0.06f);
        }
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
        else if(s.team == Team::MONTERAN && (s.can_walk() || s.can_use_rifle()))
        {
            // He drags himself toward the nearest medic, if one is close.
            const Soldier* medic = nullptr;
            float best = 260.0f;
            for(const auto& o : m_soldiers)
                if(o.team == s.team && o.alive() && o.role == Role::MEDIC && length(o.pos - s.pos) < best) { best = length(o.pos - s.pos); medic = &o; }
            if(medic && best > 12.0f)
            {
                sf::Vector2f const d = unit(medic->pos - s.pos);
                s.pos += d * 5.0f * dt;
                s.body_angle = std::atan2(d.y, d.x);
                m_map.collide(s.pos, 3.0f);
                if(Random::chance(dt * 4.0f)) draw::rect(m_stamps, std::floor(s.pos.x), std::floor(s.pos.y), 2, 1, BLOOD_DK);
            }
        }
        if(s.bleed_out <= 0.0f) kill(s, -1, s.lost ? "Bled to death from his wounds" : "Bled out waiting for a medic");
        return;
    }

    // --- alive --------------------------------------------------------------
    const WeaponSpec& w = s.weapon();
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

    // Cutting wire: kneeling at it, snipping strand by strand.
    if(s.cutting > 0.0f)
    {
        s.vel *= std::exp(-dt * 12.0f);
        if(length(s.cut_at - s.pos) > 14.0f) s.cutting = 0.0f;
        else
        {
            float const before = s.cutting;
            s.cutting -= dt;
            if(std::floor(before / 0.6f) != std::floor(s.cutting / 0.6f)) SoundBank::get().play_at("wire_snip", s.cut_at, 0.5f, 0.1f);
            if(s.cutting <= 0.0f)
            {
                cut_wire_at(s, s.cut_at);
                if(s.is_player) notice("Wire's cut", 2.0f);
                else bark(s, "Through! Gap here!", true);
            }
        }
    }

    // Digging in: a shovel going, spoil flying.
    if(s.digging > 0.0f)
    {
        s.vel *= std::exp(-dt * 12.0f);
        if(s.life != Life::OK) s.digging = 0.0f;
        else
        {
            float const before = s.digging;
            s.digging -= dt;
            if(std::floor(before / 0.7f) != std::floor(s.digging / 0.7f))
            {
                SoundBank::get().play_at("shovel", s.pos, 0.45f, 0.12f);
                dust(s.dig_rect.getCenter(), 3, MUD_LIGHT);
            }
            if(s.digging <= 0.0f) { s.digging = 0.0f; finish_dig(s); }
        }
    }

    // Fear fades, faster in cover and near a leader.
    float calm = 0.18f;
    if(m_map.in_trench(s.pos) || s.stance == Stance::PRONE) calm *= 1.3f;
    if(Soldier* lt = officer(s.team); lt && lt->id != s.id && length(lt->pos - s.pos) < 80.0f) calm *= 1.6f;
    else if(Soldier* cpl = section_leader(s.section); cpl && cpl->id != s.id && length(cpl->pos - s.pos) < 60.0f) calm *= 1.3f;
    if(s.team == Team::MONTERAN && !officer(Team::MONTERAN)) calm *= 0.8f;
    if(s.surrendered) calm = 0.5f;
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
        {
            bool const wet = m_map.in_water(s.pos);
            SoundBank::get().play_at(wet ? "wade" : (m_level == LevelId::TRENCHES ? "footstep" : "footstep_grass"),
                                     s.pos, s.is_player ? 0.35f : 0.18f, 0.1f);
            if(wet && Random::chance(0.5f)) sparks(s.pos, 3, 20.0f, STREAM_LT, 0.25f);
        }
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
        s.burning -= dt * (s.stance == Stance::PRONE ? 2.5f : 1.0f) * (m_map.in_water(s.pos) ? 6.0f : 1.0f);
        s.hp -= 21.0f * dt;
        if(Random::chance(dt * 35.0f))
            flame_lick(s.pos + sf::Vector2f{Random::real(-3, 3), Random::real(-3, 3)}, {Random::real(-5, 5), Random::real(-28, -12)},
                       Random::real(1.5f, 3.0f), Random::real(0.25f, 0.45f));
        if(Random::chance(dt * 6.0f))
            puff(s.pos, {Random::real(-4, 4), Random::real(-16, -8)}, 2.0f, 4.0f, 1.2f, with_alpha(SMOKE, 0.45f));
        if(Random::chance(dt * 1.5f)) SoundBank::get().play_at("scream_burn", s.pos, 0.9f, 0.1f);
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
                     && length(patient.pos - s.pos) < 16.0f && length(s.vel) < 5.0f && (s.medkits > 0 || (s.role != Role::MEDIC && s.bandages > 0));
        if(!ok) { s.treating = -1; }
        else
        {
            s.treat_time -= dt;
            if(s.treat_time <= 0.0f)
            {
                // A medic's kit, or a man's own field dressing on a mate.
                bool const dressing = s.medkits <= 0;
                if(dressing) --s.bandages;
                else --s.medkits;
                patient.bleeding = 0.0f;
                patient.bandaged = true;
                if(patient.life == Life::DOWNED)
                {
                    patient.life = Life::OK;
                    patient.hp = dressing ? 25.0f : 35.0f;
                    patient.bleed_out = 0.0f;
                    patient.stance = patient.can_walk() ? Stance::CROUCH : Stance::PRONE;
                    patient.suppression = 0.5f;
                    if(patient.dragged_by >= 0) { m_soldiers[static_cast<std::size_t>(patient.dragged_by)].dragging = -1; patient.dragged_by = -1; }
                    if(patient.is_player) hint(s.name + " patched you up. Back into it.", 4.0f);
                    if(patient.team == Team::MONTERAN) log(short_name(s) + " saved " + short_name(patient));
                }
                bark(s, "You'll live. Go!", false);
                s.treating = -1;
            }
        }
    }
}

// ---------------------------------------------------------------- the world

void Battle::update_world(float dt)
{
    // --- separation: men do not stand inside each other ---------------------------
    for(std::size_t i = 0; i < m_soldiers.size(); ++i)
        for(std::size_t j = i + 1; j < m_soldiers.size(); ++j)
        {
            Soldier& a = m_soldiers[i];
            Soldier& b = m_soldiers[j];
            if(!a.alive() || !b.alive()) continue;
            sf::Vector2f const d = b.pos - a.pos;
            if(std::abs(d.x) > 6.0f || std::abs(d.y) > 6.0f) continue;
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

    // --- men out of ammunition get some from the man beside them ------------------------
    for(auto& s : m_soldiers)
    {
        if(!s.fighting() || s.gun == Gun::NONE || s.has_flamer() || s.ammo + s.spare > 0) continue;
        for(auto& o : m_soldiers)
        {
            if(o.id == s.id || o.team != s.team || !o.fighting() || o.gun != s.gun) continue;
            int const mag = s.weapon().magazine;
            if(o.spare < mag * 2 || length(o.pos - s.pos) > 22.0f) continue;
            o.spare -= mag;
            s.spare += mag;
            bark(o, s.team == Team::MONTERAN ? "Here! Take these!" : "Derzhi!", false);
            SoundBank::get().play_at("ammo_pickup", s.pos, 0.6f);
            if(s.is_player) notice(o.name.substr(o.name.find(' ') + 1) + " gave you a magazine", 2.5f);
            break;
        }
    }

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
            if(std::abs(g.vz) > 30.0f) SoundBank::get().play_at(m_map.in_water(g.pos) ? "splash" : "grenade_bounce", g.pos, 0.5f);
            g.vz = -g.vz * 0.25f;
            g.vel *= m_map.in_trench(g.pos) ? 0.2f : 0.45f;
            if(std::abs(g.vz) < 10.0f) g.vz = 0.0f;
        }
        g.spin *= std::exp(-dt * 2.0f);
        g.fuse -= dt;
        if(g.fuse <= 0.0f)
        {
            if(g.satchel)
            {
                explode(g.pos, 2.2f, g.owner, g.team, false, "a satchel charge");
                cut_wire_at(m_soldiers[static_cast<std::size_t>(std::max(0, g.owner))], g.pos);
                if(m_map.bunker_area.contains(g.pos) || m_map.under_roof(g.pos)) log("A satchel charge went off inside");
            }
            else if(g.smoke)
            {
                m_smokes.push_back({g.pos, 4.0f, 24.0f, 0.0f});
                SoundBank::get().play_at("smoke_pop", g.pos, 0.8f);
                sparks(g.pos, 6, 40.0f, MON_WHITE, 0.2f);
            }
            else explode(g.pos, 1.0f, g.owner, g.team, false, "a grenade");
        }
    }
    std::erase_if(m_grenades, [](const Grenade& g) { return g.fuse <= 0.0f; });

    // --- smoke screens ----------------------------------------------------------------
    float hiss = 0.0f;
    for(auto& sm : m_smokes)
    {
        sm.life -= dt;
        // Rolls out to its full size over a few seconds, thins at the end.
        float const full = 34.0f;
        if(sm.life > 4.0f) sm.radius = std::min(full, sm.radius + dt * 12.0f);
        else sm.radius = std::max(0.0f, sm.radius - dt * 6.0f);
        if(sm.life > 16.0f) hiss = std::max(hiss, SoundBank::get().attenuation(sm.pos));
    }
    std::erase_if(m_smokes, [](const Smoke& s) { return s.life <= 0.0f; });
    SoundBank::get().set_loop("smoke_hiss", hiss * 0.5f);

    // --- flares -------------------------------------------------------------------------
    for(auto& f : m_flares)
    {
        f.life += dt;
        f.pos += f.vel * dt;
        f.height += f.vz * dt;
        if(!f.fired)
        {
            f.vz -= 110.0f * dt;
            if(f.vz <= 0.0f)
            {
                // The star bursts: a red light hanging over the field, and
                // the guns behind us answer it.
                f.fired = true;
                f.vz = f.illum ? -4.0f : -7.0f;
                f.life = 0.0f;
                SoundBank::get().play_at("flare_pop", f.pos, 0.7f);
                sparks(f.pos - sf::Vector2f{0.0f, f.height}, 16, 50.0f, f.illum ? FIRE_WHITE : FIRE_RED, 0.6f);
                for(int i = 0; i < (f.illum ? 0 : 4); ++i)
                {
                    sf::Vector2f const t = f.target + dir(Random::real(0.0f, 6.283f)) * Random::real(0.0f, 55.0f);
                    m_shells.push_back({t, 4.5f + static_cast<float>(i) * 0.8f + Random::real(0.0f, 0.4f), false, 2});
                }
            }
        }
        else
        {
            f.vel *= std::exp(-dt * 0.5f);
            if(Random::chance(dt * 20.0f))
                flame_lick(f.pos - sf::Vector2f{0.0f, f.height}, {Random::real(-3, 3), Random::real(2, 8)}, 1.5f, 0.35f);
            m_lights.push_back({f.pos, f.illum ? 210.0f : 90.0f, dt * 1.5f, dt * 1.5f, f.illum ? sf::Color(235, 238, 255) : FIRE_RED});
        }
    }
    std::erase_if(m_flares, [](const Flare& f) { return f.fired && (f.life > (f.illum ? 10.0f : 8.0f) || f.height <= 0.0f); });

    // --- shells -------------------------------------------------------------------------------
    for(auto& sh : m_shells)
    {
        sh.time -= dt;
        if(!sh.whistled && sh.time < (sh.kind == 4 ? 1.1f : 1.8f))
        {
            sh.whistled = true;
            float const d = length(sh.target - player()->pos);
            SoundBank::get().play(sh.kind == 4 ? "mortar_incoming" : "shell_incoming", std::clamp(1.0f - d / 700.0f, 0.15f, 0.9f) * (sh.kind == 1 ? 0.5f : 1.0f), 0.05f);
        }
        if(sh.time <= 0.0f)
        {
            if(sh.smoke)
            {
                // A smoke shell: a soft thump, and a wall of white.
                SoundBank::get().play_at("smoke_pop", sh.target, 1.0f);
                for(int k = 0; k < 3; ++k)
                    m_smokes.push_back({sh.target + sf::Vector2f{Random::real(-20, 20), Random::real(-20, 20)}, 8.0f, 30.0f, 0.0f});
                debris(sh.target, 12, 0.6f);
            }
            else
                explode(sh.target, sh.kind == 1 ? 1.3f : (sh.kind == 4 ? 0.9f : 1.5f), -1, Team::RECHIVII, false,
                        sh.kind == 0 ? "a shell" : (sh.kind == 3 ? "a shell he called down" : (sh.kind == 4 ? "a mortar bomb" : "our own guns")), sh.kind == 1);
        }
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
        if(m_map.in_water(f.pos)) f.life = std::min(f.life, 0.05f);

        for(auto& s : m_soldiers)
        {
            if(s.life != Life::OK && s.life != Life::DOWNED) continue;
            if(s.id == f.owner) continue;
            if(f.owner >= 0 && s.team == m_soldiers[static_cast<std::size_t>(f.owner)].team && age < 0.6f) continue;
            if(std::abs(s.pos.x - f.pos.x) > 5.0f || std::abs(s.pos.y - f.pos.y) > 5.0f) continue;
            if(length(s.pos - f.pos) < 5.0f)
            {
                if(s.life == Life::DOWNED) { s.charred = true; kill(s, f.owner, "Burned where he lay"); }
                else ignite(s, f.owner);
            }
        }
        if(f.life <= 0.0f && Random::chance(0.12f) && !m_map.in_water(f.pos))
        {
            bool const dry = m_map.in_grass(f.pos);
            m_fires.push_back({f.pos, Random::real(4, 7), Random::real(4, 8) * (dry ? 1.6f : 1.0f), 0.0f});
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
        // Rain puts them out sooner.
        fire.life -= dt * (m_level == LevelId::TRENCHES ? 1.3f : 1.0f);
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
            spray_blood(g.pos, g.vel, 1, 10.0f);
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

    std::erase_if(m_dropped, [](const Dropped& d) { return d.taken; });

    for(auto& l : m_lights) l.life -= dt;
    std::erase_if(m_lights, [](const Light& l) { return l.life <= 0.0f; });
}


// ---------------------------------------------------------------- sappers

void Battle::place_satchel(Soldier& s, sf::Vector2f at)
{
    if(s.satchels <= 0) return;
    // Thrown, but not far: a satchel is heavy.
    sf::Vector2f d = at - s.pos;
    float const dist = std::min(length(d), 40.0f);
    d = unit(d) * dist;
    Grenade g;
    g.pos = s.pos;
    g.vel = d / 0.45f;
    g.height = 4.0f;
    g.vz = 70.0f;
    g.fuse = 4.5f;
    g.owner = s.id;
    g.team = s.team;
    g.satchel = true;
    m_grenades.push_back(g);
    --s.satchels;
    s.cooldown = std::max(s.cooldown, 1.0f);
    SoundBank::get().play_at("satchel_fuse", s.pos, 0.8f);
    SoundBank::get().play_at("throw", s.pos, 0.6f);
    bark(s, s.team == Team::MONTERAN ? "Charge set! Get clear!" : "Vzryv!", true);
    s.bark_cooldown = 0.0f;
}

void Battle::cut_wire_at(Soldier& s, sf::Vector2f at)
{
    (void)s;
    auto const cleared = m_map.cut_wire(at, 14.0f);
    if(cleared.empty()) return;
    // Draw the gap into the ground: trodden mud, the cut ends curling back.
    for(const auto& r : cleared)
    {
        draw::rect(m_stamps, r.position.x, r.position.y, r.size.x, r.size.y, MUD);
        for(int k = 0; k < 10; ++k)
            draw::rect(m_stamps, std::floor(r.position.x + Random::real(0, r.size.x)), std::floor(r.position.y + Random::real(0, r.size.y)), 1, 1,
                       Random::chance(0.5f) ? MUD_DARK : MUD_LIGHT);
        for(float y : {r.position.y, r.position.y + r.size.y - 2.0f})
            for(int k = 0; k < 4; ++k)
                draw::rect(m_stamps, std::floor(r.position.x + Random::real(2, r.size.x - 2)), std::floor(y + Random::real(0, 2)), 1, 1, WIRE);
    }
    SoundBank::get().play_at("wire_cut", at, 0.8f);
}

// ---------------------------------------------------------------- the radio

void Battle::call_fire_mission(Soldier& s, sf::Vector2f at)
{
    // A mission already running, waiting for its correction: this is it.
    for(auto& m : m_missions)
        if(m.caller == s.id && m.step == 2)
        {
            m.target = at;
            m.step = 3;
            m.timer = 0.0f;
            int const rounds = m.smoke ? 4 : 6;
            for(int k = 0; k < rounds; ++k)
            {
                Shell sh;
                sh.target = at + dir(Random::real(0.0f, 6.283f)) * Random::real(0.0f, m.smoke ? 40.0f : 34.0f);
                sh.time = 4.0f + static_cast<float>(k) * 0.45f + Random::real(0.0f, 0.4f);
                sh.kind = 3;
                sh.smoke = m.smoke;
                m_shells.push_back(sh);
            }
            SoundBank::get().play_at("radio_call", s.pos, 0.7f);
            bark(s, m.smoke ? "Smoke, fire for effect!" : "On target! Fire for effect!", true);
            s.bark_cooldown = 0.0f;
            log(short_name(s) + " called " + (m.smoke ? "smoke" : "a salvo") + " down");
            return;
        }
    for(const auto& m : m_missions) if(m.caller == s.id) return;      // already on the set
    if(s.missions <= 0)
    {
        if(s.is_player) notice("The battery has nothing more for you", 2.5f);
        return;
    }
    if(length(s.vel) > 8.0f)
    {
        if(s.is_player) notice("Stop moving to use the set", 1.5f);
        return;
    }
    --s.missions;
    Mission m;
    m.caller = s.id;
    m.team = s.team;
    m.target = at;
    m.step = 0;
    m.timer = 3.0f;
    m.smoke = s.is_player ? m_smoke_mission : false;
    m_missions.push_back(m);
    if(s.stance == Stance::STAND) s.stance = Stance::CROUCH;
    SoundBank::get().play_at("radio_call", s.pos, s.is_player ? 0.8f : 0.5f);
    static const char* const words[] = {"Fire mission! Grid on my mark!", "Battery, fire mission! Ranging!", "Guns, guns, this is Varga's lot!"};
    bark(s, words[Random::integer(0, 2)], false);
    s.bark_cooldown = 0.0f;
    if(s.is_player) hint("Calling. Keep still. Watch where the ranging round falls.", 4.0f);
}

void Battle::update_missions(float dt)
{
    for(auto& m : m_missions)
    {
        Soldier& s = m_soldiers[static_cast<std::size_t>(m.caller)];
        bool const on_set = s.alive() && length(s.vel) < 12.0f;
        m.timer -= dt;
        switch(m.step)
        {
            case 0:
                if(!on_set)
                {
                    // He moved, or he's down: the call is lost.
                    if(s.is_player && s.alive()) notice("You moved off the set. The call is lost.", 2.5f);
                    m.step = 9;
                    break;
                }
                if(m.timer <= 0.0f)
                {
                    // One round first, to see where the guns are laid.
                    Shell sh;
                    sh.target = m.target + dir(Random::real(0.0f, 6.283f)) * Random::real(45.0f, 85.0f);
                    sh.time = 5.0f;
                    sh.kind = 3;
                    m_shells.push_back(sh);
                    m.step = 1;
                    m.timer = 5.2f;
                    SoundBank::get().play_at("radio_squelch", s.pos, 0.6f);
                    bark(s, "Shot, over!", false);
                    s.bark_cooldown = 0.0f;
                }
                break;
            case 1:
                if(m.timer <= 0.0f)
                {
                    m.step = 2;
                    m.timer = 14.0f;
                    if(s.is_player) hint("Ranging round down. V on the target to fire for effect.", 6.0f);
                    else if(s.alive())
                    {
                        // The AI corrects onto what he wanted all along.
                        call_fire_mission(s, m.target);
                    }
                }
                break;
            case 2:
                if(m.timer <= 0.0f)
                {
                    if(s.is_player) notice("The battery has moved on to other targets", 2.5f);
                    m.step = 9;
                }
                break;
            case 3:
                m.step = 9;
                break;
            default:
                break;
        }
    }
    std::erase_if(m_missions, [](const Mission& m) { return m.step >= 9; });
}

}
