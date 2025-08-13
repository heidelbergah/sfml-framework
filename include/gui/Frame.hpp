/**
 * Frame.hpp
 *
 * Written by James Bouchat
 */

#ifndef FRAME_HPP
#define FRAME_HPP

#include "WidgetManager.hpp"

class Frame : public Widget
{
private:
    WidgetManager m_widgets;

    sf::Color m_taskbar_color;
    sf::Color m_taskbar_outline_color;
    bool m_show_taskbar = false;
    bool m_being_dragged = false;
    bool m_skip_transition = false;

    sf::Vector2f m_drag_offset;

    TransitionFunction m_base_transition;

    sf::Font m_font;

    void draw(sf::RenderTarget &target, sf::RenderStates states) const override;

public:
    Frame(sf::Vector2f size, sf::Color color, sf::Font &font);

    void update(sf::Time delta_time) override;
    void handle_event(const sf::RenderWindow& window, std::optional<sf::Vector2f> local_mouse_pos = std::nullopt) override;

    void add_taskbar(int height, sf::Color color, sf::Color outline_color, int thickness, std::string string="");
    void toggle_moveability();

    void add_widget(std::string key, std::shared_ptr<Widget> widget);
    void remove_widget(std::string key);

    template<typename T>
    std::shared_ptr<T> get_widget(std::string key) const;
};

template<typename T>
std::shared_ptr<T> Frame::get_widget(std::string key) const
{
    return m_widgets.get_widget<T>(key);
}

#endif
