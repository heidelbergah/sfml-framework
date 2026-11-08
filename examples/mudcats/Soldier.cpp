/**
 * Soldier.cpp  -  Mud Cats
 *
 * The weapons, and how a man is drawn from above, as he is in the game.
 */

#include "Soldier.hpp"
#include "Draw.hpp"
#include "Palette.hpp"
#include "Text.hpp"

#include <cmath>

namespace mudcats
{

namespace
{
    //                       name (text key)       mag  spare  intv    reload dmg     speed   range   spread  kick    auto   flame  sound     far          reload          tracer suppress
    const WeaponSpec RIFLE {"weapons.rifle",       5,  40,  1.10f,  2.4f, 78.0f,  760.0f, 470.0f, 0.020f, 0.050f, false, false, "rifle",  "rifle_far", "rifle_reload", 0, 0.30f};
    const WeaponSpec SMG   {"weapons.smg",         30,  90,  0.085f, 2.2f, 32.0f,  520.0f, 240.0f, 0.050f, 0.016f, true,  false, "smg",    "smg_far",   "smg_reload",   0, 0.13f};
    const WeaponSpec PISTOL{"weapons.pistol",       8,  24,  0.32f,  1.6f, 30.0f,  460.0f, 180.0f, 0.045f, 0.060f, false, false, "pistol", "smg_far",   "pistol_reload",0, 0.10f};
    const WeaponSpec MG    {"weapons.mg",         100, 200,  0.08f,  4.2f, 40.0f,  740.0f, 620.0f, 0.014f, 0.0025f, true, false, "mg",     "mg_far",    "mg_reload",    3, 0.30f};
    const WeaponSpec FLAMER{"weapons.flamer",     100,   0,  0.03f,  0.0f,  0.0f,  150.0f,  78.0f, 0.110f, 0.000f, true,  true,  "",       "",          "",             0, 0.35f};
    const WeaponSpec SNIPER{"weapons.sniper",       5,  30,  1.60f,  2.8f, 115.0f, 900.0f, 900.0f, 0.004f, 0.060f, false, false, "sniper", "rifle_far", "rifle_reload", 0, 0.40f};
    const WeaponSpec BARE  {"weapons.none",         0,   0,  1.00f,  0.0f,  0.0f,    1.0f,   0.0f, 0.000f, 0.000f, false, false, "",       "",          "",             0, 0.00f};

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

std::string gun_name(Gun gun)
{
    return tr(weapon_spec(gun).key);
}

const char* role_key(Role role)
{
    switch(role)
    {
        case Role::OFFICER:      return "officer";
        case Role::MEDIC:        return "medic";
        case Role::GUNNER:       return "gunner";
        case Role::FLAMER:       return "flamer";
        case Role::RADIOMAN:     return "radioman";
        case Role::STORMTROOPER: return "stormtrooper";
        case Role::SNIPER:       return "sniper";
        case Role::SAPPER:       return "sapper";
        default:                 return "rifleman";
    }
}

std::string role_name(Role role)
{
    return tr(std::string("roles.") + role_key(role) + ".name");
}

std::string rank_name(Rank rank)
{
    switch(rank)
    {
        case Rank::CORPORAL:   return tr("ranks.corporal");
        case Rank::LIEUTENANT: return tr("ranks.lieutenant");
        case Rank::CAPTAIN:    return tr("ranks.captain");
        default:               return tr("ranks.private");
    }
}

std::string surname(const Soldier& s)
{
    std::size_t const space = s.name.rfind(' ');
    return space == std::string::npos ? s.name : s.name.substr(space + 1);
}

std::string short_name(const Soldier& s)
{
    return rank_name(s.rank) + " " + surname(s);
}

std::string full_name(const Soldier& s)
{
    return rank_name(s.rank) + " " + s.name;
}

sf::Color uniform_colour(Team team)
{
    return team == Team::MONTERAN ? MON_TUNIC : RCH_TAN;
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
    sf::Color const coat    = tone(mon ? (storm ? lerp(MON_TUNIC_DK, INK, 0.15f) : MON_TUNIC) : (storm ? lerp(RCH_TAN_DK, INK, 0.2f) : RCH_TAN));
    sf::Color const coat_dk = tone(mon ? (storm ? lerp(MON_TUNIC_DK, INK, 0.4f) : MON_TUNIC_DK) : (storm ? lerp(RCH_TAN_DK, INK, 0.45f) : RCH_TAN_DK));
    sf::Color const strap   = tone(mon ? MON_CLOTH : LEATHER);
    sf::Color const legs    = mon ? tone(MON_TROUSERS) : coat_dk;
    sf::Color const helmet  = tone(sniper ? lerp(mon ? MON_HELMET : RCH_HELMET, GRASS, 0.55f) : (mon ? MON_HELMET : RCH_HELMET));
    sf::Color const skin    = tone(SKIN);
    sf::Color const pack    = tone(mon ? lerp(LEATHER, INK, 0.5f) : LEATHER);
    sf::Color const boot    = tone(mon ? MON_BOOT : LEATHER);
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
            if(mon)
            {
                // A peaked cap: the crown, its band, a black leather peak, and the badge.
                draw::disc(va, at.x, at.y, 2.3f, tone(MON_CAP));
                draw::rotated_rect(va, at + u * 0.8f, 1.0f, 4.0f, a, tone(MON_CAP_BAND));
                draw::rotated_rect(va, at + u * 1.8f, 1.2f, 3.6f, a, tone(MON_BOOT));
                draw::rect(va, std::floor(at.x), std::floor(at.y), 1, 1, tone(MON_RANK));
            }
            else
            {
                draw::disc(va, at.x, at.y, 2.3f, tone(RCH_TAN_DK));                      // peaked cap
                draw::rotated_rect(va, at + u * 1.6f, 1.5f, 4.0f, a, tone(RCH_RED));
            }
        }
        else if(s.role == Role::MEDIC && mon)
        {
            draw::disc(va, at.x, at.y, 2.3f, tone(MON_WHITE));
            draw::rect(va, std::floor(at.x), std::floor(at.y) - 1, 1, 3, tone(RCH_RED));
            draw::rect(va, std::floor(at.x) - 1, std::floor(at.y), 3, 1, tone(RCH_RED));
        }
        else if(mon)
        {
            // The Adrian: a round crown on a brim that juts out in front and
            // behind, and a crest running front to back along the top.
            sf::Color const shell = storm ? tone(lerp(helmet, INK, 0.25f)) : helmet;
            sf::Color const rim = tone(lerp(shell, INK, 0.55f));
            draw::disc(va, at.x, at.y, 2.7f, rim);
            draw::rotated_rect(va, at + u * 0.5f, 7.4f, 2.6f, a, rim);
            draw::disc(va, at.x, at.y, 2.0f, shell);
            draw::rotated_rect(va, at + u * 0.2f, 4.2f, 1.0f, a, tone(lerp(shell, MON_WHITE, 0.4f)));
            if(!storm) draw::rotated_rect(va, at - u * 3.0f, 1.0f, 3.0f, a, tone(MON_CLOTH));   // the white scarf at the back of his neck
            if(sniper)
                for(int k = 0; k < 3; ++k)          // a scrim of grass and rag
                    draw::rect(va, std::floor(at.x + std::cos(static_cast<float>(k) * 2.1f) * 2.2f), std::floor(at.y + std::sin(static_cast<float>(k) * 2.1f) * 2.2f), 1, 1, tone(GRASS_DEAD));
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
            draw::rect(va, std::floor(body.x - u.x * 3.0f + v.x), std::floor(body.y - u.y * 3.0f + v.y), 1, 1, tone(mon ? MON_CLOTH : MON_GOLD));
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
            draw::thick_line(va, hip, hip + dir(la + kick) * 5.0f, 2.0f, legs);
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
        if(s.yours && !down) draw::rect(va, std::floor(c.x + v.x * 2.5f), std::floor(c.y + v.y * 2.5f), 1, 1, mon ? MON_CLOTH : MON_GOLD);

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
            draw::thick_line(va, hip, hip + u * 3.5f, 2.2f, legs);
            draw::rect(va, std::floor(hip.x + u.x * 4.0f), std::floor(hip.y + u.y * 4.0f), 2, 2, boot);
        }
        if(!(s.lost & LEG_L))
        {
            sf::Vector2f const hip = c - v * 1.6f;
            draw::thick_line(va, hip, hip - u * 4.5f, 2.2f, legs);
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
        draw::rect(va, std::floor(body.x + v.x * 3.2f - u.x * 0.5f), std::floor(body.y + v.y * 3.2f - u.y * 0.5f), 1, 2, mon ? MON_CLOTH : MON_GOLD);   // your section's armband

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
    // Monteran shoulder boards: white for the men, the lieutenant's rank on his.
    if(mon && !storm)
        for(int side : {-1, 1})
        {
            if(s.lost & (side < 0 ? ARM_L : ARM_R)) continue;
            draw::rect(va, std::floor(body.x + v.x * 3.0f * static_cast<float>(side)), std::floor(body.y + v.y * 3.0f * static_cast<float>(side)), 1, 1,
                       tone(s.role == Role::OFFICER ? MON_RANK : MON_CLOTH));
        }
    head(body + u * 0.6f);
}

}
