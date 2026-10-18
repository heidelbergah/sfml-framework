/**
 * Gauge.hpp
 *
 * Dial with a rotating arm -- speedometer, fuel gauge, boss health.
 *
 * Maps a value in [range.x, range.y] onto an angle in
 * [boundary.x, boundary.y] degrees. The old mapping was
 * `ratio * value`, which ignored both lower bounds, so a gauge with a
 * range of 50-100 or a boundary starting anywhere but zero pointed the
 * arm in the wrong place.
 *
 * The arm can tween to new values rather than snapping.
 */

#ifndef GAUGE_HPP
#define GAUGE_HPP

#include "Widget.hpp"

class Gauge : public Widget
{
private:
    sf::CircleShape    m_dial;
    sf::RectangleShape m_arm;

    sf::Color m_dial_color{sf::Color::Transparent};
    sf::Color m_arm_color;

    sf::Vector2f m_range;              // x = low value,  y = high value
    sf::Vector2f m_boundary_degrees;   // x = low angle,  y = high angle

    Interpolated<float> m_arm_degrees;

    const sf::Texture* m_dial_texture{nullptr};
    const sf::Texture* m_arm_texture{nullptr};

    void on_draw(sf::RenderTarget& target, sf::RenderStates states) const override;

public:
    Gauge(sf::Vector2f size, sf::Color color, sf::Vector2f range, sf::Vector2f boundary);

    void update(sf::Time delta_time) override;

    /*
     * Values outside the range are clamped to its ends.
     */
    void set_value(float value, bool instant = false);
    void set_arm_degrees(float degrees, bool instant = false);

    /*
     * Tween settings for the arm specifically, independent of the
     * widget's own position tween.
     */
    void set_arm_transition(TransitionFunction transition);
    void set_arm_transition_duration(float duration);

    void set_dial_color(sf::Color color);
    void set_arm_color(sf::Color color);

    void set_range(sf::Vector2f range);
    void set_boundary(sf::Vector2f boundary_degrees);

    /*
     * Borrowed textures -- must outlive this widget.
     */
    void set_dial_texture(const sf::Texture& texture, bool reset_rect = false);
    void set_arm_texture(const sf::Texture& texture, bool reset_rect = false);

    [[nodiscard]] float get_arm_degrees() const;
};

#endif
