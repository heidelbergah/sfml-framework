/**
 * Palette.hpp  -  Tetris
 *
 * Gruvbox dark. The neutrals carry the layout and the seven bright
 * accents are reserved exclusively for the tetrominoes, so a piece
 * colour never competes with chrome for attention.
 */

#ifndef TETRIS_PALETTE_HPP
#define TETRIS_PALETTE_HPP

#include "../../include/Includes.hpp"

namespace tetris
{
    // ---- neutrals -------------------------------------------------
    inline const sf::Color BG0_HARD {0x1d, 0x20, 0x21};
    inline const sf::Color BG0       {0x28, 0x28, 0x28};
    inline const sf::Color BG1       {0x3c, 0x38, 0x36};
    inline const sf::Color BG2       {0x50, 0x49, 0x45};
    inline const sf::Color BG3       {0x66, 0x5c, 0x54};
    inline const sf::Color BG4       {0x7c, 0x6f, 0x64};

    inline const sf::Color FG0       {0xfb, 0xf1, 0xc7};
    inline const sf::Color FG1       {0xeb, 0xdb, 0xb2};
    inline const sf::Color FG2       {0xd5, 0xc4, 0xa1};
    inline const sf::Color FG3       {0xbd, 0xae, 0x93};
    inline const sf::Color FG4       {0xa8, 0x99, 0x84};
    inline const sf::Color GRAY      {0x92, 0x83, 0x74};

    // ---- accents --------------------------------------------------
    inline const sf::Color RED       {0xfb, 0x49, 0x34};
    inline const sf::Color GREEN     {0xb8, 0xbb, 0x26};
    inline const sf::Color YELLOW    {0xfa, 0xbd, 0x2f};
    inline const sf::Color BLUE      {0x83, 0xa5, 0x98};
    inline const sf::Color PURPLE    {0xd3, 0x86, 0x9b};
    inline const sf::Color AQUA      {0x8e, 0xc0, 0x7c};
    inline const sf::Color ORANGE    {0xfe, 0x80, 0x19};

    inline const sf::Color RED_DIM   {0xcc, 0x24, 0x1d};
    inline const sf::Color GREEN_DIM {0x98, 0x97, 0x1a};
    inline const sf::Color YELLOW_DIM{0xd7, 0x99, 0x21};
    inline const sf::Color BLUE_DIM  {0x45, 0x85, 0x88};
    inline const sf::Color PURPLE_DIM{0xb1, 0x62, 0x86};
    inline const sf::Color AQUA_DIM  {0x68, 0x9d, 0x6a};
    inline const sf::Color ORANGE_DIM{0xd6, 0x5d, 0x0e};

    /*
     * Blends two colours. Used for hit flashes, fades and the level
     * accent shifting as you climb.
     */
    [[nodiscard]]
    inline sf::Color mix(sf::Color a, sf::Color b, float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        auto lerp = [t](std::uint8_t x, std::uint8_t y)
        {
            return static_cast<std::uint8_t>(x + (static_cast<float>(y) - x) * t);
        };
        return sf::Color{lerp(a.r, b.r), lerp(a.g, b.g),
                         lerp(a.b, b.b), lerp(a.a, b.a)};
    }

    [[nodiscard]]
    inline sf::Color with_alpha(sf::Color c, float alpha)
    {
        c.a = static_cast<std::uint8_t>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f);
        return c;
    }

    [[nodiscard]]
    inline sf::Color scaled(sf::Color c, float factor)
    {
        auto ch = [factor](std::uint8_t v)
        {
            return static_cast<std::uint8_t>(
                std::clamp(static_cast<float>(v) * factor, 0.0f, 255.0f));
        };
        return sf::Color{ch(c.r), ch(c.g), ch(c.b), c.a};
    }

    /*
     * Accent colour for a level, cycling through the warm end of the
     * palette so the board's glow shifts as you progress.
     */
    [[nodiscard]]
    inline sf::Color level_accent(int level)
    {
        static const std::vector<sf::Color> cycle = {
            AQUA, BLUE, PURPLE, RED, ORANGE, YELLOW, GREEN
        };
        return cycle[static_cast<std::size_t>(std::max(0, level - 1)) % cycle.size()];
    }
}

#endif
