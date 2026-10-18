/**
 * Slider.cpp
 *
 * Written by James Bouchat
 */

#include "../../include/gui/Slider.hpp"

Slider::Slider(sf::Vector2f size, sf::Color color)
    : Widget(size), m_thumb_color(color)
{
    m_color = sf::Color::Transparent;
    m_background.setFillColor(m_color);
    m_outline.setFillColor(sf::Color::Transparent);

    sf::Vector2f const body_size{size.x, std::floor(size.y / 5.0f)};
    m_body.setSize(body_size);
    m_body.setFillColor(m_body_color);
    m_body.setOrigin({body_size.x / 2.0f, body_size.y / 2.0f});
    m_body.setPosition({m_size.x / 2.0f, m_size.y / 2.0f});

    sf::Vector2f const thumb_size{std::max(2.0f, std::floor(size.x / 20.0f)), size.y};
    m_thumb.setSize(thumb_size);
    m_thumb.setFillColor(m_thumb_color);
    m_thumb.setOrigin({thumb_size.x / 2.0f, thumb_size.y / 2.0f});

    sync_thumb_position();
}

void Slider::on_draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    if(m_outline_color != sf::Color::Transparent)
        target.draw(m_outline, states);

    if(m_color != sf::Color::Transparent)
        target.draw(m_background, states);

    target.draw(m_body, states);
    target.draw(m_thumb, states);
}

void Slider::sync_thumb_position()
{
    float const offset = m_thumb.getSize().x / 2.0f;
    float const travel = m_size.x - offset * 2.0f;

    m_thumb.setPosition({offset + m_value * travel, m_size.y / 2.0f});
}

float Slider::value_from_local_x(float local_x) const
{
    float const offset = m_thumb.getSize().x / 2.0f;
    float const travel = m_size.x - offset * 2.0f;

    if(travel <= 0.0f) return 0.0f;
    return std::clamp((local_x - offset) / travel, 0.0f, 1.0f);
}

void Slider::begin_frame()
{
    m_value_changed = false;
}

void Slider::handle_event(const sf::Event& event, sf::Vector2f mouse_pos)
{
    if(!m_enabled || !m_visible) return;

    sf::Vector2f const local_pos = mouse_pos - floor_vector(m_pos.get_value());

    auto apply = [this](float local_x)
    {
        float const new_value = value_from_local_x(local_x);
        if(new_value != m_value)
        {
            m_value         = new_value;
            m_value_changed = true;
            sync_thumb_position();
            if(m_callback) (*m_callback)();
        }
    };

    if(const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if(pressed->button == sf::Mouse::Button::Left && contains(mouse_pos))
        {
            m_being_dragged = true;
            apply(local_pos.x);
        }
    }
    else if(const auto* released = event.getIf<sf::Event::MouseButtonReleased>())
    {
        // Released anywhere, not just over the slider, otherwise
        // dragging off the widget leaves it stuck in the dragged state.
        if(released->button == sf::Mouse::Button::Left) m_being_dragged = false;
    }
    else if(event.is<sf::Event::MouseMoved>() && m_being_dragged)
    {
        apply(local_pos.x);
    }
}

float Slider::get_percentage() const
{
    return m_value;
}

float Slider::get_value(float min, float max) const
{
    return min + m_value * (max - min);
}

void Slider::set_percentage(float percentage)
{
    float const clamped = std::clamp(percentage, 0.0f, 1.0f);
    if(clamped == m_value) return;

    m_value         = clamped;
    m_value_changed = true;
    sync_thumb_position();
}

bool Slider::value_changed() const
{
    return m_value_changed;
}

bool Slider::is_being_dragged() const
{
    return m_being_dragged;
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

void Slider::set_thumb_texture(const sf::Texture& texture, bool reset_rect)
{
    m_thumb_texture = &texture;
    m_thumb.setTexture(m_thumb_texture, reset_rect);
}

void Slider::set_body_texture(const sf::Texture& texture, bool reset_rect)
{
    m_body_texture = &texture;
    m_body.setTexture(m_body_texture, reset_rect);
}
