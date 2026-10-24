/**
 * Screens.hpp  -  Mud Cats
 *
 * Before and after: the briefing, where you pick where to fight, how hard
 * it is and which man you go in as, shown from the side, full height; and the debrief, which says what
 * happened to every man in the platoon, and what happened on the day.
 */

#ifndef MUDCATS_SCREENS_HPP
#define MUDCATS_SCREENS_HPP

#include "../../include/scenes/Scene.hpp"
#include "../../include/scenes/SceneManager.hpp"

#include "Soldier.hpp"

namespace mudcats
{

class BriefingScene : public Scene
{
private:
    int   m_choice{0};
    int   m_hover{-1};
    float m_time{0.0f};
    float m_raise{0.0f};            // the chosen man bringing his weapon up
    float m_picked{0.0f};           // seconds since he was chosen
    bool  m_leaving{false};
    sf::RenderTexture m_portrait;   // drawn small, scaled up with hard edges
    bool  m_portrait_ready{false};

    void choose(int index);
    void begin();
    void set_level(int index);
    void next_difficulty(int step);
    [[nodiscard]] int role_at(sf::Vector2f p) const;

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
