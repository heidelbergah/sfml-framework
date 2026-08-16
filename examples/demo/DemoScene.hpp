/**
 * DemoScene.hpp
 *
 * The feature showcase that used to live inside Game::initialize_objects
 * and Game::process_events, now an ordinary Scene.
 *
 * Everything the framework provides is exercised here: widgets, the
 * particle system, camera shake, bloom, the event bus and tweening.
 * Delete this directory once you have your own scenes.
 *
 * Controls:
 *   1 / 2            move the button and emitters
 *   WASD             move the camera
 *   Space            machine-gun burst, light shake
 *   Enter            toggle the frame's taskbar
 *   Left click       button confetti
 *   Right click      explosion at the cursor, heavy shake
 *   B                toggle bloom
 *   U                toggle whether the HUD shakes with the world
 *   P                toggle pause
 *   Escape           quit
 *   Slider           bloom intensity
 */

#ifndef DEMO_SCENE_HPP
#define DEMO_SCENE_HPP

#include "../../include/scenes/Scene.hpp"
#include "../../include/scenes/SceneManager.hpp"

#include "../../include/framework/Camera.hpp"
#include "../../include/framework/ParticleSystemManager.hpp"

#include "../../include/gui/Button.hpp"
#include "../../include/gui/Frame.hpp"
#include "../../include/gui/Gauge.hpp"
#include "../../include/gui/Slider.hpp"
#include "../../include/gui/TextBox.hpp"
#include "../../include/gui/WidgetManager.hpp"

class DemoScene : public Scene
{
private:
    Camera                m_camera;
    WidgetManager         m_widgets;
    ParticleSystemManager m_particles;

    // Cached rather than looked up by string every time. Each
    // get_widget() is a hash lookup plus a dynamic_pointer_cast, and
    // the old code did five in a row on the same widget.
    std::shared_ptr<Button>  m_button;
    std::shared_ptr<Frame>   m_frame;
    std::shared_ptr<TextBox> m_info;
    std::shared_ptr<Slider>  m_slider;
    std::shared_ptr<Gauge>   m_gauge;
    std::shared_ptr<TextBox> m_bloom_label;

    Subscription m_pause_subscription;

    sf::Vector2f m_mouse_world;
    bool         m_bloom_on{true};
    bool         m_hud_shakes{true};   // press U to toggle
    float        m_gauge_phase{0.0f};

    void build_widgets();
    void build_particles();
    void spawn_confetti(sf::Vector2f where);
    void apply_bloom_intensity();

public:
    DemoScene();

    void on_enter() override;
    void on_exit() override;

    void handle_event(const sf::Event& event, sf::Vector2f mouse_pos) override;
    void update(sf::Time delta_time) override;
    void render(sf::RenderTarget& target) override;

    [[nodiscard]] sf::Color get_clear_color() const override;
};

#endif
