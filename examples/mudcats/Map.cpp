/**
 * Map.cpp  -  Mud Cats
 *
 * Layouts for the five levels, the queries the game asks of them, and how
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

void Map::windowed(sf::FloatRect r, Material m, std::initializer_list<float> at, float width)
{
    // Along the wall's long side: wall, window, wall, window, wall.
    bool const across = r.size.x >= r.size.y;
    float const start = across ? r.position.x : r.position.y;
    float const end = start + (across ? r.size.x : r.size.y);
    auto piece = [&](float a, float b) { return across ? R(a, r.position.y, b - a, r.size.y) : R(r.position.x, a, r.size.x, b - a); };
    float from = start;
    for(float c : at)
    {
        float const a = std::max(from, c - width * 0.5f), b = std::min(end, c + width * 0.5f);
        if(b <= a) continue;
        if(a > from) wall(piece(from, a), m);
        slits.push_back(piece(a, b));
        windows.push_back(piece(a, b));
        from = b;
    }
    if(end > from) wall(piece(from, end), m);
}

void Map::low(sf::FloatRect r, Low kind)
{
    sandbags.push_back(r);
    m_low.push_back(kind);
}

void Map::build(LevelId id)
{
    level = id;
    walls.clear(); sandbags.clear(); hedges.clear(); slits.clear(); windows.clear(); wire.clear(); roofs.clear();
    floors.clear(); burning.clear();
    block_a = block_b = square_area = church_area = {};
    grass.clear(); water.clear(); bridges.clear(); trenches.clear(); craters.clear(); trees.clear();
    cover.clear(); crates.clear(); waypoints.clear();
    m_materials.clear(); m_low.clear();
    front_trench_area = wood_area = road_area = {};
    road.clear(); ambush_area = {};

    if(id == LevelId::TOWN) build_town();
    else if(id == LevelId::FOREST) build_forest();
    else build_charge();

    // Every tree has a trunk you can hide behind and can't walk through.
    for(const auto& t : trees) wall(R(t.pos.x - 2.0f, t.pos.y - 2.0f, 4.0f, 4.0f), Material::TRUNK);

    rasterise();
    place_cover();
    build_grass_tops();
}

void Map::build_charge()
{
    size = {2800.0f, 1100.0f};
    Rng rng(1940);
    player_start = {410.0f, 550.0f};

    // --- our trench: dug last night, shallow, and not finished --------------------------
    trenches.push_back({{{404, 90}, {420, 210}, {398, 330}, {418, 450}, {400, 570}, {420, 690}, {398, 810}, {416, 930}, {404, 1010}}, 18.0f});
    trenches.push_back({{{400, 570}, {300, 584}, {180, 576}, {40, 590}}, 14.0f});      // back to the road
    front_trench_area = R(380, 90, 60, 920);
    // Bags along the front of it here and there, where they had the bags.
    for(float y : {150.0f, 260.0f, 420.0f, 530.0f, 640.0f, 860.0f, 950.0f})
        low(R(436, y, 6, 36), Low::BAGS);
    // Two gun pits, a little forward.
    for(float y : {360.0f, 760.0f})
    {
        low(R(458, y - 14, 6, 28), Low::BAGS);
        low(R(440, y - 16, 22, 5), Low::BAGS);
        low(R(440, y + 11, 22, 5), Low::BAGS);
    }
    // The wire they didn't finish: stakes, and a few lengths of it strung between.
    for(float y = 120.0f; y < 1000.0f; y += 60.0f)
    {
        if(hashf(y, 4.0f) < 0.45f) wire.push_back(R(600, y, 16, 34));
        low(R(606, y - 3, 3, 3), Low::TRUNK);
    }
    // The company's dugout and a barn behind the line.
    wall(R(150, 300, 70, 6), Material::LOG);
    wall(R(150, 300, 6, 40), Material::LOG);
    wall(R(214, 300, 6, 40), Material::LOG);
    roofs.push_back(R(156, 306, 58, 34));
    windowed(R(140, 760, 120, 6), Material::BRICK, {172, 228});
    windowed(R(140, 860, 120, 6), Material::BRICK, {172, 228});
    windowed(R(140, 760, 6, 106), Material::BRICK, {813});
    for(auto r : {R(254, 760, 6, 40), R(254, 826, 6, 40)})
        wall(r, Material::BRICK);
    roofs.push_back(R(146, 766, 108, 94));
    crates = {{372, 300}, {372, 800}, {200, 560}};

    // --- the meadow: open, a few shell holes, a broken fence, nothing else ----------------
    for(int i = 0, tries = 0; i < 14 && tries < 2000; ++tries)
    {
        sf::Vector2f const p{rng.real(760, 2100), rng.real(120, 980)};
        float const r = rng.real(8, 14);
        bool ok = true;
        for(const auto& c : craters) if(std::hypot(c.pos.x - p.x, c.pos.y - p.y) < 160.0f) ok = false;
        if(!ok) continue;
        craters.push_back({p, r});
        ++i;
    }
    for(int k = 0; k < 22; ++k)
    {
        if(hashf(static_cast<float>(k), 7.0f) < 0.3f) continue;          // the gaps
        float const t = static_cast<float>(k) / 21.0f;
        sf::Vector2f const p{1180.0f + 260.0f * t, 90.0f + 920.0f * t};
        low(R(p.x - 2, p.y - 2, 4, 4), Low::TRUNK);
    }
    // The far end, where the grass hasn't been walked flat: they form up in it.
    grass.push_back(R(2150, 80, 260, 940));
    grass.push_back(R(1600, 140, 180, 200));
    grass.push_back(R(1500, 760, 220, 180));

    // --- the wood they come out of ----------------------------------------------------------
    for(int i = 0, tries = 0; i < 60 && tries < 5000; ++tries)
    {
        sf::Vector2f const p{rng.real(2560, 2790), rng.real(20, 1080)};
        bool ok = true;
        for(const auto& t : trees) if(std::hypot(t.pos.x - p.x, t.pos.y - p.y) < 26.0f) ok = false;
        if(!ok) continue;
        trees.push_back({p, rng.real(12, 19), rng.integer(0, 2)});
        ++i;
    }
    wood_area = R(2540, 0, 260, 1100);
    // Behind us, the road and a few trees.
    for(sf::Vector2f p : {sf::Vector2f{80, 200}, {60, 420}, {90, 980}, {250, 1040}, {300, 120}})
        trees.push_back({p, rng.real(12, 16), 1});

    waypoints = {{410, 550}, {2400, 550}};
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

void Map::add_low(sf::FloatRect r)
{
    sandbags.push_back(r);
    m_low.push_back(Low::CRATE);
    mark_rect(sf::FloatRect(r.position - sf::Vector2f{2, 2}, r.size + sf::Vector2f{4, 4}), SOLID, false);
    cover_around_low(r);
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

    // In the town, a place at every window, inside and out: where you'd stand to fight from a house.
    if(level == LevelId::TOWN)
        for(const auto& w : windows)
        {
            sf::Vector2f const c = w.getCenter();
            if(w.size.x >= w.size.y)
            {
                add({c.x, w.position.y - 7.0f}, {0, 1}, false);
                add({c.x, w.position.y + w.size.y + 7.0f}, {0, -1}, false);
            }
            else
            {
                add({w.position.x - 7.0f, c.y}, {1, 0}, false);
                add({w.position.x + w.size.x + 7.0f, c.y}, {-1, 0}, false);
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

float Map::roof_open(const sf::FloatRect& roof, sf::Vector2f viewer)
{
    // From the nearest point of the building, walls and all.
    float const x0 = roof.position.x - 6.0f, y0 = roof.position.y - 6.0f;
    float const x1 = roof.position.x + roof.size.x + 6.0f, y1 = roof.position.y + roof.size.y + 6.0f;
    float const dx = std::max({x0 - viewer.x, 0.0f, viewer.x - x1});
    float const dy = std::max({y0 - viewer.y, 0.0f, viewer.y - y1});
    float const d = std::sqrt(dx * dx + dy * dy);
    return std::clamp((ROOF_SHUT - d) / (ROOF_SHUT - ROOF_OPEN), 0.0f, 1.0f);
}

sf::Vector2f Map::road_at(float along) const
{
    if(road.size() < 2) return {};
    float left = std::max(0.0f, along);
    for(std::size_t i = 0; i + 1 < road.size(); ++i)
    {
        sf::Vector2f const d = road[i + 1] - road[i];
        float const len = std::hypot(d.x, d.y);
        if(left <= len) return road[i] + d * (left / len);
        left -= len;
    }
    return road.back();
}

sf::Vector2f Map::road_dir(float along) const
{
    sf::Vector2f const a = road_at(along - 6.0f), b = road_at(along + 6.0f);
    float const l = std::hypot(b.x - a.x, b.y - a.y);
    return l > 0.001f ? (b - a) / l : sf::Vector2f{1.0f, 0.0f};
}

float Map::road_length() const
{
    float total = 0.0f;
    for(std::size_t i = 0; i + 1 < road.size(); ++i) total += std::hypot(road[i + 1].x - road[i].x, road[i + 1].y - road[i].y);
    return total;
}

float Map::road_distance(sf::Vector2f p) const
{
    float best = 1e9f;
    for(std::size_t i = 0; i + 1 < road.size(); ++i) best = std::min(best, distance_to_segment(p, road[i], road[i + 1]));
    return best;
}

float Map::roof_open_at(sf::Vector2f p, sf::Vector2f viewer) const
{
    for(const auto& r : roofs) if(r.contains(p)) return roof_open(r, viewer);
    return 1.0f;
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
    if(level == LevelId::TOWN) render_town(ground);
    else if(level == LevelId::FOREST) render_forest(ground);
    else render_charge(ground);
    render_common(ground);
    ground.display();
}

void Map::draw_low(sf::VertexArray& va, const sf::FloatRect& s, Low kind, std::uint32_t seed) const
{
    Rng rng(seed);
    draw::rect(va, s.position.x + 2, s.position.y + 2, s.size.x, s.size.y, SHADOW);
    if(kind == Low::LOG)
    {
        // A fallen tree: bark along it, the sawn or broken ends paler.
        bool const across = s.size.x >= s.size.y;
        draw::rect(va, s.position.x, s.position.y, s.size.x, s.size.y, BARK);
        float const len = across ? s.size.x : s.size.y;
        for(float k = 1.0f; k < len - 1.0f; k += 3.0f)
        {
            sf::Color const c = rng.chance(0.5f) ? lerp(BARK, INK, 0.3f) : lerp(BARK, MUD_LIGHT, 0.4f);
            if(across) draw::rect(va, s.position.x + k, s.position.y + 1, 1, s.size.y - 2, c);
            else draw::rect(va, s.position.x + 1, s.position.y + k, s.size.x - 2, 1, c);
        }
        if(across) draw::rect(va, s.position.x, s.position.y, s.size.x, 1, lerp(BARK, PAPER, 0.2f));
        else draw::rect(va, s.position.x, s.position.y, 1, s.size.y, lerp(BARK, PAPER, 0.2f));
        sf::Color const end = lerp(PLANK, PAPER, 0.25f);
        if(across) { draw::rect(va, s.position.x, s.position.y + 1, 2, s.size.y - 2, end); draw::rect(va, s.position.x + s.size.x - 2, s.position.y + 1, 2, s.size.y - 2, end); }
        else { draw::rect(va, s.position.x + 1, s.position.y, s.size.x - 2, 2, end); draw::rect(va, s.position.x + 1, s.position.y + s.size.y - 2, s.size.x - 2, 2, end); }
        if(rng.chance(0.6f)) draw::rect(va, s.position.x + s.size.x * 0.4f, s.position.y - 1, 2, 2, MEADOW_LT);       // moss
        return;
    }
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

void Map::render_charge(sf::RenderTexture& ground) const
{
    Rng rng(82);
    sf::VertexArray va(sf::PrimitiveType::Triangles);
    ground.clear(MEADOW);

    // --- a meadow in grey morning light: grass, clover, dew -------------------------------
    for(float y = 0; y < size.y; y += 4.0f)
        for(float x = 0; x < size.x; x += 4.0f)
        {
            float const m = field(x / 60.0f, y / 60.0f) * 0.65f + field(x / 13.0f, y / 13.0f) * 0.35f;
            sf::Color c = m < 0.33f ? MEADOW_DK : (m < 0.72f ? MEADOW : MEADOW_LT);
            // Trodden to mud round our trench and along the road.
            float const trodden = std::clamp(1.0f - std::abs(x - 410.0f) / 90.0f, 0.0f, 1.0f) * (0.6f + 0.4f * m);
            c = lerp(c, m < 0.5f ? MUD : MUD_LIGHT, trodden * 0.8f);
            if(in_grass({x + 2, y + 2})) c = m < 0.5f ? lerp(MEADOW, WHEAT_DK, 0.35f) : lerp(MEADOW_LT, WHEAT_DK, 0.3f);
            if(field(x / 7.0f + 90.0f, y / 7.0f) > 0.86f) c = lerp(c, PAPER, 0.12f);            // dew
            draw::rect(va, x, y, 4, 4, c);
        }
    ground.draw(va); va.clear();
    // Flowers and tufts.
    for(int i = 0; i < 5200; ++i)
    {
        float const x = rng.real(500, size.x), y = rng.real(0, size.y);
        if(rng.chance(0.08f))
        {
            draw::rect(va, std::floor(x), std::floor(y), 1, 1, rng.chance(0.5f) ? sf::Color(214, 206, 170) : sf::Color(172, 120, 150));
            continue;
        }
        sf::Color const c = rng.chance(0.5f) ? MEADOW_LT : lerp(MEADOW_LT, WHEAT, 0.3f);
        for(int b = 0; b < 3; ++b) draw::rect(va, std::floor(x + b), std::floor(y - rng.real(1, 4)), 1, rng.real(2, 4), c);
    }
    ground.draw(va); va.clear();

    // --- shell holes ---------------------------------------------------------------------------
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

    // --- the trench: fresh spoil thrown up in front of it --------------------------------------
    for(const auto& t : trenches)
    {
        for(std::size_t i = 0; i + 1 < t.points.size(); ++i) draw::thick_line(va, t.points[i] + sf::Vector2f{6, 0}, t.points[i + 1] + sf::Vector2f{6, 0}, t.width + 14, MUD_PALE);
        for(auto p : t.points) draw::disc(va, p.x + 6, p.y, (t.width + 14) * 0.5f, MUD_PALE);
    }
    for(const auto& t : trenches)
    {
        for(std::size_t i = 0; i + 1 < t.points.size(); ++i) draw::thick_line(va, t.points[i], t.points[i + 1], t.width, MUD_DARK);
        for(auto p : t.points) draw::disc(va, p.x, p.y, t.width * 0.5f, MUD_DARK);
    }
    // The road behind.
    for(float y = 0; y < size.y; y += 4.0f)
        draw::rect(va, 60.0f + std::sin(y / 90.0f) * 8.0f, y, 26, 4, lerp(ROAD, MUD, 0.3f + 0.2f * field(3.0f, y / 20.0f)));
    ground.draw(va); va.clear();

    // --- floors under the roofs --------------------------------------------------------------
    for(const auto& r : roofs)
        for(float y = r.position.y; y < r.position.y + r.size.y; y += 4.0f)
            draw::rect(va, r.position.x, y, r.size.x, 3, static_cast<int>(y) / 4 % 2 ? PLANK : PLANK_DARK);
    ground.draw(va); va.clear();

    // --- the bit of wire they got up -------------------------------------------------------------
    for(const auto& w : wire)
    {
        float const cx = w.position.x + w.size.x * 0.5f;
        for(float y = w.position.y; y < w.position.y + w.size.y; y += 1.0f)
        {
            float const loop = std::sin(y * 0.45f) * (w.size.x * 0.45f);
            draw::rect(va, std::floor(cx + loop), y, 1, 1, mudcats::WIRE);
        }
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
    // Windows: a frame at each end, a sill along it, glass long gone.
    for(const auto& w : windows)
    {
        bool const across = w.size.x >= w.size.y;
        draw::rect(va, w.position.x, w.position.y, w.size.x, w.size.y, lerp(INK, WOOD, 0.35f));
        if(across)
        {
            draw::rect(va, w.position.x, w.position.y, 1, w.size.y, WOOD);
            draw::rect(va, w.position.x + w.size.x - 1, w.position.y, 1, w.size.y, WOOD);
            draw::rect(va, w.position.x, w.position.y + w.size.y * 0.5f, w.size.x, 1, lerp(WOOD, PAPER, 0.25f));
        }
        else
        {
            draw::rect(va, w.position.x, w.position.y, w.size.x, 1, WOOD);
            draw::rect(va, w.position.x, w.position.y + w.size.y - 1, w.size.x, 1, WOOD);
            draw::rect(va, w.position.x + w.size.x * 0.5f, w.position.y, 1, w.size.y, lerp(WOOD, PAPER, 0.25f));
        }
    }
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

namespace
{
    constexpr float DISC_MIN = 4.0f, DISC_MAX = 20.0f, DISC_STEP = 0.125f;
    int disc_index(float r)
    {
        return static_cast<int>(std::lround((std::clamp(r, DISC_MIN, DISC_MAX) - DISC_MIN) / DISC_STEP));
    }
}

void Map::make_discs() const
{
    if(m_discs_made) return;
    m_discs_made = true;
    // Each disc exactly as draw::disc would draw it: a row at a time, each
    // row as wide as the circle is there, rounded down.
    int const count = disc_index(DISC_MAX) + 1;
    constexpr int WIDTH = 1024;
    std::vector<std::vector<int>> halves(static_cast<std::size_t>(count));
    m_disc_cells.assign(static_cast<std::size_t>(count), {});
    int x = 0, y = 0, row = 0;
    for(int i = 0; i < count; ++i)
    {
        float const r = DISC_MIN + static_cast<float>(i) * DISC_STEP;
        int const ir = static_cast<int>(r), side = 2 * ir + 1;
        if(x + side > WIDTH) { x = 0; y += row; row = 0; }
        m_disc_cells[static_cast<std::size_t>(i)] = {x, y};
        x += side + 1;
        row = std::max(row, side + 1);
    }
    sf::Image img({WIDTH, static_cast<unsigned>(y + row)}, sf::Color::Transparent);
    for(int i = 0; i < count; ++i)
    {
        float const r = DISC_MIN + static_cast<float>(i) * DISC_STEP;
        int const ir = static_cast<int>(r);
        sf::Vector2i const cell = m_disc_cells[static_cast<std::size_t>(i)];
        for(int dy = -ir; dy <= ir; ++dy)
        {
            int const half = static_cast<int>(std::floor(std::sqrt(std::max(0.0f, r * r - static_cast<float>(dy * dy)))));
            for(int dx = -half; dx <= half; ++dx)
                img.setPixel({static_cast<unsigned>(cell.x + ir + dx), static_cast<unsigned>(cell.y + ir + dy)}, sf::Color::White);
        }
    }
    (void)m_discs.loadFromImage(img);
}

void Map::add_disc(sf::VertexArray& va, float cx, float cy, float r, sf::Color c) const
{
    // The same pixels draw::disc fills, from the atlas.
    int const i = disc_index(r);
    float const rr = DISC_MIN + static_cast<float>(i) * DISC_STEP;
    auto const ir = static_cast<float>(static_cast<int>(rr));
    sf::Vector2f const t(m_disc_cells[static_cast<std::size_t>(i)]);
    float const side = 2.0f * ir + 1.0f;
    float const x = std::floor(cx) - ir, y = std::floor(cy) - ir;
    const sf::Vertex q[6] = {
        {{x, y}, c, t}, {{x + side, y}, c, {t.x + side, t.y}}, {{x + side, y + side}, c, {t.x + side, t.y + side}},
        {{x, y}, c, t}, {{x + side, y + side}, c, {t.x + side, t.y + side}}, {{x, y + side}, c, {t.x, t.y + side}},
    };
    for(const auto& v : q) va.append(v);
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
        // Close by, the roof opens up: you're looking in, not down at it.
        float const shut = 1.0f - roof_open(r, viewer);
        if(shut <= 0.0f) continue;
        bool const town = level == LevelId::TOWN && r.size.x > 30.0f;
        bool const slate = town && (r.position.x > 2000.0f || r.size.x > 300.0f);       // the church and the town hall
        bool const tiled = town || (level == LevelId::CHARGE && r.size.y > 60.0f);      // a house or a barn, not a dugout
        // In the town, each house's tiles a little different from the next.
        float const weather = town ? hashf(r.position.x, r.position.y) : 0.4f;
        sf::Color const tiles = slate ? lerp(STONE_DK, MON_BLUE_DK, 0.25f) : lerp(BRICK, STONE, weather * 0.7f);
        sf::Color const top = with_alpha(tiled ? tiles : BARK, shut);
        sf::Color const edge = with_alpha(tiled ? (slate ? lerp(STONE_DK, INK, 0.4f) : BRICK_DK) : lerp(BARK, INK, 0.3f), shut);
        draw::rect(va, r.position.x - 6, r.position.y - 6, r.size.x + 12, r.size.y + 12, edge);
        draw::rect(va, r.position.x - 5, r.position.y - 5, r.size.x + 10, r.size.y + 9, top);
        if(tiled)
        {
            // Tiles in rows, a ridge down the middle, a few missing.
            for(float y = r.position.y - 4; y < r.position.y + r.size.y + 4; y += 3)
                draw::rect(va, r.position.x - 5, y, r.size.x + 10, 1, edge);
            draw::rect(va, r.position.x - 5, r.position.y + r.size.y * 0.5f - 1, r.size.x + 10, 2, with_alpha(lerp(edge, INK, 0.3f), shut));
            for(int k = 0; k < 9; ++k)
                draw::rect(va, r.position.x + std::fmod(static_cast<float>(k * 41), std::max(8.0f, r.size.x - 4.0f)),
                           r.position.y + std::fmod(static_cast<float>(k * 29), std::max(8.0f, r.size.y - 3.0f)), 4, 3, with_alpha(INK, shut));
        }
        else
            for(float y = r.position.y - 5; y < r.position.y + r.size.y + 4; y += 4)
                draw::rect(va, r.position.x - 5, y, r.size.x + 10, 1, with_alpha(lerp(BARK, INK, 0.4f), shut));
    }

    target.draw(va);
    va.clear();

    // --- tree crowns --------------------------------------------------------------------------
    make_discs();
    for(const auto& t : trees)
    {
        if(!visible.findIntersection(sf::FloatRect(t.pos - sf::Vector2f{t.canopy, t.canopy}, {t.canopy * 2, t.canopy * 2}))) continue;
        float const d = std::hypot(viewer.x - t.pos.x, viewer.y - t.pos.y);
        float alpha = d < t.canopy + 6.0f ? 0.35f : 0.92f;
        // In the forest the crowns are everywhere: thin enough to see the floor through, and
        // cleared away round you so you can see what you're doing.
        if(level == LevelId::FOREST) alpha = 0.18f + 0.42f * std::clamp((d - 60.0f) / 90.0f, 0.0f, 1.0f);
        sf::Color const dark = with_alpha(t.shade == 2 ? lerp(LEAF, MEADOW, 0.4f) : LEAF, alpha);
        sf::Color const mid  = with_alpha(t.shade == 1 ? lerp(LEAF_LT, WHEAT, 0.15f) : LEAF_LT, alpha);
        float const sx = std::sin(time * 0.7f + t.pos.x) * 0.6f;
        add_disc(va, t.pos.x + sx, t.pos.y - 2, t.canopy, dark);
        add_disc(va, t.pos.x - t.canopy * 0.3f + sx, t.pos.y - t.canopy * 0.35f - 2, t.canopy * 0.55f, mid);
        add_disc(va, t.pos.x + t.canopy * 0.35f + sx, t.pos.y + t.canopy * 0.1f - 2, t.canopy * 0.4f, with_alpha(lerp(LEAF, LEAF_LT, 0.5f), alpha));
    }
    target.draw(va, sf::RenderStates(&m_discs));
}

}
