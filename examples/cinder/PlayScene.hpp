/**
 * PlayScene.hpp  -  Cinder
 *
 * You are the last spark in a world gone to ash. Colour comes back only
 * where it is warm: around you, along the trail you leave, and for good
 * around every brazier you relight. Heat drains as you go; embers on the
 * ground top it up, and so does standing by a fire you have lit. Relight
 * every brazier to bring a level back.
 *
 * All three levels run in this one scene. The framework parts it leans on:
 *
 *   Camera                 follows the spark, clamped to the level, and
 *                          shakes when a brazier catches
 *   GameTime               P pauses everything; going out plays in slow
 *                          motion
 *   Interpolated           brazier warmth (EaseOutBack, so it overshoots
 *                          and settles), the level-complete flood, fades
 *   ParticleSystemManager  the streaks of sparks when a fire catches
 *   bloom                  so the fire glows
 */

#ifndef CINDER_PLAY_SCENE_HPP
#define CINDER_PLAY_SCENE_HPP

#include "../../include/scenes/Scene.hpp"
#include "../../include/scenes/SceneManager.hpp"

#include "../../include/framework/Camera.hpp"
#include "../../include/framework/ParticleSystemManager.hpp"

#include "Level.hpp"
#include "Reveal.hpp"
#include "World.hpp"

#include <vector>

namespace cinder
{

class PlayScene : public Scene
{
private:
    enum class Phase { PLAYING, DYING, COMPLETE };
    enum class Pending { NONE, RESPAWN, NEXT_LEVEL };

    struct Brazier
    {
        sf::Vector2f pos;
        bool lit{false};
        Interpolated<float> warmth{0.0f};
    };

    struct Ember
    {
        sf::Vector2f pos;
        bool  taken{false};
        float respawn{0.0f};
        float phase{0.0f};
    };

    struct Trail
    {
        sf::Vector2f pos;
        float life{0.0f};
        float max_life{1.0f};
        float radius{0.0f};
    };

    struct Flake
    {
        sf::Vector2f pos;
        float speed{20.0f};
        float sway{0.0f};
    };

    Camera                m_camera;
    Reveal                m_reveal;
    World                 m_world;
    Motes                 m_motes;
    ParticleSystemManager m_sparks;

    const sf::Font* m_font{nullptr};

    std::size_t          m_level{0};
    std::vector<Brazier> m_braziers;
    std::vector<Ember>   m_embers;
    std::vector<Trail>   m_trail;
    std::vector<Flake>   m_flakes;

    Phase   m_phase{Phase::PLAYING};
    Pending m_pending{Pending::NONE};

    // --- the spark ----------------------------------------------------
    sf::Vector2f m_pos;
    sf::Vector2f m_vel;
    sf::Vector2f m_facing{1.0f, 0.0f};
    sf::Vector2f m_checkpoint;
    sf::Vector2f m_last_trail;
    float m_heat{1.0f};
    float m_dash_time{0.0f};
    float m_dash_cooldown{0.0f};
    bool  m_dash_requested{false};
    bool  m_was_in_water{false};
    float m_flame_timer{0.0f};
    float m_heartbeat_timer{0.0f};
    float m_low_flash{0.0f};       // HUD flash when heat is lost suddenly

    bool m_up{false}, m_down{false}, m_left{false}, m_right{false};
    bool m_moved{false};

    // --- timing and presentation -------------------------------------
    float m_time{0.0f};             // scaled, for animation
    float m_level_real{0.0f};       // real seconds since the level began
    float m_phase_real{0.0f};       // real seconds in the current phase
    float m_mute_notice{0.0f};
    sf::Vector2f        m_flood_centre;
    Interpolated<float> m_flood{0.0f};
    Interpolated<float> m_fade{0.0f};

    void start_level(std::size_t index);
    void respawn();

    void update_spark(float dt);
    void update_world(float dt);
    void update_audio();
    void light(Brazier& brazier);
    void go_out();

    void draw_bowls(sf::RenderTarget& target) const;
    void draw_cold_things(sf::RenderTarget& target, const sf::FloatRect& visible);
    void draw_spark(sf::RenderTarget& target);
    void draw_hud(sf::RenderTarget& target);
    void draw_pointers(sf::RenderTarget& target);

    [[nodiscard]] int lit_count() const;

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
