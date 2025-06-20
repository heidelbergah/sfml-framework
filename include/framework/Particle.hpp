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

    sf::Vector2f m_vel;
    sf::Vector2f m_acc;
    sf::Color m_color;

    bool m_fades;
    unsigned int m_fade_time;

public:
    Particle(sf::Vector2f position, sf::Color color);

    void update(sf::Time delta_time);

    void add_velocity(sf::Vector2f velocity);
    void add_acceleration(sf::Vector2f acceleration);
    void add_fade_time(unsigned int fade_time);

    sf::Vector2f get_position() const;
    sf::Vector2f get_prev_position() const;
    sf::Vector2f get_velocity() const;
    sf::Vector2f get_acceleration() const;
    sf::Color get_color() const;
    bool fades() const;
    unsigned int get_fade_time() const;
};

#endif
