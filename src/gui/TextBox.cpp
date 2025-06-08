/**
 * TextBox.cpp
 *
 * Written by James Bouchat
 *
 * MAKE WIDGETS WITH TEXT INHERIT FROM A "TextWidget" OBJECT.
 * SHOULD HAVE THINGS LIKE add_text AND POINTERS TO FONT AND TEXT
 * BUILT IN. WILL STANDARDIZE TEXT HANDLING AND REDUCE CODE 
 * DUPLICATION
 */

#include "../../include/gui/TextBox.hpp"

void TextBox::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    m_render_texture.clear(sf::Color::Transparent);
    m_render_texture.draw(m_outline);
    m_render_texture.draw(m_background);
    if(m_text)
        m_render_texture.draw(*m_text);
    m_render_texture.display();
    m_sprite.setTexture(m_render_texture.getTexture());
    target.draw(m_sprite, states);
}

void TextBox::update_text(int num_chars)
{
    std::string new_text = "";
    new_text.append(m_string, 0, num_chars);

    set_text(new_text, m_text_color);
    set_alignment(CENTER);
}

TextBox::TextBox(sf::Vector2f size, sf::Color color,
                 std::string string, sf::Font& font,
                 sf::Color text_color) :
    TextWidget(size)
{
    m_color = color;
    m_size = size;

    add_text(string, font, text_color);

    m_sprite.setPosition(sf::Vector2f(0, 0));
    m_background.setPosition(sf::Vector2f(0, 0));
    m_background.setSize(m_size);
    m_background.setFillColor(m_color);
}

void TextBox::update(sf::Time delta_time)
{
    if(m_text_scroll)
    {
        if(clock->getElapsedTime().asMilliseconds() >= m_animation_speed)
        {
            update_text(m_chars_displayed++);
            clock->restart();
        }
        if(m_chars_displayed == m_chars_displayed_max)
        {
            m_text_scroll = false;
        }
    }
}

void TextBox::add_text_scroll(int speed)
{
    clock = std::make_unique<sf::Clock>();
    m_text_scroll = true;
    m_animation_speed = speed;
    m_chars_displayed = 0;
    m_chars_displayed_max = m_string.size()+1;
}

