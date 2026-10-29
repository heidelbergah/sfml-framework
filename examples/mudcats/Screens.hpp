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
 *   DebriefScene    what happened to every man, and what happened on the day
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

    // Settings rows: 0..3 the sound levels, 4 the display, 5 how to play, 6 done.
    static constexpr int ROWS = 7;
    void set_level(int bus, float value);
    void step_display(int step, SceneManager& manager);
    void activate(int row, SceneManager& manager);
    [[nodiscard]] int display_choices() const;
    [[nodiscard]] int display_choice(const SceneManager& manager) const;
    void set_display_choice(int choice, SceneManager& manager);
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
    int   m_page{0};            // 0 the men, 1 the day

public:
    void on_enter() override;
    void handle_event(const sf::Event& event, sf::Vector2f mouse_pos) override;
    void update_realtime(sf::Time delta_time) override;
    void render(sf::RenderTarget& target) override;
    [[nodiscard]] sf::Color get_clear_color() const override;
};

}

#endif
