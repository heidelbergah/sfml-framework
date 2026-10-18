/**
 * HubScene.hpp  -  Showcase
 *
 * The menu. One row per demo; each opens with a fade and escape brings
 * you back here.
 */

#ifndef SHOWCASE_HUB_SCENE_HPP
#define SHOWCASE_HUB_SCENE_HPP

#include "../../include/scenes/SceneManager.hpp"
#include "../../include/gui/WidgetManager.hpp"

#include "Theme.hpp"

namespace showcase
{

class HubScene : public Scene
{
private:
    struct Entry
    {
        std::string              scene_key;
        sf::Color                accent;
        std::shared_ptr<Button>  button;
        std::shared_ptr<TextBox> description;
    };

    WidgetManager      m_widgets;
    std::vector<Entry> m_entries;

    std::shared_ptr<TextBox> m_title;
    std::shared_ptr<TextBox> m_subtitle;
    std::shared_ptr<TextBox> m_footer;

    sf::VertexArray m_markers{sf::PrimitiveType::Triangles};

    void open(const std::string& scene_key);

public:
    HubScene();

    void on_enter() override;
    void begin_frame() override;
    void handle_event(const sf::Event& event, sf::Vector2f mouse_pos) override;
    void update(sf::Time delta_time) override;
    void render(sf::RenderTarget& target) override;

    [[nodiscard]] sf::Color get_clear_color() const override { return INK; }
};

}

#endif
