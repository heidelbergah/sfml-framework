/**
 * World.hpp  -  Cinder
 *
 * A level brought to life: the decoration scattered over it, how it is
 * drawn, and what the player collides with.
 *
 * Drawing is split by whether a thing should burn to ash when cold.
 *
 *   draw_living()   ground, grass, flowers, trees, ruins, brazier bowls --
 *                   drawn into the world before the reveal pass, so they
 *                   are grey until something warms them
 *   draw_cold()     water and wind -- drawn after it, always in teal, so a
 *                   hazard is never hidden in the dark
 */

#ifndef CINDER_WORLD_HPP
#define CINDER_WORLD_HPP

#include "Level.hpp"

#include <SFML/Graphics.hpp>

#include <vector>

namespace cinder
{

class World
{
private:
    const LevelDef* m_def{nullptr};

    sf::VertexArray m_ground{sf::PrimitiveType::Triangles};
    sf::VertexArray m_decor{sf::PrimitiveType::Triangles};

    // Ground and decor, cut into square chunks after they are built so a
    // frame only draws the few that are on screen.
    std::vector<sf::VertexArray> m_chunks;
    int m_chunk_cols{1};
    int m_chunk_rows{1};
    sf::VertexArray m_walls{sf::PrimitiveType::Triangles};
    sf::VertexArray m_cold{sf::PrimitiveType::Triangles};   // rebuilt every frame

    sf::Color m_ground_colour;

    [[nodiscard]] bool clear_of_obstacles(sf::Vector2f p, float margin) const;

public:
    void build(const LevelDef& def);

    [[nodiscard]] const LevelDef& def() const { return *m_def; }
    [[nodiscard]] sf::Color ground_colour() const { return m_ground_colour; }

    void draw_living(sf::RenderTarget& target, const sf::FloatRect& visible) const;
    void draw_cold(sf::RenderTarget& target, const sf::FloatRect& visible, float time);

    /*
     * Pushes a circle out of every wall and back inside the level.
     * Returns true if it touched anything.
     */
    bool collide(sf::Vector2f& pos, float radius) const;

    [[nodiscard]] bool in_water(sf::Vector2f p) const;
    [[nodiscard]] sf::Vector2f wind_at(sf::Vector2f p) const;
    [[nodiscard]] bool in_wall(sf::Vector2f p) const;
};

}

#endif
