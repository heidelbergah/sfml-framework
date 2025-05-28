/**
 * Gauge.cpp
 *
 * Written by James Bouchat
 */

#include "../../include/gui/Gauge.hpp"

void Gauge::draw(sf::RenderTarget &target, sf::RenderStates states) const
{
    m_render_texture.clear(sf::Color::Transparent);
    m_render_texture.draw(m_outline);
    m_render_texture.draw(m_background);
    m_render_texture.draw(m_dial);
    m_render_texture.draw(m_arm);
    m_render_texture.display();
    m_sprite.setTexture(m_render_texture.getTexture());
    target.draw(m_sprite, states);
}

Gauge::Gauge(sf::Vector2f size, sf::Color color, sf::Vector2f range,
        sf::Vector2f boundary) : Widget(size)
{
    m_color = color;
    m_size = size;
    m_arm_color = m_color;
    m_dial_color = sf::Color::Transparent;

    m_range = range;
    m_boundary_degrees = boundary;

    float radius = m_size.x/2;
    m_dial.setFillColor(m_dial_color);
    m_dial.setRadius(radius);
    m_dial.setOrigin(sf::Vector2f(radius, radius));
    m_dial.setPosition(sf::Vector2f(m_size.x/2, m_size.y/2));

    m_arm.setFillColor(m_arm_color);
    m_arm.setSize(sf::Vector2f(m_size.x/15, radius));
    m_arm.setOrigin(sf::Vector2f(m_arm.getSize().x/2, radius));
    m_arm.setPosition(sf::Vector2f(m_size.x/2, m_size.y/2));

    set_arm_angle_based_on_value(m_range.y / 2);

    m_sprite.setPosition(sf::Vector2f(0, 0));
    m_background.setPosition(sf::Vector2f(0, 0));
    m_background.setSize(m_size);
    m_background.setFillColor(sf::Color::Transparent);
}

void Gauge::update(sf::Time delta_time)
{
    m_arm.setRotation(m_arm_angle);
}

void Gauge::set_arm_angle_based_on_value(float value)
{
    value = std::min(value, m_range.y);
    value = std::max(value, m_range.x);

    float boundary_difference = m_boundary_degrees.y - m_boundary_degrees.x;
    float range_difference = m_range.y - m_range.x;

    float degree_ratio = boundary_difference / range_difference;

    m_arm_angle = sf::degrees(degree_ratio * value);
}

void Gauge::set_arm_angle_based_on_degrees(float degrees)
{
    m_arm_angle = sf::degrees(degrees);
}

void Gauge::set_dial_color(sf::Color color)
{
    m_dial_color = color;
    m_dial.setFillColor(m_dial_color);
}

void Gauge::set_rotation(sf::Angle angle)
{
    m_sprite.setRotation(angle);
}
