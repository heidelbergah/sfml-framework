/**
 * Render.cpp  -  Mud Cats
 *
 * The frame is drawn at 480x270 through the camera, then graded
 * (shaders/mudcats/grade.frag) and scaled up twice. The HUD is drawn
 * afterwards at full resolution, so text stays crisp.
 *
 * What the player cannot see, behind walls and hedges from where he
 * stands, is darkened, and enemies standing in it are not drawn at all.
 * Neither are enemies he can't pick out: lying in grass, under the trees,
 * lost in the mist or in smoke. Their muzzle flashes still give them away.
 *
 * Draw order, bottom to top: the ground (with everything stamped into
 * it), weapons lying about, men, blood and earth in the air, the grass
 * tops and roofs and tree crowns, smoke, fire, sparks, rounds, rain and
 * light.
 */

#include "Battle.hpp"

#include "Draw.hpp"
#include "Palette.hpp"
#include "Sound.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace mudcats
{

namespace
{
    float length(sf::Vector2f v) { return std::sqrt(v.x * v.x + v.y * v.y); }
    sf::Vector2f unit(sf::Vector2f v) { float const l = length(v); return l > 1e-5f ? v / l : sf::Vector2f{1.0f, 0.0f}; }
    sf::Vector2f dir(float a) { return {std::cos(a), std::sin(a)}; }

    sf::Text label(const sf::Font& f, const std::string& s, unsigned size, sf::Vector2f pos, sf::Color c, float align = 0.0f)
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
        t.draw(label(f, s, size, pos + sf::Vector2f{2, 2}, with_alpha(INK, c.a / 255.0f * 0.8f), align));
        t.draw(label(f, s, size, pos, c, align));
    }

    float text_width(const sf::Font& f, const std::string& s, unsigned size)
    {
        return sf::Text(f, s, size).getLocalBounds().size.x;
    }

    void box(sf::RenderTarget& t, sf::Vector2f p, sf::Vector2f s, sf::Color c)
    {
        sf::RectangleShape r(s);
        r.setPosition({std::round(p.x), std::round(p.y)});
        r.setFillColor(c);
        t.draw(r);
    }

    // A small picture made of pixels, drawn at `scale`.
    void glyph(sf::VertexArray& va, const char* const* rows, int n, sf::Vector2f at, float scale, sf::Color c)
    {
        for(int y = 0; y < n; ++y)
            for(int x = 0; rows[y][x]; ++x)
                if(rows[y][x] == '#')
                    draw::rect(va, at.x + static_cast<float>(x) * scale, at.y + static_cast<float>(y) * scale, scale, scale, c);
    }

    // The stance icon: how you would look from the side.
    const char* const STAND_ICON[] = {
        "...##....",
        "...##....",
        "..####...",
        ".#.##.#..",
        ".#.##.#..",
        "...##....",
        "...#.#...",
        "...#.#...",
        "...#.#...",
        "..##.##..",
    };
    const char* const KNEEL_ICON[] = {
        ".........",
        ".........",
        "...##....",
        "...##....",
        "..####...",
        "..####.##",
        "...####..",
        "...#..#..",
        "...#..#..",
        ".###..##.",
    };
    const char* const PRONE_ICON[] = {
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        "##.....##",
        "##########",
        ".#######.",
    };
    const char* const DOWN_ICON[] = {
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        "#########",
        "##.###.##",
    };
}

// ---------------------------------------------------------------- what you can see

void Battle::compute_visibility()
{
    m_visible.assign(m_soldiers.size(), 0);
    const Soldier& me = this->me();
    for(const auto& s : m_soldiers)
    {
        auto& v = m_visible[static_cast<std::size_t>(s.id)];
        if(s.team == me.team) { v = 1; continue; }
        if(s.life != Life::OK && s.life != Life::DOWNED) continue;
        if(!s.active && s.team == Team::RECHIVII && m_level == LevelId::FIELDS && length(s.pos - me.pos) > 50.0f) continue;
        bool const roofed = m_map.under_roof(s.pos);
        if(s.flash > 0.0f && !roofed && length(s.pos - me.pos) < vision_range() * 1.5f) { v = 1; continue; }
        if(roofed && !m_map.under_roof(me.pos) && length(s.pos - me.pos) > 30.0f) continue;
        float const d = length(s.pos - me.pos);
        if(d < 380.0f && can_spot(me, s, d)) { v = 1; continue; }
        // What a comrade nearby can see, you are told about.
        for(const auto& o : m_soldiers)
        {
            if(o.team != me.team || !o.alive()) continue;
            float const od = length(o.pos - s.pos);
            if(od < 160.0f && can_spot(o, s, od)) { v = 1; break; }
        }
    }
}

bool Battle::visible_to_player(const Soldier& s) const
{
    auto const i = static_cast<std::size_t>(s.id);
    return i < m_visible.size() && m_visible[i];
}

void Battle::flush_stamps()
{
    if(m_stamps.getVertexCount() == 0) return;
    m_ground.setView(m_ground.getDefaultView());
    m_ground.draw(m_stamps);
    m_ground.display();
    m_stamps.clear();
}

// ---------------------------------------------------------------- the frame

void Battle::render(sf::RenderTarget& target)
{
    if(!m_ready || m_soldiers.empty()) return;
    flush_stamps();
    compute_visibility();

    sf::View const view = m_camera.get_view();
    m_frame.setView(view);
    m_frame.clear(INK);
    m_frame.draw(sf::Sprite(m_ground.getTexture()));

    draw_world(m_frame, view);
    draw_shade(view);
    m_frame.setView(view);
    draw_crosshair(m_frame);
    m_frame.display();

    // Grade and scale up.
    const Soldier& me = this->me();
    sf::Vector2f const origin = view.getCenter() - view.getSize() * 0.5f;
    sf::Vector2f const eye = (me.pos - origin);
    sf::Sprite frame(m_frame.getTexture());
    frame.setScale({2.0f, 2.0f});
    target.setView(target.getDefaultView());
    if(m_grade)
    {
        float const mist = m_level == LevelId::FIELDS ? m_mist : 0.0f;
        m_grade->setUniform("texture", sf::Shader::CurrentTexture);
        m_grade->setUniform("resolution", sf::Glsl::Vec2{FRAME_F.x, FRAME_F.y});
        m_grade->setUniform("time", m_real);
        m_grade->setUniform("suppression", me.alive() ? me.suppression : 0.0f);
        m_grade->setUniform("hurt", std::min(1.0f, m_hurt + (me.alive() && me.hp < 50.0f ? (50.0f - me.hp) / 80.0f : 0.0f)));
        m_grade->setUniform("flash", m_flash);
        m_grade->setUniform("shock", me.shock);
        m_grade->setUniform("dying", me.life == Life::DOWNED ? std::clamp(1.0f - me.bleed_out / 32.0f, 0.3f, 1.0f) : 0.0f);
        m_grade->setUniform("mist", mist);
        m_grade->setUniform("eye", sf::Glsl::Vec2{eye.x, FRAME_F.y - eye.y});
        m_grade->setUniform("reach", vision_range());
        m_grade->setUniform("rain", m_level == LevelId::TRENCHES ? 1.0f : 0.0f);
        target.draw(frame, sf::RenderStates(m_grade));
    }
    else target.draw(frame);

    draw_hud(target);
}

void Battle::draw_world(sf::RenderTarget& t, const sf::View& view)
{
    sf::FloatRect const seen(view.getCenter() - view.getSize() * 0.5f - sf::Vector2f{30, 30}, view.getSize() + sf::Vector2f{60, 60});
    sf::VertexArray va(sf::PrimitiveType::Triangles);
    const Soldier& me = this->me();

    // Bottles on the range.
    for(const auto& b : m_map.bottles)
        if(!b.broken)
        {
            draw::rect(va, b.pos.x - 1, b.pos.y - 3, 2, 4, lerp(GRASS, MON_WHITE, 0.3f));
            draw::rect(va, b.pos.x - 1, b.pos.y - 4, 1, 1, MON_WHITE);
        }

    // Weapons lying where men dropped them.
    for(const auto& d : m_dropped)
    {
        if(d.taken || !seen.contains(d.pos)) continue;
        sf::Vector2f const u = dir(d.angle);
        switch(d.gun)
        {
            case Gun::RIFLE:
                draw::thick_line(va, d.pos - u * 4.0f, d.pos + u * 1.0f, 1.2f, WOOD);
                draw::thick_line(va, d.pos + u * 1.0f, d.pos + u * 4.0f, 1.0f, GUNMETAL);
                break;
            case Gun::SMG:
                draw::thick_line(va, d.pos - u * 2.5f, d.pos + u * 2.5f, 1.2f, GUNMETAL);
                draw::rect(va, std::floor(d.pos.x), std::floor(d.pos.y), 1, 1, WOOD);
                break;
            case Gun::PISTOL:
                draw::rect(va, std::floor(d.pos.x), std::floor(d.pos.y), 2, 1, GUNMETAL);
                break;
            case Gun::MG:
                draw::thick_line(va, d.pos - u * 4.5f, d.pos + u * 4.5f, 1.8f, GUNMETAL);
                draw::rect(va, std::floor(d.pos.x - u.x * 2.0f), std::floor(d.pos.y - u.y * 2.0f), 2, 2, lerp(GUNMETAL, WOOD, 0.4f));
                break;
            case Gun::FLAMER:
                draw::disc(va, d.pos.x, d.pos.y, 2.0f, lerp(GUNMETAL, RCH_TAN, 0.3f));
                break;
            case Gun::SNIPER:
                draw::thick_line(va, d.pos - u * 4.0f, d.pos + u * 1.0f, 1.2f, WOOD);
                draw::thick_line(va, d.pos + u * 1.0f, d.pos + u * 5.0f, 1.0f, GUNMETAL);
                draw::rect(va, std::floor(d.pos.x), std::floor(d.pos.y) - 1, 2, 1, INK);
                break;
            default: break;
        }
        // A glint, so you can find it when you need it.
        if(me.alive() && length(d.pos - me.pos) < 30.0f && std::fmod(m_real + d.angle, 1.6f) < 0.12f)
            draw::rect(va, std::floor(d.pos.x), std::floor(d.pos.y) - 1, 1, 1, with_alpha(MON_WHITE, 0.8f));
    }

    // Mortars: a tube on its baseplate, jumping when it fires. Left
    // tilted over once nobody is serving it.
    for(const auto& m : m_mortars)
    {
        if(!seen.contains(m.pos)) continue;
        bool const looked = m.seen > 0.0f || (m_map.line_of_sight(me.pos, m.pos) && length(m.pos - me.pos) < vision_range() * 1.1f);
        if(!looked) continue;
        sf::Vector2f const u = dir(m.facing), side{-u.y, u.x};
        draw::disc(va, m.pos.x + 1, m.pos.y + 1, 3.0f, SHADOW);
        draw::disc(va, m.pos.x, m.pos.y, 2.5f, lerp(GUNMETAL, MUD, 0.3f));                    // baseplate
        float const tube = m.silenced ? 5.0f : 7.0f - m.kick * 1.5f;
        sf::Vector2f const tip = m.pos + (m.silenced ? sf::Vector2f{side.x, side.y} * tube : u * tube);
        draw::thick_line(va, m.pos + u * 4.0f - side * 2.5f, m.pos + u * 4.0f, 1.0f, GUNMETAL);   // bipod
        draw::thick_line(va, m.pos + u * 4.0f + side * 2.5f, m.pos + u * 4.0f, 1.0f, GUNMETAL);
        draw::thick_line(va, m.pos, tip, 1.8f, GUNMETAL);
        draw::rect(va, std::floor(tip.x), std::floor(tip.y), 1, 1, lerp(GUNMETAL, PAPER, 0.3f));
        if(!m.silenced && m.rounds > 0)                                                   // the bombs, laid out ready
            for(int k = 0; k < std::min(4, m.rounds); ++k)
                draw::rect(va, std::floor(m.pos.x - u.x * 7.0f + side.x * (static_cast<float>(k) * 2.0f - 3.0f)),
                           std::floor(m.pos.y - u.y * 7.0f + side.y * (static_cast<float>(k) * 2.0f - 3.0f)), 1, 2, lerp(GUNMETAL, GRASS, 0.4f));
    }

    // Things in the air cast shadows on the ground.
    for(const auto& g : m_grenades) draw::rect(va, std::floor(g.pos.x), std::floor(g.pos.y) + 1, 2, 1, SHADOW);
    for(const auto& g : m_gibs) draw::rect(va, std::floor(g.pos.x), std::floor(g.pos.y) + 1, 2, 1, SHADOW);

    // People, back to front.
    std::vector<const Soldier*> order;
    for(const auto& s : m_soldiers)
        if((s.life == Life::OK || s.life == Life::DOWNED) && seen.contains(s.pos) && visible_to_player(s)) order.push_back(&s);
    std::sort(order.begin(), order.end(), [](const Soldier* a, const Soldier* b) { return a->pos.y < b->pos.y; });
    for(const Soldier* s : order) draw_shadow(va, *s);
    for(const Soldier* s : order)
    {
        if(s->life == Life::DOWNED)
            draw::disc(va, s->pos.x + 1, s->pos.y + 1, std::min(6.0f, 2.0f + (32.0f - s->bleed_out) * 0.15f), BLOOD_DK);
        draw_soldier(va, *s, m_time);
        if(s->is_player && s->alive())
        {
            // A small marker so you can find yourself in a crowd.
            sf::Vector2f const p = s->pos + sf::Vector2f{0.0f, -9.0f};
            draw::rect(va, std::floor(p.x) - 1, std::floor(p.y), 3, 1, with_alpha(MON_GOLD, 0.85f));
            draw::rect(va, std::floor(p.x), std::floor(p.y) + 1, 1, 1, with_alpha(MON_GOLD, 0.85f));
        }
        else if(s->yours && s->alive())
        {
            // Your section: a gold tick over each man, brighter on the corporal.
            sf::Vector2f const p = s->pos + sf::Vector2f{0.0f, -8.0f};
            draw::rect(va, std::floor(p.x), std::floor(p.y), 1, s->leader ? 2.0f : 1.0f, with_alpha(MON_GOLD, s->leader ? 0.85f : 0.55f));
        }
        else if(s->team == Team::MONTERAN && s->alive() && s->leader && player_leads())
        {
            // Corporals, when you are giving the orders.
            sf::Vector2f const p = s->pos + sf::Vector2f{0.0f, -8.0f};
            bool const picked = m_selected < 0 || m_selected == s->section;
            draw::rect(va, std::floor(p.x), std::floor(p.y), 1, 1, with_alpha(MON_WHITE, picked ? 0.8f : 0.3f));
        }
        else if(s->team == Team::MONTERAN && s->life == Life::DOWNED && std::fmod(m_real, 1.0f) < 0.5f)
        {
            // A wounded man calling: a small red cross.
            sf::Vector2f const p = s->pos + sf::Vector2f{0.0f, -7.0f};
            draw::rect(va, std::floor(p.x) - 1, std::floor(p.y), 3, 1, ALERT);
            draw::rect(va, std::floor(p.x), std::floor(p.y) - 1, 1, 3, ALERT);
        }
    }

    // Grenades and limbs in flight.
    for(const auto& g : m_grenades)
    {
        sf::Vector2f const p = g.pos - sf::Vector2f{0.0f, g.height};
        sf::Color const body = g.satchel ? LEATHER : (g.smoke ? lerp(GUNMETAL, MON_WHITE, 0.4f) : lerp(GUNMETAL, GRASS, 0.3f));
        draw::rect(va, std::floor(p.x), std::floor(p.y), g.satchel ? 4.0f : 2.0f, g.satchel ? 3.0f : 2.0f, body);
        if(g.satchel && std::fmod(g.fuse, 0.4f) < 0.2f)                    // the fuse spitting
            draw::rect(va, std::floor(p.x + 4), std::floor(p.y) - 1, 1, 1, FIRE_YELLOW);
        draw::rect(va, std::floor(p.x + std::cos(g.spin * g.fuse) * 2.0f), std::floor(p.y + std::sin(g.spin * g.fuse) * 2.0f), 1, 1, WOOD);
    }
    for(const auto& g : m_gibs)
    {
        sf::Vector2f const p = g.pos - sf::Vector2f{0.0f, g.height};
        sf::Vector2f const u = dir(g.angle);
        sf::Color const coat = g.charred ? CHAR : (g.team == Team::MONTERAN ? MON_BLUE : RCH_TAN);
        switch(g.kind)
        {
            case 1: draw::thick_line(va, p - u * 2.0f, p + u * 2.0f, 1.6f, coat); draw::rect(va, std::floor(p.x - u.x * 2.5f), std::floor(p.y - u.y * 2.5f), 1, 1, BLOOD_BRIGHT); break;
            case 2: draw::thick_line(va, p - u * 2.5f, p + u * 2.5f, 2.0f, coat); draw::rect(va, std::floor(p.x - u.x * 3.0f), std::floor(p.y - u.y * 3.0f), 1, 1, BLOOD_BRIGHT); break;
            case 3: draw::disc(va, p.x, p.y, 2.0f, g.charred ? CHAR : SKIN_DK); break;
            case 4: draw::disc(va, p.x, p.y, 2.0f, g.team == Team::MONTERAN ? MON_HELMET : RCH_HELMET); break;
            default: draw::rect(va, std::floor(p.x), std::floor(p.y), 2, 2, GORE); break;
        }
    }
    t.draw(va);
    va.clear();

    // Blood and earth in the air.
    t.draw(*m_dirt_fx);
    t.draw(*m_blood_fx);

    // What stands over the men: grass, roofs, trees.
    m_map.draw_over(t, me.pos, seen, m_time);

    // Smoke and smoke screens: lit by whatever light there is, like the ground.
    t.draw(*m_smoke_fx);
    t.draw(*m_screen_fx);
    t.draw(*m_rain_fx);

    // At night, everything so far is only as bright as the light reaching it.
    if(m_night > 0.0f) draw_lights(view);

    // Then everything that makes its own light, untouched by the dark:
    // fire, sparks, rounds in flight, tracers and flares, and their halos.
    t.setView(view);
    t.draw(*m_fire_fx);
    t.draw(*m_spark_fx);
    t.draw(*m_trail_fx);
    t.draw(*m_glow_fx);
    t.draw(*m_round_fx);

    // Light: additive glow for flashes and fire.
    for(const auto& l : m_lights)
    {
        if(!seen.contains(l.pos)) continue;
        float const k = l.life / l.max_life;
        // Rings, so the light falls off rather than ending at a hard edge.
        // Big lights (a flare, a fire) are dimmer for their size.
        float const strength = l.radius > 60.0f ? 0.5f : 1.0f;
        for(float ring : {1.0f, 0.7f, 0.45f, 0.25f})
        {
            float const r = l.radius * ring;
            sf::CircleShape glow(r, 24);
            glow.setOrigin({r, r});
            glow.setPosition(l.pos);
            glow.setFillColor(with_alpha(l.colour, 0.1f * k * strength));
            t.draw(glow, sf::BlendAdd);
        }
    }
}

void Battle::draw_lights(const sf::View& view)
{
    // A light map: the dark of the night everywhere, and every light that
    // is burning added on top, soft-edged. Multiplied over the frame.
    sf::Color const ambient(84, 92, 132);
    m_light.setView(view);
    m_light.clear(lerp(sf::Color::White, ambient, m_night));
    sf::FloatRect const seen(view.getCenter() - view.getSize() * 0.5f - sf::Vector2f{240, 240}, view.getSize() + sf::Vector2f{480, 480});
    auto glow = [this](sf::Vector2f at, float radius, sf::Color c, float strength)
    {
        for(float ring : {1.0f, 0.75f, 0.5f, 0.3f})
        {
            float const r = radius * ring;
            sf::CircleShape disc(r, 28);
            disc.setOrigin({r, r});
            disc.setPosition(at);
            disc.setFillColor(with_alpha(c, 0.25f * strength));
            m_light.draw(disc, sf::BlendAdd);
        }
    };
    for(const auto& l : m_lights)
        if(seen.contains(l.pos)) glow(l.pos, l.radius * 1.6f, l.colour, std::min(1.0f, l.life / std::max(0.001f, l.max_life) * 1.4f));
    // Your own eyes, used to the dark: a little of what is close by.
    const Soldier& me = this->me();
    glow(me.pos, 80.0f, sf::Color(110, 120, 160), 0.55f);
    m_light.display();

    m_frame.setView(m_frame.getDefaultView());
    m_frame.draw(sf::Sprite(m_light.getTexture()), sf::BlendMultiply);
    m_frame.setView(view);
}

void Battle::draw_shade(const sf::View& view)
{
    const Soldier& me = this->me();
    sf::Vector2f const eye = me.pos;

    m_shade.setView(view);
    m_shade.clear(sf::Color::White);
    sf::VertexArray va(sf::PrimitiveType::Triangles);
    sf::Color const dark(118, 116, 124);
    sf::Color const leafy(200, 200, 206);

    auto cast = [&](const sf::FloatRect& w, sf::Color c)
    {
        sf::Vector2f const centre = w.position + w.size * 0.5f;
        if(length(centre - eye) > 420.0f) return;
        if(w.contains(eye)) return;
        sf::Vector2f const p[4] = {w.position, {w.position.x + w.size.x, w.position.y},
                                   w.position + w.size, {w.position.x, w.position.y + w.size.y}};
        for(int i = 0; i < 4; ++i)
        {
            sf::Vector2f const a = p[i], b = p[(i + 1) % 4];
            sf::Vector2f const fa = a + unit(a - eye) * 900.0f, fb = b + unit(b - eye) * 900.0f;
            draw::quad(va, a, b, fb, fa, c);
        }
    };
    // Hedges first, lighter: they hide, but you can tell there is a field beyond.
    for(const auto& h : m_map.hedges) cast(h, leafy);
    m_shade.draw(va, sf::RenderStates(sf::BlendMultiply));
    va.clear();
    for(const auto& w : m_map.walls) if(w.size.x > 5.0f || w.size.y > 5.0f) cast(w, dark);
    m_shade.draw(va, sf::RenderStates(sf::BlendMin));
    va.clear();
    for(const auto& w : m_map.walls) draw::rect(va, w.position.x, w.position.y, w.size.x, w.size.y, sf::Color::White);
    for(const auto& h : m_map.hedges) draw::rect(va, h.position.x, h.position.y, h.size.x, h.size.y, sf::Color::White);
    // A roof you are not under is seen from above, whatever is behind its walls.
    for(const auto& r : m_map.roofs)
        if(!r.contains(eye)) draw::rect(va, r.position.x - 6, r.position.y - 6, r.size.x + 12, r.size.y + 12, sf::Color::White);
    m_shade.draw(va, sf::RenderStates(sf::BlendNone));
    m_shade.display();

    m_frame.setView(m_frame.getDefaultView());
    m_frame.draw(sf::Sprite(m_shade.getTexture()), sf::BlendMultiply);
}

void Battle::draw_crosshair(sf::RenderTarget& frame)
{
    const Soldier& me = this->me();
    if(!me.alive() || m_paused || m_autoplay) return;
    const WeaponSpec& w = me.weapon();

    float const spread = spread_of(me);
    float const d = length(m_mouse_world - me.pos);
    float const gap = std::clamp(spread * d * 1.6f, 2.0f, 30.0f);
    sf::Vector2f const c{std::round(m_mouse_world.x), std::round(m_mouse_world.y)};
    bool const ready = me.reloading <= 0.0f && me.cooldown <= 0.0f && (!me.has_mg() || me.deployed) && me.gun != Gun::NONE;
    sf::Color const col = with_alpha(ready ? MON_WHITE : TEXT_DIM, 0.85f);

    sf::VertexArray va(sf::PrimitiveType::Triangles);
    if(w.flame)
    {
        draw::rect(va, c.x - 1, c.y - 1, 3, 3, with_alpha(FIRE_ORANGE, 0.8f));
    }
    else
    {
        draw::rect(va, c.x - gap - 3, c.y, 3, 1, col);
        draw::rect(va, c.x + gap + 1, c.y, 3, 1, col);
        draw::rect(va, c.x, c.y - gap - 3, 1, 3, col);
        draw::rect(va, c.x, c.y + gap + 1, 1, 3, col);
        draw::rect(va, c.x, c.y, 1, 1, with_alpha(col, 0.5f));
    }
    if(me.reloading > 0.0f && w.reload > 0.0f)
    {
        float const k = 1.0f - me.reloading / w.reload;
        draw::rect(va, c.x - 6, c.y + gap + 6, 12, 1, with_alpha(INK, 0.6f));
        draw::rect(va, c.x - 6, c.y + gap + 6, std::round(12.0f * k), 1, MON_WHITE);
    }
    // The machine gun's arc when it is set up.
    if(me.deployed)
        for(float k = -0.85f; k <= 0.86f; k += 0.85f)
        {
            sf::Vector2f const e = me.pos + dir(me.deploy_dir + k) * 26.0f;
            draw::rect(va, std::floor(e.x), std::floor(e.y), 1, 1, with_alpha(MON_WHITE, 0.4f));
        }
    // Orders go to the cursor: show which section they will go to.
    if(player_leads() && m_selected >= 0)
        draw::rect(va, c.x + 4, c.y - 6, 1, 1, MON_GOLD);
    frame.draw(va);
}

// ---------------------------------------------------------------- markers and compass

void Battle::draw_markers(sf::RenderTarget& t)
{
    if(!m_font || m_stage >= Stage::WON) return;
    const sf::Font& f = *m_font;
    float const W = static_cast<float>(BASE_RESOLUTION.x), H = static_cast<float>(BASE_RESOLUTION.y);
    const Soldier& me = this->me();
    sf::View const view = m_camera.get_view();
    sf::Vector2f const origin = view.getCenter() - view.getSize() * 0.5f;
    auto to_screen = [&](sf::Vector2f p) { return (p - origin) * 2.0f; };

    // Where the objective is: the next point the platoon is making for.
    sf::Vector2f goal = objective_point();
    if(m_level == LevelId::TRENCHES)
    {
        if(m_stage <= Stage::RANGE) goal = m_map.range_area.contains(me.pos) || m_bottles_left == 0 ? m_map.front_trench_area.getCenter() : m_map.range_area.getCenter();
        else if(m_stage <= Stage::BARRAGE) goal = m_map.front_trench_area.getCenter();
        else if(m_stage == Stage::ASSAULT) goal = m_map.enemy_trench_area.getCenter();
        else if(m_stage == Stage::TRENCH) goal = m_map.bunker_area.getCenter();
        else goal = m_map.farm_area.getCenter();
    }
    else if(m_level == LevelId::FIELDS)
    {
        if(m_stage <= Stage::CONTACT) goal = m_map.waypoints.front();
        else if(m_stage == Stage::WOOD) goal = m_map.wood_area.getCenter();
        else if(m_stage == Stage::STREAM) goal = m_map.bridge_area.getCenter();
        else goal = m_map.mill_area.getCenter();
    }
    else
    {
        if(m_stage == Stage::RAID_CREEP)
        {
            // Your group's lane: the form-up point, the far side of the wire, their trench.
            float band = 0.0f;
            for(const auto& sec : m_sections) if(sec.id == me.section) band = sec.band;
            int const wp = m_creep <= 1 ? 2 : (m_creep == 2 ? 3 : 4);
            goal = m_map.waypoints[static_cast<std::size_t>(wp)] + sf::Vector2f{0.0f, band};
        }
        else if(m_stage == Stage::RAID_TRENCH) goal = m_map.enemy_trench_area.getCenter();
        else if(m_stage == Stage::RAID_HAMLET) goal = m_map.farm_area.getCenter();
        else if(m_stage == Stage::RAID_DUGOUT) goal = m_map.bunker_area.getCenter();
        else goal = m_map.front_trench_area.getCenter();
    }
    if(m_level == LevelId::HILL) goal = m_map.crest;
    bool const on_the_hill = m_level == LevelId::HILL && length(me.pos - m_map.crest) < 280.0f;

    // Wounded men of ours, at the edge of the screen when they are off it:
    // a medic needs to know where they are; anyone can drag them.
    {
        sf::VertexArray crosses(sf::PrimitiveType::Triangles);
        sf::FloatRect const edge({30.0f, 60.0f}, {W - 60.0f, H - 170.0f});
        for(const auto& s : m_soldiers)
        {
            if(s.team != Team::MONTERAN || s.life != Life::DOWNED || s.is_player) continue;
            float const d = length(s.pos - me.pos);
            if(d > (me.role == Role::MEDIC ? 700.0f : 260.0f)) continue;
            sf::Vector2f p = to_screen(s.pos);
            if(edge.contains(p)) continue;                  // on screen: the cross over him does it
            p.x = std::clamp(p.x, edge.position.x, edge.position.x + edge.size.x);
            p.y = std::clamp(p.y, edge.position.y, edge.position.y + edge.size.y);
            sf::Color const c = with_alpha(ALERT, std::fmod(m_real, 1.0f) < 0.6f ? 0.9f : 0.45f);
            draw::rect(crosses, p.x - 6, p.y - 2, 12, 4, c);
            draw::rect(crosses, p.x - 2, p.y - 6, 4, 12, c);
        }
        t.draw(crosses);
    }

    // A mortar's flash, marked where it was seen: on the screen, a ring
    // round it; off it, a mark at the edge in its direction. For a
    // radioman, the cursor on the mark and V calls the guns on it.
    {
        sf::VertexArray marks(sf::PrimitiveType::Triangles);
        sf::FloatRect const edge({30.0f, 60.0f}, {W - 60.0f, H - 170.0f});
        for(const auto& m : m_mortars)
        {
            if(m.silenced || m.seen <= 0.0f || length(m.pos - me.pos) > 1100.0f) continue;
            sf::Vector2f const p = to_screen(m.pos);
            float const a = std::min(1.0f, m.seen / 2.0f) * (std::fmod(m_real, 0.8f) < 0.5f ? 0.95f : 0.5f);
            sf::Color const c = with_alpha(ALERT, a);
            if(edge.contains(p))
            {
                for(int k = 0; k < 12; ++k)
                {
                    sf::Vector2f const q = p + dir(6.283f * static_cast<float>(k) / 12.0f) * 16.0f;
                    draw::rect(marks, std::floor(q.x) - 1, std::floor(q.y) - 1, 3, 3, c);
                }
                shadowed(t, f, "mortar", 16, p + sf::Vector2f{0.0f, 20.0f}, c, 0.5f);
            }
            else
            {
                sf::Vector2f const q = edge_marker(m.pos);
                for(int i = 0; i < 5; ++i)
                {
                    float const k = static_cast<float>(i);
                    draw::rect(marks, q.x - k, q.y - 8.0f + k * 2.0f, 1.0f + k * 2.0f, 2.0f, c);
                    draw::rect(marks, q.x - k, q.y + 8.0f - k * 2.0f, 1.0f + k * 2.0f, 2.0f, c);
                }
                shadowed(t, f, "mortar", 16, q + sf::Vector2f{0.0f, 12.0f}, c, 0.5f);
            }
        }
        t.draw(marks);
    }

    // Once you're there, there's nothing to point at.
    if(on_the_hill) return;
    for(const sf::FloatRect* area : {&m_map.front_trench_area, &m_map.farm_area, &m_map.mill_area, &m_map.range_area, &m_map.enemy_trench_area, &m_map.bunker_area, &m_map.hill_area})
        if(area->size.x > 0.0f && area->contains(me.pos) && area->contains(goal)) return;

    sf::Vector2f const sp = to_screen(goal);
    float const dist = length(goal - me.pos) / 8.0f;          // 8 px to the yard, roughly
    sf::FloatRect const inner({40.0f, 70.0f}, {W - 80.0f, H - 190.0f});
    sf::VertexArray va(sf::PrimitiveType::Triangles);
    char buf[32];
    std::snprintf(buf, sizeof buf, "%d yd", static_cast<int>(dist));
    if(inner.contains(sp))
    {
        // A diamond over the place itself, once you can see it.
        if(dist > 8.0f)
        {
            for(int i = 0; i < 4; ++i)
            {
                float const k = static_cast<float>(i);
                draw::rect(va, sp.x - k, sp.y - 6.0f + k * 2.0f, 1.0f + k * 2.0f, 2.0f, with_alpha(MON_GOLD, 0.75f));
                draw::rect(va, sp.x - k, sp.y + 6.0f - k * 2.0f, 1.0f + k * 2.0f, 2.0f, with_alpha(MON_GOLD, 0.75f));
            }
            t.draw(va);
            shadowed(t, f, buf, 16, sp + sf::Vector2f{0.0f, 12.0f}, with_alpha(MON_GOLD, 0.8f), 0.5f);
        }
    }
    else
    {
        // An arrow at the edge of the screen, pointing at it.
        sf::Vector2f const centre{W * 0.5f, H * 0.5f};
        sf::Vector2f const d = unit(sp - centre);
        float const kx = d.x != 0.0f ? (d.x > 0 ? inner.position.x + inner.size.x - centre.x : inner.position.x - centre.x) / d.x : 1e9f;
        float const ky = d.y != 0.0f ? (d.y > 0 ? inner.position.y + inner.size.y - centre.y : inner.position.y - centre.y) / d.y : 1e9f;
        sf::Vector2f const edge = centre + d * std::min(kx, ky);
        sf::Vector2f const n{-d.y, d.x};
        sf::Color const c = with_alpha(MON_GOLD, 0.8f);
        va.append({edge + d * 10.0f, c});
        va.append({edge - d * 4.0f + n * 7.0f, c});
        va.append({edge - d * 4.0f - n * 7.0f, c});
        t.draw(va);
        shadowed(t, f, buf, 16, edge - d * 22.0f - sf::Vector2f{0.0f, 8.0f}, with_alpha(MON_GOLD, 0.8f), 0.5f);
    }
    va.clear();

    // The compass: a strip along the top with the objective on it.
    float const cx = W * 0.5f, cy = 16.0f, half = 110.0f;
    box(t, {cx - half, cy + 14}, {half * 2.0f, 1}, with_alpha(TEXT_FAINT, 0.6f));
    float const facing = me.aim;
    auto put = [&](float bearing, const std::string& text, sf::Color c)
    {
        float const rel = std::remainder(bearing - facing, 6.2831853f);
        if(std::abs(rel) > 1.25f) return;
        float const x = cx + rel / 1.25f * half;
        shadowed(t, f, text, 16, {x, cy - 2.0f}, with_alpha(c, 1.0f - std::abs(rel) / 1.25f * 0.6f), 0.5f);
        box(t, {x, cy + 11}, {1, 4}, with_alpha(c, 0.8f));
    };
    // Screen up is north.
    put(-1.5708f, "N", TEXT); put(0.0f, "E", TEXT_DIM); put(1.5708f, "S", TEXT_DIM); put(3.14159f, "W", TEXT_DIM);
    put(-0.785f, "ne", TEXT_FAINT); put(0.785f, "se", TEXT_FAINT); put(2.356f, "sw", TEXT_FAINT); put(-2.356f, "nw", TEXT_FAINT);
    sf::Vector2f const to_goal = goal - me.pos;
    if(length(to_goal) > 10.0f) put(std::atan2(to_goal.y, to_goal.x), "<>", MON_GOLD);
    box(t, {cx, cy + 15}, {1, 4}, TEXT);
}

sf::Vector2f Battle::edge_marker(sf::Vector2f world) const
{
    // Where on the screen a mark for this point goes: on it, or pinned to
    // the edge of the screen in its direction.
    float const W = static_cast<float>(BASE_RESOLUTION.x), H = static_cast<float>(BASE_RESOLUTION.y);
    sf::View const view = m_camera.get_view();
    sf::Vector2f p = (world - (view.getCenter() - view.getSize() * 0.5f)) * 2.0f;
    sf::FloatRect const edge({30.0f, 60.0f}, {W - 60.0f, H - 170.0f});
    p.x = std::clamp(p.x, edge.position.x, edge.position.x + edge.size.x);
    p.y = std::clamp(p.y, edge.position.y, edge.position.y + edge.size.y);
    return p;
}

// ---------------------------------------------------------------- the HUD

void Battle::draw_hud(sf::RenderTarget& t)
{
    if(!m_font) return;
    const sf::Font& f = *m_font;
    t.setView(t.getDefaultView());
    float const W = static_cast<float>(BASE_RESOLUTION.x), H = static_cast<float>(BASE_RESOLUTION.y);
    const Soldier& me = this->me();
    sf::View const view = m_camera.get_view();
    sf::Vector2f const origin = view.getCenter() - view.getSize() * 0.5f;
    auto to_screen = [&](sf::Vector2f p) { return (p - origin) * 2.0f; };
    bool const leading = player_leads();

    // --- barks over heads -----------------------------------------------------
    for(const auto& s : m_soldiers)
    {
        if(s.bark_time <= 0.0f || s.bark.empty() || !visible_to_player(s)) continue;
        sf::Vector2f const p = to_screen(s.pos) + sf::Vector2f{0.0f, -40.0f - (1.6f - s.bark_time) * 8.0f};
        if(p.x < -50 || p.x > W + 50 || p.y < -20 || p.y > H) continue;
        float const a = std::min(1.0f, s.bark_time * 2.0f);
        shadowed(t, f, s.bark, 16, p, with_alpha(s.team == Team::MONTERAN ? MON_WHITE : RCH_TAN, a), 0.5f);
    }

    draw_markers(t);

    // --- objective, top left -------------------------------------------------------------
    if(!m_objective.empty())
    {
        shadowed(t, f, "OBJECTIVE", 16, {20, 14}, TEXT_FAINT);
        shadowed(t, f, m_objective, 16, {20, 32}, TEXT);
    }
    // The hill: how many so far, and how long until the next.
    if(m_level == LevelId::HILL && m_stage < Stage::WON)
    {
        char line[96];
        if(m_stage == Stage::HILL_LULL)
        {
            float const left = (m_wave == 0 ? FIRST_LULL : LULL) - m_stage_time;
            std::snprintf(line, sizeof line, "next wave in %d", std::max(0, static_cast<int>(std::ceil(left))));
            shadowed(t, f, line, 16, {20, 50}, left < 6.0f && std::fmod(m_real, 0.6f) < 0.35f ? ALERT : PAPER_DK);
        }
        else
        {
            int coming = 0;
            for(const auto& s : m_soldiers) if(s.team == Team::RECHIVII && s.wave == m_wave && s.fighting() && s.active) ++coming;
            for(const auto& pd : m_pending) if(pd.kind != 5) ++coming;
            std::snprintf(line, sizeof line, "wave %d   %d of them left", m_wave, coming);
            shadowed(t, f, line, 16, {20, 50}, ALERT);
        }
        std::snprintf(line, sizeof line, "waves held: %d", m_waves_held);
        shadowed(t, f, line, 16, {20, 68}, TEXT_FAINT);
    }

    // --- the platoon, top right: a row of pips for each section ---------------------------
    {
        float const right = W - 18.0f;
        float y = 14.0f;
        shadowed(t, f, "3RD RIFLES, B PLATOON", 16, {right, y}, TEXT_FAINT, 1.0f);
        y += 22.0f;
        auto pip_colour = [](const Soldier& s)
        {
            if(s.life == Life::DEAD) return with_alpha(TEXT_FAINT, 0.35f);
            if(s.life == Life::DOWNED) return ALERT;
            if(s.burning > 0.0f) return FIRE_ORANGE;
            if(s.hp < 60.0f || s.lost) return lerp(ALERT, MON_GOLD, 0.5f);
            if(s.suppression > 0.75f) return PAPER_DK;
            return MON_WHITE;
        };
        auto row = [&](const std::string& name, int section, bool picked, bool yours, const char* order)
        {
            sf::VertexArray pips(sf::PrimitiveType::Triangles);
            float x = right - 8.0f * 8.0f;
            int alive = 0;
            for(const auto& s : m_soldiers)
            {
                if(s.team != Team::MONTERAN || s.section != section) continue;
                if(s.alive()) ++alive;
                sf::Color c = pip_colour(s);
                draw::rect(pips, x, y + 4.0f, 6.0f, s.leader && s.alive() ? 10.0f : 8.0f, c);
                if(s.is_player) draw::rect(pips, x, y + 15.0f, 6.0f, 2.0f, MON_GOLD);
                x += 8.0f;
            }
            t.draw(pips);
            sf::Color const c = yours ? MON_GOLD : (alive ? TEXT : TEXT_FAINT);
            std::string const text = (picked ? "> " : "") + name;
            shadowed(t, f, text, 16, {right - 74.0f, y}, c, 1.0f);
            if(order && alive) shadowed(t, f, order, 16, {right - 76.0f - text_width(f, text, 16) - 10.0f, y}, TEXT_FAINT, 1.0f);
            y += 21.0f;
        };
        static const char* const order_words[] = {"follow", "hold", "advance", "covering", "charge"};
        // The lieutenant first.
        for(const auto& s : m_soldiers)
            if(s.team == Team::MONTERAN && s.role == Role::OFFICER)
            {
                sf::VertexArray pip(sf::PrimitiveType::Triangles);
                draw::rect(pip, right - 8.0f * 8.0f, y + 4.0f, 6.0f, 10.0f, pip_colour(s));
                if(s.is_player) draw::rect(pip, right - 8.0f * 8.0f, y + 15.0f, 6.0f, 2.0f, MON_GOLD);
                t.draw(pip);
                shadowed(t, f, s.alive() ? "Lt. Varga" : "Lt. Varga, dead", 16, {right - 74.0f, y}, s.is_player ? MON_GOLD : (s.alive() ? TEXT : TEXT_FAINT), 1.0f);
                y += 21.0f;
                break;
            }
        for(const auto& sec : m_sections)
        {
            if(sec.team != Team::MONTERAN) continue;
            bool const picked = leading && (m_selected < 0 || m_selected == sec.id);
            row(sec.name, sec.id, picked && m_selected >= 0, me.section == sec.id, fighting_started() ? order_words[static_cast<int>(sec.order)] : nullptr);
        }
        if(leading && fighting_started())
        {
            shadowed(t, f, m_selected < 0 ? "orders to: all" : "orders to: " + m_sections[static_cast<std::size_t>(m_selected)].name, 16, {right, y + 2}, TEXT_FAINT, 1.0f);
            y += 22.0f;
        }

        // Your own section by name: who they are, and how they are.
        if(me.section >= 0)
        {
            y += 8.0f;
            box(t, {right - 220.0f, y - 4.0f}, {222.0f, 1.0f}, with_alpha(MON_GOLD, 0.4f));
            for(const auto& s : m_soldiers)
            {
                if(s.team != Team::MONTERAN || s.section != me.section) continue;
                std::string state;
                sf::Color sc = TEXT_FAINT;
                if(s.life == Life::DEAD)        { state = "dead";    sc = with_alpha(TEXT_FAINT, 0.6f); }
                else if(s.life == Life::DOWNED) { state = "DOWN";    sc = ALERT; }
                else if(s.burning > 0.0f)       { state = "burning"; sc = FIRE_ORANGE; }
                else if(s.surrendered)          { state = ""; }
                else if(s.hp < 60.0f || s.lost) { state = "hurt";    sc = lerp(ALERT, TEXT, 0.4f); }
                else if(s.suppression > 0.75f)  { state = "pinned";  sc = PAPER_DK; }
                std::string const role = s.role == Role::RIFLEMAN ? "" : std::string(role_name(s.role));
                std::string who = (s.leader ? "Cpl. " : "") + s.name.substr(s.name.find(' ') + 1);
                if(s.is_player) who = "> you";
                sf::Color const nc = s.is_player ? MON_GOLD : (s.life == Life::DEAD ? with_alpha(TEXT_FAINT, 0.6f) : TEXT);
                shadowed(t, f, who, 16, {right - 220.0f, y}, nc);
                shadowed(t, f, state.empty() ? role : state, 16, {right, y}, state.empty() ? with_alpha(TEXT_FAINT, 0.8f) : sc, 1.0f);
                y += 16.0f;
            }
        }
    }

    // --- your man, bottom left ----------------------------------------------------------------
    {
        const WeaponSpec& w = me.weapon();
        float const bx = 20.0f, by = H - 92.0f;
        box(t, {bx - 10, by - 10}, {300, 86}, with_alpha(INK, 0.6f));
        box(t, {bx - 10, by - 10}, {300, 1}, with_alpha(TEXT_FAINT, 0.5f));

        // The stance, as a picture.
        sf::VertexArray icon(sf::PrimitiveType::Triangles);
        const char* const* rows = me.life == Life::DOWNED ? DOWN_ICON
                                : me.stance == Stance::STAND ? STAND_ICON : me.stance == Stance::CROUCH ? KNEEL_ICON : PRONE_ICON;
        sf::Color const ic = me.life == Life::DOWNED ? ALERT : (me.stance_change > 0.0f ? TEXT_DIM : MON_WHITE);
        glyph(icon, rows, 10, {bx, by + 6}, 5.0f, ic);
        t.draw(icon);
        std::string const stance_word = me.life == Life::DOWNED ? "down" : me.stance == Stance::STAND ? "standing"
                                      : me.stance == Stance::CROUCH ? "kneeling" : "prone";
        shadowed(t, f, stance_word, 16, {bx + 22, by + 58}, TEXT_FAINT, 0.5f);

        float const tx = bx + 62.0f;
        shadowed(t, f, me.rank + " " + me.name, 16, {tx, by - 2}, MON_GOLD);
        shadowed(t, f, std::string(role_name(me.role)) + (me.leader ? ", section leader" : ""), 16, {tx, by + 14}, TEXT_FAINT);

        // Health: a bar, and in words what is wrong.
        float const hp = std::clamp(me.hp / 100.0f, 0.0f, 1.0f);
        box(t, {tx, by + 36}, {100, 5}, with_alpha(INK, 0.9f));
        bool const blink = me.bleeding > 0.0f && std::fmod(m_real, 0.6f) < 0.3f;
        box(t, {tx, by + 36}, {std::round(100.0f * hp), 5}, blink ? BLOOD_BRIGHT : (hp < 0.4f ? ALERT : lerp(ALERT, MON_WHITE, hp)));
        std::string health = "unhurt";
        sf::Color hc = TEXT_DIM;
        if(me.life == Life::DOWNED) { health = "bleeding out"; hc = ALERT; }
        else if(me.bleeding > 0.0f) { health = "bleeding"; hc = ALERT; }
        else if(me.hp < 40.0f) { health = "badly hurt"; hc = ALERT; }
        else if(me.hp < 75.0f) { health = "wounded"; hc = lerp(ALERT, TEXT, 0.5f); }
        if(me.lost & (ARM_L | ARM_R)) health += ", no arm";
        if(me.lost & (LEG_L | LEG_R)) health += ", no leg";
        if(me.burning > 0.0f) { health = "ON FIRE"; hc = FIRE_ORANGE; }
        shadowed(t, f, health, 16, {tx + 108, by + 30}, hc);

        // The weapon and what is in it.
        char buf[96];
        float const wy = by + 50.0f;
        if(me.gun == Gun::NONE) shadowed(t, f, "no weapon", 16, {tx, wy}, ALERT);
        else if(w.flame)
        {
            box(t, {tx, wy + 6}, {100, 5}, with_alpha(INK, 0.9f));
            box(t, {tx, wy + 6}, {std::round(me.fuel), 5}, FIRE_ORANGE);
            shadowed(t, f, "fuel", 16, {tx + 108, wy}, TEXT_DIM);
        }
        else
        {
            // Rounds in the magazine as pips, up to ten; beyond that, a number.
            sf::VertexArray rounds(sf::PrimitiveType::Triangles);
            if(w.magazine <= 10)
            {
                for(int i = 0; i < w.magazine; ++i)
                    draw::rect(rounds, tx + static_cast<float>(i) * 7.0f, wy + 2, 4, 10, i < me.ammo ? MON_GOLD : with_alpha(TEXT_FAINT, 0.4f));
                t.draw(rounds);
                std::snprintf(buf, sizeof buf, "+%d", me.spare);
                shadowed(t, f, me.reloading > 0.0f ? "reloading" : buf, 16, {tx + static_cast<float>(w.magazine) * 7.0f + 6.0f, wy}, TEXT_DIM);
            }
            else
            {
                if(me.reloading > 0.0f) std::snprintf(buf, sizeof buf, "reloading   +%d", me.spare);
                else std::snprintf(buf, sizeof buf, "%d  +%d", me.ammo, me.spare);
                shadowed(t, f, buf, 16, {tx, wy}, me.ammo == 0 ? ALERT : TEXT);
            }
            if(me.has_mg())
            {
                box(t, {tx + 108, wy + 6}, {60, 5}, with_alpha(INK, 0.9f));
                box(t, {tx + 108, wy + 6}, {std::round(60.0f * std::min(1.0f, me.heat)), 5}, me.overheated > 0.0f ? ALERT : lerp(MON_GOLD, FIRE_ORANGE, me.heat));
                shadowed(t, f, me.deployed ? "set up" : (me.deploying > 0.0f ? "setting up" : "packed"), 16, {tx + 174, wy}, TEXT_FAINT);
            }
        }
        // Everything else he carries.
        std::string kit;
        if(me.grenades) kit += "G " + std::to_string(me.grenades) + "  ";
        if(me.smokes) kit += "T smoke " + std::to_string(me.smokes) + "  ";
        if(me.flares) kit += "X flare " + std::to_string(me.flares) + "  ";
        if(me.satchels) kit += "V satchel " + std::to_string(me.satchels) + "  ";
        if(me.role == Role::RADIOMAN) kit += "V call " + std::to_string(me.missions) + (m_smoke_mission ? " smoke (B)  " : " HE (B)  ");
        if(m_level == LevelId::HILL) kit += "H dig, bags " + std::to_string(me.bags) + "  ";
        if(me.role == Role::MEDIC) kit += "kits " + std::to_string(me.medkits);
        else if(me.bandages) kit += "F bandage";
        shadowed(t, f, kit, 16, {tx, by + 64}, TEXT_FAINT);

        // The radio, when a call is going.
        for(const auto& m : m_missions)
        {
            if(m.caller != me.id) continue;
            static const char* const steps[] = {"on the set: calling...", "ranging round in the air", "V on the target: fire for effect", "salvo on the way"};
            std::string line = steps[std::clamp(m.step, 0, 3)];
            if(m.step == 2) line += "  " + std::to_string(static_cast<int>(std::ceil(m.timer)));
            shadowed(t, f, line, 16, {bx - 2, by - 30}, std::fmod(m_real, 0.8f) < 0.5f ? MON_GOLD : TEXT_DIM);
        }
    }

    // --- what E would do, over your head ---------------------------------------------------
    if(std::string const prompt = interact_prompt(); !prompt.empty())
    {
        sf::Vector2f const p = to_screen(me.pos) + sf::Vector2f{0.0f, 26.0f};
        shadowed(t, f, prompt, 16, p, with_alpha(MON_WHITE, 0.9f), 0.5f);
    }
    // Digging: how far along.
    if(me.digging > 0.0f)
    {
        sf::Vector2f const p = to_screen(me.pos) + sf::Vector2f{-20.0f, 44.0f};
        box(t, p, {40, 3}, with_alpha(INK, 0.8f));
        box(t, p, {std::round(40.0f * (1.0f - me.digging / std::max(0.1f, me.dig_total))), 3}, SANDBAG);
    }
    // Cutting wire: how far along.
    if(me.cutting > 0.0f)
    {
        float const total = me.role == Role::SAPPER ? 2.5f : 4.0f;
        sf::Vector2f const p = to_screen(me.pos) + sf::Vector2f{-20.0f, 44.0f};
        box(t, p, {40, 3}, with_alpha(INK, 0.8f));
        box(t, p, {std::round(40.0f * (1.0f - me.cutting / total)), 3}, MON_WHITE);
    }

    // --- down -------------------------------------------------------------------------------------
    if(me.life == Life::DOWNED)
    {
        char line[64];
        std::snprintf(line, sizeof line, "BLEEDING OUT   %d", static_cast<int>(std::ceil(me.bleed_out)));
        shadowed(t, f, line, 32, {W * 0.5f, H * 0.5f - 60}, ALERT, 0.5f);
        shadowed(t, f, "E  call for a medic      WASD  crawl      space  let go", 16, {W * 0.5f, H * 0.5f - 22}, TEXT_DIM, 0.5f);
    }
    if(m_takeover > 0.0f)
    {
        float const a = std::min(1.0f, m_takeover);
        shadowed(t, f, m_takeover_text, 32, {W * 0.5f, H * 0.5f - 70}, with_alpha(TEXT, a), 0.5f);
    }

    // --- orders, bottom right, when they are yours to give ---------------------------------------
    if(leading && me.alive() && fighting_started())
    {
        shadowed(t, f, "tab  section", 16, {W - 18, H - 46}, TEXT_FAINT, 1.0f);
        shadowed(t, f, "1 follow   2 hold   3 advance   4 cover   5 charge", 16, {W - 18, H - 28}, TEXT_FAINT, 1.0f);
    }

    // --- dialogue, hints, notices ------------------------------------------------------------------
    if(!m_lines.empty())
    {
        const Line& l = m_lines.front();
        float const a = std::min(1.0f, l.time * 2.0f);
        float const ly = H - 150.0f;
        if(!l.who.empty()) shadowed(t, f, l.who, 16, {W * 0.5f, ly}, with_alpha(l.team == Team::MONTERAN ? MON_GOLD : RCH_TAN, a), 0.5f);
        shadowed(t, f, l.text, 16, {W * 0.5f, ly + 18}, with_alpha(TEXT, a), 0.5f);
    }
    if(m_hint_time > 0.0f && !m_hint.empty())
    {
        float const a = std::min(1.0f, m_hint_time);
        shadowed(t, f, m_hint, 16, {W * 0.5f, H - 108.0f}, with_alpha(PAPER_DK, a), 0.5f);
    }
    if(m_notice > 0.0f && !m_notice_text.empty())
    {
        float const a = std::min(1.0f, m_notice * 2.0f);
        shadowed(t, f, m_notice_text, 16, {W * 0.5f, H * 0.5f + 70.0f}, with_alpha(MON_WHITE, a), 0.5f);
    }

    // --- endings ------------------------------------------------------------------------------------
    if(m_stage == Stage::WON || m_stage == Stage::LOST)
    {
        float const a = std::clamp((m_end_timer - 1.0f) / 1.0f, 0.0f, 1.0f);
        const char* const won = m_level == LevelId::TRENCHES ? "THE FARM IS TAKEN" : (m_level == LevelId::FIELDS ? "THE MILL IS TAKEN" : "BACK BEFORE FIRST LIGHT");
        const char* const lost = m_level == LevelId::RAID ? "THE RAID HAS FAILED" : (m_level == LevelId::HILL ? "HILL 312 HAS FALLEN" : "THE ATTACK HAS FAILED");
        shadowed(t, f, m_stage == Stage::WON ? won : lost, 32, {W * 0.5f, 150}, with_alpha(TEXT, a), 0.5f);
        if(m_level == LevelId::HILL)
        {
            char line[64];
            std::snprintf(line, sizeof line, "held for %d %s", m_waves_held, m_waves_held == 1 ? "wave" : "waves");
            shadowed(t, f, line, 16, {W * 0.5f, 196}, with_alpha(PAPER_DK, a), 0.5f);
        }
    }

    if(m_paused)
    {
        box(t, {0, 0}, {W, H}, with_alpha(INK, 0.78f));
        shadowed(t, f, "PAUSED", 32, {W * 0.5f, 90}, TEXT, 0.5f);
        shadowed(t, f, "esc  back to it        q  quit to the briefing        m  music on or off", 16, {W * 0.5f, 140}, TEXT_DIM, 0.5f);
        const char* const keys[][2] = {
            {"wasd", "move"}, {"shift", "run (gets you up first)"}, {"mouse", "aim"},
            {"left click", "fire"}, {"right click", "steady aim, look further, set up a machine gun"},
            {"middle click", "bayonet"}, {"c / z", "kneel / lie down"}, {"r", "reload"},
            {"g / t", "grenade / smoke grenade"}, {"x", "red flare for the guns (officer)"},
            {"v", "radioman: call the guns   sapper: satchel charge"}, {"b", "radioman: high explosive or smoke"},
            {"e", "treat, drag, cut wire, take a weapon or ammunition"}, {"f", "bandage yourself"},
            {"h", "the hill: dig in. Sandbags in front, or lying down, a foxhole"},
            {"tab, 1 to 5", "orders, when you lead"},
        };
        float y = 172.0f;
        for(const auto& k : keys)
        {
            shadowed(t, f, k[0], 16, {W * 0.5f - 12.0f, y}, TEXT, 1.0f);
            shadowed(t, f, k[1], 16, {W * 0.5f + 12.0f, y}, TEXT_DIM);
            y += 19.0f;
        }
        char line[96];
        std::snprintf(line, sizeof line, "%s, %s", level_name(m_level), difficulty_name(Campaign::current().difficulty));
        shadowed(t, f, line, 16, {W * 0.5f, H - 40}, TEXT_FAINT, 0.5f);
    }
}

}
