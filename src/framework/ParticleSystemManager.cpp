/**
 * ParticleSystemManager.cpp
 *
 * Written by James Bouchat
 */

#include "../../include/framework/ParticleSystemManager.hpp"

void ParticleSystemManager::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    // By reference. The old loop was `for(const auto [key, system] : ...)`,
    // copying a std::string and bumping a shared_ptr refcount for every
    // system, every frame.
    for(const auto& [key, system] : m_particle_systems)
    {
        if(system) target.draw(*system, states);
    }
}

void ParticleSystemManager::update(sf::Time delta_time)
{
    for(auto& [key, system] : m_particle_systems)
    {
        if(system) system->update(delta_time);
    }
}

void ParticleSystemManager::add_particle_system(const std::string& key,
                                                ParticleSystem system)
{
    m_particle_systems[key] = std::make_shared<ParticleSystem>(std::move(system));
}

void ParticleSystemManager::add_particle_system(const std::string& key,
                                                std::shared_ptr<ParticleSystem> system)
{
    if(!system) return;
    m_particle_systems[key] = std::move(system);
}

void ParticleSystemManager::remove_particle_system(const std::string& key)
{
    m_particle_systems.erase(key);
}

void ParticleSystemManager::clear()
{
    m_particle_systems.clear();
}

std::shared_ptr<ParticleSystem>
ParticleSystemManager::get_particle_system(const std::string& key) const
{
    auto it = m_particle_systems.find(key);
    return (it == m_particle_systems.end()) ? nullptr : it->second;
}

bool ParticleSystemManager::has_particle_system(const std::string& key) const
{
    return m_particle_systems.find(key) != m_particle_systems.end();
}

void ParticleSystemManager::set_particle_system_position(const std::string& key,
                                                         sf::Vector2f pos)
{
    // World space directly, no translation between the manager's own
    // origin and the camera, because there no longer is one.
    if(auto system = get_particle_system(key)) system->set_position(pos);
}

std::vector<sf::Vector2f>
ParticleSystemManager::get_particle_positions(const std::string& key) const
{
    std::vector<sf::Vector2f> positions;

    auto system = get_particle_system(key);
    if(!system) return positions;

    const auto& particles = system->get_particles();
    positions.reserve(particles.size());

    for(const Particle& p : particles)
        positions.push_back(p.get_position());

    return positions;
}

std::size_t ParticleSystemManager::total_particle_count() const
{
    std::size_t total = 0;
    for(const auto& [key, system] : m_particle_systems)
    {
        if(system) total += system->particle_count();
    }
    return total;
}

std::size_t ParticleSystemManager::size() const
{
    return m_particle_systems.size();
}
