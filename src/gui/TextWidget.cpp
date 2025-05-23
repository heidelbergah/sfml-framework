/**
 * TextWidget.cpp
 *
 * Written by James Bouchat
 */

#include "../../include/gui/TextWidget.hpp"

void TextWidget::set_text(std::string string, sf::Color color)
{
    m_text = std::make_unique<sf::Text>(*m_font, string, string.size());
    
    m_text->setCharacterSize(12);
    m_text->setStyle(sf::Text::Regular);
    m_text->setFillColor(color);

    set_alignment(CENTER);
}

void TextWidget::set_alignment(Alignment alignment)
{
    sf::Vector2f text_pos;
    sf::Vector2f text_bounds = m_text->getLocalBounds().size;
    int letter_spacing = m_text->getLetterSpacing();
    
    switch(alignment)
    {
        case CENTER:
            m_text->setOrigin(sf::Vector2f(text_bounds.x/2, text_bounds.y/2));
            text_pos = sf::Vector2f(m_size.x/2, m_size.y/2);
            break;
        case LEFT:
            m_text->setOrigin(sf::Vector2f(0, text_bounds.y/2));
            text_pos = sf::Vector2f(letter_spacing, m_size.y/2);
            break;
        case RIGHT:
            m_text->setOrigin(sf::Vector2f(text_bounds.x, text_bounds.y/2));
            text_pos = sf::Vector2f(m_size.x-letter_spacing, m_size.y/2);
            break;
    }
    
    m_text->setPosition(text_pos);
}

void TextWidget::add_text(std::string text, sf::Font& font, sf::Color color)
{
    m_font = std::make_unique<sf::Font>(font);
    m_string = text;
    m_text_color = color;
    set_text(text, color);
}

