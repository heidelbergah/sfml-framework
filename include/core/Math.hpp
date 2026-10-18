/**
 * Math.hpp
 *
 * Small vector helpers.
 *
 * Note on conventions: screen space is y-down, but it reads better to
 * think of 0 degrees as right and 90 degrees as up. All the polar
 * helpers here flip y for you, so sf::degrees(90) points up on screen.
 */

#ifndef MATH_HPP
#define MATH_HPP

#include <SFML/System/Angle.hpp>
#include <SFML/System/Vector2.hpp>

#include <cmath>

/*
 * Polar vector. Convenient for describing emission ("30 units/sec, due
 * left"), but stored velocities are Cartesian -- adding two polar
 * vectors costs a sqrt and an atan2, and that adds up fast across
 * thousands of particles.
 */
struct Vector
{
    float     magnitude{0.0f};
    sf::Angle direction{};
};

/*
 * Polar -> Cartesian, with the y-flip described above.
 */
[[nodiscard]]
inline sf::Vector2f to_cartesian(Vector v)
{
    float const radians = v.direction.asRadians();
    return { v.magnitude * std::cos(radians),
            -v.magnitude * std::sin(radians) };
}

/*
 * Cartesian -> polar, inverse of the above.
 */
[[nodiscard]]
inline Vector to_polar(sf::Vector2f v)
{
    return { std::sqrt(v.x * v.x + v.y * v.y),
             sf::radians(std::atan2(-v.y, v.x)) };
}

[[nodiscard]]
inline float length(sf::Vector2f v)
{
    return std::sqrt(v.x * v.x + v.y * v.y);
}

[[nodiscard]]
inline float length_squared(sf::Vector2f v)
{
    return v.x * v.x + v.y * v.y;
}

[[nodiscard]]
inline sf::Vector2f normalized(sf::Vector2f v)
{
    float const len = length(v);
    return (len > 0.0f) ? sf::Vector2f{v.x / len, v.y / len} : sf::Vector2f{0.0f, 0.0f};
}

[[nodiscard]]
inline float distance(sf::Vector2f a, sf::Vector2f b)
{
    return length(b - a);
}

/*
 * Adds two polar vectors. Kept for convenience at API boundaries --
 * prefer converting to Cartesian and using operator+ in hot loops.
 */
[[nodiscard]]
inline Vector add_vectors(Vector v1, Vector v2)
{
    return to_polar(to_cartesian(v1) + to_cartesian(v2));
}

/*
 * Snaps to whole pixels. Drawing pixel art at fractional coordinates is
 * what makes it shimmer, so widget and sprite transforms round first.
 */
[[nodiscard]]
inline sf::Vector2f floor_vector(sf::Vector2f v)
{
    return { std::floor(v.x), std::floor(v.y) };
}

#endif
