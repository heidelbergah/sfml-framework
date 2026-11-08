/**
 * Perf.hpp  -  Mud Cats
 *
 * Where a frame's time goes. Each piece of the update and of the drawing
 * is timed, and every half second the averages are published: how long a
 * frame took, how many simulation steps ran in it, and how many
 * milliseconds of each frame each piece cost. What's left over is time
 * spent waiting: on the graphics card to finish, or for the screen.
 *
 * F3 in a battle shows it on screen. MUDCATS_PROFILE=1 also prints it to
 * the terminal every second, for measuring without looking.
 */

#ifndef MUDCATS_PERF_HPP
#define MUDCATS_PERF_HPP

#include <SFML/System/Clock.hpp>
#include <SFML/System/Time.hpp>

#include <array>
#include <string>

namespace mudcats
{

class Perf
{
public:
    enum Section
    {
        THINK, ACT, BODIES, FIRE, BULLETS, WORLD, EFFECTS, SCRIPT,      // the simulation
        SIGHT, PEOPLE, FX, OVER, SHADE, LIGHTS, GRADE, HUD,             // the drawing
        COUNT
    };

    static Perf& get();

    void add(Section s, float ms) { m_sum[s] += ms; }
    void step() { ++m_steps; }
    // At the start of every drawn frame: rolls the averages over when it's time.
    void frame();

    [[nodiscard]] float fps() const { return m_fps; }
    [[nodiscard]] float frame_ms() const { return m_frame_ms; }
    [[nodiscard]] float steps_per_frame() const { return m_steps_per_frame; }
    [[nodiscard]] float ms(Section s) const { return m_avg[static_cast<std::size_t>(s)]; }
    [[nodiscard]] float update_ms() const;
    [[nodiscard]] float draw_ms() const;
    [[nodiscard]] static const char* name(Section s);

    // One line of it all, for the terminal.
    [[nodiscard]] std::string line() const;

    bool shown{false};

private:
    std::array<float, COUNT> m_sum{};
    std::array<float, COUNT> m_avg{};
    sf::Clock m_window_clock;
    sf::Clock m_print_clock;
    int   m_frames{0};
    int   m_steps{0};
    float m_fps{0.0f};
    float m_frame_ms{0.0f};
    float m_steps_per_frame{0.0f};
    bool  m_print{false};
};

// Times the rest of the enclosing block.
class PerfScope
{
public:
    explicit PerfScope(Perf::Section s) : m_section(s) {}
    ~PerfScope() { Perf::get().add(m_section, static_cast<float>(m_clock.getElapsedTime().asMicroseconds()) / 1000.0f); }
    PerfScope(const PerfScope&) = delete;
    PerfScope& operator=(const PerfScope&) = delete;

private:
    Perf::Section m_section;
    sf::Clock m_clock;
};

}

#endif
