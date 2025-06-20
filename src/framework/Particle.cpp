#include "../../include/framework/Particle.hpp"

Particle::Particle(sf::Vector2f position, sf::Color color)
{
    m_pos = position;
    m_prev_pos = position;
    m_color = color;

    m_vel = sf::Vector2f(0, 0);
    m_acc = sf::Vector2f(0, 0);
    m_fades = false;
    m_fade_time = 0;
}

void Particle::update(sf::Time delta_time)
{
    // Note previous position
    m_prev_pos = m_pos;

    // Update position and velocity
    m_vel += m_acc;
    m_pos += m_vel;

    if(m_fades)
    {
        // Perform alpha dampening bs
    }
}

void Particle::add_velocity(sf::Vector2f velocity)
{
    m_vel += velocity;
}

void Particle::add_acceleration(sf::Vector2f acceleration)
{
    m_acc += acceleration;
}

void Particle::add_fade_time(unsigned int fade_time)
{
    m_fades = true;
    m_fade_time += fade_time;
}

sf::Vector2f Particle::get_position() const
{
    return m_pos;
}

sf::Vector2f Particle::get_prev_position() const
{
    return m_prev_pos;
}

sf::Vector2f Particle::get_velocity() const
{
    return m_vel;
}

sf::Vector2f Particle::get_acceleration() const
{
    return m_acc;
}

sf::Color Particle::get_color() const
{
    return m_color;
}
    
bool Particle::fades() const
{
    return m_fades;
}

unsigned int Particle::get_fade_time() const
{
    return m_fade_time;
}

