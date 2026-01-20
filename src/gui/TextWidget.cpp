/**
 * TextWidget.cpp
 *
 * Written by James Bouchat
 */

#include "../../include/gui/TextWidget.hpp"

TextWidget::TextWidget(sf::Vector2f size) :
    Widget(size)
{
    m_character_size = 12;
}

void TextWidget::set_text(std::string string, sf::Color color)
{
    m_text = std::make_unique<sf::Text>(*m_font, string, string.size());
    
    m_text->setCharacterSize(m_character_size);
    m_text->setStyle(sf::Text::Regular);
    m_text->setFillColor(color);

    set_alignment(CENTER);
}
    
void TextWidget::set_character_size(int size)
{
    m_character_size = size;
    m_text->setCharacterSize(m_character_size);
}

void TextWidget::set_alignment(Alignment alignment)
{
    sf::Vector2f text_pos;
    sf::Vector2f text_bounds = m_text->getLocalBounds().size;
    int letter_spacing = m_text->getLetterSpacing();
    int vertical_spacing = m_character_size/4;

    switch(alignment)
    {
        case CENTER:
            m_text->setOrigin(sf::Vector2f(text_bounds.x/2, (text_bounds.y/2) + vertical_spacing));
            text_pos = sf::Vector2f(int(m_size.x/2), int(m_size.y/2));
            break;
        case LEFT:
            m_text->setOrigin(sf::Vector2f(0, (text_bounds.y/2) + vertical_spacing));
            text_pos = sf::Vector2f(letter_spacing, int(m_size.y/2));
            break;
        case RIGHT:
            m_text->setOrigin(sf::Vector2f(text_bounds.x, (text_bounds.y/2) + vertical_spacing));
            text_pos = sf::Vector2f(m_size.x-letter_spacing, int(m_size.y/2));
            break;
    }
    
    int x_pos = std::floor(text_pos.x), y_pos = std::floor(text_pos.y);
    m_text->setPosition(sf::Vector2f(x_pos, y_pos));
}

void TextWidget::add_text(std::string text, sf::Font& font, sf::Color color)
{
    m_font = std::make_unique<sf::Font>(font);
    m_string = text;
    m_text_color = color;
    set_text(text, color);
}

