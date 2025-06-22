/**
 * Particle.hpp
 *
 * Written by James Bouchat
 */

#ifndef PARTICLE_HPP
#define PARTICLE_HPP

#include "../Includes.hpp"

struct Vector
{
    float magnitude;
    sf::Angle direction;
};

class Particle
{
private:
    sf::Vector2f m_pos;
    sf::Vector2f m_prev_pos;

    Vector m_vel;
    Vector m_acc;

    sf::Color m_color;

    bool m_fades;
    unsigned int m_fade_time;

    Vector add_vectors(Vector v1, Vector v2);

public:
    Particle(sf::Vector2f position, sf::Color color);

    void update(sf::Time delta_time);

    void add_velocity(Vector vector);
    void add_acceleration(Vector vector);
    void add_fade_time(unsigned int fade_time);

    sf::Vector2f get_position() const;
    sf::Vector2f get_prev_position() const;
    Vector get_velocity() const;
    Vector get_acceleration() const;
    sf::Color get_color() const;
    bool fades() const;
    unsigned int get_fade_time() const;
};

#endif
