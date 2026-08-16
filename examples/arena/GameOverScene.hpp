/**
 * GameOverScene.hpp  -  Ironvale Arena
 *
 * Run summary. Demonstrates carrying state between scenes and
 * re-entering a scene cleanly.
 */

#ifndef ARENA_GAME_OVER_SCENE_HPP
#define ARENA_GAME_OVER_SCENE_HPP

#include "../../include/scenes/Scene.hpp"
#include "../../include/scenes/SceneManager.hpp"

#include "../../include/framework/ParticleSystemManager.hpp"
#include "../../include/gui/Button.hpp"
#include "../../include/gui/TextBox.hpp"
#include "../../include/gui/WidgetManager.hpp"

#include "RunState.hpp"

namespace arena
{

class GameOverScene : public Scene
{
private:
    WidgetManager         m_widgets;
    ParticleSystemManager m_particles;

    std::shared_ptr<TextBox> m_headline;
    std::shared_ptr<TextBox> m_summary;
    std::shared_ptr<TextBox> m_record;
    std::shared_ptr<Button>  m_retry;
    std::shared_ptr<Button>  m_menu;

    float m_ash_timer{0.0f};

    void build_widgets();

public:
    GameOverScene();

    void on_enter() override;
    void begin_frame() override;
    void handle_event(const sf::Event& event, sf::Vector2f mouse_pos) override;
    void update(sf::Time delta_time) override;
    void render(sf::RenderTarget& target) override;

    [[nodiscard]] sf::Color get_clear_color() const override;
};

}

#endif
