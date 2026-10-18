/**
 * Effects.hpp  -  Showcase
 *
 * The five shader windows. Each shows a different way of using a
 * fragment shader:
 *
 *   PlasmaEffect   procedural: no texture, everything from time and position
 *   RippleEffect   CPU simulation, shaded on the GPU from a height texture
 *   FireEffect     GPU feedback: the shader reads its own previous frame
 *   CrtEffect      post-processing a scene drawn into a render texture
 *   ScopeEffect    audio uploaded as a texture every frame and traced
 */

#ifndef SHOWCASE_EFFECTS_HPP
#define SHOWCASE_EFFECTS_HPP

#include "Audio.hpp"
#include "ShaderView.hpp"
#include "Theme.hpp"

namespace showcase
{

/*
 * Every effect's one-line description, shown while its window is hovered.
 */
class HintedEffect : public Effect
{
public:
    [[nodiscard]] virtual std::string hint() const = 0;
};

// ---------------------------------------------------------------- plasma

class PlasmaEffect : public HintedEffect
{
private:
    sf::Shader*  m_shader{nullptr};
    float        m_time{0.0f};
    sf::Vector2f m_center{0.5f, 0.5f};
    std::size_t  m_ramp{0};

public:
    [[nodiscard]] std::string  title() const override { return "plasma"; }
    [[nodiscard]] std::string  hint() const override  { return "plasma   move to steer the swirl, click to recolour"; }
    [[nodiscard]] sf::Vector2u size() const override  { return {192, 128}; }

    bool load(ShaderManager& shaders) override;
    void update(float dt, const Pointer& pointer) override;
    void render(sf::RenderTarget& target) override;
};

// ---------------------------------------------------------------- ripple

class RippleEffect : public HintedEffect
{
private:
    sf::Shader*                m_shader{nullptr};
    sf::Texture                m_heights;
    std::vector<float>         m_current;
    std::vector<float>         m_previous;
    std::vector<std::uint8_t>  m_pixels;
    float                      m_step_accumulator{0.0f};
    float                      m_rain_timer{0.0f};

    void drop(sf::Vector2f at, float radius, float strength);
    void step();

public:
    [[nodiscard]] std::string  title() const override { return "ripple"; }
    [[nodiscard]] std::string  hint() const override  { return "ripple   click or drag to disturb the water"; }
    [[nodiscard]] sf::Vector2u size() const override  { return {192, 128}; }

    bool load(ShaderManager& shaders) override;
    void update(float dt, const Pointer& pointer) override;
    void render(sf::RenderTarget& target) override;
};

// ------------------------------------------------------------------ fire

class FireEffect : public HintedEffect
{
private:
    static constexpr unsigned SIM_W = 96;
    static constexpr unsigned SIM_H = 64;

    sf::Shader*       m_step{nullptr};
    sf::Shader*       m_shade{nullptr};
    sf::RenderTexture m_ping;
    sf::RenderTexture m_pong;
    bool              m_ping_is_current{true};
    float             m_step_accumulator{0.0f};
    float             m_seed{0.0f};
    float             m_steps{0.0f};
    float             m_fan{0.0f};
    sf::Vector2f      m_fan_pos;

    sf::RenderTexture& current() { return m_ping_is_current ? m_ping : m_pong; }
    sf::RenderTexture& next()    { return m_ping_is_current ? m_pong : m_ping; }

public:
    [[nodiscard]] std::string  title() const override { return "fire"; }
    [[nodiscard]] std::string  hint() const override  { return "fire   hover to feed the flames, hold to burn hotter"; }
    [[nodiscard]] sf::Vector2u size() const override  { return {192, 128}; }

    bool load(ShaderManager& shaders) override;
    void update(float dt, const Pointer& pointer) override;
    void render(sf::RenderTarget& target) override;
};

// ------------------------------------------------------------------- crt

class CrtEffect : public HintedEffect
{
private:
    static constexpr unsigned INNER_W = 96;
    static constexpr unsigned INNER_H = 72;

    sf::Shader*       m_shader{nullptr};
    sf::RenderTexture m_screen;
    float             m_time{0.0f};
    float             m_static{1.0f};
    float             m_label_timer{2.0f};
    int               m_channel{0};

    // Channel 1: a bouncing label.
    sf::Vector2f m_logo_pos{20.0f, 20.0f};
    sf::Vector2f m_logo_vel{19.0f, 13.0f};
    std::size_t  m_logo_color{0};

    // Channel 3: two paddles playing themselves.
    sf::Vector2f m_ball{48.0f, 36.0f};
    sf::Vector2f m_ball_vel{38.0f, 23.0f};
    float        m_left_y{36.0f};
    float        m_right_y{36.0f};

    void draw_channel(sf::RenderTarget& target);

public:
    [[nodiscard]] std::string  title() const override { return "crt"; }
    [[nodiscard]] std::string  hint() const override  { return "crt   click to change the channel"; }
    [[nodiscard]] sf::Vector2u size() const override  { return {192, 144}; }

    bool load(ShaderManager& shaders) override;
    void update(float dt, const Pointer& pointer) override;
    void render(sf::RenderTarget& target) override;
};

// ----------------------------------------------------------------- scope

class ScopeEffect : public HintedEffect
{
private:
    static constexpr unsigned WIDTH  = 320;
    static constexpr unsigned HEIGHT = 96;

    sf::Shader*  m_shader{nullptr};
    sf::Texture  m_samples;
    std::vector<std::uint8_t> m_pixels;
    std::vector<float>        m_window;

    SampleRing               m_ring;
    ChipSynth                m_synth{m_ring};
    std::unique_ptr<MicTap>  m_mic;

    bool          m_open{false};
    bool          m_muted{false};
    bool          m_using_mic{false};
    std::uint64_t m_last_total{0};
    float         m_starved{0.0f};       // seconds with no samples arriving
    std::uint64_t m_fallback_position{0};

    std::vector<std::pair<Voice, std::shared_ptr<Button>>> m_voice_buttons;
    std::shared_ptr<Button> m_mute_button;
    std::shared_ptr<Button> m_mic_button;

    void select_voice(Voice voice);
    void set_muted(bool muted);
    void set_mic(bool on);
    void refresh_buttons();

public:
    ~ScopeEffect() override;

    [[nodiscard]] std::string  title() const override { return "scope"; }
    [[nodiscard]] std::string  hint() const override  { return "scope   pick a voice, or switch to the microphone"; }
    [[nodiscard]] sf::Vector2u size() const override  { return {WIDTH, HEIGHT}; }

    bool  load(ShaderManager& shaders) override;
    void  update(float dt, const Pointer& pointer) override;
    void  render(sf::RenderTarget& target) override;
    float controls_height() const override { return 30.0f; }
    void  build_controls(Frame& frame, sf::Vector2f origin) override;
    void  on_open() override;
    void  on_close() override;
};

}

#endif
