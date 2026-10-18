/**
 * MenuScene.hpp  -  Cascade
 *
 * Title screen. Tetrominoes drift slowly behind the panel, and the
 * widgets fly in on a stagger.
 */

#ifndef TETRIS_MENU_SCENE_HPP
#define TETRIS_MENU_SCENE_HPP

#include "../../include/scenes/Scene.hpp"
#include "../../include/scenes/SceneManager.hpp"

#include "../../include/gui/Button.hpp"
#include "../../include/gui/TextBox.hpp"
#include "../../include/gui/WidgetManager.hpp"

#include "Effects.hpp"
#include "GameState.hpp"

namespace tetris
{

class MenuScene : public Scene
{
private:
    /*
     * A tetromino tumbling slowly in the background.
     */
    struct Drifter
    {
        PieceType    type{PieceType::I};
        sf::Vector2f pos;
        float        speed{0.0f};
        float        cell{0.0f};
        float        spin_timer{0.0f};
        float        spin_period{0.0f};
        int          rotation{0};
        float        alpha{0.0f};
    };

    WidgetManager m_widgets;
    AmbientField  m_ambient;

    std::vector<Drifter> m_drifters;
    sf::VertexArray      m_backdrop{sf::PrimitiveType::Triangles};

    const sf::Font* m_font{nullptr};

    std::shared_ptr<TextBox> m_title;
    std::shared_ptr<TextBox> m_subtitle;
    std::shared_ptr<TextBox> m_controls;
    std::shared_ptr<TextBox> m_best;
    std::shared_ptr<Button>  m_play;
    std::shared_ptr<Button>  m_quit;

    void build_widgets();
    void spawn_drifter(bool anywhere);

public:
    MenuScene();

    void on_enter() override;
    void begin_frame() override;
    void handle_event(const sf::Event& event, sf::Vector2f mouse_pos) override;
    void update(sf::Time delta_time) override;
    void render(sf::RenderTarget& target) override;

    [[nodiscard]] sf::Color get_clear_color() const override;
};

}

#endif
