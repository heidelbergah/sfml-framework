/**
 * Screens.hpp  -  Mud Cats
 *
 * Before and after: the briefing, where you pick which of the eight men
 * you go over as, and the debrief, which says what happened to each of
 * them.
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
    bool  m_leaving{false};

    void choose(int index);
    void begin();
    [[nodiscard]] int card_at(sf::Vector2f p) const;

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

public:
    void on_enter() override;
    void handle_event(const sf::Event& event, sf::Vector2f mouse_pos) override;
    void update_realtime(sf::Time delta_time) override;
    void render(sf::RenderTarget& target) override;
    [[nodiscard]] sf::Color get_clear_color() const override;
};

}

#endif
