/**
 * Palette.hpp  -  Mud Cats
 *
 * Mud first. Almost everything is a brown or a grey, so the few strong
 * colours mean something: Monteran blue and white, Rechivii tan and
 * field grey, fire, and blood.
 */

#ifndef MUDCATS_PALETTE_HPP
#define MUDCATS_PALETTE_HPP

#include <SFML/Graphics/Color.hpp>

#include <cstdint>

namespace mudcats
{

// Ground
inline const sf::Color MUD_DARK   {0x2A, 0x22, 0x1B};
inline const sf::Color MUD        {0x3B, 0x30, 0x25};
inline const sf::Color MUD_LIGHT  {0x4D, 0x40, 0x31};
inline const sf::Color MUD_PALE   {0x63, 0x55, 0x42};
inline const sf::Color GRASS_DEAD {0x4F, 0x4A, 0x2E};
inline const sf::Color GRASS      {0x45, 0x52, 0x2E};
inline const sf::Color WATER      {0x2C, 0x33, 0x30};
inline const sf::Color PLANK      {0x5E, 0x48, 0x30};
inline const sf::Color PLANK_DARK {0x40, 0x31, 0x22};
inline const sf::Color SANDBAG    {0x7A, 0x6A, 0x4B};
inline const sf::Color SANDBAG_DK {0x58, 0x4C, 0x36};
inline const sf::Color CONCRETE   {0x6E, 0x6C, 0x66};
inline const sf::Color CONCRETE_DK{0x4A, 0x48, 0x44};
inline const sf::Color BRICK      {0x7A, 0x4A, 0x38};
inline const sf::Color BRICK_DK   {0x55, 0x33, 0x27};
inline const sf::Color WIRE       {0x8A, 0x86, 0x7E};
inline const sf::Color BARK       {0x3A, 0x2E, 0x26};
inline const sf::Color SHADOW     {0x00, 0x00, 0x00, 0x60};

// Monterans: blue and white, a little gold for rank
inline const sf::Color MON_BLUE   {0x3A, 0x5A, 0x92};
inline const sf::Color MON_BLUE_DK{0x27, 0x3E, 0x68};
inline const sf::Color MON_WHITE  {0xD8, 0xD8, 0xD0};
inline const sf::Color MON_GOLD   {0xD4, 0xA8, 0x3A};
inline const sf::Color MON_HELMET {0x4C, 0x6A, 0x9C};

// Rechivii: tan, with field-grey helmets
inline const sf::Color RCH_TAN    {0xA8, 0x8E, 0x62};
inline const sf::Color RCH_TAN_DK {0x7C, 0x67, 0x45};
inline const sf::Color RCH_HELMET {0x6A, 0x6E, 0x6A};
inline const sf::Color RCH_RED    {0x8E, 0x2E, 0x2A};

// People
inline const sf::Color SKIN       {0xC9, 0x9E, 0x7C};
inline const sf::Color SKIN_DK    {0x9A, 0x74, 0x58};
inline const sf::Color LEATHER    {0x4A, 0x36, 0x24};
inline const sf::Color GUNMETAL   {0x2E, 0x2E, 0x30};
inline const sf::Color WOOD       {0x6A, 0x4A, 0x2C};

// Blood
inline const sf::Color BLOOD      {0x8A, 0x10, 0x10};
inline const sf::Color BLOOD_DK   {0x52, 0x0A, 0x0A};
inline const sf::Color BLOOD_BRIGHT{0xB8, 0x1C, 0x18};
inline const sf::Color GORE       {0xA0, 0x40, 0x3A};
inline const sf::Color BONE       {0xD8, 0xCF, 0xB8};
inline const sf::Color CHAR       {0x1E, 0x19, 0x16};

// Fire and light
inline const sf::Color FIRE_WHITE {0xFF, 0xF2, 0xC0};
inline const sf::Color FIRE_YELLOW{0xFF, 0xC8, 0x40};
inline const sf::Color FIRE_ORANGE{0xF0, 0x7A, 0x20};
inline const sf::Color FIRE_RED   {0xB8, 0x30, 0x10};
inline const sf::Color SMOKE      {0x5A, 0x56, 0x50};
inline const sf::Color TRACER     {0xFF, 0xD8, 0x80};

// Interface
inline const sf::Color INK        {0x16, 0x14, 0x12};
inline const sf::Color PAPER      {0xD9, 0xCF, 0xB4};
inline const sf::Color PAPER_DK   {0xA8, 0x9C, 0x7E};
inline const sf::Color TEXT       {0xE4, 0xDE, 0xCC};
inline const sf::Color TEXT_DIM   {0x9A, 0x92, 0x80};
inline const sf::Color TEXT_FAINT {0x60, 0x5A, 0x50};
inline const sf::Color ALERT      {0xD0, 0x4A, 0x36};

inline sf::Color with_alpha(sf::Color c, float a)
{
    c.a = static_cast<std::uint8_t>(a < 0.0f ? 0.0f : (a > 1.0f ? 255.0f : a * 255.0f));
    return c;
}

inline sf::Color lerp(sf::Color a, sf::Color b, float t)
{
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    auto l = [t](std::uint8_t x, std::uint8_t y)
    {
        return static_cast<std::uint8_t>(static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t);
    };
    return {l(a.r, b.r), l(a.g, b.g), l(a.b, b.b), l(a.a, b.a)};
}

}

#endif
