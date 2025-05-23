/**
 * Button.hpp
 *
 * Written by James Bouchat
 */

#ifndef BUTTON_HPP
#define BUTTON_HPP

#include "TextWidget.hpp"

class Button : public TextWidget
{
private:
    mutable sf::RenderTexture m_render_texture;
    mutable sf::Sprite m_sprite;
    sf::RectangleShape m_background;
    sf::RectangleShape m_outline;
    sf::Color m_color;
    sf::Color m_dark_color;
    sf::Color m_light_color;
    sf::Color m_outline_color;
    sf::Vector2f m_pos;

    void draw(sf::RenderTarget &target, sf::RenderStates states) const override;
    void create_color_variants();
public:
    Button(sf::Vector2f button_shape, sf::Color color);

    void handle_event(const sf::RenderWindow& window) override;

    void set_position(sf::Vector2f pos);
    void set_outline(sf::Color color, int thickness);
};

#endif
