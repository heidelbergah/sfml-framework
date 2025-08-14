/**
 * Slider.hpp
 *
 * Written by James Bouchat
 */

#ifndef SLIDER_HPP
#define SLIDER_HPP

#include "Widget.hpp"

class Slider : public Widget
{
private:
    sf::RectangleShape m_thumb;
    sf::Color m_thumb_color;
    sf::Color m_thumb_outline_color;
    sf::RectangleShape m_body;
    sf::Color m_body_color;
    sf::Color m_body_outline_color;
    bool m_being_dragged = false;

    std::unique_ptr<sf::Texture> m_thumb_texture = nullptr;
    std::unique_ptr<sf::Texture> m_body_texture = nullptr;

    void draw(sf::RenderTarget &target, sf::RenderStates states) const override;

public:
    Slider(sf::Vector2f size, sf::Color color);

    void handle_event(const sf::RenderWindow& window, std::optional<sf::Vector2f> local_mouse_pos) override;

    float get_percentage();

    void set_body_color(sf::Color color);
    void set_thumb_color(sf::Color color);

    void set_thumb_texture(sf::Texture texture, bool reset_rect=false);
    void set_body_texture(sf::Texture texture, bool reset_rect=false);
};

#endif
