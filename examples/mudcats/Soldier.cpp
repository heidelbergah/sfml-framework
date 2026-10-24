/**
 * Soldier.cpp  -  Mud Cats
 *
 * The weapons, and how a man is drawn: from above, as he is in the game,
 * and from the side, for the briefing.
 */

#include "Soldier.hpp"
#include "Draw.hpp"
#include "Palette.hpp"

#include <cmath>

namespace mudcats
{

namespace
{
    //                       name                  mag  spare  intv    reload dmg     speed   range   spread  kick    auto   flame  sound     far          reload          tracer suppress
    const WeaponSpec RIFLE {"Bolt-action rifle",    5,  40,  1.10f,  2.4f, 78.0f,  760.0f, 470.0f, 0.020f, 0.050f, false, false, "rifle",  "rifle_far", "rifle_reload", 0, 0.30f};
    const WeaponSpec SMG   {"Submachine gun",      30,  90,  0.085f, 2.2f, 32.0f,  520.0f, 240.0f, 0.050f, 0.016f, true,  false, "smg",    "smg_far",   "smg_reload",   0, 0.13f};
    const WeaponSpec PISTOL{"Service pistol",       8,  24,  0.32f,  1.6f, 30.0f,  460.0f, 180.0f, 0.045f, 0.060f, false, false, "pistol", "smg_far",   "pistol_reload",0, 0.10f};
    const WeaponSpec MG    {"Machine gun",        100, 200,  0.08f,  4.2f, 40.0f,  740.0f, 620.0f, 0.014f, 0.0025f, true, false, "mg",     "mg_far",    "mg_reload",    3, 0.30f};
    const WeaponSpec FLAMER{"Flamethrower",       100,   0,  0.03f,  0.0f,  0.0f,  150.0f,  78.0f, 0.110f, 0.000f, true,  true,  "",       "",          "",             0, 0.35f};
    const WeaponSpec SNIPER{"Scoped rifle",         5,  30,  1.60f,  2.8f, 115.0f, 900.0f, 900.0f, 0.004f, 0.060f, false, false, "sniper", "rifle_far", "rifle_reload", 0, 0.40f};
    const WeaponSpec BARE  {"Nothing",              0,   0,  1.00f,  0.0f,  0.0f,    1.0f,   0.0f, 0.000f, 0.000f, false, false, "",       "",          "",             0, 0.00f};

    sf::Vector2f dir(float a) { return {std::cos(a), std::sin(a)}; }
}

const WeaponSpec& weapon_spec(Gun gun)
{
    switch(gun)
    {
        case Gun::RIFLE:  return RIFLE;
        case Gun::SMG:    return SMG;
        case Gun::PISTOL: return PISTOL;
        case Gun::MG:     return MG;
        case Gun::FLAMER: return FLAMER;
        case Gun::SNIPER: return SNIPER;
        default:          return BARE;
    }
}

Gun default_gun(Role role)
{
    switch(role)
    {
        case Role::OFFICER:      return Gun::SMG;
        case Role::MEDIC:        return Gun::PISTOL;
        case Role::GUNNER:       return Gun::MG;
        case Role::FLAMER:       return Gun::FLAMER;
        case Role::STORMTROOPER: return Gun::SMG;
        case Role::SNIPER:       return Gun::SNIPER;
        default:                 return Gun::RIFLE;
    }
}

const char* gun_name(Gun gun)
{
    return weapon_spec(gun).name;
}

const char* role_name(Role role)
{
    switch(role)
    {
        case Role::OFFICER:      return "Officer";
        case Role::MEDIC:        return "Medic";
        case Role::GUNNER:       return "Machine gunner";
        case Role::FLAMER:       return "Flamethrower";
        case Role::RADIOMAN:     return "Radioman";
        case Role::STORMTROOPER: return "Stormtrooper";
        case Role::SNIPER:       return "Sniper";
        case Role::SAPPER:       return "Sapper";
        default:                 return "Rifleman";
    }
}

sf::Color uniform_colour(Team team)
{
    return team == Team::MONTERAN ? MON_BLUE : RCH_TAN;
}

void draw_shadow(sf::VertexArray& va, const Soldier& s)
{
    if(s.life == Life::DEAD) return;
    // A standing man throws a longer shadow than a kneeling one: from
    // straight above, that is most of what tells them apart.
    if(s.life == Life::DOWNED || s.stance == Stance::PRONE)
        draw::disc(va, s.pos.x + 1.0f, s.pos.y + 1.5f, 4.0f, SHADOW);
    else if(s.stance == Stance::CROUCH)
        draw::disc(va, s.pos.x + 1.0f, s.pos.y + 1.5f, 3.0f, SHADOW);
    else
    {
        draw::disc(va, s.pos.x + 2.0f, s.pos.y + 3.0f, 3.5f, SHADOW);
        draw::disc(va, s.pos.x + 3.5f, s.pos.y + 5.0f, 2.5f, with_alpha(SHADOW, 0.25f));
    }
}

void draw_soldier(sf::VertexArray& va, const Soldier& s, float time)
{
    bool const mon = s.team == Team::MONTERAN;
    auto tone = [&s](sf::Color c)
    {
        if(s.charred) return lerp(c, CHAR, 0.85f);
        if(s.life == Life::DEAD) return lerp(c, MUD_DARK, 0.25f);
        return c;
    };

    bool const storm = s.role == Role::STORMTROOPER;
    bool const sniper = s.role == Role::SNIPER;
    // Stormtroopers on both sides wear something darker than the line.
    sf::Color const coat    = tone(mon ? (storm ? lerp(MON_BLUE_DK, INK, 0.15f) : MON_BLUE) : (storm ? lerp(RCH_TAN_DK, INK, 0.2f) : RCH_TAN));
    sf::Color const coat_dk = tone(mon ? (storm ? lerp(MON_BLUE_DK, INK, 0.4f) : MON_BLUE_DK) : (storm ? lerp(RCH_TAN_DK, INK, 0.45f) : RCH_TAN_DK));
    sf::Color const strap   = tone(mon ? (storm ? LEATHER : MON_WHITE) : LEATHER);
    sf::Color const helmet  = tone(sniper ? lerp(mon ? MON_HELMET : RCH_HELMET, GRASS, 0.55f) : (mon ? MON_HELMET : RCH_HELMET));
    sf::Color const skin    = tone(SKIN);
    sf::Color const pack    = tone(mon ? lerp(LEATHER, MON_BLUE_DK, 0.3f) : LEATHER);
    sf::Color const boot    = tone(LEATHER);
    sf::Color const metal   = tone(GUNMETAL);
    sf::Color const wood    = tone(WOOD);
    sf::Color const stump   = s.charred ? CHAR : BLOOD_BRIGHT;

    bool const lying = s.life != Life::OK || s.stance == Stance::PRONE;
    float const a = lying ? s.body_angle : s.aim;
    sf::Vector2f const u = dir(a), v{-u.y, u.x};
    sf::Vector2f const c = s.pos + (s.lunge > 0.0f ? u * 2.0f : sf::Vector2f{});

    auto head = [&](sf::Vector2f at)
    {
        if(s.lost & HEAD)
        {
            draw::disc(va, at.x, at.y, 1.5f, stump);
            return;
        }
        if(s.role == Role::OFFICER)
        {
            draw::disc(va, at.x, at.y, 2.3f, tone(mon ? MON_BLUE_DK : RCH_TAN_DK));       // peaked cap
            draw::rotated_rect(va, at + u * 1.6f, 1.5f, 4.0f, a, tone(mon ? MON_GOLD : RCH_RED));
        }
        else if(s.role == Role::MEDIC && mon)
        {
            draw::disc(va, at.x, at.y, 2.3f, tone(MON_WHITE));
            draw::rect(va, std::floor(at.x), std::floor(at.y) - 1, 1, 3, tone(RCH_RED));
            draw::rect(va, std::floor(at.x) - 1, std::floor(at.y), 3, 1, tone(RCH_RED));
        }
        else if(storm)
        {
            // A deep steel helmet with a flared skirt: wider than anyone else's.
            draw::disc(va, at.x, at.y, 2.9f, tone(lerp(helmet, INK, 0.35f)));
            draw::disc(va, at.x + u.x * 0.4f, at.y + u.y * 0.4f, 2.2f, helmet);
        }
        else
        {
            draw::disc(va, at.x, at.y, 2.4f, helmet);
            if(sniper)
                for(int k = 0; k < 3; ++k)          // a scrim of grass and rag
                    draw::rect(va, std::floor(at.x + std::cos(static_cast<float>(k) * 2.1f) * 2.2f), std::floor(at.y + std::sin(static_cast<float>(k) * 2.1f) * 2.2f), 1, 1, tone(GRASS_DEAD));
            else
                draw::rect(va, std::floor(at.x - u.y), std::floor(at.y) - 1, 1, 1, tone(lerp(helmet, MON_WHITE, 0.4f)));
        }
    };

    auto weapon = [&](sf::Vector2f from, float angle)
    {
        sf::Vector2f const w = dir(angle), side{-w.y, w.x};
        switch(s.gun)
        {
            case Gun::MG:
                draw::thick_line(va, from - w * 2.0f, from + w * 11.0f, 2.0f, metal);
                draw::thick_line(va, from + w * 1.0f, from + w * 4.0f, 2.6f, metal);
                if(s.deployed)
                {
                    sf::Vector2f const bip = from + w * 8.0f;
                    draw::thick_line(va, bip, bip + side * 2.5f + w, 1.0f, metal);
                    draw::thick_line(va, bip, bip - side * 2.5f + w, 1.0f, metal);
                }
                break;
            case Gun::FLAMER:
                draw::thick_line(va, from, from + w * 8.0f, 1.6f, metal);
                draw::rect(va, std::floor(from.x + w.x * 8.5f), std::floor(from.y + w.y * 8.5f), 1, 1,
                           std::fmod(time * 7.0f, 1.0f) < 0.5f ? FIRE_ORANGE : FIRE_YELLOW);   // pilot light
                break;
            case Gun::SMG:
                draw::thick_line(va, from, from + w * 6.0f, 1.4f, metal);
                draw::thick_line(va, from + w * 2.0f + side * 1.2f, from + w * 3.0f + side * 2.5f, 1.0f, metal);
                break;
            case Gun::PISTOL:
                draw::thick_line(va, from + w * 1.0f, from + w * 4.0f, 1.3f, metal);
                break;
            case Gun::SNIPER:
                draw::thick_line(va, from - w * 1.0f, from + w * 4.0f, 1.5f, wood);
                draw::thick_line(va, from + w * 4.0f, from + w * 11.0f, 1.1f, metal);
                draw::thick_line(va, from + w * 2.0f - side * 0.8f, from + w * 5.0f - side * 0.8f, 1.2f, tone(INK));   // the scope
                break;
            case Gun::RIFLE:
                draw::thick_line(va, from - w * 1.0f, from + w * 4.0f, 1.5f, wood);
                draw::thick_line(va, from + w * 4.0f, from + w * 10.0f, 1.1f, metal);
                draw::rect(va, std::floor(from.x + w.x * 11.5f), std::floor(from.y + w.y * 11.5f), 1, 1, tone(lerp(GUNMETAL, MON_WHITE, 0.5f)));  // bayonet
                break;
            default:
                break;
        }
    };

    // What he carries on his back, under everything else.
    auto back = [&](sf::Vector2f body, float width)
    {
        if(s.gun == Gun::FLAMER)
        {
            for(int side : {-1, 1})
            {
                sf::Vector2f const tank = body - u * 3.0f + v * (1.6f * static_cast<float>(side));
                draw::disc(va, tank.x, tank.y, 1.7f, tone(CONCRETE_DK));
                draw::rect(va, std::floor(tank.x), std::floor(tank.y) - 1, 1, 1, tone(CONCRETE));
            }
            return;
        }
        if(s.role == Role::RADIOMAN)
        {
            // The set: a big box, and the aerial whipping back behind him.
            draw::rotated_rect(va, body - u * 3.0f, 4.0f, width - 1.0f, a, tone(lerp(GUNMETAL, GRASS_DEAD, 0.4f)));
            draw::rect(va, std::floor(body.x - u.x * 3.0f + v.x), std::floor(body.y - u.y * 3.0f + v.y), 1, 1, tone(MON_GOLD));
            sf::Vector2f const base = body - u * 4.0f - v * 2.0f;
            float const sway = std::sin(time * 3.0f + static_cast<float>(s.id)) * 0.15f;
            draw::thick_line(va, base, base + dir(a + 3.14159f - 0.35f + sway) * 9.0f, 0.8f, tone(lerp(GUNMETAL, PAPER, 0.3f)));
            return;
        }
        if(storm)
        {
            // Light: a small bag of grenades instead of a pack.
            draw::rotated_rect(va, body - u * 2.0f, 2.0f, width - 3.0f, a, pack);
            return;
        }
        draw::rotated_rect(va, body - u * 2.2f, 3.0f, width - 2.0f, a, pack);
        if(s.role == Role::SAPPER)                  // a shovel across the pack
            draw::thick_line(va, body - u * 2.8f - v * 3.0f, body - u * 1.6f + v * 3.0f, 0.9f, wood);
    };

    if(lying)
    {
        // Legs behind, torso, arms and head in front.
        bool const down = s.life != Life::OK;
        float const splay = down ? 0.45f : 0.12f;
        for(int side : {-1, 1})
        {
            Limb const leg = side < 0 ? LEG_L : LEG_R;
            sf::Vector2f const hip = c - u * 3.0f + v * (1.3f * static_cast<float>(side));
            if(s.lost & leg)
            {
                draw::disc(va, hip.x - u.x, hip.y - u.y, 1.0f, stump);
                continue;
            }
            float const la = a + 3.14159f + splay * static_cast<float>(side);
            float const kick = (!down && s.vel.x * s.vel.x + s.vel.y * s.vel.y > 1.0f) ? std::sin(s.walk_phase * 2.0f + static_cast<float>(side)) * 0.25f : 0.0f;
            draw::thick_line(va, hip, hip + dir(la + kick) * 5.0f, 2.0f, coat_dk);
            draw::rect(va, std::floor(hip.x + dir(la + kick).x * 5.0f), std::floor(hip.y + dir(la + kick).y * 5.0f), 2, 2, boot);
        }
        if(s.gun == Gun::FLAMER)
            for(int side : {-1, 1})
                draw::disc(va, c.x - u.x + v.x * 1.6f * static_cast<float>(side), c.y - u.y + v.y * 1.6f * static_cast<float>(side), 1.6f, tone(CONCRETE_DK));
        draw::rotated_rect(va, c, 7.0f, 5.0f, a, coat);
        draw::rotated_rect(va, c - u * 0.5f, 1.0f, 5.0f, a, strap);
        if(!down)
        {
            if(s.role == Role::RADIOMAN) draw::rotated_rect(va, c - u * 1.0f, 4.0f, 4.0f, a, tone(lerp(GUNMETAL, GRASS_DEAD, 0.4f)));
            else if(s.gun != Gun::FLAMER) draw::rotated_rect(va, c - u * 1.0f, 3.0f, 3.0f, a, pack);
        }
        if(s.yours && !down) draw::rect(va, std::floor(c.x + v.x * 2.5f), std::floor(c.y + v.y * 2.5f), 1, 1, MON_GOLD);

        for(int side : {-1, 1})
        {
            Limb const arm = side < 0 ? ARM_L : ARM_R;
            sf::Vector2f const shoulder = c + u * 2.5f + v * (2.5f * static_cast<float>(side));
            if(s.lost & arm)
            {
                draw::disc(va, shoulder.x, shoulder.y, 1.0f, stump);
                continue;
            }
            float const aa = down ? a + 1.2f * static_cast<float>(side) + std::sin(time * 0.7f + static_cast<float>(s.id)) * (s.life == Life::DOWNED ? 0.2f : 0.0f)
                                  : a + 0.25f * static_cast<float>(side);
            draw::thick_line(va, shoulder, shoulder + dir(aa) * 4.0f, 1.5f, coat);
            draw::rect(va, std::floor(shoulder.x + dir(aa).x * 4.0f), std::floor(shoulder.y + dir(aa).y * 4.0f), 1, 1, skin);
        }
        // Lying down, a man keeps his weapon pointed where he looks: there
        // is nowhere else to put it.
        if(!down) weapon(c + u * 3.0f, s.aim);
        head(c + u * 5.0f);
        return;
    }

    bool const crouch = s.stance == Stance::CROUCH;
    bool const moving = s.vel.x * s.vel.x + s.vel.y * s.vel.y > 4.0f;

    // --- legs -------------------------------------------------------------------
    if(crouch && !moving)
    {
        // Kneeling: one knee up in front, the other leg folded under and
        // behind with the boot sticking out. A clearly different shape.
        if(!(s.lost & LEG_R))
        {
            sf::Vector2f const hip = c + v * 1.6f;
            draw::thick_line(va, hip, hip + u * 3.5f, 2.2f, coat_dk);
            draw::rect(va, std::floor(hip.x + u.x * 4.0f), std::floor(hip.y + u.y * 4.0f), 2, 2, boot);
        }
        if(!(s.lost & LEG_L))
        {
            sf::Vector2f const hip = c - v * 1.6f;
            draw::thick_line(va, hip, hip - u * 4.5f, 2.2f, coat_dk);
            draw::rect(va, std::floor(hip.x - u.x * 5.5f), std::floor(hip.y - u.y * 5.5f), 2, 2, boot);
        }
    }
    else
    {
        float const stride = moving ? std::sin(s.walk_phase) * (crouch ? 1.2f : 2.2f) : 0.0f;
        sf::Vector2f const travel = moving ? s.vel / std::sqrt(s.vel.x * s.vel.x + s.vel.y * s.vel.y) : u;
        for(int side : {-1, 1})
        {
            Limb const leg = side < 0 ? LEG_L : LEG_R;
            if(s.lost & leg) continue;
            sf::Vector2f const foot = c + v * (1.5f * static_cast<float>(side)) + travel * (stride * static_cast<float>(side)) - u * (crouch ? 1.5f : 0.0f);
            draw::rotated_rect(va, foot, crouch ? 2.0f : 3.0f, 2.0f, a, boot);
        }
    }

    // --- body -----------------------------------------------------------------------
    float const width = crouch ? 6.0f : 7.0f;
    sf::Vector2f const body = c - u * (crouch ? 0.8f : 0.0f);
    back(body, width);

    draw::rotated_rect(va, body, crouch ? 3.5f : 4.0f, width, a, coat);
    draw::rotated_rect(va, body + u * 0.2f, 4.0f, 1.0f, a + 0.6f, strap);          // cross strap
    draw::rotated_rect(va, body - u * 1.5f, 1.0f, width, a, coat_dk);
    if(storm)                                                                       // stick grenades in the belt
        for(int side : {-1, 1})
            draw::rect(va, std::floor(body.x + u.x * 1.2f + v.x * 2.0f * static_cast<float>(side)), std::floor(body.y + u.y * 1.2f + v.y * 2.0f * static_cast<float>(side)), 1, 1, wood);
    if(s.role == Role::SAPPER)                                                      // the satchel on his hip
        draw::rotated_rect(va, body + v * 3.2f - u * 0.5f, 2.5f, 2.0f, a, tone(LEATHER));
    if(s.leader)
        draw::rect(va, std::floor(body.x - v.x * 3.0f), std::floor(body.y - v.y * 3.0f), 1, 1, mon ? MON_WHITE : RCH_RED);   // the corporal's stripe
    if(s.yours)
        draw::rect(va, std::floor(body.x + v.x * 3.2f - u.x * 0.5f), std::floor(body.y + v.y * 3.2f - u.y * 0.5f), 1, 2, MON_GOLD);   // your section's armband

    // --- arms --------------------------------------------------------------------------
    if(s.surrendered)
    {
        // Hands up, nothing in them.
        for(int side : {-1, 1})
        {
            Limb const arm = side < 0 ? ARM_L : ARM_R;
            sf::Vector2f const shoulder = body + v * (3.0f * static_cast<float>(side));
            if(s.lost & arm) continue;
            sf::Vector2f const hand = body + u * 1.5f + v * (3.5f * static_cast<float>(side));
            draw::thick_line(va, shoulder, hand, 1.5f, coat);
            draw::rect(va, std::floor(hand.x), std::floor(hand.y), 1, 1, skin);
        }
        head(body + u * 0.6f);
        return;
    }

    // Raised, the weapon points where he looks, held out in front. Lowered,
    // it is carried across the body at the low ready, pointing ahead and
    // off to his right, close in. `ready` moves between the two.
    float const r = std::clamp(s.ready, 0.0f, 1.0f);
    float const low = 1.0f - r;
    float const carry = s.has_mg() ? 0.55f : (s.gun == Gun::PISTOL ? 1.1f : 0.85f);
    float const angle = s.aim + low * carry;
    float const raise = crouch ? 0.5f : 0.0f;               // kneeling men hold the rifle tighter in
    sf::Vector2f const grip = body + u * (1.5f + 2.0f * r + raise + (s.lunge > 0.0f ? 2.0f : 0.0f)) + v * (low * 1.3f);
    sf::Vector2f const w = dir(angle);
    for(int side : {-1, 1})
    {
        Limb const arm = side < 0 ? ARM_L : ARM_R;
        sf::Vector2f const shoulder = body + v * (3.0f * static_cast<float>(side));
        if(s.lost & arm)
        {
            draw::disc(va, shoulder.x, shoulder.y, 1.0f, stump);
            continue;
        }
        sf::Vector2f const hand = side > 0 ? grip + v * 0.5f : grip + w * 2.5f - v * 0.5f;
        draw::thick_line(va, shoulder, hand, 1.6f, coat);
        draw::rect(va, std::floor(hand.x), std::floor(hand.y), 1, 1, skin);
        if(s.role == Role::MEDIC && mon && side < 0)
            draw::rect(va, std::floor(shoulder.x), std::floor(shoulder.y), 1, 1, MON_WHITE);
    }
    if(s.can_shoot() || s.gun == Gun::FLAMER) weapon(grip - w * 1.0f + v * 0.3f, angle);
    if(s.role == Role::OFFICER && mon)
        for(int side : {-1, 1})
            draw::rect(va, std::floor(body.x + v.x * 3.0f * static_cast<float>(side)), std::floor(body.y + v.y * 3.0f * static_cast<float>(side)), 1, 1, MON_GOLD);
    head(body + u * 0.6f);
}

// ---------------------------------------------------------------- from the side

void draw_profile(sf::VertexArray& va, Role role, Team team, float breath, float raise)
{
    bool const mon = team == Team::MONTERAN;
    bool const storm = role == Role::STORMTROOPER;
    sf::Color const coat    = mon ? (storm ? lerp(MON_BLUE_DK, INK, 0.15f) : MON_BLUE) : RCH_TAN;
    sf::Color const coat_dk = mon ? (storm ? lerp(MON_BLUE_DK, INK, 0.4f) : MON_BLUE_DK) : RCH_TAN_DK;
    sf::Color const coat_lt = lerp(coat, PAPER, 0.18f);
    sf::Color const helmet  = role == Role::SNIPER ? lerp(MON_HELMET, GRASS, 0.5f) : (mon ? MON_HELMET : RCH_HELMET);
    sf::Color const strap   = mon && !storm ? MON_WHITE : LEATHER;

    // Pixel helpers: everything sits on whole pixels, up is negative.
    auto px = [&va](float x, float y, float w, float h, sf::Color c) { draw::rect(va, std::floor(x), std::floor(y), w, h, c); };
    auto line = [&va](sf::Vector2f a, sf::Vector2f b, sf::Color c)
    {
        // A one-pixel line, stepped: no anti-aliasing, so it scales up crisp.
        float const len = std::max(std::abs(b.x - a.x), std::abs(b.y - a.y));
        for(float k = 0; k <= len; k += 1.0f)
        {
            sf::Vector2f const p = a + (b - a) * (len > 0 ? k / len : 0.0f);
            draw::rect(va, std::floor(p.x), std::floor(p.y), 1, 1, c);
        }
    };

    float const chest = std::round(std::sin(breath * 1.9f) * 0.5f + 0.5f);    // 0 or 1: the rise and fall
    float const up = -chest;

    // --- boots and legs --------------------------------------------------------
    px(-3, -2, 4, 2, LEATHER);  px(1, -2, 1, 1, LEATHER);            // back boot
    px(1, -2, 5, 2, LEATHER);                                         // front boot, toe forward
    px(-3, -8, 3, 6, mon ? lerp(coat_dk, PAPER, 0.25f) : lerp(LEATHER, RCH_TAN_DK, 0.5f));    // puttees, wound up the shin
    px(1, -8, 3, 6, mon ? lerp(coat_dk, PAPER, 0.32f) : lerp(LEATHER, RCH_TAN_DK, 0.4f));
    px(-3, -13, 3, 5, coat_dk);
    px(1, -13, 3, 5, coat_dk);

    // --- what's on his back -------------------------------------------------------
    if(role == Role::FLAMER)
    {
        px(-8, -25 + up, 4, 13, CONCRETE_DK);  px(-7, -24 + up, 1, 11, CONCRETE);
        line({-6, -12 + up}, {2, -14}, LEATHER);                       // the hose
    }
    else if(role == Role::RADIOMAN)
    {
        px(-9, -26 + up, 6, 12, lerp(GUNMETAL, GRASS_DEAD, 0.4f));
        px(-8, -24 + up, 2, 1, MON_GOLD); px(-8, -21 + up, 4, 1, lerp(GUNMETAL, INK, 0.3f));
        line({-8, -26 + up}, {-12, -44}, lerp(GUNMETAL, PAPER, 0.3f));   // the aerial
    }
    else if(storm)
    {
        px(-5, -20 + up, 2, 5, LEATHER);                               // a sandbag of grenades
    }
    else
    {
        px(-7, -23 + up, 4, 8, mon ? lerp(LEATHER, MON_BLUE_DK, 0.3f) : LEATHER);
        px(-7, -15 + up, 4, 2, lerp(LEATHER, INK, 0.3f));              // rolled blanket
        if(role == Role::SAPPER) { line({-6, -27 + up}, {-6, -13 + up}, WOOD); px(-7, -28 + up, 3, 2, GUNMETAL); }
    }

    // --- the coat --------------------------------------------------------------------
    px(-4, -15 + up, 8, 3, coat);                                   // skirts of the coat
    px(-3, -24 + up, 6, 10, coat);
    px(2, -24 + up, 1, 9, coat_lt);                                 // light on the chest
    px(-3, -16 + up, 6, 1, LEATHER);                                // belt
    px(1, -16 + up, 1, 1, lerp(GUNMETAL, PAPER, 0.5f));             // buckle
    line({-3, -24 + up}, {2, -16 + up}, strap);                     // cross strap
    if(role == Role::MEDIC && mon) { px(-3, -21 + up, 2, 2, MON_WHITE); px(-3, -20 + up, 2, 1, RCH_RED); }   // armband
    if(role == Role::OFFICER) { px(-3, -24 + up, 6, 1, mon ? MON_GOLD : RCH_RED); }                          // epaulettes
    if(storm) { px(-1, -17 + up, 1, 3, WOOD); px(1, -17 + up, 1, 3, WOOD); px(-1, -18 + up, 1, 1, GUNMETAL); px(1, -18 + up, 1, 1, GUNMETAL); }
    if(role == Role::SAPPER) px(2, -15 + up, 3, 4, LEATHER);       // the satchel
    if(role == Role::RADIOMAN) line({1, -22 + up}, {3, -27 + up}, INK);   // handset cord

    // --- head ---------------------------------------------------------------------------
    px(0, -26 + up, 2, 2, SKIN_DK);                                 // neck
    px(0, -30 + up, 4, 4, SKIN);
    px(3, -28 + up, 1, 1, SKIN_DK);                                 // nose
    px(2, -29 + up, 1, 1, INK);                                     // eye
    if(role == Role::OFFICER)
    {
        px(-1, -32 + up, 5, 2, mon ? MON_BLUE_DK : RCH_TAN_DK);
        px(3, -30 + up, 3, 1, INK);                                 // the peak
        px(-1, -31 + up, 5, 1, mon ? MON_GOLD : RCH_RED);
    }
    else if(role == Role::MEDIC && mon)
    {
        px(-1, -33 + up, 5, 3, MON_WHITE);  px(-2, -30 + up, 7, 1, MON_WHITE);
        px(1, -32 + up, 1, 1, RCH_RED);
    }
    else if(storm)
    {
        px(-1, -33 + up, 5, 3, helmet);                             // deep dome
        px(-2, -30 + up, 3, 3, lerp(helmet, INK, 0.25f));          // the flared skirt over the ear and neck
        px(3, -30 + up, 2, 1, lerp(helmet, INK, 0.25f));           // short brim over the eyes
    }
    else
    {
        px(-1, -33 + up, 5, 3, helmet);
        px(1, -34 + up, 1, 1, lerp(helmet, PAPER, 0.3f));           // the crest
        px(-2, -30 + up, 7, 1, lerp(helmet, INK, 0.2f));            // brim
        if(role == Role::SNIPER)
            for(int k = 0; k < 5; ++k) px(-2 + k, -35 + up + (k % 2), 1, 2, k % 2 ? GRASS : GRASS_DEAD);
    }

    // --- arms and the weapon --------------------------------------------------------------
    // At ease, the weapon is held low across the body, muzzle forward and
    // down. Raised, it comes up level with the eye, stock in the shoulder.
    float const r = std::clamp(raise, 0.0f, 1.0f);
    auto mixv = [r](sf::Vector2f a, sf::Vector2f b) { return a + (b - a) * r; };
    sf::Vector2f const shoulder{1, -23 + up};
    sf::Vector2f stock = mixv({-2, -14 + up}, {-1, -24 + up});
    sf::Vector2f muzzle = mixv({11, -10 + up}, {16, -25 + up});
    sf::Color const metal = GUNMETAL;
    Gun const gun = default_gun(role);
    if(gun == Gun::MG) { stock = mixv({-4, -16 + up}, {-1, -22 + up}); muzzle = mixv({14, -13 + up}, {17, -22 + up}); }
    if(gun == Gun::SMG || gun == Gun::PISTOL) { muzzle = stock + (muzzle - stock) * (gun == Gun::PISTOL ? 0.35f : 0.65f); }

    sf::Vector2f const d = (muzzle - stock) / std::max(1.0f, std::hypot(muzzle.x - stock.x, muzzle.y - stock.y));
    sf::Vector2f const fore = stock + d * (gun == Gun::PISTOL ? 3.0f : 7.0f);
    sf::Vector2f const grip = stock + d * 2.5f;

    // The far arm first, behind the weapon.
    line(shoulder + sf::Vector2f{-1, 0}, fore, coat_dk);
    px(fore.x, fore.y, 1, 1, SKIN_DK);
    switch(gun)
    {
        case Gun::RIFLE:
        case Gun::SNIPER:
            line(stock, stock + d * 6.0f, WOOD);
            line(stock + sf::Vector2f{0, 1}, stock + d * 4.0f + sf::Vector2f{0, 1}, WOOD);
            line(stock + d * 6.0f, muzzle, metal);
            if(gun == Gun::SNIPER) line(stock + d * 4.0f + sf::Vector2f{0, -1}, stock + d * 8.0f + sf::Vector2f{0, -1}, INK);
            else line(muzzle, muzzle + d * 3.0f, lerp(GUNMETAL, PAPER, 0.5f));                 // bayonet
            break;
        case Gun::SMG:
            line(stock, muzzle, metal);
            px(grip.x + d.x * 3.0f, grip.y + d.y * 3.0f + 1, 1, 3, metal);                      // magazine
            break;
        case Gun::PISTOL:
            line(stock, muzzle, metal);
            break;
        case Gun::MG:
            line(stock, muzzle, metal);
            line(stock + sf::Vector2f{0, 1}, stock + d * 8.0f + sf::Vector2f{0, 1}, metal);
            px(stock.x + d.x * 7.0f, stock.y + d.y * 7.0f - 2, 2, 2, lerp(GUNMETAL, INK, 0.3f)); // feed cover
            break;
        case Gun::FLAMER:
            line(stock, muzzle, metal);
            px(muzzle.x, muzzle.y, 1, 1, FIRE_ORANGE);
            break;
        default: break;
    }
    // The near arm, over it: from the shoulder to the grip.
    line(shoulder, grip, coat);
    line(shoulder + sf::Vector2f{0, 1}, grip + sf::Vector2f{0, 1}, coat);
    px(grip.x, grip.y, 2, 1, SKIN);
}

}
