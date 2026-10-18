/**
 * MenuScene.hpp  -  Ironvale Arena
 *
 * Title screen. Shows widget tweening and callbacks, an ambient
 * particle field, and event-driven scene switching.
 */

#ifndef ARENA_MENU_SCENE_HPP
#define ARENA_MENU_SCENE_HPP

#include "../../include/scenes/Scene.hpp"
#include "../../include/scenes/SceneManager.hpp"

#include "../../include/framework/Camera.hpp"
#include "../../include/framework/ParticleSystemManager.hpp"

#include "../../include/gui/Button.hpp"
#include "../../include/gui/TextBox.hpp"
#include "../../include/gui/WidgetManager.hpp"

#include "RunState.hpp"

namespace arena
{

class MenuScene : public Scene
{
private:
    Camera                m_camera;
    WidgetManager         m_widgets;
    ParticleSystemManager m_particles;

    std::shared_ptr<TextBox> m_title;
    std::shared_ptr<TextBox> m_subtitle;
    std::shared_ptr<TextBox> m_controls;
    std::shared_ptr<TextBox> m_best;
    std::shared_ptr<Button>  m_play;
    std::shared_ptr<Button>  m_quit;

    float m_ember_timer{0.0f};

    void build_widgets();

public:
    MenuScene();

    void on_enter() override;

    void begin_frame() override;
    void handle_event(const sf::Event& event, sf::Vector2f mouse_pos) override;
    void update(sf::Time delta_time) override;
    void render(sf::RenderTarget& target) override;

    [[nodiscard]] sf::Color get_clear_color() const override;
};

}   // namespace arena

#endif
