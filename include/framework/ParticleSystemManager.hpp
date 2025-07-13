/**
 * ParticleSystemManager.hpp
 *
 * Written by James Bouchat
 */

#ifndef PARTICLE_SYSTEM_MANAGER_HPP
#define PARTICLE_SYSTEM_MANAGER_HPP

#include "ParticleSystem.hpp"

class ParticleSystemManager : public sf::Drawable
{
private:

    mutable sf::RenderTexture m_render_texture;
    mutable sf::Sprite m_sprite;
    std::unordered_map<std::string, std::shared_ptr<ParticleSystem>> m_particle_systems;
    
    void draw(sf::RenderTarget &target, sf::RenderStates states) const override;

public:
    ParticleSystemManager();

    void update(sf::Time delta_time);

    void add_particle_system(std::string key, ParticleSystem ps);
    void remove_particle_system(std::string key);

    std::shared_ptr<ParticleSystem> get_particle_system(std::string key);

    std::vector<std::shared_ptr<ParticleSystem>> get_particle_systems();
};

#endif
