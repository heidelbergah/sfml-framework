/**
 * Screens.cpp  -  Mud Cats
 */

#include "Screens.hpp"

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
    struct Card
    {
        Role role;
        const char* tag;
        const char* lines[3];
        const char* keys;
    };

    const Card CARDS[] = {
        {Role::RIFLEMAN,     "the backbone",       {"Bolt-action rifle, five rounds, and a bayonet.", "Deadly at range if you take your time.", "Two grenades."}, "G grenade   middle click bayonet"},
        {Role::OFFICER,      "Lt. Aurel Varga",    {"Submachine gun. Leads the platoon: orders", "to each section, smoke to cover a bound,", "red flares to bring down the guns."}, "Tab, 1-5 orders   T smoke   X flare"},
        {Role::MEDIC,        "the only hope",      {"A pistol and eight field dressings.", "Only he can get a man who is down", "back on his feet. Wounded crawl to him."}, "E on a wounded man to treat him"},
        {Role::FLAMER,       "the horror",         {"Burns through any cover, up close.", "Slow, and short of fuel. Everyone shoots", "at him first, and his tanks can burst."}, "hold left click to burn"},
        {Role::GUNNER,       "the anchor",         {"Must be set up to fire. Pins a whole", "section with one burst. Fire in bursts,", "or the barrel overheats."}, "right click to set up"},
        {Role::RADIOMAN,     "the long arm",       {"The set on his back reaches the guns:", "a ranging round, then a salvo of shells", "or smoke. They aim for his aerial."}, "V call the guns   B shells or smoke"},
        {Role::STORMTROOPER, "the raider",         {"Submachine gun, five grenades and a", "trench club. Light and quick. Clears", "trenches and houses at arm's length."}, "G grenade   middle click club"},
        {Role::SNIPER,       "the patient one",    {"Scoped rifle. Right click looks a long way", "out. Lying still, he's a heap of rags;", "every shot gives him away for longer."}, "hold right click to scope"},
        {Role::SAPPER,       "the way through",    {"Rifle, wire cutters, two satchel charges.", "Cuts a gap in wire, and a satchel", "through a bunker door ends the bunker."}, "E on wire to cut   V satchel charge"},
    };
    constexpr int CARD_COUNT = 9;

    struct Place
    {
        LevelId level;
        const char* line;
    };
    const Place PLACES[] = {
        {LevelId::TRENCHES, "Rain. Over the top, their trench, a bunker, a farm."},
        {LevelId::FIELDS,   "Dawn mist. Hedgerows, a wood, a stream, the mill."},
        {LevelId::RAID,     "Night. Eleven raiders, their wire, a hamlet, a dugout."},
        {LevelId::HILL,     "Afternoon into night. Hold the hill, wave after wave."},
    };
    constexpr int PLACE_COUNT = 4;
    constexpr float PLACE_H = 30.0f;

    // Layout.
    constexpr float LEFT = 40.0f;
    constexpr float PLACE_Y = 134.0f;
    constexpr float DIFF_Y = 266.0f;
    constexpr float ROLE_Y = 344.0f;
    constexpr float ROLE_H = 18.0f;
    const sf::FloatRect STAGE({450, 40}, {470, 400});
    const sf::FloatRect GO_BOX({720, 462}, {200, 40});

    sf::Text text(const sf::Font& f, const std::string& s, unsigned size, sf::Vector2f pos, sf::Color c, float align = 0.0f)
    {
        sf::Text t(f, s, size);
        t.setFillColor(c);
        sf::FloatRect const b = t.getLocalBounds();
        t.setOrigin({std::round(b.position.x + b.size.x * align), std::round(b.position.y)});
        t.setPosition({std::round(pos.x), std::round(pos.y)});
        return t;
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
}

sf::Color BriefingScene::get_clear_color() const { return INK; }

void BriefingScene::on_enter()
{
    m_leaving = false;
    m_manager->set_bloom_enabled(false);
    for(int i = 0; i < CARD_COUNT; ++i)
        if(CARDS[i].role == Campaign::current().chosen) m_choice = i;
    if(!m_portrait_ready) m_portrait_ready = m_portrait.resize({64u, 64u});
    m_picked = 10.0f;
    m_raise = 0.0f;
    SoundBank& s = SoundBank::get();
    s.stop_loops();
    s.set_dread(false);
    s.start_music();
    s.set_intensity(0.0f);
    s.set_front(0.35f);
}

int BriefingScene::role_at(sf::Vector2f p) const
{
    for(int i = 0; i < CARD_COUNT; ++i)
        if(sf::FloatRect({LEFT - 6.0f, ROLE_Y + ROLE_H * static_cast<float>(i) - 2.0f}, {360.0f, ROLE_H}).contains(p)) return i;
    return -1;
}

void BriefingScene::choose(int index)
{
    if(index < 0 || index >= CARD_COUNT || index == m_choice) return;
    m_choice = index;
    m_picked = 0.0f;
    SoundBank::get().play("ui_select", 0.7f);
}

void BriefingScene::set_level(int index)
{
    Campaign& c = Campaign::current();
    LevelId const want = PLACES[(index + PLACE_COUNT) % PLACE_COUNT].level;
    if(want == c.level) return;
    c.level = want;
    SoundBank::get().play("ui_select", 0.7f);
}

void BriefingScene::next_difficulty(int step)
{
    Campaign& c = Campaign::current();
    c.difficulty = (c.difficulty + step + 3) % 3;
    SoundBank::get().play("ui_select", 0.7f);
}

void BriefingScene::begin()
{
    if(m_leaving) return;
    m_leaving = true;
    Campaign::current().chosen = CARDS[m_choice].role;
    SoundBank::get().play("whistle", 0.4f);
    m_manager->request_scene_switch("battle", SceneTransitionType::FADE);
}

void BriefingScene::handle_event(const sf::Event& event, sf::Vector2f mouse_pos)
{
    using S = sf::Keyboard::Scancode;
    int const level = static_cast<int>(Campaign::current().level);
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
            case S::L: case S::Right: case S::D: set_level(level + 1); break;
            case S::Left: case S::A: set_level(level - 1); break;
            case S::K: next_difficulty(1); break;
            case S::Enter: case S::Space: begin(); break;
            case S::M: SoundBank::get().toggle_music_mute(); break;
            case S::Escape: EventBus::publish(Event(EventType::QUIT)); break;
            default: break;
        }
    }
    else if(event.is<sf::Event::MouseMoved>())
    {
        int const h = role_at(mouse_pos);
        if(h != m_hover && h >= 0) SoundBank::get().play("ui_hover", 0.5f);
        m_hover = h;
    }
    else if(const auto* press = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if(press->button != sf::Mouse::Button::Left) return;
        for(int i = 0; i < PLACE_COUNT; ++i)
            if(sf::FloatRect({LEFT - 6.0f, PLACE_Y + PLACE_H * static_cast<float>(i) - 4.0f}, {380.0f, PLACE_H}).contains(mouse_pos)) { set_level(i); return; }
        for(int i = 0; i < 3; ++i)
            if(sf::FloatRect({LEFT + 110.0f * static_cast<float>(i) - 6.0f, DIFF_Y + 18.0f}, {104.0f, 22.0f}).contains(mouse_pos))
            {
                if(Campaign::current().difficulty != i) { Campaign::current().difficulty = i; SoundBank::get().play("ui_select", 0.7f); }
                return;
            }
        int const c = role_at(mouse_pos);
        if(c >= 0) { choose(c); return; }
        if(GO_BOX.contains(mouse_pos) || STAGE.contains(mouse_pos)) begin();
    }
}

void BriefingScene::update_realtime(sf::Time delta_time)
{
    float const dt = delta_time.asSeconds();
    m_time += dt;
    m_picked += dt;
    // Chosen: the weapon comes up for a moment, then back to the ready.
    float const want = m_picked < 2.2f ? 1.0f : 0.0f;
    m_raise += (want - m_raise) * std::min(1.0f, dt * (want > 0.5f ? 9.0f : 3.0f));
    SoundBank::get().update(dt);
}

void BriefingScene::render(sf::RenderTarget& t)
{
    const sf::Font* f = Assets::font("assets/fonts/pixel-font-bitmap.ttf");
    if(!f) return;
    t.setView(t.getDefaultView());
    const Campaign& c = Campaign::current();
    const Card& card = CARDS[m_choice];

    // --- the left page: where, how hard, who -------------------------------------------------
    t.draw(text(*f, "MUD CATS", 64, {LEFT - 4, 20}, PAPER));
    t.draw(text(*f, "Monteran 3rd Rifles, B Platoon.  The Ochre front, spring 1936.", 16, {LEFT, 90}, TEXT_DIM));

    t.draw(text(*f, "WHERE", 16, {LEFT, PLACE_Y - 22}, TEXT_FAINT));
    for(int i = 0; i < PLACE_COUNT; ++i)
    {
        bool const on = PLACES[i].level == c.level;
        float const y = PLACE_Y + PLACE_H * static_cast<float>(i);
        if(on) box(t, {LEFT - 8, y - 4}, {384, PLACE_H}, with_alpha(MON_BLUE_DK, 0.55f));
        if(on) box(t, {LEFT - 8, y - 4}, {3, PLACE_H}, MON_GOLD);
        t.draw(text(*f, level_name(PLACES[i].level), 16, {LEFT, y - 1}, on ? MON_GOLD : TEXT));
        t.draw(text(*f, PLACES[i].line, 16, {LEFT, y + 13}, on ? TEXT_DIM : TEXT_FAINT));
    }

    t.draw(text(*f, "DIFFICULTY", 16, {LEFT, DIFF_Y}, TEXT_FAINT));
    for(int i = 0; i < 3; ++i)
    {
        bool const on = c.difficulty == i;
        float const x = LEFT + 110.0f * static_cast<float>(i);
        if(on) box(t, {x - 6, DIFF_Y + 18}, {98, 22}, with_alpha(MON_BLUE_DK, 0.55f), MON_GOLD);
        t.draw(text(*f, difficulty_name(i), 16, {x + 43, DIFF_Y + 21}, on ? MON_GOLD : TEXT_DIM, 0.5f));
    }

    t.draw(text(*f, "WHO YOU GO IN AS", 16, {LEFT, ROLE_Y - 22}, TEXT_FAINT));
    for(int i = 0; i < CARD_COUNT; ++i)
    {
        bool const on = i == m_choice;
        bool const hover = i == m_hover;
        float const y = ROLE_Y + ROLE_H * static_cast<float>(i);
        if(on) box(t, {LEFT - 8, y - 2}, {300, ROLE_H}, with_alpha(MON_BLUE_DK, 0.55f));
        if(on) box(t, {LEFT - 8, y - 2}, {3, ROLE_H}, MON_GOLD);
        t.draw(text(*f, std::to_string(i + 1), 16, {LEFT, y}, on ? MON_GOLD : TEXT_FAINT));
        t.draw(text(*f, role_name(CARDS[i].role), 16, {LEFT + 22, y}, on ? MON_GOLD : (hover ? TEXT : TEXT_DIM)));
        t.draw(text(*f, CARDS[i].tag, 16, {LEFT + 284, y}, on ? TEXT_DIM : TEXT_FAINT, 1.0f));
    }

    // --- the right page: the man, full height, in his weather -----------------------------------
    sf::Color sky_top = lerp(INK, MON_BLUE_DK, 0.25f), sky_low = lerp(INK, SMOKE, 0.35f), ground = MUD_DARK;
    if(c.level == LevelId::FIELDS) { sky_top = lerp(INK, MIST, 0.35f); sky_low = lerp(MIST, FIRE_ORANGE, 0.18f); ground = lerp(MEADOW_DK, INK, 0.35f); }
    if(c.level == LevelId::RAID)   { sky_top = lerp(INK, sf::Color(20, 24, 48), 0.9f); sky_low = lerp(INK, sf::Color(48, 54, 90), 0.7f); ground = lerp(MUD_DARK, INK, 0.5f); }
    if(c.level == LevelId::HILL)   { sky_top = lerp(INK, MON_BLUE_DK, 0.55f); sky_low = lerp(PAPER_DK, FIRE_ORANGE, 0.3f); ground = lerp(MEADOW_DK, INK, 0.25f); }
    float const sx = STAGE.position.x, sy = STAGE.position.y, sw = STAGE.size.x, sh = STAGE.size.y;
    float const horizon = sy + sh * 0.72f;
    sf::VertexArray va(sf::PrimitiveType::Triangles);
    // Sky in bands, not a smooth gradient: it's pixel art.
    for(int k = 0; k < 10; ++k)
    {
        float const y0 = sy + (horizon - sy) * static_cast<float>(k) / 10.0f;
        draw::rect(va, sx, y0, sw, (horizon - sy) / 10.0f + 1.0f, lerp(sky_top, sky_low, static_cast<float>(k) / 9.0f));
    }
    if(c.level == LevelId::RAID)
        for(int k = 0; k < 60; ++k)
            if(std::fmod(m_time * 0.3f + hash(static_cast<float>(k)) * 9.0f, 3.0f) > 0.25f)
                draw::rect(va, std::floor(sx + hash(static_cast<float>(k) + 0.3f) * sw), std::floor(sy + hash(static_cast<float>(k) + 0.7f) * (horizon - sy) * 0.8f), 2, 2,
                           with_alpha(PAPER, 0.35f + 0.4f * hash(static_cast<float>(k) + 1.1f)));
    // A distant line: broken trees and a ruined spire, in silhouette.
    sf::Color const far = lerp(sky_low, INK, 0.45f);
    if(c.level == LevelId::HILL)
    {
        // The hill itself, off to one side, with a low sun behind it.
        draw::disc(va, sx + sw * 0.82f, horizon - 70.0f, 22.0f, lerp(sky_low, FIRE_YELLOW, 0.35f));
        sf::Color const hill = lerp(sky_low, INK, 0.62f);
        for(float x = sx; x < sx + sw; x += 4.0f)
        {
            float const u = (x - (sx + sw * 0.7f)) / (sw * 0.32f);
            float const h = std::max(0.0f, 1.0f - u * u) * 78.0f;
            if(h > 0.0f) draw::rect(va, x, horizon - h, 4, h + 1, hill);
        }
    }
    for(float x = sx; x < sx + sw; x += 4.0f)
    {
        float const h = 6.0f + 10.0f * hash(std::floor(x / 4.0f) * 0.37f) * hash(std::floor(x / 28.0f));
        draw::rect(va, x, horizon - h, 4, h + 1, far);
    }
    draw::rect(va, sx + sw * 0.78f, horizon - 54, 8, 54, far);
    draw::rect(va, sx + sw * 0.78f + 2, horizon - 66, 4, 12, far);
    for(float x = sx + 30; x < sx + sw; x += 90.0f)
    {
        draw::rect(va, x, horizon - 40 + hash(x) * 14.0f, 3, 40 - hash(x) * 14.0f, far);
        draw::rect(va, x - 6, horizon - 30 + hash(x) * 14.0f, 6, 2, far);
    }
    // The ground he stands on.
    draw::rect(va, sx, horizon, sw, sy + sh - horizon, ground);
    for(int k = 0; k < 140; ++k)
        draw::rect(va, std::floor(sx + hash(static_cast<float>(k) * 1.7f) * sw), std::floor(horizon + 2 + hash(static_cast<float>(k) * 2.3f) * (sy + sh - horizon - 4)), 3, 1,
                   lerp(ground, PAPER, 0.08f + 0.08f * hash(static_cast<float>(k))));
    // Weather.
    if(c.level == LevelId::TRENCHES)
        for(int k = 0; k < 120; ++k)
        {
            float const x = sx + std::fmod(hash(static_cast<float>(k)) * sw + m_time * 30.0f, sw);
            float const y = sy + std::fmod(hash(static_cast<float>(k) + 5.0f) * sh + m_time * 260.0f, sh);
            draw::rect(va, std::floor(x), std::floor(y), 1, 5, with_alpha(PAPER_DK, 0.35f));
        }
    if(c.level == LevelId::FIELDS)
        for(int k = 0; k < 18; ++k)
        {
            float const x = sx + std::fmod(hash(static_cast<float>(k)) * sw + m_time * 6.0f, sw + 80.0f) - 80.0f;
            float const y = horizon - 30.0f + hash(static_cast<float>(k) + 2.0f) * 70.0f;
            draw::rect(va, std::max(sx, x), y, std::min(80.0f, sx + sw - std::max(sx, x)), 10, with_alpha(MIST, 0.18f));
        }
    t.draw(va);

    // The man himself, drawn small and blown up with hard edges.
    if(m_portrait_ready)
    {
        m_portrait.clear(sf::Color::Transparent);
        sf::VertexArray man(sf::PrimitiveType::Triangles);
        draw_profile(man, card.role, Team::MONTERAN, m_time, m_raise);
        sf::Transform at;
        at.translate({22.0f, 58.0f});
        // His shadow on the ground first.
        sf::VertexArray shadow(sf::PrimitiveType::Triangles);
        draw::rect(shadow, -6, -1, 16, 2, with_alpha(INK, 0.45f));
        m_portrait.draw(shadow, sf::RenderStates(at));
        m_portrait.draw(man, sf::RenderStates(at));
        m_portrait.display();
        sf::Sprite sprite(m_portrait.getTexture());
        sprite.setScale({8.0f, 8.0f});
        sprite.setPosition({std::round(sx + sw * 0.5f - 22.0f * 8.0f), std::round(horizon + 34.0f - 58.0f * 8.0f)});
        t.draw(sprite);
    }

    // What he is, over the picture.
    box(t, {sx, sy + sh - 2}, {sw, 2}, MON_GOLD);
    box(t, {sx, sy + sh}, {sw, 70}, with_alpha(INK, 0.95f));
    t.draw(text(*f, role_name(card.role), 32, {sx + 16, sy + 12}, PAPER));
    t.draw(text(*f, card.tag, 16, {sx + 18, sy + 48}, MON_GOLD));
    for(int k = 0; k < 3; ++k)
        t.draw(text(*f, card.lines[k], 16, {sx + 16, sy + sh + 8 + 17.0f * static_cast<float>(k)}, TEXT_DIM));
    t.draw(text(*f, fit(*f, card.keys, 16, sw - 230.0f), 16, {sx + 16, sy + sh + 62}, TEXT_FAINT));
    box(t, STAGE.position, STAGE.size, sf::Color::Transparent, with_alpha(TEXT_FAINT, 0.8f));

    float const pulse = 0.75f + 0.25f * std::sin(m_time * 3.0f);
    box(t, GO_BOX.position, GO_BOX.size, with_alpha(MON_BLUE_DK, 0.95f), with_alpha(MON_GOLD, pulse));
    const char* const go = c.level == LevelId::FIELDS ? "MOVE OUT" : (c.level == LevelId::RAID ? "GO IN" : (c.level == LevelId::HILL ? "DIG IN" : "GO OVER"));
    t.draw(text(*f, go, 16, {GO_BOX.position.x + GO_BOX.size.x * 0.5f, GO_BOX.position.y + 12}, PAPER, 0.5f));
    t.draw(text(*f, "1-9 or click: who     left/right: where     K: difficulty     enter: go     m music     esc quit", 16, {480, 518}, TEXT_FAINT, 0.5f));
}

// ---------------------------------------------------------------- debrief

sf::Color DebriefScene::get_clear_color() const { return INK; }

void DebriefScene::on_enter()
{
    m_time = 0.0f;
    m_page = 0;
    m_manager->set_bloom_enabled(false);
    SoundBank& s = SoundBank::get();
    s.stop_loops();
    s.set_intensity(0.0f);
    s.set_front(0.25f);
    s.play("paper", 0.6f);
}

void DebriefScene::handle_event(const sf::Event& event, sf::Vector2f mouse_pos)
{
    (void)mouse_pos;
    const auto* key = event.getIf<sf::Event::KeyPressed>();
    bool const click = event.is<sf::Event::MouseButtonPressed>();
    if((!key && !click) || m_time < 1.2f) return;
    if(key && key->scancode == sf::Keyboard::Scancode::Escape) { EventBus::publish(Event(EventType::QUIT)); return; }
    if(key && key->scancode == sf::Keyboard::Scancode::M) { SoundBank::get().toggle_music_mute(); return; }
    SoundBank::get().play("paper", 0.5f);
    if(m_page == 0)
    {
        m_page = 1;
        m_time = 0.0f;
        return;
    }
    m_manager->request_scene_switch("briefing", SceneTransitionType::FADE);
    m_time = -100.0f;
}

void DebriefScene::update_realtime(sf::Time delta_time)
{
    m_time += delta_time.asSeconds();
    SoundBank::get().update(delta_time.asSeconds());
}

void DebriefScene::render(sf::RenderTarget& t)
{
    const sf::Font* f = Assets::font("assets/fonts/pixel-font-bitmap.ttf");
    if(!f) return;
    t.setView(t.getDefaultView());
    const Campaign& c = Campaign::current();
    auto fade = [this](float at) { return std::clamp((m_time - at) / 0.5f, 0.0f, 1.0f); };

    const char* const won = c.level == LevelId::TRENCHES ? "THE FARM IS OURS" : (c.level == LevelId::FIELDS ? "THE MILL IS OURS" : "THE RAIDERS ARE BACK");
    if(c.level == LevelId::HILL)
    {
        // The hill always falls in the end. What counts is how long it took.
        char held[64];
        std::snprintf(held, sizeof held, "HILL 312: HELD FOR %d %s", c.waves, c.waves == 1 ? "WAVE" : "WAVES");
        t.draw(text(*f, held, 32, {480, 22}, with_alpha(c.waves >= 5 ? PAPER : ALERT, fade(0.1f)), 0.5f));
    }
    else
        t.draw(text(*f, c.won ? won : (c.level == LevelId::RAID ? "THE RAID FAILED" : "THE ATTACK FAILED"), 32, {480, 22}, with_alpha(c.won ? PAPER : ALERT, fade(0.1f)), 0.5f));

    int alive = 0, dead = 0, wounded = 0;
    for(const auto& s : c.platoon)
    {
        if(s.life == Life::DEAD) ++dead;
        else { ++alive; if(s.hp < 60.0f || s.lost || s.life == Life::DOWNED) ++wounded; }
    }
    int const secs = static_cast<int>(c.time);
    char line[160];
    if(c.level == LevelId::HILL)
        std::snprintf(line, sizeof line, "%s, %s.   %d:%02d.   %d men held it.   Every one of them is dead.",
                      level_name(c.level), difficulty_name(c.difficulty), secs / 60, secs % 60, static_cast<int>(c.platoon.size()));
    else
        std::snprintf(line, sizeof line, "%s, %s.   %d:%02d.   %d came back, %d of them wounded.   %d dead.",
                      level_name(c.level), difficulty_name(c.difficulty), secs / 60, secs % 60, alive, wounded, dead);
    t.draw(text(*f, line, 16, {480, 64}, with_alpha(TEXT_DIM, fade(0.4f)), 0.5f));

    if(m_page == 0)
    {
        // The men, in two columns.
        float const col_x[2] = {40.0f, 500.0f};
        for(int k = 0; k < 2; ++k)
        {
            t.draw(text(*f, c.level == LevelId::RAID ? "THE RAIDING PARTY" : (c.level == LevelId::HILL ? "ON THE HILL" : "B PLATOON"), 16, {col_x[k], 96}, with_alpha(TEXT_FAINT, fade(0.6f))));
            t.draw(text(*f, "kills", 16, {col_x[k] + 420, 96}, with_alpha(TEXT_FAINT, fade(0.6f)), 1.0f));
        }
        int const per_column = static_cast<int>((c.platoon.size() + 1) / 2);
        int i = 0;
        for(const auto& s : c.platoon)
        {
            int const column = i / std::max(1, per_column);
            int const row = i % std::max(1, per_column);
            float const x = col_x[std::min(column, 1)];
            float const y = 120.0f + static_cast<float>(row) * std::min(26.0f, 384.0f / static_cast<float>(std::max(1, per_column)));
            float const a = fade(0.8f + 0.06f * static_cast<float>(i));
            bool const gone = s.life == Life::DEAD;
            std::string const who = s.rank + " " + s.name.substr(s.name.find(' ') + 1);
            t.draw(text(*f, who, 16, {x, y}, with_alpha(s.is_player ? MON_GOLD : (gone ? TEXT_FAINT : TEXT), a)));
            t.draw(text(*f, fit(*f, s.fate, 16, 240.0f), 16, {x + 150, y}, with_alpha(gone ? lerp(ALERT, TEXT_DIM, 0.4f) : PAPER_DK, a)));
            if(s.kills > 0) t.draw(text(*f, std::to_string(s.kills), 16, {x + 420, y}, with_alpha(TEXT_DIM, a), 1.0f));
            ++i;
        }
        if(m_time > 2.0f)
            t.draw(text(*f, "any key: the day     esc quit", 16, {480, 514}, with_alpha(TEXT_FAINT, fade(2.0f)), 0.5f));
        return;
    }

    // The day: the numbers, then what happened, in order.
    std::snprintf(line, sizeof line, "Rechivii dead %d      ran %d      taken prisoner %d", c.enemy_dead, c.enemy_fled, c.prisoners);
    t.draw(text(*f, line, 16, {480, 100}, with_alpha(TEXT, fade(0.2f)), 0.5f));
    if(c.prisoners_shot > 0)
    {
        std::snprintf(line, sizeof line, "%d men were killed after they had surrendered.", c.prisoners_shot);
        t.draw(text(*f, line, 16, {480, 122}, with_alpha(ALERT, fade(0.5f)), 0.5f));
    }

    std::size_t const show = 16;
    std::size_t const first = c.log.size() > show ? c.log.size() - show : 0;
    float y = 160.0f;
    if(first > 0)
    {
        t.draw(text(*f, "...", 16, {120, y}, with_alpha(TEXT_FAINT, fade(0.6f))));
        y += 20.0f;
    }
    for(std::size_t i = first; i < c.log.size(); ++i)
    {
        float const a = fade(0.6f + 0.08f * static_cast<float>(i - first));
        t.draw(text(*f, fit(*f, c.log[i], 16, 720.0f), 16, {120, y}, with_alpha(TEXT_DIM, a)));
        y += 20.0f;
    }
    if(m_time > 2.0f)
        t.draw(text(*f, "any key to go again     esc to quit", 16, {480, 514}, with_alpha(TEXT_FAINT, fade(2.0f)), 0.5f));
}

}
