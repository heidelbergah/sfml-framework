/**
 * Frame.cpp
 *
 * Written by James Bouchat
 */

#include "../../include/gui/Frame.hpp"
#include "../../include/gui/Button.hpp"
#include "../../include/gui/TextBox.hpp"

Frame::Frame(sf::Vector2f size, sf::Color color, const sf::Font& font)
    : Widget(size), m_font(&font)
{
    m_color = color;
    m_background.setFillColor(m_color);
}

void Frame::on_draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    if(m_outline_color != sf::Color::Transparent)
        target.draw(m_outline, states);

    target.draw(m_background, states);

    // Children inherit the frame's transform, so they draw at their own
    // local positions without any manual offsetting.
    target.draw(m_widgets, states);

    if(m_show_taskbar)
    {
        if(m_taskbar) target.draw(*m_taskbar, states);
        if(m_exit)    target.draw(*m_exit, states);
    }
}

sf::Vector2f Frame::to_local(sf::Vector2f parent_pos) const
{
    return parent_pos - floor_vector(m_pos.get_value());
}

void Frame::begin_frame()
{
    m_close_requested = false;
    m_widgets.begin_frame();

    if(m_taskbar) m_taskbar->begin_frame();
    if(m_exit)    m_exit->begin_frame();
}

void Frame::update(sf::Time delta_time)
{
    // Guarded throughout. The old version dereferenced the taskbar and
    // exit button unconditionally, so a Frame created without
    // add_taskbar() crashed on its first update.
    m_widgets.update(delta_time);

    if(!m_show_taskbar) return;

    if(m_taskbar) m_taskbar->update(delta_time);
    if(m_exit)
    {
        m_exit->update(delta_time);
        if(m_exit->was_clicked()) m_close_requested = true;
    }
}

void Frame::handle_event(const sf::Event& event, sf::Vector2f mouse_pos)
{
    if(!m_enabled || !m_visible) return;

    sf::Vector2f const local_pos = to_local(mouse_pos);

    m_widgets.handle_event(event, local_pos);

    if(!m_show_taskbar) return;

    if(m_exit)    m_exit->handle_event(event, local_pos);
    if(!m_taskbar) return;
    m_taskbar->handle_event(event, local_pos);

    if(const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>())
    {
        // Pressing the close button should not also start a drag.
        bool const on_exit = m_exit && m_exit->contains(local_pos);

        if(pressed->button == sf::Mouse::Button::Left &&
           m_taskbar->contains(local_pos) && !on_exit && !m_being_dragged)
        {
            m_being_dragged = true;
            m_drag_offset   = mouse_pos - m_pos.get_value();
        }
    }
    else if(const auto* released = event.getIf<sf::Event::MouseButtonReleased>())
    {
        if(released->button == sf::Mouse::Button::Left) m_being_dragged = false;
    }
    else if(event.is<sf::Event::MouseMoved>() && m_being_dragged)
    {
        // Instant, so the frame tracks the cursor rather than easing
        // toward it a frame behind.
        set_position(mouse_pos - m_drag_offset, true);
    }
}

void Frame::add_taskbar(float height, sf::Color color, sf::Color outline_color,
                        float thickness, const std::string& title)
{
    if(!m_font) return;

    m_show_taskbar   = true;
    m_taskbar_height = height;

    m_taskbar = std::make_shared<TextBox>(sf::Vector2f{m_size.x, height},
                                          color, title, *m_font, sf::Color::Black);
    m_taskbar->set_transition(TransitionFunction::None);
    m_taskbar->set_position({0.0f, 0.0f}, true);
    m_taskbar->set_outline(outline_color, thickness);

    m_exit = std::make_shared<Button>(sf::Vector2f{height, height}, sf::Color::Red);
    m_exit->set_transition(TransitionFunction::None);
    m_exit->set_position({m_size.x - height, 0.0f}, true);
    m_exit->set_outline(outline_color, thickness);
    m_exit->add_text("X", *m_font, sf::Color::Black);
}

bool Frame::has_taskbar() const
{
    return m_taskbar != nullptr;
}

void Frame::toggle_moveability()
{
    set_moveable(!m_show_taskbar);
}

void Frame::set_moveable(bool moveable)
{
    m_show_taskbar = moveable && has_taskbar();
    if(!m_show_taskbar) m_being_dragged = false;
}

void Frame::add_widget(const std::string& key, std::shared_ptr<Widget> widget)
{
    m_widgets.add_widget(key, std::move(widget));
}

void Frame::remove_widget(const std::string& key)
{
    m_widgets.remove_widget(key);
}

void Frame::add_taskbar_texture(const sf::Texture& texture, bool reset_rect)
{
    if(m_taskbar) m_taskbar->set_background_texture(texture, reset_rect);
}

void Frame::add_taskbar_exit_texture(const sf::Texture& texture, bool reset_rect)
{
    if(m_exit) m_exit->set_background_texture(texture, reset_rect);
}

bool Frame::close_requested() const
{
    return m_close_requested;
}
