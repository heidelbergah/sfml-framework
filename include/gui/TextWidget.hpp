/**
 * TextWidget.hpp
 *
 * Written by James Bouchat
 */

#ifndef TEXTWIDGET_HPP
#define TEXTWIDGET_HPP

#include "Widget.hpp"

class TextWidget : public Widget
{
private:
    void virtual draw(sf::RenderTarget &target, sf::RenderStates states) const = 0;

protected:
    std::unique_ptr<sf::Font> m_font;
    std::unique_ptr<sf::Text> m_text;
    std::string m_string;
    Alignment m_alignment;
    sf::Color m_text_color;

    void set_text(std::string string, sf::Color color);

public:
    TextWidget(sf::Vector2f size);
    virtual void update(sf::Time delta_time) {};
    virtual void handle_event(const sf::RenderWindow& window) {};
    
    void set_alignment(Alignment alignment);
    void add_text(std::string text, sf::Font& font, sf::Color color);

};

#endif
