/**
 * ParticleSystem.cpp
 *
 * Written by James Bouchat
 */

#include "../../include/framework/ParticleSystem.hpp"

ParticleSystem::ParticleSystem(sf::Vector2f pos)
    : m_pos(pos)
{
    // No srand() here. Seeding the global C generator from a
    // constructor meant two systems built in the same second produced
    // identical sequences, and it clobbered any seed the caller had
    // chosen. Randomness now comes from the shared Random:: generator.
}

void ParticleSystem::update(sf::Time delta_time)
{
    float const dt = delta_time.asSeconds();

    /*
     * A zero step is a pause, not a frame of simulation.
     *
     * Each particle is drawn as a line from its previous position to
     * its current one. Stepping with dt = 0 sets previous = current, so
     * every line collapses to zero length and the whole system vanishes
     * the instant the game is paused. Returning early leaves the last
     * good vertex data in place, so particles stay on screen, frozen.
     */
    if(dt <= 0.0f) return;

    // Drag as a per-second retention factor, so it behaves the same at
    // any framerate.
    float const drag_factor = (m_drag >= 1.0f) ? 1.0f : std::pow(m_drag, dt);

    for(Particle& p : m_particles)
    {
        if(m_gravity_on) p.add_acceleration(m_gravity * dt);

        p.scale_velocity(drag_factor);
        p.update(delta_time);
    }

    /*
     * Removal happens in one pass after the update loop.
     *
     * The old code called m_particles.erase() from inside a range-for
     * over m_particles. That invalidates the iterator the loop is
     * holding -- undefined behaviour -- and it also skipped whichever
     * particle shifted down into the freed slot.
     */
    std::erase_if(m_particles, [](const Particle& p) { return p.is_expired(); });

    // Two vertices per particle: previous position to current, which
    // gives each one a short motion-blur streak.
    m_particle_lines.resize(m_particles.size() * 2);

    std::size_t index = 0;
    for(const Particle& p : m_particles)
    {
        m_particle_lines[index].position = p.get_prev_position();
        m_particle_lines[index].color    = p.get_color();
        ++index;

        m_particle_lines[index].position = p.get_position();
        m_particle_lines[index].color    = p.get_color();
        ++index;
    }
}

void ParticleSystem::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    if(m_particle_lines.getVertexCount() == 0) return;
    target.draw(m_particle_lines, states);
}

void ParticleSystem::add_particles(int count, sf::Color color, Vector velocity,
                                   float spread_degrees)
{
    add_particles_varied(count, color, velocity, spread_degrees, 0.0f);
}

void ParticleSystem::add_particles_varied(int count, sf::Color color, Vector velocity,
                                          float spread_degrees, float speed_stddev)
{
    if(count <= 0) return;

    // Never grow past the cap, however hard the caller pushes.
    std::size_t const room = (m_particles.size() >= m_max_particles)
                           ? 0u
                           : m_max_particles - m_particles.size();

    std::size_t const to_spawn = std::min(static_cast<std::size_t>(count), room);
    if(to_spawn == 0) return;

    m_particles.reserve(m_particles.size() + to_spawn);

    for(std::size_t i = 0; i < to_spawn; ++i)
    {
        // spread_degrees of 0 is a valid, meaningful input: emit in a
        // perfectly straight line. Guarded so it cannot become a
        // modulo-by-zero the way rand() % (randomness * 2) did.
        float const jitter = (spread_degrees > 0.0f)
                           ? Random::real(-spread_degrees, spread_degrees)
                           : 0.0f;

        float speed = velocity.magnitude;
        if(speed_stddev > 0.0f)
            speed = std::max(0.0f, Random::normal(speed, speed_stddev));

        Particle particle(m_pos, color, m_lifespans);
        particle.set_fade(m_fade);
        particle.set_velocity(Vector{speed, velocity.direction + sf::degrees(jitter)});

        m_particles.push_back(particle);
    }
}

void ParticleSystem::apply_force(sf::Vector2f force)
{
    for(Particle& p : m_particles) p.add_velocity(force);
}

void ParticleSystem::apply_force(Vector force)
{
    apply_force(to_cartesian(force));
}

void ParticleSystem::set_position(sf::Vector2f pos)
{
    m_pos = pos;
}

void ParticleSystem::set_gravity(sf::Vector2f gravity)
{
    m_gravity = gravity;
}

void ParticleSystem::set_gravity(Vector gravity)
{
    m_gravity = to_cartesian(gravity);
}

void ParticleSystem::toggle_gravity()
{
    m_gravity_on = !m_gravity_on;
}

void ParticleSystem::set_gravity_enabled(bool enabled)
{
    m_gravity_on = enabled;
}

void ParticleSystem::toggle_fade()
{
    set_fade(!m_fade);
}

void ParticleSystem::set_fade(bool fade)
{
    m_fade = fade;
    for(Particle& p : m_particles) p.set_fade(m_fade);
}

void ParticleSystem::set_drag(float retained_per_second)
{
    m_drag = std::clamp(retained_per_second, 0.0f, 1.0f);
}

void ParticleSystem::set_lifespan(sf::Time lifespan)
{
    m_lifespans = lifespan;
}

void ParticleSystem::set_max_particles(std::size_t max)
{
    m_max_particles = max;
}

void ParticleSystem::clear()
{
    m_particles.clear();
    m_particle_lines.clear();
}

sf::Vector2f ParticleSystem::get_position() const
{
    return m_pos;
}

const std::vector<Particle>& ParticleSystem::get_particles() const
{
    return m_particles;
}

std::size_t ParticleSystem::particle_count() const
{
    return m_particles.size();
}

const sf::VertexArray& ParticleSystem::get_particle_lines() const
{
    return m_particle_lines;
}
