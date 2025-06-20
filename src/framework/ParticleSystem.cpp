#include "../../include/framework/ParticleSystem.hpp"
    
void ParticleSystem::draw(sf::RenderTarget &target, sf::RenderStates states) const
{
    target.draw(m_particle_lines, states);
}

ParticleSystem::ParticleSystem(sf::Vector2f pos, int particles)
{
    srand(time(NULL));

    m_particle_lines.setPrimitiveType(sf::PrimitiveType::Lines);
    set_position(pos);
    add_particles(particles);
}

void ParticleSystem::update(sf::Time delta_time)
{
    for(Particle& p : m_particles)
    {
        p.update(delta_time);
    }

    int index_count = 0;
    for(const Particle& p : m_particles)
    {
        m_particle_lines[index_count].position = p.get_prev_position();
        index_count++;
        m_particle_lines[index_count].position = p.get_position();
        index_count++;
    }
}

void ParticleSystem::set_position(sf::Vector2f pos)
{
    m_pos = pos;
}

void ParticleSystem::add_particles(int count)
{
    for(int i = 0; i < count; ++i)
    {
        int r = rand() % 255;
        int g = rand() % 255;
        int b = rand() % 255;
        
        int x_force = (rand() % 10) - 5;
        int y_force = (rand() % 10) - 5;
        
        Particle p(m_pos, sf::Color(r, g, b));
        p.add_velocity(sf::Vector2f(x_force, y_force));
        m_particles.push_back(p);
    }

    m_particle_lines.resize(m_particles.size()*2);
}

void ParticleSystem::toggle_gravity()
{
    m_gravity_on = !m_gravity_on;
    if(m_gravity_on)
        m_force.y += m_gravity;
    else
        m_force.y -= m_gravity;
}

void ParticleSystem::set_gravity(float g)
{
    if(m_gravity_on)
    {
        // If gravity on, reset the current force applied
        // to reflect the new value
        m_force.y -= m_gravity;
        m_gravity = g;
        m_force.y += m_gravity;
    }
    else
    {
        m_gravity = g;
    }
}

void ParticleSystem::apply_force(sf::Vector2f force)
{
    m_force += force;
}

