/**
 * Perf.cpp  -  Mud Cats
 */

#include "Perf.hpp"

#include <cstdio>
#include <cstdlib>

namespace mudcats
{

Perf& Perf::get()
{
    static Perf p = []
    {
        Perf q;
        q.m_print = std::getenv("MUDCATS_PROFILE") != nullptr;
        return q;
    }();
    return p;
}

void Perf::frame()
{
    ++m_frames;
    float const elapsed = m_window_clock.getElapsedTime().asSeconds();
    if(elapsed < 0.5f) return;
    for(std::size_t i = 0; i < COUNT; ++i)
    {
        m_avg[i] = m_sum[i] / static_cast<float>(m_frames);
        m_sum[i] = 0.0f;
    }
    m_fps = static_cast<float>(m_frames) / elapsed;
    m_frame_ms = elapsed * 1000.0f / static_cast<float>(m_frames);
    m_steps_per_frame = static_cast<float>(m_steps) / static_cast<float>(m_frames);
    m_frames = 0;
    m_steps = 0;
    m_window_clock.restart();
    if(m_print && m_print_clock.getElapsedTime().asSeconds() >= 1.0f)
    {
        m_print_clock.restart();
        std::printf("%s\n", line().c_str());
        std::fflush(stdout);
    }
}

float Perf::update_ms() const
{
    float t = 0.0f;
    for(int i = THINK; i <= SCRIPT; ++i) t += m_avg[static_cast<std::size_t>(i)];
    return t;
}

float Perf::draw_ms() const
{
    float t = 0.0f;
    for(int i = SIGHT; i <= HUD; ++i) t += m_avg[static_cast<std::size_t>(i)];
    return t;
}

const char* Perf::name(Section s)
{
    static const char* const names[] = {"think", "act", "bodies", "fire", "bullets", "world", "effects", "script",
                                        "sight", "people", "fx", "over", "shade", "lights", "grade", "hud"};
    return names[static_cast<std::size_t>(s)];
}

std::string Perf::line() const
{
    char buf[512];
    int n = std::snprintf(buf, sizeof buf, "fps %.1f frame %.1fms steps %.2f update %.1f draw %.1f wait %.1f |",
                          m_fps, m_frame_ms, m_steps_per_frame, update_ms(), draw_ms(),
                          m_frame_ms - update_ms() - draw_ms());
    for(std::size_t i = 0; i < COUNT && n > 0 && n < static_cast<int>(sizeof buf); ++i)
        n += std::snprintf(buf + n, sizeof buf - static_cast<std::size_t>(n), " %s %.2f", name(static_cast<Section>(i)), m_avg[i]);
    return buf;
}

}
