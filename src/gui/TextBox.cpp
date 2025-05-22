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
    m_render_texture.clear();
    m_render_texture.draw(m_outline);
    m_render_texture.draw(m_background);
    m_render_texture.draw(m_text);
    m_render_texture.display();
    m_sprite.setTexture(m_render_texture.getTexture());
    target.draw(m_sprite, states);
}

void TextBox::update_text(int num_chars)
{
    std::string new_text = "";
    new_text.append(m_string, 0, num_chars);

    m_text.setString(new_text);
    m_text.setCharacterSize(12);
    m_text.setStyle(sf::Text::Regular);
    m_text.setFillColor(sf::Color::Black);

    sf::Vector2f text_pos;
    sf::Vector2f text_bounds = m_text.getLocalBounds().size;
    int letter_spacing = m_text.getLetterSpacing();

    m_text.setOrigin(sf::Vector2f(text_bounds.x/2, text_bounds.y/2));
    text_pos = sf::Vector2f(m_size.x/2, m_size.y/2);

    m_text.setPosition(text_pos);
}

TextBox::TextBox(sf::Vector2f size, sf::Color color,
                 std::string text, sf::Font& font) :
    m_render_texture({static_cast<unsigned>(size.x),
                      static_cast<unsigned>(size.y)}),
    m_sprite(m_render_texture.getTexture()),
    m_font(font),
    m_text(m_font, text, text.size())
{
    m_color = color;
    m_size = size;
    m_string = text;

    m_sprite.setPosition(sf::Vector2f(0, 0));
    m_background.setPosition(sf::Vector2f(0, 0));
    m_background.setSize(m_size);
    m_background.setFillColor(m_color);

    // ================================
    update_text(text.size());   
    // ================================
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

void TextBox::set_position(sf::Vector2f pos)
{
    m_pos = pos;
    m_sprite.setPosition(m_pos);
}

void TextBox::set_outline(sf::Color color, int thickness)
{
    m_outline_color = color;
    m_outline.setSize(m_size);
    m_outline.setFillColor(m_outline_color);
    m_outline.setPosition(sf::Vector2f(0, 0));
    m_background.setPosition(sf::Vector2f(thickness, thickness));
    m_background.setSize(sf::Vector2f(m_size.x - (thickness*2), m_size.y - (thickness*2)));
}

void TextBox::add_text_scroll(int speed)
{
    clock = std::make_unique<sf::Clock>();
    m_text_scroll = true;
    m_animation_speed = speed;
    m_chars_displayed = 0;
    m_chars_displayed_max = m_string.size();
}


