/**
 * Widget.hpp
 *
 * Written by James Bouchat
 */

#ifndef WIDGET_HPP
#define WIDGET_HPP

#include "../Includes.hpp"

enum Alignment
{
    CENTER = 1,
    RIGHT = 2,
    LEFT = 3
};

class Widget : public sf::Drawable
{
private:
    void virtual draw(sf::RenderTarget &target, sf::RenderStates states) const = 0;

protected:
    sf::Vector2f m_pos;
    sf::Vector2f m_size;

public:
    Widget() = default;
    virtual void update(sf::Time delta_time) {};
    virtual void handle_event(const sf::RenderWindow& window) {};
};

#endif
