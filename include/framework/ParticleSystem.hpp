/**
 * ParticleSystem.hpp
 *
 * A pool of particles sharing an emitter position and physics settings.
 *
 * Particles live in world space and are drawn straight into the target
 * with the active view. The old version rendered into a screen-sized
 * RenderTexture positioned in world space and then shifted every
 * emitter whenever the camera moved, which meant three different
 * coordinate spaces and a hand-rolled reimplementation of sf::View.
 */

#ifndef PARTICLE_SYSTEM_HPP
#define PARTICLE_SYSTEM_HPP

#include "Particle.hpp"

class ParticleSystem : public sf::Drawable
{
private:
    std::vector<Particle> m_particles;
    sf::VertexArray       m_particle_lines{sf::PrimitiveType::Lines};

    sf::Vector2f m_pos;             // emitter, in world space

    bool         m_fade{false};
    bool         m_gravity_on{false};
    sf::Vector2f m_gravity{0.0f, 200.0f};   // units/sec^2, downward

    sf::Time m_lifespans{sf::seconds(1.0f)};

    // Fraction of speed retained per second. 1.0 is frictionless.
    float m_drag{0.5f};

    std::size_t m_max_particles{20000};

    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

public:
    explicit ParticleSystem(sf::Vector2f pos = {});

    void update(sf::Time delta_time);

    /*
     * Spawns `count` particles at the emitter.
     *
     * `velocity` is polar: magnitude in units per second, direction in
     * degrees with 0 pointing right and 90 pointing up. `spread_degrees`
     * randomises the direction by +/- that amount; 0 means a straight
     * line. Passing 0 used to compute rand() % 0, which is undefined
     * and traps on x86.
     */
    void add_particles(int count, sf::Color color,
                       Vector velocity = {0.0f, sf::degrees(0.0f)},
                       float spread_degrees = 0.0f);

    /*
     * Randomises speed as well, drawn from a normal distribution.
     */
    void add_particles_varied(int count, sf::Color color, Vector velocity,
                              float spread_degrees, float speed_stddev);

    void apply_force(sf::Vector2f force);
    void apply_force(Vector force);

    void set_position(sf::Vector2f pos);
    void set_gravity(sf::Vector2f gravity);
    void set_gravity(Vector gravity);
    void toggle_gravity();
    void set_gravity_enabled(bool enabled);

    void toggle_fade();
    void set_fade(bool fade);

    /*
     * Fraction of speed retained per second, in (0, 1]. 0.5 halves a
     * particle's speed every second. Note this is per second, not per
     * frame -- the old 0.99-per-frame default worked out to roughly
     * 0.55 per second at 60fps.
     */
    void set_drag(float retained_per_second);

    void set_lifespan(sf::Time lifespan);

    /*
     * Hard cap, so a runaway emitter cannot exhaust memory.
     */
    void set_max_particles(std::size_t max);

    void clear();

    [[nodiscard]] sf::Vector2f get_position() const;

    /*
     * By reference -- the old version returned a copy of the whole
     * vector, so asking for particle positions duplicated every
     * particle in the system.
     */
    [[nodiscard]] const std::vector<Particle>& get_particles() const;

    [[nodiscard]] std::size_t          particle_count() const;
    [[nodiscard]] const sf::VertexArray& get_particle_lines() const;
};

#endif
