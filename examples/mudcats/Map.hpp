/**
 * Map.hpp  -  Mud Cats
 *
 * The one battlefield, as geometry:
 *
 *   walls      buildings and ruins: block movement, bullets and sight
 *   sandbags   low cover: block movement, stop rounds aimed at whoever
 *              is crouched behind them
 *   slits      a bunker's firing slit: blocks movement, nothing else
 *   trenches   lines with a width; anyone inside is in cover from
 *              almost everything except a man standing right over them
 *   craters    shell holes; cover for anyone lying in them
 *   wire       slows you to a crawl
 *
 * It also owns the ground texture. Blood, scorch marks, craters from new
 * shells and the dead are stamped into that texture as they happen, so
 * the field keeps everything that was done to it.
 *
 * Paths come from A* over an 8 pixel grid, which is plenty for a map this
 * size and a couple of dozen men.
 */

#ifndef MUDCATS_MAP_HPP
#define MUDCATS_MAP_HPP

#include <SFML/Graphics.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace mudcats
{

struct Trench
{
    std::vector<sf::Vector2f> points;
    float width{20.0f};
};

struct Crater
{
    sf::Vector2f pos;
    float radius{12.0f};
};

struct CoverPoint
{
    sf::Vector2f pos;
    sf::Vector2f protects;      // unit vector toward the cover; zero = every direction
    bool  trench{false};
};

struct Target                   // a bottle on the range
{
    sf::Vector2f pos;
    bool broken{false};
};

class Map
{
public:
    static constexpr float CELL = 8.0f;

    enum Flag : std::uint8_t
    {
        SOLID  = 1,
        TRENCH = 2,
        WIRE   = 4,
        CRATER = 8,
    };

    sf::Vector2f size{2700.0f, 900.0f};

    std::vector<sf::FloatRect> walls;
    std::vector<sf::FloatRect> sandbags;
    std::vector<sf::FloatRect> slits;
    std::vector<sf::FloatRect> wire;
    std::vector<sf::FloatRect> roofs;
    std::vector<Trench>        trenches;
    std::vector<Crater>        craters;
    std::vector<CoverPoint>    cover;
    std::vector<Target>        bottles;

    // Places the level script refers to.
    sf::Vector2f player_start;
    sf::FloatRect range_area;
    sf::FloatRect front_trench_area;
    sf::FloatRect enemy_trench_area;
    sf::FloatRect bunker_area;
    sf::FloatRect farm_area;
    std::vector<sf::Vector2f> waypoints;

    void build();

    /*
     * Draws the static field into `ground`, which must be `size` big.
     */
    void render(sf::RenderTexture& ground) const;
    void draw_roofs(sf::RenderTarget& target, sf::Vector2f viewer) const;

    // --- queries --------------------------------------------------------
    [[nodiscard]] bool in_trench(sf::Vector2f p) const;
    [[nodiscard]] bool in_crater(sf::Vector2f p) const;
    [[nodiscard]] bool in_wire(sf::Vector2f p) const;
    [[nodiscard]] bool under_roof(sf::Vector2f p) const;
    [[nodiscard]] bool walkable(sf::Vector2f p) const;

    /*
     * Where a segment first enters a wall, as a fraction along it.
     */
    [[nodiscard]] std::optional<float> wall_hit(sf::Vector2f a, sf::Vector2f b) const;
    [[nodiscard]] bool line_of_sight(sf::Vector2f a, sf::Vector2f b) const;

    /*
     * Does a sandbag lie across the last `reach` pixels of a shot coming
     * from `from` to `to`? That is the cover the target is crouched behind.
     */
    [[nodiscard]] bool sandbag_between(sf::Vector2f from, sf::Vector2f to, float reach) const;

    bool collide(sf::Vector2f& pos, float radius) const;

    [[nodiscard]] std::vector<sf::Vector2f> find_path(sf::Vector2f from, sf::Vector2f to) const;

private:
    int m_cols{0};
    int m_rows{0};
    std::vector<std::uint8_t> m_grid;

    void rasterise();
    void place_cover();

    [[nodiscard]] int cell_index(int cx, int cy) const { return cy * m_cols + cx; }
    [[nodiscard]] std::uint8_t flags_at(int cx, int cy) const;
};

[[nodiscard]] float distance_to_segment(sf::Vector2f p, sf::Vector2f a, sf::Vector2f b);
[[nodiscard]] bool  segment_rect(sf::Vector2f a, sf::Vector2f b, const sf::FloatRect& r, float* t_enter = nullptr);

}

#endif
