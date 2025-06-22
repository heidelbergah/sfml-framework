#include "../../include/framework/Particle.hpp"

Vector Particle::add_vectors(Vector v1, Vector v2)
{
    float r1 = v1.magnitude, r2 = v2.magnitude;
    float p1 = v1.direction.asRadians();
    float p2 = v2.direction.asRadians();

    float p_diff = p2 - p1;
    
    float r = sqrt((r1 * r1) + (r2 * r2) + (2 * r1 * r2 * cos(p_diff)));

    sf::Angle p;
    float y = r2 * sin(p_diff);
    float x = r1 + (r2 * cos(p_diff));

    p = sf::radians(p1 + atan2(y, x));

    Vector vector;
    vector.magnitude = r;
    vector.direction = p;
    return vector;
}

Particle::Particle(sf::Vector2f position, sf::Color color)
{
    m_pos = position;
    m_prev_pos = position;
    m_color = color;

    Vector v;
    v.magnitude = 0;
    v.direction = sf::degrees(0);

    m_vel = v;
    m_acc = v;

    m_fades = false;
    m_fade_time = 0;
}

void Particle::update(sf::Time delta_time)
{
    // Note previous position
    m_prev_pos = m_pos;

    // Add acceleration to velocity
    add_velocity(m_acc);

    std::cout << m_vel.magnitude << ", " << m_vel.direction.asDegrees() << std::endl;

    // Update position with velocity
    m_pos.x += cos(m_vel.direction.asRadians()) * m_vel.magnitude;
    m_pos.y -= sin(m_vel.direction.asRadians()) * m_vel.magnitude;

    if(m_fades)
    {
        // Perform alpha dampening bs
    }
}

void Particle::add_velocity(Vector velocity)
{
    m_vel = add_vectors(m_vel, velocity);
}

void Particle::add_acceleration(Vector acceleration)
{
    m_acc = add_vectors(m_acc, acceleration);
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
    
bool Particle::fades() const
{
    return m_fades;
}

unsigned int Particle::get_fade_time() const
{
    return m_fade_time;
}

