#include "../../include/framework/ParticleSystem.hpp"
    
void ParticleSystem::draw(sf::RenderTarget &target, sf::RenderStates states) const
{
    m_render_texture.clear(sf::Color::Transparent);
    m_render_texture.draw(m_particle_lines);
    m_render_texture.display();
    m_sprite.setTexture(m_render_texture.getTexture());

    std::cout << "POS: " << m_sprite.getPosition().x << ", " << m_sprite.getPosition().y << std::endl;
    std::cout << "SIZE: " << m_render_texture.getSize().x << ", " << m_render_texture.getSize().y << std::endl;

    target.draw(m_sprite, states);
}

void ParticleSystem::determine_texture_size(sf::Vector2u &min, sf::Vector2u &max) const
{
    min.x = min.y = 999999999.f;
    max.x = max.y = 0;

    for(int i = 0; i < m_particle_lines.getVertexCount(); ++i)
    {
        sf::Vector2f particle = m_particle_lines[i].position;

        if(particle.x > max.x) max.x = particle.x;
        else if(particle.x < min.x) min.x = particle.x; 
        
        if(particle.y > max.y) max.y = particle.y;
        else if(particle.y < min.y) min.y = particle.y; 
    }
}

void ParticleSystem::update_particle_positions(sf::Vector2u offset)
{
    for(int i = 0; i < m_particle_lines.getVertexCount(); ++i)
    {
        m_particle_lines[i].position.x -= offset.x;
        m_particle_lines[i].position.y -= offset.y;
    }
}

ParticleSystem::ParticleSystem(sf::Vector2f pos) :
    m_render_texture({1920, 1080}),
    m_sprite(m_render_texture.getTexture())
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

    // Update rendering data
    sf::Vector2u min = {9999999, 9999999};
    sf::Vector2u max = {0, 0};

    unsigned padding = 20; // value added to all faces of render texture

    determine_texture_size(min, max);

    max.x += padding * 2;
    max.y += padding * 2;

    min.x -= padding;
    min.y -= padding;

    update_particle_positions(min);

    m_render_texture.clear(sf::Color::Transparent);
    if(m_particles.size() != 0)
    {
        if(!m_render_texture.resize(max))
        {
            std::cerr << "COULD NOT RESIZE PARTICLE SYSTEM RENDER TEXTURE" << std::endl;
        }
    }
    else
    {
        if(!m_render_texture.resize({5, 5}))
        {
            std::cerr << "COULD NOT RESIZE PARTICLE SYSTEM RENDER TEXTURE" << std::endl;
        }
    }
    m_sprite.setPosition(sf::Vector2f{min});

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
