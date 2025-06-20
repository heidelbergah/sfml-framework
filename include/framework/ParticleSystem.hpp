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
    sf::Vector2f m_force;

    bool m_gravity_on = false;
    float m_gravity = 9.3;

    void draw(sf::RenderTarget &target, sf::RenderStates states) const override;

public:
    ParticleSystem(sf::Vector2f pos, int particles);

    void update(sf::Time delta_time);

    void set_position(sf::Vector2f pos);
    void add_particles(int count);
    void toggle_gravity();
    void set_gravity(float g);
    void apply_force(sf::Vector2f force);

    /**
     * Add whole bunch of attribute changing functions
     * Be fun and creative, let the user change a whole lot
     * about these particles!
     */
};

#endif
