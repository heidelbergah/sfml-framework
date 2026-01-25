/**
 * Slider.cpp
 *
 * Written by James Bouchat
 */

#include "../../include/gui/Slider.hpp"

void Slider::draw(sf::RenderTarget &target, sf::RenderStates states) const
{
    m_render_texture.clear(m_color);
    m_render_texture.draw(m_outline);
    m_render_texture.draw(m_background);
    m_render_texture.draw(m_body);
    m_render_texture.draw(m_thumb);
    m_render_texture.display();
    m_sprite.setTexture(m_render_texture.getTexture());
    target.draw(m_sprite, states);
}

Slider::Slider(sf::Vector2f size, sf::Color color) :
    Widget(size)
{
    m_color = sf::Color::Transparent;
    m_size = size;
    m_thumb_color = color;
    m_body_color = sf::Color::White;
    
    m_sprite.setPosition(sf::Vector2f(0, 0));
    m_background.setPosition(sf::Vector2f(0, 0));
    m_background.setSize(m_size);
    m_background.setFillColor(m_color);
    
    m_outline.setPosition(sf::Vector2f(0, 0));
    m_outline.setSize(m_size);
    m_outline.setFillColor(m_color);

    sf::Vector2f thumb_size = sf::Vector2f(int(size.x/20), size.y);
    m_thumb.setSize(thumb_size);
    m_thumb.setFillColor(m_thumb_color);
    m_thumb.setOrigin(sf::Vector2f(thumb_size.x/2, thumb_size.y/2));
    m_thumb.setPosition(sf::Vector2f(m_size.x/2, m_size.y/2));
    
    sf::Vector2f body_size = sf::Vector2f(size.x, int(size.y/5));
    m_body.setSize(body_size);
    m_body.setFillColor(m_body_color);
    m_body.setOrigin(sf::Vector2f(body_size.x/2, body_size.y/2));
    m_body.setPosition(sf::Vector2f(m_size.x/2, m_size.y/2));
}

void Slider::handle_event(const sf::RenderWindow& window, sf::Vector2f world_pos, std::optional<sf::Vector2f> local_mouse_pos)
{
    if(local_mouse_pos.has_value())
    {
        world_pos = local_mouse_pos.value();
    }

    sf::Vector2f local_pos = world_pos - m_sprite.getPosition();

    if(sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
    {
        if(m_sprite.getGlobalBounds().contains(world_pos) && !m_being_dragged)
        {
            m_being_dragged = true;
        }
    }
    else if(m_being_dragged)
    {
        m_being_dragged = false;
    }

    if(m_being_dragged)
    {
        float offset = m_thumb.getSize().x/2;
        local_pos.x = std::max(offset, local_pos.x);
        local_pos.x = std::min(m_size.x-offset, local_pos.x);
        m_thumb.setPosition(sf::Vector2f(local_pos.x, m_thumb.getPosition().y));
        if(m_callback.has_value())
        {
            m_callback.value()();
        }
    }
}

float Slider::get_percentage()
{
    float offset = m_thumb.getSize().x/2;
    return (m_thumb.getPosition().x - offset) / (m_size.x - (offset*2));
}

void Slider::set_body_color(sf::Color color)
{
    m_body_color = color;
    m_body.setFillColor(m_body_color);
}

void Slider::set_thumb_color(sf::Color color)
{
    m_thumb_color = color;
    m_thumb.setFillColor(m_thumb_color);
}

void Slider::set_thumb_texture(sf::Texture texture, bool reset_rect)
{
    m_thumb_texture = std::make_unique<sf::Texture>(texture);
    m_thumb.setTexture(m_thumb_texture.get(), reset_rect);
}

void Slider::set_body_texture(sf::Texture texture, bool reset_rect)
{
    m_body_texture = std::make_unique<sf::Texture>(texture);
    m_body.setTexture(m_body_texture.get(), reset_rect);
}

