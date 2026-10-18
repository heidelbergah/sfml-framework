/**
 * Random.hpp
 *
 * One shared generator for the whole framework.
 *
 * Replaces rand()/srand(). Seeding the global C generator inside a
 * constructor -- which the old ParticleSystem and Markov both did --
 * means two objects built in the same second produce identical
 * sequences, and it silently resets any seed the caller had chosen.
 *
 * Call seed() once at startup if you want reproducible runs.
 */

#ifndef RANDOM_HPP
#define RANDOM_HPP

#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Angle.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstdint>
#include <random>
#include <vector>

namespace Random
{
    inline std::mt19937& generator()
    {
        static std::mt19937 gen{std::random_device{}()};
        return gen;
    }

    /*
     * Fix the sequence, for replays or deterministic tests.
     */
    inline void seed(std::uint32_t value)
    {
        generator().seed(value);
    }

    [[nodiscard]]
    inline float real(float min, float max)
    {
        if(min >= max) return min;
        return std::uniform_real_distribution<float>{min, max}(generator());
    }

    /*
     * Inclusive on both ends.
     */
    [[nodiscard]]
    inline int integer(int min, int max)
    {
        if(min >= max) return min;
        return std::uniform_int_distribution<int>{min, max}(generator());
    }

    [[nodiscard]]
    inline float normal(float mean, float stddev)
    {
        if(stddev <= 0.0f) return mean;
        return std::normal_distribution<float>{mean, stddev}(generator());
    }

    [[nodiscard]]
    inline bool chance(float probability)
    {
        return real(0.0f, 1.0f) < probability;
    }

    [[nodiscard]]
    inline sf::Angle angle()
    {
        return sf::degrees(real(0.0f, 360.0f));
    }

    /*
     * Uniform point in [-1, 1] on both axes.
     */
    [[nodiscard]]
    inline sf::Vector2f unit_offset()
    {
        return { real(-1.0f, 1.0f), real(-1.0f, 1.0f) };
    }

    template<typename T>
    [[nodiscard]]
    const T& pick(const std::vector<T>& options)
    {
        return options[static_cast<std::size_t>(
            integer(0, static_cast<int>(options.size()) - 1))];
    }
}

#endif
