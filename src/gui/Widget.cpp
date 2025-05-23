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

}

void Widget::set_position(sf::Vector2f pos)
{
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
