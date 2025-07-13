/**
 * ParticleSystem.hpp
 *
 * Written by James Bouchat
 */

#ifndef PARTICLE_SYSTEM_HPP
#define PARTICLE_SYSTEM_HPP

#include "Particle.hpp"

class ParticleSystem : public sf::Drawable
{
private:
    std::vector<Particle> m_particles;
    
    sf::VertexArray m_particle_lines;

    sf::Vector2f m_pos;

    bool m_fade = false;
    bool m_gravity_on = false;
    Vector m_gravity = {0.05, sf::degrees(270)};

    sf::Time m_lifespans = sf::milliseconds(1000);

    float m_drag = 0.99; // (0.0 - 1.0); lower is stronger

    void draw(sf::RenderTarget &target, sf::RenderStates states) const override;

public:
    ParticleSystem(sf::Vector2f pos);

    void update(sf::Time delta_time);

    sf::VertexArray get_particle_lines() const;

    void set_position(sf::Vector2f pos);
    void add_particles(int count, sf::Color c, Vector v, int randomness);
    void toggle_gravity();
    void toggle_fade();
    void set_gravity(Vector g);
    void set_drag(float drag);
    void apply_force(Vector force);
    void set_lifespan(sf::Time lifespan);

    /**
     * Add whole bunch of attribute changing functions
     * Be fun and creative, let the user change a whole lot
     * about these particles!
     */
};

#endif
