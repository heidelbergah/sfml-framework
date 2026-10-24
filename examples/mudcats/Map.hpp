/**
 * Map.hpp  -  Mud Cats
 *
 * A battlefield, as geometry. There are two:
 *
 *   TRENCHES   the Ochre line in the rain: our trench, no man's land, theirs,
 *              a bunker and a farm
 *   FIELDS     open country at dawn: a sunken lane, hedgerows, a wood, tall
 *              grass, a stream with a mill at its bridge
 *   RAID       before dawn on the Dravo ridge: a slope of shell holes, two
 *              belts of wire, their trench, a hamlet behind it and the
 *              command dugout beyond
 *   HILL       Hill 312, through an afternoon and into the night: a ring of
 *              trench round the crest, gun pits, foxholes on the forward
 *              slope, and every way up it something to hide behind
 *
 * What things do:
 *
 *   walls      buildings, ruins, tree trunks: block movement, bullets and sight
 *   sandbags   low cover (also low stone walls, crates): block movement, stop
 *              rounds aimed at whoever is crouched behind them
 *   hedges     block movement and sight; rounds go through them more often
 *              than not
 *   slits      a bunker's firing slit: blocks movement, nothing else
 *   trenches   lines with a width; anyone inside is in cover from almost
 *              everything except a man standing right over them. The sunken
 *              lane is one too.
 *   craters    shell holes: cover for anyone lying in them
 *   wire       slows you to a crawl
 *   grass      tall grass and wheat: a man lying in it can't be seen from far
 *   canopy     under the trees: hard to see into from outside
 *   water      the stream: slow, cold, and no cover at all
 *
 * The ground texture keeps everything that happens on it: blood, scorch
 * marks, new craters and the dead are stamped into it as they happen.
 *
 * Paths come from A* over an 8 pixel grid.
 */

#ifndef MUDCATS_MAP_HPP
#define MUDCATS_MAP_HPP

#include <SFML/Graphics.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace mudcats
{

enum class LevelId : std::uint8_t { TRENCHES, FIELDS, RAID, HILL };

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

struct Tree
{
    sf::Vector2f pos;
    float canopy{16.0f};
    int   shade{0};
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
        WATER  = 16,
        GRASS  = 32,
        CANOPY = 64,
    };

    LevelId      level{LevelId::TRENCHES};
    sf::Vector2f size{2700.0f, 900.0f};

    std::vector<sf::FloatRect> walls;
    std::vector<sf::FloatRect> sandbags;
    std::vector<sf::FloatRect> hedges;
    std::vector<sf::FloatRect> slits;
    std::vector<sf::FloatRect> wire;
    std::vector<sf::FloatRect> roofs;
    std::vector<sf::FloatRect> grass;
    std::vector<sf::FloatRect> water;
    std::vector<sf::FloatRect> bridges;
    std::vector<Trench>        trenches;
    std::vector<Crater>        craters;
    std::vector<Tree>          trees;
    std::vector<CoverPoint>    cover;
    std::vector<Target>        bottles;
    std::vector<sf::Vector2f>  crates;      // ammunition

    // Places the level scripts refer to.
    sf::Vector2f player_start;
    sf::FloatRect range_area;
    sf::FloatRect front_trench_area;
    sf::FloatRect enemy_trench_area;
    sf::FloatRect bunker_area;
    sf::FloatRect farm_area;
    sf::FloatRect wood_area;
    sf::FloatRect mill_area;
    sf::FloatRect bridge_area;
    sf::FloatRect hill_area;                // the hill: inside the trench ring
    sf::Vector2f  crest;                    // the hill: its top
    std::vector<sf::Vector2f> waypoints;

    void build(LevelId id);

    /*
     * Cuts a gap in any wire within `radius` of `at`. Returns the pieces of
     * ground that were cleared, so the caller can draw the gap.
     */
    std::vector<sf::FloatRect> cut_wire(sf::Vector2f at, float radius);
    [[nodiscard]] bool near_wire(sf::Vector2f p, float reach, sf::Vector2f* nearest = nullptr) const;

    /*
     * Digging in, while the battle goes on. A row of sandbags is low cover
     * and blocks the way; a foxhole is a shell hole you made yourself.
     * can_dig says whether there is room; the add_ functions change the
     * map (paths, cover points) and the draw_ functions put the result
     * into a vertex array, to be stamped into the ground.
     */
    [[nodiscard]] bool can_dig(sf::FloatRect r) const;
    void add_bags(sf::FloatRect r);
    void add_foxhole(sf::Vector2f at, float radius);
    void draw_bags(sf::VertexArray& va, const sf::FloatRect& r) const;
    void draw_foxhole(sf::VertexArray& va, sf::Vector2f at, float radius) const;

    /*
     * How high the ground is, 0 at the foot to 1 on top. Only the hill has
     * any; a man up there sees further, and a man going up it is slower.
     */
    [[nodiscard]] float height(sf::Vector2f p) const;

    /*
     * Draws the static field into `ground`, which must be `size` big.
     */
    void render(sf::RenderTexture& ground) const;

    /*
     * What goes over the people: roofs, tree crowns, the tops of the tall
     * grass. Anything right over the viewer is drawn faint, so you can
     * always see yourself.
     */
    void draw_over(sf::RenderTarget& target, sf::Vector2f viewer, const sf::FloatRect& visible, float time) const;

    // --- queries --------------------------------------------------------
    [[nodiscard]] bool in_trench(sf::Vector2f p) const;
    [[nodiscard]] bool in_crater(sf::Vector2f p) const;
    [[nodiscard]] bool in_wire(sf::Vector2f p) const;
    [[nodiscard]] bool in_grass(sf::Vector2f p) const;
    [[nodiscard]] bool in_water(sf::Vector2f p) const;
    [[nodiscard]] bool under_canopy(sf::Vector2f p) const;
    [[nodiscard]] bool under_roof(sf::Vector2f p) const;
    [[nodiscard]] bool walkable(sf::Vector2f p) const;

    /*
     * Where a segment first enters a wall, as a fraction along it.
     */
    [[nodiscard]] std::optional<float> wall_hit(sf::Vector2f a, sf::Vector2f b) const;
    [[nodiscard]] bool line_of_sight(sf::Vector2f a, sf::Vector2f b) const;
    [[nodiscard]] bool crosses_hedge(sf::Vector2f a, sf::Vector2f b) const;

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
    mutable std::vector<float> m_cost;     // A* scratch
    mutable std::vector<int>   m_came;

    enum class Material : std::uint8_t { LOG, CONCRETE, BRICK, STONE, TRUNK };
    enum class Low : std::uint8_t { BAGS, CRATE, TRUNK, STONE };
    std::vector<Material> m_materials;
    std::vector<Low>      m_low;

    // Grass tops, cut into chunks so a frame only draws what is on screen.
    std::vector<sf::VertexArray> m_grass_tops;
    int m_chunk_cols{1};
    int m_chunk_rows{1};

    void build_trenches();
    void build_fields();
    void build_raid();
    void build_hill();
    void rasterise();
    void place_cover();
    void build_grass_tops();
    void wall(sf::FloatRect r, Material m);
    void low(sf::FloatRect r, Low kind);

    void render_trenches(sf::RenderTexture& ground) const;
    void render_fields(sf::RenderTexture& ground) const;
    void render_raid(sf::RenderTexture& ground) const;
    void render_hill(sf::RenderTexture& ground) const;
    void draw_low(sf::VertexArray& va, const sf::FloatRect& s, Low kind, std::uint32_t seed) const;
    void mark_rect(const sf::FloatRect& r, std::uint8_t flag, bool centres);
    void cover_around_low(const sf::FloatRect& s);
    void render_common(sf::RenderTexture& ground) const;

    [[nodiscard]] int cell_index(int cx, int cy) const { return cy * m_cols + cx; }
    [[nodiscard]] std::uint8_t flags_at(int cx, int cy) const;
};

[[nodiscard]] float distance_to_segment(sf::Vector2f p, sf::Vector2f a, sf::Vector2f b);
[[nodiscard]] bool  segment_rect(sf::Vector2f a, sf::Vector2f b, const sf::FloatRect& r, float* t_enter = nullptr);

}

#endif
