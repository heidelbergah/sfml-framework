/**
 * Effects.cpp  -  Showcase
 */

#include "Effects.hpp"

#include <cmath>

namespace showcase
{

namespace
{
    // Fixed simulation rate for the effects that step a simulation, so
    // they behave the same however fast frames arrive.
    constexpr float STEP = 1.0f / 60.0f;

    /*
     * Loads shaders/showcase/<name>.frag once; later windows reuse it.
     */
    sf::Shader* fetch(ShaderManager& shaders, const std::string& name)
    {
        if(sf::Shader* existing = shaders.get_shader("showcase_" + name)) return existing;
        return shaders.load_fragment("showcase_" + name, "shaders/showcase/" + name + ".frag");
    }

    sf::Glsl::Vec2 to_vec2(sf::Vector2u size)
    {
        return {static_cast<float>(size.x), static_cast<float>(size.y)};
    }

    const sf::Color WATER_DEEP{36, 58, 82};
    const sf::Color SCOPE_DIM {40, 62, 52};
    const sf::Color BEZEL     {12, 11, 15};
}

// ================================================================ plasma

bool PlasmaEffect::load(ShaderManager& shaders)
{
    m_shader = fetch(shaders, "plasma");
    return m_shader != nullptr;
}

void PlasmaEffect::update(float dt, const Pointer& pointer)
{
    m_time += dt;

    // Hovered, the swirl's centre chases the cursor. Otherwise it drifts
    // on a slow loop of its own.
    sf::Vector2f const extent(size());
    sf::Vector2f const target = pointer.inside
        ? sf::Vector2f{pointer.pos.x / extent.x, pointer.pos.y / extent.y}
        : sf::Vector2f{0.5f + 0.28f * std::cos(m_time * 0.31f),
                       0.5f + 0.22f * std::sin(m_time * 0.43f)};

    m_center += (target - m_center) * std::min(1.0f, dt * 3.0f);

    if(pointer.pressed) m_ramp = (m_ramp + 1) % 3;
}

void PlasmaEffect::render(sf::RenderTarget& target)
{
    m_shader->setUniform("resolution", to_vec2(size()));
    m_shader->setUniform("time", m_time);
    m_shader->setUniform("center", sf::Glsl::Vec2{m_center.x, m_center.y});

    switch(m_ramp)
    {
        case 0:  set_ramp(*m_shader, INK, ROSE,  AMBER, TEXT); break;
        case 1:  set_ramp(*m_shader, INK, SKY,   SAGE,  TEXT); break;
        default: set_ramp(*m_shader, INK, CORAL, AMBER, ROSE); break;
    }

    draw_quad(target, size(), m_shader);
}

// ================================================================ ripple

bool RippleEffect::load(ShaderManager& shaders)
{
    m_shader = fetch(shaders, "ripple");
    if(!m_shader || !m_heights.resize(size())) return false;

    std::size_t const cells = static_cast<std::size_t>(size().x) * size().y;
    m_current.assign(cells, 0.0f);
    m_previous.assign(cells, 0.0f);
    m_pixels.assign(cells * 4, 255);
    return true;
}

void RippleEffect::drop(sf::Vector2f at, float radius, float strength)
{
    int const w  = static_cast<int>(size().x);
    int const h  = static_cast<int>(size().y);
    int const r  = static_cast<int>(std::ceil(radius));
    int const cx = static_cast<int>(at.x);
    int const cy = static_cast<int>(at.y);

    for(int y = std::max(1, cy - r); y <= std::min(h - 2, cy + r); ++y)
    {
        for(int x = std::max(1, cx - r); x <= std::min(w - 2, cx + r); ++x)
        {
            float const d = std::hypot(static_cast<float>(x - cx), static_cast<float>(y - cy));
            if(d > radius) continue;
            m_current[static_cast<std::size_t>(y * w + x)] -= strength * (1.0f - d / radius);
        }
    }
}

void RippleEffect::step()
{
    // The classic two-buffer wave: each cell becomes the average of its
    // neighbours, doubled, minus what it was a step ago, then damped.
    int const w = static_cast<int>(size().x);
    int const h = static_cast<int>(size().y);

    for(int y = 1; y < h - 1; ++y)
    {
        for(int x = 1; x < w - 1; ++x)
        {
            std::size_t const i = static_cast<std::size_t>(y * w + x);
            float const sum = m_current[i - 1] + m_current[i + 1] +
                              m_current[i - static_cast<std::size_t>(w)] +
                              m_current[i + static_cast<std::size_t>(w)];
            m_previous[i] = (sum * 0.5f - m_previous[i]) * 0.982f;
        }
    }
    std::swap(m_current, m_previous);
}

void RippleEffect::update(float dt, const Pointer& pointer)
{
    if(pointer.pressed) drop(pointer.pos, 4.0f, 1.6f);

    // Left alone, it rains now and then, so the window is never still.
    m_rain_timer -= dt;
    if(m_rain_timer <= 0.0f && !pointer.inside)
    {
        sf::Vector2f const extent(size());
        drop({Random::real(8.0f, extent.x - 8.0f), Random::real(8.0f, extent.y - 8.0f)},
             3.0f, 1.0f);
        m_rain_timer = Random::real(0.5f, 1.3f);
    }

    m_step_accumulator = std::min(m_step_accumulator + dt, STEP * 4.0f);
    while(m_step_accumulator >= STEP)
    {
        m_step_accumulator -= STEP;
        if(pointer.down && pointer.inside) drop(pointer.pos, 2.5f, 0.45f);
        step();
    }

    // Heights go up as a texture, 0.5 meaning still water.
    for(std::size_t i = 0; i < m_current.size(); ++i)
    {
        float const v = std::clamp(m_current[i] * 0.5f + 0.5f, 0.0f, 1.0f);
        m_pixels[i * 4] = static_cast<std::uint8_t>(v * 255.0f);
    }
    m_heights.update(m_pixels.data());
}

void RippleEffect::render(sf::RenderTarget& target)
{
    m_shader->setUniform("resolution", to_vec2(size()));
    m_shader->setUniform("heights", m_heights);
    set_ramp(*m_shader, INK, WATER_DEEP, SKY, TEXT);
    draw_quad(target, size(), m_shader);
}

// ================================================================== fire

bool FireEffect::load(ShaderManager& shaders)
{
    m_step  = fetch(shaders, "fire_step");
    m_shade = fetch(shaders, "fire_shade");
    if(!m_step || !m_shade) return false;

    if(!m_ping.resize({SIM_W, SIM_H}) || !m_pong.resize({SIM_W, SIM_H})) return false;

    for(sf::RenderTexture* t : {&m_ping, &m_pong})
    {
        t->setSmooth(false);
        t->clear(sf::Color::Black);
        t->display();
    }
    return true;
}

void FireEffect::update(float dt, const Pointer& pointer)
{
    // Hovering fans the flames; holding the button burns hotter.
    float const want = !pointer.inside ? 0.0f : (pointer.down ? 1.0f : 0.7f);
    m_fan += (want - m_fan) * std::min(1.0f, dt * 10.0f);

    sf::Vector2f const extent(size());
    if(pointer.inside) m_fan_pos = {pointer.pos.x / extent.x, pointer.pos.y / extent.y};

    m_step_accumulator = std::min(m_step_accumulator + dt, STEP * 4.0f);
    while(m_step_accumulator >= STEP)
    {
        m_step_accumulator -= STEP;
        m_seed  = std::fmod(m_seed + 0.6180339f, 97.0f);
        m_steps = std::fmod(m_steps + 1.0f, 7000.0f);   // whole rows, kept small for float precision

        // Read the current field, write the next one, swap.
        m_step->setUniform("resolution", sf::Glsl::Vec2{float(SIM_W), float(SIM_H)});
        m_step->setUniform("previous", current().getTexture());
        m_step->setUniform("seed", m_seed);
        m_step->setUniform("step", m_steps);
        m_step->setUniform("fan_pos", sf::Glsl::Vec2{m_fan_pos.x, m_fan_pos.y});
        m_step->setUniform("fan", m_fan);

        draw_quad(next(), {SIM_W, SIM_H}, m_step);
        next().display();
        m_ping_is_current = !m_ping_is_current;
    }
}

void FireEffect::render(sf::RenderTarget& target)
{
    m_shade->setUniform("resolution", to_vec2(size()));
    m_shade->setUniform("heat", current().getTexture());
    set_ramp(*m_shade, INK, CORAL, AMBER, TEXT);
    draw_quad(target, size(), m_shade);
}

// =================================================================== crt

bool CrtEffect::load(ShaderManager& shaders)
{
    m_shader = fetch(shaders, "crt");
    if(!m_shader || !m_screen.resize({INNER_W, INNER_H})) return false;
    m_screen.setSmooth(false);
    return true;
}

void CrtEffect::update(float dt, const Pointer& pointer)
{
    m_time        += dt;
    m_static       = std::max(0.0f, m_static - dt * 3.0f);
    m_label_timer  = std::max(0.0f, m_label_timer - dt);

    if(pointer.pressed)
    {
        m_channel     = (m_channel + 1) % 3;
        m_static      = 1.0f;
        m_label_timer = 1.6f;
    }

    sf::Vector2f const screen{float(INNER_W), float(INNER_H)};

    // Channel 1: the label bounces off the edges, changing colour on
    // every hit.
    sf::Vector2f const logo{30.0f, 13.0f};
    m_logo_pos += m_logo_vel * dt;
    if(m_logo_pos.x < 0.0f || m_logo_pos.x + logo.x > screen.x)
    {
        m_logo_vel.x = -m_logo_vel.x;
        m_logo_pos.x = std::clamp(m_logo_pos.x, 0.0f, screen.x - logo.x);
        ++m_logo_color;
    }
    if(m_logo_pos.y < 0.0f || m_logo_pos.y + logo.y > screen.y)
    {
        m_logo_vel.y = -m_logo_vel.y;
        m_logo_pos.y = std::clamp(m_logo_pos.y, 0.0f, screen.y - logo.y);
        ++m_logo_color;
    }

    // Channel 3: the paddles follow the ball, a little lazily.
    m_ball += m_ball_vel * dt;
    if(m_ball.y < 2.0f || m_ball.y > screen.y - 4.0f)
    {
        m_ball_vel.y = -m_ball_vel.y;
        m_ball.y = std::clamp(m_ball.y, 2.0f, screen.y - 4.0f);
    }
    if(m_ball.x < 8.0f || m_ball.x > screen.x - 10.0f)
    {
        m_ball_vel.x = -m_ball_vel.x;
        m_ball.x = std::clamp(m_ball.x, 8.0f, screen.x - 10.0f);
    }
    m_left_y  += (m_ball.y - m_left_y)  * std::min(1.0f, dt * 4.0f);
    m_right_y += (m_ball.y - m_right_y) * std::min(1.0f, dt * 3.2f);
}

void CrtEffect::draw_channel(sf::RenderTarget& target)
{
    sf::Vector2f const screen{float(INNER_W), float(INNER_H)};
    sf::VertexArray shapes{sf::PrimitiveType::Triangles};

    if(m_channel == 0)
    {
        target.clear(sf::Color(22, 24, 40));

        static const sf::Color colors[] = {AMBER, CORAL, SKY, SAGE, ROSE};
        sf::Color const color = colors[m_logo_color % std::size(colors)];

        sf::Vector2f const at = floor_vector(m_logo_pos);
        push_rect(shapes, {at, {30.0f, 13.0f}}, color);
        target.draw(shapes);

        sf::Text text(*font(), "sfml", TEXT_SIZE);
        text.setFillColor(INK);
        text.setPosition(at + sf::Vector2f{1.0f, -3.0f});
        target.draw(text);
    }
    else if(m_channel == 1)
    {
        // Colour bars, in the showcase palette.
        static const sf::Color bars[] = {TEXT, AMBER, SAGE, SKY, ROSE, CORAL, PANEL_HI};
        float const bar_w = screen.x / static_cast<float>(std::size(bars));
        for(std::size_t i = 0; i < std::size(bars); ++i)
            push_rect(shapes, {{std::floor(bar_w * float(i)), 0.0f},
                               {std::ceil(bar_w), screen.y * 0.68f}}, bars[i]);

        push_rect(shapes, {{0.0f, screen.y * 0.68f}, {screen.x, screen.y * 0.32f}}, INK);
        target.clear(INK);
        target.draw(shapes);

        int const seconds = static_cast<int>(m_time);
        char clock[16];
        std::snprintf(clock, sizeof(clock), "%02d:%02d", (seconds / 60) % 60, seconds % 60);
        sf::Text text(*font(), clock, TEXT_SIZE);
        text.setFillColor(TEXT);
        text.setPosition({screen.x / 2.0f - 16.0f, screen.y * 0.68f + 1.0f});
        target.draw(text);
    }
    else
    {
        target.clear(sf::Color(18, 26, 22));

        for(float y = 2.0f; y < screen.y; y += 8.0f)
            push_rect(shapes, {{screen.x / 2.0f - 1.0f, y}, {2.0f, 4.0f}}, SCOPE_DIM);

        push_rect(shapes, {floor_vector({4.0f, m_left_y - 7.0f}), {3.0f, 14.0f}}, SAGE);
        push_rect(shapes, {floor_vector({screen.x - 7.0f, m_right_y - 7.0f}), {3.0f, 14.0f}}, SAGE);
        push_rect(shapes, {floor_vector(m_ball), {3.0f, 3.0f}}, TEXT);
        target.draw(shapes);
    }

    // The channel number, for a moment after switching.
    if(m_label_timer > 0.0f)
    {
        sf::Text label(*font(), "CH " + std::to_string(m_channel + 1), TEXT_SIZE);
        label.setFillColor(SAGE);
        label.setPosition({screen.x - 30.0f, -2.0f});
        target.draw(label);
    }
}

void CrtEffect::render(sf::RenderTarget& target)
{
    m_screen.setView(m_screen.getDefaultView());
    draw_channel(m_screen);
    m_screen.display();

    m_shader->setUniform("resolution", to_vec2(size()));
    m_shader->setUniform("screen", m_screen.getTexture());
    m_shader->setUniform("time", m_time);
    m_shader->setUniform("static_amount", m_static);
    m_shader->setUniform("bezel", sf::Glsl::Vec4(BEZEL));
    draw_quad(target, size(), m_shader);
}

// ================================================================= scope

ScopeEffect::~ScopeEffect()
{
    // Stop both audio threads before the ring they write into goes away.
    if(m_mic) m_mic->stop();
    m_synth.stop();
}

bool ScopeEffect::load(ShaderManager& shaders)
{
    m_shader = fetch(shaders, "scope");
    if(!m_shader || !m_samples.resize({WIDTH, 1})) return false;

    m_pixels.assign(WIDTH * 4, 255);
    return true;
}

void ScopeEffect::build_controls(Frame& frame, sf::Vector2f origin)
{
    float const y = origin.y + 5.0f;
    float       x = origin.x + 6.0f;

    auto add = [&](const std::string& key, float width, const std::string& text)
    {
        auto button = make_button({width, 20.0f}, text);
        button->set_position({x, y}, true);
        frame.add_widget(key, button);
        x += width + 6.0f;
        return button;
    };

    struct Choice { Voice voice; const char* name; float width; };
    for(Choice c : {Choice{Voice::SQUARE, "square", 64.0f},
                    Choice{Voice::TRIANGLE, "triangle", 72.0f},
                    Choice{Voice::SAW, "saw", 44.0f}})
    {
        auto button = add(std::string("voice_") + c.name, c.width, c.name);
        Voice const voice = c.voice;
        button->set_callback([this, voice]() { select_voice(voice); });
        m_voice_buttons.emplace_back(voice, button);
    }

    x = origin.x + static_cast<float>(WIDTH) - 6.0f - 56.0f - 6.0f - 52.0f;
    m_mute_button = add("mute", 52.0f, "mute");
    m_mute_button->set_callback([this]() { set_muted(!m_muted); });

    m_mic_button = add("mic", 56.0f, "mic");
    m_mic_button->set_callback([this]() { set_mic(!m_using_mic); });

    refresh_buttons();
}

void ScopeEffect::refresh_buttons()
{
    for(auto& [voice, button] : m_voice_buttons)
    {
        bool const active = (voice == m_synth.get_voice()) && !m_using_mic;
        recolor(*button, active ? LINE : PANEL_HI);
        button->set_text_color(active ? TEXT : TEXT_DIM);
    }

    if(m_mute_button)
    {
        m_mute_button->add_text(m_muted ? "muted" : "mute", *font(), m_muted ? TEXT : TEXT_DIM);
        recolor(*m_mute_button, m_muted ? LINE : PANEL_HI);
    }

    if(m_mic_button)
    {
        bool const available = sf::SoundRecorder::isAvailable();
        m_mic_button->set_enabled(available);
        m_mic_button->add_text(available ? "mic" : "no mic", *font(),
                               m_using_mic ? TEXT : TEXT_DIM);
        recolor(*m_mic_button, m_using_mic ? LINE : PANEL_HI);
    }
}

void ScopeEffect::select_voice(Voice voice)
{
    m_synth.set_voice(voice);
    if(m_using_mic) set_mic(false);
    refresh_buttons();
}

void ScopeEffect::set_muted(bool muted)
{
    m_muted = muted;
    m_synth.setVolume(m_muted ? 0.0f : 55.0f);
    refresh_buttons();
}

void ScopeEffect::set_mic(bool on)
{
    if(on && sf::SoundRecorder::isAvailable())
    {
        if(!m_mic) m_mic = std::make_unique<MicTap>(m_ring);

        m_synth.stop();
        m_ring.clear();
        m_using_mic = m_mic->start(ChipSynth::SAMPLE_RATE);
        if(!m_using_mic && m_open) m_synth.play();
    }
    else
    {
        if(m_mic) m_mic->stop();
        m_using_mic = false;
        m_ring.clear();
        if(m_open) m_synth.play();
    }
    refresh_buttons();
}

void ScopeEffect::on_open()
{
    // Sound plays only while this window is open.
    m_open = true;
    m_ring.clear();
    m_last_total = m_ring.total();
    m_starved    = 0.0f;
    if(!m_using_mic) m_synth.play();
}

void ScopeEffect::on_close()
{
    m_open = false;
    m_synth.stop();
    if(m_mic) m_mic->stop();
    m_using_mic = false;
}

void ScopeEffect::update(float dt, const Pointer& pointer)
{
    (void)pointer;

    // If nothing is pulling the synth, no audio device, say, render
    // the same music here so the trace still has something to show.
    std::uint64_t const total = m_ring.total();
    m_starved = (total == m_last_total) ? m_starved + dt : 0.0f;
    m_last_total = total;

    if(!m_using_mic && m_starved > 0.25f)
    {
        std::size_t const count = static_cast<std::size_t>(dt * ChipSynth::SAMPLE_RATE);
        std::vector<std::int16_t> local(count);
        ChipSynth::render(local.data(), local.size(), m_fallback_position, m_synth.get_voice());
        m_fallback_position += count;
        m_ring.push(local.data(), local.size());
        m_last_total = m_ring.total();
    }

    // Trigger on a rising zero crossing, the way a real scope does, so a
    // steady note stands still instead of scrolling.
    m_ring.latest(m_window, 2048);
    std::size_t start = m_window.size() - WIDTH;
    for(std::size_t i = m_window.size() - WIDTH - 1; i > 1; --i)
    {
        if(m_window[i - 1] < 0.0f && m_window[i] >= 0.0f)
        {
            start = i;
            break;
        }
    }

    for(unsigned x = 0; x < WIDTH; ++x)
    {
        float const s = m_window[start + x];
        m_pixels[x * 4] = static_cast<std::uint8_t>(std::clamp(s * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f);
    }
    m_samples.update(m_pixels.data());
}

void ScopeEffect::render(sf::RenderTarget& target)
{
    m_shader->setUniform("resolution", to_vec2(size()));
    m_shader->setUniform("samples", m_samples);
    m_shader->setUniform("level", 1.0f);
    set_ramp(*m_shader, INK, SCOPE_DIM, SAGE, TEXT);
    draw_quad(target, size(), m_shader);
}

}
