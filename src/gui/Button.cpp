/**
 * Button.cpp
 *
 * Written by James Bouchat
 */

#include "../../include/gui/Button.hpp"

void Button::draw(sf::RenderTarget &target, sf::RenderStates states) const
{
    // Render everything to a render texture, then DRAW that render texture
    m_render_texture.clear(sf::Color::Transparent);
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
    int pressing_factor = 120;
    int hovering_factor = 80;

    m_dark_color.r = std::max(0, m_color.r - pressing_factor);
    m_dark_color.g = std::max(0, m_color.g - pressing_factor);
    m_dark_color.b = std::max(0, m_color.b - pressing_factor);
    m_dark_color.a = 255;

    m_light_color.r = std::max(0, m_color.r - hovering_factor);
    m_light_color.g = std::max(0, m_color.g - hovering_factor);
    m_light_color.b = std::max(0, m_color.b - hovering_factor);
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

void Button::update(sf::Time delta_time)
{
    m_sprite.setPosition(m_pos);
}

void Button::handle_event(const sf::RenderWindow& window, std::optional<sf::Vector2f> local_mouse_pos)
{
    sf::Vector2i pixel_pos = sf::Mouse::getPosition(window);
    
    sf::Vector2f world_pos = window.mapPixelToCoords(pixel_pos);
    world_pos.x /= m_mouse_scale;
    world_pos.y /= m_mouse_scale;

    if(local_mouse_pos.has_value())
    {
        world_pos = local_mouse_pos.value();
    }

    m_background.setFillColor(m_color);

    m_hovered = false;
    m_pressed = false;
    if (m_sprite.getGlobalBounds().contains(world_pos))
    {
        // If hovering, lighten the background
        m_hovered = true;
        m_background.setFillColor(m_light_color);
        if(sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
        {
            m_pressed = true;
            // If clicking, darken the background
            m_background.setFillColor(m_dark_color);
            if(m_callback)
                m_callback.value()();
        }
    }
}

bool Button::is_hovered()
{
    return m_hovered;
}

bool Button::is_pressed()
{
    return m_pressed;
}

