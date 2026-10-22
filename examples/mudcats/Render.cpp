/**
 * Render.cpp  -  Mud Cats
 *
 * The frame is drawn at 480x270 through the camera, then graded
 * (shaders/mudcats/grade.frag) and scaled up twice. The HUD is drawn
 * afterwards at full resolution, so text stays crisp.
 *
 * What the player cannot see -- behind walls, relative to where he
 * stands -- is darkened, and enemies standing in it are not drawn at all.
 * Their muzzle flashes still are.
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

    void box(sf::RenderTarget& t, sf::Vector2f p, sf::Vector2f s, sf::Color c)
    {
        sf::RectangleShape r(s);
        r.setPosition({std::round(p.x), std::round(p.y)});
        r.setFillColor(c);
        t.draw(r);
    }
}

bool Battle::visible_to_player(const Soldier& s) const
{
    const Soldier& me = m_soldiers[static_cast<std::size_t>(std::max(0, m_player))];
    if(s.team == me.team) return true;
    if(s.flash > 0.0f && !m_map.under_roof(s.pos)) return true;
    if(m_map.under_roof(s.pos) && !m_map.under_roof(me.pos)) return false;
    if(length(s.pos - me.pos) < 380.0f && m_map.line_of_sight(me.pos, s.pos)) return true;
    // What a comrade nearby can see, you are told about.
    for(const auto& o : m_soldiers)
        if(o.team == me.team && o.alive() && length(o.pos - s.pos) < 160.0f && m_map.line_of_sight(o.pos, s.pos)) return true;
    return false;
}

void Battle::flush_stamps()
{
    if(m_stamps.getVertexCount() == 0) return;
    m_ground.setView(m_ground.getDefaultView());
    m_ground.draw(m_stamps);
    m_ground.display();
    m_stamps.clear();
}

void Battle::render(sf::RenderTarget& target)
{
    if(!m_ready) return;
    flush_stamps();

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
    const Soldier& me = m_soldiers[static_cast<std::size_t>(std::max(0, m_player))];
    sf::Sprite frame(m_frame.getTexture());
    frame.setScale({2.0f, 2.0f});
    target.setView(target.getDefaultView());
    if(m_grade)
    {
        m_grade->setUniform("texture", sf::Shader::CurrentTexture);
        m_grade->setUniform("resolution", sf::Glsl::Vec2{FRAME_F.x, FRAME_F.y});
        m_grade->setUniform("time", m_real);
        m_grade->setUniform("suppression", me.alive() ? me.suppression : 0.0f);
        m_grade->setUniform("hurt", std::min(1.0f, m_hurt + (me.alive() && me.hp < 50.0f ? (50.0f - me.hp) / 80.0f : 0.0f)));
        m_grade->setUniform("flash", m_flash);
        m_grade->setUniform("shock", me.shock);
        m_grade->setUniform("dying", me.life == Life::DOWNED ? std::clamp(1.0f - me.bleed_out / 32.0f, 0.3f, 1.0f) : 0.0f);
        target.draw(frame, sf::RenderStates(m_grade));
    }
    else target.draw(frame);

    draw_hud(target);
}

void Battle::draw_world(sf::RenderTarget& t, const sf::View& view)
{
    sf::FloatRect const seen(view.getCenter() - view.getSize() * 0.5f - sf::Vector2f{30, 30}, view.getSize() + sf::Vector2f{60, 60});
    sf::VertexArray va(sf::PrimitiveType::Triangles);

    // Bottles on the range.
    for(const auto& b : m_map.bottles)
        if(!b.broken)
        {
            draw::rect(va, b.pos.x - 1, b.pos.y - 3, 2, 4, lerp(GRASS, MON_WHITE, 0.3f));
            draw::rect(va, b.pos.x - 1, b.pos.y - 4, 1, 1, MON_WHITE);
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
        if(s->life == Life::DOWNED && s->life != Life::DEAD)
            draw::disc(va, s->pos.x + 1, s->pos.y + 1, std::min(6.0f, 2.0f + (32.0f - s->bleed_out) * 0.15f), BLOOD_DK);
        draw_soldier(va, *s, m_time);
        if(s->is_player && s->alive())
        {
            // A small marker so you can find yourself in a crowd.
            sf::Vector2f const p = s->pos + sf::Vector2f{0.0f, -9.0f};
            draw::rect(va, std::floor(p.x) - 1, std::floor(p.y), 3, 1, with_alpha(MON_WHITE, 0.7f));
            draw::rect(va, std::floor(p.x), std::floor(p.y) + 1, 1, 1, with_alpha(MON_WHITE, 0.7f));
        }
    }

    // Grenades, limbs and blood in flight.
    for(const auto& g : m_grenades)
    {
        sf::Vector2f const p = g.pos - sf::Vector2f{0.0f, g.height};
        draw::rect(va, std::floor(p.x), std::floor(p.y), 2, 2, lerp(GUNMETAL, GRASS, 0.3f));
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
    for(const auto& d : m_drops)
        draw::rect(va, std::floor(d.pos.x), std::floor(d.pos.y - d.height), d.size, d.size, d.dirt ? MUD_LIGHT : BLOOD_BRIGHT);
    t.draw(va);
    va.clear();

    m_map.draw_roofs(t, m_soldiers[static_cast<std::size_t>(std::max(0, m_player))].pos);

    // Smoke, dust and fire.
    for(const auto& p : m_puffs)
    {
        if(!seen.contains(p.pos)) continue;
        float const k = p.life / p.max_life;
        sf::Color c = p.colour;
        c.a = static_cast<std::uint8_t>(static_cast<float>(c.a) * std::min(1.0f, k * 1.5f));
        float const s = std::round(p.size);
        draw::rect(va, std::floor(p.pos.x - s * 0.5f), std::floor(p.pos.y - s * 0.5f), s, s, c);
    }
    for(const auto& f : m_flames)
    {
        float const age = 1.0f - f.life / f.max_life;
        sf::Color const c = age < 0.15f ? FIRE_WHITE : age < 0.4f ? FIRE_YELLOW : age < 0.7f ? FIRE_ORANGE : with_alpha(FIRE_RED, 1.0f - age);
        float const s = std::round(2.0f + age * 4.0f);
        draw::rect(va, std::floor(f.pos.x - s * 0.5f), std::floor(f.pos.y - s * 0.5f), s, s, c);
    }
    t.draw(va);
    va.clear();

    // Rounds in flight.
    for(const auto& b : m_bullets)
    {
        sf::Vector2f const u = unit(b.vel);
        if(b.tracer) draw::thick_line(va, b.pos - u * 9.0f, b.pos, 1.0f, TRACER);
        else draw::thick_line(va, b.pos - u * 4.0f, b.pos, 1.0f, with_alpha(PAPER, 0.45f));
    }
    for(const auto& s : m_sparks) draw::rect(va, std::floor(s.pos.x), std::floor(s.pos.y), 1, 1, s.colour);
    t.draw(va);
    va.clear();

    // Light: additive glow for flashes and fire.
    for(const auto& l : m_lights)
    {
        if(!seen.contains(l.pos)) continue;
        float const k = l.life / l.max_life;
        sf::CircleShape glow(l.radius, 20);
        glow.setOrigin({l.radius, l.radius});
        glow.setPosition(l.pos);
        glow.setFillColor(with_alpha(l.colour, 0.18f * k));
        t.draw(glow, sf::BlendAdd);
        sf::CircleShape core(l.radius * 0.4f, 12);
        core.setOrigin({l.radius * 0.4f, l.radius * 0.4f});
        core.setPosition(l.pos);
        core.setFillColor(with_alpha(l.colour, 0.3f * k));
        t.draw(core, sf::BlendAdd);
    }
}

void Battle::draw_shade(const sf::View& view)
{
    const Soldier& me = m_soldiers[static_cast<std::size_t>(std::max(0, m_player))];
    sf::Vector2f const eye = me.pos;

    m_shade.setView(view);
    m_shade.clear(sf::Color::White);
    sf::VertexArray va(sf::PrimitiveType::Triangles);
    sf::Color const dark(118, 116, 124);

    for(const auto& w : m_map.walls)
    {
        sf::Vector2f const c = w.position + w.size * 0.5f;
        if(length(c - eye) > 520.0f) continue;
        sf::Vector2f const p[4] = {w.position, {w.position.x + w.size.x, w.position.y},
                                   w.position + w.size, {w.position.x, w.position.y + w.size.y}};
        for(int i = 0; i < 4; ++i)
        {
            sf::Vector2f const a = p[i], b = p[(i + 1) % 4];
            sf::Vector2f const fa = a + unit(a - eye) * 900.0f, fb = b + unit(b - eye) * 900.0f;
            draw::quad(va, a, b, fb, fa, dark);
        }
    }
    m_shade.draw(va, sf::RenderStates(sf::BlendNone));
    va.clear();
    for(const auto& w : m_map.walls) draw::rect(va, w.position.x, w.position.y, w.size.x, w.size.y, sf::Color::White);
    m_shade.draw(va, sf::RenderStates(sf::BlendNone));
    m_shade.display();

    m_frame.setView(m_frame.getDefaultView());
    m_frame.draw(sf::Sprite(m_shade.getTexture()), sf::BlendMultiply);
}

void Battle::draw_crosshair(sf::RenderTarget& frame)
{
    const Soldier& me = m_soldiers[static_cast<std::size_t>(std::max(0, m_player))];
    if(!me.alive() || m_paused) return;
    const WeaponSpec& w = weapon_of(me.role);

    float spread = w.spread;
    if(me.stance == Stance::CROUCH) spread *= 0.75f;
    if(me.stance == Stance::PRONE)  spread *= 0.55f;
    if(length(me.vel) > 8.0f) spread *= 2.4f;
    spread *= 1.0f + 2.2f * me.suppression + 1.5f * me.shock;
    if(m_aiming) spread *= 0.45f;
    spread += me.kick;

    float const d = length(m_mouse_world - me.pos);
    float const gap = std::clamp(spread * d * 1.6f, 2.0f, 30.0f);
    sf::Vector2f const c{std::round(m_mouse_world.x), std::round(m_mouse_world.y)};
    bool const ready = me.reloading <= 0.0f && me.cooldown <= 0.0f && (me.role != Role::GUNNER || me.deployed);
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
    if(me.reloading > 0.0f)
    {
        float const k = 1.0f - me.reloading / w.reload;
        draw::rect(va, c.x - 6, c.y + gap + 6, 12, 1, with_alpha(INK, 0.6f));
        draw::rect(va, c.x - 6, c.y + gap + 6, std::round(12.0f * k), 1, MON_WHITE);
    }
    // The machine gun's arc when it is set up.
    if(me.deployed)
        for(float k = -0.85f; k <= 0.85f; k += 0.85f)
        {
            sf::Vector2f const e = me.pos + dir(me.deploy_dir + k) * 26.0f;
            draw::rect(va, std::floor(e.x), std::floor(e.y), 1, 1, with_alpha(MON_WHITE, 0.4f));
        }
    frame.draw(va);
}

void Battle::draw_hud(sf::RenderTarget& t)
{
    if(!m_font) return;
    const sf::Font& f = *m_font;
    t.setView(t.getDefaultView());
    float const W = static_cast<float>(BASE_RESOLUTION.x), H = static_cast<float>(BASE_RESOLUTION.y);
    const Soldier& me = m_soldiers[static_cast<std::size_t>(std::max(0, m_player))];
    sf::View const view = m_camera.get_view();
    sf::Vector2f const origin = view.getCenter() - view.getSize() * 0.5f;
    auto to_screen = [&](sf::Vector2f p) { return (p - origin) * 2.0f; };

    // --- barks over heads -----------------------------------------------------
    for(const auto& s : m_soldiers)
    {
        if(s.bark_time <= 0.0f || s.bark.empty() || !visible_to_player(s)) continue;
        sf::Vector2f const p = to_screen(s.pos) + sf::Vector2f{0.0f, -40.0f - (1.6f - s.bark_time) * 8.0f};
        if(p.x < -50 || p.x > W + 50 || p.y < -20 || p.y > H) continue;
        float const a = std::min(1.0f, s.bark_time * 2.0f);
        shadowed(t, f, s.bark, 16, p, with_alpha(s.team == Team::MONTERAN ? MON_WHITE : RCH_TAN, a), 0.5f);
    }

    // --- objective -------------------------------------------------------------------
    if(!m_objective.empty())
    {
        shadowed(t, f, "OBJECTIVE", 16, {20, 16}, TEXT_FAINT);
        shadowed(t, f, m_objective, 16, {20, 34}, TEXT);
    }

    // --- the squad, top right -------------------------------------------------------------
    float y = 16.0f;
    shadowed(t, f, "3RD RIFLES, 2 SECTION", 16, {W - 20, y}, TEXT_FAINT, 1.0f);
    y += 20.0f;
    for(const auto& s : m_soldiers)
    {
        if(s.team != Team::MONTERAN) continue;
        std::string state;
        sf::Color c = TEXT_DIM;
        if(s.life == Life::DEAD)        { state = "dead";    c = TEXT_FAINT; }
        else if(s.life == Life::DOWNED) { state = "DOWN";    c = ALERT; }
        else if(s.burning > 0.0f)       { state = "BURNING"; c = FIRE_ORANGE; }
        else if(s.suppression > 0.75f)  { state = "pinned";  c = PAPER_DK; }
        else if(s.hp < 60.0f || s.lost) { state = "wounded"; c = lerp(ALERT, TEXT, 0.4f); }
        else                            { state = "";        c = TEXT; }
        std::string const who = (s.is_player ? "> " : "") + s.name.substr(s.name.find(' ') + 1);
        shadowed(t, f, who, 16, {W - 120, y}, s.is_player ? MON_GOLD : (s.life == Life::DEAD ? TEXT_FAINT : TEXT), 1.0f);
        shadowed(t, f, state, 16, {W - 20, y}, c, 1.0f);
        y += 17.0f;
    }

    // --- your man, bottom left ----------------------------------------------------------------
    const WeaponSpec& w = weapon_of(me.role);
    float const bx = 20.0f, by = H - 84.0f;
    box(t, {bx - 8, by - 8}, {250, 76}, with_alpha(INK, 0.55f));
    shadowed(t, f, me.rank + " " + me.name + "  -  " + role_name(me.role), 16, {bx, by}, MON_GOLD);
    std::string health = "unhurt";
    sf::Color hc = TEXT;
    if(me.life == Life::DOWNED) { health = "down, bleeding out"; hc = ALERT; }
    else if(me.bleeding > 0.0f) { health = "bleeding"; hc = ALERT; }
    else if(me.hp < 40.0f) { health = "badly hurt"; hc = ALERT; }
    else if(me.hp < 75.0f) { health = "wounded"; hc = lerp(ALERT, TEXT, 0.5f); }
    if(me.lost & (ARM_L | ARM_R)) health += ", lost an arm";
    if(me.lost & (LEG_L | LEG_R)) health += ", lost a leg";
    shadowed(t, f, health, 16, {bx, by + 18}, hc);

    std::string kit;
    char buf[96];
    if(w.flame) std::snprintf(buf, sizeof buf, "fuel %d%%", static_cast<int>(me.fuel));
    else if(me.reloading > 0.0f) std::snprintf(buf, sizeof buf, "reloading...   %d spare", me.spare);
    else std::snprintf(buf, sizeof buf, "%d / %d", me.ammo, me.spare);
    kit = buf;
    if(me.grenades) kit += "   grenades " + std::to_string(me.grenades);
    if(me.role == Role::MEDIC) kit += "   kits " + std::to_string(me.medkits);
    else if(me.bandages) kit += "   bandage";
    shadowed(t, f, kit, 16, {bx, by + 36}, TEXT_DIM);

    std::string const stance = me.stance == Stance::STAND ? "standing" : me.stance == Stance::CROUCH ? "crouched" : "prone";
    shadowed(t, f, stance + (me.deployed ? ", gun set up" : (me.deploying > 0.0f ? ", setting up..." : "")), 16, {bx + 140, by + 18}, TEXT_FAINT);

    if(me.role == Role::GUNNER)
    {
        box(t, {bx + 140, by + 42}, {90, 6}, with_alpha(INK, 0.8f));
        box(t, {bx + 140, by + 42}, {std::round(90.0f * std::min(1.0f, me.heat)), 6}, me.overheated > 0.0f ? ALERT : lerp(MON_GOLD, FIRE_ORANGE, me.heat));
    }
    if(w.flame)
    {
        box(t, {bx + 140, by + 42}, {90, 6}, with_alpha(INK, 0.8f));
        box(t, {bx + 140, by + 42}, {std::round(90.0f * me.fuel / 100.0f), 6}, FIRE_ORANGE);
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

    // --- officer's orders -----------------------------------------------------------------------
    bool const leading = me.role == Role::OFFICER || std::none_of(m_soldiers.begin(), m_soldiers.end(),
        [](const Soldier& o) { return o.team == Team::MONTERAN && o.role == Role::OFFICER && o.alive(); });
    if(leading && me.alive() && m_stage >= Stage::ASSAULT && m_stage < Stage::WON)
        shadowed(t, f, "1 follow   2 hold   3 advance   4 covering fire", 16, {W - 20, H - 26}, TEXT_FAINT, 1.0f);

    // --- dialogue and hints -------------------------------------------------------------------------
    if(!m_lines.empty())
    {
        const Line& l = m_lines.front();
        float const a = std::min(1.0f, l.time * 2.0f);
        float const ly = H - 140.0f;
        if(!l.who.empty()) shadowed(t, f, l.who, 16, {W * 0.5f, ly}, with_alpha(l.team == Team::MONTERAN ? MON_GOLD : RCH_TAN, a), 0.5f);
        shadowed(t, f, l.text, 16, {W * 0.5f, ly + 18}, with_alpha(TEXT, a), 0.5f);
    }
    if(m_hint_time > 0.0f && !m_hint.empty())
    {
        float const a = std::min(1.0f, m_hint_time);
        shadowed(t, f, m_hint, 16, {W * 0.5f, H - 96.0f}, with_alpha(PAPER_DK, a), 0.5f);
    }

    // --- endings ------------------------------------------------------------------------------------
    if(m_stage == Stage::WON || m_stage == Stage::LOST)
    {
        float const a = std::clamp((m_end_timer - 1.0f) / 1.0f, 0.0f, 1.0f);
        shadowed(t, f, m_stage == Stage::WON ? "THE FARM IS TAKEN" : "THE ATTACK HAS FAILED", 32, {W * 0.5f, 150}, with_alpha(TEXT, a), 0.5f);
    }

    if(m_paused)
    {
        box(t, {0, 0}, {W, H}, with_alpha(INK, 0.75f));
        shadowed(t, f, "PAUSED", 32, {W * 0.5f, H * 0.5f - 60}, TEXT, 0.5f);
        shadowed(t, f, "esc  back to it        q  quit to the briefing        m  music on or off", 16, {W * 0.5f, H * 0.5f - 14}, TEXT_DIM, 0.5f);
        shadowed(t, f, "wasd move   shift run   mouse aim   left click fire   right click steady / set up gun", 16, {W * 0.5f, H * 0.5f + 30}, TEXT_FAINT, 0.5f);
        shadowed(t, f, "c crouch   z prone   r reload   g grenade   e drag or treat   f bandage yourself", 16, {W * 0.5f, H * 0.5f + 50}, TEXT_FAINT, 0.5f);
    }
}

}
