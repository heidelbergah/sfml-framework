/**
 * TextBox.hpp
 *
 * Static or typewriter-scrolling text in a coloured box.
 */

#ifndef TEXTBOX_HPP
#define TEXTBOX_HPP

#include "TextWidget.hpp"

class TextBox : public TextWidget
{
private:
    bool  m_text_scroll{false};
    float m_seconds_per_char{0.02f};
    float m_scroll_accumulator{0.0f};
    std::size_t m_chars_displayed{0};

    void on_draw(sf::RenderTarget& target, sf::RenderStates states) const override;

public:
    TextBox(sf::Vector2f size, sf::Color color, const std::string& text,
            const sf::Font& font, sf::Color text_color);

    void update(sf::Time delta_time) override;

    /*
     * Reveals the string one character at a time.
     * `chars_per_second` of 20 is a readable dialogue speed.
     */
    void add_text_scroll(float chars_per_second);

    /*
     * Restarts the reveal from the first character.
     */
    void restart_scroll();

    /*
     * Jumps to the fully revealed string.
     */
    void finish_scroll();

    [[nodiscard]] bool is_scrolling() const;

    /*
     * Replaces the string. Restarts the reveal if scrolling is on.
     */
    void set_string(const std::string& text);
};

#endif
