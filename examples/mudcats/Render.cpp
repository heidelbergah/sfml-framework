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
 * Roofs hide what's under them until he's close; then they open up and
 * the same line of sight decides what he sees inside, through doors and
 * windows. Men he can hear and not see, a shot or a shout or boots close
 * by, leave a faint ring where the sound came from.
 *
 * Draw order, bottom to top: the ground (with everything stamped into
 * it), weapons lying about, men, blood and earth in the air, the grass
 * tops and roofs and tree crowns, smoke, fire, sparks, rounds, rain and
 * light.
 */

#include "Battle.hpp"

#include "Draw.hpp"
#include "Icons.hpp"
#include "Palette.hpp"
#include "Sound.hpp"

#include "../../include/core/Random.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <vector>

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
        // Under a roof that's still shut, from where you are, nobody can be seen. Under one that has
        // opened up, it's line of sight like anywhere else: through the door, through a window.
        bool const roofed = m_map.under_roof(s.pos);
        bool const shut = roofed && m_map.roof_open_at(s.pos, me.pos) < 0.5f;
        if(shut && length(s.pos - me.pos) > 30.0f) { listen(s, length(s.pos - me.pos)); continue; }
        if(s.flash > 0.0f && length(s.pos - me.pos) < vision_range() * 1.5f && (!roofed || m_map.line_of_sight(me.pos, s.pos))) { v = 1; continue; }
        float const d = length(s.pos - me.pos);
        if(d < 380.0f && can_spot(me, s, d)) { v = 1; continue; }
        // What a comrade nearby can see, you are told about.
        for(const auto& o : m_soldiers)
        {
            if(o.team != me.team || !o.alive()) continue;
            float const od = length(o.pos - s.pos);
            if(od < 160.0f && can_spot(o, s, od)) { v = 1; break; }
        }
        if(!v) listen(s, d);
    }
    // Old sounds fade; a man you can see now needs no mark.
    std::erase_if(m_heard, [&](const Heard& h)
    {
        auto const i = static_cast<std::size_t>(h.id);
        return h.until < m_real || (i < m_visible.size() && m_visible[i]);
    });
}

void Battle::listen(const Soldier& s, float d)
{
    // What carries: a shot a long way, a shout less, a man moving only close by,
    // and a man running on boards or through a doorway further than one creeping.
    float reach = 0.0f, rough = 0.0f;
    if(s.flash > 0.0f) { reach = 320.0f; rough = 8.0f; }
    else if(s.bark_time > 0.0f) { reach = 240.0f; rough = 14.0f; }
    else
    {
        float const speed = length(s.vel);
        if(speed > 34.0f) { reach = m_map.under_roof(s.pos) ? 150.0f : 110.0f; rough = 12.0f; }
        else if(speed > 12.0f && s.stance != Stance::PRONE) { reach = 70.0f; rough = 12.0f; }
    }
    if(reach <= 0.0f || d > reach || !me().alive()) return;
    auto it = std::find_if(m_heard.begin(), m_heard.end(), [&](const Heard& h) { return h.id == s.id; });
    if(it == m_heard.end())
    {
        m_heard.push_back({s.id, s.pos + sf::Vector2f{Random::real(-rough, rough), Random::real(-rough, rough)}, m_real, m_real + 1.6f});
        return;
    }
    // Heard again: the mark stays put a moment, then moves to where he is now, roughly.
    it->until = m_real + 1.6f;
    if(m_real - it->since > 0.8f)
    {
        it->pos = s.pos + sf::Vector2f{Random::real(-rough, rough), Random::real(-rough, rough)};
        it->since = m_real;
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
        float const mist = m_level == LevelId::CHARGE ? 0.22f : (m_level == LevelId::FOREST ? 0.08f : 0.0f);   // the charge: a grey morning
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
        m_grade->setUniform("rain", 0.0f);
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

    if(m_level == LevelId::FOREST) draw_wagons(va);

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
        else if(s->team == me.team && s->alive() && s->leader && player_leads())
        {
            // Corporals, when you are giving the orders.
            sf::Vector2f const p = s->pos + sf::Vector2f{0.0f, -8.0f};
            bool const picked = m_selected < 0 || m_selected == s->section;
            draw::rect(va, std::floor(p.x), std::floor(p.y), 1, 1, with_alpha(MON_WHITE, picked ? 0.8f : 0.3f));
        }
        else if(s->team == me.team && s->life == Life::DOWNED && std::fmod(m_real, 1.0f) < 0.5f)
        {
            // A wounded man calling: a small red cross.
            sf::Vector2f const p = s->pos + sf::Vector2f{0.0f, -7.0f};
            draw::rect(va, std::floor(p.x) - 1, std::floor(p.y), 3, 1, ALERT);
            draw::rect(va, std::floor(p.x), std::floor(p.y) - 1, 1, 3, ALERT);
        }
    }

    // The regiment's colours, over the man carrying them: a pole and a red flag
    // streaming back over his shoulder as he runs.
    for(const Soldier* s : order)
    {
        if(!s->colours || !s->alive()) continue;
        sf::Vector2f const base = s->pos + sf::Vector2f{1.0f, -2.0f};
        sf::Vector2f const top = base + sf::Vector2f{0.0f, -15.0f};
        draw::thick_line(va, base, top, 1.0f, WOOD);
        float const flutter = std::sin(m_time * 9.0f + s->pos.y) * 1.2f;
        float const back = s->vel.x < -5.0f ? 1.0f : (s->vel.x > 5.0f ? -1.0f : 1.0f);
        for(int k = 0; k < 9; ++k)
        {
            float const kk = static_cast<float>(k);
            float const wave = std::sin(m_time * 10.0f - kk * 0.8f) * 1.1f + flutter * kk / 9.0f;
            draw::rect(va, std::floor(top.x + back * kk), std::floor(top.y + wave), 1, 6, k == 4 ? lerp(RCH_RED, PAPER, 0.5f) : RCH_RED);
        }
        draw::rect(va, std::floor(top.x) - 1, std::floor(top.y) - 1, 2, 1, MON_GOLD);
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
        sf::Color const coat = g.charred ? CHAR : (g.team == Team::MONTERAN ? MON_TUNIC : RCH_TAN);
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
    m_shade.draw(va, sf::RenderStates(sf::BlendNone));
    va.clear();
    // A shut roof is seen from above, whatever is behind its walls; as it opens up close by,
    // what's under it is as dark as your line of sight leaves it.
    for(const auto& r : m_map.roofs)
    {
        float const shut = 1.0f - Map::roof_open(r, eye);
        if(shut <= 0.0f) continue;
        auto const g = static_cast<std::uint8_t>(std::round(255.0f * shut));
        draw::rect(va, r.position.x - 6, r.position.y - 6, r.size.x + 12, r.size.y + 12, sf::Color(g, g, g));
    }
    m_shade.draw(va, sf::RenderStates(sf::BlendMax));
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

    // Somebody heard and not seen: a ring that swells and fades where the sound came from.
    for(const auto& h : m_heard)
    {
        sf::Vector2f const p = to_screen(h.pos);
        if(p.x < 0 || p.x > W || p.y < 0 || p.y > H) continue;
        float const age = m_real - h.since;
        float const fade = std::clamp((h.until - m_real) / 0.6f, 0.0f, 1.0f);
        float const r = 6.0f + 8.0f * std::min(1.0f, age / 0.5f);
        sf::CircleShape ring(r, 20);
        ring.setOrigin({r, r});
        ring.setPosition(p);
        ring.setFillColor(sf::Color::Transparent);
        ring.setOutlineThickness(2.0f);
        ring.setOutlineColor(with_alpha(PAPER, 0.75f * fade * (1.0f - 0.5f * std::min(1.0f, age / 0.8f))));
        t.draw(ring);
        sf::RectangleShape dot({2.0f, 2.0f});
        dot.setPosition(p - sf::Vector2f{1.0f, 1.0f});
        dot.setFillColor(with_alpha(PAPER, 0.85f * fade));
        t.draw(dot);
    }

    // Where the objective is: the next point the platoon is making for.
    sf::Vector2f goal = objective_point();
    if(m_level == LevelId::CHARGE) goal = me.team == Team::MONTERAN ? sf::Vector2f{410.0f, me.pos.y} : sf::Vector2f{410.0f, me.file_y > 0.0f ? me.file_y : me.pos.y};

    // Wounded men of ours, at the edge of the screen when they are off it:
    // a medic needs to know where they are; anyone can drag them.
    {
        sf::VertexArray crosses(sf::PrimitiveType::Triangles);
        sf::FloatRect const edge({30.0f, 60.0f}, {W - 60.0f, H - 170.0f});
        for(const auto& s : m_soldiers)
        {
            if(s.team != me.team || s.life != Life::DOWNED || s.is_player) continue;
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
                shadowed(t, f, tr("hud.mortar"), 16, p + sf::Vector2f{0.0f, 20.0f}, c, 0.5f);
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
                shadowed(t, f, tr("hud.mortar"), 16, q + sf::Vector2f{0.0f, 12.0f}, c, 0.5f);
            }
        }
        t.draw(marks);
    }

    // Once you're there, there's nothing to point at.
    for(const sf::FloatRect* area : {&m_map.front_trench_area, &m_map.church_area, &m_map.road_area})
        if(area->size.x > 0.0f && area->contains(me.pos) && area->contains(goal)) return;

    sf::Vector2f const sp = to_screen(goal);
    float const dist = length(goal - me.pos) / 8.0f;          // 8 px to the yard, roughly
    sf::FloatRect const inner({40.0f, 70.0f}, {W - 80.0f, H - 190.0f});
    sf::VertexArray va(sf::PrimitiveType::Triangles);
    std::string const buf = tr("hud.yards", {{"n", std::to_string(static_cast<int>(dist))}});
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
    put(-1.5708f, tr("hud.compass.n"), TEXT); put(0.0f, tr("hud.compass.e"), TEXT_DIM);
    put(1.5708f, tr("hud.compass.s"), TEXT_DIM); put(3.14159f, tr("hud.compass.w"), TEXT_DIM);
    put(-0.785f, tr("hud.compass.ne"), TEXT_FAINT); put(0.785f, tr("hud.compass.se"), TEXT_FAINT);
    put(2.356f, tr("hud.compass.sw"), TEXT_FAINT); put(-2.356f, tr("hud.compass.nw"), TEXT_FAINT);
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
    if(!m_objective.empty()) shadowed(t, f, m_objective, 16, {16, 12}, TEXT);

    // How a man is, as a colour: the same everywhere on the HUD.
    auto state_colour = [](const Soldier& s)
    {
        if(s.life == Life::DEAD || s.life == Life::FLED) return with_alpha(TEXT_FAINT, 0.55f);
        if(s.life == Life::DOWNED) return ALERT;
        if(s.burning > 0.0f) return FIRE_ORANGE;
        if(s.hp < 60.0f || s.lost) return lerp(ALERT, MON_GOLD, 0.5f);
        if(s.suppression > 0.75f) return PAPER_DK;
        return MON_WHITE;
    };

    // --- the platoon, top right: each section a row of men, each man his role ---------------
    {
        constexpr float PITCH = 16.0f;
        float const right = W - 14.0f;
        float y = 12.0f;
        bool const theirs = me.team == Team::RECHIVII;

        std::vector<const Section*> shown;
        for(const auto& sec : m_sections)
            if(sec.team == me.team && (!theirs || sec.id == me.section)) shown.push_back(&sec);
        int widest = 1;
        for(const Section* sec : shown)
        {
            int n = 0;
            for(const auto& s : m_soldiers) if(s.team == me.team && s.section == sec->id) ++n;
            widest = std::max(widest, n);
        }
        float const x0 = right - PITCH * static_cast<float>(widest) + 2.0f;

        sf::VertexArray va(sf::PrimitiveType::Triangles);
        auto man = [&](const Soldier& s, float x)
        {
            sf::Color const c = state_colour(s);
            icon(va, s.life == Life::DEAD ? ICON_DEAD : role_icon(s.role), {x, y}, c);
            if(s.leader && s.alive() && s.role != Role::OFFICER) draw::rect(va, x + 4.0f, y - 3.0f, 6.0f, 2.0f, c);
            if(s.is_player) draw::rect(va, x, y + 16.0f, 14.0f, 2.0f, MON_GOLD);
        };

        // The officer over them all.
        if(!theirs)
            for(const auto& s : m_soldiers)
                if(s.team == me.team && s.role == Role::OFFICER) { man(s, x0); y += 24.0f; break; }

        for(const Section* sec : shown)
        {
            bool const picked = (leading && (m_selected < 0 || m_selected == sec->id)) || (player_commands_section() && me.section == sec->id);
            bool const yours = me.section == sec->id;
            if(picked && fighting_started())
                box(t, {x0 - 40.0f, y - 5.0f}, {right - x0 + 46.0f, 24.0f}, with_alpha(MON_GOLD, 0.10f));
            float x = x0;
            int alive = 0;
            for(const auto& s : m_soldiers)
            {
                if(s.team != me.team || s.section != sec->id) continue;
                if(s.alive()) ++alive;
                man(s, x);
                x += PITCH;
            }
            // The section's number, and what it has been told to do.
            std::string const number = sec->name.substr(0, sec->name.find(' '));
            bool const numbered = !number.empty() && std::isdigit(static_cast<unsigned char>(number[0]));
            if(numbered) shadowed(t, f, number, 16, {x0 - 8.0f, y}, yours ? MON_GOLD : (alive ? TEXT : TEXT_FAINT), 1.0f);
            if(!theirs && alive && fighting_started())
                icon(va, order_icon(sec->order), {x0 - 36.0f, y}, with_alpha(TEXT_DIM, 0.9f));
            y += 24.0f;
        }
        t.draw(va);

        if(theirs)
        {
            // A company is too many rows: how many of it are still going. In
            // the town, how many of them are still coming.
            int going = 0, stepped = 0;
            bool const town = m_level == LevelId::TOWN;
            for(const auto& s : m_soldiers)
            {
                if(town ? s.team != Team::MONTERAN : (s.team != Team::RECHIVII || s.line < 0)) continue;
                ++stepped;
                if(s.fighting() && (s.morale > 0.0f || town)) ++going;
            }
            std::string const line = tr("hud.charge_count", {{"going", std::to_string(going)}, {"stepped", std::to_string(stepped)}});
            float const tw = text_width(f, line, 16);
            sf::VertexArray mark(sf::PrimitiveType::Triangles);
            icon(mark, town ? ICON_RIFLEMAN : ICON_CHARGE, {right - tw - 22.0f, y}, town ? MON_BLUE : RCH_TAN);
            t.draw(mark);
            shadowed(t, f, line, 16, {right, y + 1.0f}, TEXT_DIM, 1.0f);
        }
    }

    // --- your man, bottom left: how he stands, how he is, what he has -----------------------
    {
        const WeaponSpec& w = me.weapon();
        float const bx = 16.0f, by = H - 64.0f;
        float const tx = bx + 40.0f;
        sf::VertexArray va(sf::PrimitiveType::Triangles);

        // Health, across the top: a bar, a drop when you're bleeding, a flame when you burn.
        float const hp = std::clamp(me.hp / 100.0f, 0.0f, 1.0f);
        bool const blink = std::fmod(m_real, 0.6f) < 0.3f;
        draw::rect(va, bx, by + 5.0f, 110.0f, 4.0f, with_alpha(INK, 0.9f));
        draw::rect(va, bx, by + 5.0f, std::round(110.0f * hp), 4.0f,
                   me.bleeding > 0.0f && blink ? BLOOD_BRIGHT : (hp < 0.4f ? ALERT : lerp(ALERT, MON_WHITE, hp)));
        float sx = bx + 118.0f;
        if(me.bleeding > 0.0f || me.life == Life::DOWNED) { icon(va, ICON_DROP, {sx, by}, blink ? BLOOD_BRIGHT : ALERT); sx += 18.0f; }
        if(me.burning > 0.0f) icon(va, ICON_FIRE, {sx, by}, FIRE_ORANGE);

        // How he stands, as a picture of it.
        const char* const* rows = me.life == Life::DOWNED ? DOWN_ICON
                                : me.stance == Stance::STAND ? STAND_ICON : me.stance == Stance::CROUCH ? KNEEL_ICON : PRONE_ICON;
        glyph(va, rows, 10, {bx, by + 18.0f}, 3.0f, me.life == Life::DOWNED ? ALERT : (me.stance_change > 0.0f ? TEXT_DIM : MON_WHITE));

        // The weapon: what is in it, drawn the way that gun holds its rounds.
        float const wy = by + 18.0f;
        float const filled = me.reloading > 0.0f ? std::clamp(1.0f - me.reloading / std::max(0.1f, w.reload), 0.0f, 1.0f) : 1.0f;
        int const shown_rounds = me.reloading > 0.0f ? static_cast<int>(std::floor(filled * static_cast<float>(w.magazine))) : me.ammo;
        sf::Color const brass = me.reloading > 0.0f ? lerp(TEXT_FAINT, MON_GOLD, 0.5f) : MON_GOLD;
        sf::Color const spent = with_alpha(TEXT_FAINT, 0.3f);
        float ax = tx;
        switch(me.gun)
        {
            case Gun::RIFLE:
            case Gun::SNIPER:
                // Five rounds in a clip, standing up.
                for(int i = 0; i < w.magazine; ++i)
                {
                    bool const in = i < shown_rounds;
                    draw::rect(va, ax, wy + 3.0f, 4.0f, 9.0f, in ? brass : spent);
                    if(in) draw::rect(va, ax + 1.0f, wy, 2.0f, 3.0f, lerp(MON_GOLD, FIRE_ORANGE, 0.6f));
                    ax += 7.0f;
                }
                draw::rect(va, tx - 1.0f, wy + 12.0f, ax - tx, 2.0f, with_alpha(TEXT_FAINT, 0.6f));
                break;
            case Gun::PISTOL:
                // A short magazine of short rounds.
                for(int i = 0; i < w.magazine; ++i)
                {
                    draw::rect(va, ax, wy + 6.0f, 3.0f, 6.0f, i < shown_rounds ? brass : spent);
                    ax += 5.0f;
                }
                break;
            case Gun::SMG:
                // The stick magazine, side on, rounds stacked in it.
                draw::rect(va, ax, wy + 2.0f, 2.0f * static_cast<float>(w.magazine) + 3.0f, 11.0f, with_alpha(INK, 0.9f));
                for(int i = 0; i < w.magazine; ++i)
                    draw::rect(va, ax + 2.0f + 2.0f * static_cast<float>(i), wy + 4.0f, 1.0f, 7.0f, i < shown_rounds ? brass : spent);
                ax += 2.0f * static_cast<float>(w.magazine) + 6.0f;
                break;
            case Gun::MG:
            {
                // The belt, a link for every two rounds, and how hot the barrel is under it.
                int const links = w.magazine / 2;
                for(int i = 0; i < links; ++i)
                    draw::rect(va, ax + 2.0f * static_cast<float>(i), wy + 2.0f, 1.0f, 7.0f, i * 2 < shown_rounds ? brass : spent);
                float const len = 2.0f * static_cast<float>(links);
                draw::rect(va, ax, wy + 11.0f, len, 2.0f, with_alpha(INK, 0.9f));
                draw::rect(va, ax, wy + 11.0f, std::round(len * std::min(1.0f, me.heat)), 2.0f,
                           me.overheated > 0.0f ? (blink ? ALERT : FIRE_ORANGE) : lerp(MON_GOLD, FIRE_ORANGE, me.heat));
                ax += len + 6.0f;
                sf::Color const legs = me.deployed ? MON_WHITE : (me.deploying > 0.0f ? (blink ? MON_WHITE : TEXT_FAINT) : with_alpha(TEXT_FAINT, 0.5f));
                icon(va, ICON_BIPOD, {ax, wy}, legs);
                ax += 18.0f;
                break;
            }
            case Gun::FLAMER:
            {
                // The two tanks on his back, emptying from the top.
                float const fuel = std::clamp(me.fuel / 100.0f, 0.0f, 1.0f);
                for(int k = 0; k < 2; ++k)
                {
                    float const x = ax + static_cast<float>(k) * 10.0f;
                    draw::rect(va, x, wy, 8.0f, 14.0f, with_alpha(INK, 0.9f));
                    float const h = std::round(12.0f * fuel);
                    draw::rect(va, x + 1.0f, wy + 13.0f - h, 6.0f, h, fuel < 0.2f && blink ? ALERT : FIRE_ORANGE);
                }
                ax += 26.0f;
                break;
            }
            default:
                break;
        }
        bool const has_reserve = me.gun != Gun::NONE && me.gun != Gun::FLAMER;
        if(has_reserve) ax += 3.0f;

        // Everything else he carries, a picture and how many.
        struct Kit { const char* const* rows; sf::Color colour; int count; };
        std::vector<Kit> kit;
        kit.reserve(8);
        if(me.grenades)  kit.push_back({ICON_GRENADE, lerp(MON_WHITE, SANDBAG, 0.4f), me.grenades});
        if(me.smokes)    kit.push_back({ICON_SMOKE, TEXT_DIM, me.smokes});
        if(me.flares)    kit.push_back({ICON_FLARE, ALERT, me.flares});
        if(me.satchels)  kit.push_back({ICON_SATCHEL, SANDBAG, me.satchels});
        if(me.role == Role::RADIOMAN) kit.push_back({ICON_RADIO, MON_WHITE, me.missions});
        if(me.role == Role::MEDIC) kit.push_back({ICON_KIT, MON_WHITE, me.medkits});
        else if(me.bandages) kit.push_back({ICON_BANDAGE, MON_WHITE, me.bandages});

        float kx = tx;
        float const ky = by + 34.0f;
        std::vector<std::pair<std::string, float>> counts;
        for(const Kit& k : kit)
        {
            icon(va, k.rows, {kx, ky}, k.count ? k.colour : with_alpha(k.colour, 0.35f));
            kx += 16.0f;
            std::string const n = std::to_string(k.count);
            counts.push_back({n, kx});
            kx += text_width(f, n, 16) + 10.0f;
            // The radio's next call: a shell for high explosive, a cloud for smoke.
            if(k.rows == ICON_RADIO)
            {
                icon(va, m_smoke_mission ? ICON_SMOKE : ICON_SHELL, {kx - 4.0f, ky}, m_smoke_mission ? TEXT_DIM : MON_GOLD);
                kx += 18.0f;
            }
        }

        // A shadow behind it all, as wide as it needs to be.
        std::string const reserve = std::to_string(me.spare);
        float const width = std::max({bx + 138.0f, ax + (has_reserve ? text_width(f, reserve, 16) + 4.0f : 0.0f), kx}) - bx + 12.0f;
        box(t, {bx - 8.0f, by - 6.0f}, {width, 66.0f}, with_alpha(INK, 0.45f));
        t.draw(va);
        if(has_reserve) shadowed(t, f, reserve, 16, {ax, wy + 1.0f}, me.spare == 0 && me.ammo == 0 ? ALERT : TEXT_DIM);
        else if(me.gun == Gun::NONE) shadowed(t, f, "-", 16, {ax, wy + 1.0f}, ALERT);
        for(const auto& [n, x] : counts) shadowed(t, f, n, 16, {x, ky + 1.0f}, TEXT_DIM);

        // The radio, when a call is going.
        for(const auto& m : m_missions)
        {
            if(m.caller != me.id) continue;
            std::string line = item("hud.radio_steps", static_cast<std::size_t>(std::clamp(m.step, 0, 3)));
            if(m.step == 2) line += "  " + std::to_string(static_cast<int>(std::ceil(m.timer)));
            shadowed(t, f, line, 16, {bx - 2.0f, by - 26.0f}, std::fmod(m_real, 0.8f) < 0.5f ? MON_GOLD : TEXT_DIM);
        }
    }

    // --- what E would do, over your head ---------------------------------------------------
    if(std::string const prompt = interact_prompt(); !prompt.empty())
    {
        sf::Vector2f const p = to_screen(me.pos) + sf::Vector2f{0.0f, 26.0f};
        shadowed(t, f, prompt, 16, p, with_alpha(MON_WHITE, 0.9f), 0.5f);
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
        std::string const line = tr("hud.bleeding_out", {{"n", std::to_string(static_cast<int>(std::ceil(me.bleed_out)))}});
        shadowed(t, f, line, 32, {W * 0.5f, H * 0.5f - 60}, ALERT, 0.5f);
        shadowed(t, f, tr("hud.down_keys"), 16, {W * 0.5f, H * 0.5f - 22}, TEXT_DIM, 0.5f);
    }
    if(m_takeover > 0.0f)
    {
        float const a = std::min(1.0f, m_takeover);
        shadowed(t, f, m_takeover_text, 32, {W * 0.5f, H * 0.5f - 70}, with_alpha(TEXT, a), 0.5f);
    }

    // --- orders, bottom right, when they are yours to give: the five, and their keys ----------
    if((leading || player_commands_section()) && me.alive() && fighting_started())
    {
        static const Order orders[] = {Order::FOLLOW, Order::HOLD, Order::ADVANCE, Order::SUPPRESS, Order::CHARGE};
        sf::VertexArray va(sf::PrimitiveType::Triangles);
        float x = W - 14.0f - 5.0f * 26.0f + 12.0f;
        float const y = H - 44.0f;
        for(int i = 0; i < 5; ++i)
        {
            icon(va, order_icon(orders[i]), {x + static_cast<float>(i) * 26.0f, y}, TEXT_DIM);
            shadowed(t, f, std::to_string(i + 1), 16, {x + static_cast<float>(i) * 26.0f + 7.0f, y + 17.0f}, TEXT_FAINT, 0.5f);
        }
        t.draw(va);
        if(leading) shadowed(t, f, tr("hud.tab"), 16, {x - 12.0f, y + 17.0f}, TEXT_FAINT, 1.0f);
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
        std::string const ending = tr(std::string("levels.") + level_key(m_level) + ".ending." + side_key(me.team) + (m_stage == Stage::WON ? "_won" : "_lost"));
        shadowed(t, f, ending, 32, {W * 0.5f, 150}, with_alpha(TEXT, a), 0.5f);
    }

    if(m_paused)
    {
        box(t, {0, 0}, {W, H}, with_alpha(INK, 0.88f));
        shadowed(t, f, tr("pause.title"), 32, {W * 0.5f, 70}, TEXT, 0.5f);

        // Sound, on the left: drag a bar, or up and down to pick one and left and right to change it.
        auto& bank = SoundBank::get();
        shadowed(t, f, tr("pause.sound"), 16, {sound_bar(0).position.x, 156}, TEXT_FAINT);
        for(int i = 0; i < SoundBank::BUSES; ++i)
        {
            sf::FloatRect const bar = sound_bar(i);
            float const v = bank.volume(static_cast<SoundBank::Bus>(i));
            bool const on = i == m_sound_row;
            shadowed(t, f, item("settings.buses", static_cast<std::size_t>(i)), 16, {bar.position.x - 14.0f, bar.position.y - 5.0f}, on ? MON_GOLD : TEXT_DIM, 1.0f);
            box(t, bar.position, bar.size, with_alpha(PAPER_DK, 0.25f));
            box(t, bar.position, {std::round(bar.size.x * v), bar.size.y}, on ? MON_GOLD : MON_WHITE);
            box(t, {bar.position.x + std::round(bar.size.x * v) - 2.0f, bar.position.y - 4.0f}, {4.0f, bar.size.y + 8.0f}, on ? MON_GOLD : MON_WHITE);
            shadowed(t, f, std::to_string(static_cast<int>(std::round(v * 100.0f))), 16, {bar.position.x + bar.size.x + 14.0f, bar.position.y - 5.0f}, TEXT_FAINT);
        }
        if(bank.music_muted()) shadowed(t, f, tr("pause.music_off"), 16, {sound_bar(0).position.x, sound_bar(3).position.y + 30.0f}, ALERT);

        // The keys, on the right.
        float y = 156.0f;
        float const kx = W * 0.5f + 150.0f;
        for(std::size_t i = 0; has_text("controls.pause." + std::to_string(i)); ++i)
        {
            const auto& k = list("controls.pause." + std::to_string(i));
            if(k.size() < 2) continue;
            shadowed(t, f, k[0], 16, {kx - 12.0f, y}, TEXT, 1.0f);
            shadowed(t, f, k[1], 16, {kx + 12.0f, y}, TEXT_DIM);
            y += 19.0f;
        }
        shadowed(t, f, tr("pause.keys"), 16, {W * 0.5f, H - 64}, TEXT_DIM, 0.5f);
        std::string const line = tr("pause.where", {{"level", level_name(m_level)}, {"difficulty", difficulty_name(Campaign::current().difficulty)}});
        shadowed(t, f, line, 16, {W * 0.5f, H - 40}, TEXT_FAINT, 0.5f);
    }
}

}
