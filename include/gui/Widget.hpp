/**
 * Widget.hpp
 *
 * Written by James Bouchat
 */

#ifndef WIDGET_HPP
#define WIDGET_HPP

#include "../Includes.hpp"

enum Alignment
{
    CENTER = 1,
    RIGHT = 2,
    LEFT = 3
};

class Widget : public sf::Drawable
{
private:
    void virtual draw(sf::RenderTarget &target, sf::RenderStates states) const = 0;

protected:
    mutable sf::RenderTexture m_render_texture;
    mutable sf::Sprite m_sprite;
    sf::RectangleShape m_background;
    sf::RectangleShape m_outline;
    sf::Color m_color;
    sf::Color m_outline_color;
    sf::Vector2f m_pos;
    sf::Vector2f m_size;

public:
    Widget(sf::Vector2f size);
    virtual void update(sf::Time delta_time) {};
    virtual void handle_event(const sf::RenderWindow& window, std::optional<sf::Vector2f> local_mouse_pos = std::nullopt) {};

    void set_position(sf::Vector2f pos);
    void set_outline(sf::Color color, int thickness);

    void move(sf::Vector2f move_vector);

    sf::Vector2f get_position();
    sf::Vector2f get_size();
    
    sf::FloatRect get_global_bounds();
};

#endif
