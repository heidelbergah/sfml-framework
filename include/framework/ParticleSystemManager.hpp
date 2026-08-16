/**
 * ParticleSystemManager.hpp
 *
 * Keyed collection of particle systems.
 *
 * Everything is world space now. The manager no longer owns a
 * RenderTexture, no longer positions a sprite in the world, and no
 * longer shifts emitters to compensate for camera movement -- sf::View
 * already does all of that. Bloom moved to a post-process over the
 * finished frame, which is where a full-screen effect belongs.
 */

#ifndef PARTICLE_SYSTEM_MANAGER_HPP
#define PARTICLE_SYSTEM_MANAGER_HPP

#include "ParticleSystem.hpp"

class ParticleSystemManager : public sf::Drawable
{
private:
    std::unordered_map<std::string, std::shared_ptr<ParticleSystem>> m_particle_systems;

    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

public:
    ParticleSystemManager() = default;

    void update(sf::Time delta_time);

    void add_particle_system(const std::string& key, ParticleSystem system);
    void add_particle_system(const std::string& key,
                             std::shared_ptr<ParticleSystem> system);
    void remove_particle_system(const std::string& key);
    void clear();

    /*
     * Returns nullptr for an unknown key. The old version used
     * map::operator[], which inserted a null shared_ptr on a typo and
     * then handed it back to be dereferenced.
     */
    [[nodiscard]] std::shared_ptr<ParticleSystem>
        get_particle_system(const std::string& key) const;

    [[nodiscard]] bool has_particle_system(const std::string& key) const;

    /*
     * No-ops if the key is unknown.
     */
    void set_particle_system_position(const std::string& key, sf::Vector2f pos);

    /*
     * World-space positions of every particle in a system. Empty if the
     * key is unknown.
     */
    [[nodiscard]] std::vector<sf::Vector2f>
        get_particle_positions(const std::string& key) const;

    [[nodiscard]] std::size_t total_particle_count() const;
    [[nodiscard]] std::size_t size() const;
};

#endif
