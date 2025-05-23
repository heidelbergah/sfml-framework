/**
 * TextBox.hpp
 *
 * Written by James Bouchat
 */

#ifndef TEXTBOX_HPP
#define TEXTBOX_HPP

#include "TextWidget.hpp"

class TextBox : public TextWidget
{
private:
    std::unique_ptr<sf::Clock> clock;

    bool m_text_scroll = false;
    int m_animation_speed;
    int m_chars_displayed;
    int m_chars_displayed_max;

    void draw(sf::RenderTarget &target, sf::RenderStates states) const override;

    void update_text(int num_chars);
public:
    TextBox(sf::Vector2f size, sf::Color color, std::string text, sf::Font& font, sf::Color text_color);

    void update(sf::Time delta_time) override;

    void add_text_scroll(int speed);
};

#endif
