/**
 * ParticleSystemManager.cpp
 *
 * Written by James Bouchat
 */

#include "../../include/framework/ParticleSystemManager.hpp"

ParticleSystemManager::ParticleSystemManager(sf::Vector2f camera_pos) :
    m_render_texture({WINDOW_WIDTH, WINDOW_HEIGHT}),
    m_sprite(m_render_texture.getTexture())
{
    m_pos = camera_pos;
    m_sprite.setPosition(m_pos);
    original_pos = m_pos;
}

void ParticleSystemManager::draw(sf::RenderTarget &target, sf::RenderStates states) const
{
    m_render_texture.clear(sf::Color::Transparent);
    for(const auto [key, system] : m_particle_systems)
    {
        m_render_texture.draw(*system);
    }
    m_render_texture.display();
    m_sprite.setTexture(m_render_texture.getTexture());
    target.draw(m_sprite, states);
}

void ParticleSystemManager::update(sf::Time delta_time)
{
    for(auto& [id, ps] : m_particle_systems)
    {
        ps->update(delta_time);
    }
}

void ParticleSystemManager::sync_with_view(sf::Vector2f camera_pos)
{
    if(camera_pos != m_pos)
    {
        sf::Vector2f d = m_pos - camera_pos;
        m_pos = camera_pos;
        for(auto& [id, ps] : m_particle_systems)
        {
            ps->set_position(ps->get_position() + d);
        }
        m_sprite.setPosition(m_pos);
    }
}

void ParticleSystemManager::add_particle_system(std::string key, ParticleSystem ps)
{
    std::shared_ptr p_ps = std::make_shared<ParticleSystem>(std::move(ps));
    m_particle_systems[key] = p_ps;
}

void ParticleSystemManager::remove_particle_system(std::string key)
{
    m_particle_systems.erase(key);
}

void ParticleSystemManager::set_particle_system_position(std::string key, sf::Vector2f pos)
{
    m_particle_systems[key]->set_position(pos + (m_pos - original_pos));
}

std::shared_ptr<ParticleSystem> ParticleSystemManager::get_particle_system(std::string key)
{
    return m_particle_systems[key];
}

std::vector<std::shared_ptr<ParticleSystem>> ParticleSystemManager::get_particle_systems()
{
    std::vector<std::shared_ptr<ParticleSystem>> particle_systems;

    for(auto& [id, ps] : m_particle_systems)
    {
        particle_systems.emplace_back(ps);
    }

    return particle_systems;
}


std::vector<sf::Vector2f> ParticleSystemManager::get_global_particle_positions(std::string key)
{
    std::vector<sf::Vector2f> positions;

    for(Particle &p : m_particle_systems[key]->get_particles())
    {
        positions.push_back(p.get_position() - (m_pos - original_pos));
    }

    return positions;
}
