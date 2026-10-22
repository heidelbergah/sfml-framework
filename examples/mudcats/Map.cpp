/**
 * Map.cpp  -  Mud Cats
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

    // Which wall is made of what, for drawing.
    enum class Material { LOG, CONCRETE, BRICK };
    std::vector<Material> g_materials;
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

void Map::build()
{
    walls.clear(); sandbags.clear(); slits.clear(); wire.clear(); roofs.clear();
    trenches.clear(); craters.clear(); cover.clear(); bottles.clear(); waypoints.clear();
    g_materials.clear();

    auto wall = [this](sf::FloatRect r, Material m) { walls.push_back(r); g_materials.push_back(m); };

    player_start = {110.0f, 262.0f};

    // --- behind our lines ----------------------------------------------
    trenches.push_back({{{40, 262}, {200, 254}, {320, 282}, {405, 272}}, 22.0f});                     // reserve
    trenches.push_back({{{405, 272}, {470, 332}, {560, 322}, {640, 402}, {740, 382}, {848, 452}}, 18.0f}); // communication
    wall(R(150, 196, 80, 6), Material::LOG);                                   // dugout
    wall(R(150, 196, 6, 46), Material::LOG);
    wall(R(224, 196, 6, 46), Material::LOG);
    roofs.push_back(R(156, 202, 68, 40));

    range_area = R(440, 140, 260, 130);
    sandbags.push_back(R(476, 180, 8, 50));                                    // the firing line
    for(float y : {172.0f, 202.0f, 232.0f})
    {
        sandbags.push_back(R(652, y, 12, 12));                                 // crates
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
        sandbags.push_back(r);
    for(sf::Vector2f p : {sf::Vector2f{1120, 480}, {1380, 330}, {1600, 560}, {980, 660}, {1270, 720}})
        sandbags.push_back(R(p.x - 3, p.y - 3, 6, 6));                         // shattered tree trunks

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

    sandbags.push_back(R(1650, 314, 8, 42));                                   // MG nest
    sandbags.push_back(R(1650, 314, 32, 8));
    sandbags.push_back(R(1650, 348, 32, 8));
    for(auto r : {R(1684, 180, 8, 30), R(1688, 520, 8, 30), R(1692, 690, 8, 30)})
        sandbags.push_back(r);

    wall(R(1850, 240, 40, 8), Material::BRICK);                                // ruins
    wall(R(1850, 240, 8, 44), Material::BRICK);
    wall(R(1900, 620, 52, 8), Material::BRICK);
    sandbags.push_back(R(1950, 520, 22, 12));                                  // a wrecked cart

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
        sandbags.push_back(r);
    for(sf::Vector2f p : {sf::Vector2f{2200, 200}, {2470, 540}, {2180, 600}, {2500, 230}, {2560, 400}})
        sandbags.push_back(R(p.x - 3, p.y - 3, 6, 6));                         // trees
    farm_area = R(2240, 280, 160, 190);

    waypoints = {{850, 440}, {1000, 440}, {1130, 440}, {1330, 450}, {1480, 440},
                 {1715, 420}, {1880, 420}, {2080, 420}, {2320, 380}};

    rasterise();
    place_cover();
}

void Map::rasterise()
{
    m_cols = static_cast<int>(std::ceil(size.x / CELL));
    m_rows = static_cast<int>(std::ceil(size.y / CELL));
    m_grid.assign(static_cast<std::size_t>(m_cols * m_rows), 0);

    for(int cy = 0; cy < m_rows; ++cy)
        for(int cx = 0; cx < m_cols; ++cx)
        {
            sf::FloatRect const cell({cx * CELL, cy * CELL}, {CELL, CELL});
            sf::Vector2f const c = cell.position + sf::Vector2f{CELL, CELL} * 0.5f;
            std::uint8_t f = 0;

            auto grown = [](sf::FloatRect r) { return sf::FloatRect(r.position - sf::Vector2f{2, 2}, r.size + sf::Vector2f{4, 4}); };
            for(const auto& r : walls)    if(grown(r).findIntersection(cell)) f |= SOLID;
            for(const auto& r : sandbags) if(grown(r).findIntersection(cell)) f |= SOLID;
            for(const auto& r : slits)    if(grown(r).findIntersection(cell)) f |= SOLID;
            for(const auto& r : wire)     if(r.contains(c)) f |= WIRE;
            if(in_trench(c)) f |= TRENCH;
            if(in_crater(c)) f |= CRATER;

            m_grid[static_cast<std::size_t>(cell_index(cx, cy))] = f;
        }
}

void Map::place_cover()
{
    auto add = [this](sf::Vector2f p, sf::Vector2f protects, bool trench)
    {
        if(!walkable(p) || in_wire(p)) return;
        for(const auto& r : walls) if(r.contains(p)) return;
        cover.push_back({p, protects, trench});
    };

    // Both sides of every sandbag line.
    for(const auto& s : sandbags)
    {
        bool const vertical = s.size.y > s.size.x;
        float const len = vertical ? s.size.y : s.size.x;
        if(len < 10.0f) continue;                       // trunks and crates are too small to hide behind
        for(float k = 5.0f; k < len; k += 10.0f)
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
    }

    // Along every trench.
    for(const auto& t : trenches)
        for(std::size_t i = 0; i + 1 < t.points.size(); ++i)
        {
            sf::Vector2f const a = t.points[i], b = t.points[i + 1];
            float const len = std::hypot(b.x - a.x, b.y - a.y);
            for(float k = 6.0f; k < len; k += 13.0f)
                add(a + (b - a) * (k / len), {}, true);
        }

    for(const auto& c : craters) add(c.pos, {}, false);

    // Beside walls, on both sides.
    for(const auto& w : walls)
    {
        sf::Vector2f const c = w.position + w.size * 0.5f;
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
        (void)c;
    }
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

bool Map::line_of_sight(sf::Vector2f a, sf::Vector2f b) const
{
    // Only walls. Roofs hide what is under them from the player's eye, but
    // a man inside a bunker can see out of its slit perfectly well.
    return !wall_hit(a, b).has_value();
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

    std::size_t const n = static_cast<std::size_t>(m_cols * m_rows);
    std::vector<float> cost(n, 1e30f);
    std::vector<int>   came(n, -1);
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
            if(fl & (SOLID | WIRE)) return false;
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
            else if(field_side)  c = m < 0.3f ? MUD : (m < 0.55f ? GRASS_DEAD : (m < 0.8f ? GRASS : MUD_LIGHT));
            else                 c = m < 0.3f ? MUD : (m < 0.65f ? MUD_LIGHT : (m < 0.85f ? GRASS_DEAD : GRASS));
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
        sf::Color const c = rng.chance(0.5f) ? GRASS : GRASS_DEAD;
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
                draw::disc(va, c.pos.x + 2 + rng.real(-2, 2), c.pos.y + 2 + rng.real(-1.5f, 1.5f), c.radius * rng.real(0.18f, 0.3f), WATER);
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

    // --- sandbags ---------------------------------------------------------------
    for(const auto& s : sandbags)
    {
        if(s.size.x <= 6.0f && s.size.y <= 6.0f)                            // trunks
        {
            sf::Vector2f const c = s.position + s.size * 0.5f;
            bool const farm = c.x > 2100;
            draw::rect(va, c.x - 1, c.y + 2, 6, 3, SHADOW);
            if(farm)
            {
                draw::disc(va, c.x, c.y - 4, 9, lerp(GRASS, INK, 0.25f));
                draw::disc(va, c.x - 1, c.y - 5, 7, GRASS);
                draw::disc(va, c.x - 3, c.y - 7, 3, lerp(GRASS, MON_GOLD, 0.2f));
            }
            else
            {
                draw::disc(va, c.x, c.y, 3, BARK);
                draw::disc(va, c.x, c.y, 1.5f, PLANK);
                for(int k = 0; k < 3; ++k)
                    draw::thick_line(va, c, c + sf::Vector2f{rng.real(-9, 9), rng.real(-9, 9)}, 1, BARK);
            }
            continue;
        }
        if(s.size.x == 12.0f && s.size.y == 12.0f)                          // crates on the range
        {
            draw::rect(va, s.position.x + 2, s.position.y + 2, 12, 12, SHADOW);
            draw::rect(va, s.position.x, s.position.y, 12, 12, PLANK);
            draw::rect(va, s.position.x + 1, s.position.y + 5, 10, 1, PLANK_DARK);
            draw::rect(va, s.position.x, s.position.y, 12, 1, lerp(PLANK, PAPER, 0.3f));
            continue;
        }
        draw::rect(va, s.position.x + 2, s.position.y + 2, s.size.x, s.size.y, SHADOW);
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
    ground.draw(va); va.clear();

    // --- walls ------------------------------------------------------------------
    for(std::size_t i = 0; i < walls.size(); ++i)
    {
        const auto& w = walls[i];
        Material const m = g_materials[i];
        sf::Color const face = m == Material::CONCRETE ? CONCRETE : (m == Material::BRICK ? BRICK : BARK);
        sf::Color const dark = m == Material::CONCRETE ? CONCRETE_DK : (m == Material::BRICK ? BRICK_DK : lerp(BARK, INK, 0.4f));
        draw::rect(va, w.position.x + 3, w.position.y + 3, w.size.x, w.size.y, SHADOW);
        draw::rect(va, w.position.x, w.position.y, w.size.x, w.size.y, dark);
        draw::rect(va, w.position.x, w.position.y, w.size.x, w.size.y - 1, face);
        if(m == Material::BRICK)
            for(float y = w.position.y; y < w.position.y + w.size.y; y += 3)
                for(float x = w.position.x + std::fmod(y, 6.0f); x < w.position.x + w.size.x; x += 6)
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
    // Rubble around the ruins and the farmhouse.
    for(int i = 0; i < 140; ++i)
    {
        sf::Vector2f p = rng.chance(0.6f) ? sf::Vector2f{rng.real(2220, 2420), rng.real(260, 490)}
                                          : sf::Vector2f{rng.real(1830, 1970), rng.real(220, 650)};
        if(!walkable(p)) continue;
        draw::rect(va, std::floor(p.x), std::floor(p.y), rng.real(1, 3), rng.real(1, 2), rng.chance(0.5f) ? BRICK : BRICK_DK);
    }
    ground.draw(va); va.clear();

    // --- the range: a painted sign and a few spent cases ---------------------
    for(int i = 0; i < 25; ++i)
        draw::rect(va, std::floor(rng.real(462, 474)), std::floor(rng.real(180, 230)), 1, 1, MON_GOLD);
    ground.draw(va);

    ground.display();
}

void Map::draw_roofs(sf::RenderTarget& target, sf::Vector2f viewer) const
{
    sf::VertexArray va(sf::PrimitiveType::Triangles);
    for(std::size_t i = 0; i < roofs.size(); ++i)
    {
        const auto& r = roofs[i];
        if(r.contains(viewer)) continue;
        bool const bunker = r.position.x > 1000;
        sf::Color const top = bunker ? lerp(CONCRETE, MUD_LIGHT, 0.2f) : BARK;
        draw::rect(va, r.position.x - 6, r.position.y - 6, r.size.x + 12, r.size.y + 12, bunker ? CONCRETE_DK : lerp(BARK, INK, 0.3f));
        draw::rect(va, r.position.x - 5, r.position.y - 5, r.size.x + 10, r.size.y + 9, top);
        if(bunker)
        {
            draw::rect(va, r.position.x + 6, r.position.y + 10, 18, 1, CONCRETE_DK);
            draw::rect(va, r.position.x + 30, r.position.y + 24, 1, 12, CONCRETE_DK);
            for(int k = 0; k < 12; ++k)                             // earth thrown over it
                draw::rect(va, r.position.x - 4 + static_cast<float>((k * 37) % 56), r.position.y - 4 + static_cast<float>((k * 23) % 48),
                           3, 2, MUD_LIGHT);
        }
        else
        {
            for(float y = r.position.y - 5; y < r.position.y + r.size.y + 4; y += 4)
                draw::rect(va, r.position.x - 5, y, r.size.x + 10, 1, lerp(BARK, INK, 0.4f));
        }
    }
    target.draw(va);
}

}
