/**
 * Map.hpp  -  Mud Cats
 *
 * A battlefield, as geometry. There are three:
 *
 *   CHARGE     the Sava meadow, on a grey morning: a long open field, a
 *              shallow new trench at one end and a wood at the other, and
 *              nothing much in between
 *   TOWN       Kalvo, a market town at dusk: a main street of terraced
 *              houses with yards behind them, a cross-street, a square
 *              with a town hall, and the church at the far end of it all
 *   FOREST     the Lesna road, at midday under the trees: a supply column
 *              on a forest track, and the ambush waiting for it
 *
 * What things do:
 *
 *   walls      buildings, ruins, tree trunks: block movement, bullets and sight
 *   sandbags   low cover (also low stone walls, crates): block movement, stop
 *              rounds aimed at whoever is crouched behind them
 *   hedges     block movement and sight; rounds go through them more often
 *              than not
 *   slits      a bunker's firing slit: blocks movement, nothing else
 *   windows    the same in a house wall: you see, and shoot, through them
 *   roofs      hide what's under them from above. Close to a building its
 *              roof opens up, and you see in as far as your eyes reach
 *              through its doors and windows
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
#include <initializer_list>
#include <optional>
#include <vector>

namespace mudcats
{

enum class LevelId : std::uint8_t { CHARGE, TOWN, FOREST };

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

    LevelId      level{LevelId::CHARGE};
    sf::Vector2f size{2700.0f, 900.0f};

    std::vector<sf::FloatRect> walls;
    std::vector<sf::FloatRect> sandbags;
    std::vector<sf::FloatRect> hedges;
    std::vector<sf::FloatRect> slits;
    std::vector<sf::FloatRect> wire;
    std::vector<sf::FloatRect> windows;     // also in slits; kept apart to be drawn as windows
    std::vector<sf::FloatRect> roofs;
    std::vector<sf::FloatRect> grass;
    std::vector<sf::FloatRect> water;
    std::vector<sf::FloatRect> bridges;
    std::vector<Trench>        trenches;
    std::vector<Crater>        craters;
    std::vector<Tree>          trees;
    std::vector<CoverPoint>    cover;
    std::vector<sf::Vector2f>  crates;      // ammunition

    // Places the level scripts refer to.
    sf::Vector2f player_start;
    sf::FloatRect front_trench_area;
    sf::FloatRect wood_area;
    // The town.
    std::vector<sf::FloatRect> floors;      // inside every building, roofed or not
    std::vector<sf::Vector2f>  burning;     // fires that are still going when the battle starts
    sf::FloatRect block_a;                  // the first block of houses, both sides of the street
    sf::FloatRect block_b;                  // the second, past the cross-street
    sf::FloatRect square_area;
    sf::FloatRect church_area;
    // The forest.
    std::vector<sf::Vector2f>  road;        // the track through the trees, west to east
    sf::FloatRect road_area;                // where the column halts
    sf::FloatRect ambush_area;              // the trees the ambush is in
    std::vector<sf::Vector2f> waypoints;

    void build(LevelId id);

    /*
     * Cuts a gap in any wire within `radius` of `at`. Returns the pieces of
     * ground that were cleared, so the caller can draw the gap.
     */
    std::vector<sf::FloatRect> cut_wire(sf::Vector2f at, float radius);
    [[nodiscard]] bool near_wire(sf::Vector2f p, float reach, sf::Vector2f* nearest = nullptr) const;

    /*
     * Something that comes to rest on the field while the battle goes on,
     * a wagon pulling up, say: low cover from then on, in the paths and
     * the cover points, but not drawn into the ground.
     */
    void add_low(sf::FloatRect r);

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

    /*
     * How far a roof has opened up for someone at `viewer`, 0 to 1: shut
     * from a distance, open from close by and from inside. What's under a
     * shut roof can't be seen at all; under an open one, it's a matter of
     * line of sight, through the doors and windows.
     */
    static constexpr float ROOF_OPEN = 110.0f;
    static constexpr float ROOF_SHUT = 150.0f;
    [[nodiscard]] static float roof_open(const sf::FloatRect& roof, sf::Vector2f viewer);
    [[nodiscard]] float roof_open_at(sf::Vector2f p, sf::Vector2f viewer) const;   // over the point p; 1 if no roof

    // The forest's road: a point and its direction, so far along it from the west end.
    [[nodiscard]] sf::Vector2f road_at(float along) const;
    [[nodiscard]] sf::Vector2f road_dir(float along) const;
    [[nodiscard]] float road_length() const;
    [[nodiscard]] float road_distance(sf::Vector2f p) const;      // how far p is from the road
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
    enum class Low : std::uint8_t { BAGS, CRATE, TRUNK, STONE, LOG };
    std::vector<Material> m_materials;
    std::vector<Low>      m_low;

    // Grass tops, cut into chunks so a frame only draws what is on screen.
    std::vector<sf::VertexArray> m_grass_tops;

    // Filled pixel discs, every radius from 4 to 20 in eighths, drawn once:
    // a tree's crown is three of them, as textured quads, not hundreds of rows.
    mutable sf::Texture m_discs;
    mutable std::vector<sf::Vector2i> m_disc_cells;   // where each radius's disc sits in m_discs
    mutable bool m_discs_made{false};
    void make_discs() const;
    void add_disc(sf::VertexArray& va, float cx, float cy, float r, sf::Color c) const;
    int m_chunk_cols{1};
    int m_chunk_rows{1};

    void build_charge();
    void build_town();
    void build_forest();
    void rasterise();
    void place_cover();
    void build_grass_tops();
    void wall(sf::FloatRect r, Material m);
    // A wall with windows in it, centred at `at` along its length, each `width` across.
    void windowed(sf::FloatRect r, Material m, std::initializer_list<float> at, float width = 10.0f);
    void low(sf::FloatRect r, Low kind);

    void render_charge(sf::RenderTexture& ground) const;
    void render_town(sf::RenderTexture& ground) const;
    void render_forest(sf::RenderTexture& ground) const;
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
