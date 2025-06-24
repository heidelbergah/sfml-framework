/**
 * Particle.hpp
 *
 * Written by James Bouchat
 */

#ifndef PARTICLE_HPP
#define PARTICLE_HPP

#include "../Includes.hpp"

class Particle
{
private:
    sf::Vector2f m_pos;
    sf::Vector2f m_prev_pos;

    Vector m_vel;
    Vector m_acc;

    sf::Color m_color;

    sf::Time m_lifespan;
    sf::Time m_elapsed_time;

    bool m_fade = false;

public:
    Particle(sf::Vector2f position, sf::Color color, sf::Time lifespan);

    void update(sf::Time delta_time);
    void toggle_fade();

    void add_velocity(Vector vector);
    void add_acceleration(Vector vector);

    void set_velocity(Vector v);

    sf::Vector2f get_position() const;
    sf::Vector2f get_prev_position() const;
    Vector get_velocity() const;
    Vector get_acceleration() const;
    sf::Color get_color() const;
    sf::Time get_lifespan() const;
    sf::Time get_elapsed_time() const;
    bool fades() const;
};

#endif
