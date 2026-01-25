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
    
    sf::Vector2f m_pos;

    void draw(sf::RenderTarget &target, sf::RenderStates states) const override;

public:
    ParticleSystemManager(sf::Vector2f camera_pos);

    void update(sf::Time delta_time, sf::Vector2f camera_pos);

    void add_particle_system(std::string key, ParticleSystem ps);
    void remove_particle_system(std::string key);

    void set_particle_system_position(std::string key, sf::Vector2f pos);

    std::shared_ptr<ParticleSystem> get_particle_system(std::string key);

    std::vector<std::shared_ptr<ParticleSystem>> get_particle_systems();
};

#endif
