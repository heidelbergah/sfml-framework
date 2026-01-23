/**
 * Frame.cpp
 *
 * Written by James Bouchat
 */

#include "../../include/gui/Frame.hpp"
#include "../../include/gui/Button.hpp"
#include "../../include/gui/TextBox.hpp"

void Frame::draw(sf::RenderTarget &target, sf::RenderStates states) const
{
    m_render_texture.clear(sf::Color::Transparent);
    m_render_texture.draw(m_outline);
    m_render_texture.draw(m_background);
    if(m_show_taskbar)
    {
        auto taskbar = m_widgets.get_widget<TextBox>("taskbar");
        auto button = m_widgets.get_widget<Button>("exit");
        if (taskbar && button) {
            m_render_texture.draw(*taskbar);
            m_render_texture.draw(*button);
        }
    }
    m_render_texture.display();
    m_sprite.setTexture(m_render_texture.getTexture());
    target.draw(m_sprite, states);
}

Frame::Frame(sf::Vector2f size, sf::Color color, sf::Font& font) :
    Widget(size), m_font(font)
{
    m_color = color;
    m_size = size;

    m_base_transition = get_transition();
    m_skip_transition = true;

    m_show_taskbar = false;

    m_sprite.setPosition(sf::Vector2f(0, 0));
    m_background.setPosition(sf::Vector2f(0, 0));
    m_background.setSize(m_size);
    m_background.setFillColor(m_color);
}

void Frame::update(sf::Time delta_time)
{
    int start_index = 2;
    if(m_show_taskbar)
        start_index = 0;

    if(m_skip_transition)
    {
        set_transition(TransitionFunction::None);
        m_skip_transition = false;
    }
    else
    {
        set_transition(m_base_transition);
    }

    m_widgets.get_widget<TextBox>("taskbar")->update(delta_time);
    m_widgets.get_widget<Button>("exit")->update(delta_time);

    m_sprite.setPosition(m_pos);
}

void Frame::handle_event(const sf::RenderWindow& window, std::optional<sf::Vector2f> local_mouse_pos)
{
    int start_index = 2;
    if(m_show_taskbar)
        start_index = 0;
    
    sf::Vector2i pixel_pos = sf::Mouse::getPosition(window);
    
    sf::Vector2f world_pos = window.mapPixelToCoords(pixel_pos);
    world_pos.x /= m_mouse_scale;
    world_pos.y /= m_mouse_scale;
    
    sf::Vector2f local_mouse = world_pos - m_sprite.getPosition();

    m_widgets.get_widget<TextBox>("taskbar")->handle_event(window, local_mouse);
    m_widgets.get_widget<Button>("exit")->handle_event(window, local_mouse);

    if(m_show_taskbar)
    {
        pixel_pos = sf::Mouse::getPosition(window);
        
        sf::Vector2f world_pos = window.mapPixelToCoords(pixel_pos);
        world_pos.x /= m_mouse_scale;
        world_pos.y /= m_mouse_scale;

        // Need to localize position
        sf::Vector2f local_pos = world_pos - m_sprite.getPosition();

        if(sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
        {
            if(m_widgets.get_widget<TextBox>("taskbar")->get_global_bounds().contains(local_pos) && !m_being_dragged)
            {
                m_being_dragged = true;
                m_drag_offset = world_pos - m_pos.get_value();
            }
        }
        else if(m_being_dragged)
        {
            m_being_dragged = false;
        }

        if(m_being_dragged)
        {
            set_position(world_pos - m_drag_offset);
        }
    }
}

void Frame::add_taskbar(int height, sf::Color color, sf::Color outline_color, int thickness, std::string string)
{
    m_show_taskbar = true;

    // Add the taskbar
    m_widgets.add_widget("taskbar", std::make_shared<TextBox>(sf::Vector2f(m_size.x, height),
                color, string, m_font, sf::Color::Black));
    m_widgets.get_widget<TextBox>("taskbar")->set_transition(TransitionFunction::None);
    m_widgets.get_widget<TextBox>("taskbar")->set_position(sf::Vector2f(0, 0));
    m_widgets.get_widget<TextBox>("taskbar")->set_outline(outline_color, thickness);

    // Add the exit button
    m_widgets.add_widget("exit", std::make_shared<Button>(sf::Vector2f(height, height),
                sf::Color::Red));
    m_widgets.get_widget<Button>("exit")->set_transition(TransitionFunction::None);
    m_widgets.get_widget<Button>("exit")->set_position(sf::Vector2f(m_size.x-height, 0));
    m_widgets.get_widget<Button>("exit")->set_outline(outline_color, thickness);
    m_widgets.get_widget<Button>("exit")->add_text("X", m_font, sf::Color::Black);

}

void Frame::toggle_moveability()
{
    m_show_taskbar = !m_show_taskbar;
}

void Frame::add_widget(std::string key, std::shared_ptr<Widget> widget)
{
    m_widgets.add_widget(key, widget);
}

void Frame::remove_widget(std::string key)
{
    m_widgets.remove_widget(key);
}

void Frame::add_taskbar_texture(sf::Texture texture, bool reset_rect)
{
    m_taskbar_texture = std::make_unique<sf::Texture>(texture);
    m_widgets.get_widget<TextBox>("taskbar")->set_background_texture(*m_taskbar_texture, reset_rect);
}

void Frame::add_taskbar_exit(sf::Texture texture, bool reset_rect)
{
    m_exit_texture = std::make_unique<sf::Texture>(texture);
    m_widgets.get_widget<Button>("exit")->set_background_texture(*m_exit_texture, reset_rect);
}

