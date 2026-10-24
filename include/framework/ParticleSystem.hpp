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
 *
 * By default each particle is a one pixel line from where it was to where
 * it is, a little motion streak. A system can instead draw them as
 * squares that grow, shrink and change colour over their lives, blend
 * them additively for fire and light, and give them a height above the
 * ground, so a top-down game can throw sparks up and catch the moment
 * each one lands.
 *
 *   ParticleSystem blood;
 *   blood.set_shape(ParticleShape::SQUARE);
 *   blood.set_height_gravity(260.0f);
 *   blood.set_on_land([&](const Particle& p) { stain(p.get_position()); });
 *   blood.emit(at, velocity, sf::seconds(2.0f), red, 1.0f, 1.0f, 3.0f, 40.0f);
 */

#ifndef PARTICLE_SYSTEM_HPP
#define PARTICLE_SYSTEM_HPP

#include "Particle.hpp"

enum class ParticleShape
{
    LINE,       // a streak from the last position to this one (the default)
    SQUARE,     // a filled square of the particle's size
};

class ParticleSystem : public sf::Drawable
{
public:
    using LandCallback = std::function<void(const Particle&)>;

private:
    std::vector<Particle> m_particles;
    sf::VertexArray       m_particle_lines{sf::PrimitiveType::Lines};

    ParticleShape m_shape{ParticleShape::LINE};
    sf::BlendMode m_blend{sf::BlendAlpha};
    bool          m_pixel_snap{false};

    float        m_height_gravity{0.0f};    // 0 = heights stay where they are
    LandCallback m_on_land;
    bool         m_remove_on_land{true};

    sf::Vector2f m_pos;             // emitter, in world space

    bool         m_fade{false};
    bool         m_gravity_on{false};
    sf::Vector2f m_gravity{0.0f, 200.0f};   // units/sec^2, downward

    sf::Time m_lifespans{sf::seconds(1.0f)};

    // Fraction of speed retained per second. 1.0 is frictionless.
    float m_drag{0.5f};

    std::size_t m_max_particles{20000};

    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;
    void rebuild_vertices();

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

    /*
     * One particle anywhere, with everything spelled out. This is the one
     * to use when effects come from many places at once: there is no need
     * to move the emitter for each. Returns false if the pool is full.
     *
     * `height` and `vertical_speed` only matter with height gravity on.
     */
    bool emit(sf::Vector2f position, sf::Vector2f velocity, sf::Time lifespan, sf::Color color,
              float size = 1.0f, float end_size = 1.0f,
              float height = 0.0f, float vertical_speed = 0.0f);

    /*
     * The same, blending to `end_color` over its life.
     */
    bool emit(sf::Vector2f position, sf::Vector2f velocity, sf::Time lifespan,
              sf::Color color, sf::Color end_color,
              float size = 1.0f, float end_size = 1.0f,
              float height = 0.0f, float vertical_speed = 0.0f);

    void set_shape(ParticleShape shape);

    /*
     * How particles are blended into the target. sf::BlendAdd makes
     * fire and sparks glow where they overlap.
     */
    void set_blend_mode(sf::BlendMode mode);

    /*
     * Rounds squares to whole pixels, for pixel art.
     */
    void set_pixel_snap(bool snap);

    /*
     * Pulls particles' heights down at this rate, in units per second
     * squared. When one reaches the ground the callback runs, and the
     * particle is removed unless set_remove_on_land(false). Particles are
     * drawn raised by their height, so they appear to arc up and fall.
     */
    void set_height_gravity(float gravity);
    void set_on_land(LandCallback callback);
    void set_remove_on_land(bool remove);

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
     * frame, the old 0.99-per-frame default worked out to roughly
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
     * By reference, the old version returned a copy of the whole
     * vector, so asking for particle positions duplicated every
     * particle in the system.
     */
    [[nodiscard]] const std::vector<Particle>& get_particles() const;

    [[nodiscard]] std::size_t          particle_count() const;
    [[nodiscard]] ParticleShape        get_shape() const;
    [[nodiscard]] const sf::VertexArray& get_particle_lines() const;
};

#endif
