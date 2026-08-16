/**
 * TextWidget.hpp
 *
 * Base for widgets that display text. Factors font handling and
 * alignment out of Button and TextBox.
 *
 * The font is borrowed, not copied. Copying an sf::Font duplicates its
 * glyph atlas in VRAM, so ten buttons sharing one typeface used to mean
 * ten atlases. Pass a reference from Assets::fonts().
 */

#ifndef TEXTWIDGET_HPP
#define TEXTWIDGET_HPP

#include "Widget.hpp"

class TextWidget : public Widget
{
protected:
    // Borrowed. Must outlive this widget.
    const sf::Font* m_font{nullptr};

    std::unique_ptr<sf::Text> m_text;
    std::string               m_string;
    Alignment                 m_alignment{Alignment::CENTER};
    sf::Color                 m_text_color{sf::Color::Black};
    unsigned int              m_character_size{12};

    /*
     * Updates the displayed string in place. sf::Text is created once
     * and reused -- rebuilding it per change meant a heap allocation
     * and a fresh layout for every character of a scrolling line.
     */
    void set_text(const std::string& string, sf::Color color);

    void refresh_alignment();

public:
    explicit TextWidget(sf::Vector2f size);

    /*
     * Borrows `font`; it must outlive this widget.
     */
    void add_text(const std::string& text, const sf::Font& font, sf::Color color);

    void set_alignment(Alignment alignment);
    void set_character_size(unsigned int size);
    void set_text_color(sf::Color color);

    [[nodiscard]] const std::string& get_string() const;
    [[nodiscard]] bool               has_text() const;
};

#endif
