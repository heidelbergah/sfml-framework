/**
 * Widget.cpp
 *
 * Written by James Bouchat
 */

#include "../../include/gui/Widget.hpp"

Widget::Widget(sf::Vector2f size)
    : m_size(size)
{
    // Setting m_size here means derived classes cannot forget to do it.
    m_background.setPosition({0.0f, 0.0f});
    m_background.setSize(m_size);
    m_background.setFillColor(m_color);

    m_outline.setPosition({0.0f, 0.0f});
    m_outline.setSize(m_size);
    m_outline.setFillColor(sf::Color::Transparent);
}

void Widget::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    if(!m_visible) return;

    states.transform *= get_transform();
    on_draw(target, states);
}

sf::Transform Widget::get_transform() const
{
    sf::Transform transform;
    transform.translate(floor_vector(m_pos.get_value()));
    return transform;
}

void Widget::set_position(sf::Vector2f pos, bool instant)
{
    if(instant) m_pos.set_instant(pos);
    else        m_pos.set_value(pos);
}

void Widget::set_size(sf::Vector2f size)
{
    m_size = size;
    m_background.setSize(m_size);
    m_outline.setSize(m_size);
}

void Widget::set_color(sf::Color color)
{
    m_color = color;
    m_background.setFillColor(m_color);
}

void Widget::set_outline(sf::Color color, float thickness)
{
    m_outline_color = color;

    // The "outline" is a full-size rectangle drawn behind a background
    // inset by `thickness`, which is how the original worked. Clamped
    // so a thickness wider than the widget cannot produce a negative
    // size (SFML would draw that mirrored).
    float const max_thickness = std::min(m_size.x, m_size.y) / 2.0f;
    thickness = std::clamp(thickness, 0.0f, max_thickness);

    m_outline.setSize(m_size);
    m_outline.setFillColor(m_outline_color);
    m_outline.setPosition({0.0f, 0.0f});

    m_background.setPosition({thickness, thickness});
    m_background.setSize({m_size.x - thickness * 2.0f,
                          m_size.y - thickness * 2.0f});
}

void Widget::set_z_value(unsigned int z)
{
    m_z_value = z;
}

void Widget::set_visible(bool visible)
{
    m_visible = visible;
}

void Widget::set_enabled(bool enabled)
{
    m_enabled = enabled;
}

void Widget::set_callback(std::function<void()> callback)
{
    m_callback = std::move(callback);
}

void Widget::set_transition(TransitionFunction transition)
{
    m_pos.set_transition(transition);
}

void Widget::set_transition_duration(float duration)
{
    m_pos.set_duration(duration);
}

void Widget::set_background_texture(const sf::Texture& texture, bool reset_rect)
{
    // Borrowed. No copy is made, an sf::Texture copy is a fresh GPU
    // allocation plus a blit, and the old code did two of them per call.
    m_background_texture = &texture;
    m_background.setTexture(m_background_texture, reset_rect);
}

void Widget::clear_background_texture()
{
    m_background_texture = nullptr;
    m_background.setTexture(nullptr, false);
}

void Widget::move(sf::Vector2f offset)
{
    m_pos.set_value(m_pos.get_value() + offset);
}

sf::Vector2f Widget::get_position() const
{
    return m_pos.get_value();
}

sf::Vector2f Widget::get_target_position() const
{
    return m_pos.get_target();
}

sf::Vector2f Widget::get_size() const
{
    return m_size;
}

sf::Color Widget::get_color() const
{
    return m_color;
}

sf::FloatRect Widget::get_global_bounds() const
{
    return get_transform().transformRect(sf::FloatRect{{0.0f, 0.0f}, m_size});
}

bool Widget::contains(sf::Vector2f point) const
{
    return get_global_bounds().contains(point);
}

TransitionFunction Widget::get_transition() const
{
    return m_pos.get_transition();
}

unsigned int Widget::get_z_value() const
{
    return m_z_value;
}

bool Widget::is_visible() const
{
    return m_visible;
}

bool Widget::is_enabled() const
{
    return m_enabled;
}
