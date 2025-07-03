/**
 * Includes.hpp
 */

#ifndef INCLUDES_HPP
#define INCLUDES_HPP

#include <SFML/Window.hpp>
#include <SFML/System.hpp>
#include <SFML/Graphics.hpp>
//#include <SFML/Audio.hpp>
//#include <SFML/Network.hpp>

#include <functional>
#include <iostream>
#include <algorithm>
#include <optional>
#include <cmath>

float ease_in_out_exponential(float t);
float ease_out_back(float t);
float ease_in_back(float t);
float ease_out_elastic(float t);

enum class TransitionFunction
{
    None,
    Linear,
    EaseInOutExponential,
    EaseOutBack,
    EaseInBack,
    EaseOutElastic
};

float get_ratio(float t, TransitionFunction transition);

template<typename T>
struct Interpolated
{
    T m_start{};
    T m_end{};

    float m_start_time{};

    float m_speed{1.0f};

    TransitionFunction m_transition{TransitionFunction::Linear};

    explicit Interpolated(T const& initial_value = {})
        : m_start{initial_value},
          m_end{m_start}
    {}

    [[nodiscard]]
    float get_current_time() const
    {
        auto const now = std::chrono::steady_clock::now();
        auto const duration = now.time_since_epoch();

        auto const seconds = std::chrono::duration_cast<std::chrono::duration<float>>(duration);
        return seconds.count();
    }

    [[nodiscard]]
    float get_elapsed_seconds() const
    {
        return get_current_time() - m_start_time;
    }

    [[nodiscard]]
    T get_value() const
    {
        float const elapsed = get_elapsed_seconds();
        float const t = elapsed * m_speed;
        if(t >= 1.0f)
        {
            return m_end;
        }

        T const delta{m_end - m_start};
        return m_start + delta * get_ratio(t, m_transition);
    }

    void set_value(T const& new_value)
    {
        m_start = get_value();
        m_end = new_value;
        m_start_time = get_current_time();
    }

    void set_duration(float duration)
    {
        m_speed = 1.0f / duration;
    }

    void set_transition(TransitionFunction transition)
    {
        m_transition = transition;
    }

    /*
     * Cast operator to use this object as if it was of type T
     */
    [[nodiscard]]
    operator T() const
    {
        return get_value();
    }

    void operator=(T const& new_value)
    {
        set_value(new_value);
    }
};

struct Vector
{
    float magnitude;
    sf::Angle direction;
};

static Vector add_vectors(Vector v1, Vector v2)
{
    float r1 = v1.magnitude, r2 = v2.magnitude;
    float p1 = v1.direction.asRadians();
    float p2 = v2.direction.asRadians();

    float p_diff = p2 - p1;
    
    float r = sqrt((r1 * r1) + (r2 * r2) + (2 * r1 * r2 * cos(p_diff)));

    sf::Angle p;
    float y = r2 * sin(p_diff);
    float x = r1 + (r2 * cos(p_diff));

    p = sf::radians(p1 + atan2(y, x));

    Vector vector;
    vector.magnitude = r;
    vector.direction = p;
    return vector;
}

#endif
