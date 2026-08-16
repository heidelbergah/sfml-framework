/**
 * GameOverScene.hpp  -  Cascade
 *
 * Run summary. Reads GameState, which the play scene filled in on its
 * way out.
 */

#ifndef TETRIS_GAME_OVER_SCENE_HPP
#define TETRIS_GAME_OVER_SCENE_HPP

#include "../../include/scenes/Scene.hpp"
#include "../../include/scenes/SceneManager.hpp"

#include "../../include/framework/ParticleSystemManager.hpp"
#include "../../include/gui/Button.hpp"
#include "../../include/gui/TextBox.hpp"
#include "../../include/gui/WidgetManager.hpp"

#include "Effects.hpp"
#include "GameState.hpp"

namespace tetris
{

class GameOverScene : public Scene
{
private:
    WidgetManager         m_widgets;
    ParticleSystemManager m_particles;
    AmbientField          m_ambient;

    const sf::Font* m_font{nullptr};

    std::shared_ptr<TextBox> m_headline;
    std::shared_ptr<TextBox> m_score;
    std::shared_ptr<TextBox> m_detail;
    std::shared_ptr<TextBox> m_record;
    std::shared_ptr<Button>  m_again;
    std::shared_ptr<Button>  m_menu;

    float m_settle_timer{0.0f};

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
