/**
 * Screens.cpp  -  Mud Cats
 */

#include "Screens.hpp"

#include "Battle.hpp"
#include "Pad.hpp"
#include "../../include/core/Random.hpp"
#include "Draw.hpp"
#include "Icons.hpp"
#include "Palette.hpp"
#include "Settings.hpp"
#include "Sound.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace mudcats
{

namespace
{
    constexpr float W = 960.0f;
    constexpr float H = 540.0f;
    constexpr float PI = 3.14159f;

    // The order you're handed: off-white paper, black ink.
    const sf::Color SHEET    {0xE6, 0xE1, 0xD3};
    const sf::Color SHEET_DK {0xCF, 0xC8, 0xB6};
    const sf::Color PEN      {0x1C, 0x1A, 0x18};

    const char* const FONT = "assets/fonts/pixel-font-bitmap.ttf";

    sf::Text text(const sf::Font& f, const std::string& s, unsigned size, sf::Vector2f pos, sf::Color c, float align = 0.0f)
    {
        sf::Text t(f, s, size);
        t.setFillColor(c);
        sf::FloatRect const b = t.getLocalBounds();
        t.setOrigin({std::round(b.position.x + b.size.x * align), std::round(b.position.y)});
        t.setPosition({std::round(pos.x), std::round(pos.y)});
        return t;
    }

    void shadowed(sf::RenderTarget& t, const sf::Font& f, const std::string& s, unsigned size, sf::Vector2f pos, sf::Color c, float align = 0.0f)
    {
        float const off = size >= 32 ? 2.0f : 1.0f;
        t.draw(text(f, s, size, pos + sf::Vector2f{off, off}, with_alpha(INK, c.a / 255.0f * 0.9f), align));
        t.draw(text(f, s, size, pos, c, align));
    }

    float width_of(const sf::Font& f, const std::string& s, unsigned size)
    {
        return sf::Text(f, s, size).getLocalBounds().size.x;
    }

    // Shortens `s` with an ellipsis until it fits.
    std::string fit(const sf::Font& f, std::string s, unsigned size, float width)
    {
        if(width_of(f, s, size) <= width) return s;
        while(s.size() > 3 && width_of(f, s + "...", size) > width) s.pop_back();
        while(!s.empty() && (s.back() == ' ' || s.back() == ',')) s.pop_back();
        return s + "...";
    }

    // Breaks `s` into lines no wider than `width`.
    std::vector<std::string> wrap(const sf::Font& f, const std::string& s, unsigned size, float width)
    {
        std::vector<std::string> lines;
        std::istringstream words(s);
        std::string word, line;
        while(words >> word)
        {
            std::string const longer = line.empty() ? word : line + " " + word;
            if(!line.empty() && width_of(f, longer, size) > width)
            {
                lines.push_back(line);
                line = word;
            }
            else line = longer;
        }
        if(!line.empty()) lines.push_back(line);
        return lines;
    }

    void box(sf::RenderTarget& t, sf::Vector2f p, sf::Vector2f s, sf::Color fill, sf::Color line = sf::Color::Transparent)
    {
        sf::RectangleShape r(s - sf::Vector2f{2, 2});
        r.setPosition(p + sf::Vector2f{1, 1});
        r.setFillColor(fill);
        r.setOutlineThickness(line.a ? 1.0f : 0.0f);
        r.setOutlineColor(line);
        t.draw(r);
    }

    float hash(float a) { float const s = std::sin(a * 127.1f) * 43758.5453f; return s - std::floor(s); }

    // A ring, as a band of thin quads.
    void ring(sf::VertexArray& va, sf::Vector2f c, float r, float width, sf::Color col, int segments = 0)
    {
        if(segments <= 0) segments = std::clamp(static_cast<int>(r * 1.6f), 10, 64);
        for(int i = 0; i < segments; ++i)
        {
            float const a0 = 2.0f * PI * static_cast<float>(i) / static_cast<float>(segments);
            float const a1 = 2.0f * PI * static_cast<float>(i + 1) / static_cast<float>(segments);
            sf::Vector2f const u0{std::cos(a0), std::sin(a0)}, u1{std::cos(a1), std::sin(a1)};
            draw::quad(va, c + u0 * r, c + u1 * r, c + u1 * (r - width), c + u0 * (r - width), col);
        }
    }

    void dashed(sf::VertexArray& va, sf::Vector2f a, sf::Vector2f b, float width, sf::Color c, float dash = 5.0f, float gap = 4.0f)
    {
        sf::Vector2f const d = b - a;
        float const len = std::sqrt(d.x * d.x + d.y * d.y);
        if(len < 0.5f) return;
        sf::Vector2f const u = d / len;
        for(float s = 0.0f; s < len; s += dash + gap)
            draw::thick_line(va, a + u * s, a + u * std::min(len, s + dash), width, c);
    }

    void arrow_head(sf::VertexArray& va, sf::Vector2f tip, sf::Vector2f dir, float size, sf::Color c)
    {
        sf::Vector2f const n{-dir.y, dir.x};
        sf::Vector2f const back = tip - dir * size;
        draw::quad(va, tip, back + n * size * 0.55f, back, back - n * size * 0.55f, c);
    }

    // Hatching inside a rectangle, at 45 degrees, `gap` apart: a building on a staff map.
    void hatch(sf::VertexArray& va, const sf::FloatRect& r, float gap, sf::Color c)
    {
        float const x0 = r.position.x, y0 = r.position.y, x1 = x0 + r.size.x, y1 = y0 + r.size.y;
        for(float k = x0 + y0 + gap * 0.5f; k < x1 + y1; k += gap)
        {
            // The line x + y = k, clipped to the rectangle.
            sf::Vector2f a{std::max(x0, k - y1), 0.0f}, b{std::min(x1, k - y0), 0.0f};
            a.y = k - a.x;
            b.y = k - b.x;
            if(a.x > b.x) continue;
            draw::thick_line(va, a, b, 0.8f, c);
        }
    }

    // --- the classes ------------------------------------------------------------------------------
    // Each class's card: the words are in the text file, under roles.<key>.
    const Role CARDS[] = {
        Role::RIFLEMAN, Role::OFFICER, Role::MEDIC, Role::FLAMER, Role::GUNNER,
        Role::RADIOMAN, Role::STORMTROOPER, Role::SNIPER, Role::SAPPER,
    };
    constexpr int CARD_COUNT = 9;

    // --- the battles ------------------------------------------------------------------------------
    const LevelId PLACES[] = {LevelId::CHARGE, LevelId::TOWN, LevelId::FOREST};
    constexpr int PLACE_COUNT = 3;

    // levels.<battle>.<what>
    std::string level_text(LevelId level, const std::string& what)
    {
        return tr(std::string("levels.") + level_key(level) + "." + what);
    }

    struct Orders
    {
        std::string situation;
        std::string mission;
        std::string notes;
    };

    Orders orders(LevelId level, Team side)
    {
        std::string const at = std::string("orders.") + side_key(side) + ".";
        return {level_text(level, at + "situation"), level_text(level, at + "mission"), level_text(level, at + "notes")};
    }

    // --- what's drawn on the map ------------------------------------------------------------------
    struct Mark
    {
        char kind;          // U unit, G gun, A arrow, O objective, R objective area, ? suspected, W wagon, M mine, T text
        Team team;
        std::vector<sf::Vector2f> pts;
        std::string text;
    };

    std::vector<Mark> marks(LevelId level, Team side, const Map& map)
    {
        constexpr Team MON = Team::MONTERAN, RCH = Team::RECHIVII;
        bool const mon = side == MON;
        std::vector<Mark> m;
        switch(level)
        {
            case LevelId::CHARGE:
                m.push_back({'R', mon ? MON : RCH, {{372, 84}, {84, 932}}, level_text(level, mon ? "map.hold" : "map.objective")});
                for(float y : {300.0f, 550.0f, 800.0f}) m.push_back({'U', MON, {{410, y}}, y < 400 ? "1" : (y < 700 ? "2" : "3")});
                m.push_back({'U', MON, {{250, 560}}, level_text(level, "map.hq")});
                m.push_back({'G', MON, {{470, 360}}, ""});
                m.push_back({'G', MON, {{470, 760}}, ""});
                for(float y : {230.0f, 550.0f, 870.0f})
                {
                    m.push_back({'U', RCH, {{2380, y}}, level_text(level, "map.first")});
                    m.push_back({'U', RCH, {{2500, y}}, level_text(level, "map.second")});
                }
                for(float y : {230.0f, 550.0f, 870.0f}) m.push_back({'A', RCH, {{2320, y}, {1500, y + (550.0f - y) * 0.12f}, {520, y + (550.0f - y) * 0.15f}}, ""});
                m.push_back({'T', RCH, {{2580, 30}}, level_text(level, "map.wood")});
                break;
            case LevelId::TOWN:
                m.push_back({'O', mon ? MON : RCH, {{2138, 500}, {150, 0}}, level_text(level, mon ? "map.objective" : "map.hold")});
                m.push_back({'U', MON, {{110, 420}}, "1"});
                m.push_back({'U', MON, {{110, 500}}, "2"});
                m.push_back({'U', MON, {{110, 580}}, "3"});
                m.push_back({'U', MON, {{110, 700}}, level_text(level, "map.hq")});
                m.push_back({'A', MON, {{180, 410}, {820, 395}, {1440, 400}}, ""});
                m.push_back({'A', MON, {{180, 590}, {820, 605}, {1440, 600}}, ""});
                m.push_back({'A', MON, {{1520, 520}, {1760, 560}, {1990, 515}}, ""});
                m.push_back({'U', RCH, {{540, 400}}, ""});
                m.push_back({'U', RCH, {{540, 600}}, ""});
                m.push_back({'U', RCH, {{1180, 400}}, ""});
                m.push_back({'U', RCH, {{1180, 600}}, ""});
                m.push_back({'U', RCH, {{1800, 330}}, ""});
                m.push_back({'G', RCH, {{864, 492}}, ""});
                m.push_back({'G', RCH, {{1540, 512}}, ""});
                m.push_back({'G', RCH, {{2040, 477}}, ""});
                if(!mon)
                {
                    m.push_back({'U', RCH, {{2340, 500}}, level_text(level, "map.reserve")});
                    m.push_back({'A', RCH, {{2320, 560}, {2240, 640}, {2120, 610}}, ""});
                }
                m.push_back({'T', RCH, {{2060, 640}}, level_text(level, "map.church")});
                break;
            case LevelId::FOREST:
            {
                auto at = [&map](float along, float off = 0.0f)
                {
                    sf::Vector2f const d = map.road_dir(along);
                    return map.road_at(along) + sf::Vector2f{-d.y, d.x} * off;
                };
                for(int k = 0; k < 6; ++k) m.push_back({'W', MON, {at(860.0f - 58.0f * static_cast<float>(k)), map.road_dir(860.0f - 58.0f * static_cast<float>(k))}, ""});
                if(mon)
                {
                    m.push_back({'U', MON, {at(1020.0f, 44.0f)}, "1"});
                    m.push_back({'U', MON, {at(720.0f, 44.0f)}, level_text(level, "map.hq")});
                    m.push_back({'U', MON, {at(640.0f, -44.0f)}, "2"});
                    m.push_back({'U', MON, {at(470.0f, 44.0f)}, "3"});
                }
                else m.push_back({'U', MON, {at(660.0f, 50.0f)}, level_text(level, "map.escort")});
                {
                    Mark march{'A', MON, {}, ""};
                    for(float a = 1080.0f; a <= 2480.0f; a += 140.0f) march.pts.push_back(at(a, 34.0f));
                    m.push_back(march);
                }
                m.push_back({'O', mon ? MON : RCH, mon ? std::vector<sf::Vector2f>{at(2470.0f), {70, 0}} : std::vector<sf::Vector2f>{at(1460.0f), {170, 0}}, level_text(level, mon ? "map.objective" : "map.kill_zone")});
                if(mon)
                {
                    m.push_back({'?', RCH, {{1520, 300}, {330, 120}}, ""});
                    m.push_back({'T', RCH, {{1300, 140}}, tr("levels.forest.map.patrols")});
                    break;
                }
                for(float x : {1310.0f, 1740.0f}) m.push_back({'U', RCH, {at(x, -126.0f)}, ""});
                m.push_back({'G', RCH, {at(1520.0f, -118.0f)}, ""});
                m.push_back({'U', RCH, {at(1915.0f, -14.0f)}, level_text(level, "map.block")});
                m.push_back({'U', RCH, {{1520, 205}}, level_text(level, "map.assault")});
                m.push_back({'A', RCH, {{1520, 232}, {1500, 340}, at(1470.0f, -30.0f)}, ""});
                m.push_back({'M', RCH, {at(1640.0f)}, ""});
                m.push_back({'T', RCH, {at(1612.0f, 30.0f)}, level_text(level, "map.mine")});
                break;
            }
        }
        return m;
    }

    // --- the map, in ink ----------------------------------------------------------------------------
    void draw_sheet(const Map& map, sf::RenderTexture& rt)
    {
        sf::Vector2f const px(rt.getSize());
        float const k = px.x / map.size.x;
        auto at = [k](sf::Vector2f p) { return p * k; };
        sf::Color const ink = PEN;
        sf::VertexArray va(sf::PrimitiveType::Triangles);
        rt.clear(SHEET);

        // The grid, faint, every 200 yards or so.
        for(float x = 200.0f; x < map.size.x; x += 200.0f) draw::rect(va, std::floor(x * k), 0, 1, px.y, with_alpha(ink, 0.08f));
        for(float y = 200.0f; y < map.size.y; y += 200.0f) draw::rect(va, 0, std::floor(y * k), px.x, 1, with_alpha(ink, 0.08f));

        // Water: ruled across.
        for(const auto& w : map.water)
        {
            sf::FloatRect const r(at(w.position), w.size * k);
            for(float y = r.position.y; y < r.position.y + r.size.y; y += 2.0f) draw::rect(va, r.position.x, y, r.size.x, 1, with_alpha(ink, 0.45f));
            draw::rect(va, r.position.x - 1, r.position.y, 1, r.size.y, with_alpha(ink, 0.8f));
            draw::rect(va, r.position.x + r.size.x, r.position.y, 1, r.size.y, with_alpha(ink, 0.8f));
        }
        // Rough grass and bracken: ticks.
        for(const auto& g : map.grass)
        {
            int const n = static_cast<int>(g.size.x * g.size.y * k * k / 28.0f);
            for(int i = 0; i < n; ++i)
            {
                float const x = g.position.x + hash(g.position.x + static_cast<float>(i) * 1.31f) * g.size.x;
                float const y = g.position.y + hash(g.position.y + static_cast<float>(i) * 2.17f) * g.size.y;
                draw::rect(va, std::floor(x * k), std::floor(y * k), 1, 2, with_alpha(ink, 0.35f));
            }
        }
        // The forest road: two thin lines.
        if(!map.road.empty())
        {
            float const len = map.road_length();
            for(float s = 0.0f; s < len; s += 3.0f)
            {
                sf::Vector2f const p = map.road_at(s), d = map.road_dir(s), n{-d.y, d.x};
                for(float side : {-18.0f, 18.0f})
                {
                    sf::Vector2f const q = at(p + n * side);
                    draw::rect(va, std::floor(q.x), std::floor(q.y), 1, 1, with_alpha(ink, 0.75f));
                }
            }
        }
        for(const auto& b : map.bridges)
        {
            sf::FloatRect const r(at(b.position), b.size * k);
            draw::rect(va, r.position.x, r.position.y, r.size.x, r.size.y, SHEET);
            draw::rect(va, r.position.x, r.position.y, 1, r.size.y, ink);
            draw::rect(va, r.position.x + r.size.x, r.position.y, 1, r.size.y, ink);
        }
        // Trees: little rings, the classic wood.
        for(const auto& tr : map.trees)
        {
            float const r = std::max(1.6f, tr.canopy * k * 0.75f);
            ring(va, at(tr.pos), r, 0.8f, with_alpha(ink, 0.42f), 8);
        }
        // Trenches: a line with its traverses ticked off along it.
        for(const auto& tch : map.trenches)
            for(std::size_t i = 0; i + 1 < tch.points.size(); ++i)
            {
                sf::Vector2f const a = at(tch.points[i]), b = at(tch.points[i + 1]);
                draw::thick_line(va, a, b, 1.4f, ink);
                sf::Vector2f const d = b - a;
                float const len = std::sqrt(d.x * d.x + d.y * d.y);
                if(len < 1.0f) continue;
                sf::Vector2f const u = d / len, n{-u.y, u.x};
                for(float s = 1.5f; s < len; s += 3.0f) draw::thick_line(va, a + u * s, a + u * s + n * 2.0f, 0.8f, ink);
            }
        for(const auto& c : map.craters) ring(va, at(c.pos), std::max(1.5f, c.radius * k), 0.8f, with_alpha(ink, 0.6f), 10);
        // Wire: a row of crosses.
        for(const auto& w : map.wire)
        {
            sf::Vector2f const c = at(w.getCenter());
            draw::thick_line(va, c + sf::Vector2f{-1.5f, -1.5f}, c + sf::Vector2f{1.5f, 1.5f}, 0.7f, ink);
            draw::thick_line(va, c + sf::Vector2f{-1.5f, 1.5f}, c + sf::Vector2f{1.5f, -1.5f}, 0.7f, ink);
        }
        for(const auto& h : map.hedges)
        {
            sf::FloatRect const r(at(h.position), h.size * k);
            bool const across = r.size.x > r.size.y;
            float const len = across ? r.size.x : r.size.y;
            for(float s = 0.0f; s < len; s += 2.0f)
                draw::rect(va, std::floor(r.position.x + (across ? s : r.size.x * 0.5f)), std::floor(r.position.y + (across ? r.size.y * 0.5f : s)), 1, 1, ink);
        }
        // Buildings: hatched under the roofs, the walls in solid ink.
        for(const auto& r : map.roofs) hatch(va, sf::FloatRect(at(r.position), r.size * k), 2.0f, with_alpha(ink, 0.55f));
        for(const auto& w : map.walls)
        {
            sf::FloatRect const r(at(w.position), w.size * k);
            draw::rect(va, r.position.x, r.position.y, std::max(1.0f, r.size.x), std::max(1.0f, r.size.y), ink);
        }
        for(const auto& s : map.sandbags)
        {
            sf::FloatRect const r(at(s.position), s.size * k);
            draw::rect(va, r.position.x, r.position.y, std::max(1.0f, r.size.x), std::max(1.0f, r.size.y), with_alpha(ink, 0.7f));
        }
        rt.draw(va);
        rt.display();
    }

    // A unit's symbol: ours a box with the cross of the infantry in it, theirs a diamond.
    void unit_symbol(sf::VertexArray& va, sf::Vector2f c, bool ours, sf::Color ink)
    {
        if(ours)
        {
            float const w = 9.0f, h = 6.0f;
            draw::rect(va, c.x - w, c.y - h, w * 2, h * 2, SHEET);
            draw::rect(va, c.x - w, c.y - h, w * 2, 1.5f, ink);
            draw::rect(va, c.x - w, c.y + h - 1.5f, w * 2, 1.5f, ink);
            draw::rect(va, c.x - w, c.y - h, 1.5f, h * 2, ink);
            draw::rect(va, c.x + w - 1.5f, c.y - h, 1.5f, h * 2, ink);
            draw::thick_line(va, {c.x - w + 1, c.y - h + 1}, {c.x + w - 1, c.y + h - 1}, 1.0f, ink);
            draw::thick_line(va, {c.x - w + 1, c.y + h - 1}, {c.x + w - 1, c.y - h + 1}, 1.0f, ink);
            return;
        }
        float const r = 8.0f;
        sf::Vector2f const top{c.x, c.y - r}, right{c.x + r, c.y}, bottom{c.x, c.y + r}, left{c.x - r, c.y};
        draw::quad(va, top, right, bottom, left, SHEET);
        draw::thick_line(va, top, right, 1.5f, ink);
        draw::thick_line(va, right, bottom, 1.5f, ink);
        draw::thick_line(va, bottom, left, 1.5f, ink);
        draw::thick_line(va, left, top, 1.5f, ink);
        draw::thick_line(va, c + sf::Vector2f{-3.5f, -3.5f}, c + sf::Vector2f{3.5f, 3.5f}, 1.0f, ink);
        draw::thick_line(va, c + sf::Vector2f{-3.5f, 3.5f}, c + sf::Vector2f{3.5f, -3.5f}, 1.0f, ink);
    }

    // A machine gun: a filled dot with an arrow for its barrel, in the frame of whichever side.
    void gun_symbol(sf::VertexArray& va, sf::Vector2f c, bool ours, sf::Color ink)
    {
        if(ours)
        {
            draw::rect(va, c.x - 5, c.y - 5, 10, 10, SHEET);
            draw::rect(va, c.x - 5, c.y - 5, 10, 1.5f, ink);
            draw::rect(va, c.x - 5, c.y + 3.5f, 10, 1.5f, ink);
            draw::rect(va, c.x - 5, c.y - 5, 1.5f, 10, ink);
            draw::rect(va, c.x + 3.5f, c.y - 5, 1.5f, 10, ink);
        }
        else
        {
            draw::quad(va, c + sf::Vector2f{0, -6}, c + sf::Vector2f{6, 0}, c + sf::Vector2f{0, 6}, c + sf::Vector2f{-6, 0}, ink);
            draw::quad(va, c + sf::Vector2f{0, -4.5f}, c + sf::Vector2f{4.5f, 0}, c + sf::Vector2f{0, 4.5f}, c + sf::Vector2f{-4.5f, 0}, SHEET);
        }
        draw::disc(va, c.x, c.y, 1.8f, ink);
    }
}

void draw_pad_gate(sf::RenderTarget& t)
{
    if(!pad::missing()) return;
    const sf::Font* f = Assets::font(FONT);
    if(!f) return;
    sf::View const before = t.getView();
    t.setView(t.getDefaultView());
    box(t, {0, 0}, {W, H}, with_alpha(INK, 0.94f));
    // A pad, in outline: the body, the two grips, a stick and four buttons.
    sf::VertexArray va(sf::PrimitiveType::Triangles);
    float const cx = W * 0.5f, cy = 200.0f;
    float const pulse = 0.55f + 0.45f * std::sin(GameTime::real_now() * 3.0f);
    sf::Color const ink = with_alpha(MON_GOLD, pulse);
    draw::rect(va, cx - 60, cy - 18, 120, 4, ink);
    draw::rect(va, cx - 60, cy - 18, 4, 30, ink);
    draw::rect(va, cx + 56, cy - 18, 4, 30, ink);
    draw::rect(va, cx - 72, cy + 8, 4, 30, ink);
    draw::rect(va, cx + 68, cy + 8, 4, 30, ink);
    draw::rect(va, cx - 72, cy + 34, 30, 4, ink);
    draw::rect(va, cx + 42, cy + 34, 30, 4, ink);
    draw::rect(va, cx - 42, cy + 18, 84, 4, ink);
    draw::disc(va, cx - 30, cy + 2, 6.0f, ink);
    for(sf::Vector2f b : {sf::Vector2f{30, -6}, {38, 2}, {22, 2}, {30, 10}}) draw::disc(va, cx + b.x, cy + b.y, 2.5f, ink);
    t.draw(va);
    shadowed(t, *f, tr("pad.connect.title"), 32, {cx, 270}, PAPER, 0.5f);
    t.draw(text(*f, tr("pad.connect.body"), 16, {cx, 316}, TEXT_DIM, 0.5f));
    t.setView(before);
}

bool global_key(const sf::Event& event, SceneManager& manager)
{
    // The mouse pointer only while the mouse is what you're using.
    static int shown = -1;
    int const want = pad::in_use() ? 0 : 1;
    if(want != shown) { manager.get_window().setMouseCursorVisible(want == 1); shown = want; }

    const auto* key = event.getIf<sf::Event::KeyPressed>();
    if(!key || key->scancode != sf::Keyboard::Scancode::F11) return false;
    Settings& s = Settings::get();
    s.fullscreen = !manager.is_fullscreen();
    s.apply_display(manager);
    s.save();
    return true;
}

// ================================================================ the panels

namespace
{
    // Where the settings panel puts things.
    const sf::FloatRect SETTINGS_BOX({250, 82}, {460, 392});
    sf::FloatRect sound_bar(int row) { return {{410.0f, 158.0f + 30.0f * static_cast<float>(row)}, {200.0f, 8.0f}}; }
    sf::FloatRect settings_row(int row)
    {
        if(row < 4) return {{270.0f, 148.0f + 30.0f * static_cast<float>(row)}, {420.0f, 28.0f}};
        if(row == 4) return {{270.0f, 290.0f}, {420.0f, 50.0f}};
        if(row == 5) return {{270.0f, 366.0f}, {420.0f, 26.0f}};
        if(row == 6) return {{290.0f, 418.0f}, {180.0f, 28.0f}};
        return {{490.0f, 418.0f}, {180.0f, 28.0f}};
    }
    // The two choices for effects: full, low.
    sf::FloatRect effects_button(int i)
    {
        return i == 0 ? sf::FloatRect{{380.0f, 368.0f}, {70.0f, 22.0f}} : sf::FloatRect{{454.0f, 368.0f}, {226.0f, 22.0f}};
    }
    sf::FloatRect display_button(int i, int count)
    {
        float const width = 400.0f / static_cast<float>(count);
        return {{280.0f + width * static_cast<float>(i), 312.0f}, {width - 4.0f, 24.0f}};
    }

    // A list of pairs in the text file, key.0, key.1, ...: the keys cards and the credits.
    std::vector<std::pair<std::string, std::string>> pairs(std::string key)
    {
        if(pad_text() && has_text(key + "_pad.0")) key += "_pad";        // the pad's buttons, not the keys
        std::vector<std::pair<std::string, std::string>> out;
        for(std::size_t i = 0; has_text(key + "." + std::to_string(i)); ++i)
        {
            const auto& two = list(key + "." + std::to_string(i));
            out.emplace_back(two.size() > 0 ? two[0] : "", two.size() > 1 ? two[1] : "");
        }
        return out;
    }
}

void Panels::open(Kind kind)
{
    m_kind = kind;
    m_time = 0.0f;
    m_row = 0;
    m_drag = -1;
    SoundBank::get().play("paper", 0.5f);
}

void Panels::close()
{
    if(m_kind == Kind::SETTINGS) Settings::get().save();
    if(m_kind == Kind::HOW_TO && !Settings::get().seen_how_to)
    {
        Settings::get().seen_how_to = true;
        Settings::get().save();
    }
    m_kind = Kind::NONE;
    m_drag = -1;
    SoundBank::get().play("ui_select", 0.5f);
}

void Panels::set_level(int bus, float value)
{
    auto& bank = SoundBank::get();
    auto const b = static_cast<SoundBank::Bus>(bus);
    float const before = bank.volume(b);
    bank.set_volume(b, value);
    if(bank.volume(b) == before) return;
    if(b == SoundBank::Bus::VOICES) bank.play("grunt", 0.7f);
    else if(b != SoundBank::Bus::MUSIC) bank.play("ui_hover", 0.8f);
}

int Panels::display_choices() const
{
    // Each window size that fits, then fullscreen.
    return static_cast<int>(std::min(4u, SceneManager::largest_window_scale())) + 1;
}

int Panels::display_choice(const SceneManager& manager) const
{
    if(manager.is_fullscreen()) return display_choices() - 1;
    return std::clamp(static_cast<int>(manager.get_window_scale()) - 1, 0, display_choices() - 2);
}

void Panels::set_display_choice(int choice, SceneManager& manager)
{
    Settings& s = Settings::get();
    choice = std::clamp(choice, 0, display_choices() - 1);
    s.fullscreen = choice == display_choices() - 1;
    if(!s.fullscreen) s.window_scale = choice + 1;
    s.apply_display(manager);
    SoundBank::get().play("ui_select", 0.6f);
}

void Panels::step_display(int step, SceneManager& manager)
{
    set_display_choice(display_choice(manager) + step, manager);
}

void Panels::set_low_effects(bool low, SceneManager& manager)
{
    Settings& s = Settings::get();
    if(s.low_effects == low) return;
    s.low_effects = low;
    (void)manager;      // bloom follows it from the next battle on: the menus never use it
    SoundBank::get().play("ui_select", 0.6f);
}

void Panels::activate(int row, SceneManager& manager)
{
    if(row == 5) { set_low_effects(!Settings::get().low_effects, manager); return; }
    if(row == 6) { open(Kind::HOW_TO); return; }
    if(row == 7) close();
}

bool Panels::handle_event(const sf::Event& event, sf::Vector2f mouse, SceneManager& manager)
{
    if(m_kind == Kind::NONE) return false;
    using S = sf::Keyboard::Scancode;
    const auto* key = event.getIf<sf::Event::KeyPressed>();

    if(m_kind != Kind::SETTINGS)
    {
        // How to play, the credits: any key or click puts it away.
        bool const click = event.is<sf::Event::MouseButtonPressed>();
        if((key || click) && m_time > 0.25f) close();
        return true;
    }

    if(key)
    {
        switch(key->scancode)
        {
            case S::Escape: case S::Backspace: close(); break;
            case S::Up: case S::W: m_row = (m_row + ROWS - 1) % ROWS; SoundBank::get().play("ui_hover", 0.5f); break;
            case S::Down: case S::S: case S::Tab: m_row = (m_row + 1) % ROWS; SoundBank::get().play("ui_hover", 0.5f); break;
            case S::Left: case S::A:
                if(m_row < 4) set_level(m_row, SoundBank::get().volume(static_cast<SoundBank::Bus>(m_row)) - 0.05f);
                else if(m_row == 4) step_display(-1, manager);
                else if(m_row == 5) set_low_effects(false, manager);
                else m_row = 6;
                break;
            case S::Right: case S::D:
                if(m_row < 4) set_level(m_row, SoundBank::get().volume(static_cast<SoundBank::Bus>(m_row)) + 0.05f);
                else if(m_row == 4) step_display(1, manager);
                else if(m_row == 5) set_low_effects(true, manager);
                else m_row = 7;
                break;
            case S::Enter: case S::Space: activate(m_row, manager); break;
            default: break;
        }
        return true;
    }
    if(const auto* press = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if(press->button != sf::Mouse::Button::Left) return true;
        for(int i = 0; i < 4; ++i)
        {
            sf::FloatRect const bar = sound_bar(i);
            if(sf::FloatRect({bar.position.x - 8.0f, bar.position.y - 10.0f}, {bar.size.x + 16.0f, bar.size.y + 20.0f}).contains(mouse))
            {
                m_row = i;
                m_drag = i;
                set_level(i, (mouse.x - bar.position.x) / bar.size.x);
                return true;
            }
        }
        int const n = display_choices();
        for(int i = 0; i < n; ++i)
            if(display_button(i, n).contains(mouse)) { m_row = 4; set_display_choice(i, manager); return true; }
        for(int i = 0; i < 2; ++i)
            if(effects_button(i).contains(mouse)) { m_row = 5; set_low_effects(i == 1, manager); return true; }
        for(int r = 6; r < ROWS; ++r)
            if(settings_row(r).contains(mouse)) { m_row = r; activate(r, manager); return true; }
        if(!SETTINGS_BOX.contains(mouse)) close();
        return true;
    }
    if(event.is<sf::Event::MouseButtonReleased>()) { m_drag = -1; return true; }
    if(event.is<sf::Event::MouseMoved>())
    {
        if(m_drag >= 0)
        {
            sf::FloatRect const bar = sound_bar(m_drag);
            set_level(m_drag, (mouse.x - bar.position.x) / bar.size.x);
        }
        int h = -1;
        for(int r = 0; r < ROWS; ++r) if(settings_row(r).contains(mouse)) h = r;
        if(h != m_hover && h >= 6) SoundBank::get().play("ui_hover", 0.4f);
        m_hover = h;
        return true;
    }
    return true;
}

void Panels::render(sf::RenderTarget& t, SceneManager& manager)
{
    if(m_kind == Kind::NONE) return;
    const sf::Font* f = Assets::font(FONT);
    if(!f) return;
    t.setView(t.getDefaultView());
    box(t, {0, 0}, {W, H}, with_alpha(INK, 0.82f));

    if(m_kind == Kind::SETTINGS)
    {
        const sf::FloatRect& b = SETTINGS_BOX;
        box(t, b.position, b.size, with_alpha(lerp(INK, MUD_DARK, 0.5f), 0.98f), with_alpha(TEXT_FAINT, 0.9f));
        box(t, b.position, {b.size.x, 2}, MON_GOLD);
        shadowed(t, *f, tr("settings.title"), 32, {b.position.x + b.size.x * 0.5f, b.position.y + 14}, PAPER, 0.5f);

        auto& bank = SoundBank::get();
        t.draw(text(*f, tr("settings.sound"), 16, {270, 128}, TEXT_FAINT));
        for(int i = 0; i < 4; ++i)
        {
            sf::FloatRect const bar = sound_bar(i);
            float const v = bank.volume(static_cast<SoundBank::Bus>(i));
            bool const on = i == m_row;
            t.draw(text(*f, item("settings.buses", static_cast<std::size_t>(i)), 16, {bar.position.x - 20.0f, bar.position.y - 5.0f}, on ? MON_GOLD : TEXT_DIM, 1.0f));
            box(t, bar.position, bar.size, with_alpha(PAPER_DK, 0.25f));
            box(t, bar.position, {std::round(bar.size.x * v), bar.size.y}, on ? MON_GOLD : MON_WHITE);
            box(t, {bar.position.x + std::round(bar.size.x * v) - 2.0f, bar.position.y - 4.0f}, {4.0f, bar.size.y + 8.0f}, on ? MON_GOLD : MON_WHITE);
            t.draw(text(*f, std::to_string(static_cast<int>(std::round(v * 100.0f))), 16, {bar.position.x + bar.size.x + 14.0f, bar.position.y - 5.0f}, TEXT_FAINT));
        }

        t.draw(text(*f, tr("settings.display"), 16, {270, 290}, m_row == 4 ? MON_GOLD : TEXT_FAINT));
        int const n = display_choices();
        int const chosen = display_choice(manager);
        for(int i = 0; i < n; ++i)
        {
            sf::FloatRect const r = display_button(i, n);
            bool const on = i == chosen;
            std::string label = i == n - 1 ? tr("settings.fullscreen") : tr("settings.window", {{"width", std::to_string(960 * (i + 1))}, {"height", std::to_string(540 * (i + 1))}});
            box(t, r.position, r.size, on ? with_alpha(MON_BLUE_DK, 0.9f) : sf::Color::Transparent, on ? MON_GOLD : with_alpha(TEXT_FAINT, 0.6f));
            t.draw(text(*f, fit(*f, label, 16, r.size.x - 6.0f), 16, {r.position.x + r.size.x * 0.5f, r.position.y + 4}, on ? MON_GOLD : TEXT_DIM, 0.5f));
        }
        t.draw(text(*f, tr("settings.f11"), 16, {480, 344}, TEXT_FAINT, 0.5f));

        t.draw(text(*f, tr("settings.effects"), 16, {270, 370}, m_row == 5 ? MON_GOLD : TEXT_FAINT));
        for(int i = 0; i < 2; ++i)
        {
            sf::FloatRect const r = effects_button(i);
            bool const on = (i == 1) == Settings::get().low_effects;
            box(t, r.position, r.size, on ? with_alpha(MON_BLUE_DK, 0.9f) : sf::Color::Transparent, on ? MON_GOLD : with_alpha(TEXT_FAINT, 0.6f));
            t.draw(text(*f, tr(i == 0 ? "settings.effects_full" : "settings.effects_low"), 16, {r.position.x + r.size.x * 0.5f, r.position.y + 3}, on ? MON_GOLD : TEXT_DIM, 0.5f));
        }

        std::string const labels[] = {tr("settings.how_to"), tr("settings.done")};
        for(int r = 6; r < ROWS; ++r)
        {
            sf::FloatRect const rr = settings_row(r);
            bool const on = r == m_row || r == m_hover;
            box(t, rr.position, rr.size, on ? with_alpha(MON_BLUE_DK, 0.9f) : with_alpha(INK, 0.5f), on ? MON_GOLD : with_alpha(TEXT_FAINT, 0.7f));
            t.draw(text(*f, labels[r - 6], 16, {rr.position.x + rr.size.x * 0.5f, rr.position.y + 6}, on ? MON_GOLD : TEXT_DIM, 0.5f));
        }
        t.draw(text(*f, tr("settings.keys"), 16, {480, 482}, TEXT_FAINT, 0.5f));
        return;
    }

    if(m_kind == Kind::HOW_TO)
    {
        sf::Vector2f const p{60, 34};
        sf::Vector2f const s{840, 472};
        box(t, p, s, with_alpha(lerp(INK, MUD_DARK, 0.5f), 0.98f), with_alpha(TEXT_FAINT, 0.9f));
        box(t, p, {s.x, 2}, MON_GOLD);
        shadowed(t, *f, tr("how_to.title"), 32, {W * 0.5f, p.y + 12}, PAPER, 0.5f);

        // The idea, on the left.
        float y = p.y + 66.0f;
        for(const std::string& para : list("how_to.idea"))
        {
            for(const auto& line : wrap(*f, para, 16, 380.0f))
            {
                t.draw(text(*f, line, 16, {p.x + 24, y}, TEXT));
                y += 17.0f;
            }
            y += 8.0f;
        }

        float ky = p.y + 66.0f;
        float const kx = p.x + 560.0f;
        for(const auto& [keys, what] : pairs("controls.how_to"))
        {
            t.draw(text(*f, keys, 16, {kx - 12.0f, ky}, MON_GOLD, 1.0f));
            t.draw(text(*f, what, 16, {kx + 10.0f, ky}, TEXT_DIM));
            ky += 21.0f;
        }
        if(m_time > 0.3f) t.draw(text(*f, tr("how_to.go_on"), 16, {W * 0.5f, p.y + s.y - 26}, TEXT_FAINT, 0.5f));
        return;
    }

    // The credits.
    sf::Vector2f const p{220, 90};
    sf::Vector2f const s{520, 360};
    box(t, p, s, with_alpha(lerp(INK, MUD_DARK, 0.5f), 0.98f), with_alpha(TEXT_FAINT, 0.9f));
    box(t, p, {s.x, 2}, MON_GOLD);
    shadowed(t, *f, tr("credits.title"), 32, {W * 0.5f, p.y + 18}, PAPER, 0.5f);
    t.draw(text(*f, tr("credits.subtitle"), 16, {W * 0.5f, p.y + 58}, TEXT_DIM, 0.5f));
    float y = p.y + 100.0f;
    for(const auto& [what, who] : pairs("credits.lines"))
    {
        if(!who.empty())
        {
            t.draw(text(*f, what, 16, {W * 0.5f - 10.0f, y}, TEXT_FAINT, 1.0f));
            t.draw(text(*f, who, 16, {W * 0.5f + 10.0f, y}, TEXT));
        }
        else if(!what.empty()) t.draw(text(*f, what, 16, {W * 0.5f, y}, TEXT_DIM, 0.5f));
        y += 22.0f;
    }
    if(m_time > 0.3f) t.draw(text(*f, tr("credits.back"), 16, {W * 0.5f, p.y + s.y - 30}, TEXT_FAINT, 0.5f));
}

// ================================================================ the title

namespace
{
    const char* const TITLE_ITEMS[] = {"title.menu.play", "title.menu.how_to", "title.menu.settings", "title.menu.credits", "title.menu.quit"};
    constexpr int TITLE_COUNT = 5;
    constexpr float MENU_Y = 300.0f;
    constexpr float MENU_STEP = 30.0f;
    sf::FloatRect title_item(int i) { return {{W * 0.5f - 110.0f, MENU_Y + MENU_STEP * static_cast<float>(i) - 4.0f}, {220.0f, 26.0f}}; }

}

sf::Color TitleScene::get_clear_color() const { return INK; }

void TitleScene::on_enter()
{
    SoundBank::get().set_calm(true);
    m_leaving = false;
    m_manager->set_bloom_enabled(false);
    SoundBank& s = SoundBank::get();
    s.stop_loops();
    s.set_combat_track(SoundBank::Track::NONE);
    s.start_music();
    s.set_intensity(0.0f);
    s.set_front(0.4f);
}

void TitleScene::pick(int item)
{
    if(m_leaving) return;
    switch(item)
    {
        case 0:
            m_leaving = true;
            SoundBank::get().play("ui_select", 0.7f);
            m_manager->request_scene_switch("briefing", SceneTransitionType::FADE);
            break;
        case 1: m_panels.open(Panels::Kind::HOW_TO); break;
        case 2: m_panels.open(Panels::Kind::SETTINGS); break;
        case 3: m_panels.open(Panels::Kind::CREDITS); break;
        default: EventBus::publish(Event(EventType::QUIT)); break;
    }
}

void TitleScene::handle_event(const sf::Event& event, sf::Vector2f mouse_pos)
{
    pad::notice(event);
    if(pad::missing()) return;
    // B on the title never quits the game: on a console that's the way out of everything.
    if(auto const b = pad::pressed(event); b && *b == pad::Button::B && !m_panels.is_open()) return;
    if(auto const k = pad::menu_key(event)) { handle_event(pad::key_event(*k), mouse_pos); return; }
    if(global_key(event, *m_manager)) return;
    if(m_panels.handle_event(event, mouse_pos, *m_manager)) return;
    using S = sf::Keyboard::Scancode;
    if(const auto* key = event.getIf<sf::Event::KeyPressed>())
    {
        switch(key->scancode)
        {
            case S::Up: case S::W: m_choice = (m_choice + TITLE_COUNT - 1) % TITLE_COUNT; SoundBank::get().play("ui_hover", 0.5f); break;
            case S::Down: case S::S: m_choice = (m_choice + 1) % TITLE_COUNT; SoundBank::get().play("ui_hover", 0.5f); break;
            case S::Enter: case S::Space: pick(m_choice); break;
            case S::Escape: EventBus::publish(Event(EventType::QUIT)); break;
            case S::M: SoundBank::get().toggle_music_mute(); break;
            default: break;
        }
    }
    else if(event.is<sf::Event::MouseMoved>())
    {
        int h = -1;
        for(int i = 0; i < TITLE_COUNT; ++i) if(title_item(i).contains(mouse_pos)) h = i;
        if(h >= 0 && h != m_hover) { SoundBank::get().play("ui_hover", 0.4f); m_choice = h; }
        m_hover = h;
    }
    else if(const auto* press = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if(press->button != sf::Mouse::Button::Left) return;
        for(int i = 0; i < TITLE_COUNT; ++i) if(title_item(i).contains(mouse_pos)) { m_choice = i; pick(i); }
    }
}

void TitleScene::update_realtime(sf::Time delta_time)
{
    float const dt = delta_time.asSeconds();
    float const before = m_time;
    m_time += dt;
    m_panels.update(dt);
    // Now and then a gun, far off.
    if(std::floor(before / 7.0f) != std::floor(m_time / 7.0f))
        SoundBank::get().play("explosion_far", 0.18f, 0.1f);
    SoundBank::get().update(dt);
}

void TitleScene::render(sf::RenderTarget& t)
{
    const sf::Font* f = Assets::font(FONT);
    if(!f) return;
    PadGateOnTop const gate{t};
    t.setView(t.getDefaultView());

    // --- a dusk sky in bands, and the guns flashing along the horizon ---------------------------
    float const horizon = 268.0f;
    sf::VertexArray va(sf::PrimitiveType::Triangles);
    sf::Color const top = lerp(INK, MON_BLUE_DK, 0.18f), low = lerp(lerp(INK, FIRE_RED, 0.35f), SMOKE, 0.3f);
    for(int k = 0; k < 14; ++k)
    {
        float const y0 = horizon * static_cast<float>(k) / 14.0f;
        draw::rect(va, 0, y0, W, horizon / 14.0f + 1.0f, lerp(top, low, std::pow(static_cast<float>(k) / 13.0f, 1.6f)));
    }
    // The flashes: a few places along the line, each on its own beat.
    for(int g = 0; g < 5; ++g)
    {
        float const period = 3.0f + 2.3f * hash(static_cast<float>(g) + 1.0f);
        float const phase = std::fmod(m_time + 10.0f * hash(static_cast<float>(g) + 9.0f), period);
        float const glow = std::max(0.0f, 1.0f - phase / 0.6f);
        if(glow <= 0.0f) continue;
        float const x = W * (0.08f + 0.84f * hash(static_cast<float>(g) * 3.1f + std::floor((m_time + 10.0f * hash(static_cast<float>(g) + 9.0f)) / period)));
        for(int b = 0; b < 6; ++b)
        {
            float const w = 60.0f + 50.0f * static_cast<float>(b);
            float const h = 10.0f + 12.0f * static_cast<float>(b);
            draw::rect(va, std::floor(x - w * 0.5f), horizon - h, w, h, with_alpha(FIRE_ORANGE, 0.10f * glow * (1.0f - static_cast<float>(b) / 6.0f)));
        }
    }
    // The far line: broken trees and a ruined spire.
    sf::Color const far = lerp(low, INK, 0.62f);
    for(float x = 0; x < W; x += 4.0f)
    {
        float const h = 6.0f + 12.0f * hash(std::floor(x / 4.0f) * 0.37f) * hash(std::floor(x / 28.0f));
        draw::rect(va, x, horizon - h, 4, h + 1, far);
    }
    draw::rect(va, W * 0.81f, horizon - 58, 8, 58, far);
    draw::rect(va, W * 0.81f + 2, horizon - 72, 4, 14, far);
    for(float x = 40; x < W; x += 110.0f)
    {
        float const h = 34.0f + hash(x) * 20.0f;
        draw::rect(va, x, horizon - h, 3, h, far);
        draw::rect(va, x - 6 + hash(x + 1.0f) * 4.0f, horizon - h * 0.7f, 6, 2, far);
    }
    // Smoke drifting along it.
    for(int k = 0; k < 18; ++k)
    {
        float const u = std::fmod(m_time * 0.012f + hash(static_cast<float>(k) * 1.7f), 1.0f);
        float const x = W * (1.1f - 1.2f * u);
        float const y = horizon - 30.0f - 40.0f * hash(static_cast<float>(k) * 2.9f);
        draw::rect(va, std::floor(x), std::floor(y), 60.0f + 40.0f * hash(static_cast<float>(k)), 10.0f + 8.0f * hash(static_cast<float>(k) + 3.0f),
                   with_alpha(SMOKE, 0.10f * std::sin(u * PI)));
    }
    // The ground: dark, a few ruts catching the light.
    sf::Color const ground = lerp(MUD_DARK, INK, 0.55f);
    draw::rect(va, 0, horizon, W, H - horizon, ground);
    for(int k = 0; k < 90; ++k)
        draw::rect(va, std::floor(hash(static_cast<float>(k) * 1.7f) * W), std::floor(horizon + 4 + hash(static_cast<float>(k) * 2.3f) * (H - horizon - 8)),
                   3.0f + 6.0f * hash(static_cast<float>(k) + 0.5f), 1, lerp(ground, low, 0.25f));
    t.draw(va);

    // --- the name ---------------------------------------------------------------------------------
    float const in = std::clamp(m_time / 1.2f, 0.0f, 1.0f);
    shadowed(t, *f, tr("game.title"), 64, {W * 0.5f, 84}, with_alpha(PAPER, in), 0.5f);
    box(t, {W * 0.5f - 120.0f, 168}, {240, 2}, with_alpha(MON_GOLD, in));
    shadowed(t, *f, tr("game.subtitle"), 16, {W * 0.5f, 182}, with_alpha(TEXT_DIM, in), 0.5f);

    // --- the menu -----------------------------------------------------------------------------------
    for(int i = 0; i < TITLE_COUNT; ++i)
    {
        sf::FloatRect const r = title_item(i);
        bool const on = i == m_choice;
        if(on)
        {
            box(t, r.position, r.size, with_alpha(MON_BLUE_DK, 0.55f));
            box(t, r.position, {3, r.size.y}, MON_GOLD);
            box(t, {r.position.x + r.size.x - 3.0f, r.position.y}, {3, r.size.y}, MON_GOLD);
        }
        shadowed(t, *f, tr(TITLE_ITEMS[i]), 16, {W * 0.5f, r.position.y + 5}, on ? MON_GOLD : TEXT_DIM, 0.5f);
    }
    shadowed(t, *f, tr("title.footer"), 16, {20, H - 28}, TEXT_FAINT);
    shadowed(t, *f, tr("game.version"), 16, {W - 20, H - 28}, TEXT_FAINT, 1.0f);

    m_panels.render(t, *m_manager);
}

// ================================================================ the briefing

namespace
{
    // The left page.
    constexpr float LX = 28.0f;
    constexpr float BATTLE_Y = 70.0f;
    constexpr float SIDE_Y = 160.0f;
    constexpr float DIFF_Y = 214.0f;
    constexpr float ROLE_Y = 268.0f;
    constexpr float ROLE_H = 19.0f;
    constexpr float CARD_Y = 392.0f;
    sf::FloatRect battle_row(int i) { return {{LX - 6.0f, BATTLE_Y + 18.0f + 22.0f * static_cast<float>(i)}, {324.0f, 22.0f}}; }
    sf::FloatRect side_button(int k) { return {{LX - 2.0f + 162.0f * static_cast<float>(k), SIDE_Y + 18.0f}, {158.0f, 22.0f}}; }
    sf::FloatRect diff_button(int i) { return {{LX - 2.0f + 108.0f * static_cast<float>(i), DIFF_Y + 18.0f}, {104.0f, 22.0f}}; }
    sf::FloatRect role_row(int i)
    {
        int const col = i < 5 ? 0 : 1;
        int const row = i < 5 ? i : i - 5;
        return {{LX - 6.0f + 164.0f * static_cast<float>(col), ROLE_Y + 18.0f + ROLE_H * static_cast<float>(row)}, {160.0f, ROLE_H}};
    }

    // The right page: the order, on paper.
    const sf::FloatRect PAPER_BOX({370, 14}, {572, 470});
    const sf::FloatRect MAP_BOX({384, 48}, {544, 226});
    constexpr float TEXT_Y = 286.0f;

    // The buttons along the bottom: 0 go, 1 back, 2 settings, 3 how to play.
    const sf::FloatRect BUTTONS[] = {
        sf::FloatRect({752, 494}, {190, 36}),
        sf::FloatRect({22, 500}, {96, 26}),
        sf::FloatRect({124, 500}, {110, 26}),
        sf::FloatRect({240, 500}, {116, 26}),
    };
    const char* const BUTTON_LABELS[] = {"", "briefing.back", "briefing.settings", "briefing.how_to"};
}

sf::Color BriefingScene::get_clear_color() const { return INK; }

const sf::RenderTexture* BriefingScene::sheet(LevelId level)
{
    auto const i = static_cast<std::size_t>(level);
    if(!m_sheets[i])
    {
        Map map;
        map.build(level);
        m_world[i] = map.size;
        float const k = std::min(MAP_BOX.size.x / map.size.x, MAP_BOX.size.y / map.size.y);
        auto rt = std::make_unique<sf::RenderTexture>();
        if(!rt->resize({static_cast<unsigned>(std::round(map.size.x * k)), static_cast<unsigned>(std::round(map.size.y * k))})) return nullptr;
        draw_sheet(map, *rt);
        m_sheets[i] = std::move(rt);
    }
    return m_sheets[i].get();
}

void BriefingScene::on_enter()
{
    SoundBank::get().set_calm(false);
    m_leaving = false;
    m_manager->set_bloom_enabled(false);
    for(int i = 0; i < CARD_COUNT; ++i)
        if(CARDS[i] == Campaign::current().chosen) m_choice = i;
    m_changed = 0.0f;
    SoundBank& s = SoundBank::get();
    s.stop_loops();
    s.set_combat_track(SoundBank::Track::NONE);
    s.start_music();
    s.set_intensity(0.0f);
    s.set_front(0.35f);
    // The first time anyone plays: how it works, before anything else.
    if(!Settings::get().seen_how_to) m_panels.open(Panels::Kind::HOW_TO);
}

int BriefingScene::role_at(sf::Vector2f p) const
{
    for(int i = 0; i < CARD_COUNT; ++i) if(role_row(i).contains(p)) return i;
    return -1;
}

int BriefingScene::button_at(sf::Vector2f p) const
{
    for(int i = 0; i < 4; ++i) if(BUTTONS[i].contains(p)) return i;
    return -1;
}

void BriefingScene::press(int button)
{
    switch(button)
    {
        case 0: begin(); break;
        case 1:
            if(m_leaving) return;
            m_leaving = true;
            SoundBank::get().play("ui_select", 0.6f);
            m_manager->request_scene_switch("title", SceneTransitionType::FADE);
            break;
        case 2: m_panels.open(Panels::Kind::SETTINGS); break;
        case 3: m_panels.open(Panels::Kind::HOW_TO); break;
        default: break;
    }
}

void BriefingScene::choose(int index)
{
    if(index < 0 || index >= CARD_COUNT || index == m_choice) return;
    m_choice = index;
    SoundBank::get().play("ui_select", 0.7f);
}

void BriefingScene::set_side(Team side)
{
    Campaign& c = Campaign::current();
    if(c.side == side) return;
    c.side = side;
    m_changed = 0.0f;
    SoundBank::get().play("paper", 0.5f);
}

void BriefingScene::set_level(int index)
{
    Campaign& c = Campaign::current();
    LevelId const want = PLACES[(index + PLACE_COUNT) % PLACE_COUNT];
    if(want == c.level) return;
    c.level = want;
    m_changed = 0.0f;
    SoundBank::get().play("paper", 0.6f);
}

void BriefingScene::set_difficulty(int d)
{
    Campaign& c = Campaign::current();
    d = (d + 3) % 3;
    if(c.difficulty == d) return;
    c.difficulty = d;
    SoundBank::get().play("ui_select", 0.7f);
}

void BriefingScene::begin()
{
    if(m_leaving) return;
    m_leaving = true;
    Campaign::current().chosen = CARDS[m_choice];
    SoundBank::get().play("whistle", 0.4f);
    m_manager->request_scene_switch("battle", SceneTransitionType::FADE);
}

void BriefingScene::handle_event(const sf::Event& event, sf::Vector2f mouse_pos)
{
    pad::notice(event);
    if(pad::missing()) return;
    using P = pad::Button;
    if(auto const b = pad::pressed(event); b && !m_panels.is_open())
    {
        // X the side, Y the difficulty, BACK the settings, LB and RB the battle.
        auto const k = *b == P::X ? sf::Keyboard::Scancode::T : *b == P::Y ? sf::Keyboard::Scancode::K
                     : *b == P::BACK ? sf::Keyboard::Scancode::O : *b == P::LB ? sf::Keyboard::Scancode::Left
                     : *b == P::RB ? sf::Keyboard::Scancode::Right : sf::Keyboard::Scancode::Unknown;
        if(k != sf::Keyboard::Scancode::Unknown) { handle_event(pad::key_event(k), mouse_pos); return; }
    }
    if(auto const k = pad::menu_key(event)) { handle_event(pad::key_event(*k), mouse_pos); return; }
    if(global_key(event, *m_manager)) return;
    if(m_panels.handle_event(event, mouse_pos, *m_manager)) return;
    using S = sf::Keyboard::Scancode;
    Campaign& c = Campaign::current();
    int const level = static_cast<int>(c.level);
    if(const auto* key = event.getIf<sf::Event::KeyPressed>())
    {
        switch(key->scancode)
        {
            case S::Num1: choose(0); break;
            case S::Num2: choose(1); break;
            case S::Num3: choose(2); break;
            case S::Num4: choose(3); break;
            case S::Num5: choose(4); break;
            case S::Num6: choose(5); break;
            case S::Num7: choose(6); break;
            case S::Num8: choose(7); break;
            case S::Num9: choose(8); break;
            case S::Up: case S::W: choose((m_choice + CARD_COUNT - 1) % CARD_COUNT); break;
            case S::Down: case S::S: choose((m_choice + 1) % CARD_COUNT); break;
            case S::Right: case S::D: set_level(level + 1); break;
            case S::Left: case S::A: set_level(level - 1); break;
            case S::T: set_side(c.side == Team::MONTERAN ? Team::RECHIVII : Team::MONTERAN); break;
            case S::K: set_difficulty(c.difficulty + 1); break;
            case S::O: press(2); break;
            case S::F1: case S::H: press(3); break;
            case S::Enter: case S::Space: begin(); break;
            case S::M: SoundBank::get().toggle_music_mute(); break;
            case S::Escape: press(1); break;
            default: break;
        }
    }
    else if(event.is<sf::Event::MouseMoved>())
    {
        int const r = role_at(mouse_pos);
        int const b = button_at(mouse_pos);
        if((r != m_hover_role && r >= 0) || (b != m_hover_button && b >= 0)) SoundBank::get().play("ui_hover", 0.4f);
        m_hover_role = r;
        m_hover_button = b;
    }
    else if(const auto* press_event = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if(press_event->button != sf::Mouse::Button::Left) return;
        for(int i = 0; i < PLACE_COUNT; ++i) if(battle_row(i).contains(mouse_pos)) { set_level(i); return; }
        for(int k = 0; k < 2; ++k) if(side_button(k).contains(mouse_pos)) { set_side(k == 0 ? Team::MONTERAN : Team::RECHIVII); return; }
        for(int i = 0; i < 3; ++i) if(diff_button(i).contains(mouse_pos)) { set_difficulty(i); return; }
        int const r = role_at(mouse_pos);
        if(r >= 0) { choose(r); return; }
        int const b = button_at(mouse_pos);
        if(b >= 0) press(b);
    }
}

void BriefingScene::update_realtime(sf::Time delta_time)
{
    float const dt = delta_time.asSeconds();
    m_time += dt;
    m_changed += dt;
    m_panels.update(dt);
    SoundBank::get().update(dt);
}

void BriefingScene::render(sf::RenderTarget& t)
{
    const sf::Font* f = Assets::font(FONT);
    if(!f) return;
    PadGateOnTop const gate{t};
    t.setView(t.getDefaultView());
    const Campaign& c = Campaign::current();
    std::string const card = std::string("roles.") + role_key(CARDS[m_choice]);
    bool const mon = c.side == Team::MONTERAN;

    // ================================ the left page: where, which side, how hard, who
    shadowed(t, *f, tr("game.title"), 32, {LX - 2, 16}, PAPER);

    auto label = [&](const std::string& s, float y) { t.draw(text(*f, s, 16, {LX, y}, TEXT_FAINT)); };
    auto option = [&](sf::FloatRect r, const std::string& s, bool on, sf::Color fill)
    {
        if(on) box(t, r.position, r.size, with_alpha(fill, 0.9f), MON_GOLD);
        else box(t, r.position, r.size, sf::Color::Transparent, with_alpha(TEXT_FAINT, 0.5f));
        t.draw(text(*f, s, 16, {r.position.x + r.size.x * 0.5f, r.position.y + 3}, on ? MON_GOLD : TEXT_DIM, 0.5f));
    };

    label(tr("briefing.battle"), BATTLE_Y);
    for(int i = 0; i < PLACE_COUNT; ++i)
    {
        sf::FloatRect const r = battle_row(i);
        bool const on = PLACES[i] == c.level;
        if(on)
        {
            box(t, r.position, r.size, with_alpha(MON_BLUE_DK, 0.55f));
            box(t, r.position, {3, r.size.y}, MON_GOLD);
        }
        t.draw(text(*f, level_name(PLACES[i]), 16, {LX + 4, r.position.y + 3}, on ? MON_GOLD : TEXT_DIM));
        t.draw(text(*f, level_text(PLACES[i], "when"), 16, {r.position.x + r.size.x - 8, r.position.y + 3}, TEXT_FAINT, 1.0f));
    }

    label(tr("briefing.side"), SIDE_Y);
    option(side_button(0), tr("sides.monteran"), mon, MON_BLUE_DK);
    option(side_button(1), tr("sides.rechivii"), !mon, RCH_TAN_DK);

    label(tr("briefing.difficulty"), DIFF_Y);
    for(int i = 0; i < 3; ++i) option(diff_button(i), difficulty_name(i), c.difficulty == i, MON_BLUE_DK);

    label(tr("briefing.go_in_as"), ROLE_Y);
    for(int i = 0; i < CARD_COUNT; ++i)
    {
        sf::FloatRect const r = role_row(i);
        bool const on = i == m_choice;
        bool const hover = i == m_hover_role;
        if(on)
        {
            box(t, r.position, r.size, with_alpha(MON_BLUE_DK, 0.55f));
            box(t, r.position, {3, r.size.y}, MON_GOLD);
        }
        sf::VertexArray mark(sf::PrimitiveType::Triangles);
        icon(mark, role_icon(CARDS[i]), {r.position.x + 8, r.position.y + 2}, on ? MON_GOLD : (hover ? TEXT : TEXT_DIM));
        t.draw(mark);
        t.draw(text(*f, role_name(CARDS[i]), 16, {r.position.x + 30, r.position.y + 1}, on ? MON_GOLD : (hover ? TEXT : TEXT_DIM)));
        t.draw(text(*f, std::to_string(i + 1), 16, {r.position.x + r.size.x - 6, r.position.y + 1}, TEXT_FAINT, 1.0f));
    }
    t.draw(text(*f, tr(card + ".tag"), 16, {LX, CARD_Y}, MON_GOLD));
    const auto& card_lines = list(card + ".lines");
    for(std::size_t k = 0; k < card_lines.size(); ++k)
        t.draw(text(*f, card_lines[k], 16, {LX, CARD_Y + 20.0f + 17.0f * static_cast<float>(k)}, TEXT_DIM));

    for(int i = 1; i < 4; ++i)
    {
        sf::FloatRect const r = BUTTONS[i];
        bool const hover = i == m_hover_button;
        box(t, r.position, r.size, hover ? with_alpha(MON_BLUE_DK, 0.8f) : with_alpha(INK, 0.4f), hover ? MON_GOLD : with_alpha(TEXT_FAINT, 0.6f));
        t.draw(text(*f, tr(BUTTON_LABELS[i]), 16, {r.position.x + r.size.x * 0.5f, r.position.y + 5}, hover ? MON_GOLD : TEXT_DIM, 0.5f));
    }

    // ================================ the right page: the order, on paper
    box(t, PAPER_BOX.position + sf::Vector2f{3, 3}, PAPER_BOX.size, with_alpha(sf::Color::Black, 0.5f));
    box(t, PAPER_BOX.position, PAPER_BOX.size, SHEET);
    // The paper's grain.
    {
        sf::VertexArray grain(sf::PrimitiveType::Triangles);
        for(int k = 0; k < 900; ++k)
            draw::rect(grain, std::floor(PAPER_BOX.position.x + 2 + hash(static_cast<float>(k) * 1.3f) * (PAPER_BOX.size.x - 4)),
                       std::floor(PAPER_BOX.position.y + 2 + hash(static_cast<float>(k) * 2.7f) * (PAPER_BOX.size.y - 4)), 1, 1, with_alpha(SHEET_DK, 0.7f));
        t.draw(grain);
    }
    float const px = PAPER_BOX.position.x;
    t.draw(text(*f, tr(std::string("briefing.unit.") + side_key(c.side)), 16, {px + 14, 24}, PEN));
    t.draw(text(*f, level_text(c.level, "dated"), 16, {px + PAPER_BOX.size.x - 14, 24}, PEN, 1.0f));

    // The map.
    if(const sf::RenderTexture* rt = sheet(c.level))
    {
        auto const li = static_cast<std::size_t>(c.level);
        sf::Vector2f const size(rt->getSize());
        sf::Vector2f const origin = MAP_BOX.position + (MAP_BOX.size - size) * 0.5f;
        sf::Sprite ground(rt->getTexture());
        ground.setPosition({std::round(origin.x), std::round(origin.y)});
        t.draw(ground);
        // Its frame, and its name in the corner.
        sf::VertexArray frame(sf::PrimitiveType::Triangles);
        draw::rect(frame, origin.x - 2, origin.y - 2, size.x + 4, 1.5f, PEN);
        draw::rect(frame, origin.x - 2, origin.y + size.y, size.x + 4, 1.5f, PEN);
        draw::rect(frame, origin.x - 2, origin.y - 2, 1.5f, size.y + 4, PEN);
        draw::rect(frame, origin.x + size.x, origin.y - 2, 1.5f, size.y + 4, PEN);

        // What's on it, drawn in as it comes up.
        float const k = size.x / m_world[li].x;
        auto at = [&](sf::Vector2f p) { return origin + p * k; };
        static Map forest_road;             // the road's geometry, for the marks that follow it
        static bool forest_ready = false;
        if(!forest_ready) { forest_road.build(LevelId::FOREST); forest_ready = true; }
        std::vector<Mark> const ms = marks(c.level, c.side, forest_road);
        sf::VertexArray ink(sf::PrimitiveType::Triangles);
        std::vector<sf::Text> labels;
        int n = 0;
        for(const auto& m : ms)
        {
            float const show = std::clamp((m_changed - 0.12f * static_cast<float>(n++)) / 0.25f, 0.0f, 1.0f);
            if(show <= 0.0f) continue;
            sf::Color const pen = with_alpha(PEN, show);
            bool const ours = m.team == c.side;
            switch(m.kind)
            {
                case 'U':
                    unit_symbol(ink, at(m.pts[0]), ours, pen);
                    if(!m.text.empty()) labels.push_back(text(*f, m.text, 16, at(m.pts[0]) + sf::Vector2f{12, -9}, pen));
                    break;
                case 'G': gun_symbol(ink, at(m.pts[0]), ours, pen); break;
                case 'W':
                {
                    sf::Vector2f const d = m.pts[1];
                    draw::rotated_rect(ink, at(m.pts[0]), 7.0f, 4.0f, std::atan2(d.y, d.x), pen);
                    break;
                }
                case 'M':
                {
                    sf::Vector2f const p = at(m.pts[0]);
                    draw::thick_line(ink, p + sf::Vector2f{-4, -4}, p + sf::Vector2f{4, 4}, 2.0f, pen);
                    draw::thick_line(ink, p + sf::Vector2f{-4, 4}, p + sf::Vector2f{4, -4}, 2.0f, pen);
                    break;
                }
                case 'A':
                {
                    // Ours a firm line; theirs, what we expect of them, dashed.
                    float const reveal = std::clamp((m_changed - 0.12f * static_cast<float>(n)) / 0.8f, 0.0f, 1.0f);
                    float total = 0.0f;
                    for(std::size_t i = 0; i + 1 < m.pts.size(); ++i)
                    {
                        sf::Vector2f const d = at(m.pts[i + 1]) - at(m.pts[i]);
                        total += std::sqrt(d.x * d.x + d.y * d.y);
                    }
                    float left = total * reveal;
                    sf::Vector2f tip = at(m.pts[0]), dir{1, 0};
                    for(std::size_t i = 0; i + 1 < m.pts.size() && left > 0.0f; ++i)
                    {
                        sf::Vector2f const a = at(m.pts[i]), b = at(m.pts[i + 1]);
                        sf::Vector2f const d = b - a;
                        float const len = std::sqrt(d.x * d.x + d.y * d.y);
                        if(len < 0.1f) continue;
                        dir = d / len;
                        float const go = std::min(len, left);
                        tip = a + dir * go;
                        if(ours) draw::thick_line(ink, a, tip, 2.0f, pen);
                        else dashed(ink, a, tip, 1.5f, pen);
                        left -= go;
                    }
                    if(reveal > 0.05f) arrow_head(ink, tip + dir * 4.0f, dir, 8.0f, pen);
                    break;
                }
                case 'O':
                {
                    sf::Vector2f const p = at(m.pts[0]);
                    float const r = m.pts[1].x * k;
                    // A ring, broken into dashes.
                    int const segs = 28;
                    for(int i = 0; i < segs; i += 2)
                    {
                        float const a0 = 2.0f * PI * static_cast<float>(i) / segs, a1 = 2.0f * PI * static_cast<float>(i + 1) / segs;
                        draw::thick_line(ink, p + sf::Vector2f{std::cos(a0), std::sin(a0)} * r, p + sf::Vector2f{std::cos(a1), std::sin(a1)} * r, 2.0f, pen);
                    }
                    labels.push_back(text(*f, m.text, 16, p + sf::Vector2f{r * 0.72f + 4.0f, r * 0.72f - 4.0f}, pen));
                    break;
                }
                case 'R':
                {
                    sf::Vector2f const a = at(m.pts[0]), s = m.pts[1] * k;
                    dashed(ink, a, a + sf::Vector2f{s.x, 0}, 2.0f, pen);
                    dashed(ink, a + sf::Vector2f{0, s.y}, a + s, 2.0f, pen);
                    dashed(ink, a, a + sf::Vector2f{0, s.y}, 2.0f, pen);
                    dashed(ink, a + sf::Vector2f{s.x, 0}, a + s, 2.0f, pen);
                    labels.push_back(text(*f, m.text, 16, a + sf::Vector2f{s.x + 6.0f, -2.0f}, pen));
                    break;
                }
                case '?':
                {
                    sf::Vector2f const p = at(m.pts[0]);
                    sf::Vector2f const r = m.pts[1] * k;
                    int const segs = 36;
                    for(int i = 0; i < segs; i += 2)
                    {
                        float const a0 = 2.0f * PI * static_cast<float>(i) / segs, a1 = 2.0f * PI * static_cast<float>(i + 1) / segs;
                        draw::thick_line(ink, p + sf::Vector2f{std::cos(a0) * r.x, std::sin(a0) * r.y}, p + sf::Vector2f{std::cos(a1) * r.x, std::sin(a1) * r.y}, 1.2f, pen);
                    }
                    labels.push_back(text(*f, "?", 32, p, pen, 0.5f));
                    break;
                }
                case 'T': labels.push_back(text(*f, m.text, 16, at(m.pts[0]), pen)); break;
                default: break;
            }
        }
        t.draw(ink);
        for(auto& l : labels)
        {
            // Kept on the sheet: one that would run off the right edge goes the other side.
            sf::FloatRect const lb = l.getGlobalBounds();
            if(lb.position.x + lb.size.x > origin.x + size.x - 3.0f) l.move({-(lb.size.x + 26.0f), 0.0f});
            // A little paper behind each label, so it reads over the woods.
            sf::FloatRect const b = l.getGlobalBounds();
            box(t, b.position - sf::Vector2f{2, 1}, b.size + sf::Vector2f{4, 3}, with_alpha(SHEET, l.getFillColor().a / 255.0f * 0.85f));
            t.draw(l);
        }

        // The sheet's name, a north arrow and a scale.
        std::string const title = level_text(c.level, "sheet") + "     " + tr("briefing.scale");
        sf::Text name = text(*f, title, 16, {origin.x + 6, origin.y + size.y - 22}, PEN);
        sf::FloatRect const nb = name.getGlobalBounds();
        box(t, nb.position - sf::Vector2f{4, 2}, nb.size + sf::Vector2f{8, 5}, SHEET, PEN);
        t.draw(name);
        sf::VertexArray north(sf::PrimitiveType::Triangles);
        sf::Vector2f const np{origin.x + size.x - 16, origin.y + 26};
        draw::thick_line(north, np + sf::Vector2f{0, 10}, np + sf::Vector2f{0, -8}, 1.5f, PEN);
        arrow_head(north, np + sf::Vector2f{0, -12}, {0, -1}, 6.0f, PEN);
        float const bar = 200.0f * k;           // 200 yards
        sf::Vector2f const sp{origin.x + size.x - 20 - bar, origin.y + size.y - 12};
        draw::rect(north, sp.x, sp.y, bar, 1.5f, PEN);
        draw::rect(north, sp.x, sp.y - 3, 1.5f, 6, PEN);
        draw::rect(north, sp.x + bar - 1.5f, sp.y - 3, 1.5f, 6, PEN);
        draw::rect(north, sp.x + bar * 0.5f - 0.75f, sp.y - 2, 1.5f, 4, PEN);
        t.draw(north);
        t.draw(text(*f, tr("briefing.north"), 16, np + sf::Vector2f{0, -30}, PEN, 0.5f));
        t.draw(text(*f, tr("briefing.yards"), 16, {sp.x + bar * 0.5f, sp.y - 18}, PEN, 0.5f));
    }

    // The orders, under the map.
    {
        Orders const o = orders(c.level, c.side);
        std::string const heads[] = {tr("briefing.situation"), tr("briefing.mission"), tr("briefing.notes")};
        const std::string* const bodies[] = {&o.situation, &o.mission, &o.notes};
        float y = TEXT_Y;
        float const head_x = px + 14.0f, body_x = px + 112.0f, body_w = PAPER_BOX.size.x - 126.0f;
        for(int i = 0; i < 3; ++i)
        {
            t.draw(text(*f, heads[i], 16, {head_x, y}, PEN));
            for(const auto& line : wrap(*f, *bodies[i], 16, body_w))
            {
                t.draw(text(*f, line, 16, {body_x, y}, PEN));
                y += 16.0f;
            }
            y += 7.0f;
        }
    }

    // Go.
    {
        const sf::FloatRect& g = BUTTONS[0];
        bool const hover = m_hover_button == 0;
        float const pulse = 0.75f + 0.25f * std::sin(m_time * 3.0f);
        box(t, g.position, g.size, with_alpha(mon ? MON_BLUE_DK : RCH_TAN_DK, 0.95f), with_alpha(MON_GOLD, hover ? 1.0f : pulse));
        std::string const go = level_text(c.level, std::string("go.") + side_key(c.side));
        shadowed(t, *f, go, 16, {g.position.x + g.size.x * 0.5f, g.position.y + 10}, PAPER, 0.5f);
        t.draw(text(*f, tr("briefing.enter"), 16, {g.position.x - 10, g.position.y + 10}, TEXT_FAINT, 1.0f));
    }

    m_panels.render(t, *m_manager);
}

// ================================================================ the debrief

namespace
{
    // The left page: the numbers. The right: the paper.
    constexpr float DX = 24.0f;
    const sf::FloatRect NEWS_BOX({352, 14}, {588, 512});
    sf::FloatRect debrief_button(int i) { return {{DX - 6.0f, 410.0f + 38.0f * static_cast<float>(i)}, {306.0f, 30.0f}}; }
    const char* const DEBRIEF_BUTTONS[] = {"debrief.again", "debrief.briefing", "debrief.title"};

    std::string clock(float seconds)
    {
        int const s = static_cast<int>(seconds);
        if(s < 60) return tr("debrief.time_short", {{"seconds", std::to_string(s)}});
        return tr("debrief.time", {{"minutes", std::to_string(s / 60)}, {"seconds", std::to_string(s % 60)}});
    }
}

sf::Color DebriefScene::get_clear_color() const { return INK; }

const sf::RenderTexture* DebriefScene::sheet(LevelId level)
{
    if(m_sheet && m_sheet_level == level) return m_sheet.get();
    Map map;
    map.build(level);
    float const k = 268.0f / map.size.x;
    auto rt = std::make_unique<sf::RenderTexture>();
    if(!rt->resize({static_cast<unsigned>(std::round(map.size.x * k)), static_cast<unsigned>(std::round(map.size.y * k))})) return nullptr;
    draw_sheet(map, *rt);
    m_sheet = std::move(rt);
    m_sheet_level = level;
    return m_sheet.get();
}

void DebriefScene::on_enter()
{
    SoundBank::get().set_calm(false);
    m_time = 0.0f;
    m_choice = 0;
    m_hover = -1;
    m_leaving = false;
    m_manager->set_bloom_enabled(false);
    SoundBank& s = SoundBank::get();
    s.stop_loops();
    s.set_intensity(0.0f);
    s.set_front(0.25f);
    s.play("paper", 0.6f);
    // The headline is picked now, so it doesn't change under you.
    const Campaign& c = Campaign::current();
    std::string const key = std::string("newspaper.") + level_key(c.level) + "." + side_key(c.playing) + (c.won ? ".won" : ".lost") + ".headline";
    std::size_t const n = list(key).size();
    m_headline = n > 1 ? static_cast<std::size_t>(Random::integer(0, static_cast<int>(n) - 1)) : 0;
}

int DebriefScene::button_at(sf::Vector2f p) const
{
    for(int i = 0; i < 3; ++i) if(debrief_button(i).contains(p)) return i;
    return -1;
}

void DebriefScene::press(int button)
{
    if(m_leaving || button < 0 || button > 2) return;
    m_leaving = true;
    SoundBank::get().play(button == 0 ? "whistle" : "ui_select", button == 0 ? 0.4f : 0.6f);
    static const char* const where[] = {"battle", "briefing", "title"};
    m_manager->request_scene_switch(where[button], SceneTransitionType::FADE);
}

void DebriefScene::handle_event(const sf::Event& event, sf::Vector2f mouse_pos)
{
    pad::notice(event);
    if(pad::missing()) return;
    if(auto const k = pad::menu_key(event)) { handle_event(pad::key_event(*k), mouse_pos); return; }
    if(global_key(event, *m_manager)) return;
    if(m_time < 0.8f) return;               // a moment to see it before a stray key skips it
    using S = sf::Keyboard::Scancode;
    if(const auto* key = event.getIf<sf::Event::KeyPressed>())
    {
        switch(key->scancode)
        {
            case S::Up: case S::W: m_choice = (m_choice + 2) % 3; SoundBank::get().play("ui_hover", 0.5f); break;
            case S::Down: case S::S: case S::Tab: m_choice = (m_choice + 1) % 3; SoundBank::get().play("ui_hover", 0.5f); break;
            case S::Enter: case S::Space: press(m_choice); break;
            case S::B: press(1); break;
            case S::Escape: press(2); break;
            case S::M: SoundBank::get().toggle_music_mute(); break;
            default: break;
        }
    }
    else if(event.is<sf::Event::MouseMoved>())
    {
        int const h = button_at(mouse_pos);
        if(h >= 0 && h != m_hover) { SoundBank::get().play("ui_hover", 0.4f); m_choice = h; }
        m_hover = h;
    }
    else if(const auto* click = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if(click->button == sf::Mouse::Button::Left) press(button_at(mouse_pos));
    }
}

void DebriefScene::update_realtime(sf::Time delta_time)
{
    float const before = m_time;
    m_time += delta_time.asSeconds();
    // The paper lands on the desk.
    if(before < 0.7f && m_time >= 0.7f) SoundBank::get().play("paper", 0.7f);
    SoundBank::get().update(delta_time.asSeconds());
}

void DebriefScene::render(sf::RenderTarget& t)
{
    const sf::Font* f = Assets::font(FONT);
    if(!f) return;
    PadGateOnTop const gate{t};
    t.setView(t.getDefaultView());
    const Campaign& c = Campaign::current();
    auto fade = [this](float at, float over = 0.4f) { return std::clamp((m_time - at) / over, 0.0f, 1.0f); };
    Team const us = c.playing;
    Team const them = us == Team::MONTERAN ? Team::RECHIVII : Team::MONTERAN;

    // ================================ the left page: what it cost
    t.draw(text(*f, tr(c.won ? "debrief.won" : "debrief.lost"), 32, {DX, 16}, with_alpha(c.won ? PAPER : ALERT, fade(0.0f))));
    t.draw(text(*f, level_name(c.level), 16, {DX, 58}, with_alpha(TEXT, fade(0.15f))));
    t.draw(text(*f, difficulty_name(c.difficulty), 16, {DX, 76}, with_alpha(TEXT_DIM, fade(0.2f))));
    t.draw(text(*f, c.time > 0.5f ? tr("debrief.duration", {{"time", clock(c.time)}}) : tr("debrief.no_fight"), 16, {DX, 94}, with_alpha(TEXT_DIM, fade(0.25f))));

    // The losses: a row for each way a man is lost, a column for each side.
    float const top = 136.0f;
    float const col[2] = {DX + 196.0f, DX + 292.0f};
    t.draw(text(*f, tr("debrief.losses"), 16, {DX, top}, with_alpha(TEXT_FAINT, fade(0.35f))));
    t.draw(text(*f, tr(std::string("sides.") + side_key(us)), 16, {col[0], top}, with_alpha(MON_GOLD, fade(0.35f)), 1.0f));
    t.draw(text(*f, tr(std::string("sides.") + side_key(them)), 16, {col[1], top}, with_alpha(TEXT_DIM, fade(0.35f)), 1.0f));
    box(t, {DX, top + 21}, {col[1] - DX, 1}, with_alpha(TEXT_FAINT, 0.6f * fade(0.35f)));
    const Losses* sides[2] = {&c.ours, &c.theirs};
    for(int row = 0; row < 4; ++row)
    {
        float const y = top + 30.0f + 24.0f * static_cast<float>(row);
        float const a = fade(0.5f + 0.15f * static_cast<float>(row));
        t.draw(text(*f, item("debrief.columns", static_cast<std::size_t>(row + 1)), 16, {DX, y}, with_alpha(TEXT_DIM, a)));
        for(int k = 0; k < 2; ++k)
        {
            const Losses& l = *sides[k];
            int const n = row == 0 ? l.killed : (row == 1 ? l.wounded : (row == 2 ? l.captured : l.fled));
            // The numbers count up as they come in.
            int const shown = static_cast<int>(std::round(static_cast<float>(n) * fade(0.5f + 0.15f * static_cast<float>(row), 0.7f)));
            sf::Color const ink = row == 0 && n > 0 ? lerp(ALERT, TEXT, 0.35f) : TEXT;
            t.draw(text(*f, std::to_string(shown), 16, {col[k], y}, with_alpha(n == 0 ? TEXT_FAINT : ink, a), 1.0f));
        }
    }
    {
        float const y = top + 30.0f + 24.0f * 4.0f + 4.0f;
        float const a = fade(1.2f);
        box(t, {DX, y - 4}, {col[1] - DX, 1}, with_alpha(TEXT_FAINT, 0.6f * a));
        for(int k = 0; k < 2; ++k)
            t.draw(text(*f, tr("debrief.of", {{"total", std::to_string(sides[k]->total)}}), 16, {col[k], y + 2}, with_alpha(TEXT_FAINT, a), 1.0f));
        if(c.level == LevelId::FOREST)
            t.draw(text(*f, tr("debrief.wagons", {{"n", std::to_string(c.wagons_lost)}, {"total", "6"}}), 16, {DX, y + 34}, with_alpha(TEXT_DIM, fade(1.35f))));
    }

    // What next.
    for(int i = 0; i < 3; ++i)
    {
        sf::FloatRect const r = debrief_button(i);
        bool const on = i == m_choice;
        float const a = fade(1.0f + 0.1f * static_cast<float>(i));
        box(t, r.position, r.size, with_alpha(on ? MON_BLUE_DK : INK, (on ? 0.9f : 0.4f) * a), with_alpha(on ? MON_GOLD : TEXT_FAINT, 0.7f * a));
        if(on) box(t, r.position, {3, r.size.y}, with_alpha(MON_GOLD, a));
        t.draw(text(*f, tr(DEBRIEF_BUTTONS[i]), 16, {r.position.x + 14, r.position.y + 6}, with_alpha(on ? MON_GOLD : TEXT_DIM, a)));
    }
    t.draw(text(*f, tr("debrief.keys"), 16, {DX - 6, 522}, with_alpha(TEXT_FAINT, fade(1.4f))));

    // ================================ the right page: what the paper made of it
    float const land = fade(0.55f, 0.35f);
    if(land <= 0.0f) return;
    sf::Vector2f const p = NEWS_BOX.position + sf::Vector2f{0.0f, std::round(18.0f * (1.0f - land))};
    sf::Vector2f const size = NEWS_BOX.size;
    box(t, p + sf::Vector2f{4, 4}, size, with_alpha(sf::Color::Black, 0.5f * land));
    sf::Color const newsprint = lerp(SHEET, PAPER, 0.35f);
    box(t, p, size, with_alpha(newsprint, land));
    {
        sf::VertexArray grain(sf::PrimitiveType::Triangles);
        for(int k = 0; k < 1100; ++k)
            draw::rect(grain, std::floor(p.x + 2 + hash(static_cast<float>(k) * 1.7f) * (size.x - 4)),
                       std::floor(p.y + 2 + hash(static_cast<float>(k) * 3.1f) * (size.y - 4)), 1, 1, with_alpha(SHEET_DK, 0.8f * land));
        t.draw(grain);
    }
    sf::Color const pen = with_alpha(PEN, land);
    sf::Color const pen_dim = with_alpha(lerp(PEN, newsprint, 0.35f), land);
    float const cx = p.x + size.x * 0.5f;
    float const left = p.x + 16.0f, right = p.x + size.x - 16.0f, width = right - left;
    std::string const paper = std::string("newspaper.") + side_key(us);
    std::string const story = std::string("newspaper.") + level_key(c.level) + "." + side_key(us) + (c.won ? ".won" : ".lost");

    // The masthead.
    t.draw(text(*f, tr(paper + ".masthead"), 32, {cx, p.y + 10}, pen, 0.5f));
    t.draw(text(*f, tr(paper + ".motto"), 16, {cx, p.y + 48}, pen_dim, 0.5f));
    box(t, {left, p.y + 70}, {width, 2}, pen);
    t.draw(text(*f, tr(paper + ".edition"), 16, {left, p.y + 75}, pen));
    t.draw(text(*f, level_text(c.level, "paper_date"), 16, {cx, p.y + 75}, pen, 0.5f));
    t.draw(text(*f, tr(paper + ".price"), 16, {right, p.y + 75}, pen, 1.0f));
    box(t, {left, p.y + 95}, {width, 1}, pen);
    box(t, {left, p.y + 98}, {width, 1}, pen);

    // The headline, and what it means.
    float y = p.y + 106.0f;
    for(const auto& line : wrap(*f, item(story + ".headline", m_headline), 32, width))
    {
        t.draw(text(*f, line, 32, {cx, y}, pen, 0.5f));
        y += 32.0f;
    }
    y += 4.0f;
    for(const auto& line : wrap(*f, tr(story + ".subhead"), 16, width - 40.0f))
    {
        t.draw(text(*f, line, 16, {cx, y}, pen_dim, 0.5f));
        y += 17.0f;
    }
    y += 6.0f;
    box(t, {left, y}, {width, 1}, pen);
    y += 8.0f;

    // The story, in two columns; the map at the head of the second.
    int const minutes = std::max(1, static_cast<int>(std::round(c.time / 60.0f)));
    TextArgs const args = {
        {"place", level_name(c.level)},
        {"date", level_text(c.level, "date")},
        {"minutes", minutes == 1 ? tr("newspaper.minutes_one") : tr("newspaper.minutes_many", {{"n", std::to_string(minutes)}})},
        {"our_killed", std::to_string(c.ours.killed)},
        {"our_wounded", std::to_string(c.ours.wounded)},
        {"their_killed", std::to_string(c.theirs.killed)},
        {"their_captured", std::to_string(c.theirs.captured)},
        {"their_fled", std::to_string(c.theirs.fled)},
        {"wagons", std::to_string(c.wagons_lost)},
        {"wagons_saved", std::to_string(6 - c.wagons_lost)},
        {"wagon_count", c.wagons_lost == 0 ? tr("newspaper.wagons_none") : (c.wagons_lost == 1 ? tr("newspaper.wagons_one")
                        : tr("newspaper.wagons_many", {{"n", std::to_string(c.wagons_lost)}}))},
    };
    float const gutter = 14.0f;
    float const col_w = (width - gutter) * 0.5f;
    float const col_x[2] = {left, left + col_w + gutter};
    float const bottom = p.y + size.y - 12.0f;
    box(t, {left + col_w + gutter * 0.5f, y}, {1, bottom - y}, pen_dim);

    float col_y[2] = {y, y};
    if(const sf::RenderTexture* rt = sheet(c.level))
    {
        sf::Vector2f const ms(rt->getSize());
        float const k = std::min(1.0f, col_w / ms.x);
        sf::Sprite ground(rt->getTexture());
        ground.setScale({k, k});
        ground.setPosition({std::round(col_x[1] + (col_w - ms.x * k) * 0.5f), std::round(y)});
        ground.setColor(sf::Color(255, 255, 255, static_cast<std::uint8_t>(255.0f * land)));
        t.draw(ground);
        sf::FloatRect const gb = ground.getGlobalBounds();
        box(t, gb.position - sf::Vector2f{1, 1}, {gb.size.x + 2, 1}, pen);
        box(t, gb.position + sf::Vector2f{-1, gb.size.y}, {gb.size.x + 2, 1}, pen);
        box(t, gb.position - sf::Vector2f{1, 1}, {1, gb.size.y + 2}, pen);
        box(t, gb.position + sf::Vector2f{gb.size.x, -1}, {1, gb.size.y + 2}, pen);
        col_y[1] = gb.position.y + gb.size.y + 4.0f;
        for(const auto& line : wrap(*f, tr("newspaper.map_caption"), 16, col_w))
        {
            t.draw(text(*f, line, 16, {col_x[1], col_y[1]}, pen_dim));
            col_y[1] += 16.0f;
        }
        col_y[1] += 8.0f;
    }

    int column = 0;
    for(const auto& para : list(story + ".story"))
    {
        for(const auto& line : wrap(*f, fill(para, args), 16, col_w))
        {
            if(col_y[column] + 16.0f > bottom && column == 0) column = 1;
            if(col_y[column] + 16.0f > bottom) break;
            t.draw(text(*f, line, 16, {col_x[column], col_y[column]}, pen));
            col_y[column] += 16.0f;
        }
        col_y[column] += 6.0f;
    }
}

}
