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
    sf::Color m_dark_color;
    sf::Color m_light_color;
    sf::Color m_outline_color;

    void draw(sf::RenderTarget &target, sf::RenderStates states) const override;
    void create_color_variants();
public:
    Button(sf::Vector2f button_shape, sf::Color color);

    void handle_event(const sf::RenderWindow& window) override;
};

#endif
