/**
 * World.cpp  -  Cinder
 */

#include "World.hpp"
#include "Palette.hpp"

#include <algorithm>
#include <cmath>
#include <random>

namespace cinder
{

namespace
{
    constexpr float CHUNK = 128.0f;

    void rect(sf::VertexArray& va, float x, float y, float w, float h, sf::Color c)
    {
        sf::Vector2f const a{x, y}, b{x + w, y}, d{x + w, y + h}, e{x, y + h};
        va.append({a, c}); va.append({b, c}); va.append({d, c});
        va.append({a, c}); va.append({d, c}); va.append({e, c});
    }

    void disc(sf::VertexArray& va, float cx, float cy, float r, sf::Color c)
    {
        int const ir = static_cast<int>(r);
        for(int dy = -ir; dy <= ir; ++dy)
        {
            float const half = std::floor(std::sqrt(r * r - static_cast<float>(dy * dy)));
            rect(va, cx - half, cy + static_cast<float>(dy), half * 2.0f + 1.0f, 1.0f, c);
        }
    }

    float hash(float a, float b)
    {
        float const s = std::sin(a * 127.1f + b * 311.7f) * 43758.5453f;
        return s - std::floor(s);
    }

    struct Rng
    {
        std::mt19937 gen;
        explicit Rng(unsigned seed) : gen(seed) {}
        float real(float a, float b) { return std::uniform_real_distribution<float>{a, b}(gen); }
        int   integer(int a, int b) { return std::uniform_int_distribution<int>{a, b}(gen); }
        bool  chance(float p) { return real(0.0f, 1.0f) < p; }
        template<typename T> const T& pick(const std::vector<T>& v)
        {
            return v[static_cast<std::size_t>(integer(0, static_cast<int>(v.size()) - 1))];
        }
    };

    // Smooth value noise, for meadows and bare patches at a larger scale
    // than individual tufts.
    float field(float x, float y, float seed)
    {
        float const ix = std::floor(x), iy = std::floor(y);
        float fx = x - ix, fy = y - iy;
        fx = fx * fx * (3.0f - 2.0f * fx);
        fy = fy * fy * (3.0f - 2.0f * fy);
        float const a = hash(ix + seed, iy), b = hash(ix + 1 + seed, iy);
        float const c = hash(ix + seed, iy + 1), d = hash(ix + 1 + seed, iy + 1);
        return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
    }

    bool near_rect(sf::Vector2f p, const sf::FloatRect& r, float margin)
    {
        return p.x > r.position.x - margin && p.x < r.position.x + r.size.x + margin
            && p.y > r.position.y - margin && p.y < r.position.y + r.size.y + margin;
    }

    bool overlaps(const sf::FloatRect& a, const sf::FloatRect& b)
    {
        return a.findIntersection(b).has_value();
    }
}

bool World::clear_of_obstacles(sf::Vector2f p, float margin) const
{
    for(const auto& w : m_def->walls) if(near_rect(p, w, margin)) return false;
    for(const auto& w : m_def->water) if(near_rect(p, w, margin)) return false;
    return p.x > 0 && p.y > 0 && p.x < m_def->size.x && p.y < m_def->size.y;
}

void World::build(const LevelDef& def)
{
    m_def = &def;
    m_ground.clear();
    m_decor.clear();
    m_walls.clear();

    Rng rng(def.seed);
    bool const cold = def.snow;

    m_ground_colour = cold ? sf::Color(0x1C, 0x1C, 0x1E) : sf::Color(0x22, 0x1C, 0x16);

    // --- ground: a speckle of earth, moss and (up high) frost ------------
    std::vector<sf::Color> const speck = cold
        ? std::vector<sf::Color>{G9, G8, sf::Color(0x2A, 0x2A, 0x2C), sf::Color(0x3A, 0x2C, 0x22), sf::Color(0x1A, 0x2B, 0x21)}
        : std::vector<sf::Color>{sf::Color(0x23, 0x1F, 0x1A), sf::Color(0x17, 0x24, 0x1C),
                                 sf::Color(0x1A, 0x2B, 0x21), sf::Color(0x2A, 0x22, 0x1C)};

    // A base of earth and moss in broad soft patches, stepped into three
    // tones with a checker dither between them so it reads as pixel art.
    std::vector<sf::Color> const tones = cold
        ? std::vector<sf::Color>{sf::Color(0x1C, 0x1C, 0x1E), sf::Color(0x22, 0x22, 0x25), sf::Color(0x1A, 0x22, 0x1E)}
        : std::vector<sf::Color>{sf::Color(0x22, 0x1C, 0x16), sf::Color(0x1C, 0x1F, 0x17), sf::Color(0x15, 0x24, 0x1A)};
    for(float y = 0; y < def.size.y; y += 4.0f)
        for(float x = 0; x < def.size.x; x += 4.0f)
        {
            float const m = field(x / 70.0f, y / 70.0f, static_cast<float>(def.seed));
            float const scaled = std::clamp(m * 2.6f - 0.3f, 0.0f, 1.999f);
            int level = static_cast<int>(scaled);
            bool const odd = (static_cast<int>(x / 4.0f) + static_cast<int>(y / 4.0f)) % 2 == 1;
            if(odd && scaled - static_cast<float>(level) > 0.5f) ++level;
            if(level > 0) rect(m_ground, x, y, 4.0f, 4.0f, tones[static_cast<std::size_t>(level)]);
        }

    for(float y = 0; y < def.size.y; y += 4.0f)
        for(float x = 0; x < def.size.x; x += 4.0f)
            if(rng.chance(0.10f)) rect(m_ground, x, y, rng.chance(0.5f) ? 4.0f : 2.0f, 2.0f, rng.pick(speck));

    // Larger moss or frost patches, so the ground is not uniform noise.
    for(int i = 0; i < static_cast<int>(def.size.x * def.size.y / 9000.0f); ++i)
    {
        float const cx = rng.real(0, def.size.x), cy = rng.real(0, def.size.y);
        sf::Color const c = cold ? sf::Color(0x2E, 0x2E, 0x31) : sf::Color(0x18, 0x26, 0x1D);
        for(int k = 0; k < 14; ++k)
            rect(m_ground, std::floor(cx + rng.real(-14, 14)), std::floor(cy + rng.real(-8, 8)),
                 rng.real(3, 8), 2.0f, c);
    }

    // --- shadows under walls --------------------------------------------
    for(const auto& w : def.walls)
        rect(m_ground, w.position.x + 2, w.position.y + w.size.y, w.size.x, 4.0f, sf::Color(0x10, 0x0E, 0x0C));

    // --- grass ------------------------------------------------------------
    std::vector<sf::Color> const blade_low  = cold ? std::vector<sf::Color>{GREEN, G7}
                                                   : std::vector<sf::Color>{GREEN, sf::Color(0x2A, 0x5A, 0x40)};
    std::vector<sf::Color> const blade_high = cold ? std::vector<sf::Color>{GREEN_LIGHT, G4}
                                                   : std::vector<sf::Color>{GREEN_LIGHT, sf::Color(0x8A, 0xA8, 0x5C)};
    float const grass_density = cold ? 0.45f : 0.85f;

    for(float y = 4; y < def.size.y; y += 7.0f)
        for(float x = 4; x < def.size.x; x += 7.0f)
        {
            // Thick meadow in some places, bare earth in others.
            float const meadow = field(x / 70.0f, y / 70.0f, static_cast<float>(def.seed));
            float const local  = std::clamp((meadow - 0.28f) * 2.4f, 0.06f, 1.0f);
            if(!rng.chance(grass_density * local)) continue;
            sf::Vector2f const p{std::floor(x + rng.real(-3, 3)), std::floor(y + rng.real(-3, 3))};
            if(!clear_of_obstacles(p, 2.0f)) continue;

            int const blades = rng.integer(2, 4);
            for(int b = 0; b < blades; ++b)
            {
                float const bx = p.x + static_cast<float>(b) * 1.0f + rng.real(-1, 1);
                float const h  = std::floor(rng.real(2, 5));
                rect(m_decor, std::floor(bx), p.y - h, 1.0f, h, rng.pick(blade_low));
                rect(m_decor, std::floor(bx + (rng.chance(0.5f) ? 1.0f : 0.0f)), p.y - h - 1, 1.0f, 1.0f, rng.pick(blade_high));
            }
        }

    // --- flowers ------------------------------------------------------------
    std::vector<sf::Color> const petals = cold
        ? std::vector<sf::Color>{G1, G2, YELLOW}
        : std::vector<sf::Color>{RED_GLOWING, YELLOW, ORANGE_SMOLDER, G1, RED_KINDLING, GOLDEN};
    int const flowers = static_cast<int>(def.size.x * def.size.y / (cold ? 5000.0f : 1100.0f));
    for(int i = 0; i < flowers; ++i)
    {
        // Flowers grow in little clumps.
        float const cx = rng.real(0, def.size.x), cy = rng.real(0, def.size.y);
        sf::Color const c = rng.pick(petals);
        for(int k = rng.integer(1, 4); k > 0; --k)
        {
            sf::Vector2f const p{std::floor(cx + rng.real(-8, 8)), std::floor(cy + rng.real(-5, 5))};
            if(!clear_of_obstacles(p, 3.0f)) continue;
            rect(m_decor, p.x, p.y - 2, 1.0f, 3.0f, GREEN);
            rect(m_decor, p.x - 1, p.y - 4, 3.0f, 1.0f, c);
            rect(m_decor, p.x, p.y - 5, 1.0f, 3.0f, c);
            rect(m_decor, p.x, p.y - 4, 1.0f, 1.0f, rng.chance(0.5f) ? YELLOW : G1);
        }
    }

    // --- up in the pass: berry bushes and lichen, the last warm colours -----
    if(cold)
    {
        for(int i = 0; i < static_cast<int>(def.size.x * def.size.y / 4000.0f); ++i)
        {
            sf::Vector2f const p{std::floor(rng.real(0, def.size.x)), std::floor(rng.real(0, def.size.y))};
            if(!clear_of_obstacles(p, 4.0f)) continue;
            if(rng.chance(0.5f))
            {
                disc(m_decor, p.x, p.y, 3.0f, sf::Color(0x17, 0x55, 0x40));
                disc(m_decor, p.x, p.y - 1, 2.0f, GREEN);
                for(int k = 0; k < 4; ++k)
                    rect(m_decor, std::floor(p.x + rng.real(-3, 3)), std::floor(p.y + rng.real(-3, 2)), 1.0f, 1.0f,
                         rng.chance(0.5f) ? RED_GLOWING : RED_FLAME);
            }
            else
            {
                for(int k = 0; k < 5; ++k)
                    rect(m_decor, std::floor(p.x + rng.real(-4, 4)), std::floor(p.y + rng.real(-2, 2)),
                         2.0f, 1.0f, rng.chance(0.5f) ? ORANGE_SMOLDER : GOLDEN);
            }
        }
    }

    // --- reeds along the water ----------------------------------------------
    for(const auto& w : def.water)
    {
        for(float x = w.position.x; x < w.position.x + w.size.x; x += 3.0f)
            for(float edge : {w.position.y - 1.0f, w.position.y + w.size.y + 6.0f})
            {
                if(!rng.chance(0.45f)) continue;
                sf::Vector2f const p{std::floor(x + rng.real(-1, 1)), std::floor(edge + rng.real(-1, 2))};
                if(in_water(p)) continue;
                float const h = std::floor(rng.real(4, 8));
                rect(m_decor, p.x, p.y - h, 1.0f, h, rng.chance(0.5f) ? GREEN_LIGHT : GREEN);
                if(rng.chance(0.35f)) rect(m_decor, p.x, p.y - h - 2, 1.0f, 3.0f, RED_BRICK);
            }
    }

    // --- stones ---------------------------------------------------------------
    for(int i = 0; i < static_cast<int>(def.size.x * def.size.y / 6000.0f); ++i)
    {
        sf::Vector2f const p{std::floor(rng.real(0, def.size.x)), std::floor(rng.real(0, def.size.y))};
        if(!clear_of_obstacles(p, 2.0f)) continue;
        float const w = std::floor(rng.real(2, 5));
        rect(m_decor, p.x, p.y, w, 2.0f, G7);
        rect(m_decor, p.x, p.y, w - 1.0f, 1.0f, G5);
    }

    // --- trees ----------------------------------------------------------------
    std::vector<sf::Color> const leaves = {ORANGE_BLAZE, RED_CRIMSON, GOLDEN, RED_EMBER, ORANGE_GLOW};
    int const trees = static_cast<int>(def.size.x * def.size.y / 17000.0f);
    std::vector<sf::Vector2f> placed;
    for(int i = 0, tries = 0; i < trees && tries < trees * 20; ++tries)
    {
        sf::Vector2f const p{std::floor(rng.real(20, def.size.x - 20)), std::floor(rng.real(30, def.size.y - 10))};
        if(!clear_of_obstacles(p, 26.0f)) continue;
        bool crowded = false;
        for(const auto& q : placed) if(std::hypot(p.x - q.x, p.y - q.y) < 26.0f) crowded = true;
        for(const auto& b : def.braziers) if(std::hypot(p.x - b.x, p.y - b.y) < 40.0f) crowded = true;
        if(std::hypot(p.x - def.start.x, p.y - def.start.y) < 40.0f) crowded = true;
        if(crowded) continue;
        placed.push_back(p);
        ++i;

        // Shadow, then trunk, then the crown.
        rect(m_decor, p.x - 8, p.y, 18.0f, 3.0f, sf::Color(0x11, 0x0F, 0x0D));
        if(cold)
        {
            rect(m_decor, p.x, p.y - 5, 2.0f, 5.0f, RED_DEEP);
            for(int layer = 0; layer < 4; ++layer)
            {
                float const top  = p.y - 6.0f - static_cast<float>(layer) * 5.0f;
                float const half = 9.0f - static_cast<float>(layer) * 2.0f;
                for(int row = 0; row < 6; ++row)
                {
                    float const hw = std::floor(half * static_cast<float>(row + 1) / 6.0f);
                    rect(m_decor, p.x + 1 - hw, top - 6 + static_cast<float>(row), hw * 2.0f, 1.0f,
                         row < 2 ? G2 : (row % 2 ? GREEN : sf::Color(0x17, 0x55, 0x40)));
                }
            }
        }
        else
        {
            rect(m_decor, p.x, p.y - 9, 2.0f, 9.0f, RED_DEEP);
            rect(m_decor, p.x, p.y - 9, 1.0f, 9.0f, RED_BRICK);
            float const r = std::floor(rng.real(7, 11));
            sf::Color const c = rng.pick(leaves);
            float const cy = std::floor(p.y - 7 - r);
            sf::Color const shade = lerp(c, BACKGROUND, 0.5f);
            sf::Color const light = lerp(c, YELLOW, 0.3f);

            // A crown of three or four lumps rather than one ball: a dark
            // layer, the leaves a pixel higher, then a lit side.
            std::vector<sf::Vector2f> lumps = {{-r * 0.45f, r * 0.15f}, {r * 0.5f, r * 0.2f}, {0.0f, -r * 0.4f}};
            if(rng.chance(0.6f)) lumps.push_back({rng.real(-r * 0.3f, r * 0.3f), r * 0.35f});
            float const lr = std::floor(r * 0.68f);
            for(auto o : lumps) disc(m_decor, std::floor(p.x + 1 + o.x), std::floor(cy + o.y + 1), lr, shade);
            for(auto o : lumps) disc(m_decor, std::floor(p.x + 1 + o.x), std::floor(cy + o.y - 1), lr - 1, c);
            disc(m_decor, std::floor(p.x - r * 0.35f), std::floor(cy - r * 0.45f), std::floor(r * 0.3f), light);
            rect(m_decor, p.x - 1, std::floor(cy + r * 0.5f), 4.0f, 1.0f, RED_DEEP);     // a glimpse of branch
            for(int k = 0; k < 6; ++k)
                rect(m_decor, std::floor(p.x + rng.real(-r, r)), std::floor(cy + rng.real(-r, r * 0.5f)),
                     1.0f, 1.0f, rng.pick(leaves));
            // A few fallen leaves.
            for(int k = 0; k < 5; ++k)
                rect(m_ground, std::floor(p.x + rng.real(-14, 14)), std::floor(p.y + rng.real(-2, 8)),
                     1.0f, 1.0f, rng.pick(leaves));
        }
    }

    // --- cut into chunks --------------------------------------------------------
    m_chunk_cols = static_cast<int>(std::ceil(def.size.x / CHUNK));
    m_chunk_rows = static_cast<int>(std::ceil(def.size.y / CHUNK));
    m_chunks.assign(static_cast<std::size_t>(m_chunk_cols * m_chunk_rows),
                    sf::VertexArray(sf::PrimitiveType::Triangles));

    // Every shape above is a rect of six vertices; file each by its corner.
    for(const sf::VertexArray* source : {&m_ground, &m_decor})
        for(std::size_t i = 0; i + 6 <= source->getVertexCount(); i += 6)
        {
            sf::Vector2f const p = (*source)[i].position;
            int const cx = std::clamp(static_cast<int>(p.x / CHUNK), 0, m_chunk_cols - 1);
            int const cy = std::clamp(static_cast<int>(p.y / CHUNK), 0, m_chunk_rows - 1);
            auto& chunk = m_chunks[static_cast<std::size_t>(cy * m_chunk_cols + cx)];
            for(std::size_t k = 0; k < 6; ++k) chunk.append((*source)[i + k]);
        }
    m_ground.clear();
    m_decor.clear();

    // --- walls ----------------------------------------------------------------
    for(const auto& w : def.walls)
    {
        float const x = w.position.x, y = w.position.y, ww = w.size.x, hh = w.size.y;
        if(cold)
        {
            rect(m_walls, x, y, ww, hh, G7);
            for(int k = 0; k < static_cast<int>(ww * hh / 40.0f); ++k)
                rect(m_walls, std::floor(x + rng.real(0, ww - 3)), std::floor(y + rng.real(0, hh - 2)),
                     std::floor(rng.real(2, 6)), 1.0f, rng.chance(0.5f) ? G6 : G8);
            rect(m_walls, x, y, ww, 2.0f, G4);
            rect(m_walls, x, y + hh - 2, ww, 2.0f, G8);
        }
        else
        {
            rect(m_walls, x, y, ww, hh, RED_BRICK);
            // Courses of brick, offset every other row.
            for(float row = 0; row * 4.0f < hh; row += 1.0f)
            {
                float const ry = y + row * 4.0f;
                rect(m_walls, x, ry + 3, ww, 1.0f, RED_DEEP);
                float const offset = std::fmod(row, 2.0f) * 4.0f;
                for(float bx = x + offset; bx < x + ww; bx += 8.0f)
                    rect(m_walls, bx, ry, 1.0f, std::min(3.0f, y + hh - ry), RED_DEEP);
                if(rng.chance(0.3f))
                    rect(m_walls, std::floor(x + rng.real(0, std::max(1.0f, ww - 6))), ry,
                         5.0f, std::min(3.0f, y + hh - ry), RED_CRIMSON);
            }
            rect(m_walls, x, y, ww, 2.0f, RED_KINDLING);
            // Moss creeping over the top.
            for(float mx = x; mx < x + ww; mx += 2.0f)
                if(rng.chance(0.35f)) rect(m_walls, mx, y - 1, 2.0f, rng.chance(0.4f) ? 3.0f : 2.0f, GREEN);
        }
    }
}

void World::draw_living(sf::RenderTarget& target, const sf::FloatRect& visible) const
{
    // A margin, since a shape belongs to the chunk of its top-left corner.
    int const x0 = std::max(0, static_cast<int>((visible.position.x - 24.0f) / CHUNK));
    int const y0 = std::max(0, static_cast<int>((visible.position.y - 24.0f) / CHUNK));
    int const x1 = std::min(m_chunk_cols - 1, static_cast<int>((visible.position.x + visible.size.x) / CHUNK));
    int const y1 = std::min(m_chunk_rows - 1, static_cast<int>((visible.position.y + visible.size.y) / CHUNK));

    for(int cy = y0; cy <= y1; ++cy)
        for(int cx = x0; cx <= x1; ++cx)
            target.draw(m_chunks[static_cast<std::size_t>(cy * m_chunk_cols + cx)]);

    target.draw(m_walls);
}

void World::draw_cold(sf::RenderTarget& target, const sf::FloatRect& visible, float time)
{
    m_cold.clear();

    // --- water ---------------------------------------------------------------
    for(const auto& w : m_def->water)
    {
        if(!overlaps(w, visible)) continue;
        rect(m_cold, w.position.x, w.position.y, w.size.x, w.size.y, TEAL_DEEP);
    }
    for(const auto& w : m_def->water)
    {
        if(!overlaps(w, visible)) continue;
        float const x0 = w.position.x, y0 = w.position.y, x1 = x0 + w.size.x, y1 = y0 + w.size.y;

        // Shore lines, only where the neighbour is dry land.
        for(float x = x0; x < x1; x += 2.0f)
        {
            if(!in_water({x + 1, y0 - 1})) rect(m_cold, x, y0, 2.0f, 1.0f, TEAL);
            if(!in_water({x + 1, y1 + 1})) rect(m_cold, x, y1 - 1, 2.0f, 1.0f, TEAL_DARK);
        }
        for(float y = y0; y < y1; y += 2.0f)
        {
            if(!in_water({x0 - 1, y + 1})) rect(m_cold, x0, y, 1.0f, 2.0f, TEAL_DARK);
            if(!in_water({x1 + 1, y + 1})) rect(m_cold, x1 - 1, y, 1.0f, 2.0f, TEAL_DARK);
        }

        // Ripples drifting slowly across.
        for(float y = y0 + 4; y < y1 - 2; y += 7.0f)
        {
            float const row = std::floor(y);
            for(int k = 0; k < static_cast<int>(w.size.x / 22.0f) + 1; ++k)
            {
                float const len   = 3.0f + std::floor(hash(row, static_cast<float>(k)) * 5.0f);
                float const speed = 4.0f + hash(static_cast<float>(k), row) * 6.0f;
                float const x     = x0 + std::fmod(hash(static_cast<float>(k) + 3.0f, row) * w.size.x + time * speed, w.size.x);
                float const bob   = std::floor(std::sin(time * 1.3f + static_cast<float>(k) + row * 0.3f) * 1.2f);
                if(x + len < x1) rect(m_cold, std::floor(x), row + bob, len, 1.0f, TEAL_DARK);
            }
        }

        // Glints.
        for(int k = 0; k < static_cast<int>(w.size.x * w.size.y / 900.0f); ++k)
        {
            float const phase = std::fmod(time * 0.7f + hash(static_cast<float>(k), x0) * 7.0f, 7.0f);
            if(phase > 0.5f) continue;
            float const gx = std::floor(x0 + 2 + hash(x0, static_cast<float>(k)) * (w.size.x - 4));
            float const gy = std::floor(y0 + 2 + hash(static_cast<float>(k), y0) * (w.size.y - 4));
            rect(m_cold, gx, gy, 1.0f, 1.0f, AQUA);
        }
    }

    // --- wind -------------------------------------------------------------------
    for(const auto& wind : m_def->winds)
    {
        if(!overlaps(wind.area, visible)) continue;
        const auto& a = wind.area;
        float const speed = std::hypot(wind.push.x, wind.push.y);
        sf::Vector2f const dir = wind.push / speed;
        int const streaks = static_cast<int>(a.size.x * a.size.y / 1800.0f);

        for(int k = 0; k < streaks; ++k)
        {
            float const fk  = static_cast<float>(k);
            float const len = 6.0f + hash(fk, 1.0f) * 10.0f;
            float const v   = speed * (1.6f + hash(fk, 2.0f) * 1.4f);
            // Position along the wind wraps; across it is fixed per streak.
            float const along_span  = std::abs(dir.x) > 0.5f ? a.size.x : a.size.y;
            float const across_span = std::abs(dir.x) > 0.5f ? a.size.y : a.size.x;
            float along  = std::fmod(hash(fk, 3.0f) * along_span + time * v, along_span + len);
            float const across = hash(fk, 4.0f) * across_span + std::sin(time * 2.0f + fk) * 2.0f;

            float const sign = (dir.x + dir.y) < 0.0f ? -1.0f : 1.0f;
            if(sign < 0.0f) along = along_span - along;

            sf::Vector2f head = std::abs(dir.x) > 0.5f
                ? sf::Vector2f{a.position.x + along, a.position.y + across}
                : sf::Vector2f{a.position.x + across, a.position.y + along};
            if(!visible.contains(head)) continue;
            if(in_wall(head)) continue;

            sf::Color const c = with_alpha(k % 3 == 0 ? AQUA : TEAL, 0.55f);
            for(float s = 0; s < len; s += 1.0f)
            {
                sf::Vector2f const p = head - dir * s;
                if(!near_rect(p, a, 0.0f)) break;
                rect(m_cold, std::floor(p.x), std::floor(p.y), 1.0f, 1.0f,
                     with_alpha(c, 0.55f * (1.0f - s / len)));
            }
        }
    }

    target.draw(m_cold);
}

bool World::collide(sf::Vector2f& pos, float radius) const
{
    bool hit = false;
    for(const auto& w : m_def->walls)
    {
        float const x0 = w.position.x, y0 = w.position.y, x1 = x0 + w.size.x, y1 = y0 + w.size.y;
        float const cx = std::clamp(pos.x, x0, x1);
        float const cy = std::clamp(pos.y, y0, y1);
        float const dx = pos.x - cx, dy = pos.y - cy;
        float const d2 = dx * dx + dy * dy;

        if(d2 > radius * radius) continue;
        hit = true;

        if(d2 > 1e-6f)
        {
            float const d = std::sqrt(d2);
            pos += sf::Vector2f{dx, dy} * ((radius - d) / d);
        }
        else
        {
            // Centre inside the wall: leave by the nearest side.
            float const left = pos.x - x0, right = x1 - pos.x, up = pos.y - y0, down = y1 - pos.y;
            float const m = std::min({left, right, up, down});
            if(m == left)       pos.x = x0 - radius;
            else if(m == right) pos.x = x1 + radius;
            else if(m == up)    pos.y = y0 - radius;
            else                pos.y = y1 + radius;
        }
    }

    sf::Vector2f const before = pos;
    pos.x = std::clamp(pos.x, radius, m_def->size.x - radius);
    pos.y = std::clamp(pos.y, radius, m_def->size.y - radius);
    return hit || pos != before;
}

bool World::in_water(sf::Vector2f p) const
{
    for(const auto& w : m_def->water) if(w.contains(p)) return true;
    return false;
}

sf::Vector2f World::wind_at(sf::Vector2f p) const
{
    sf::Vector2f push;
    for(const auto& w : m_def->winds) if(w.area.contains(p)) push += w.push;
    return push;
}

bool World::in_wall(sf::Vector2f p) const
{
    for(const auto& w : m_def->walls) if(w.contains(p)) return true;
    return false;
}

}
