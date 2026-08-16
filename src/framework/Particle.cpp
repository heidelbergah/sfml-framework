/**
 * Particle.cpp
 *
 * Written by James Bouchat
 */

#include "../../include/framework/Particle.hpp"

Particle::Particle(sf::Vector2f position, sf::Color color, sf::Time lifespan)
    : m_pos(position),
      m_prev_pos(position),
      m_color(color),
      m_base_color(color),
      m_lifespan(lifespan),
      m_elapsed_time(sf::Time::Zero)
{}

void Particle::update(sf::Time delta_time)
{
    float const dt = delta_time.asSeconds();

    m_prev_pos = m_pos;

    // Semi-implicit Euler. Everything is scaled by dt, so speeds are in
    // units per second and behave identically at 30, 60 or 144 fps --
    // the old version added raw magnitudes per frame, which made every
    // tuned value silently framerate-dependent.
    m_vel += m_acc * dt;
    m_pos += m_vel * dt;

    // Accumulated as sf::Time, which is microsecond-resolution. The old
    // code round-tripped through asMilliseconds(), truncating a 16.67ms
    // frame to 16 and making particles outlive their lifespan by ~4%.
    m_elapsed_time += delta_time;

    if(m_fade)
    {
        float const ratio = std::clamp(1.0f - get_life_ratio(), 0.0f, 1.0f);
        m_color.a = static_cast<std::uint8_t>(m_base_color.a * ratio);
    }
}

void Particle::set_fade(bool fade)
{
    m_fade = fade;
    if(!m_fade) m_color.a = m_base_color.a;
}

void Particle::toggle_fade()
{
    set_fade(!m_fade);
}

void Particle::add_velocity(sf::Vector2f velocity)
{
    m_vel += velocity;
}

void Particle::add_velocity(Vector velocity)
{
    m_vel += to_cartesian(velocity);
}

void Particle::set_velocity(sf::Vector2f velocity)
{
    m_vel = velocity;
}

void Particle::set_velocity(Vector velocity)
{
    m_vel = to_cartesian(velocity);
}

void Particle::add_acceleration(sf::Vector2f acceleration)
{
    m_acc += acceleration;
}

void Particle::add_acceleration(Vector acceleration)
{
    m_acc += to_cartesian(acceleration);
}

void Particle::set_acceleration(sf::Vector2f acceleration)
{
    m_acc = acceleration;
}

void Particle::set_lifespan(sf::Time lifespan)
{
    m_lifespan = lifespan;
}

void Particle::set_position(sf::Vector2f position)
{
    m_pos      = position;
    m_prev_pos = position;
}

void Particle::scale_velocity(float factor)
{
    m_vel *= factor;
}

bool Particle::is_expired() const
{
    return m_elapsed_time >= m_lifespan;
}

float Particle::get_life_ratio() const
{
    if(m_lifespan <= sf::Time::Zero) return 1.0f;
    return m_elapsed_time.asSeconds() / m_lifespan.asSeconds();
}

sf::Vector2f Particle::get_position() const      { return m_pos; }
sf::Vector2f Particle::get_prev_position() const { return m_prev_pos; }
sf::Vector2f Particle::get_velocity() const      { return m_vel; }
sf::Vector2f Particle::get_acceleration() const  { return m_acc; }
sf::Color    Particle::get_color() const         { return m_color; }
sf::Time     Particle::get_lifespan() const      { return m_lifespan; }
sf::Time     Particle::get_elapsed_time() const  { return m_elapsed_time; }
bool         Particle::fades() const             { return m_fade; }
