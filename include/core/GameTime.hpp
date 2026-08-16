/**
 * GameTime.hpp
 *
 * A single, authoritative clock for the whole framework.
 *
 * Everything that animates over time reads from here rather than from
 * std::chrono directly. That means one call to set_scale() pauses or
 * slows down every tween, particle and camera move in the game at once,
 * and replays/tests can step time deterministically.
 *
 * Advance it exactly once per fixed update, from your game loop.
 */

#ifndef GAME_TIME_HPP
#define GAME_TIME_HPP

#include <SFML/System/Time.hpp>

class GameTime
{
private:
    inline static float s_now   = 0.0f;   // scaled, accumulated seconds
    inline static float s_real  = 0.0f;   // unscaled seconds since start
    inline static float s_scale = 1.0f;

    GameTime() = delete;
    ~GameTime() = delete;

public:
    /*
     * Call once per fixed timestep. Everything downstream sees the result.
     */
    static void advance(sf::Time delta_time)
    {
        s_real += delta_time.asSeconds();
        s_now  += delta_time.asSeconds() * s_scale;
    }

    /*
     * Scaled game time. This is what tweens and gameplay should use.
     */
    [[nodiscard]]
    static float now() { return s_now; }

    /*
     * Unscaled wall time. Use for things that must keep running while
     * paused: UI transitions, pause-menu animations, loading spinners.
     */
    [[nodiscard]]
    static float real_now() { return s_real; }

    /*
     * 0.0 freezes the game, 0.25 is slow-motion, 1.0 is normal,
     * 2.0 is double speed. Negative values are clamped to zero.
     */
    static void set_scale(float scale) { s_scale = scale < 0.0f ? 0.0f : scale; }

    [[nodiscard]]
    static float get_scale() { return s_scale; }

    static void pause()  { s_scale = 0.0f; }
    static void resume() { s_scale = 1.0f; }

    [[nodiscard]]
    static bool is_paused() { return s_scale == 0.0f; }

    /*
     * Rewind everything to zero. Useful between test cases.
     */
    static void reset()
    {
        s_now   = 0.0f;
        s_real  = 0.0f;
        s_scale = 1.0f;
    }
};

#endif
