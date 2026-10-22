/**
 * Palette.hpp  -  Cinder
 *
 * Colours from Ashen by ficd (MIT licensed), a Neovim colorscheme
 * described as "embers sizzling out in an old fire pit":
 * https://codeberg.org/ficd/ashen
 *
 * The game gives the palette a meaning. Grey is ash: dead, cold, still.
 * Reds, oranges and gold are fire and life, and only show where it is
 * warm. Teal is kept for the one cold thing, water and wind, so hazards
 * stay readable even in a world gone grey.
 */

#ifndef CINDER_PALETTE_HPP
#define CINDER_PALETTE_HPP

#include <SFML/Graphics/Color.hpp>

namespace cinder
{

// Fire
inline const sf::Color RED_FLAME     {0xC5, 0x30, 0x30};
inline const sf::Color RED_GLOWING   {0xDF, 0x64, 0x64};
inline const sf::Color RED_EMBER     {0xB1, 0x42, 0x42};
inline const sf::Color RED_KINDLING  {0xBD, 0x4C, 0x4C};
inline const sf::Color RED_CRIMSON   {0xA8, 0x48, 0x48};
inline const sf::Color RED_BRICK     {0x85, 0x3D, 0x3D};
inline const sf::Color RED_DEEP      {0x7A, 0x2E, 0x2E};
inline const sf::Color RED_ASHEN     {0x6F, 0x29, 0x29};
inline const sf::Color ORANGE_GLOW   {0xD8, 0x7C, 0x4A};
inline const sf::Color ORANGE_BLAZE  {0xC4, 0x69, 0x3D};
inline const sf::Color ORANGE_SMOLDER{0xE4, 0x9A, 0x44};
inline const sf::Color GOLDEN        {0xE5, 0xA7, 0x2A};
inline const sf::Color YELLOW        {0xF4, 0xCA, 0x64};

// Cold
inline const sf::Color TEAL          {0x4A, 0x8B, 0x8B};
inline const sf::Color TEAL_DARK     {0x3A, 0x6E, 0x6E};
inline const sf::Color TEAL_DEEP     {0x1A, 0x3F, 0x3F};
inline const sf::Color AQUA          {0x4A, 0xC4, 0xC4};

// Life
inline const sf::Color GREEN_LIGHT   {0x62, 0x9C, 0x7D};
inline const sf::Color GREEN         {0x1E, 0x6F, 0x54};

// Ash: background and the twelve greys
inline const sf::Color BACKGROUND    {0x12, 0x12, 0x12};
inline const sf::Color G1  {0xE5, 0xE5, 0xE5};
inline const sf::Color G2  {0xD5, 0xD5, 0xD5};
inline const sf::Color G3  {0xB4, 0xB4, 0xB4};
inline const sf::Color G4  {0xA7, 0xA7, 0xA7};
inline const sf::Color G5  {0x94, 0x94, 0x94};
inline const sf::Color G6  {0x73, 0x73, 0x73};
inline const sf::Color G7  {0x53, 0x53, 0x53};
inline const sf::Color G8  {0x32, 0x32, 0x32};
inline const sf::Color G9  {0x21, 0x21, 0x21};
inline const sf::Color G10 {0x1D, 0x1D, 0x1D};
inline const sf::Color G11 {0x19, 0x19, 0x19};
inline const sf::Color G12 {0x15, 0x15, 0x15};

inline sf::Color with_alpha(sf::Color c, float a)
{
    c.a = static_cast<std::uint8_t>(a < 0.0f ? 0.0f : (a > 1.0f ? 255.0f : a * 255.0f));
    return c;
}

inline sf::Color lerp(sf::Color a, sf::Color b, float t)
{
    auto l = [t](std::uint8_t x, std::uint8_t y)
    {
        return static_cast<std::uint8_t>(static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t);
    };
    return {l(a.r, b.r), l(a.g, b.g), l(a.b, b.b), l(a.a, b.a)};
}

}

#endif
