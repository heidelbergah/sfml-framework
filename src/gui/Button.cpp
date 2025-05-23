/**
 * Button.cpp
 *
 * Written by James Bouchat
 */

#include "../../include/gui/Button.hpp"

void Button::draw(sf::RenderTarget &target, sf::RenderStates states) const
{
    // Render everything to a render texture, then DRAW that render texture
    m_render_texture.clear();
    m_render_texture.draw(m_outline);
    m_render_texture.draw(m_background);
    if(m_text) // If m_text is not null
        m_render_texture.draw(*m_text);
    m_render_texture.display();
    m_sprite.setTexture(m_render_texture.getTexture());
    target.draw(m_sprite, states);
}

void Button::create_color_variants()
{
    int darkening_factor = 80;
    int lightening_factor = 80;

    m_dark_color.r = std::max(0, m_color.r - darkening_factor);
    m_dark_color.g = std::max(0, m_color.g - darkening_factor);
    m_dark_color.b = std::max(0, m_color.b - darkening_factor);
    m_dark_color.a = 255;

    m_light_color.r = std::min(255, m_color.r + lightening_factor);
    m_light_color.g = std::min(255, m_color.g + lightening_factor);
    m_light_color.b = std::min(255, m_color.b + lightening_factor);
    m_light_color.a = 255; 
}

Button::Button(sf::Vector2f size, sf::Color color) :
    TextWidget(size)
{
    m_color = color;
    m_size = size;

    create_color_variants();

    m_sprite.setPosition(sf::Vector2f(0, 0));
    m_background.setPosition(sf::Vector2f(0, 0));
    m_background.setSize(m_size);
    m_background.setFillColor(m_color);
}

void Button::handle_event(const sf::RenderWindow& window)
{
    sf::Vector2i pixelPos = sf::Mouse::getPosition(window);
    sf::Vector2f worldPos = window.mapPixelToCoords(pixelPos);

    m_background.setFillColor(m_color);

    if (m_sprite.getGlobalBounds().contains(worldPos))
    {
        // If hovering, lighten the background
        m_background.setFillColor(m_light_color);
        if(sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
        {
            // If clicking, darken the background
            m_background.setFillColor(m_dark_color);
        }
    }
}

