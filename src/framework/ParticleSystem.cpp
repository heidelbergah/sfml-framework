#include "../../include/framework/ParticleSystem.hpp"
    
void ParticleSystem::draw(sf::RenderTarget &target, sf::RenderStates states) const
{
    target.draw(m_particle_lines, states);
}

ParticleSystem::ParticleSystem(sf::Vector2f pos)
{
    srand(time(NULL));

    m_particle_lines.setPrimitiveType(sf::PrimitiveType::Lines);
    set_position(pos);
}

void ParticleSystem::update(sf::Time delta_time)
{
    int index = 0;
    for(Particle& p : m_particles)
    {
        if(m_gravity_on)
            p.add_velocity(m_gravity);

        Vector new_vel = p.get_velocity();
        new_vel.magnitude *= m_drag;
        p.set_velocity(new_vel);
        
        p.update(delta_time);
        if(p.get_elapsed_time().asMilliseconds() > p.get_lifespan().asMilliseconds())
        {
            m_particles.erase(m_particles.begin() + index);
        }
        index++;
    }

    int index_count = 0;
    m_particle_lines.resize(m_particles.size()*2);
    for(const Particle& p : m_particles)
    {
        m_particle_lines[index_count].position = p.get_prev_position();
        m_particle_lines[index_count].color = p.get_color();
        index_count++;
        m_particle_lines[index_count].position = p.get_position();
        m_particle_lines[index_count].color = p.get_color();
        index_count++;
    }
}

sf::VertexArray ParticleSystem::get_particle_lines() const
{
    return m_particle_lines;
}

void ParticleSystem::set_position(sf::Vector2f pos)
{
    m_pos = pos;
}

void ParticleSystem::add_particles(int count, sf::Color c, Vector v = {0, sf::degrees(0)}, int randomness = 0)
{
    for(int i = 0; i < count; ++i)
    {
        float magnitude = v.magnitude;
        float direction = v.direction.asDegrees() + ((rand() % (randomness*2)) - randomness);

        Particle p(m_pos, c, m_lifespans);

        Vector v;
        v.magnitude = magnitude;
        v.direction = sf::degrees(direction);

        if(m_fade)
            p.toggle_fade();

        p.add_velocity(v);
        m_particles.push_back(p);
    }
}

void ParticleSystem::toggle_gravity()
{
    m_gravity_on = !m_gravity_on;
}

void ParticleSystem::toggle_fade()
{
    m_fade = !m_fade;
    for(Particle& p : m_particles)
    {
        p.toggle_fade();
    }
}

void ParticleSystem::set_gravity(Vector g)
{
    m_gravity = g;
}

void ParticleSystem::set_drag(float drag)
{
    m_drag = drag;
}

void ParticleSystem::apply_force(Vector force)
{
    for(Particle& p : m_particles)
    {
        p.add_velocity(force);
    }
}

void ParticleSystem::set_lifespan(sf::Time lifespan)
{
    m_lifespans = lifespan;
}
