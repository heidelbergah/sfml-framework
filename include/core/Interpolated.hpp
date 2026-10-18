/**
 * Interpolated.hpp
 *
 * A value that tweens to its new target whenever you assign to it.
 *
 *     Interpolated<sf::Vector2f> pos;
 *     pos.set_transition(TransitionFunction::EaseOutBack);
 *     pos.set_duration(0.4f);
 *     pos = {200.f, 100.f};        // starts a 0.4s eased move
 *     sprite.setPosition(pos);     // implicitly converts to the current value
 *
 * Time comes from GameTime, not the system clock, so every tween in the
 * game pauses and slows down together. Tweens that must keep running
 * while the game is paused (pause menus, UI) should call set_unscaled(true).
 */

#ifndef INTERPOLATED_HPP
#define INTERPOLATED_HPP

#include "Easing.hpp"
#include "GameTime.hpp"

template<typename T>
struct Interpolated
{
    T m_start{};
    T m_end{};

    float m_start_time{};
    float m_speed{1.0f};        // 1 / duration_in_seconds

    bool m_unscaled{false};     // read real time instead of scaled game time

    TransitionFunction m_transition{TransitionFunction::None};

    explicit Interpolated(T const& initial_value = {})
        : m_start{initial_value},
          m_end{initial_value},
          m_start_time{GameTime::now()}
    {}

    [[nodiscard]]
    float get_current_time() const
    {
        return m_unscaled ? GameTime::real_now() : GameTime::now();
    }

    [[nodiscard]]
    float get_elapsed_seconds() const
    {
        return get_current_time() - m_start_time;
    }

    /*
     * Progress through the tween, 0 at the start and 1 at the end.
     */
    [[nodiscard]]
    float get_progress() const
    {
        if(m_transition == TransitionFunction::None) return 1.0f;
        float const t = get_elapsed_seconds() * m_speed;
        return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    }

    [[nodiscard]]
    bool is_transition_over() const
    {
        if(m_transition == TransitionFunction::None) return true;
        return get_elapsed_seconds() * m_speed >= 1.0f;
    }

    [[nodiscard]]
    T get_value() const
    {
        if(m_transition == TransitionFunction::None) return m_end;

        float const t = get_elapsed_seconds() * m_speed;
        if(t >= 1.0f) return m_end;
        if(t <= 0.0f) return m_start;

        T const delta{m_end - m_start};
        return m_start + delta * get_ratio(t, m_transition);
    }

    [[nodiscard]]
    T get_target() const { return m_end; }

    /*
     * Retarget the tween. Starts from wherever the value currently is,
     * so redirecting mid-flight stays smooth.
     */
    void set_value(T const& new_value)
    {
        m_start      = get_value();
        m_end        = new_value;
        m_start_time = get_current_time();
    }

    /*
     * Jump straight there with no tween.
     */
    void set_instant(T const& value)
    {
        m_start      = value;
        m_end        = value;
        m_start_time = get_current_time();
    }

    void set_duration(float duration)
    {
        m_speed = (duration > 0.0f) ? (1.0f / duration) : 1.0e9f;
    }

    [[nodiscard]]
    float get_duration() const { return 1.0f / m_speed; }

    void set_transition(TransitionFunction transition) { m_transition = transition; }

    [[nodiscard]]
    TransitionFunction get_transition() const { return m_transition; }

    /*
     * true  -> ignores GameTime::set_scale, keeps animating while paused
     * false -> obeys pause and slow-motion (the default)
     */
    void set_unscaled(bool unscaled) { m_unscaled = unscaled; }

    [[nodiscard]]
    bool is_unscaled() const { return m_unscaled; }

    /*
     * Use the object as if it were a plain T.
     */
    [[nodiscard]]
    operator T() const { return get_value(); }

    Interpolated& operator=(T const& new_value)
    {
        set_value(new_value);
        return *this;
    }
};

#endif
