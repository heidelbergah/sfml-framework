/**
 * Soldier.cpp  -  Mud Cats
 */

#include "Soldier.hpp"
#include "Draw.hpp"
#include "Palette.hpp"

#include <cmath>

namespace mudcats
{

namespace
{
    //                       name                  mag  spare  intv  reload dmg  speed range spread  kick   auto   flame  sound     far          reload          tracer suppress
    const WeaponSpec RIFLE {"Bolt-action rifle",    5,  40,  1.10f, 2.4f, 78.0f, 760.0f, 470.0f, 0.020f, 0.050f, false, false, "rifle",  "rifle_far", "rifle_reload", 0, 0.24f};
    const WeaponSpec SMG   {"Submachine gun",      30,  90,  0.095f, 2.2f, 32.0f, 520.0f, 240.0f, 0.050f, 0.016f, true,  false, "smg",    "rifle_far", "smg_reload",   0, 0.10f};
    const WeaponSpec PISTOL{"Service pistol",       8,  24,  0.32f, 1.6f, 30.0f, 460.0f, 180.0f, 0.045f, 0.060f, false, false, "pistol", "rifle_far", "smg_reload",   0, 0.08f};
    const WeaponSpec MG    {"Machine gun",        100, 200,  0.11f, 4.2f, 40.0f, 720.0f, 620.0f, 0.026f, 0.007f, true,  false, "mg",     "mg_far",    "mg_reload",    3, 0.22f};
    const WeaponSpec FLAMER{"Flamethrower",       100,   0,  0.03f, 0.0f,  0.0f, 150.0f,  78.0f, 0.110f, 0.000f, true,  true,  "",       "",          "",             0, 0.35f};

    sf::Vector2f dir(float a) { return {std::cos(a), std::sin(a)}; }
}

const WeaponSpec& weapon_of(Role role)
{
    switch(role)
    {
        case Role::OFFICER: return SMG;
        case Role::MEDIC:   return PISTOL;
        case Role::GUNNER:  return MG;
        case Role::FLAMER:  return FLAMER;
        default:            return RIFLE;
    }
}

const char* role_name(Role role)
{
    switch(role)
    {
        case Role::OFFICER: return "Officer";
        case Role::MEDIC:   return "Medic";
        case Role::GUNNER:  return "Machine gunner";
        case Role::FLAMER:  return "Flamethrower";
        default:            return "Rifleman";
    }
}

sf::Color uniform_colour(Team team)
{
    return team == Team::MONTERAN ? MON_BLUE : RCH_TAN;
}

void draw_shadow(sf::VertexArray& va, const Soldier& s)
{
    if(s.life == Life::DEAD) return;
    draw::disc(va, s.pos.x + 1.0f, s.pos.y + 2.0f, s.stance == Stance::PRONE || s.life == Life::DOWNED ? 4.0f : 3.5f, SHADOW);
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

    sf::Color const coat    = tone(mon ? MON_BLUE : RCH_TAN);
    sf::Color const coat_dk = tone(mon ? MON_BLUE_DK : RCH_TAN_DK);
    sf::Color const strap   = tone(mon ? MON_WHITE : LEATHER);
    sf::Color const helmet  = tone(mon ? MON_HELMET : RCH_HELMET);
    sf::Color const skin    = tone(SKIN);
    sf::Color const pack    = tone(mon ? lerp(LEATHER, MON_BLUE_DK, 0.3f) : LEATHER);
    sf::Color const gun     = tone(GUNMETAL);
    sf::Color const wood    = tone(WOOD);
    sf::Color const stump   = s.charred ? CHAR : BLOOD_BRIGHT;

    bool const lying = s.life != Life::OK || s.stance == Stance::PRONE;
    float const a = lying ? s.body_angle : s.aim;
    sf::Vector2f const u = dir(a), v{-u.y, u.x};
    sf::Vector2f const c = s.pos;

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
        else
        {
            draw::disc(va, at.x, at.y, 2.4f, helmet);
            draw::rect(va, std::floor(at.x - u.y), std::floor(at.y + u.x * 0.0f) - 1, 1, 1, tone(lerp(helmet, MON_WHITE, 0.4f)));
        }
    };

    auto weapon = [&](sf::Vector2f from, float angle)
    {
        sf::Vector2f const w = dir(angle);
        switch(s.role)
        {
            case Role::GUNNER:
                draw::thick_line(va, from - w * 2.0f, from + w * 11.0f, 2.0f, gun);
                draw::thick_line(va, from + w * 1.0f, from + w * 4.0f, 2.6f, gun);
                if(s.deployed)
                {
                    sf::Vector2f const bip = from + w * 8.0f, side{-w.y, w.x};
                    draw::thick_line(va, bip, bip + side * 2.5f + w, 1.0f, gun);
                    draw::thick_line(va, bip, bip - side * 2.5f + w, 1.0f, gun);
                }
                break;
            case Role::FLAMER:
                draw::thick_line(va, from, from + w * 8.0f, 1.6f, gun);
                draw::rect(va, std::floor(from.x + w.x * 8.5f), std::floor(from.y + w.y * 8.5f), 1, 1,
                           std::fmod(time * 7.0f, 1.0f) < 0.5f ? FIRE_ORANGE : FIRE_YELLOW);   // pilot light
                break;
            case Role::OFFICER:
                draw::thick_line(va, from, from + w * 6.0f, 1.4f, gun);
                draw::thick_line(va, from + w * 2.0f + sf::Vector2f{-w.y, w.x} * 1.2f, from + w * 3.0f + sf::Vector2f{-w.y, w.x} * 2.5f, 1.0f, gun);
                break;
            case Role::MEDIC:
                draw::thick_line(va, from + w * 1.0f, from + w * 4.0f, 1.3f, gun);
                break;
            default:
                draw::thick_line(va, from - w * 1.0f, from + w * 4.0f, 1.5f, wood);
                draw::thick_line(va, from + w * 4.0f, from + w * 10.0f, 1.1f, gun);
                break;
        }
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
            draw::rect(va, std::floor(hip.x + dir(la + kick).x * 5.0f), std::floor(hip.y + dir(la + kick).y * 5.0f), 2, 2, tone(LEATHER));
        }
        if(s.role == Role::FLAMER)
        {
            for(int side : {-1, 1}) draw::disc(va, c.x - u.x * 1.0f + v.x * 1.6f * static_cast<float>(side), c.y - u.y * 1.0f + v.y * 1.6f * static_cast<float>(side), 1.6f, tone(CONCRETE_DK));
        }
        draw::rotated_rect(va, c, 7.0f, 5.0f, a, coat);
        draw::rotated_rect(va, c - u * 0.5f, 1.0f, 5.0f, a, strap);
        if(!down) draw::rotated_rect(va, c - u * 1.0f, 3.0f, 3.0f, a, pack);

        for(int side : {-1, 1})
        {
            Limb const arm = side < 0 ? ARM_L : ARM_R;
            sf::Vector2f const shoulder = c + u * 2.5f + v * (2.5f * static_cast<float>(side));
            if(s.lost & arm)
            {
                draw::disc(va, shoulder.x, shoulder.y, 1.0f, stump);
                continue;
            }
            float const aa = down ? a + 1.2f * static_cast<float>(side) + std::sin(time * 0.7f + s.id) * (s.life == Life::DOWNED ? 0.2f : 0.0f)
                                  : a + 0.25f * static_cast<float>(side);
            draw::thick_line(va, shoulder, shoulder + dir(aa) * 4.0f, 1.5f, coat);
            draw::rect(va, std::floor(shoulder.x + dir(aa).x * 4.0f), std::floor(shoulder.y + dir(aa).y * 4.0f), 1, 1, skin);
        }
        if(!down) weapon(c + u * 3.0f, s.aim);
        head(c + u * 5.0f);
        return;
    }

    // --- standing or crouched ---------------------------------------------------
    bool const crouch = s.stance == Stance::CROUCH;
    bool const moving = s.vel.x * s.vel.x + s.vel.y * s.vel.y > 4.0f;
    float const stride = moving ? std::sin(s.walk_phase) * (crouch ? 1.2f : 2.2f) : 0.0f;
    sf::Vector2f const travel = moving ? s.vel / std::sqrt(s.vel.x * s.vel.x + s.vel.y * s.vel.y) : u;

    for(int side : {-1, 1})
    {
        Limb const leg = side < 0 ? LEG_L : LEG_R;
        if(s.lost & leg) continue;
        sf::Vector2f const foot = c + v * (1.5f * static_cast<float>(side)) + travel * (stride * static_cast<float>(side)) - u * (crouch ? 1.5f : 0.0f);
        draw::rotated_rect(va, foot, crouch ? 2.0f : 3.0f, 2.0f, a, tone(LEATHER));
    }

    if(s.role == Role::FLAMER)
        for(int side : {-1, 1})
        {
            sf::Vector2f const t = c - u * 3.0f + v * (1.6f * static_cast<float>(side));
            draw::disc(va, t.x, t.y, 1.7f, tone(CONCRETE_DK));
            draw::rect(va, std::floor(t.x), std::floor(t.y) - 1, 1, 1, tone(CONCRETE));
        }
    else
        draw::rotated_rect(va, c - u * 2.2f, 3.0f, 5.0f, a, pack);

    draw::rotated_rect(va, c, 4.0f, 7.0f, a, coat);
    draw::rotated_rect(va, c + u * 0.2f, 4.0f, 1.0f, a + 0.6f, strap);          // cross strap
    draw::rotated_rect(va, c - u * 1.5f, 1.0f, 7.0f, a, coat_dk);

    // Arms reach to the weapon, unless they are gone.
    sf::Vector2f const grip = c + u * 3.5f;
    for(int side : {-1, 1})
    {
        Limb const arm = side < 0 ? ARM_L : ARM_R;
        sf::Vector2f const shoulder = c + v * (3.0f * static_cast<float>(side));
        if(s.lost & arm)
        {
            draw::disc(va, shoulder.x, shoulder.y, 1.0f, stump);
            continue;
        }
        sf::Vector2f const hand = side > 0 ? grip + v * 0.5f : grip + u * 2.5f - v * 0.5f;
        draw::thick_line(va, shoulder, hand, 1.6f, coat);
        draw::rect(va, std::floor(hand.x), std::floor(hand.y), 1, 1, skin);
        if(s.role == Role::MEDIC && mon && side < 0)
            draw::rect(va, std::floor(shoulder.x), std::floor(shoulder.y), 1, 1, MON_WHITE);
    }
    if(s.can_use_rifle() || s.role == Role::MEDIC || s.role == Role::OFFICER) weapon(grip - u * 1.0f + v * 0.3f, s.aim);
    if(s.role == Role::OFFICER && mon)
        for(int side : {-1, 1})
            draw::rect(va, std::floor(c.x + v.x * 3.0f * static_cast<float>(side)), std::floor(c.y + v.y * 3.0f * static_cast<float>(side)), 1, 1, MON_GOLD);
    head(c + u * 0.6f);
}

}
