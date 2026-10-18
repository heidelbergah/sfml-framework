/**
 * ShaderScene.hpp  -  Showcase
 *
 * A small desktop of shader windows. Each window is a Frame with a
 * ShaderView inside, running one effect; the dock along the bottom opens
 * them, and they can be dragged, raised and closed.
 *
 * Input goes only to the window under the cursor, the front-most one,
 * found with WidgetManager::widget_at, so each effect behaves like a
 * self-contained program. A window being dragged keeps the mouse until
 * the button is released, even if the cursor slips off it.
 */

#ifndef SHOWCASE_SHADER_SCENE_HPP
#define SHOWCASE_SHADER_SCENE_HPP

#include "DemoScene.hpp"
#include "Effects.hpp"

#include <map>

namespace showcase
{

class ShaderScene : public DemoScene
{
private:
    struct Window
    {
        std::unique_ptr<HintedEffect> effect;
        std::shared_ptr<Frame>        frame;
        std::shared_ptr<ShaderView>   view;
        std::shared_ptr<Button>       dock_button;
        sf::Vector2f                  home;
    };

    // Key order is dock order.
    std::vector<std::string>      m_order;
    std::map<std::string, Window> m_windows;

    WidgetManager          m_desktop;     // open windows, front-most last
    std::shared_ptr<Frame> m_captured;    // owns the mouse while a button is held
    std::shared_ptr<Frame> m_hovered;

    std::string m_default_hint;

    void add_window(const std::string& key, std::unique_ptr<HintedEffect> effect,
                    sf::Vector2f home);
    void open(const std::string& key);
    void close(const std::string& key);
    void refresh_dock();
    void set_hovered(const std::shared_ptr<Frame>& frame);

    [[nodiscard]] Window* find(const std::shared_ptr<Widget>& frame);

public:
    ShaderScene();

    void on_enter() override;
    void on_exit() override;
    void begin_frame() override;
    void handle_event(const sf::Event& event, sf::Vector2f mouse_pos) override;
    void update(sf::Time delta_time) override;
    void render(sf::RenderTarget& target) override;
};

}

#endif
