/**
 * Button.cpp
 *
 * Written by James Bouchat
 */

#include "../../include/gui/Button.hpp"

namespace
{
    /*
     * Shifts a colour by `amount`, clamped. Positive lightens.
     */
    sf::Color shade(sf::Color color, int amount)
    {
        auto adjust = [amount](std::uint8_t channel)
        {
            return static_cast<std::uint8_t>(
                std::clamp(static_cast<int>(channel) + amount, 0, 255));
        };

        return sf::Color{adjust(color.r), adjust(color.g), adjust(color.b), color.a};
    }
}

Button::Button(sf::Vector2f size, sf::Color color)
    : TextWidget(size)
{
    m_color = color;
    m_background.setFillColor(m_color);

    create_color_variants();
}

void Button::create_color_variants()
{
    // Both variants darken -- hovering slightly, pressing more. The old
    // names said "light" and "dark" but both subtracted, so the light
    // one was in fact darker than the base colour.
    m_hover_color = shade(m_color, -40);
    m_press_color = shade(m_color, -80);
}

void Button::set_state_tints(int hover_amount, int press_amount)
{
    m_hover_color = shade(m_color, -hover_amount);
    m_press_color = shade(m_color, -press_amount);
}

void Button::on_draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    if(m_outline_color != sf::Color::Transparent)
        target.draw(m_outline, states);

    target.draw(m_background, states);

    if(m_text) target.draw(*m_text, states);
}

void Button::begin_frame()
{
    m_clicked = false;
}

void Button::update(sf::Time delta_time)
{
    (void)delta_time;

    sf::Color const target = m_held    ? m_press_color
                           : m_hovered ? m_hover_color
                                       : m_color;

    m_background.setFillColor(m_enabled ? target : shade(m_color, -110));
}

void Button::handle_event(const sf::Event& event, sf::Vector2f mouse_pos)
{
    if(!m_enabled || !m_visible) return;

    if(const auto* moved = event.getIf<sf::Event::MouseMoved>())
    {
        (void)moved;
        m_hovered = contains(mouse_pos);
        return;
    }

    if(const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if(pressed->button != sf::Mouse::Button::Left) return;

        m_hovered = contains(mouse_pos);
        if(m_hovered) m_held = true;
        return;
    }

    if(const auto* released = event.getIf<sf::Event::MouseButtonReleased>())
    {
        if(released->button != sf::Mouse::Button::Left) return;

        m_hovered = contains(mouse_pos);

        // A click is press-inside followed by release-inside. Dragging
        // off the button before releasing cancels it, which is what
        // every other UI toolkit does.
        if(m_held && m_hovered)
        {
            m_clicked = true;
            if(m_callback) (*m_callback)();
        }

        m_held = false;
    }
}

bool Button::is_hovered() const
{
    return m_hovered;
}

bool Button::is_pressed() const
{
    return m_held;
}

bool Button::was_clicked() const
{
    return m_clicked;
}
