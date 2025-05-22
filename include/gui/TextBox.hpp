/**
 * TextBox.hpp
 *
 * Written by James Bouchat
 */

#ifndef TEXTBOX_HPP
#define TEXTBOX_HPP

#include "Widget.hpp"

class TextBox : public Widget
{
private:
    mutable sf::RenderTexture m_render_texture;
    mutable sf::Sprite m_sprite;
    sf::RectangleShape m_background;
    sf::RectangleShape m_outline;
    sf::Color m_color;
    sf::Color m_outline_color;
    sf::Vector2f m_pos;
    sf::Vector2f m_size;

    std::unique_ptr<sf::Clock> clock;
    sf::Font m_font;
    sf::Text m_text;
    std::string m_string;

    bool m_text_scroll = false;
    int m_animation_speed;
    int m_chars_displayed;
    int m_chars_displayed_max;

    void draw(sf::RenderTarget &target, sf::RenderStates states) const override;

    void update_text(int num_chars);
public:
    TextBox(sf::Vector2f size, sf::Color color, std::string text, sf::Font& font);

    void update(sf::Time delta_time) override;

    void set_position(sf::Vector2f pos);
    void set_outline(sf::Color color, int thickness);

    void add_text_scroll(int speed);
};

#endif
