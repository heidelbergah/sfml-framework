/**
 * ParticleSystemManager.cpp
 *
 * Written by James Bouchat
 */

#include "../../include/framework/ParticleSystemManager.hpp"

ParticleSystemManager::ParticleSystemManager() {}

void ParticleSystemManager::update(sf::Time delta_time)
{
    for(auto& [id, ps] : m_particle_systems)
    {
        ps->update(delta_time);
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
