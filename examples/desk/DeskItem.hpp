/**
 * DeskItem.hpp  -  Desk
 *
 * A piece of paper lying on the desk: a newspaper, a letter, a photo.
 *
 * Each page is an ordinary Frame full of widgets. The item renders the
 * current page into a texture of its own and draws that as a physical
 * object: it lifts and casts a longer shadow while held, leans into the
 * direction it is dragged, and squashes edge-on to turn a page.
 *
 * Rendering through a texture is what makes that possible -- a Frame
 * drawn directly cannot rotate, squash or be clipped -- and it is the
 * part most worth moving into the framework later.
 */

#ifndef DESK_ITEM_HPP
#define DESK_ITEM_HPP

#include "../../include/gui/Frame.hpp"

namespace desk
{

class DeskItem
{
public:
    enum class Turn
    {
        CORNERS,     // click a folded corner to go forward or back
        WHOLE        // click anywhere to turn over (a photo)
    };

private:
    std::vector<std::shared_ptr<Frame>> m_pages;
    sf::Vector2f      m_size;
    sf::RenderTexture m_target;
    Turn              m_turn{Turn::CORNERS};
    float             m_age{0.0f};

    std::size_t m_page{0};
    std::size_t m_next_page{0};
    bool        m_turning{false};
    bool        m_swapped{false};
    Interpolated<float> m_flip;      // 0 to 1 over one page turn
    Interpolated<float> m_lift;      // 0 resting, 1 held

    sf::Vector2f m_center;
    float        m_angle{0.0f};      // degrees
    float        m_lean{0.0f};       // where the angle is heading
    int          m_hover_corner{0};  // -1 back, 1 forward, 0 none

    [[nodiscard]] float squash() const;

public:
    DeskItem(sf::Vector2f size, std::vector<std::shared_ptr<Frame>> pages,
             sf::Vector2f center, Turn turn = Turn::CORNERS);

    /*
     * 0 is fresh paper; 1 is old, yellowed and faded. Applied by the
     * paper shader when the item is drawn.
     */
    void set_age(float age) { m_age = age; }

    void update(float dt);
    void draw(sf::RenderTarget& target, sf::Shader* paper) const;

    // Hit testing and coordinates, through the item's real transform,
    // so a tilted page still catches the mouse exactly where it is drawn.
    [[nodiscard]] sf::Transform transform() const;
    [[nodiscard]] sf::Vector2f  to_local(sf::Vector2f point) const;
    [[nodiscard]] bool          contains(sf::Vector2f point) const;

    /*
     * -1 over the back corner, 1 over the forward corner, 0 elsewhere.
     */
    [[nodiscard]] int corner_at(sf::Vector2f local) const;

    void set_hover_corner(int corner) { m_hover_corner = corner; }

    void pick_up();
    void put_down();
    void drag_to(sf::Vector2f center, sf::Vector2f velocity);

    /*
     * A click that was not a drag: turns a page if it landed in the
     * right place.
     */
    void click(sf::Vector2f local);
    void turn(int direction);

    [[nodiscard]] sf::Vector2f get_center() const { return m_center; }
    [[nodiscard]] sf::Vector2f get_size() const   { return m_size; }
};

}

#endif
