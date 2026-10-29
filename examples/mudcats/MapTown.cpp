/**
 * MapTown.cpp  -  Mud Cats
 *
 * Kalvo, a market town at dusk. A main street runs east from the fields
 * to the square, and the church stands at the far end of the square, its
 * tower looking straight back down the street. Halfway along, a
 * cross-street cuts the houses into two blocks.
 *
 * The houses are terraced, two rows facing each other across the street.
 * Each has a door and two windows on the street, a back door and a window
 * onto its yard, and party walls shared with the next. Some party walls
 * have been knocked through, which is how you get along a row without
 * going out into the street. Behind the yards are low walls with gates,
 * a back lane, then gardens and sheds.
 *
 * A few houses are already burning when the fighting starts. Their fronts
 * have come down, and what's left is open to the sky.
 */

#include "Map.hpp"

#include "Draw.hpp"
#include "Palette.hpp"

#include <algorithm>
#include <cmath>
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
    };

    // Where things are, so the build and the drawing agree.
    constexpr float STREET_TOP = 470.0f, STREET_BOTTOM = 530.0f;
    constexpr float CROSS_LEFT = 820.0f, CROSS_RIGHT = 900.0f;
    constexpr float BLOCKS_END = 1480.0f;
    constexpr float SQUARE_LEFT = 1480.0f, SQUARE_RIGHT = 2010.0f, SQUARE_TOP = 270.0f, SQUARE_BOTTOM = 740.0f;
    constexpr float NORTH_LANE = 200.0f, SOUTH_LANE = 770.0f;     // the back lanes, 30 deep
}

void Map::build_town()
{
    size = {2400.0f, 1000.0f};
    Rng rng(1938);
    player_start = {90.0f, 500.0f};

    /*
     * One house in a terrace, from x0 to x1 and y0 to y1. `front_top` says
     * which side faces the street. Neighbours share a party wall: each house
     * builds the wall on its left, and the last in the row the one on its
     * right. `hole` knocks a way through the left one.
     */
    auto house = [&](float x0, float x1, float y0, float y1, bool front_top, Material m,
                     bool first, bool last, bool hole, bool ruined)
    {
        float const w = x1 - x0, h = y1 - y0;
        float const fy = front_top ? y0 : y1 - 6.0f;
        float const by = front_top ? y1 - 6.0f : y0;
        float const xc = std::round((x0 + x1) * 0.5f);

        // The front: a door in the middle, a window either side. A ruin has
        // only the ends of it standing, and a heap of what fell inside.
        if(ruined)
        {
            wall(R(x0, fy, 22, 6), m);
            wall(R(x1 - 22, fy, 22, 6), m);
            float const heap_y = front_top ? y0 + 26.0f : y1 - 40.0f;
            low(R(x0 + 14, heap_y, 22, 14), Low::STONE);
            low(R(x1 - 40, heap_y + (front_top ? 34.0f : -30.0f), 24, 12), Low::STONE);
            burning.push_back({x0 + 25.0f, heap_y + 7.0f});
        }
        else
        {
            windowed(R(x0, fy, xc - 12.0f - x0, 6), m, {x0 + 18.0f});
            windowed(R(xc + 12.0f, fy, x1 - xc - 12.0f, 6), m, {x1 - 18.0f});
        }
        // The back: a door to the yard near one end, a window at the other.
        wall(R(x0, by, 12, 6), m);
        windowed(R(x0 + 36.0f, by, w - 36.0f, 6), m, {x1 - 22.0f});

        // The sides. The ends of a row look out, so they have windows.
        float const sy = y0 + 6.0f, sh = h - 12.0f;
        if(first) windowed(R(x0, sy, 6, sh), m, {sy + sh * 0.3f, sy + sh * 0.72f});
        else if(hole)
        {
            float const mid = std::round(sy + sh * 0.5f);
            wall(R(x0, sy, 6, mid - 12.0f - sy), m);
            wall(R(x0, mid + 12.0f, 6, sy + sh - mid - 12.0f), m);
        }
        else wall(R(x0, sy, 6, sh), m);
        if(last) windowed(R(x1 - 6.0f, sy, 6, sh), m, {sy + sh * 0.3f, sy + sh * 0.72f});

        if(!ruined) roofs.push_back(R(x0 + 6.0f, y0 + 6.0f, w - (last ? 12.0f : 6.0f), h - 12.0f));
        floors.push_back(R(x0, y0, w, h));
    };
    auto row = [&](std::initializer_list<float> edges, float y0, float y1, bool front_top,
                   std::initializer_list<float> holes, std::initializer_list<float> ruins)
    {
        std::vector<float> const xs(edges);
        for(std::size_t i = 0; i + 1 < xs.size(); ++i)
        {
            float const x0 = xs[i], x1 = xs[i + 1];
            bool const hole = std::find(holes.begin(), holes.end(), x0) != holes.end();
            bool const ruined = std::find(ruins.begin(), ruins.end(), x0) != ruins.end();
            Material const m = (static_cast<int>(x0) / 10 + static_cast<int>(y0)) % 3 == 0 ? Material::STONE : Material::BRICK;
            house(x0, x1, y0, y1, front_top, m, i == 0, i + 2 == xs.size(), hole, ruined);
        }
    };
    // The yards behind a row: a low wall between each, and a back wall onto the lane with a gate in it.
    auto yards = [&](std::initializer_list<float> edges, float y0, float y1, bool lane_top)
    {
        std::vector<float> const xs(edges);
        for(std::size_t i = 0; i < xs.size(); ++i)
            if(i > 0 && i + 1 < xs.size()) low(R(xs[i] - 3.0f, y0 + 6.0f, 6, y1 - y0 - 12.0f), Low::STONE);
        float const wy = lane_top ? y0 : y1 - 6.0f;
        for(std::size_t i = 0; i + 1 < xs.size(); ++i)
        {
            float const xc = std::round((xs[i] + xs[i + 1]) * 0.5f);
            low(R(xs[i], wy, xc - 13.0f - xs[i], 6), Low::STONE);
            low(R(xc + 13.0f, wy, xs[i + 1] - xc - 13.0f, 6), Low::STONE);
            // A woodpile or a cart in most of them.
            if(hashf(xs[i], y0) < 0.6f)
            {
                float const yy = lane_top ? y0 + 30.0f : y1 - 44.0f;
                low(R(xs[i] + 14.0f + hashf(xs[i], 3.0f) * 30.0f, yy, 16, 10), Low::CRATE);
            }
        }
    };

    // --- the first block, west of the cross-street ------------------------------------------
    row({250, 350, 460, 560, 680, 820}, 340, STREET_TOP, false, {350, 560}, {350});
    row({250, 360, 450, 570, 690, 820}, STREET_BOTTOM, 660, true, {450, 690}, {570});
    yards({250, 350, 460, 560, 680, 820}, 230, 340, true);
    yards({250, 360, 450, 570, 690, 820}, 660, 770, false);

    // --- the second block, east of it, with the inn in it ---------------------------------
    row({900, 1010, 1120, 1260, 1370, BLOCKS_END}, 340, STREET_TOP, false, {1010, 1370}, {});
    row({900, 1000, 1130, 1240, 1360, BLOCKS_END}, STREET_BOTTOM, 660, true, {1130, 1360}, {1130});
    yards({900, 1010, 1120, 1260, 1370, BLOCKS_END}, 230, 340, true);
    yards({900, 1000, 1130, 1240, 1360, BLOCKS_END}, 660, 770, false);
    // The inn: two rooms, a doorway between.
    wall(R(1190, 346, 6, 47), Material::BRICK);
    wall(R(1190, 417, 6, 47), Material::BRICK);

    // --- barricades: across the street at the cross-street, and into the square ---------------
    low(R(846, 474, 8, 52), Low::BAGS);
    low(R(1500, 474, 12, 26), Low::CRATE);                  // a cart on its side
    low(R(1502, 500, 8, 26), Low::BAGS);

    // --- the square: the town hall on its north side, houses on its south ----------------------
    {
        Material const m = Material::STONE;
        windowed(R(1540, 264, 168, 6), m, {1580, 1630, 1680});      // the front, door at 1708..1732
        windowed(R(1732, 264, 168, 6), m, {1770, 1820, 1870});
        windowed(R(1540, 150, 260, 6), m, {1600, 1700});            // the back, door at 1800..1824
        windowed(R(1824, 150, 76, 6), m, {1866});
        windowed(R(1540, 156, 6, 108), m, {200, 232});
        windowed(R(1894, 156, 6, 108), m, {200, 232});
        wall(R(1720, 156, 6, 24), m);                                // between its two rooms
        wall(R(1720, 204, 6, 60), m);
        roofs.push_back(R(1546, 156, 348, 108));
        floors.push_back(R(1540, 150, 360, 120));
    }
    row({1530, 1660, 1800, 1950}, SQUARE_BOTTOM, 850, true, {1660}, {});

    // The fountain in the middle, its low basin a ring of stone.
    low(R(1742, 482, 36, 6), Low::STONE);
    low(R(1742, 512, 36, 6), Low::STONE);
    low(R(1742, 488, 6, 24), Low::STONE);
    low(R(1772, 488, 6, 24), Low::STONE);
    // Market stalls tipped over, a cart.
    for(auto r : {R(1596, 356, 18, 10), R(1634, 640, 20, 10), R(1874, 372, 18, 10), R(1900, 634, 16, 12), R(1822, 600, 24, 12), R(1660, 470, 12, 10)})
        low(r, Low::CRATE);
    burning.push_back({1834.0f, 606.0f});                           // the cart is on fire
    for(sf::Vector2f p : {sf::Vector2f{1502, 292}, {1990, 294}, {1500, 716}, {1990, 718}})
        trees.push_back({p, 15.0f, 0});
    // Gardens and orchards round the square, behind its buildings.
    for(sf::Vector2f p : {sf::Vector2f{1560, 60}, {1660, 100}, {1780, 50}, {1950, 90}, {2080, 160}, {2180, 260}, {2060, 300},
                          {1600, 920}, {1720, 960}, {1880, 900}, {2010, 860}, {2100, 760}, {2190, 690}, {2050, 650}})
        trees.push_back({p, rng.real(12, 17), 2});
    hedges.push_back(R(1530, 130, 200, 8));
    hedges.push_back(R(1540, 870, 260, 8));
    hedges.push_back(R(2030, 360, 8, 120));
    hedges.push_back(R(2030, 560, 8, 140));

    // --- the church, its tower facing down the street -------------------------------------------
    {
        Material const m = Material::STONE;
        // The tower: a door in its west face, a window either side of it.
        windowed(R(2010, 466, 6, 22), m, {477});
        windowed(R(2010, 512, 6, 22), m, {523});
        wall(R(2016, 466, 50, 6), m);
        wall(R(2016, 528, 50, 6), m);
        // The nave: a doorway from the tower, windows down both sides, a door to the graveyard.
        windowed(R(2066, 400, 6, 66), m, {430});
        windowed(R(2066, 534, 6, 66), m, {570});
        wall(R(2066, 466, 6, 22), m);
        wall(R(2066, 512, 6, 22), m);
        windowed(R(2066, 400, 200, 6), m, {2110, 2170, 2230});
        windowed(R(2066, 594, 200, 6), m, {2110, 2170, 2230});
        windowed(R(2260, 406, 6, 82), m, {440});
        windowed(R(2260, 512, 6, 82), m, {560});
        roofs.push_back(R(2016, 472, 50, 56));
        roofs.push_back(R(2072, 406, 188, 188));
        floors.push_back(R(2010, 466, 56, 68));
        floors.push_back(R(2066, 400, 200, 200));
        // Pews either side of the aisle, the altar at the far end.
        for(float y : {426.0f, 450.0f, 548.0f, 572.0f})
        {
            low(R(2096, y, 64, 5), Low::CRATE);
            low(R(2172, y, 64, 5), Low::CRATE);
        }
        low(R(2238, 486, 10, 28), Low::STONE);
    }

    // --- the graveyard behind it -------------------------------------------------------------------
    low(R(2272, 300, 128, 6), Low::STONE);
    low(R(2272, 694, 128, 6), Low::STONE);
    low(R(2272, 306, 6, 94), Low::STONE);
    low(R(2272, 600, 6, 94), Low::STONE);
    for(float y = 324.0f; y < 680.0f; y += 30.0f)
        for(float x = 2296.0f; x < 2392.0f; x += 26.0f)
        {
            if(std::abs(y - 500.0f) < 26.0f || hashf(x, y) < 0.3f) continue;
            low(R(x + hashf(y, x) * 6.0f, y, 6, 3), Low::STONE);
        }
    for(sf::Vector2f p : {sf::Vector2f{2300, 340}, {2380, 640}, {2360, 330}})
        trees.push_back({p, 13.0f, 2});

    // --- the back lanes, gardens and sheds ------------------------------------------------------
    auto shed = [&](float x, float y, bool door_down)
    {
        wall(R(x, y, 36, 5), Material::LOG);
        wall(R(x, y + 23, 36, 5), Material::LOG);
        wall(R(x, y + 5, 5, 18), Material::LOG);
        if(door_down) wall(R(x + 31, y + 5, 5, 6), Material::LOG);
        else wall(R(x + 31, y + 5, 5, 18), Material::LOG);
        roofs.push_back(R(x + 5, y + 5, 26, 18));
        floors.push_back(R(x, y, 36, 28));
    };
    for(sf::Vector2f p : {sf::Vector2f{300, 120}, {610, 70}, {1040, 120}, {1330, 60}})
        shed(p.x, p.y, true);
    for(sf::Vector2f p : {sf::Vector2f{420, 860}, {760, 900}, {1110, 850}, {1420, 920}})
        shed(p.x, p.y, false);
    for(float x : {240.0f, 520.0f, 940.0f, 1220.0f})
    {
        hedges.push_back(R(x, NORTH_LANE - 10.0f, 180, 8));
        hedges.push_back(R(x + 30.0f, SOUTH_LANE + 32.0f, 160, 8));
    }
    for(int i = 0, tries = 0; i < 26 && tries < 3000; ++tries)
    {
        sf::Vector2f const p{rng.real(250, 1480), rng.chance(0.5f) ? rng.real(20, 170) : rng.real(830, 985)};
        bool ok = true;
        for(const auto& t : trees) if(std::hypot(t.pos.x - p.x, t.pos.y - p.y) < 34.0f) ok = false;
        for(const auto& f : floors)
            if(std::abs(p.x - f.getCenter().x) < f.size.x * 0.5f + 16.0f && std::abs(p.y - f.getCenter().y) < f.size.y * 0.5f + 16.0f) ok = false;
        for(const auto& h : hedges) if(h.findIntersection(R(p.x - 10, p.y - 10, 20, 20))) ok = false;
        if(p.x > CROSS_LEFT - 20.0f && p.x < CROSS_RIGHT + 20.0f) ok = false;
        if(!ok) continue;
        trees.push_back({p, rng.real(11, 16), 1});
        ++i;
    }

    // --- outside the town: the fields we come in from ---------------------------------------
    hedges.push_back(R(20, 300, 200, 8));
    hedges.push_back(R(20, 700, 200, 8));
    low(R(186, 478, 22, 12), Low::CRATE);                   // a wrecked cart at the edge of town
    low(R(212, 520, 6, 30), Low::STONE);                    // the first garden wall
    low(R(212, 448, 6, 22), Low::STONE);
    for(sf::Vector2f p : {sf::Vector2f{60, 210}, {150, 380}, {40, 620}, {170, 800}, {100, 920}})
        trees.push_back({p, rng.real(13, 17), 1});

    // Shell holes in the streets and the square.
    for(sf::Vector2f p : {sf::Vector2f{170, 516}, {560, 492}, {1060, 512}, {1320, 488}, {1610, 420}, {1700, 610}, {1890, 520}, {860, 300}, {860, 690}})
        craters.push_back({p, rng.real(9, 13)});

    crates = {{120, 560}, {2120, 520}};
    waypoints = {{170, 500}, {330, 500}, {520, 500}, {720, 500}, {960, 500}, {1180, 500}, {1400, 500}, {1640, 500}, {2140, 500}};
    block_a = R(240, 330, 590, 340);
    block_b = R(890, 330, 600, 340);
    square_area = R(SQUARE_LEFT, 150, SQUARE_RIGHT - SQUARE_LEFT, 710);
    church_area = R(2010, 400, 256, 200);
}

void Map::render_town(sf::RenderTexture& ground) const
{
    Rng rng(83);
    sf::VertexArray va(sf::PrimitiveType::Triangles);
    ground.clear(MUD);

    auto in = [](float x, float y, float x0, float y0, float x1, float y1) { return x >= x0 && x < x1 && y >= y0 && y < y1; };
    auto paved = [&](float x, float y)
    {
        return in(x, y, 0, STREET_TOP, SQUARE_LEFT, STREET_BOTTOM) || in(x, y, CROSS_LEFT, 0, CROSS_RIGHT, size.y)
            || in(x, y, SQUARE_LEFT, SQUARE_TOP, SQUARE_RIGHT, SQUARE_BOTTOM) || in(x, y, SQUARE_RIGHT, 466, 2272, 534)
            || in(x, y, 1500, 150, 1540, SQUARE_TOP) || in(x, y, 1900, 150, SQUARE_RIGHT, SQUARE_TOP);
    };
    auto lane = [&](float x, float y)
    {
        return x > 230.0f && x < BLOCKS_END + 20.0f && (in(x, y, 0, NORTH_LANE, size.x, NORTH_LANE + 30.0f) || in(x, y, 0, SOUTH_LANE, size.x, SOUTH_LANE + 30.0f));
    };

    // --- the ground: gardens and yards, trodden earth, the graveyard's grass -----------------
    for(float y = 0; y < size.y; y += 4.0f)
        for(float x = 0; x < size.x; x += 4.0f)
        {
            float const m = field(x / 50.0f, y / 50.0f) * 0.65f + field(x / 12.0f, y / 12.0f) * 0.35f;
            bool const garden = (y < NORTH_LANE || y > SOUTH_LANE + 30.0f) && x > 230.0f && x < BLOCKS_END + 20.0f;
            bool const green = garden || x < 230.0f || x > BLOCKS_END + 20.0f;
            sf::Color c = green ? (m < 0.35f ? MEADOW_DK : (m < 0.7f ? lerp(MEADOW_DK, GRASS_DEAD, 0.5f) : GRASS_DEAD))
                                : (m < 0.35f ? MUD_DARK : (m < 0.72f ? MUD : MUD_LIGHT));
            if(lane(x, y)) c = m < 0.5f ? lerp(MUD_LIGHT, ROAD, 0.4f) : MUD_LIGHT;
            draw::rect(va, x, y, 4, 4, c);
        }
    ground.draw(va); va.clear();

    // Vegetable rows in the gardens, and grass tufts.
    for(float x = 260.0f; x < BLOCKS_END; x += 70.0f)
        for(float yb : {30.0f, 840.0f})
        {
            if(hashf(x, yb) < 0.35f) continue;
            for(float y = yb; y < yb + 120.0f; y += 6.0f)
                for(float k = 0; k < 44.0f; k += 3.0f)
                    if(hashf(x + k, y) < 0.7f) draw::rect(va, x + k, y, 2, 2, hashf(k, y) < 0.5f ? MEADOW_LT : lerp(MEADOW_LT, WHEAT, 0.3f));
        }
    for(int i = 0; i < 2600; ++i)
    {
        float const x = rng.real(0, size.x), y = rng.real(0, size.y);
        if(paved(x, y)) continue;
        sf::Color const c = rng.chance(0.5f) ? mudcats::GRASS : GRASS_DEAD;
        for(int b = 0; b < 3; ++b) draw::rect(va, std::floor(x + b), std::floor(y - rng.real(1, 4)), 1, rng.real(2, 3), c);
    }
    ground.draw(va); va.clear();

    // --- cobbles: the street, the cross-street, the square, the way into the church --------------
    sf::Color const joint = lerp(STONE_DK, INK, 0.45f);
    for(float y = 0; y < size.y; y += 3.0f)
        for(float x = 0; x < size.x; x += 4.0f)
        {
            if(!paved(x, y)) continue;
            draw::rect(va, x, y, 4, 3, joint);
            float const off = std::fmod(y / 3.0f, 2.0f) * 2.0f;
            float const h = hashf(x + off, y);
            bool const square = in(x, y, SQUARE_LEFT, SQUARE_TOP, SQUARE_RIGHT, SQUARE_BOTTOM);
            sf::Color const stone = h < 0.33f ? STONE : (h < 0.66f ? lerp(STONE, STONE_DK, 0.5f) : STONE_DK);
            draw::rect(va, x + off, y, 3, 2, square ? lerp(stone, SANDBAG, 0.18f) : lerp(stone, MUD_LIGHT, 0.12f));
        }
    ground.draw(va); va.clear();
    // Kerbs and gutters along the main street.
    for(float x = 0; x < BLOCKS_END; x += 1.0f)
    {
        if(x > CROSS_LEFT && x < CROSS_RIGHT) continue;
        draw::rect(va, x, STREET_TOP, 1, 4, lerp(STONE, PAPER, 0.1f));
        draw::rect(va, x, STREET_BOTTOM - 4, 1, 4, lerp(STONE, PAPER, 0.1f));
        draw::rect(va, x, STREET_TOP + 4, 1, 1, joint);
        draw::rect(va, x, STREET_BOTTOM - 5, 1, 1, joint);
    }
    // The square: a ring of lighter setts round the fountain.
    for(int k = 0; k < 360; ++k)
    {
        float const a = static_cast<float>(k) * 0.01745f * 1.0f;
        for(float r : {44.0f, 47.0f, 90.0f})
            draw::rect(va, std::floor(1760.0f + std::cos(a) * r), std::floor(500.0f + std::sin(a) * r), 2, 2, lerp(STONE, PAPER, 0.2f));
    }
    ground.draw(va); va.clear();

    // --- floors: boards in the houses, tiles in the town hall, flags in the church ------------
    for(const auto& f : floors)
    {
        bool roofed = false;
        for(const auto& r : roofs) if(r.findIntersection(f)) roofed = true;
        bool const church = church_area.contains(f.getCenter());
        bool const hall = f.size.x > 300.0f;
        for(float y = f.position.y; y < f.position.y + f.size.y; y += 4.0f)
            for(float x = f.position.x; x < f.position.x + f.size.x; x += 8.0f)
            {
                float const w = std::min(8.0f, f.position.x + f.size.x - x), h = std::min(4.0f, f.position.y + f.size.y - y);
                sf::Color c;
                if(church) c = (static_cast<int>(x / 8) + static_cast<int>(y / 8)) % 2 ? lerp(STONE, PAPER, 0.08f) : STONE_DK;
                else if(hall) c = (static_cast<int>(x / 8) + static_cast<int>(y / 4)) % 2 ? lerp(BRICK_DK, STONE_DK, 0.5f) : lerp(PAPER_DK, STONE, 0.5f);
                else c = static_cast<int>(y / 4) % 2 ? PLANK : PLANK_DARK;
                if(!roofed) c = lerp(c, INK, 0.35f + 0.3f * hashf(x, y));        // burnt out, open to the sky
                draw::rect(va, x, y, w, h, c);
                if(!church && !hall && hashf(x * 0.7f, y) < 0.12f) draw::rect(va, x, y, 1, h, PLANK_DARK);
            }
        // What's lying about: rubble in the ruins, a rug or a table in the rest.
        if(!roofed)
            for(int k = 0; k < 60; ++k)
                draw::rect(va, std::floor(f.position.x + rng.real(4, f.size.x - 6)), std::floor(f.position.y + rng.real(4, f.size.y - 6)), 2, 2,
                           rng.chance(0.4f) ? CHAR : (rng.chance(0.5f) ? STONE_DK : BRICK_DK));
        else if(!church && !hall && f.size.x > 60.0f && rng.chance(0.5f))
        {
            sf::Vector2f const c = f.getCenter() + sf::Vector2f{rng.real(-14, 14), rng.real(-12, 12)};
            sf::Color const rug = rng.chance(0.4f) ? RCH_RED : (rng.chance(0.5f) ? MON_BLUE_DK : WHEAT_DK);
            draw::rect(va, c.x - 11, c.y - 7, 22, 14, lerp(rug, MUD_DARK, 0.5f));
            draw::rect(va, c.x - 9, c.y - 5, 18, 10, lerp(rug, MUD, 0.4f));
        }
    }
    ground.draw(va); va.clear();
    // The church: a runner up the aisle.
    draw::rect(va, 2072, 494, 166, 12, lerp(RCH_RED, MUD_DARK, 0.3f));
    draw::rect(va, 2016, 494, 50, 12, lerp(RCH_RED, MUD_DARK, 0.3f));

    // --- hedges ---------------------------------------------------------------------------------------
    for(const auto& h : hedges)
    {
        draw::rect(va, h.position.x + 2, h.position.y + 3, h.size.x, h.size.y, SHADOW);
        for(float x = h.position.x; x < h.position.x + h.size.x; x += 3.0f)
        {
            draw::disc(va, x + 1.5f, h.position.y + h.size.y * 0.5f, h.size.y * 0.6f, HEDGE);
            if(hashf(x, h.position.y) < 0.5f) draw::disc(va, x + 1.0f, h.position.y + 2.0f, 1.6f, HEDGE_LT);
        }
    }
    ground.draw(va); va.clear();

    // --- shell holes in the setts ---------------------------------------------------------------------
    for(const auto& c : craters)
    {
        for(int k = 0; k < 18; ++k)
        {
            float const a = 6.283f * static_cast<float>(k) / 18.0f + rng.real(-0.15f, 0.15f);
            float const d = c.radius + rng.real(0.0f, 4.0f);
            draw::rect(va, std::floor(c.pos.x + std::cos(a) * d), std::floor(c.pos.y + std::sin(a) * d * 0.9f), 3, 2, rng.chance(0.5f) ? STONE : STONE_DK);
        }
        draw::disc(va, c.pos.x, c.pos.y, c.radius, MUD_DARK);
        draw::disc(va, c.pos.x + 1, c.pos.y + 1, c.radius * 0.6f, lerp(MUD_DARK, INK, 0.3f));
    }

    // Glass, slates and brick ends in the street below the windows.
    for(int i = 0; i < 1400; ++i)
    {
        sf::Vector2f const p{rng.real(240, 2010), rng.chance(0.5f) ? rng.real(STREET_TOP + 2, STREET_TOP + 14) : rng.real(STREET_BOTTOM - 14, STREET_BOTTOM - 2)};
        sf::Color const c = rng.chance(0.15f) ? lerp(PAPER, MIST, 0.4f) : (rng.chance(0.5f) ? lerp(STONE_DK, INK, 0.3f) : BRICK_DK);
        draw::rect(va, std::floor(p.x), std::floor(p.y), 1, 1, c);
    }
    // Scorch marks round the fires that are still going.
    for(auto b : burning)
        for(int k = 0; k < 30; ++k)
        {
            float const a = rng.real(0, 6.283f), d = rng.real(0, 18);
            draw::disc(va, b.x + std::cos(a) * d, b.y + std::sin(a) * d * 0.8f, rng.real(1.5f, 3.5f), with_alpha(CHAR, 0.6f));
        }
    ground.draw(va);
}

}
