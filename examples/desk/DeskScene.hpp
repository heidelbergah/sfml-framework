/**
 * DeskScene.hpp  -  Desk
 *
 * The forward base's table: two editions of the local paper, a letter
 * from home, a photograph and a note, all under a desk lamp.
 *
 * Press on a paper and it comes to the top of the pile. Move while
 * holding and it lifts off the table and follows; let go without moving
 * and it counts as a click instead, which turns pages.
 */

#ifndef DESK_SCENE_HPP
#define DESK_SCENE_HPP

#include "../../include/scenes/SceneManager.hpp"

#include "DeskItem.hpp"

namespace desk
{

class DeskScene : public Scene
{
private:
    std::vector<std::unique_ptr<DeskItem>> m_items;   // bottom of the pile first

    sf::VertexArray m_desk{sf::PrimitiveType::Triangles};
    sf::Shader*     m_paper{nullptr};
    sf::Shader*     m_lamp{nullptr};

    DeskItem*    m_held{nullptr};
    bool         m_dragging{false};
    sf::Vector2f m_mouse;
    sf::Vector2f m_press;
    sf::Vector2f m_grab;            // cursor relative to the item's centre
    sf::Vector2f m_last_center;

    void build_desk();
    void raise(DeskItem* item);

    [[nodiscard]] DeskItem* item_at(sf::Vector2f point);

public:
    DeskScene();

    void on_enter() override;
    void handle_event(const sf::Event& event, sf::Vector2f mouse) override;
    void update(sf::Time delta_time) override;
    void render(sf::RenderTarget& target) override;

    [[nodiscard]] sf::Color get_clear_color() const override { return sf::Color(40, 26, 18); }
};

}

#endif
