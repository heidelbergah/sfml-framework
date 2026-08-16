/**
 * PlayScene.hpp  -  Cascade
 *
 * The game. Guideline Tetris: seven-bag, SRS with wall kicks, hold,
 * five-piece preview, ghost, hard drop, lock delay with move resets,
 * T-spins, back-to-back and combos.
 *
 * The presentation leans on the framework rather than reimplementing
 * anything:
 *
 *   Camera        shakes on lock, hard drop and line clears; the board
 *                 is drawn through get_ui_view() so the whole playfield
 *                 jolts together.
 *   Particles     four systems -- lock dust, hard-drop trail, line
 *                 clear burst, and level-up motes.
 *   Interpolated  board glow, danger pulse, hold flash, row collapse.
 *   GameTime      pause freezes everything; the game-over sequence uses
 *                 slow motion.
 *   EventBus      publishes SWITCH_TO_GAME_OVER rather than knowing
 *                 what comes next.
 */

#ifndef TETRIS_PLAY_SCENE_HPP
#define TETRIS_PLAY_SCENE_HPP

#include "../../include/scenes/Scene.hpp"
#include "../../include/scenes/SceneManager.hpp"

#include "../../include/framework/Camera.hpp"
#include "../../include/framework/ParticleSystemManager.hpp"

#include "Board.hpp"
#include "Effects.hpp"
#include "GameState.hpp"

namespace tetris
{

class PlayScene : public Scene
{
private:
    /*
     * CLEARING holds the board still while the cleared rows flash and
     * burst; TOPPED_OUT plays the failure sequence before handing over
     * to the game-over scene.
     */
    enum class Phase { PLAYING, CLEARING, TOPPED_OUT };

    Camera                m_camera;
    ParticleSystemManager m_particles;
    AmbientField          m_ambient;
    PopupList             m_popups;

    Board     m_board;
    PieceBag  m_bag;
    Piece     m_piece;
    PieceType m_hold{PieceType::NONE};
    std::vector<PieceType> m_queue;

    const sf::Font* m_font{nullptr};

    Phase m_phase{Phase::PLAYING};

    // --- timing -----------------------------------------------------
    float m_gravity_timer{0.0f};
    float m_lock_timer{0.0f};
    int   m_lock_resets{0};
    float m_clear_timer{0.0f};
    float m_topout_timer{0.0f};
    float m_spawn_delay{0.0f};

    // --- input ------------------------------------------------------
    int   m_move_dir{0};
    float m_das_timer{0.0f};
    bool  m_das_charged{false};
    bool  m_soft_dropping{false};
    bool  m_hold_used{false};

    // --- rule bookkeeping -------------------------------------------
    bool m_last_move_rotation{false};
    bool m_last_rotation_kicked{false};

    // --- presentation -----------------------------------------------
    std::vector<int>    m_clearing_rows;
    Interpolated<float> m_board_glow;
    Interpolated<float> m_hold_flash;
    Interpolated<float> m_piece_fade;
    float               m_danger{0.0f};
    float               m_energy{0.0f};
    int                 m_last_level{1};

    // Vertex buffers, rebuilt each frame.
    sf::VertexArray m_chrome{sf::PrimitiveType::Triangles};
    sf::VertexArray m_blocks{sf::PrimitiveType::Triangles};
    sf::VertexArray m_overlay{sf::PrimitiveType::Triangles};

    // --- helpers ----------------------------------------------------
    [[nodiscard]] std::shared_ptr<ParticleSystem> system(const std::string& key);

    [[nodiscard]] sf::Vector2f cell_position(int x, int y) const;
    [[nodiscard]] sf::Vector2f board_origin() const;

    void build_particles();

    void refill_queue();
    void spawn_next();
    void spawn(PieceType type);
    bool try_move(int dx, int dy);
    bool try_rotate(int direction);
    void hard_drop();
    void hold_piece();
    void lock_piece();
    void begin_clear(const std::vector<int>& rows, const ClearResult& result);
    void finish_clear();
    void top_out();

    void reset_lock_timer(bool moved);

    void emit_lock_dust(const Piece& piece);
    void emit_drop_trail(const Piece& piece, int distance);
    void emit_clear_burst(int row, sf::Color color);
    void announce(const ClearResult& result);

    void draw_chrome();
    void draw_stack();
    void draw_active_piece();
    void draw_preview_and_hold();
    void draw_text(sf::RenderTarget& target);

    void draw_mini_piece(sf::VertexArray& va, PieceType type,
                         sf::Vector2f centre, float cell, float alpha);

public:
    PlayScene();

    void on_enter() override;
    void on_exit() override;

    void handle_event(const sf::Event& event, sf::Vector2f mouse_pos) override;
    void update(sf::Time delta_time) override;
    void update_realtime(sf::Time delta_time) override;
    void render(sf::RenderTarget& target) override;

    [[nodiscard]] sf::Color get_clear_color() const override;
};

}

#endif
