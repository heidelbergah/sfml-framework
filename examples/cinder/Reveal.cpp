/**
 * Reveal.cpp  -  Cinder
 */

#include "Reveal.hpp"
#include "Palette.hpp"

#include "../../include/core/Random.hpp"

#include <cmath>

namespace cinder
{

namespace
{
    constexpr int   BLOB_SEGMENTS = 20;
    constexpr float TAU           = 6.2831853f;

    void quad(sf::VertexArray& va, sf::Vector2f p, float s, sf::Color c)
    {
        float const h = s * 0.5f;
        sf::Vector2f const a{p.x - h, p.y - h}, b{p.x + h, p.y - h},
                           d{p.x + h, p.y + h}, e{p.x - h, p.y + h};
        va.append({a, c}); va.append({b, c}); va.append({d, c});
        va.append({a, c}); va.append({d, c}); va.append({e, c});
    }

    // Fire cools from white-gold at birth to dark red at death.
    sf::Color fire_colour(float age)
    {
        if(age < 0.15f) return lerp(YELLOW, GOLDEN, age / 0.15f);
        if(age < 0.40f) return lerp(GOLDEN, ORANGE_GLOW, (age - 0.15f) / 0.25f);
        if(age < 0.70f) return lerp(ORANGE_GLOW, RED_FLAME, (age - 0.40f) / 0.30f);
        return lerp(RED_FLAME, RED_ASHEN, (age - 0.70f) / 0.30f);
    }
}

// ---------------------------------------------------------------- Reveal

bool Reveal::initialize(ShaderManager& shaders)
{
    if(m_ready) return true;

    if(!m_world.resize(FRAME_SIZE) || !m_frame.resize(FRAME_SIZE)
       || !m_heat.resize({FRAME_SIZE.x / 2, FRAME_SIZE.y / 2}))
        return false;

    m_world.setSmooth(false);
    m_frame.setSmooth(false);
    m_heat.setSmooth(true);

    m_shader = shaders.get_shader("cinder_reveal");
    if(!m_shader) m_shader = shaders.load_fragment("cinder_reveal", "shaders/cinder/reveal.frag");

    m_ready = true;
    return true;
}

sf::RenderTexture& Reveal::world(const sf::View& view, sf::Color ground)
{
    m_blobs.clear();
    m_world.setView(view);
    m_world.clear(ground);
    return m_world;
}

void Reveal::warm(sf::Vector2f centre, float radius, float strength)
{
    if(radius <= 0.5f || strength <= 0.0f) return;

    // Three rings, so the falloff is rounded rather than a cone: full at
    // the centre, still strong at half the radius, gone at the edge.
    sf::Color const inner(255, 255, 255, static_cast<std::uint8_t>(std::min(1.0f, strength) * 255.0f));
    sf::Color const mid  (255, 255, 255, static_cast<std::uint8_t>(std::min(1.0f, strength) * 190.0f));
    sf::Color const outer(255, 255, 255, 0);

    for(int i = 0; i < BLOB_SEGMENTS; ++i)
    {
        float const a0 = TAU * static_cast<float>(i)     / BLOB_SEGMENTS;
        float const a1 = TAU * static_cast<float>(i + 1) / BLOB_SEGMENTS;
        sf::Vector2f const u0{std::cos(a0), std::sin(a0)}, u1{std::cos(a1), std::sin(a1)};

        sf::Vector2f const m0 = centre + u0 * radius * 0.5f, m1 = centre + u1 * radius * 0.5f;
        sf::Vector2f const o0 = centre + u0 * radius,        o1 = centre + u1 * radius;

        m_blobs.append({centre, inner}); m_blobs.append({m0, mid});   m_blobs.append({m1, mid});
        m_blobs.append({m0, mid});       m_blobs.append({o0, outer}); m_blobs.append({o1, outer});
        m_blobs.append({m0, mid});       m_blobs.append({o1, outer}); m_blobs.append({m1, mid});
    }
}

sf::RenderTexture& Reveal::composite(const sf::View& view, float time, float flicker)
{
    m_world.display();

    m_heat.setView(view);
    m_heat.clear(sf::Color::Black);
    m_heat.draw(m_blobs, sf::RenderStates(sf::BlendAdd));
    m_heat.display();

    sf::Vector2f const origin = view.getCenter() - view.getSize() * 0.5f;

    m_frame.setView(m_frame.getDefaultView());
    m_frame.clear(sf::Color::Black);

    sf::Sprite sprite(m_world.getTexture());
    if(m_shader)
    {
        m_shader->setUniform("texture", sf::Shader::CurrentTexture);
        m_shader->setUniform("heat", m_heat.getTexture());
        m_shader->setUniform("origin", sf::Glsl::Vec2{std::round(origin.x), std::round(origin.y)});
        m_shader->setUniform("time", time);
        m_shader->setUniform("ember_flicker", flicker);
        m_frame.draw(sprite, sf::RenderStates(m_shader));
    }
    else
    {
        m_frame.draw(sprite);
    }

    m_frame.setView(view);
    return m_frame;
}

void Reveal::present(sf::RenderTarget& target)
{
    m_frame.display();
    sf::Sprite sprite(m_frame.getTexture());
    sprite.setScale({2.0f, 2.0f});

    sf::View const old = target.getView();
    target.setView(target.getDefaultView());
    target.draw(sprite);
    target.setView(old);
}

// ----------------------------------------------------------------- Motes

void Motes::flame(sf::Vector2f at, float spread, float size, float rise)
{
    Mote m;
    m.pos      = at + sf::Vector2f{Random::real(-spread, spread), Random::real(-spread * 0.4f, spread * 0.4f)};
    m.vel      = {Random::real(-6.0f, 6.0f), -Random::real(rise * 0.6f, rise * 1.3f)};
    m.max_life = m.life = Random::real(0.35f, 0.8f);
    m.size     = size * Random::real(0.7f, 1.2f);
    m_motes.push_back(m);
}

void Motes::burst(sf::Vector2f at, int count, float speed, float size)
{
    for(int i = 0; i < count; ++i)
    {
        float const a = Random::real(0.0f, TAU);
        float const s = speed * Random::real(0.3f, 1.0f);
        Mote m;
        m.pos      = at;
        m.vel      = {std::cos(a) * s, std::sin(a) * s - speed * 0.2f};
        m.max_life = m.life = Random::real(0.5f, 1.2f);
        m.size     = size * Random::real(0.6f, 1.2f);
        m_motes.push_back(m);
    }
}

void Motes::smoke(sf::Vector2f at, int count)
{
    for(int i = 0; i < count; ++i)
    {
        Mote m;
        m.pos      = at + sf::Vector2f{Random::real(-4.0f, 4.0f), Random::real(-3.0f, 3.0f)};
        m.vel      = {Random::real(-8.0f, 8.0f), -Random::real(8.0f, 22.0f)};
        m.max_life = m.life = Random::real(0.8f, 1.8f);
        m.size     = Random::real(2.0f, 4.0f);
        m.smoke    = true;
        m_motes.push_back(m);
    }
}

void Motes::steam(sf::Vector2f at, int count)
{
    for(int i = 0; i < count; ++i)
    {
        Mote m;
        m.pos      = at + sf::Vector2f{Random::real(-5.0f, 5.0f), Random::real(-2.0f, 2.0f)};
        m.vel      = {Random::real(-6.0f, 6.0f), -Random::real(14.0f, 30.0f)};
        m.max_life = m.life = Random::real(0.4f, 0.9f);
        m.size     = Random::real(1.0f, 2.0f);
        m.steam    = true;
        m_motes.push_back(m);
    }
}

void Motes::update(float dt)
{
    for(auto& m : m_motes)
    {
        m.life -= dt;
        m.pos  += m.vel * dt;
        m.vel.x *= std::pow(0.3f, dt);
        if(!m.smoke && !m.steam) m.vel.y -= 10.0f * dt;     // fire keeps rising
    }
    std::erase_if(m_motes, [](const Mote& m) { return m.life <= 0.0f; });
}

void Motes::draw(sf::RenderTarget& target)
{
    m_quads.clear();
    for(const auto& m : m_motes)
    {
        float const age = 1.0f - m.life / m.max_life;
        sf::Color colour;
        float     size = m.size;

        if(m.smoke)
        {
            colour = with_alpha(lerp(G6, G8, age), 0.8f * (1.0f - age));
            size  *= 1.0f + age;
        }
        else if(m.steam)
        {
            colour = with_alpha(lerp(AQUA, TEAL, age), 1.0f - age);
        }
        else
        {
            colour = fire_colour(age);
            size  *= 1.0f - age * 0.6f;
        }

        // Snap to whole pixels: the frame is pixel art.
        sf::Vector2f const p{std::round(m.pos.x), std::round(m.pos.y)};
        quad(m_quads, p, std::max(1.0f, std::round(size)), colour);
    }
    target.draw(m_quads);
}

}
