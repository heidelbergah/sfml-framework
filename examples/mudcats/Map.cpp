/**
 * Map.cpp  -  Mud Cats
 *
 * Layouts for the four levels, the queries the game asks of them, and how
 * the ground is drawn.
 */

#include "Map.hpp"
#include "Draw.hpp"
#include "Palette.hpp"

#include <algorithm>
#include <cmath>
#include <queue>
#include <random>

namespace mudcats
{

namespace
{
    sf::FloatRect R(float x, float y, float w, float h) { return {{x, y}, {w, h}}; }

    float hashf(float a, float b)
    {
        float const s = std::sin(a * 127.1f + b * 311.7f) * 43758.5453f;
        return s - std::floor(s);
    }

    float field(float x, float y)
    {
        float const ix = std::floor(x), iy = std::floor(y);
        float fx = x - ix, fy = y - iy;
        fx = fx * fx * (3.0f - 2.0f * fx);
        fy = fy * fy * (3.0f - 2.0f * fy);
        float const a = hashf(ix, iy), b = hashf(ix + 1, iy), c = hashf(ix, iy + 1), d = hashf(ix + 1, iy + 1);
        float const top = a + (b - a) * fx, bottom = c + (d - c) * fx;
        return top + (bottom - top) * fy;
    }

    struct Rng
    {
        std::mt19937 gen;
        explicit Rng(unsigned s) : gen(s) {}
        float real(float a, float b) { return std::uniform_real_distribution<float>{a, b}(gen); }
        bool  chance(float p) { return real(0.0f, 1.0f) < p; }
        int   integer(int a, int b) { return std::uniform_int_distribution<int>{a, b}(gen); }
    };

}

float distance_to_segment(sf::Vector2f p, sf::Vector2f a, sf::Vector2f b)
{
    sf::Vector2f const ab = b - a;
    float const len2 = ab.x * ab.x + ab.y * ab.y;
    float t = len2 > 0.0f ? ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / len2 : 0.0f;
    t = std::clamp(t, 0.0f, 1.0f);
    sf::Vector2f const c = a + ab * t;
    return std::hypot(p.x - c.x, p.y - c.y);
}

bool segment_rect(sf::Vector2f a, sf::Vector2f b, const sf::FloatRect& r, float* t_enter)
{
    // Slab test.
    float t0 = 0.0f, t1 = 1.0f;
    sf::Vector2f const d = b - a;
    float const lo[2] = {r.position.x, r.position.y};
    float const hi[2] = {r.position.x + r.size.x, r.position.y + r.size.y};
    float const o[2]  = {a.x, a.y};
    float const v[2]  = {d.x, d.y};
    for(int i = 0; i < 2; ++i)
    {
        if(std::abs(v[i]) < 1e-6f)
        {
            if(o[i] < lo[i] || o[i] > hi[i]) return false;
            continue;
        }
        float ta = (lo[i] - o[i]) / v[i];
        float tb = (hi[i] - o[i]) / v[i];
        if(ta > tb) std::swap(ta, tb);
        t0 = std::max(t0, ta);
        t1 = std::min(t1, tb);
        if(t0 > t1) return false;
    }
    if(t_enter) *t_enter = t0;
    return true;
}

// ---------------------------------------------------------------- layout

void Map::wall(sf::FloatRect r, Material m)
{
    walls.push_back(r);
    m_materials.push_back(m);
}

void Map::low(sf::FloatRect r, Low kind)
{
    sandbags.push_back(r);
    m_low.push_back(kind);
}

void Map::build(LevelId id)
{
    level = id;
    walls.clear(); sandbags.clear(); hedges.clear(); slits.clear(); wire.clear(); roofs.clear();
    grass.clear(); water.clear(); bridges.clear(); trenches.clear(); craters.clear(); trees.clear();
    cover.clear(); bottles.clear(); crates.clear(); waypoints.clear();
    m_materials.clear(); m_low.clear();
    range_area = front_trench_area = enemy_trench_area = bunker_area = farm_area = {};
    wood_area = mill_area = bridge_area = hill_area = {};
    crest = {};

    if(id == LevelId::TRENCHES) build_trenches();
    else if(id == LevelId::FIELDS) build_fields();
    else if(id == LevelId::RAID) build_raid();
    else build_hill();

    // Every tree has a trunk you can hide behind and can't walk through.
    for(const auto& t : trees) wall(R(t.pos.x - 2.0f, t.pos.y - 2.0f, 4.0f, 4.0f), Material::TRUNK);

    rasterise();
    place_cover();
    build_grass_tops();
}

void Map::build_trenches()
{
    size = {2700.0f, 900.0f};
    player_start = {110.0f, 262.0f};

    // --- behind our lines ----------------------------------------------
    trenches.push_back({{{40, 262}, {200, 254}, {320, 282}, {405, 272}}, 22.0f});                     // reserve
    trenches.push_back({{{405, 272}, {470, 332}, {560, 322}, {640, 402}, {740, 382}, {848, 452}}, 18.0f}); // communication
    wall(R(150, 196, 80, 6), Material::LOG);                                   // dugout
    wall(R(150, 196, 6, 46), Material::LOG);
    wall(R(224, 196, 6, 46), Material::LOG);
    roofs.push_back(R(156, 202, 68, 40));

    range_area = R(440, 140, 260, 130);
    low(R(476, 180, 8, 50), Low::BAGS);                                        // the firing line
    for(float y : {172.0f, 202.0f, 232.0f})
    {
        low(R(652, y, 12, 12), Low::CRATE);
        bottles.push_back({{658.0f, y - 2.0f}, false});
    }

    // --- our front line ---------------------------------------------------
    trenches.push_back({{{840, 110}, {866, 240}, {834, 360}, {862, 480}, {836, 600}, {858, 790}}, 22.0f});
    front_trench_area = R(820, 110, 60, 680);

    // --- no man's land ----------------------------------------------------
    for(float y0 : {110.0f, 320.0f, 600.0f})
        wire.push_back(R(1040, y0, 20, y0 == 110.0f ? 170.0f : (y0 == 320.0f ? 240.0f : 180.0f)));
    wire.push_back(R(1540, 110, 20, 120));
    wire.push_back(R(1540, 270, 20, 130));
    wire.push_back(R(1540, 440, 20, 220));
    wire.push_back(R(1540, 700, 20, 90));

    for(auto r : {R(1180, 380, 30, 8), R(1300, 560, 8, 30), R(1420, 250, 30, 8), R(1460, 640, 30, 8),
                  R(1250, 170, 8, 26), R(960, 300, 26, 8), R(980, 590, 8, 26)})
        low(r, Low::BAGS);
    for(sf::Vector2f p : {sf::Vector2f{1120, 480}, {1380, 330}, {1600, 560}, {980, 660}, {1270, 720}})
        low(R(p.x - 3, p.y - 3, 6, 6), Low::TRUNK);                             // shattered tree trunks

    Rng rng(1936);
    for(int i = 0, tries = 0; i < 80 && tries < 3000; ++tries)
    {
        sf::Vector2f const p{rng.real(900, 1680), rng.real(130, 770)};
        float const r = rng.real(8, 19);
        bool ok = true;
        for(const auto& c : craters) if(std::hypot(c.pos.x - p.x, c.pos.y - p.y) < (c.radius + r) * 0.8f) ok = false;
        for(const auto& w : wire) if(w.findIntersection(R(p.x - r, p.y - r, 2 * r, 2 * r))) ok = false;
        if(!ok) continue;
        craters.push_back({p, r});
        ++i;
    }

    // --- their line ---------------------------------------------------------
    trenches.push_back({{{1722, 110}, {1700, 232}, {1732, 352}, {1706, 472}, {1736, 592}, {1714, 790}}, 22.0f});
    trenches.push_back({{{1732, 352}, {1800, 362}, {1880, 432}, {1994, 432}}, 18.0f});
    enemy_trench_area = R(1690, 110, 60, 680);

    low(R(1650, 314, 8, 42), Low::BAGS);                                       // MG nest
    low(R(1650, 314, 32, 8), Low::BAGS);
    low(R(1650, 348, 32, 8), Low::BAGS);
    for(auto r : {R(1684, 180, 8, 30), R(1688, 520, 8, 30), R(1692, 690, 8, 30)})
        low(r, Low::BAGS);

    wall(R(1850, 240, 40, 8), Material::BRICK);                                // ruins
    wall(R(1850, 240, 8, 44), Material::BRICK);
    wall(R(1900, 620, 52, 8), Material::BRICK);
    low(R(1950, 520, 22, 12), Low::CRATE);                                     // a wrecked cart

    // The bunker, with its firing slit facing us and its door facing home.
    wall(R(2000, 390, 64, 6), Material::CONCRETE);
    wall(R(2000, 440, 64, 6), Material::CONCRETE);
    wall(R(2000, 396, 6, 12), Material::CONCRETE);
    wall(R(2000, 420, 6, 20), Material::CONCRETE);
    slits.push_back(R(2000, 408, 6, 12));
    wall(R(2058, 396, 6, 14), Material::CONCRETE);
    wall(R(2058, 426, 6, 14), Material::CONCRETE);
    roofs.push_back(R(2006, 396, 52, 44));
    bunker_area = R(2000, 390, 64, 56);

    // The farm: a broken house in the middle of a field.
    for(auto r : {R(2240, 280, 60, 6), R(2330, 280, 70, 6), R(2240, 464, 40, 6), R(2310, 464, 90, 6),
                  R(2240, 280, 6, 80), R(2240, 390, 6, 80), R(2394, 280, 6, 60), R(2394, 370, 6, 100),
                  R(2310, 330, 6, 70)})
        wall(r, Material::BRICK);
    for(auto r : {R(2424, 318, 8, 32), R(2424, 400, 8, 32), R(2290, 220, 30, 8)})
        low(r, Low::BAGS);
    for(sf::Vector2f p : {sf::Vector2f{2200, 200}, {2470, 540}, {2180, 600}, {2500, 230}, {2560, 400}})
        trees.push_back({p, 10.0f, 1});                                        // the farm's trees
    crates = {{870, 250}, {872, 660}, {1760, 430}, {2330, 440}};
    farm_area = R(2240, 280, 160, 190);

    waypoints = {{850, 440}, {1000, 440}, {1130, 440}, {1330, 450}, {1480, 440},
                 {1715, 420}, {1880, 420}, {2080, 420}, {2320, 380}};

}

void Map::build_fields()
{
    size = {2800.0f, 1000.0f};
    Rng rng(1937);
    player_start = {150.0f, 520.0f};

    // --- the sunken lane we march up, between two hedges -------------------------
    trenches.push_back({{{0, 520}, {260, 506}, {560, 540}, {820, 516}, {990, 540}}, 26.0f});
    for(auto r : {R(60, 470, 320, 10), R(420, 470, 280, 10), R(740, 470, 220, 10),
                  R(40, 568, 250, 10), R(330, 568, 470, 10), R(840, 568, 120, 10)})
        hedges.push_back(r);
    grass.push_back(R(120, 200, 760, 250));                 // wheat to the north
    grass.push_back(R(100, 610, 820, 300));                 // a meadow to the south
    crates = {{690, 534}, {1816, 530}};

    // --- the wood -------------------------------------------------------------------
    wood_area = R(1010, 110, 460, 330);
    for(int i = 0, tries = 0; i < 58 && tries < 4000; ++tries)
    {
        sf::Vector2f const p{rng.real(1020, 1460), rng.real(120, 430)};
        bool ok = true;
        for(const auto& t : trees) if(std::hypot(t.pos.x - p.x, t.pos.y - p.y) < 24.0f) ok = false;
        if(!ok) continue;
        trees.push_back({p, rng.real(12, 20), rng.integer(0, 2)});
        ++i;
    }

    // --- the middle fields: hedgerows and tall grass ----------------------------
    for(auto r : {R(1000, 452, 150, 10), R(1180, 452, 240, 10), R(1450, 452, 200, 10),
                  R(1268, 462, 10, 128), R(1268, 620, 10, 160), R(1268, 810, 10, 150)})
        hedges.push_back(r);
    grass.push_back(R(1300, 640, 330, 280));
    grass.push_back(R(1010, 600, 240, 330));
    for(sf::Vector2f p : {sf::Vector2f{1120, 560}, {1460, 560}, {1560, 860}, {1200, 900}})
        trees.push_back({p, rng.real(12, 18), 1});

    // --- the stream, the bridge and the mill -------------------------------------
    water.push_back(R(1680, 0, 44, 1000));
    bridges.push_back(R(1672, 478, 60, 46));
    bridge_area = R(1672, 478, 60, 46);

    wall(R(1748, 404, 140, 6), Material::STONE);
    wall(R(1748, 554, 140, 6), Material::STONE);
    wall(R(1748, 410, 6, 60), Material::STONE);            // door at 470..500, toward the bridge
    wall(R(1748, 500, 6, 54), Material::STONE);
    wall(R(1882, 410, 6, 90), Material::STONE);            // door at 500..520, toward the road
    wall(R(1882, 520, 6, 34), Material::STONE);
    wall(R(1810, 410, 6, 62), Material::STONE);
    roofs.push_back(R(1754, 410, 128, 144));
    mill_area = R(1748, 404, 140, 156);

    // Their side: low stone walls along the road, a few sandbags, an orchard.
    for(auto r : {R(1910, 440, 8, 40), R(1910, 524, 8, 40)}) low(r, Low::BAGS);
    for(auto r : {R(1950, 456, 70, 6), R(1950, 548, 70, 6), R(2140, 380, 6, 70), R(2140, 600, 6, 70), R(1760, 360, 110, 6), R(1760, 600, 110, 6)})
        low(r, Low::STONE);
    for(int i = 0, tries = 0; i < 20 && tries < 2000; ++tries)
    {
        sf::Vector2f const p{rng.real(2000, 2380), rng.real(120, 360)};
        bool ok = true;
        for(const auto& t : trees) if(std::hypot(t.pos.x - p.x, t.pos.y - p.y) < 40.0f) ok = false;
        if(!ok) continue;
        trees.push_back({p, rng.real(10, 13), 2});
        ++i;
    }
    for(auto r : {R(2100, 600, 120, 10), R(2260, 600, 160, 10),
                  R(2450, 150, 10, 140), R(2450, 330, 10, 150), R(2450, 530, 10, 170), R(2450, 740, 10, 160)})
        hedges.push_back(r);
    grass.push_back(R(2500, 620, 280, 300));
    grass.push_back(R(2180, 650, 240, 260));

    waypoints = {{960, 530}, {1110, 420}, {1300, 520}, {1560, 520}, {1700, 500}, {1815, 482}, {1965, 500}};
}

void Map::build_raid()
{
    size = {1700.0f, 900.0f};
    Rng rng(1938);
    player_start = {130.0f, 450.0f};

    // --- our jumping-off ditch --------------------------------------------------------
    trenches.push_back({{{120, 220}, {140, 340}, {118, 460}, {138, 580}, {124, 690}}, 20.0f});
    front_trench_area = R(100, 210, 60, 490);

    // --- the slope: shell holes, long grass, a few stumps --------------------------------
    grass.push_back(R(200, 180, 180, 220));
    grass.push_back(R(330, 520, 230, 220));
    grass.push_back(R(470, 260, 120, 150));
    for(sf::Vector2f p : {sf::Vector2f{300, 470}, {520, 450}, {420, 200}, {610, 640}})
        low(R(p.x - 3, p.y - 3, 6, 6), Low::TRUNK);
    for(int i = 0, tries = 0; i < 46 && tries < 3000; ++tries)
    {
        sf::Vector2f const p{rng.real(190, 640), rng.real(140, 780)};
        float const r = rng.real(8, 17);
        bool ok = true;
        for(const auto& c : craters) if(std::hypot(c.pos.x - p.x, c.pos.y - p.y) < (c.radius + r) * 0.8f) ok = false;
        if(!ok) continue;
        craters.push_back({p, r});
        ++i;
    }

    // --- two belts of wire, the second with one old gap ---------------------------------
    wire.push_back(R(660, 120, 22, 680));
    wire.push_back(R(720, 120, 22, 300));
    wire.push_back(R(720, 446, 22, 354));

    // A listening post out in front of the wire, joined to their trench by a sap.
    trenches.push_back({{{812, 470}, {760, 474}, {700, 478}, {690, 482}}, 14.0f});
    craters.push_back({{690, 482}, 13.0f});

    // --- their trench --------------------------------------------------------------------
    trenches.push_back({{{820, 120}, {796, 240}, {826, 350}, {800, 470}, {830, 590}, {806, 790}}, 22.0f});
    trenches.push_back({{{800, 470}, {880, 462}, {960, 470}}, 18.0f});              // to the hamlet
    enemy_trench_area = R(780, 120, 60, 680);
    low(R(770, 286, 8, 30), Low::BAGS);  low(R(770, 286, 26, 8), Low::BAGS);  low(R(770, 312, 26, 8), Low::BAGS);   // MG nests
    low(R(784, 606, 8, 30), Low::BAGS);  low(R(784, 606, 26, 8), Low::BAGS);  low(R(784, 632, 26, 8), Low::BAGS);
    // A dugout off the trench, where the off-watch sleep.
    wall(R(846, 196, 46, 6), Material::LOG);  wall(R(846, 236, 46, 6), Material::LOG);  wall(R(886, 202, 6, 34), Material::LOG);
    roofs.push_back(R(846, 202, 40, 34));
    wall(R(850, 690, 46, 6), Material::LOG);  wall(R(850, 730, 46, 6), Material::LOG);  wall(R(890, 696, 6, 34), Material::LOG);
    roofs.push_back(R(850, 696, 40, 34));

    // --- the hamlet: two cottages, a barn, a yard wall, a well ----------------------------
    auto house = [this](float x, float y, float w, float h, float door_y, Material m)
    {
        // Walls with a door in the west side and a window gap to the east.
        wall(R(x, y, w, 6), m);
        wall(R(x, y + h - 6, w, 6), m);
        wall(R(x, y + 6, 6, door_y - y - 6), m);
        // Wide enough to get through with the walls grown for pathing.
        wall(R(x, door_y + 24, 6, y + h - 6 - door_y - 24), m);
        wall(R(x + w - 6, y + 6, 6, (h - 12) * 0.35f), m);
        wall(R(x + w - 6, y + 6 + (h - 12) * 0.35f + 12, 6, (h - 12) * 0.65f - 12), m);
        roofs.push_back(R(x + 6, y + 6, w - 12, h - 12));
    };
    house(1000, 270, 90, 80, 296, Material::STONE);
    house(1030, 560, 90, 76, 584, Material::STONE);
    house(1180, 380, 130, 100, 418, Material::BRICK);         // the barn
    farm_area = R(990, 260, 330, 390);
    for(auto r : {R(980, 380, 6, 70), R(980, 470, 6, 70), R(1120, 360, 50, 6), R(1130, 520, 40, 6), R(1330, 250, 6, 120), R(1330, 520, 6, 140)})
        low(r, Low::STONE);
    low(R(1144, 482, 12, 12), Low::STONE);                     // the well
    for(sf::Vector2f p : {sf::Vector2f{1240, 270}, {1270, 600}, {1360, 420}, {1210, 680}, {1100, 700}})
        trees.push_back({p, rng.real(11, 15), 2});

    // --- the command dugout, behind it all --------------------------------------------------
    wall(R(1400, 446, 64, 6), Material::CONCRETE);
    wall(R(1400, 494, 64, 6), Material::CONCRETE);
    wall(R(1458, 452, 6, 42), Material::CONCRETE);
    wall(R(1400, 452, 6, 12), Material::CONCRETE);             // door to the west, 464..482
    wall(R(1400, 482, 6, 12), Material::CONCRETE);
    roofs.push_back(R(1406, 452, 52, 42));
    bunker_area = R(1400, 446, 64, 54);
    for(auto r : {R(1380, 420, 8, 22), R(1380, 504, 8, 22)}) low(r, Low::BAGS);

    crates = {{150, 300}, {150, 600}};
    waypoints = {{150, 450}, {380, 450}, {560, 450}, {760, 450}, {815, 450}, {990, 450}, {1160, 450}, {1350, 410}};     // the last: beside the dugout, out of its door
}

void Map::build_hill()
{
    size = {1800.0f, 1200.0f};
    Rng rng(1939);
    crest = {760.0f, 600.0f};
    player_start = {742.0f, 598.0f};

    // --- the ring: a trench all the way round the crest -------------------------------
    std::vector<sf::Vector2f> ring;
    for(int k = 0; k <= 14; ++k)
    {
        float const a = 6.2831853f * static_cast<float>(k % 14) / 14.0f;
        float const r = 132.0f + (k % 14 == 0 ? 0.0f : (hashf(static_cast<float>(k % 14), 3.0f) - 0.5f) * 22.0f);
        ring.push_back(crest + sf::Vector2f{std::cos(a) * r * 1.08f, std::sin(a) * r});
    }
    trenches.push_back({ring, 20.0f});
    hill_area = R(crest.x - 150.0f, crest.y - 140.0f, 300.0f, 280.0f);
    // A communication trench down the back slope, the way replacements come.
    trenches.push_back({{ring[7], {560, 612}, {430, 640}, {280, 652}, {120, 640}}, 18.0f});
    // And a short one in to the dugout and the dump.
    trenches.push_back({{ring[7], crest + sf::Vector2f{-60, 6}}, 16.0f});

    // --- the top: a log dugout for the lieutenant, the dump beside it ----------------------
    wall(R(crest.x - 64, crest.y - 40, 44, 6), Material::LOG);
    wall(R(crest.x - 64, crest.y - 40, 6, 26), Material::LOG);
    wall(R(crest.x - 26, crest.y - 40, 6, 26), Material::LOG);
    roofs.push_back(R(crest.x - 58, crest.y - 34, 32, 22));
    low(R(crest.x + 6, crest.y - 14, 34, 6), Low::BAGS);          // the dump, walled in
    low(R(crest.x + 6, crest.y + 22, 34, 6), Low::BAGS);
    low(R(crest.x + 40, crest.y - 14, 6, 42), Low::BAGS);
    low(R(crest.x + 12, crest.y - 4, 10, 8), Low::CRATE);
    low(R(crest.x + 26, crest.y + 8, 10, 8), Low::CRATE);
    crates = {crest + sf::Vector2f{18.0f, 8.0f}, crest + sf::Vector2f{-40.0f, 30.0f}};

    // --- gun pits on the ring, facing the long spur and the two flanks -----------------------
    auto pit = [this](sf::Vector2f c, sf::Vector2f face)
    {
        // A horseshoe of bags, open behind.
        sf::Vector2f const side{-face.y, face.x};
        sf::Vector2f const front = c + face * 10.0f;
        if(std::abs(face.x) > std::abs(face.y))
        {
            low(R(front.x - 3, c.y - 12, 6, 24), Low::BAGS);
            low(R(std::min(c.x, front.x) - 2, c.y - 14, 12, 5), Low::BAGS);
            low(R(std::min(c.x, front.x) - 2, c.y + 9, 12, 5), Low::BAGS);
        }
        else
        {
            low(R(c.x - 12, front.y - 3, 24, 6), Low::BAGS);
            low(R(c.x - 14, std::min(c.y, front.y) - 2, 5, 12), Low::BAGS);
            low(R(c.x + 9, std::min(c.y, front.y) - 2, 5, 12), Low::BAGS);
        }
        (void)side;
    };
    pit(crest + sf::Vector2f{168, -46}, {1, 0});
    pit(crest + sf::Vector2f{168, 50}, {1, 0});
    pit(crest + sf::Vector2f{-6, -158}, {0, -1});
    pit(crest + sf::Vector2f{0, 160}, {0, 1});

    // --- foxholes on the forward slopes -------------------------------------------------
    for(float a : {-1.05f, -0.62f, -0.2f, 0.2f, 0.62f, 1.05f, -1.75f, 1.75f, -2.4f, 2.4f})
    {
        sf::Vector2f const at = crest + sf::Vector2f{std::cos(a) * 232.0f, std::sin(a) * 214.0f};
        craters.push_back({at, 9.0f});
        sf::Vector2f const out{std::cos(a), std::sin(a)};
        sf::Vector2f const lip = at + out * 12.0f;
        if(std::abs(out.x) > std::abs(out.y)) low(R(lip.x - 3, lip.y - 9, 6, 18), Low::BAGS);
        else low(R(lip.x - 9, lip.y - 3, 18, 6), Low::BAGS);
    }

    // --- a belt of wire, half way down, broken by the shelling ---------------------------
    for(int k = 0; k < 64; ++k)
    {
        float const a = -1.9f + 3.8f * static_cast<float>(k) / 63.0f;
        if(hashf(static_cast<float>(k), 9.0f) < 0.22f) continue;              // the gaps
        sf::Vector2f const at = crest + sf::Vector2f{std::cos(a) * 330.0f, std::sin(a) * 300.0f};
        wire.push_back(R(at.x - 9, at.y - 9, 18, 18));
    }

    // --- the forward slopes: shell holes, stumps, the odd low wall ---------------------------
    for(int i = 0, tries = 0; i < 70 && tries < 5000; ++tries)
    {
        sf::Vector2f const p{rng.real(520, 1450), rng.real(160, 1060)};
        float const d = std::hypot((p.x - crest.x) / 1.08f, p.y - crest.y);
        if(d < 190.0f || (p.x < crest.x - 120.0f && d < 420.0f)) continue;     // not on top, not behind us
        float const r = rng.real(7, 15);
        bool ok = true;
        for(const auto& c : craters) if(std::hypot(c.pos.x - p.x, c.pos.y - p.y) < (c.radius + r) * 0.9f) ok = false;
        for(const auto& w : wire) if(w.findIntersection(R(p.x - r, p.y - r, 2 * r, 2 * r))) ok = false;
        if(!ok) continue;
        craters.push_back({p, r});
        ++i;
    }
    for(sf::Vector2f p : {sf::Vector2f{1060, 420}, {1120, 760}, {1260, 560}, {980, 900}, {960, 300}, {1330, 320}, {1300, 880}})
        low(R(p.x - 3, p.y - 3, 6, 6), Low::TRUNK);
    for(auto r : {R(1180, 470, 40, 6), R(1220, 640, 6, 40), R(1040, 980, 44, 6), R(1030, 210, 44, 6), R(1380, 720, 6, 36)})
        low(r, Low::STONE);

    // --- the foot of the hill to the east: a sunken road, a ruined farm, an orchard ---------
    trenches.push_back({{{1430, 0}, {1414, 240}, {1440, 480}, {1420, 760}, {1446, 1000}, {1430, 1200}}, 26.0f});
    for(auto r : {R(1400, 60, 10, 160), R(1396, 300, 10, 140), R(1404, 520, 10, 200), R(1408, 820, 10, 150),
                  R(1466, 120, 10, 170), R(1468, 380, 10, 120), R(1474, 600, 10, 200), R(1470, 880, 10, 140)})
        hedges.push_back(r);
    for(auto r : {R(1560, 260, 90, 6), R(1560, 260, 6, 70), R(1644, 260, 6, 30), R(1560, 360, 50, 6)})
        wall(r, Material::STONE);
    low(R(1600, 300, 14, 10), Low::CRATE);
    for(int i = 0, tries = 0; i < 16 && tries < 2000; ++tries)
    {
        sf::Vector2f const p{rng.real(1520, 1760), rng.real(760, 1120)};
        bool ok = true;
        for(const auto& t : trees) if(std::hypot(t.pos.x - p.x, t.pos.y - p.y) < 42.0f) ok = false;
        if(!ok) continue;
        trees.push_back({p, rng.real(10, 13), 2});
        ++i;
    }
    grass.push_back(R(1080, 260, 190, 150));                 // hollows full of long grass
    grass.push_back(R(1120, 820, 220, 160));
    grass.push_back(R(1500, 460, 260, 220));

    // --- the north: a wood they can come out of -----------------------------------------------
    for(int i = 0, tries = 0; i < 46 && tries < 4000; ++tries)
    {
        sf::Vector2f const p{rng.real(460, 1240), rng.real(30, 210)};
        bool ok = true;
        for(const auto& t : trees) if(std::hypot(t.pos.x - p.x, t.pos.y - p.y) < 26.0f) ok = false;
        if(!ok) continue;
        trees.push_back({p, rng.real(12, 19), rng.integer(0, 2)});
        ++i;
    }
    wood_area = R(460, 20, 780, 200);

    // --- the south: wheat and a track ----------------------------------------------------------
    grass.push_back(R(480, 990, 520, 180));
    grass.push_back(R(1100, 1040, 260, 140));
    for(auto r : {R(460, 968, 220, 10), R(720, 968, 300, 10)}) hedges.push_back(r);

    // --- behind us, to the west: the way down to the rear --------------------------------------
    for(sf::Vector2f p : {sf::Vector2f{200, 420}, {260, 860}, {120, 300}, {340, 980}, {90, 900}, {380, 300}})
        trees.push_back({p, rng.real(12, 16), 1});

    waypoints = {crest, {120.0f, 640.0f}};
}

void Map::rasterise()
{
    m_cols = static_cast<int>(std::ceil(size.x / CELL));
    m_rows = static_cast<int>(std::ceil(size.y / CELL));
    m_grid.assign(static_cast<std::size_t>(m_cols * m_rows), 0);

    auto grown = [](sf::FloatRect r) { return sf::FloatRect(r.position - sf::Vector2f{2, 2}, r.size + sf::Vector2f{4, 4}); };
    auto mark = [this](const sf::FloatRect& r, std::uint8_t flag, bool centres) { mark_rect(r, flag, centres); };
    for(const auto& r : walls)    mark(grown(r), SOLID, false);
    for(const auto& r : sandbags) mark(grown(r), SOLID, false);
    for(const auto& r : hedges)   mark(grown(r), SOLID, false);
    for(const auto& r : slits)    mark(grown(r), SOLID, false);
    for(const auto& r : wire)     mark(r, WIRE, true);
    for(const auto& r : grass)    mark(r, GRASS, true);
    for(const auto& r : water)    mark(r, WATER, true);
    for(const auto& r : bridges)
        for(int cy = 0; cy < m_rows; ++cy)
            for(int cx = 0; cx < m_cols; ++cx)
                if(r.contains({(static_cast<float>(cx) + 0.5f) * CELL, (static_cast<float>(cy) + 0.5f) * CELL}))
                    m_grid[static_cast<std::size_t>(cell_index(cx, cy))] &= static_cast<std::uint8_t>(~WATER);

    for(int cy = 0; cy < m_rows; ++cy)
        for(int cx = 0; cx < m_cols; ++cx)
        {
            sf::Vector2f const c{(static_cast<float>(cx) + 0.5f) * CELL, (static_cast<float>(cy) + 0.5f) * CELL};
            std::uint8_t& f = m_grid[static_cast<std::size_t>(cell_index(cx, cy))];
            if(in_trench(c)) f |= TRENCH;
            if(in_crater(c)) f |= CRATER;
            for(const auto& t : trees)
                if(std::hypot(c.x - t.pos.x, c.y - t.pos.y) < t.canopy * 0.85f) { f |= CANOPY; break; }
        }
}

void Map::mark_rect(const sf::FloatRect& r, std::uint8_t flag, bool centres)
{
    int const x0 = std::max(0, static_cast<int>(r.position.x / CELL) - 1);
    int const y0 = std::max(0, static_cast<int>(r.position.y / CELL) - 1);
    int const x1 = std::min(m_cols - 1, static_cast<int>((r.position.x + r.size.x) / CELL) + 1);
    int const y1 = std::min(m_rows - 1, static_cast<int>((r.position.y + r.size.y) / CELL) + 1);
    for(int cy = y0; cy <= y1; ++cy)
        for(int cx = x0; cx <= x1; ++cx)
        {
            sf::FloatRect const cell({static_cast<float>(cx) * CELL, static_cast<float>(cy) * CELL}, {CELL, CELL});
            bool const hit = centres ? r.contains(cell.position + sf::Vector2f{CELL, CELL} * 0.5f) : r.findIntersection(cell).has_value();
            if(hit) m_grid[static_cast<std::size_t>(cell_index(cx, cy))] |= flag;
        }
}

void Map::cover_around_low(const sf::FloatRect& s)
{
    auto add = [this](sf::Vector2f p, sf::Vector2f protects)
    {
        if(!walkable(p) || in_wire(p) || in_water(p)) return;
        for(const auto& r : walls) if(r.contains(p)) return;
        cover.push_back({p, protects, false});
    };
    bool const vertical = s.size.y > s.size.x;
    float const len = vertical ? s.size.y : s.size.x;
    if(len < 10.0f) return;
    for(float k = 5.0f; k < len; k += 10.0f)
    {
        if(vertical)
        {
            float const y = s.position.y + k;
            add({s.position.x - 6.0f, y},            {1.0f, 0.0f});
            add({s.position.x + s.size.x + 6.0f, y}, {-1.0f, 0.0f});
        }
        else
        {
            float const x = s.position.x + k;
            add({x, s.position.y - 6.0f},            {0.0f, 1.0f});
            add({x, s.position.y + s.size.y + 6.0f}, {0.0f, -1.0f});
        }
    }
}

// ---------------------------------------------------------------- digging in

bool Map::can_dig(sf::FloatRect r) const
{
    if(r.position.x < 8 || r.position.y < 8 || r.position.x + r.size.x > size.x - 8 || r.position.y + r.size.y > size.y - 8) return false;
    sf::FloatRect const room(r.position - sf::Vector2f{3, 3}, r.size + sf::Vector2f{6, 6});
    for(const auto& w : walls)    if(w.findIntersection(room)) return false;
    for(const auto& b : sandbags) if(b.findIntersection(room)) return false;
    for(const auto& h : hedges)   if(h.findIntersection(room)) return false;
    for(const auto& w : wire)     if(w.findIntersection(room)) return false;
    for(const auto& w : water)    if(w.findIntersection(room)) return false;
    // Not in a trench, which is cover enough, and which a row of bags would block.
    for(sf::Vector2f c : {r.position, r.position + r.size, r.position + sf::Vector2f{r.size.x, 0}, r.position + sf::Vector2f{0, r.size.y}, r.getCenter()})
        if(in_trench(c)) return false;
    return true;
}

void Map::add_bags(sf::FloatRect r)
{
    low(r, Low::BAGS);
    mark_rect(sf::FloatRect(r.position - sf::Vector2f{2, 2}, r.size + sf::Vector2f{4, 4}), SOLID, false);
    cover_around_low(r);
}

void Map::add_foxhole(sf::Vector2f at, float radius)
{
    craters.push_back({at, radius});
    for(int cy = static_cast<int>((at.y - radius) / CELL); cy <= static_cast<int>((at.y + radius) / CELL); ++cy)
        for(int cx = static_cast<int>((at.x - radius) / CELL); cx <= static_cast<int>((at.x + radius) / CELL); ++cx)
        {
            if(cx < 0 || cy < 0 || cx >= m_cols || cy >= m_rows) continue;
            sf::Vector2f const c{(static_cast<float>(cx) + 0.5f) * CELL, (static_cast<float>(cy) + 0.5f) * CELL};
            if(std::hypot(c.x - at.x, c.y - at.y) < radius) m_grid[static_cast<std::size_t>(cell_index(cx, cy))] |= CRATER;
        }
    cover.push_back({at, {}, false});
}

void Map::draw_bags(sf::VertexArray& va, const sf::FloatRect& r) const
{
    draw_low(va, r, Low::BAGS, static_cast<std::uint32_t>(r.position.x * 7.0f + r.position.y * 13.0f));
}

void Map::draw_foxhole(sf::VertexArray& va, sf::Vector2f at, float radius) const
{
    // Fresh spoil thrown up round the lip, the hole dark and raw.
    Rng rng(static_cast<unsigned>(at.x * 3.0f + at.y * 5.0f));
    for(int k = 0; k < 14; ++k)
    {
        float const a = 6.283f * static_cast<float>(k) / 14.0f + rng.real(-0.2f, 0.2f);
        float const d = radius + rng.real(0.0f, 2.5f);
        draw::disc(va, at.x + std::cos(a) * d, at.y + std::sin(a) * d * 0.9f, rng.real(1.8f, 3.0f), rng.chance(0.5f) ? MUD_LIGHT : MUD_PALE);
    }
    draw::disc(va, at.x, at.y, radius, MUD_DARK);
    draw::disc(va, at.x + 1, at.y + 1, radius * 0.6f, lerp(MUD_DARK, INK, 0.3f));
}

float Map::height(sf::Vector2f p) const
{
    if(level != LevelId::HILL) return 0.0f;
    // A round top, a long spur running down to the east.
    float const dx = p.x - crest.x, dy = p.y - crest.y;
    float const d = std::sqrt(dx * dx * (dx > 0.0f ? 0.55f : 1.0f) + dy * dy);
    float const t = std::clamp(1.0f - d / 620.0f, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

void Map::place_cover()
{
    auto add = [this](sf::Vector2f p, sf::Vector2f protects, bool trench)
    {
        if(!walkable(p) || in_wire(p) || in_water(p)) return;
        for(const auto& r : walls) if(r.contains(p)) return;
        cover.push_back({p, protects, trench});
    };

    // Both sides of every low wall and every hedge.
    auto both_sides = [&](const sf::FloatRect& s, float step)
    {
        bool const vertical = s.size.y > s.size.x;
        float const len = vertical ? s.size.y : s.size.x;
        if(len < 10.0f) return;
        for(float k = 5.0f; k < len; k += step)
        {
            if(vertical)
            {
                float const y = s.position.y + k;
                add({s.position.x - 6.0f, y},            {1.0f, 0.0f}, false);
                add({s.position.x + s.size.x + 6.0f, y}, {-1.0f, 0.0f}, false);
            }
            else
            {
                float const x = s.position.x + k;
                add({x, s.position.y - 6.0f},            {0.0f, 1.0f}, false);
                add({x, s.position.y + s.size.y + 6.0f}, {0.0f, -1.0f}, false);
            }
        }
    };
    for(const auto& s : sandbags) both_sides(s, 10.0f);
    for(const auto& h : hedges)   both_sides(h, 14.0f);

    // Along every trench and the lane.
    for(const auto& t : trenches)
        for(std::size_t i = 0; i + 1 < t.points.size(); ++i)
        {
            sf::Vector2f const a = t.points[i], b = t.points[i + 1];
            float const len = std::hypot(b.x - a.x, b.y - a.y);
            for(float k = 6.0f; k < len; k += 13.0f)
                add(a + (b - a) * (k / len), {}, true);
        }

    for(const auto& c : craters) add(c.pos, {}, false);

    // Behind tree trunks, all round.
    for(const auto& t : trees)
        for(sf::Vector2f o : {sf::Vector2f{-6, 0}, {6, 0}, {0, -6}, {0, 6}})
            add(t.pos + o, -o / 6.0f, false);

    // Beside walls, on both sides.
    for(std::size_t i = 0; i < walls.size(); ++i)
    {
        if(m_materials[i] == Material::TRUNK) continue;
        const auto& w = walls[i];
        if(w.size.x >= w.size.y)
            for(float k = 6.0f; k < w.size.x; k += 12.0f)
            {
                add({w.position.x + k, w.position.y - 6.0f},            {0, 1}, false);
                add({w.position.x + k, w.position.y + w.size.y + 6.0f}, {0, -1}, false);
            }
        else
            for(float k = 6.0f; k < w.size.y; k += 12.0f)
            {
                add({w.position.x - 6.0f, w.position.y + k},            {1, 0}, false);
                add({w.position.x + w.size.x + 6.0f, w.position.y + k}, {-1, 0}, false);
            }
    }

    // Tall grass hides more than it stops, but men will lie in it.
    for(const auto& g : grass)
        for(float y = g.position.y + 12.0f; y < g.position.y + g.size.y; y += 34.0f)
            for(float x = g.position.x + 12.0f; x < g.position.x + g.size.x; x += 34.0f)
                add({x, y}, {}, false);
}

// ---------------------------------------------------------------- queries

std::uint8_t Map::flags_at(int cx, int cy) const
{
    if(cx < 0 || cy < 0 || cx >= m_cols || cy >= m_rows) return SOLID;
    return m_grid[static_cast<std::size_t>(cell_index(cx, cy))];
}

bool Map::in_trench(sf::Vector2f p) const
{
    for(const auto& t : trenches)
        for(std::size_t i = 0; i + 1 < t.points.size(); ++i)
            if(distance_to_segment(p, t.points[i], t.points[i + 1]) < t.width * 0.5f) return true;
    return false;
}

bool Map::in_crater(sf::Vector2f p) const
{
    for(const auto& c : craters)
        if(std::hypot(p.x - c.pos.x, p.y - c.pos.y) < c.radius * 0.8f) return true;
    return false;
}

bool Map::in_wire(sf::Vector2f p) const
{
    for(const auto& w : wire) if(w.contains(p)) return true;
    return false;
}

bool Map::near_wire(sf::Vector2f p, float reach, sf::Vector2f* nearest) const
{
    float best = reach;
    bool found = false;
    for(const auto& w : wire)
    {
        sf::Vector2f const c{std::clamp(p.x, w.position.x, w.position.x + w.size.x), std::clamp(p.y, w.position.y, w.position.y + w.size.y)};
        float const d = std::hypot(c.x - p.x, c.y - p.y);
        if(d <= best) { best = d; found = true; if(nearest) *nearest = c; }
    }
    return found;
}

std::vector<sf::FloatRect> Map::cut_wire(sf::Vector2f at, float radius)
{
    std::vector<sf::FloatRect> cleared;
    std::vector<sf::FloatRect> kept;
    for(const auto& w : wire)
    {
        bool const vertical = w.size.y >= w.size.x;
        float const lo = vertical ? w.position.y : w.position.x;
        float const hi = lo + (vertical ? w.size.y : w.size.x);
        float const across = vertical ? at.x : at.y;
        float const a0 = vertical ? w.position.x : w.position.y;
        float const a1 = a0 + (vertical ? w.size.x : w.size.y);
        float const along = vertical ? at.y : at.x;
        if(across < a0 - radius || across > a1 + radius || along + radius < lo || along - radius > hi)
        {
            kept.push_back(w);
            continue;
        }
        float const g0 = std::max(lo, along - radius), g1 = std::min(hi, along + radius);
        auto piece = [&](float p0, float p1)
        {
            if(p1 - p0 < 2.0f) return;
            kept.push_back(vertical ? R(w.position.x, p0, w.size.x, p1 - p0) : R(p0, w.position.y, p1 - p0, w.size.y));
        };
        piece(lo, g0);
        piece(g1, hi);
        cleared.push_back(vertical ? R(w.position.x - 2, g0, w.size.x + 4, g1 - g0) : R(g0, w.position.y - 2, g1 - g0, w.size.y + 4));
    }
    wire = kept;
    // The cut cells are open ground again, for paths and for the slow-down.
    for(const auto& r : cleared)
        for(int cy = std::max(0, static_cast<int>(r.position.y / CELL)); cy <= std::min(m_rows - 1, static_cast<int>((r.position.y + r.size.y) / CELL)); ++cy)
            for(int cx = std::max(0, static_cast<int>(r.position.x / CELL)); cx <= std::min(m_cols - 1, static_cast<int>((r.position.x + r.size.x) / CELL)); ++cx)
            {
                sf::Vector2f const c{(static_cast<float>(cx) + 0.5f) * CELL, (static_cast<float>(cy) + 0.5f) * CELL};
                if(!in_wire(c)) m_grid[static_cast<std::size_t>(cell_index(cx, cy))] &= static_cast<std::uint8_t>(~WIRE);
            }
    return cleared;
}

bool Map::in_grass(sf::Vector2f p) const
{
    return flags_at(static_cast<int>(std::floor(p.x / CELL)), static_cast<int>(std::floor(p.y / CELL))) & GRASS;
}

bool Map::in_water(sf::Vector2f p) const
{
    return flags_at(static_cast<int>(std::floor(p.x / CELL)), static_cast<int>(std::floor(p.y / CELL))) & WATER;
}

bool Map::under_canopy(sf::Vector2f p) const
{
    return flags_at(static_cast<int>(std::floor(p.x / CELL)), static_cast<int>(std::floor(p.y / CELL))) & CANOPY;
}

bool Map::under_roof(sf::Vector2f p) const
{
    for(const auto& r : roofs) if(r.contains(p)) return true;
    return false;
}

bool Map::walkable(sf::Vector2f p) const
{
    int const cx = static_cast<int>(std::floor(p.x / CELL)), cy = static_cast<int>(std::floor(p.y / CELL));
    return !(flags_at(cx, cy) & SOLID);
}

std::optional<float> Map::wall_hit(sf::Vector2f a, sf::Vector2f b) const
{
    std::optional<float> best;
    for(const auto& w : walls)
    {
        float t = 0.0f;
        if(segment_rect(a, b, w, &t) && (!best || t < *best)) best = t;
    }
    return best;
}

bool Map::crosses_hedge(sf::Vector2f a, sf::Vector2f b) const
{
    for(const auto& h : hedges) if(segment_rect(a, b, h)) return true;
    return false;
}

bool Map::line_of_sight(sf::Vector2f a, sf::Vector2f b) const
{
    // Walls and hedges. Roofs hide what is under them from the player's
    // eye, but a man inside a bunker can see out of its slit.
    return !wall_hit(a, b).has_value() && !crosses_hedge(a, b);
}

bool Map::sandbag_between(sf::Vector2f from, sf::Vector2f to, float reach) const
{
    sf::Vector2f const d = to - from;
    float const len = std::hypot(d.x, d.y);
    if(len < 1.0f) return false;
    sf::Vector2f const start = to - d / len * std::min(reach, len);
    for(const auto& s : sandbags)
        if(segment_rect(start, to, s)) return true;
    return false;
}

bool Map::collide(sf::Vector2f& pos, float radius) const
{
    bool hit = false;
    auto push = [&](const sf::FloatRect& w)
    {
        float const x0 = w.position.x, y0 = w.position.y, x1 = x0 + w.size.x, y1 = y0 + w.size.y;
        if(pos.x < x0 - radius || pos.x > x1 + radius || pos.y < y0 - radius || pos.y > y1 + radius) return;
        float const cx = std::clamp(pos.x, x0, x1), cy = std::clamp(pos.y, y0, y1);
        float const dx = pos.x - cx, dy = pos.y - cy;
        float const d2 = dx * dx + dy * dy;
        if(d2 > radius * radius) return;
        hit = true;
        if(d2 > 1e-6f)
        {
            float const d = std::sqrt(d2);
            pos += sf::Vector2f{dx, dy} * ((radius - d) / d);
        }
        else
        {
            float const l = pos.x - x0, r = x1 - pos.x, u = pos.y - y0, dn = y1 - pos.y;
            float const m = std::min({l, r, u, dn});
            if(m == l) pos.x = x0 - radius; else if(m == r) pos.x = x1 + radius;
            else if(m == u) pos.y = y0 - radius; else pos.y = y1 + radius;
        }
    };
    for(const auto& w : walls) push(w);
    for(const auto& w : sandbags) push(w);
    for(const auto& w : hedges) push(w);
    for(const auto& w : slits) push(w);

    sf::Vector2f const before = pos;
    pos.x = std::clamp(pos.x, radius, size.x - radius);
    pos.y = std::clamp(pos.y, radius, size.y - radius);
    return hit || before != pos;
}

std::vector<sf::Vector2f> Map::find_path(sf::Vector2f from, sf::Vector2f to) const
{
    auto cell_of = [](sf::Vector2f p) { return sf::Vector2i{static_cast<int>(p.x / CELL), static_cast<int>(p.y / CELL)}; };
    sf::Vector2i start = cell_of(from), goal = cell_of(to);
    goal.x = std::clamp(goal.x, 0, m_cols - 1);
    goal.y = std::clamp(goal.y, 0, m_rows - 1);

    // A goal inside something solid moves to the nearest open cell.
    if(flags_at(goal.x, goal.y) & SOLID)
    {
        bool found = false;
        for(int r = 1; r < 6 && !found; ++r)
            for(int dy = -r; dy <= r && !found; ++dy)
                for(int dx = -r; dx <= r && !found; ++dx)
                    if(!(flags_at(goal.x + dx, goal.y + dy) & SOLID)) { goal += {dx, dy}; found = true; }
        if(!found) return {};
    }

    // Scratch space kept between calls: a few dozen men path every few
    // seconds, and allocating two map-sized arrays each time adds up.
    std::size_t const n = static_cast<std::size_t>(m_cols * m_rows);
    std::vector<float>& cost = m_cost;
    std::vector<int>&   came = m_came;
    cost.assign(n, 1e30f);
    came.assign(n, -1);
    using Node = std::pair<float, int>;
    std::priority_queue<Node, std::vector<Node>, std::greater<>> open;

    int const s = cell_index(std::clamp(start.x, 0, m_cols - 1), std::clamp(start.y, 0, m_rows - 1));
    int const g = cell_index(goal.x, goal.y);
    cost[static_cast<std::size_t>(s)] = 0.0f;
    open.push({0.0f, s});

    auto h = [&](int i)
    {
        float const dx = static_cast<float>(i % m_cols - goal.x), dy = static_cast<float>(i / m_cols - goal.y);
        return std::sqrt(dx * dx + dy * dy);
    };

    int expanded = 0;
    while(!open.empty() && expanded < 30000)
    {
        auto [f, cur] = open.top();
        open.pop();
        if(cur == g) break;
        ++expanded;
        int const cx = cur % m_cols, cy = cur / m_cols;
        float const base = cost[static_cast<std::size_t>(cur)];
        if(f - h(cur) > base + 1e-3f) continue;

        for(int dy = -1; dy <= 1; ++dy)
            for(int dx = -1; dx <= 1; ++dx)
            {
                if(!dx && !dy) continue;
                int const nx = cx + dx, ny = cy + dy;
                std::uint8_t const fl = flags_at(nx, ny);
                if(fl & SOLID) continue;
                if(dx && dy && ((flags_at(cx + dx, cy) & SOLID) || (flags_at(cx, cy + dy) & SOLID))) continue;
                float step = (dx && dy) ? 1.4142f : 1.0f;
                if(fl & WIRE) step *= 6.0f;
                if(fl & WATER) step *= 3.0f;
                int const ni = cell_index(nx, ny);
                float const c = base + step;
                if(c < cost[static_cast<std::size_t>(ni)])
                {
                    cost[static_cast<std::size_t>(ni)] = c;
                    came[static_cast<std::size_t>(ni)] = cur;
                    open.push({c + h(ni), ni});
                }
            }
    }
    if(came[static_cast<std::size_t>(g)] < 0 && g != s) return {};

    std::vector<sf::Vector2f> cells;
    for(int i = g; i != -1 && i != s; i = came[static_cast<std::size_t>(i)])
        cells.push_back({(static_cast<float>(i % m_cols) + 0.5f) * CELL, (static_cast<float>(i / m_cols) + 0.5f) * CELL});
    std::reverse(cells.begin(), cells.end());
    if(!cells.empty()) cells.back() = walkable(to) ? to : cells.back();

    // String-pull: skip any point we can walk straight past.
    auto clear = [this](sf::Vector2f a, sf::Vector2f b)
    {
        float const len = std::hypot(b.x - a.x, b.y - a.y);
        for(float k = 0.0f; k < len; k += 3.0f)
        {
            sf::Vector2f const p = a + (b - a) * (k / len);
            int const cx = static_cast<int>(p.x / CELL), cy = static_cast<int>(p.y / CELL);
            std::uint8_t const fl = flags_at(cx, cy);
            if(fl & (SOLID | WIRE | WATER)) return false;
        }
        return true;
    };
    std::vector<sf::Vector2f> path;
    sf::Vector2f anchor = from;
    for(std::size_t i = 0; i < cells.size(); ++i)
    {
        if(i + 1 < cells.size() && clear(anchor, cells[i + 1])) continue;
        path.push_back(cells[i]);
        anchor = cells[i];
    }
    return path;
}

// ---------------------------------------------------------------- drawing

void Map::render(sf::RenderTexture& ground) const
{
    if(level == LevelId::TRENCHES) render_trenches(ground);
    else if(level == LevelId::FIELDS) render_fields(ground);
    else if(level == LevelId::RAID) render_raid(ground);
    else render_hill(ground);
    render_common(ground);
    ground.display();
}

void Map::render_trenches(sf::RenderTexture& ground) const
{
    Rng rng(77);
    sf::VertexArray va(sf::PrimitiveType::Triangles);

    ground.clear(MUD);

    // --- ground tones -----------------------------------------------------
    for(float y = 0; y < size.y; y += 4.0f)
        for(float x = 0; x < size.x; x += 4.0f)
        {
            float const m = field(x / 60.0f, y / 60.0f) * 0.7f + field(x / 17.0f, y / 17.0f) * 0.3f;
            bool const churned = x > 880 && x < 1690;
            bool const field_side = x > 2120;
            sf::Color c;
            if(churned)          c = m < 0.35f ? MUD_DARK : (m < 0.7f ? MUD : MUD_LIGHT);
            else if(field_side)  c = m < 0.3f ? MUD : (m < 0.55f ? GRASS_DEAD : (m < 0.8f ? mudcats::GRASS : MUD_LIGHT));
            else                 c = m < 0.3f ? MUD : (m < 0.65f ? MUD_LIGHT : (m < 0.85f ? GRASS_DEAD : mudcats::GRASS));
            bool const odd = (static_cast<int>(x / 4) + static_cast<int>(y / 4)) % 2;
            if(odd && std::fmod(m * 10.0f, 1.0f) > 0.85f) c = lerp(c, MUD_DARK, 0.5f);
            draw::rect(va, x, y, 4, 4, c);
        }
    ground.draw(va); va.clear();

    // Furrows in the farm field, tufts of grass at the edges.
    for(float y = 150; y < size.y - 20; y += 6.0f)
        for(float x = 2440; x < size.x; x += 2.0f)
            if(rng.chance(0.5f)) draw::rect(va, x, y + std::sin(x * 0.01f) * 2.0f, 2, 1, MUD_DARK);
    for(int i = 0; i < 2600; ++i)
    {
        float const x = rng.real(0, size.x), y = rng.real(0, size.y);
        if(x > 880 && x < 1690 && rng.chance(0.85f)) continue;
        sf::Color const c = rng.chance(0.5f) ? mudcats::GRASS : GRASS_DEAD;
        for(int b = 0; b < 3; ++b) draw::rect(va, std::floor(x + b), std::floor(y - rng.real(1, 4)), 1, rng.real(2, 4), c);
    }
    for(int i = 0; i < 1800; ++i)
        draw::rect(va, std::floor(rng.real(0, size.x)), std::floor(rng.real(0, size.y)), rng.chance(0.5f) ? 2.0f : 1.0f, 1, rng.chance(0.5f) ? MUD_PALE : MUD_DARK);
    ground.draw(va); va.clear();

    // --- craters ----------------------------------------------------------
    for(const auto& c : craters)
    {
        // A ragged lip of thrown earth, lit from the top left, then the
        // hole, then standing water in the bigger ones.
        for(int k = 0; k < 18; ++k)
        {
            float const a = 6.283f * static_cast<float>(k) / 18.0f + rng.real(-0.15f, 0.15f);
            float const d = c.radius + rng.real(-1.0f, 2.5f);
            sf::Color const lip = (std::cos(a + 2.4f) > 0.3f) ? MUD_LIGHT : lerp(MUD, MUD_LIGHT, 0.5f);
            draw::disc(va, c.pos.x + std::cos(a) * d, c.pos.y + std::sin(a) * d * 0.9f, rng.real(2.0f, 3.5f), lip);
        }
        draw::disc(va, c.pos.x, c.pos.y, c.radius, MUD_DARK);
        draw::disc(va, c.pos.x + 1, c.pos.y + 1, c.radius * 0.65f, lerp(MUD_DARK, INK, 0.25f));
        if(c.radius > 12)
            for(int k = 0; k < 4; ++k)
                draw::disc(va, c.pos.x + 2 + rng.real(-2, 2), c.pos.y + 2 + rng.real(-1.5f, 1.5f), c.radius * rng.real(0.18f, 0.3f), mudcats::WATER);
        for(int k = 0; k < 8; ++k)
        {
            float const a = rng.real(0, 6.283f), d = c.radius + rng.real(3, 9);
            draw::rect(va, std::floor(c.pos.x + std::cos(a) * d), std::floor(c.pos.y + std::sin(a) * d), 1, 1, MUD_LIGHT);
        }
    }
    ground.draw(va); va.clear();

    // --- trenches: dug earth, then the floor, then duckboards --------------
    for(const auto& t : trenches)
    {
        for(std::size_t i = 0; i + 1 < t.points.size(); ++i) draw::thick_line(va, t.points[i], t.points[i + 1], t.width + 8, MUD_PALE);
        for(auto p : t.points) draw::disc(va, p.x, p.y, (t.width + 8) * 0.5f, MUD_PALE);
    }
    for(const auto& t : trenches)
    {
        for(std::size_t i = 0; i + 1 < t.points.size(); ++i) draw::thick_line(va, t.points[i], t.points[i + 1], t.width, MUD_DARK);
        for(auto p : t.points) draw::disc(va, p.x, p.y, t.width * 0.5f, MUD_DARK);
    }
    for(const auto& t : trenches)
        for(std::size_t i = 0; i + 1 < t.points.size(); ++i)
        {
            sf::Vector2f const a = t.points[i], b = t.points[i + 1];
            float const len = std::hypot(b.x - a.x, b.y - a.y);
            sf::Vector2f const dir = (b - a) / len, side{-dir.y, dir.x};
            draw::thick_line(va, a + side * (t.width * 0.5f - 1), b + side * (t.width * 0.5f - 1), 2, lerp(MUD_DARK, INK, 0.4f));
            for(float k = 2.0f; k < len; k += 4.0f)
            {
                sf::Vector2f const p = a + dir * k;
                draw::thick_line(va, p - side * 3.5f, p + side * 3.5f, 2, k > len - 3 ? PLANK_DARK : PLANK);
            }
        }
    ground.draw(va); va.clear();

    // --- floors inside buildings ------------------------------------------
    draw::rect(va, 2006, 396, 52, 44, CONCRETE_DK);
    for(float y = 286; y < 464; y += 4.0f)
        draw::rect(va, 2246, y, 148, 3, (static_cast<int>(y) / 4) % 2 ? PLANK : PLANK_DARK);
    draw::rect(va, 156, 202, 68, 40, PLANK_DARK);
    ground.draw(va); va.clear();

    // --- the dead of earlier attacks -----------------------------------
    for(int i = 0; i < 22; ++i)
    {
        sf::Vector2f const p{rng.real(920, 1660), rng.real(140, 760)};
        float const a = rng.real(0, 6.283f);
        sf::Color const coat = rng.chance(0.7f) ? lerp(MON_BLUE_DK, MUD, 0.45f) : lerp(RCH_TAN_DK, MUD, 0.45f);
        draw::rotated_rect(va, p, 6, 4, a, coat);
        draw::rotated_rect(va, p + sf::Vector2f{std::cos(a), std::sin(a)} * 4.5f, 3, 3, a, lerp(SKIN_DK, MUD, 0.5f));
        draw::rotated_rect(va, p - sf::Vector2f{std::cos(a), std::sin(a)} * 4.5f, 5, 3, a, lerp(coat, MUD_DARK, 0.4f));
        if(rng.chance(0.5f)) draw::disc(va, p.x + rng.real(-3, 3), p.y + rng.real(-3, 3), rng.real(2, 4), lerp(BLOOD_DK, MUD_DARK, 0.6f));
    }
    for(int i = 0; i < 18; ++i)                                             // helmets, rifles, planks
    {
        sf::Vector2f const p{rng.real(900, 1700), rng.real(130, 770)};
        if(rng.chance(0.3f)) draw::disc(va, p.x, p.y, 2, lerp(MON_HELMET, MUD, 0.65f));
        else draw::rotated_rect(va, p, rng.real(6, 12), 1.5f, rng.real(0, 3.14f), rng.chance(0.5f) ? PLANK_DARK : GUNMETAL);
    }
    ground.draw(va); va.clear();

    // --- wire ----------------------------------------------------------------
    for(const auto& w : wire)
    {
        float const cx = w.position.x + w.size.x * 0.5f;
        for(float y = w.position.y; y < w.position.y + w.size.y; y += 1.0f)
        {
            float const loop = std::sin(y * 0.45f) * (w.size.x * 0.45f);
            draw::rect(va, std::floor(cx + loop), y, 1, 1, mudcats::WIRE);
            if(static_cast<int>(y) % 3 == 0) draw::rect(va, std::floor(cx - loop * 0.7f), y, 1, 1, lerp(mudcats::WIRE, MUD_DARK, 0.4f));
        }
        for(float y = w.position.y + 4; y < w.position.y + w.size.y; y += 18.0f)
        {
            draw::rect(va, cx - 1, y, 2, 3, BARK);
            draw::rect(va, cx, y - 1, 1, 1, PLANK);
        }
    }
    ground.draw(va); va.clear();


    // The range: a few spent cases by the firing line.
    for(int i = 0; i < 25; ++i)
        draw::rect(va, std::floor(rng.real(462, 474)), std::floor(rng.real(180, 230)), 1, 1, MON_GOLD);
    // Rubble around the ruins and the farmhouse.
    for(int i = 0; i < 140; ++i)
    {
        sf::Vector2f p = rng.chance(0.6f) ? sf::Vector2f{rng.real(2220, 2420), rng.real(260, 490)}
                                          : sf::Vector2f{rng.real(1830, 1970), rng.real(220, 650)};
        if(!walkable(p)) continue;
        draw::rect(va, std::floor(p.x), std::floor(p.y), rng.real(1, 3), rng.real(1, 2), rng.chance(0.5f) ? BRICK : BRICK_DK);
    }
    ground.draw(va);
}

void Map::render_fields(sf::RenderTexture& ground) const
{
    Rng rng(78);
    sf::VertexArray va(sf::PrimitiveType::Triangles);
    ground.clear(MEADOW);

    // --- grass in broad soft patches ------------------------------------------------
    for(float y = 0; y < size.y; y += 4.0f)
        for(float x = 0; x < size.x; x += 4.0f)
        {
            float const m = field(x / 70.0f, y / 70.0f) * 0.7f + field(x / 19.0f, y / 19.0f) * 0.3f;
            sf::Color c = m < 0.3f ? MEADOW_DK : (m < 0.62f ? MEADOW : (m < 0.85f ? MEADOW_LT : GRASS_DEAD));
            if(in_grass({x + 2, y + 2})) c = m < 0.5f ? WHEAT_DK : lerp(WHEAT_DK, MEADOW, 0.5f);
            bool const odd = (static_cast<int>(x / 4) + static_cast<int>(y / 4)) % 2;
            if(odd && std::fmod(m * 10.0f, 1.0f) > 0.85f) c = lerp(c, MEADOW_DK, 0.5f);
            draw::rect(va, x, y, 4, 4, c);
        }
    ground.draw(va); va.clear();

    // Wheat to the north is gold; the other tall grass stays green.
    for(const auto& g : grass)
    {
        bool const wheat = g.position.y < 460.0f && g.position.x < 900.0f;
        for(int i = 0; i < static_cast<int>(g.size.x * g.size.y / 10.0f); ++i)
        {
            float const x = rng.real(g.position.x, g.position.x + g.size.x), y = rng.real(g.position.y, g.position.y + g.size.y);
            draw::rect(va, std::floor(x), std::floor(y), 1, 2, wheat ? (rng.chance(0.5f) ? WHEAT : WHEAT_DK) : (rng.chance(0.5f) ? MEADOW_LT : mudcats::GRASS));
        }
    }
    // Wildflowers and tufts everywhere else.
    for(int i = 0; i < 5000; ++i)
    {
        sf::Vector2f const p{rng.real(0, size.x), rng.real(0, size.y)};
        if(in_grass(p)) continue;
        if(rng.chance(0.08f)) draw::rect(va, std::floor(p.x), std::floor(p.y), 1, 1, rng.chance(0.5f) ? PAPER : MON_GOLD);
        else for(int b = 0; b < 3; ++b) draw::rect(va, std::floor(p.x + b), std::floor(p.y - rng.real(1, 3)), 1, rng.real(2, 3), rng.chance(0.5f) ? MEADOW_LT : MEADOW_DK);
    }
    ground.draw(va); va.clear();

    // --- the road from the bridge east, and the ruts in it ---------------------------
    std::vector<sf::Vector2f> const road = {{1730, 501}, {1900, 505}, {2100, 492}, {2400, 510}, {2800, 500}};
    for(std::size_t i = 0; i + 1 < road.size(); ++i)
    {
        draw::thick_line(va, road[i], road[i + 1], 24, lerp(ROAD, MEADOW_DK, 0.4f));
        draw::thick_line(va, road[i], road[i + 1], 18, ROAD);
        draw::thick_line(va, road[i] + sf::Vector2f{0, -4}, road[i + 1] + sf::Vector2f{0, -4}, 2, lerp(ROAD, MUD_DARK, 0.5f));
        draw::thick_line(va, road[i] + sf::Vector2f{0, 4}, road[i + 1] + sf::Vector2f{0, 4}, 2, lerp(ROAD, MUD_DARK, 0.5f));
    }
    for(auto p : road) draw::disc(va, p.x, p.y, 9, ROAD);
    ground.draw(va); va.clear();

    // --- the sunken lane: steep grassy banks, a rutted floor ---------------------------
    for(const auto& t : trenches)
    {
        for(std::size_t i = 0; i + 1 < t.points.size(); ++i) draw::thick_line(va, t.points[i], t.points[i + 1], t.width + 10, MEADOW_DK);
        for(auto p : t.points) draw::disc(va, p.x, p.y, (t.width + 10) * 0.5f, MEADOW_DK);
    }
    for(const auto& t : trenches)
    {
        for(std::size_t i = 0; i + 1 < t.points.size(); ++i) draw::thick_line(va, t.points[i], t.points[i + 1], t.width, lerp(ROAD, MUD_DARK, 0.35f));
        for(auto p : t.points) draw::disc(va, p.x, p.y, t.width * 0.5f, lerp(ROAD, MUD_DARK, 0.35f));
        for(std::size_t i = 0; i + 1 < t.points.size(); ++i)
        {
            sf::Vector2f const a = t.points[i], b = t.points[i + 1];
            float const len = std::hypot(b.x - a.x, b.y - a.y);
            sf::Vector2f const d = (b - a) / len, side{-d.y, d.x};
            for(float k = -4.0f; k <= 4.0f; k += 8.0f)
                draw::thick_line(va, a + side * k, b + side * k, 2, lerp(ROAD, MUD_DARK, 0.6f));
            draw::thick_line(va, a + side * (t.width * 0.5f - 1), b + side * (t.width * 0.5f - 1), 2, lerp(MEADOW_DK, INK, 0.4f));
        }
    }
    ground.draw(va); va.clear();

    // --- the stream ---------------------------------------------------------------------
    for(const auto& w : water)
    {
        draw::rect(va, w.position.x - 4, w.position.y, w.size.x + 8, w.size.y, MUD);
        draw::rect(va, w.position.x, w.position.y, w.size.x, w.size.y, STREAM);
        for(float y = w.position.y; y < w.position.y + w.size.y; y += 5.0f)
        {
            float const x = w.position.x + 4.0f + std::fmod(hashf(y, 3.0f) * w.size.x, w.size.x - 10.0f);
            draw::rect(va, std::floor(x), y, rng.real(3, 8), 1, STREAM_LT);
        }
        for(float y = w.position.y; y < w.position.y + w.size.y; y += 2.0f)
        {
            if(rng.chance(0.5f)) draw::rect(va, w.position.x - 2, y, 2, 2, MUD_LIGHT);
            if(rng.chance(0.5f)) draw::rect(va, w.position.x + w.size.x, y, 2, 2, MUD_LIGHT);
            if(rng.chance(0.2f)) draw::rect(va, w.position.x - 5, y, 2, rng.real(2, 5), MEADOW_LT);       // reeds
        }
    }
    for(const auto& b : bridges)
    {
        draw::rect(va, b.position.x + 2, b.position.y + 3, b.size.x, b.size.y, SHADOW);
        draw::rect(va, b.position.x, b.position.y, b.size.x, b.size.y, STONE_DK);
        for(float x = b.position.x; x < b.position.x + b.size.x; x += 5.0f)
            draw::rect(va, x, b.position.y + 2, 4, b.size.y - 4, (static_cast<int>(x) / 5) % 2 ? STONE : lerp(STONE, STONE_DK, 0.4f));
        draw::rect(va, b.position.x, b.position.y, b.size.x, 2, lerp(STONE, PAPER, 0.3f));
        draw::rect(va, b.position.x, b.position.y + b.size.y - 2, b.size.x, 2, STONE_DK);
    }
    ground.draw(va); va.clear();

    // --- the mill's floor and its waterwheel ------------------------------------------
    for(float y = mill_area.position.y + 6; y < mill_area.position.y + mill_area.size.y - 6; y += 4.0f)
        draw::rect(va, mill_area.position.x + 6, y, mill_area.size.x - 12, 3, (static_cast<int>(y) / 4) % 2 ? PLANK : PLANK_DARK);
    for(int k = 0; k < 6; ++k)
    {
        float const y = 516.0f + static_cast<float>(k) * 6.0f;
        draw::rect(va, 1726, y, 22, 3, PLANK_DARK);
        draw::rect(va, 1726, y, 22, 1, PLANK);
    }
    draw::disc(va, 1737, 532, 3, GUNMETAL);
    // Sacks of grain spilled inside.
    for(int i = 0; i < 12; ++i)
        draw::rect(va, std::floor(rng.real(1820, 1875)), std::floor(rng.real(420, 540)), 4, 3, rng.chance(0.5f) ? SANDBAG : SANDBAG_DK);
    ground.draw(va); va.clear();

    // --- hedges ---------------------------------------------------------------------------
    for(const auto& h : hedges)
    {
        draw::rect(va, h.position.x + 2, h.position.y + 3, h.size.x, h.size.y, SHADOW);
        draw::rect(va, h.position.x, h.position.y, h.size.x, h.size.y, HEDGE);
        float const len = std::max(h.size.x, h.size.y);
        bool const vertical = h.size.y > h.size.x;
        for(float k = 0; k < len; k += 4.0f)
        {
            float const j = rng.real(-1.5f, 1.5f);
            sf::Vector2f const c = vertical ? sf::Vector2f{h.position.x + h.size.x * 0.5f + j, h.position.y + k}
                                            : sf::Vector2f{h.position.x + k, h.position.y + h.size.y * 0.5f + j};
            draw::disc(va, c.x, c.y, rng.real(3.5f, 5.5f), rng.chance(0.4f) ? HEDGE_LT : HEDGE);
            if(rng.chance(0.3f)) draw::rect(va, std::floor(c.x - 1), std::floor(c.y - 2), 1, 1, LEAF_LT);
            if(rng.chance(0.06f)) draw::rect(va, std::floor(c.x + 1), std::floor(c.y), 1, 1, RCH_RED);            // haws
        }
    }
    ground.draw(va); va.clear();

    // --- under the wood: dark ground, leaf litter, ferns -----------------------------------
    for(const auto& t : trees)
    {
        draw::disc(va, t.pos.x + 2, t.pos.y + 2, t.canopy * 0.9f, lerp(MEADOW_DK, INK, 0.25f));
        for(int k = 0; k < 10; ++k)
            draw::rect(va, std::floor(t.pos.x + rng.real(-t.canopy, t.canopy)), std::floor(t.pos.y + rng.real(-t.canopy, t.canopy)), 2, 1,
                       rng.chance(0.5f) ? lerp(WHEAT_DK, MUD, 0.5f) : LEAF);
    }
    ground.draw(va);
}

void Map::render_raid(sf::RenderTexture& ground) const
{
    Rng rng(80);
    sf::VertexArray va(sf::PrimitiveType::Triangles);
    ground.clear(MUD);

    // --- a cold, worn slope: mud and dead grass, rimed pale in places ---------------
    for(float y = 0; y < size.y; y += 4.0f)
        for(float x = 0; x < size.x; x += 4.0f)
        {
            float const m = field(x / 55.0f, y / 55.0f) * 0.7f + field(x / 15.0f, y / 15.0f) * 0.3f;
            bool const churned = x > 180 && x < 880;
            sf::Color c = churned ? (m < 0.35f ? MUD_DARK : (m < 0.72f ? MUD : GRASS_DEAD))
                                  : (m < 0.3f ? MUD : (m < 0.6f ? GRASS_DEAD : (m < 0.85f ? mudcats::GRASS : MUD_LIGHT)));
            if(in_grass({x + 2, y + 2})) c = m < 0.5f ? GRASS_DEAD : lerp(GRASS_DEAD, WHEAT_DK, 0.4f);
            if(field(x / 9.0f + 40.0f, y / 9.0f) > 0.82f) c = lerp(c, PAPER_DK, 0.25f);          // frost
            draw::rect(va, x, y, 4, 4, c);
        }
    ground.draw(va); va.clear();
    for(int i = 0; i < 2400; ++i)
    {
        float const x = rng.real(0, size.x), y = rng.real(0, size.y);
        sf::Color const c = rng.chance(0.5f) ? mudcats::GRASS : GRASS_DEAD;
        for(int b = 0; b < 3; ++b) draw::rect(va, std::floor(x + b), std::floor(y - rng.real(1, 4)), 1, rng.real(2, 4), c);
    }
    ground.draw(va); va.clear();

    // --- shell holes --------------------------------------------------------------------
    for(const auto& c : craters)
    {
        for(int k = 0; k < 16; ++k)
        {
            float const a = 6.283f * static_cast<float>(k) / 16.0f + rng.real(-0.15f, 0.15f);
            float const d = c.radius + rng.real(-1.0f, 2.5f);
            draw::disc(va, c.pos.x + std::cos(a) * d, c.pos.y + std::sin(a) * d * 0.9f, rng.real(2.0f, 3.5f), std::cos(a + 2.4f) > 0.3f ? MUD_LIGHT : MUD);
        }
        draw::disc(va, c.pos.x, c.pos.y, c.radius, MUD_DARK);
        draw::disc(va, c.pos.x + 1, c.pos.y + 1, c.radius * 0.6f, lerp(MUD_DARK, INK, 0.25f));
        if(c.radius > 12) draw::disc(va, c.pos.x + 2, c.pos.y + 2, c.radius * 0.3f, lerp(mudcats::WATER, PAPER_DK, 0.15f));
    }
    ground.draw(va); va.clear();

    // --- trenches --------------------------------------------------------------------------
    for(const auto& t : trenches)
    {
        for(std::size_t i = 0; i + 1 < t.points.size(); ++i) draw::thick_line(va, t.points[i], t.points[i + 1], t.width + 8, MUD_PALE);
        for(auto p : t.points) draw::disc(va, p.x, p.y, (t.width + 8) * 0.5f, MUD_PALE);
    }
    for(const auto& t : trenches)
    {
        for(std::size_t i = 0; i + 1 < t.points.size(); ++i) draw::thick_line(va, t.points[i], t.points[i + 1], t.width, MUD_DARK);
        for(auto p : t.points) draw::disc(va, p.x, p.y, t.width * 0.5f, MUD_DARK);
        for(std::size_t i = 0; i + 1 < t.points.size(); ++i)
        {
            sf::Vector2f const a = t.points[i], b = t.points[i + 1];
            float const len = std::hypot(b.x - a.x, b.y - a.y);
            sf::Vector2f const d = (b - a) / len, side{-d.y, d.x};
            for(float k = 2.0f; k < len; k += 4.0f)
                draw::thick_line(va, a + d * k - side * 3.0f, a + d * k + side * 3.0f, 2, PLANK_DARK);
        }
    }
    ground.draw(va); va.clear();

    // --- floors: cottages, barn, dugouts ------------------------------------------------------
    for(const auto& r : roofs)
    {
        bool const dugout = r.position.x > 1380 || (r.position.x > 840 && r.position.x < 900);
        for(float y = r.position.y; y < r.position.y + r.size.y; y += 4.0f)
            draw::rect(va, r.position.x, y, r.size.x, 3, dugout ? (static_cast<int>(y) / 4 % 2 ? CONCRETE_DK : lerp(CONCRETE_DK, INK, 0.3f))
                                                                : (static_cast<int>(y) / 4 % 2 ? PLANK : PLANK_DARK));
    }
    // The yard: cobbles between the cottages.
    for(int i = 0; i < 900; ++i)
    {
        sf::Vector2f const p{rng.real(1090, 1330), rng.real(360, 560)};
        if(!walkable(p) || under_roof(p)) continue;
        draw::rect(va, std::floor(p.x), std::floor(p.y), 2, 2, rng.chance(0.5f) ? STONE_DK : lerp(STONE, MUD, 0.5f));
    }
    ground.draw(va); va.clear();

    // --- wire ------------------------------------------------------------------------------------
    for(const auto& w : wire)
    {
        float const cx = w.position.x + w.size.x * 0.5f;
        for(float y = w.position.y; y < w.position.y + w.size.y; y += 1.0f)
        {
            float const loop = std::sin(y * 0.45f) * (w.size.x * 0.45f);
            draw::rect(va, std::floor(cx + loop), y, 1, 1, mudcats::WIRE);
            if(static_cast<int>(y) % 3 == 0) draw::rect(va, std::floor(cx - loop * 0.7f), y, 1, 1, lerp(mudcats::WIRE, MUD_DARK, 0.4f));
        }
        for(float y = w.position.y + 4; y < w.position.y + w.size.y; y += 18.0f)
            draw::rect(va, cx - 1, y, 2, 3, BARK);
    }
    // The dead of an earlier raid, hung on the wire.
    for(int i = 0; i < 7; ++i)
    {
        sf::Vector2f const p{rng.chance(0.5f) ? 671.0f : 731.0f, rng.real(140, 780)};
        float const a = rng.real(0, 6.283f);
        draw::rotated_rect(va, p, 6, 4, a, lerp(MON_BLUE_DK, MUD, 0.5f));
        draw::rotated_rect(va, p + sf::Vector2f{std::cos(a), std::sin(a)} * 4.5f, 3, 3, a, lerp(SKIN_DK, MUD, 0.5f));
    }
    ground.draw(va);
}

void Map::draw_low(sf::VertexArray& va, const sf::FloatRect& s, Low kind, std::uint32_t seed) const
{
    Rng rng(seed);
    draw::rect(va, s.position.x + 2, s.position.y + 2, s.size.x, s.size.y, SHADOW);
    if(kind == Low::TRUNK)
    {
        sf::Vector2f const c = s.position + s.size * 0.5f;
        draw::disc(va, c.x, c.y, 3, BARK);
        draw::disc(va, c.x, c.y, 1.5f, PLANK);
        for(int k = 0; k < 3; ++k)
            draw::thick_line(va, c, c + sf::Vector2f{rng.real(-9, 9), rng.real(-9, 9)}, 1, BARK);
        return;
    }
    if(kind == Low::CRATE)
    {
        draw::rect(va, s.position.x, s.position.y, s.size.x, s.size.y, PLANK);
        draw::rect(va, s.position.x + 1, s.position.y + s.size.y * 0.5f - 0.5f, s.size.x - 2, 1, PLANK_DARK);
        draw::rect(va, s.position.x, s.position.y, s.size.x, 1, lerp(PLANK, PAPER, 0.3f));
        return;
    }
    if(kind == Low::STONE)
    {
        draw::rect(va, s.position.x, s.position.y, s.size.x, s.size.y, STONE_DK);
        bool const vertical = s.size.y > s.size.x;
        float const len = vertical ? s.size.y : s.size.x;
        for(float k = 0; k < len; k += 4.0f)
        {
            float const w = std::min(rng.real(3, 5), len - k);
            if(vertical) draw::rect(va, s.position.x + 1, s.position.y + k, s.size.x - 2, w - 1, rng.chance(0.5f) ? STONE : lerp(STONE, STONE_DK, 0.4f));
            else draw::rect(va, s.position.x + k, s.position.y + 1, w - 1, s.size.y - 2, rng.chance(0.5f) ? STONE : lerp(STONE, STONE_DK, 0.4f));
        }
        if(rng.chance(0.5f)) draw::rect(va, s.position.x, s.position.y, 2, 1, MEADOW_LT);
        return;
    }
    draw::rect(va, s.position.x, s.position.y, s.size.x, s.size.y, SANDBAG_DK);
    bool const vertical = s.size.y > s.size.x;
    float const len = vertical ? s.size.y : s.size.x, thick = vertical ? s.size.x : s.size.y;
    for(float k = 0; k < len; k += 6.0f)
        for(float j = 0; j < thick; j += 4.0f)
        {
            float const off = std::fmod(j / 4.0f, 2.0f) * 3.0f;
            float const bx = vertical ? s.position.x + j : s.position.x + std::min(k + off, len - 1);
            float const by = vertical ? s.position.y + std::min(k + off, len - 1) : s.position.y + j;
            float const bw = vertical ? std::min(3.0f, thick - j) : std::min(5.0f, len - k - off);
            float const bh = vertical ? std::min(5.0f, len - k - off) : std::min(3.0f, thick - j);
            if(bw > 0 && bh > 0) draw::rect(va, bx, by, bw, bh, SANDBAG);
            if(bw > 1 && bh > 1) draw::rect(va, bx, by, std::max(1.0f, bw - 1), 1, lerp(SANDBAG, PAPER, 0.3f));
        }
}

void Map::render_hill(sf::RenderTexture& ground) const
{
    Rng rng(81);
    sf::VertexArray va(sf::PrimitiveType::Triangles);
    ground.clear(MEADOW);

    // --- the hill: grass going to chalk and churned mud toward the top, with
    //     a contour drawn every so often so you can read the slope -------------
    for(float y = 0; y < size.y; y += 4.0f)
        for(float x = 0; x < size.x; x += 4.0f)
        {
            float const h = height({x + 2, y + 2});
            float const m = field(x / 50.0f, y / 50.0f) * 0.7f + field(x / 14.0f, y / 14.0f) * 0.3f;
            sf::Color c = m < 0.35f ? MEADOW_DK : (m < 0.7f ? MEADOW : MEADOW_LT);
            // Higher up, the shelling has had at it.
            float const churn = std::clamp((h - 0.45f) * 2.2f + (m - 0.5f) * 0.6f, 0.0f, 1.0f);
            c = lerp(c, m < 0.5f ? MUD : MUD_LIGHT, churn);
            c = lerp(c, PAPER_DK, h * 0.10f);                                 // and it catches the light
            if(in_grass({x + 2, y + 2})) c = m < 0.5f ? lerp(MEADOW, WHEAT_DK, 0.5f) : WHEAT_DK;
            int const band = static_cast<int>(h * 8.0f);
            if(band > 0 && band != static_cast<int>(height({x + 6, y + 2}) * 8.0f)) c = lerp(c, INK, 0.18f);
            if(band > 0 && band != static_cast<int>(height({x + 2, y + 6}) * 8.0f)) c = lerp(c, INK, 0.18f);
            draw::rect(va, x, y, 4, 4, c);
        }
    ground.draw(va); va.clear();
    for(int i = 0; i < 3200; ++i)
    {
        float const x = rng.real(0, size.x), y = rng.real(0, size.y);
        if(height({x, y}) > 0.6f && rng.chance(0.7f)) continue;
        sf::Color const c = rng.chance(0.5f) ? MEADOW_LT : lerp(MEADOW_LT, WHEAT, 0.35f);
        for(int b = 0; b < 3; ++b) draw::rect(va, std::floor(x + b), std::floor(y - rng.real(1, 4)), 1, rng.real(2, 4), c);
    }
    ground.draw(va); va.clear();

    // --- shell holes and foxholes --------------------------------------------------------
    for(const auto& c : craters)
    {
        for(int k = 0; k < 16; ++k)
        {
            float const a = 6.283f * static_cast<float>(k) / 16.0f + rng.real(-0.15f, 0.15f);
            float const d = c.radius + rng.real(-1.0f, 2.5f);
            draw::disc(va, c.pos.x + std::cos(a) * d, c.pos.y + std::sin(a) * d * 0.9f, rng.real(2.0f, 3.5f), std::cos(a + 2.4f) > 0.3f ? MUD_LIGHT : MUD);
        }
        draw::disc(va, c.pos.x, c.pos.y, c.radius, MUD_DARK);
        draw::disc(va, c.pos.x + 1, c.pos.y + 1, c.radius * 0.6f, lerp(MUD_DARK, INK, 0.25f));
    }
    ground.draw(va); va.clear();

    // --- trenches, and the sunken road ------------------------------------------------------
    for(const auto& t : trenches)
    {
        for(std::size_t i = 0; i + 1 < t.points.size(); ++i) draw::thick_line(va, t.points[i], t.points[i + 1], t.width + 8, t.width > 24 ? ROAD : MUD_PALE);
        for(auto p : t.points) draw::disc(va, p.x, p.y, (t.width + 8) * 0.5f, t.width > 24 ? ROAD : MUD_PALE);
    }
    for(const auto& t : trenches)
    {
        bool const road = t.width > 24;
        for(std::size_t i = 0; i + 1 < t.points.size(); ++i) draw::thick_line(va, t.points[i], t.points[i + 1], t.width, road ? lerp(ROAD, MUD, 0.5f) : MUD_DARK);
        for(auto p : t.points) draw::disc(va, p.x, p.y, t.width * 0.5f, road ? lerp(ROAD, MUD, 0.5f) : MUD_DARK);
        if(road) continue;
        for(std::size_t i = 0; i + 1 < t.points.size(); ++i)
        {
            sf::Vector2f const a = t.points[i], b = t.points[i + 1];
            float const len = std::hypot(b.x - a.x, b.y - a.y);
            sf::Vector2f const d = (b - a) / len, side{-d.y, d.x};
            for(float k = 2.0f; k < len; k += 4.0f)
                draw::thick_line(va, a + d * k - side * 3.0f, a + d * k + side * 3.0f, 2, PLANK_DARK);
        }
    }
    ground.draw(va); va.clear();

    // --- the dugout floor, the hedges ------------------------------------------------------------
    for(const auto& r : roofs)
        for(float y = r.position.y; y < r.position.y + r.size.y; y += 4.0f)
            draw::rect(va, r.position.x, y, r.size.x, 3, static_cast<int>(y) / 4 % 2 ? PLANK : PLANK_DARK);
    for(const auto& h : hedges)
    {
        draw::rect(va, h.position.x + 2, h.position.y + 3, h.size.x, h.size.y, SHADOW);
        draw::rect(va, h.position.x, h.position.y, h.size.x, h.size.y, HEDGE);
        float const len = std::max(h.size.x, h.size.y);
        bool const vertical = h.size.y > h.size.x;
        for(float k = 0; k < len; k += 4.0f)
        {
            float const j = rng.real(-1.5f, 1.5f);
            sf::Vector2f const c = vertical ? sf::Vector2f{h.position.x + h.size.x * 0.5f + j, h.position.y + k}
                                            : sf::Vector2f{h.position.x + k, h.position.y + h.size.y * 0.5f + j};
            draw::disc(va, c.x, c.y, rng.real(3.5f, 5.5f), rng.chance(0.4f) ? HEDGE_LT : HEDGE);
            if(rng.chance(0.3f)) draw::rect(va, std::floor(c.x - 1), std::floor(c.y - 2), 1, 1, LEAF_LT);
        }
    }
    ground.draw(va); va.clear();

    // --- wire ----------------------------------------------------------------------------------
    for(const auto& w : wire)
    {
        sf::Vector2f const c = w.position + w.size * 0.5f;
        for(int k = 0; k < 40; ++k)
        {
            float const a = static_cast<float>(k) * 0.9f;
            float const r = 3.0f + std::fmod(static_cast<float>(k) * 1.7f, 6.0f);
            draw::rect(va, std::floor(c.x + std::cos(a) * r), std::floor(c.y + std::sin(a) * r), 1, 1, k % 3 ? mudcats::WIRE : lerp(mudcats::WIRE, MUD_DARK, 0.4f));
        }
        draw::rect(va, c.x - 1, c.y - 1, 2, 3, BARK);
    }
    ground.draw(va);
}

void Map::render_common(sf::RenderTexture& ground) const
{
    Rng rng(79);
    sf::VertexArray va(sf::PrimitiveType::Triangles);

    // --- low cover: sandbags, crates, trunks, stone walls -------------------------
    for(std::size_t i = 0; i < sandbags.size(); ++i) draw_low(va, sandbags[i], m_low[i], static_cast<std::uint32_t>(i * 977u + 79u));
    ground.draw(va); va.clear();

    // --- ammunition crates -----------------------------------------------------------
    for(auto c : crates)
    {
        draw::rect(va, c.x - 5, c.y - 3, 12, 8, SHADOW);
        draw::rect(va, c.x - 6, c.y - 4, 12, 8, lerp(PLANK, mudcats::GRASS, 0.3f));
        draw::rect(va, c.x - 6, c.y - 4, 12, 1, lerp(PLANK, PAPER, 0.3f));
        draw::rect(va, c.x - 1, c.y - 2, 2, 4, MON_WHITE);
    }

    // --- walls ------------------------------------------------------------------
    for(std::size_t i = 0; i < walls.size(); ++i)
    {
        const auto& w = walls[i];
        Material const m = m_materials[i];
        if(m == Material::TRUNK)
        {
            sf::Vector2f const c = w.position + w.size * 0.5f;
            draw::disc(va, c.x + 1, c.y + 1, 2.5f, SHADOW);
            draw::disc(va, c.x, c.y, 2.5f, BARK);
            draw::rect(va, std::floor(c.x - 1), std::floor(c.y - 1), 1, 1, PLANK);
            continue;
        }
        sf::Color const face = m == Material::CONCRETE ? CONCRETE : (m == Material::BRICK ? BRICK : (m == Material::STONE ? STONE : BARK));
        sf::Color const dark = m == Material::CONCRETE ? CONCRETE_DK : (m == Material::BRICK ? BRICK_DK : (m == Material::STONE ? STONE_DK : lerp(BARK, INK, 0.4f)));
        draw::rect(va, w.position.x + 3, w.position.y + 3, w.size.x, w.size.y, SHADOW);
        draw::rect(va, w.position.x, w.position.y, w.size.x, w.size.y, dark);
        draw::rect(va, w.position.x, w.position.y, w.size.x, w.size.y - 1, face);
        if(m == Material::BRICK || m == Material::STONE)
            for(float y = w.position.y; y < w.position.y + w.size.y; y += 3)
                for(float x = w.position.x + std::fmod(y, 6.0f); x < w.position.x + w.size.x; x += m == Material::STONE ? 5.0f : 6.0f)
                    draw::rect(va, x, y, 1, 2, dark);
        if(m == Material::LOG)
            for(float k = 0; k < std::max(w.size.x, w.size.y); k += 4)
            {
                if(w.size.x > w.size.y) draw::rect(va, w.position.x + k, w.position.y, 1, w.size.y, dark);
                else draw::rect(va, w.position.x, w.position.y + k, w.size.x, 1, dark);
            }
        draw::rect(va, w.position.x, w.position.y, w.size.x, 1, lerp(face, PAPER, 0.25f));
    }
    for(const auto& s : slits) draw::rect(va, s.position.x, s.position.y, s.size.x, s.size.y, INK);
    ground.draw(va);
}

void Map::build_grass_tops()
{
    // Blades that stand up over anyone lying among them.
    m_chunk_cols = static_cast<int>(std::ceil(size.x / 128.0f));
    m_chunk_rows = static_cast<int>(std::ceil(size.y / 128.0f));
    m_grass_tops.assign(static_cast<std::size_t>(m_chunk_cols * m_chunk_rows), sf::VertexArray(sf::PrimitiveType::Triangles));

    Rng rng(80);
    for(const auto& g : grass)
    {
        bool const wheat = g.position.y < 460.0f && g.position.x < 900.0f;
        for(float y = g.position.y; y < g.position.y + g.size.y; y += 3.0f)
            for(float x = g.position.x; x < g.position.x + g.size.x; x += 3.0f)
            {
                if(!rng.chance(0.55f)) continue;
                sf::Vector2f const p{std::floor(x + rng.real(0, 3)), std::floor(y + rng.real(0, 3))};
                sf::Color c = wheat ? (rng.chance(0.5f) ? WHEAT : lerp(WHEAT, PAPER, 0.25f)) : (rng.chance(0.5f) ? MEADOW_LT : lerp(MEADOW_LT, WHEAT, 0.3f));
                c.a = 215;
                int const cx = std::clamp(static_cast<int>(p.x / 128.0f), 0, m_chunk_cols - 1);
                int const cy = std::clamp(static_cast<int>(p.y / 128.0f), 0, m_chunk_rows - 1);
                draw::rect(m_grass_tops[static_cast<std::size_t>(cy * m_chunk_cols + cx)], p.x, p.y, 1, wheat ? 3.0f : 2.0f, c);
            }
    }
}

void Map::draw_over(sf::RenderTarget& target, sf::Vector2f viewer, const sf::FloatRect& visible, float time) const
{
    // --- grass tops --------------------------------------------------------------------
    int const x0 = std::max(0, static_cast<int>(visible.position.x / 128.0f));
    int const y0 = std::max(0, static_cast<int>(visible.position.y / 128.0f));
    int const x1 = std::min(m_chunk_cols - 1, static_cast<int>((visible.position.x + visible.size.x) / 128.0f));
    int const y1 = std::min(m_chunk_rows - 1, static_cast<int>((visible.position.y + visible.size.y) / 128.0f));
    // A gentle sway: the whole field shifts a pixel back and forth.
    sf::Transform sway;
    sway.translate({std::round(std::sin(time * 0.9f) * 0.7f), 0.0f});
    for(int cy = y0; cy <= y1; ++cy)
        for(int cx = x0; cx <= x1; ++cx)
            target.draw(m_grass_tops[static_cast<std::size_t>(cy * m_chunk_cols + cx)], sf::RenderStates(sway));

    sf::VertexArray va(sf::PrimitiveType::Triangles);

    // --- roofs -----------------------------------------------------------------------------
    for(const auto& r : roofs)
    {
        if(r.contains(viewer)) continue;
        bool const bunker = (level == LevelId::TRENCHES && r.position.x > 1000) || (level == LevelId::RAID && r.position.x > 1380);
        bool const mill = level == LevelId::FIELDS || (level == LevelId::RAID && r.position.x > 980 && r.position.x < 1380);
        sf::Color const top = bunker ? lerp(CONCRETE, MUD_LIGHT, 0.2f) : (mill ? lerp(BRICK, STONE, 0.4f) : BARK);
        sf::Color const edge = bunker ? CONCRETE_DK : (mill ? BRICK_DK : lerp(BARK, INK, 0.3f));
        draw::rect(va, r.position.x - 6, r.position.y - 6, r.size.x + 12, r.size.y + 12, edge);
        draw::rect(va, r.position.x - 5, r.position.y - 5, r.size.x + 10, r.size.y + 9, top);
        if(bunker)
        {
            draw::rect(va, r.position.x + 6, r.position.y + 10, 18, 1, CONCRETE_DK);
            draw::rect(va, r.position.x + 30, r.position.y + 24, 1, 12, CONCRETE_DK);
            for(int k = 0; k < 12; ++k)
                draw::rect(va, r.position.x - 4 + static_cast<float>((k * 37) % 56), r.position.y - 4 + static_cast<float>((k * 23) % 48), 3, 2, MUD_LIGHT);
        }
        else if(mill)
        {
            // Tiles in rows, a ridge down the middle, a few missing.
            for(float y = r.position.y - 4; y < r.position.y + r.size.y + 4; y += 3)
                draw::rect(va, r.position.x - 5, y, r.size.x + 10, 1, edge);
            draw::rect(va, r.position.x - 5, r.position.y + r.size.y * 0.5f - 1, r.size.x + 10, 2, lerp(edge, INK, 0.3f));
            for(int k = 0; k < 9; ++k)
                draw::rect(va, r.position.x + std::fmod(static_cast<float>(k * 41), std::max(8.0f, r.size.x - 4.0f)),
                           r.position.y + std::fmod(static_cast<float>(k * 29), std::max(8.0f, r.size.y - 3.0f)), 4, 3, INK);
        }
        else
            for(float y = r.position.y - 5; y < r.position.y + r.size.y + 4; y += 4)
                draw::rect(va, r.position.x - 5, y, r.size.x + 10, 1, lerp(BARK, INK, 0.4f));
    }

    // --- tree crowns --------------------------------------------------------------------------
    for(const auto& t : trees)
    {
        if(!visible.findIntersection(sf::FloatRect(t.pos - sf::Vector2f{t.canopy, t.canopy}, {t.canopy * 2, t.canopy * 2}))) continue;
        float const d = std::hypot(viewer.x - t.pos.x, viewer.y - t.pos.y);
        float const alpha = d < t.canopy + 6.0f ? 0.35f : 0.92f;
        sf::Color const dark = with_alpha(t.shade == 2 ? lerp(LEAF, MEADOW, 0.4f) : LEAF, alpha);
        sf::Color const mid  = with_alpha(t.shade == 1 ? lerp(LEAF_LT, WHEAT, 0.15f) : LEAF_LT, alpha);
        float const sx = std::sin(time * 0.7f + t.pos.x) * 0.6f;
        draw::disc(va, t.pos.x + sx, t.pos.y - 2, t.canopy, dark);
        draw::disc(va, t.pos.x - t.canopy * 0.3f + sx, t.pos.y - t.canopy * 0.35f - 2, t.canopy * 0.55f, mid);
        draw::disc(va, t.pos.x + t.canopy * 0.35f + sx, t.pos.y + t.canopy * 0.1f - 2, t.canopy * 0.4f, with_alpha(lerp(LEAF, LEAF_LT, 0.5f), alpha));
    }
    target.draw(va);
}

}
