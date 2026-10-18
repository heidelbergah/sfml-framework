/**
 * TextBox.cpp
 *
 * Written by James Bouchat
 */

#include "../../include/gui/TextBox.hpp"

TextBox::TextBox(sf::Vector2f size, sf::Color color, const std::string& text,
                 const sf::Font& font, sf::Color text_color)
    : TextWidget(size)
{
    m_color = color;
    m_background.setFillColor(m_color);

    add_text(text, font, text_color);
}

void TextBox::on_draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    if(m_outline_color != sf::Color::Transparent)
        target.draw(m_outline, states);

    target.draw(m_background, states);

    if(m_text) target.draw(*m_text, states);
}

void TextBox::update(sf::Time delta_time)
{
    if(!m_text_scroll) return;

    // Driven by delta time rather than its own sf::Clock, so the reveal
    // obeys pause and slow-motion along with everything else.
    m_scroll_accumulator += delta_time.asSeconds();

    bool changed = false;
    while(m_scroll_accumulator >= m_seconds_per_char &&
          m_chars_displayed < m_string.size())
    {
        m_scroll_accumulator -= m_seconds_per_char;
        ++m_chars_displayed;
        changed = true;
    }

    if(changed)
    {
        // Updates the existing sf::Text in place rather than building a
        // new one per character.
        set_text(m_string.substr(0, m_chars_displayed), m_text_color);
    }

    if(m_chars_displayed >= m_string.size())
    {
        m_text_scroll = false;
    }
}

void TextBox::add_text_scroll(float chars_per_second)
{
    m_seconds_per_char = (chars_per_second > 0.0f) ? (1.0f / chars_per_second) : 0.0f;
    restart_scroll();
}

void TextBox::restart_scroll()
{
    m_text_scroll        = true;
    m_chars_displayed    = 0;
    m_scroll_accumulator = 0.0f;
    set_text("", m_text_color);
}

void TextBox::finish_scroll()
{
    m_text_scroll     = false;
    m_chars_displayed = m_string.size();
    set_text(m_string, m_text_color);
}

bool TextBox::is_scrolling() const
{
    return m_text_scroll;
}

void TextBox::set_string(const std::string& text)
{
    m_string = text;

    if(m_text_scroll) restart_scroll();
    else              set_text(m_string, m_text_color);
}
