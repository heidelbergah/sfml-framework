/**
 * Screens.hpp  -  Mud Cats
 *
 * Everything that isn't the battle:
 *
 *   TitleScene      the name, a horizon flickering with the guns, and the
 *                   way in: play, how to play, settings, credits, quit
 *   BriefingScene   where you pick the battle, the side, how hard it is and
 *                   who you go in as. The right half is the order you'd be
 *                   handed: a black-and-white staff map of the ground, with
 *                   the units and the plan marked on it, and the briefing
 *                   under it. What the map shows depends on whose side
 *                   you're on: the escort doesn't know where the ambush is
 *   DebriefScene    the losses on both sides, how long it took, and your side's newspaper
 *
 * Settings, how to play and the credits open over the top of the title or
 * the briefing, as panels (Panels, below). F11 flips fullscreen anywhere.
 */

#ifndef MUDCATS_SCREENS_HPP
#define MUDCATS_SCREENS_HPP

#include "../../include/scenes/Scene.hpp"
#include "../../include/scenes/SceneManager.hpp"

#include "Map.hpp"
#include "Soldier.hpp"

#include <array>
#include <memory>

namespace mudcats
{

/*
 * F11 for fullscreen, from any screen. Returns true if it was that.
 */
bool global_key(const sf::Event& event, SceneManager& manager);

/*
 * With no controller plugged in where one is needed (a Raspberry Pi
 * console), every screen is covered by this, asking for one. Drawn last.
 */
void draw_pad_gate(sf::RenderTarget& t);

// Draws the pad gate over whatever a render function has drawn, however it returns.
struct PadGateOnTop
{
    sf::RenderTarget& target;
    ~PadGateOnTop() { draw_pad_gate(target); }
};

/*
 * The panels that open over a screen: settings, how to play, credits.
 * While one is open it takes all the input.
 */
class Panels
{
public:
    enum class Kind { NONE, SETTINGS, HOW_TO, CREDITS };

    void open(Kind kind);
    void close();
    [[nodiscard]] bool is_open() const { return m_kind != Kind::NONE; }
    [[nodiscard]] Kind kind() const { return m_kind; }

    // True if the event was the panel's.
    bool handle_event(const sf::Event& event, sf::Vector2f mouse, SceneManager& manager);
    void update(float dt) { m_time += dt; }
    void render(sf::RenderTarget& t, SceneManager& manager);

private:
    Kind  m_kind{Kind::NONE};
    float m_time{0.0f};
    int   m_row{0};             // settings: which row the keys move
    int   m_drag{-1};           // settings: the sound bar being dragged
    int   m_hover{-1};

    // Settings rows: 0..3 the sound levels, 4 the display, 5 effects, 6 how to play, 7 done.
    static constexpr int ROWS = 8;
    void set_level(int bus, float value);
    void step_display(int step, SceneManager& manager);
    void activate(int row, SceneManager& manager);
    [[nodiscard]] int display_choices() const;
    [[nodiscard]] int display_choice(const SceneManager& manager) const;
    void set_display_choice(int choice, SceneManager& manager);
    void set_low_effects(bool low, SceneManager& manager);
};

class TitleScene : public Scene
{
private:
    float  m_time{0.0f};
    int    m_choice{0};
    int    m_hover{-1};
    bool   m_leaving{false};
    Panels m_panels;

    void pick(int item);

public:
    void on_enter() override;
    void handle_event(const sf::Event& event, sf::Vector2f mouse_pos) override;
    void update_realtime(sf::Time delta_time) override;
    void render(sf::RenderTarget& target) override;
    [[nodiscard]] sf::Color get_clear_color() const override;
};

class BriefingScene : public Scene
{
private:
    int    m_choice{0};
    int    m_hover_role{-1};
    int    m_hover_button{-1};
    float  m_time{0.0f};
    float  m_changed{0.0f};         // since the map last changed: it's drawn in as it comes up
    bool   m_leaving{false};
    Panels m_panels;

    // The ground of each battle, drawn once in ink.
    std::array<std::unique_ptr<sf::RenderTexture>, 3> m_sheets;
    std::array<sf::Vector2f, 3> m_world;        // each battle's map size, in world pixels
    const sf::RenderTexture* sheet(LevelId level);

    void choose(int index);
    void begin();
    void set_level(int index);
    void set_side(Team side);
    void set_difficulty(int d);
    [[nodiscard]] int role_at(sf::Vector2f p) const;
    [[nodiscard]] int button_at(sf::Vector2f p) const;
    void press(int button);

public:
    void on_enter() override;
    void handle_event(const sf::Event& event, sf::Vector2f mouse_pos) override;
    void update_realtime(sf::Time delta_time) override;
    void render(sf::RenderTarget& target) override;
    [[nodiscard]] sf::Color get_clear_color() const override;
};

class DebriefScene : public Scene
{
private:
    float m_time{0.0f};
    int   m_choice{0};          // the button under the keys: 0 again, 1 the briefing, 2 the title
    int   m_hover{-1};
    bool  m_leaving{false};
    std::size_t m_headline{0};  // which of the paper's headlines, picked once

    // The ground, drawn small for the paper.
    std::unique_ptr<sf::RenderTexture> m_sheet;
    LevelId m_sheet_level{LevelId::CHARGE};
    const sf::RenderTexture* sheet(LevelId level);

    void press(int button);
    [[nodiscard]] int button_at(sf::Vector2f p) const;

public:
    void on_enter() override;
    void handle_event(const sf::Event& event, sf::Vector2f mouse_pos) override;
    void update_realtime(sf::Time delta_time) override;
    void render(sf::RenderTarget& target) override;
    [[nodiscard]] sf::Color get_clear_color() const override;
};

}

#endif
