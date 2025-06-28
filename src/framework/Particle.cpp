#include "../../include/framework/Particle.hpp"

Particle::Particle(sf::Vector2f position, sf::Color color, sf::Time lifespan)
{
    m_pos = position;
    m_prev_pos = position;
    m_color = color;

    Vector v;
    v.magnitude = 0;
    v.direction = sf::degrees(0);

    m_vel = v;
    m_acc = v;

    m_lifespan = lifespan;
    m_elapsed_time = sf::milliseconds(0);
}

void Particle::update(sf::Time delta_time)
{
    // Note previous position
    m_prev_pos = m_pos;

    // Add acceleration to velocity
    add_velocity(m_acc);

    // Update position with velocity
    m_pos.x += cos(m_vel.direction.asRadians()) * m_vel.magnitude;
    m_pos.y -= sin(m_vel.direction.asRadians()) * m_vel.magnitude;

    m_elapsed_time = sf::milliseconds(m_elapsed_time.asMilliseconds() + delta_time.asMilliseconds());

    if(m_fade)
    {
        float time_diff = m_lifespan.asMilliseconds() - m_elapsed_time.asMilliseconds();
        float ratio = time_diff / m_lifespan.asMilliseconds();
        m_color.a = 255 * ratio;
    }
}

void Particle::toggle_fade()
{
    m_fade = !m_fade;
}

void Particle::add_velocity(Vector velocity)
{
    m_vel = add_vectors(m_vel, velocity);
}

void Particle::add_acceleration(Vector acceleration)
{
    m_acc = add_vectors(m_acc, acceleration);
}

void Particle::set_velocity(Vector v)
{
    m_vel = v;
}

void Particle::set_lifespan(sf::Time lifespan)
{
    m_lifespan = lifespan;
}

sf::Vector2f Particle::get_position() const
{
    return m_pos;
}

sf::Vector2f Particle::get_prev_position() const
{
    return m_prev_pos;
}

Vector Particle::get_velocity() const
{
    return m_vel;
}

Vector Particle::get_acceleration() const
{
    return m_acc;
}

sf::Color Particle::get_color() const
{
    return m_color;
}

sf::Time Particle::get_lifespan() const
{
    return m_lifespan;
}

sf::Time Particle::get_elapsed_time() const
{
    return m_elapsed_time;
}

bool Particle::fades() const
{
    return m_fade;
}

