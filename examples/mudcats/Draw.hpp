/**
 * Draw.hpp  -  Mud Cats
 *
 * Small helpers that append pixel-art shapes to a triangle VertexArray.
 * Everything in the game is drawn from these: no sprites, no textures.
 */

#ifndef MUDCATS_DRAW_HPP
#define MUDCATS_DRAW_HPP

#include <SFML/Graphics/VertexArray.hpp>

#include <algorithm>
#include <cmath>

namespace mudcats::draw
{

inline void quad(sf::VertexArray& va, sf::Vector2f a, sf::Vector2f b, sf::Vector2f c, sf::Vector2f d, sf::Color col)
{
    va.append({a, col}); va.append({b, col}); va.append({c, col});
    va.append({a, col}); va.append({c, col}); va.append({d, col});
}

inline void rect(sf::VertexArray& va, float x, float y, float w, float h, sf::Color c)
{
    quad(va, {x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}, c);
}

inline void disc(sf::VertexArray& va, float cx, float cy, float r, sf::Color c)
{
    int const ir = static_cast<int>(r);
    for(int dy = -ir; dy <= ir; ++dy)
    {
        float const half = std::floor(std::sqrt(std::max(0.0f, r * r - static_cast<float>(dy * dy))));
        rect(va, std::floor(cx - half), std::floor(cy) + static_cast<float>(dy), half * 2.0f + 1.0f, 1.0f, c);
    }
}

inline void rotated_rect(sf::VertexArray& va, sf::Vector2f centre, float length, float width, float angle, sf::Color c)
{
    sf::Vector2f const u{std::cos(angle), std::sin(angle)}, v{-u.y, u.x};
    sf::Vector2f const hu = u * (length * 0.5f), hv = v * (width * 0.5f);
    quad(va, centre - hu - hv, centre + hu - hv, centre + hu + hv, centre - hu + hv, c);
}

inline void thick_line(sf::VertexArray& va, sf::Vector2f a, sf::Vector2f b, float width, sf::Color c)
{
    sf::Vector2f const d = b - a;
    float const len = std::sqrt(d.x * d.x + d.y * d.y);
    if(len < 0.001f) return;
    sf::Vector2f const n = sf::Vector2f{-d.y, d.x} / len * (width * 0.5f);
    quad(va, a - n, b - n, b + n, a + n, c);
}

}

#endif
