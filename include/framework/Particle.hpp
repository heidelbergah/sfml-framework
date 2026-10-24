/**
 * Particle.hpp
 *
 * A single particle.
 *
 * Velocity and acceleration are Cartesian and expressed in units per
 * second. The old version stored them in polar form, so every velocity
 * change cost a sqrt and an atan2, and then update() converted back to
 * Cartesian with a cos and a sin to move, converting in a loop, both
 * ways, thousands of times a frame. Construction still accepts polar
 * Vectors, because that is the natural way to describe emission.
 *
 * Optional extras, all off by default so a plain particle behaves exactly
 * as before:
 *
 *   size         grows or shrinks from a start size to an end size
 *   end colour   blends from the spawn colour to another over its life
 *   height       a z above the ground with its own vertical speed, for
 *                top-down games: sparks arc up and fall back, blood lands
 */

#ifndef PARTICLE_HPP
#define PARTICLE_HPP

#include "../Includes.hpp"

class Particle
{
private:
    sf::Vector2f m_pos;
    sf::Vector2f m_prev_pos;      // for drawing motion-blur style lines

    sf::Vector2f m_vel;           // units per second
    sf::Vector2f m_acc;           // units per second squared

    sf::Color m_color;
    sf::Color m_base_color;       // fade multiplies against this

    sf::Time m_lifespan;
    sf::Time m_elapsed_time;

    bool m_fade{false};

    float     m_size{1.0f};
    float     m_end_size{1.0f};
    sf::Color m_end_color;
    bool      m_blend_color{false};

    float m_height{0.0f};         // above the ground
    float m_vz{0.0f};             // units per second, up is positive
    bool  m_landed{false};

    void refresh_color();

public:
    Particle(sf::Vector2f position, sf::Color color, sf::Time lifespan);

    void update(sf::Time delta_time);

    void set_fade(bool fade);
    void toggle_fade();

    void add_velocity(sf::Vector2f velocity);
    void add_velocity(Vector velocity);          // polar convenience
    void set_velocity(sf::Vector2f velocity);
    void set_velocity(Vector velocity);

    void add_acceleration(sf::Vector2f acceleration);
    void add_acceleration(Vector acceleration);
    void set_acceleration(sf::Vector2f acceleration);

    void set_lifespan(sf::Time lifespan);
    void set_position(sf::Vector2f position);

    /*
     * Scales velocity, used by the drag model.
     */
    void scale_velocity(float factor);

    /*
     * Size in units, from birth to death. Only drawn by systems whose
     * shape is SQUARE; LINE particles are always one pixel wide.
     */
    void set_size(float start, float end);

    /*
     * Blends from the spawn colour to `end` over the particle's life.
     * Fading, if on, still applies on top.
     */
    void set_end_color(sf::Color end);

    /*
     * Height above the ground and vertical speed. Height only moves when
     * the owning system has height gravity switched on.
     */
    void set_height(float height, float vertical_speed);
    void update_height(float gravity, float dt);

    [[nodiscard]] bool is_expired() const;

    /*
     * Fraction of life elapsed, 0 at birth and 1 at death.
     */
    [[nodiscard]] float get_life_ratio() const;

    [[nodiscard]] sf::Vector2f get_position() const;
    [[nodiscard]] sf::Vector2f get_prev_position() const;
    [[nodiscard]] sf::Vector2f get_velocity() const;
    [[nodiscard]] sf::Vector2f get_acceleration() const;
    [[nodiscard]] sf::Color    get_color() const;
    [[nodiscard]] sf::Time     get_lifespan() const;
    [[nodiscard]] sf::Time     get_elapsed_time() const;
    [[nodiscard]] bool         fades() const;
    [[nodiscard]] float        get_size() const;
    [[nodiscard]] float        get_height() const;
    [[nodiscard]] float        get_vertical_speed() const;
    [[nodiscard]] bool         has_landed() const;
};

#endif
