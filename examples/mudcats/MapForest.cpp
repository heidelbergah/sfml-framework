/**
 * MapForest.cpp  -  Mud Cats
 *
 * The Lesna road, at midday: a cart track through old forest, beeches and
 * pines close on both sides, a stream crossed by a plank bridge, a
 * woodcutter's hut in a clearing. Ferns and brambles under the trees deep
 * enough to lie in. Past the bridge the track climbs gently through the
 * stretch where the trees come closest; at the end of it, a pine has come
 * down across the road. It didn't fall on its own.
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

    constexpr float ROAD_HALF = 18.0f;          // the track is about two wagons wide
    constexpr float STREAM_X = 1080.0f;
    constexpr float DITCH_FROM = 1170.0f;       // along the road
    constexpr float DITCH_TO = 1960.0f;
    const sf::Vector2f HUT{1150.0f, 236.0f};
}

void Map::build_forest()
{
    size = {2600.0f, 1000.0f};
    Rng rng(1939);
    road = {{-20, 560}, {300, 545}, {620, 580}, {940, 560}, {1240, 505}, {1520, 480}, {1800, 495}, {2100, 535}, {2400, 505}, {2620, 490}};
    player_start = road_at(700.0f);

    // --- the stream and its bridge -----------------------------------------------------------
    water.push_back(R(STREAM_X, 0, 30, 1000));
    sf::Vector2f const crossing = road_at(1160.0f);
    (void)crossing;
    bridges.push_back(R(STREAM_X - 8, 508, 46, 48));

    // --- the woodcutter's hut, in its clearing ----------------------------------------------
    wall(R(HUT.x - 26, HUT.y - 20, 52, 5), Material::LOG);
    wall(R(HUT.x - 26, HUT.y - 15, 5, 35), Material::LOG);
    wall(R(HUT.x + 21, HUT.y - 15, 5, 35), Material::LOG);
    wall(R(HUT.x - 21, HUT.y + 15, 12, 5), Material::LOG);               // door in the south side
    wall(R(HUT.x + 9, HUT.y + 15, 12, 5), Material::LOG);
    roofs.push_back(R(HUT.x - 21, HUT.y - 15, 42, 30));
    floors.push_back(R(HUT.x - 26, HUT.y - 20, 52, 40));
    for(float y : {HUT.y + 34.0f, HUT.y + 42.0f})
        low(R(HUT.x - 70, y, 40, 7), Low::LOG);                               // his stacked logs
    low(R(HUT.x + 40, HUT.y + 10, 14, 10), Low::CRATE);                      // a chopping block, a barrow

    // --- the felled pine across the road, and the trees where they wait ---------------------
    sf::Vector2f const block = road_at(1880.0f);
    low(R(block.x - 5, block.y - 26, 10, 52), Low::LOG);
    for(sf::Vector2f p : {sf::Vector2f{block.x + 4, block.y - 34}, {block.x - 2, block.y + 30}})
        low(R(p.x - 3, p.y - 3, 6, 6), Low::TRUNK);
    // Undergrowth along the north side of the stretch: ferns and brambles to lie in.
    for(float x = 1220.0f; x < 1800.0f; x += 70.0f)
        grass.push_back(R(x + rng.real(-10, 10), road_at(x).y - 150.0f + rng.real(-10, 10), rng.real(60, 90), rng.real(70, 100)));
    grass.push_back(R(1840, 380, 90, 70));
    grass.push_back(R(1440, 170, 160, 90));                                  // where the rest of them wait
    // Logs to lie behind in the ambush line: they've dragged one up for nearly every man.
    for(int i = 0; i < 12; ++i)
    {
        float const x = 1260.0f + 44.0f * static_cast<float>(i);
        if(i == 6) continue;                                                  // where the gun is
        low(R(x - 9.0f + rng.real(-3, 3), road_at(x).y - 111.0f - (i % 3 == 1 ? 14.0f : 0.0f), rng.real(18, 26), 5), Low::LOG);
    }
    low(R(1508.0f, road_at(1520.0f).y - 106.0f, 26, 6), Low::LOG);

    // --- the forest: thick everywhere but the road, the stream and the clearing ---------------
    auto clear_of = [&](sf::Vector2f p, float r)
    {
        if(road_distance(p) < ROAD_HALF + r + 8.0f) return false;
        if(std::abs(p.x - (STREAM_X + 15.0f)) < 24.0f + r) return false;
        if(std::hypot(p.x - HUT.x, p.y - HUT.y) < 95.0f + r) return false;
        if(std::hypot(p.x - (block.x + 40.0f), p.y - block.y) < 30.0f) return false;
        for(const auto& t : trees) if(std::hypot(t.pos.x - p.x, t.pos.y - p.y) < (t.canopy + r) * 0.62f) return false;
        for(const auto& s : sandbags) if(s.findIntersection(R(p.x - r * 0.4f, p.y - r * 0.4f, r * 0.8f, r * 0.8f))) return false;
        for(const auto& w : walls) if(w.findIntersection(R(p.x - r, p.y - r, r * 2, r * 2))) return false;
        return true;
    };
    // Between the road and the ambush, the trees stand thickest: trunks to fire round, and to get behind.
    for(int i = 0, tries = 0; i < 46 && tries < 6000; ++tries)
    {
        float const x = rng.real(1225, 1800);
        sf::Vector2f const p{x, road_at(x).y - rng.real(ROAD_HALF + 22.0f, 100.0f)};
        float const r = rng.real(10, 15);
        if(!clear_of(p, r)) continue;
        trees.push_back({p, r, rng.chance(0.5f) ? 2 : 1});
        ++i;
    }
    for(int i = 0, tries = 0; i < 520 && tries < 30000; ++tries)
    {
        sf::Vector2f const p{rng.real(10, size.x - 10), rng.real(10, size.y - 10)};
        float const r = rng.real(12, 21);
        if(!clear_of(p, r)) continue;
        trees.push_back({p, r, rng.chance(0.3f) ? 2 : (rng.chance(0.5f) ? 1 : 0)});
        ++i;
    }
    // Ferns and bracken all through it, not just where they wait.
    for(int i = 0, tries = 0; i < 70 && tries < 4000; ++tries)
    {
        sf::Vector2f const p{rng.real(40, size.x - 120), rng.real(30, size.y - 110)};
        sf::FloatRect const g = R(p.x, p.y, rng.real(50, 110), rng.real(40, 90));
        if(road_distance(g.getCenter()) < ROAD_HALF + 40.0f) continue;
        if(g.contains({STREAM_X + 15.0f, g.getCenter().y})) continue;
        grass.push_back(g);
        ++i;
    }
    // Fallen trees here and there, and boulders.
    for(int i = 0, tries = 0; i < 26 && tries < 3000; ++tries)
    {
        sf::Vector2f const p{rng.real(40, size.x - 80), rng.real(30, size.y - 40)};
        if(road_distance(p) < ROAD_HALF + 30.0f || std::abs(p.x - STREAM_X) < 60.0f) continue;
        bool const across = rng.chance(0.6f);
        sf::FloatRect const r = across ? R(p.x, p.y, rng.real(30, 56), 7) : R(p.x, p.y, 7, rng.real(30, 56));
        bool ok = true;
        for(const auto& t : trees) if(r.contains(t.pos) || std::hypot(t.pos.x - r.getCenter().x, t.pos.y - r.getCenter().y) < 10.0f) ok = false;
        if(!ok) continue;
        low(r, rng.chance(0.75f) ? Low::LOG : Low::STONE);
        ++i;
    }

    // The ditch along the south side of the road, the whole length of the stretch: the one place
    // on the road a man can get down out of the fire.
    {
        Trench ditch;
        ditch.width = 11.0f;
        for(float a = DITCH_FROM; a <= DITCH_TO; a += 30.0f)
        {
            sf::Vector2f const d = road_dir(a);
            ditch.points.push_back(road_at(a) + sf::Vector2f{-d.y, d.x} * (ROAD_HALF + 7.0f));
        }
        trenches.push_back(ditch);
    }

    crates = {road_at(260.0f), road_at(2350.0f)};
    // Where the column's sections go once it's sprung: off the road, up into the trees at them.
    sf::Vector2f const kill = road_at(1560.0f);
    sf::Vector2f const kd = road_dir(1560.0f);
    waypoints = {kill + sf::Vector2f{-kd.y, kd.x} * (ROAD_HALF + 7.0f), kill + sf::Vector2f{0.0f, -80.0f}, kill + sf::Vector2f{0.0f, -165.0f}, kill + sf::Vector2f{0.0f, -280.0f}};
    road_area = R(1150, 430, 760, 130);
    ambush_area = R(1200, 150, 760, 300);
}

void Map::render_forest(sf::RenderTexture& ground) const
{
    Rng rng(84);
    sf::VertexArray va(sf::PrimitiveType::Triangles);
    ground.clear(MEADOW_DK);

    // --- the forest floor: moss, needles, dead leaves, patches of sun ---------------------------
    for(float y = 0; y < size.y; y += 4.0f)
        for(float x = 0; x < size.x; x += 4.0f)
        {
            float const m = field(x / 46.0f, y / 46.0f) * 0.6f + field(x / 11.0f, y / 11.0f) * 0.4f;
            float const sun = field(x / 70.0f + 30.0f, y / 70.0f);
            sf::Color c = m < 0.3f ? lerp(MEADOW_DK, INK, 0.25f) : (m < 0.55f ? MEADOW_DK : (m < 0.78f ? lerp(MEADOW_DK, BARK, 0.45f) : lerp(GRASS_DEAD, BARK, 0.35f)));
            if(sun > 0.72f) c = lerp(c, MEADOW_LT, 0.35f);
            if(in_grass({x + 2, y + 2})) c = m < 0.5f ? lerp(LEAF, MEADOW_DK, 0.4f) : LEAF;
            draw::rect(va, x, y, 4, 4, c);
        }
    ground.draw(va); va.clear();

    // Leaf litter and pine needles.
    for(int i = 0; i < 9000; ++i)
    {
        float const x = rng.real(0, size.x), y = rng.real(0, size.y);
        if(road_distance({x, y}) < ROAD_HALF - 2.0f) continue;
        sf::Color const c = rng.chance(0.35f) ? lerp(WHEAT_DK, BARK, 0.4f) : (rng.chance(0.5f) ? lerp(BARK, MUD_LIGHT, 0.3f) : lerp(MEADOW_LT, WHEAT_DK, 0.3f));
        draw::rect(va, std::floor(x), std::floor(y), rng.chance(0.3f) ? 2.0f : 1.0f, 1, c);
    }
    // Ferns in the undergrowth: fronds fanning out from a centre.
    for(const auto& g : grass)
        for(int k = 0; k < static_cast<int>(g.size.x * g.size.y / 140.0f); ++k)
        {
            sf::Vector2f const c{rng.real(g.position.x + 3, g.position.x + g.size.x - 3), rng.real(g.position.y + 3, g.position.y + g.size.y - 3)};
            sf::Color const leaf = rng.chance(0.5f) ? LEAF_LT : lerp(LEAF_LT, WHEAT, 0.2f);
            for(int f = 0; f < 5; ++f)
            {
                float const a = -2.6f + static_cast<float>(f) * 0.55f + rng.real(-0.15f, 0.15f);
                draw::thick_line(va, c, c + sf::Vector2f{std::cos(a), std::sin(a)} * rng.real(4.0f, 7.0f), 1.0f, leaf);
            }
        }
    ground.draw(va); va.clear();

    // --- the road: a dirt track, two ruts, grass up the middle, puddles ---------------------------
    float const len = road_length();
    for(float s = 0.0f; s < len; s += 2.0f)
    {
        sf::Vector2f const p = road_at(s), d = road_dir(s), n{-d.y, d.x};
        for(float k = -ROAD_HALF - 3.0f; k <= ROAD_HALF + 3.0f; k += 2.0f)
        {
            sf::Vector2f const q = p + n * k;
            float const m = field(q.x / 9.0f, q.y / 9.0f);
            float const edge = std::abs(k) / ROAD_HALF;
            sf::Color c = m < 0.5f ? MUD : MUD_LIGHT;
            if(std::abs(std::abs(k) - 9.0f) < 2.5f) c = lerp(MUD_DARK, MUD, m);                      // the ruts
            if(std::abs(k) < 3.5f && m > 0.35f) c = lerp(GRASS_DEAD, MEADOW_DK, m);                // grass up the middle
            if(edge > 1.0f) c = lerp(MEADOW_DK, MUD, 0.5f);                                       // the verge
            draw::rect(va, std::floor(q.x), std::floor(q.y), 2, 2, c);
        }
        if(field(s / 23.0f, 3.0f) > 0.82f && std::fmod(s, 6.0f) < 2.0f)
            for(float k : {-9.0f, 9.0f})
                draw::disc(va, p.x + n.x * k, p.y + n.y * k, 2.5f, lerp(mudcats::WATER, PAPER_DK, 0.15f));
    }
    ground.draw(va); va.clear();

    // --- the ditch: a dark wet bottom, a lip of turned earth on the road side ------------------------
    for(const auto& t : trenches)
    {
        for(std::size_t i = 0; i + 1 < t.points.size(); ++i) draw::thick_line(va, t.points[i], t.points[i + 1], t.width + 4.0f, lerp(MUD, MEADOW_DK, 0.35f));
        for(std::size_t i = 0; i + 1 < t.points.size(); ++i) draw::thick_line(va, t.points[i], t.points[i + 1], t.width, MUD_DARK);
        for(std::size_t i = 0; i + 1 < t.points.size(); ++i)
            draw::thick_line(va, t.points[i] + sf::Vector2f{0.0f, 1.5f}, t.points[i + 1] + sf::Vector2f{0.0f, 1.5f}, t.width * 0.35f, lerp(MUD_DARK, mudcats::WATER, 0.25f));
        for(auto p : t.points) draw::disc(va, p.x, p.y, t.width * 0.5f, MUD_DARK);
    }
    ground.draw(va); va.clear();

    // --- the stream: dark water, stony banks, the plank bridge -------------------------------------
    for(const auto& w : water)
    {
        for(float y = w.position.y; y < w.position.y + w.size.y; y += 2.0f)
            for(float x = w.position.x - 4.0f; x < w.position.x + w.size.x + 4.0f; x += 2.0f)
            {
                bool const bank = x < w.position.x || x >= w.position.x + w.size.x;
                float const m = field(x / 6.0f, y / 14.0f + 7.0f);
                sf::Color const c = bank ? (m < 0.5f ? STONE_DK : lerp(STONE, MEADOW_DK, 0.4f))
                                         : (m < 0.65f ? STREAM : STREAM_LT);
                draw::rect(va, x, y, 2, 2, c);
            }
    }
    for(const auto& b : bridges)
    {
        draw::rect(va, b.position.x + 2, b.position.y + 2, b.size.x, b.size.y, SHADOW);
        for(float x = b.position.x; x < b.position.x + b.size.x; x += 4.0f)
            draw::rect(va, x, b.position.y, 3, b.size.y, static_cast<int>(x) / 4 % 2 ? PLANK : lerp(PLANK, PLANK_DARK, 0.5f));
        draw::rect(va, b.position.x, b.position.y, b.size.x, 2, PLANK_DARK);
        draw::rect(va, b.position.x, b.position.y + b.size.y - 2, b.size.x, 2, PLANK_DARK);
    }
    ground.draw(va); va.clear();

    // --- the clearing round the hut: trampled grass, chips of wood ---------------------------------
    sf::Vector2f const hut = HUT;
    for(int i = 0; i < 900; ++i)
    {
        float const a = rng.real(0, 6.283f), d = std::sqrt(rng.real(0, 1)) * 90.0f;
        sf::Vector2f const p = hut + sf::Vector2f{std::cos(a), std::sin(a)} * d;
        draw::rect(va, std::floor(p.x), std::floor(p.y), 2, 1, rng.chance(0.4f) ? lerp(PLANK, PAPER, 0.3f) : lerp(GRASS_DEAD, MEADOW_DK, 0.4f));
    }
    for(const auto& f : floors)
        for(float y = f.position.y; y < f.position.y + f.size.y; y += 4.0f)
            draw::rect(va, f.position.x, y, f.size.x, 3, static_cast<int>(y) / 4 % 2 ? PLANK : PLANK_DARK);
    ground.draw(va);
}

}
