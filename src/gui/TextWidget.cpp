/**
 * TextWidget.cpp
 *
 * Written by James Bouchat
 */

#include "../../include/gui/TextWidget.hpp"

TextWidget::TextWidget(sf::Vector2f size)
    : Widget(size)
{}

void TextWidget::add_text(const std::string& text, const sf::Font& font, sf::Color color)
{
    m_font       = &font;
    m_string     = text;
    m_text_color = color;

    set_text(text, color);
}

void TextWidget::set_text(const std::string& string, sf::Color color)
{
    if(!m_font) return;   // add_text has not been called yet

    m_text_color = color;

    if(!m_text)
    {
        // Third argument is characterSize, not string length. The old
        // code passed string.size() here, which built a glyph atlas
        // sized to the length of the text before overwriting it.
        m_text = std::make_unique<sf::Text>(*m_font, string, m_character_size);
        m_text->setStyle(sf::Text::Regular);
    }
    else
    {
        m_text->setString(string);
    }

    m_text->setFillColor(m_text_color);
    refresh_alignment();
}

void TextWidget::refresh_alignment()
{
    if(!m_text) return;

    sf::FloatRect const bounds = m_text->getLocalBounds();
    float const letter_spacing   = m_text->getLetterSpacing();
    float const vertical_spacing = static_cast<float>(m_character_size) / 4.0f;

    sf::Vector2f origin;
    sf::Vector2f position;

    switch(m_alignment)
    {
        case Alignment::CENTER:
            origin   = {bounds.size.x / 2.0f, bounds.size.y / 2.0f + vertical_spacing};
            position = {m_size.x / 2.0f, m_size.y / 2.0f};
            break;

        case Alignment::LEFT:
            origin   = {0.0f, bounds.size.y / 2.0f + vertical_spacing};
            position = {letter_spacing, m_size.y / 2.0f};
            break;

        case Alignment::RIGHT:
            origin   = {bounds.size.x, bounds.size.y / 2.0f + vertical_spacing};
            position = {m_size.x - letter_spacing, m_size.y / 2.0f};
            break;
    }

    m_text->setOrigin(origin);

    // Whole-pixel placement keeps bitmap-style fonts sharp.
    m_text->setPosition(floor_vector(position));
}

void TextWidget::set_alignment(Alignment alignment)
{
    m_alignment = alignment;
    refresh_alignment();
}

void TextWidget::set_character_size(unsigned int size)
{
    m_character_size = size;
    if(m_text)
    {
        m_text->setCharacterSize(m_character_size);
        refresh_alignment();
    }
}

void TextWidget::set_text_color(sf::Color color)
{
    m_text_color = color;
    if(m_text) m_text->setFillColor(m_text_color);
}

const std::string& TextWidget::get_string() const
{
    return m_string;
}

bool TextWidget::has_text() const
{
    return m_text != nullptr;
}
