/**
 * Screens.cpp  -  Mud Cats
 */

#include "Screens.hpp"

#include "Battle.hpp"
#include "Draw.hpp"
#include "Palette.hpp"
#include "Sound.hpp"

#include <cmath>
#include <cstdio>

namespace mudcats
{

namespace
{
    struct Card
    {
        Role role;
        const char* who;
        const char* lines[4];
    };

    const Card CARDS[] = {
        {Role::RIFLEMAN, "Pvt. Emil Rell",   {"Bolt-action rifle,", "five rounds. Deadly", "at long range.", "Three grenades."}},
        {Role::OFFICER,  "Lt. Aurel Varga",  {"Submachine gun.", "Leads the section:", "1 to 4 give orders.", "Men near him hold."}},
        {Role::MEDIC,    "Pvt. Teodor Halm", {"Pistol and eight", "field dressings.", "Only he can save", "a man who is down."}},
        {Role::FLAMER,   "Pvt. Dorian Vosk", {"Burns through any", "cover, up close.", "Slow, and his tanks", "can burst."}},
        {Role::GUNNER,   "Cpl. Anselm Kray", {"Machine gun. Set it", "up to fire. Pins", "whole sections.", "Barrel overheats."}},
    };
    constexpr int CARD_COUNT = 5;

    constexpr float CARD_W = 176.0f, CARD_H = 240.0f, CARD_Y = 210.0f;
    float card_x(int i) { return 480.0f - (CARD_W * 5 + 8 * 4) * 0.5f + static_cast<float>(i) * (CARD_W + 8.0f); }

    sf::Text text(const sf::Font& f, const std::string& s, unsigned size, sf::Vector2f pos, sf::Color c, float align = 0.0f)
    {
        sf::Text t(f, s, size);
        t.setFillColor(c);
        sf::FloatRect const b = t.getLocalBounds();
        t.setOrigin({std::round(b.position.x + b.size.x * align), std::round(b.position.y)});
        t.setPosition({std::round(pos.x), std::round(pos.y)});
        return t;
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

    // A soldier drawn big: the same figure as in the game, scaled up.
    void portrait(sf::RenderTarget& t, Role role, sf::Vector2f at, float time, float scale)
    {
        Soldier s;
        s.team = Team::MONTERAN;
        s.role = role;
        s.aim = -1.5708f + std::sin(time * 0.8f + static_cast<float>(role)) * 0.15f;
        s.stance = Stance::STAND;
        sf::VertexArray va(sf::PrimitiveType::Triangles);
        draw_shadow(va, s);
        draw_soldier(va, s, time);
        sf::Transform tr;
        tr.translate(at).scale({scale, scale});
        t.draw(va, sf::RenderStates(tr));
    }
}

sf::Color BriefingScene::get_clear_color() const { return INK; }

void BriefingScene::on_enter()
{
    m_leaving = false;
    m_manager->set_bloom_enabled(false);
    for(int i = 0; i < CARD_COUNT; ++i)
        if(CARDS[i].role == Campaign::current().chosen) m_choice = i;
    SoundBank& s = SoundBank::get();
    s.stop_loops();
    s.start_music();
    s.set_intensity(0.0f);
    s.set_front(0.35f);
}

int BriefingScene::card_at(sf::Vector2f p) const
{
    for(int i = 0; i < CARD_COUNT; ++i)
        if(sf::FloatRect({card_x(i), CARD_Y}, {CARD_W, CARD_H}).contains(p)) return i;
    return -1;
}

void BriefingScene::choose(int index)
{
    if(index < 0 || index >= CARD_COUNT || index == m_choice) return;
    m_choice = index;
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
    if(const auto* key = event.getIf<sf::Event::KeyPressed>())
    {
        switch(key->scancode)
        {
            case S::Num1: choose(0); break;
            case S::Num2: choose(1); break;
            case S::Num3: choose(2); break;
            case S::Num4: choose(3); break;
            case S::Num5: choose(4); break;
            case S::Left: case S::A: choose((m_choice + CARD_COUNT - 1) % CARD_COUNT); break;
            case S::Right: case S::D: choose((m_choice + 1) % CARD_COUNT); break;
            case S::Enter: case S::Space: begin(); break;
            case S::M: SoundBank::get().toggle_music_mute(); break;
            case S::Escape: EventBus::publish(Event(EventType::QUIT)); break;
            default: break;
        }
    }
    else if(event.is<sf::Event::MouseMoved>())
    {
        int const h = card_at(mouse_pos);
        if(h != m_hover && h >= 0) SoundBank::get().play("ui_hover", 0.5f);
        m_hover = h;
    }
    else if(const auto* press = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if(press->button != sf::Mouse::Button::Left) return;
        int const c = card_at(mouse_pos);
        if(c >= 0)
        {
            if(c == m_choice) begin();
            else choose(c);
        }
        if(sf::FloatRect({380, 470}, {200, 36}).contains(mouse_pos)) begin();
    }
}

void BriefingScene::update_realtime(sf::Time delta_time)
{
    m_time += delta_time.asSeconds();
    SoundBank::get().update(delta_time.asSeconds());
}

void BriefingScene::render(sf::RenderTarget& t)
{
    const sf::Font* f = Assets::font("assets/fonts/pixel-font-bitmap.ttf");
    if(!f) return;
    t.setView(t.getDefaultView());

    // A faint field of mud behind it all, drifting.
    sf::VertexArray va(sf::PrimitiveType::Triangles);
    for(int i = 0; i < 260; ++i)
    {
        float const x = std::fmod(static_cast<float>(i * 97 % 960) + m_time * (4.0f + static_cast<float>(i % 5)), 960.0f);
        float const y = static_cast<float>((i * 53) % 540);
        draw::rect(va, std::floor(x), y, 2, 2, with_alpha(MUD_LIGHT, 0.25f));
    }
    t.draw(va);

    t.draw(text(*f, "M U D   C A T S", 64, {480, 34}, PAPER, 0.5f));
    t.draw(text(*f, "Monteran 3rd Rifles, 2 Section   -   the Ochre front, spring 1936", 16, {480, 108}, TEXT_DIM, 0.5f));
    t.draw(text(*f, "At first light the section goes over. Cross the field, take the Rechivii trench,", 16, {480, 140}, TEXT, 0.5f));
    t.draw(text(*f, "the bunker behind it and the farm beyond. Then hold the farm.", 16, {480, 158}, TEXT, 0.5f));
    t.draw(text(*f, "Choose who you go over as.  If he falls, you carry on as another.", 16, {480, 186}, PAPER_DK, 0.5f));

    for(int i = 0; i < CARD_COUNT; ++i)
    {
        const Card& c = CARDS[i];
        float const x = card_x(i);
        bool const chosen = i == m_choice;
        bool const hover = i == m_hover;
        box(t, {x, CARD_Y}, {CARD_W, CARD_H}, with_alpha(chosen ? lerp(INK, MON_BLUE_DK, 0.5f) : INK, 0.9f),
            chosen ? MON_GOLD : (hover ? TEXT_DIM : TEXT_FAINT));
        portrait(t, c.role, {x + CARD_W * 0.5f, CARD_Y + 64.0f}, m_time, 6.0f);
        t.draw(text(*f, std::to_string(i + 1) + "   " + role_name(c.role), 16, {x + 10, CARD_Y + 118}, chosen ? MON_GOLD : TEXT));
        t.draw(text(*f, c.who, 16, {x + 10, CARD_Y + 138}, TEXT_DIM));
        for(int k = 0; k < 4; ++k)
            t.draw(text(*f, c.lines[k], 16, {x + 10, CARD_Y + 162 + 18.0f * static_cast<float>(k)}, TEXT_DIM));
    }

    float const pulse = 0.75f + 0.25f * std::sin(m_time * 3.0f);
    box(t, {380, 470}, {200, 36}, with_alpha(MON_BLUE_DK, 0.9f), with_alpha(MON_GOLD, pulse));
    t.draw(text(*f, "GO OVER", 16, {480, 480}, PAPER, 0.5f));
    t.draw(text(*f, "1-5 or click to choose     enter to begin     m music     esc quit", 16, {480, 516}, TEXT_FAINT, 0.5f));
}

// ---------------------------------------------------------------- debrief

sf::Color DebriefScene::get_clear_color() const { return INK; }

void DebriefScene::on_enter()
{
    m_time = 0.0f;
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
    if((!key && !click) || m_time < 1.5f) return;
    if(key && key->scancode == sf::Keyboard::Scancode::Escape) { EventBus::publish(Event(EventType::QUIT)); return; }
    if(key && key->scancode == sf::Keyboard::Scancode::M) { SoundBank::get().toggle_music_mute(); return; }
    SoundBank::get().play("ui_select", 0.6f);
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
    auto fade = [this](float at) { return std::clamp((m_time - at) / 0.6f, 0.0f, 1.0f); };

    t.draw(text(*f, c.won ? "THE FARM IS OURS" : "THE ATTACK FAILED", 32, {480, 34}, with_alpha(c.won ? PAPER : ALERT, fade(0.2f)), 0.5f));
    int const secs = static_cast<int>(c.time);
    char line[128];
    std::snprintf(line, sizeof line, "%d:%02d under fire      Rechivii killed %d      ran %d", secs / 60, secs % 60, c.enemy_dead, c.enemy_fled);
    t.draw(text(*f, line, 16, {480, 78}, with_alpha(TEXT_DIM, fade(0.6f)), 0.5f));
    t.draw(text(*f, "2 SECTION", 16, {120, 120}, with_alpha(TEXT_FAINT, fade(0.8f))));

    float y = 146.0f;
    int i = 0;
    for(const auto& s : c.squad)
    {
        float const a = fade(1.0f + 0.25f * static_cast<float>(i++));
        bool const dead = s.life == Life::DEAD;
        sf::Color const name = dead ? TEXT_FAINT : TEXT;
        t.draw(text(*f, s.rank + " " + s.name, 16, {120, y}, with_alpha(name, a)));
        t.draw(text(*f, role_name(s.role), 16, {330, y}, with_alpha(TEXT_FAINT, a)));
        t.draw(text(*f, s.fate, 16, {470, y}, with_alpha(dead ? lerp(ALERT, TEXT_DIM, 0.4f) : PAPER_DK, a)));
        if(s.kills > 0) t.draw(text(*f, std::to_string(s.kills), 16, {850, y}, with_alpha(TEXT_DIM, a), 1.0f));
        y += 30.0f;
    }
    t.draw(text(*f, "kills", 16, {850, 120}, with_alpha(TEXT_FAINT, fade(0.8f)), 1.0f));

    if(m_time > 3.5f)
        t.draw(text(*f, "any key to go again     esc to quit", 16, {480, 500}, with_alpha(TEXT_FAINT, fade(3.5f)), 0.5f));
}

}
