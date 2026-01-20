/**
 * Widget.cpp
 *
 * Written by James Bouchat
 */

#include "../../include/gui/Widget.hpp"

Widget::Widget(sf::Vector2f size) :
    m_render_texture({static_cast<unsigned>(size.x),
                      static_cast<unsigned>(size.y)}),
    m_sprite(m_render_texture.getTexture())
{
    m_render_texture.setSmooth(false);
}

void Widget::set_position(sf::Vector2f pos, bool instant)
{
    if(instant)
        m_pos.set_instant(pos);
    else
        m_pos = pos;
    m_sprite.setPosition(m_pos);
}

void Widget::set_outline(sf::Color color, int thickness)
{
    m_outline_color = color;
    m_outline.setSize(m_size);
    m_outline.setFillColor(m_outline_color);
    m_outline.setPosition(sf::Vector2f(0, 0));
    m_background.setPosition(sf::Vector2f(thickness, thickness));
    m_background.setSize(sf::Vector2f(m_size.x - (thickness*2), m_size.y - (thickness*2)));
}

void Widget::set_z_value(unsigned int z)
{
    m_z_value = z;
}

void Widget::set_callback(std::function<void()> callback)
{
    m_callback = callback;
}

void Widget::set_transition(TransitionFunction transition)
{
    m_pos.set_transition(transition);
}

void Widget::set_transition_duration(float duration)
{
    m_pos.set_duration(duration);
}

void Widget::set_background_texture(sf::Texture texture, bool reset_rect)
{
    m_background_texture = std::make_unique<sf::Texture>(texture);
    m_background.setTexture(m_background_texture.get(), reset_rect);
}

void Widget::move(sf::Vector2f move_vector)
{
    m_sprite.move(move_vector);
}

sf::Vector2f Widget::get_position()
{
    return m_pos;
}

sf::Vector2f Widget::get_size()
{
    return m_size;
}

sf::FloatRect Widget::get_global_bounds()
{
    return m_sprite.getGlobalBounds();
}

TransitionFunction Widget::get_transition() const
{
    return m_pos.get_transition();
}

unsigned int Widget::get_z_value()
{
    return m_z_value;
}
