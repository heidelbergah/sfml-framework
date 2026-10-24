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
        /*
         * Gravity changes velocity at a constant rate.
         *
         * It used to be added to each particle's acceleration every frame,
         * so the pull itself grew the longer a particle lived and falls
         * curved away faster and faster. Now it is a steady g.
         */
        if(m_gravity_on) p.add_velocity(m_gravity * dt);
        p.scale_velocity(drag_factor);
        p.update(delta_time);

        if(m_height_gravity > 0.0f)
        {
            bool const was_down = p.has_landed();
            p.update_height(m_height_gravity, dt);
            if(!was_down && p.has_landed() && m_on_land) m_on_land(p);
        }
    }

    /*
     * Removal happens in one pass after the update loop.
     *
     * The old code called m_particles.erase() from inside a range-for
     * over m_particles. That invalidates the iterator the loop is
     * holding, undefined behaviour, and it also skipped whichever
     * particle shifted down into the freed slot.
     */
    bool const drop_landed = m_height_gravity > 0.0f && m_remove_on_land;
    std::erase_if(m_particles, [drop_landed](const Particle& p)
    {
        return p.is_expired() || (drop_landed && p.has_landed());
    });

    rebuild_vertices();
}

void ParticleSystem::rebuild_vertices()
{
    if(m_shape == ParticleShape::LINE)
    {
        m_particle_lines.setPrimitiveType(sf::PrimitiveType::Lines);

        // Two vertices per particle: previous position to current, which
        // gives each one a short motion-blur streak.
        m_particle_lines.resize(m_particles.size() * 2);
        std::size_t index = 0;
        for(const Particle& p : m_particles)
        {
            sf::Vector2f const lift{0.0f, -p.get_height()};
            m_particle_lines[index].position = p.get_prev_position() + lift;
            m_particle_lines[index].color    = p.get_color();
            ++index;
            m_particle_lines[index].position = p.get_position() + lift;
            m_particle_lines[index].color    = p.get_color();
            ++index;
        }
        return;
    }

    // Squares: two triangles each.
    m_particle_lines.setPrimitiveType(sf::PrimitiveType::Triangles);
    m_particle_lines.resize(m_particles.size() * 6);
    std::size_t index = 0;
    for(const Particle& p : m_particles)
    {
        float size = std::max(0.0f, p.get_size());
        sf::Vector2f c = p.get_position() + sf::Vector2f{0.0f, -p.get_height()};
        if(m_pixel_snap)
        {
            size = std::max(1.0f, std::round(size));
            c = {std::floor(c.x - size * 0.5f) + size * 0.5f, std::floor(c.y - size * 0.5f) + size * 0.5f};
        }
        float const h = size * 0.5f;
        sf::Vector2f const a{c.x - h, c.y - h}, b{c.x + h, c.y - h}, d{c.x + h, c.y + h}, e{c.x - h, c.y + h};
        sf::Color const col = p.get_color();
        sf::Vertex* v = &m_particle_lines[index];
        v[0] = {a, col}; v[1] = {b, col}; v[2] = {d, col};
        v[3] = {a, col}; v[4] = {d, col}; v[5] = {e, col};
        index += 6;
    }
}

void ParticleSystem::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    if(m_particle_lines.getVertexCount() == 0) return;
    states.blendMode = m_blend;
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

bool ParticleSystem::emit(sf::Vector2f position, sf::Vector2f velocity, sf::Time lifespan, sf::Color color,
                          float size, float end_size, float height, float vertical_speed)
{
    if(m_particles.size() >= m_max_particles) return false;
    Particle particle(position, color, lifespan);
    particle.set_fade(m_fade);
    particle.set_velocity(velocity);
    particle.set_size(size, end_size);
    particle.set_height(height, vertical_speed);
    m_particles.push_back(particle);
    return true;
}

bool ParticleSystem::emit(sf::Vector2f position, sf::Vector2f velocity, sf::Time lifespan,
                          sf::Color color, sf::Color end_color,
                          float size, float end_size, float height, float vertical_speed)
{
    if(!emit(position, velocity, lifespan, color, size, end_size, height, vertical_speed)) return false;
    m_particles.back().set_end_color(end_color);
    return true;
}

void ParticleSystem::set_shape(ParticleShape shape)
{
    m_shape = shape;
}

void ParticleSystem::set_blend_mode(sf::BlendMode mode)
{
    m_blend = mode;
}

void ParticleSystem::set_pixel_snap(bool snap)
{
    m_pixel_snap = snap;
}

void ParticleSystem::set_height_gravity(float gravity)
{
    m_height_gravity = std::max(0.0f, gravity);
}

void ParticleSystem::set_on_land(LandCallback callback)
{
    m_on_land = std::move(callback);
}

void ParticleSystem::set_remove_on_land(bool remove)
{
    m_remove_on_land = remove;
}

ParticleShape ParticleSystem::get_shape() const
{
    return m_shape;
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
