/**
 * DemoScene.hpp  -  Showcase
 *
 * Common frame for each demo: a title, a one-line hint along the bottom,
 * and escape to return to the hub. Derived scenes add their own widgets
 * to m_widgets and call the base handlers.
 */

#ifndef SHOWCASE_DEMO_SCENE_HPP
#define SHOWCASE_DEMO_SCENE_HPP

#include "../../include/scenes/SceneManager.hpp"
#include "../../include/gui/WidgetManager.hpp"

#include "Theme.hpp"

namespace showcase
{

class DemoScene : public Scene
{
protected:
    WidgetManager m_widgets;

    std::shared_ptr<TextBox> m_title;
    std::shared_ptr<TextBox> m_hint;

    void back_to_hub();

public:
    DemoScene(const std::string& title, const std::string& hint);

    void on_enter() override;
    void begin_frame() override;
    void handle_event(const sf::Event& event, sf::Vector2f mouse_pos) override;
    void update(sf::Time delta_time) override;

    [[nodiscard]] sf::Color get_clear_color() const override { return INK; }
};

}

#endif
