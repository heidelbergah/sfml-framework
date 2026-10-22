/**
 * Reveal.hpp  -  Cinder
 *
 * The trick the whole game rests on: colour only where it is warm.
 *
 *   1. world()      draw the level in full colour
 *   2. warm()       add soft blobs of heat, in world space
 *   3. composite()  burn everything cold back to ash (reveal.frag) and
 *                   hand back the frame, still at low resolution, for
 *                   the things that are always in colour: fire, water,
 *                   wind and the player
 *   4. present()    scale the frame up into the scene's target
 *
 * Everything is drawn at 480x270 and scaled up twice, so world pixels are
 * chunky and the dither in the shader reads as pixel art. The heat buffer
 * is half that again and smoothed; heat is soft by nature.
 *
 * Motes are the small square sparks that make up every flame.
 */

#ifndef CINDER_REVEAL_HPP
#define CINDER_REVEAL_HPP

#include "../../include/framework/ShaderManager.hpp"

#include <SFML/Graphics.hpp>

#include <vector>

namespace cinder
{

inline constexpr sf::Vector2u FRAME_SIZE{480u, 270u};
inline constexpr sf::Vector2f FRAME_SIZE_F{480.0f, 270.0f};

class Reveal
{
private:
    sf::RenderTexture m_world;
    sf::RenderTexture m_heat;
    sf::RenderTexture m_frame;
    sf::Shader*       m_shader{nullptr};
    sf::VertexArray   m_blobs{sf::PrimitiveType::Triangles};
    bool              m_ready{false};

public:
    bool initialize(ShaderManager& shaders);
    [[nodiscard]] bool ready() const { return m_ready; }

    /*
     * Cleared to `ground` and set to `view`, ready for the level.
     */
    sf::RenderTexture& world(const sf::View& view, sf::Color ground);

    /*
     * A blob of heat: `strength` at the centre, fading to nothing at
     * `radius`. Blobs add up and saturate at 1.
     */
    void warm(sf::Vector2f centre, float radius, float strength = 1.0f);

    /*
     * Runs the reveal pass. The returned frame has `view` set, so drawing
     * into it continues in world coordinates.
     */
    sf::RenderTexture& composite(const sf::View& view, float time, float flicker);

    void present(sf::RenderTarget& target);
};

struct Mote
{
    sf::Vector2f pos;
    sf::Vector2f vel;
    float life{1.0f};       // seconds remaining
    float max_life{1.0f};
    float size{2.0f};
    bool  smoke{false};     // grey, for puffs that are not fire
    bool  steam{false};     // pale teal, for water
};

class Motes
{
private:
    std::vector<Mote> m_motes;
    sf::VertexArray   m_quads{sf::PrimitiveType::Triangles};

public:
    void flame(sf::Vector2f at, float spread, float size, float rise = 30.0f);
    void burst(sf::Vector2f at, int count, float speed, float size);
    void smoke(sf::Vector2f at, int count);
    void steam(sf::Vector2f at, int count);

    void update(float dt);
    void draw(sf::RenderTarget& target);
    void clear() { m_motes.clear(); }
};

}

#endif
