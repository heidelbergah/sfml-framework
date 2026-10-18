/**
 * Gauge.cpp
 *
 * Written by James Bouchat
 */

#include "../../include/gui/Gauge.hpp"

Gauge::Gauge(sf::Vector2f size, sf::Color color, sf::Vector2f range,
             sf::Vector2f boundary)
    : Widget(size),
      m_arm_color(color),
      m_range(range),
      m_boundary_degrees(boundary)
{
    m_color = color;
    m_background.setFillColor(sf::Color::Transparent);
    m_outline.setFillColor(sf::Color::Transparent);

    float const radius = m_size.x / 2.0f;

    m_dial.setRadius(radius);
    m_dial.setFillColor(m_dial_color);
    m_dial.setOrigin({radius, radius});
    m_dial.setPosition({m_size.x / 2.0f, m_size.y / 2.0f});

    m_arm.setSize({std::max(1.0f, m_size.x / 15.0f), radius});
    m_arm.setFillColor(m_arm_color);
    m_arm.setOrigin({m_arm.getSize().x / 2.0f, radius});
    m_arm.setPosition({m_size.x / 2.0f, m_size.y / 2.0f});

    // Start at the midpoint of the range, not half the upper bound --
    // those are only the same thing when the range starts at zero.
    set_value((m_range.x + m_range.y) / 2.0f, true);
}

void Gauge::on_draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    if(m_outline_color != sf::Color::Transparent)
        target.draw(m_outline, states);

    if(m_color != sf::Color::Transparent && m_background_texture)
        target.draw(m_background, states);

    target.draw(m_dial, states);
    target.draw(m_arm, states);
}

void Gauge::update(sf::Time delta_time)
{
    (void)delta_time;

    m_arm.setRotation(sf::degrees(m_arm_degrees.get_value()));
}

void Gauge::set_value(float value, bool instant)
{
    float const span = m_range.y - m_range.x;
    if(span == 0.0f)
    {
        set_arm_degrees(m_boundary_degrees.x, instant);
        return;
    }

    value = std::clamp(value, std::min(m_range.x, m_range.y),
                              std::max(m_range.x, m_range.y));

    float const ratio = (value - m_range.x) / span;
    float const degrees =
        m_boundary_degrees.x + ratio * (m_boundary_degrees.y - m_boundary_degrees.x);

    set_arm_degrees(degrees, instant);
}

void Gauge::set_arm_degrees(float degrees, bool instant)
{
    if(instant) m_arm_degrees.set_instant(degrees);
    else        m_arm_degrees.set_value(degrees);
}

void Gauge::set_arm_transition(TransitionFunction transition)
{
    m_arm_degrees.set_transition(transition);
}

void Gauge::set_arm_transition_duration(float duration)
{
    m_arm_degrees.set_duration(duration);
}

void Gauge::set_dial_color(sf::Color color)
{
    m_dial_color = color;
    m_dial.setFillColor(m_dial_color);
}

void Gauge::set_arm_color(sf::Color color)
{
    m_arm_color = color;
    m_arm.setFillColor(m_arm_color);
}

void Gauge::set_range(sf::Vector2f range)
{
    m_range = range;
}

void Gauge::set_boundary(sf::Vector2f boundary_degrees)
{
    m_boundary_degrees = boundary_degrees;
}

void Gauge::set_dial_texture(const sf::Texture& texture, bool reset_rect)
{
    m_dial_texture = &texture;
    m_dial.setTexture(m_dial_texture, reset_rect);
}

void Gauge::set_arm_texture(const sf::Texture& texture, bool reset_rect)
{
    m_arm_texture = &texture;
    m_arm.setTexture(m_arm_texture, reset_rect);
}

float Gauge::get_arm_degrees() const
{
    return m_arm_degrees.get_value();
}
