/**
 * Frame.hpp
 *
 * Written by James Bouchat
 */

#ifndef FRAME_HPP
#define FRAME_HPP

#include "Widget.hpp"

class Frame : public Widget
{
private:
    sf::Color m_taskbar_color;
    sf::Color m_taskbar_outline_color;
    bool m_show_taskbar = false;
    bool m_being_dragged = false;

    sf::Font m_font;

    std::vector<std::unique_ptr<Widget>> m_widgets;

    void draw(sf::RenderTarget &target, sf::RenderStates states) const override;

public:
    Frame(sf::Vector2f size, sf::Color color, sf::Font &font);

    void update(sf::Time delta_time) override;
    void handle_event(const sf::RenderWindow& window, std::optional<sf::Vector2f> local_mouse_pos = std::nullopt) override;

    void add_taskbar(int height, sf::Color color, sf::Color outline_color, int thickness, std::string string="");
    void toggle_moveability();

    void add_widget(std::unique_ptr<Widget> widget);
};

#endif
