/**
 * ParticleScene.hpp  -  Showcase
 *
 * Hold the left mouse button to paint with particles. Right click
 * anywhere to open a Frame of Sliders that tunes the emitter live.
 *
 * Particles are drawn with additive blending, so they only get bright
 * enough to bloom where they pile up.
 */

#ifndef SHOWCASE_PARTICLE_SCENE_HPP
#define SHOWCASE_PARTICLE_SCENE_HPP

#include "../../include/framework/ParticleSystemManager.hpp"
#include "../../include/gui/Frame.hpp"
#include "../../include/gui/Slider.hpp"

#include "DemoScene.hpp"

namespace showcase
{

class ParticleScene : public DemoScene
{
private:
    struct Setting
    {
        std::string              name;
        float                    min;
        float                    max;
        const char*              format;
        std::shared_ptr<Slider>  slider;
        std::shared_ptr<TextBox> value;

        [[nodiscard]] float get() const { return slider->get_value(min, max); }
    };

    struct Palette
    {
        sf::Color a;
        sf::Color b;
    };

    ParticleSystemManager m_particles;
    std::shared_ptr<ParticleSystem> m_brush;

    std::shared_ptr<Frame>   m_panel;
    std::vector<Setting>     m_settings;
    std::vector<std::shared_ptr<Button>> m_swatches;
    std::vector<Palette>     m_palettes;
    std::shared_ptr<Button>  m_fade_button;
    std::shared_ptr<TextBox> m_count;

    std::size_t  m_palette{0};
    bool         m_fade{true};
    bool         m_painting{false};
    bool         m_demo_mode{true};     // a fountain plays until you take over
    float        m_emit_accumulator{0.0f};
    float        m_time{0.0f};
    sf::Vector2f m_mouse;

    void build_panel();
    void open_panel(sf::Vector2f at);
    void close_panel();
    void select_palette(std::size_t index);
    void set_fade(bool fade);
    void apply_settings();

    [[nodiscard]] float setting(std::size_t index) const;
    [[nodiscard]] bool  over_panel(sf::Vector2f point) const;

public:
    ParticleScene();

    void on_enter() override;
    void on_exit() override;
    void handle_event(const sf::Event& event, sf::Vector2f mouse_pos) override;
    void update(sf::Time delta_time) override;
    void render(sf::RenderTarget& target) override;
};

}

#endif
