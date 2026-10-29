/**
 * Icons.hpp  -  Mud Cats
 *
 * Little pictures made of pixels, for the HUD and the briefing: a man's
 * role, a section's orders, the things he carries. Each is a few rows of
 * text, '#' for a pixel, drawn at whatever scale is wanted.
 */

#ifndef MUDCATS_ICONS_HPP
#define MUDCATS_ICONS_HPP

#include "Draw.hpp"
#include "Palette.hpp"
#include "Soldier.hpp"

namespace mudcats
{

// A small picture made of pixels, drawn at `scale`.
inline void glyph(sf::VertexArray& va, const char* const* rows, int n, sf::Vector2f at, float scale, sf::Color c)
{
    for(int y = 0; y < n; ++y)
        for(int x = 0; rows[y][x]; ++x)
            if(rows[y][x] == '#')
                draw::rect(va, at.x + static_cast<float>(x) * scale, at.y + static_cast<float>(y) * scale, scale, scale, c);
}

// HUD icons, 7 by 7, drawn at twice that. A man's role, a section's
// orders and what you carry are shown as these rather than spelled out.
inline const char* const ICON_RIFLEMAN[] = {"..###..", "..###..", ".#####.", "#.###.#", "..###..", "..#.#..", ".##.##."};
inline const char* const ICON_OFFICER[]  = {"...#...", "..###..", "#######", ".#####.", "..###..", ".##.##.", ".#...#."};
inline const char* const ICON_MEDIC[]    = {"..###..", "..###..", "#######", "#######", "#######", "..###..", "..###.."};
inline const char* const ICON_GUNNER[]   = {".......", "..#....", "#######", "####...", ".#..#..", "#....#.", "......."};
inline const char* const ICON_FLAMER[]   = {"...#...", "..##...", "..###.#", ".#####.", "#######", "#######", ".#####."};
inline const char* const ICON_RADIO[]    = {"#......", ".#.....", "..#....", "#######", "#.#.###", "#######", "#######"};
inline const char* const ICON_STORM[]    = {"...###.", "..###..", ".###...", "#######", "...###.", "..###..", ".###..."};
inline const char* const ICON_SNIPER[]   = {"..###..", ".#.#.#.", "#..#..#", "#######", "#..#..#", ".#.#.#.", "..###.."};
inline const char* const ICON_SAPPER[]   = {"#.....#", ".#...#.", "..#.#..", "...#...", "..#.#..", ".##.##.", "##...##"};

inline const char* const ICON_FOLLOW[]   = {"..###..", ".#...#.", "#.....#", "#..#..#", "#.....#", ".#...#.", "..###.."};
inline const char* const ICON_HOLD[]     = {".......", ".##.##.", ".##.##.", ".##.##.", ".##.##.", ".##.##.", "......."};
inline const char* const ICON_ADVANCE[]  = {"...#...", "..###..", ".#####.", "#######", "..###..", "..###..", "..###.."};
inline const char* const ICON_COVER[]    = {".......", "###.##.", ".......", ".###.##", ".......", "###.##.", "......."};
inline const char* const ICON_CHARGE[]   = {"...#...", "..###..", ".##.##.", "##.#.##", "..###..", ".##.##.", "##...##"};

inline const char* const ICON_GRENADE[]  = {".....##", "....###", "....##.", "...#...", "..#....", ".#.....", "#......"};
inline const char* const ICON_DEAD[]     = {".......", ".......", ".......", "##.....", "#######", "##.#.#.", "......."};
inline const char* const ICON_SMOKE[]    = {".......", "..##...", ".####..", "######.", "#######", ".#####.", "......."};
inline const char* const ICON_FLARE[]    = {"#..#..#", ".#.#.#.", "..###..", "#######", "..###..", ".#.#.#.", "#..#..#"};
inline const char* const ICON_SATCHEL[]  = {"..###..", ".#...#.", "#######", "#######", "##.####", "#######", "#######"};
inline const char* const ICON_BANDAGE[]  = {".......", ".#####.", "#######", "##.####", "#######", ".#####.", "......."};
inline const char* const ICON_KIT[]      = {"..###..", "#######", "###.###", "##...##", "###.###", "#######", "#######"};
inline const char* const ICON_BAGS[]     = {".......", ".......", ".#####.", "#######", "#######", ".#####.", "......."};
inline const char* const ICON_SHELL[]    = {"...#...", "..###..", "..###..", "..###..", "..###..", ".#####.", "......."};
inline const char* const ICON_BIPOD[]    = {".......", "#######", "..#....", ".#.#...", "#...#..", ".......", "......."};
inline const char* const ICON_DROP[]     = {"...#...", "..###..", "..###..", ".#####.", ".#####.", ".#####.", "..###.."};
inline const char* const ICON_FIRE[]     = {"...#...", "..##.#.", ".####..", ".#####.", "#######", "#######", ".#####."};

inline const char* const* role_icon(Role r)
{
    switch(r)
    {
        case Role::OFFICER:      return ICON_OFFICER;
        case Role::MEDIC:        return ICON_MEDIC;
        case Role::GUNNER:       return ICON_GUNNER;
        case Role::FLAMER:       return ICON_FLAMER;
        case Role::RADIOMAN:     return ICON_RADIO;
        case Role::STORMTROOPER: return ICON_STORM;
        case Role::SNIPER:       return ICON_SNIPER;
        case Role::SAPPER:       return ICON_SAPPER;
        default:                 return ICON_RIFLEMAN;
    }
}

inline const char* const* order_icon(Order o)
{
    switch(o)
    {
        case Order::FOLLOW:   return ICON_FOLLOW;
        case Order::ADVANCE:  return ICON_ADVANCE;
        case Order::SUPPRESS: return ICON_COVER;
        case Order::CHARGE:   return ICON_CHARGE;
        default:              return ICON_HOLD;
    }
}

inline void icon(sf::VertexArray& va, const char* const* rows, sf::Vector2f at, sf::Color c, float scale = 2.0f)
{
    glyph(va, rows, 7, at + sf::Vector2f{scale, scale}, scale, with_alpha(INK, c.a / 255.0f * 0.7f));
    glyph(va, rows, 7, at, scale, c);
}

}

#endif
