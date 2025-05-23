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
    m_render_texture.clear();
    m_render_texture.draw(m_outline);
    m_render_texture.draw(m_background);
    if(m_show_taskbar)
    {
        m_render_texture.draw(*m_widgets[0]);
        m_render_texture.draw(*m_widgets[1]);
    }
    for(int i = 2; i < m_widgets.size(); ++i)
    {
        m_render_texture.draw(*m_widgets[i]);
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
    for(int i = start_index; i < m_widgets.size(); ++i)
    {
        m_widgets[i]->update(delta_time);
    }
}

void Frame::handle_event(const sf::RenderWindow& window)
{
    int start_index = 2;
    if(m_show_taskbar)
        start_index = 0;
    for(int i = start_index; i < m_widgets.size(); ++i)
    {
        m_widgets[i]->handle_event(window);
    }
}

void Frame::set_position(sf::Vector2f pos)
{
    m_pos = pos;
    m_sprite.setPosition(m_pos);
}

void Frame::set_outline(sf::Color color, int thickness)
{
    m_outline_color = color;
    m_outline.setSize(m_size);
    m_outline.setFillColor(m_outline_color);
    m_outline.setPosition(sf::Vector2f(0, 0));
    m_background.setPosition(sf::Vector2f(thickness, thickness));
    m_background.setSize(sf::Vector2f(m_size.x - (thickness*2), m_size.y - (thickness*2)));
}

void Frame::add_taskbar(int height, sf::Color color, sf::Color outline_color, int thickness, std::string string)
{
    m_show_taskbar = true;

    auto taskbar = std::make_unique<TextBox>(sf::Vector2f(m_size.x, height), color,
            string, m_font, sf::Color::Black);
    taskbar->set_position(sf::Vector2f(0, 0));
    taskbar->set_outline(outline_color, thickness);
    m_widgets.push_back(std::move(taskbar));

    auto button = std::make_unique<Button>(sf::Vector2f(height, height), sf::Color::Red);
    button->set_position(sf::Vector2f(m_size.x-height, 0));
    button->set_outline(outline_color, thickness);
    button->add_text("X", m_font, sf::Color::Black);
    m_widgets.push_back(std::move(button));
}

void Frame::toggle_moveability()
{
    m_show_taskbar = !m_show_taskbar;
}

void Frame::add_widget(std::unique_ptr<Widget> widget)
{
    if(m_show_taskbar)
    {
        int height = m_widgets[0]->get_size().y;
        widget->move(sf::Vector2f(0, height));
    }
    m_widgets.push_back(std::move(widget));
}
