/**
 * ShakeScene.hpp  -  Showcase
 *
 * A small night skyline under a Camera, and a panel for shaking it.
 *
 * Impulses (tap, hit, boom) add trauma that decays on its own; holding
 * rumble sets a sustained floor that impulses spike above and settle
 * back to. A Gauge and a scrolling graph show the trauma as it happens.
 *
 * The panel can be drawn through the camera's UI view, so it jolts with
 * the world, or through the default view, so it stays perfectly still.
 */

#ifndef SHOWCASE_SHAKE_SCENE_HPP
#define SHOWCASE_SHAKE_SCENE_HPP

#include "../../include/framework/Camera.hpp"
#include "../../include/framework/ParticleSystemManager.hpp"
#include "../../include/gui/Frame.hpp"
#include "../../include/gui/Gauge.hpp"
#include "../../include/gui/Slider.hpp"

#include "DemoScene.hpp"

#include <deque>

namespace showcase
{

class ShakeScene : public DemoScene
{
private:
    struct Roof
    {
        float left;
        float right;
        float y;
    };

    Camera                m_camera;
    ParticleSystemManager m_particles;
    std::shared_ptr<ParticleSystem> m_sparks;

    sf::VertexArray   m_world{sf::PrimitiveType::Triangles};
    sf::CircleShape   m_moon;
    std::vector<Roof> m_roofs;

    std::shared_ptr<Frame>   m_panel;
    std::shared_ptr<Gauge>   m_gauge;
    std::shared_ptr<TextBox> m_gauge_value;
    std::shared_ptr<Button>  m_rumble;
    std::shared_ptr<Slider>  m_rumble_strength;
    std::shared_ptr<TextBox> m_rumble_label;
    std::shared_ptr<Button>  m_hud_toggle;
    std::shared_ptr<Button>  m_roll_toggle;

    std::deque<float> m_trauma_history;
    std::deque<float> m_floor_history;
    sf::VertexArray   m_graph{sf::PrimitiveType::Triangles};

    bool m_hud_shakes{true};
    bool m_roll{false};
    bool m_space_held{false};

    void build_world();
    void build_panel();
    void impulse(float amount, int sparks);
    void set_hud_shakes(bool on);
    void set_roll(bool on);
    void rebuild_graph();

    [[nodiscard]] float rumble_amount() const;

public:
    ShakeScene();

    void on_enter() override;
    void handle_event(const sf::Event& event, sf::Vector2f mouse_pos) override;
    void update(sf::Time delta_time) override;
    void render(sf::RenderTarget& target) override;
};

}

#endif
