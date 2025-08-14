/**
 * Gauge.hpp
 *
 * Written by James Bouchat
 */

#ifndef GAUGE_HPP
#define GAUGE_HPP

#include "Widget.hpp"

class Gauge : public Widget
{
private:
    sf::CircleShape m_dial;
    sf::Color m_dial_color;
    sf::RectangleShape m_arm;
    sf::Color m_arm_color;
    sf::Vector2f m_range;              // x = low end; y = high end
    sf::Vector2f m_boundary_degrees;   // x = low end; y = high end

    sf::Angle m_arm_angle;

    std::unique_ptr<sf::Texture> m_dial_texture = nullptr;
    std::unique_ptr<sf::Texture> m_arm_texture = nullptr;

    void draw(sf::RenderTarget &target, sf::RenderStates states) const override;
public:
    Gauge(sf::Vector2f size, sf::Color color, sf::Vector2f range, sf::Vector2f boundary);
    
    void update(sf::Time delta_time) override;

    void set_arm_angle_based_on_value(float value);
    void set_arm_angle_based_on_degrees(float degrees);

    void set_dial_color(sf::Color color);

    void set_rotation(sf::Angle angle);

    void set_dial_texture(sf::Texture texture, bool reset_rect=false);
    void set_arm_texture(sf::Texture texture, bool reset_rect=false);
};

#endif
