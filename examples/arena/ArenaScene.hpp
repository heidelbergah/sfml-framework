/**
 * ArenaScene.hpp  -  Ironvale Arena
 *
 * The game itself, and the main showcase for the framework.
 *
 * What each piece is demonstrating:
 *
 *   Camera        follows the tank with a look-ahead toward the cursor,
 *                 clamped to the arena, and shakes on every shot and
 *                 explosion. The HUD is drawn through get_ui_view() so
 *                 it jolts with the world without panning.
 *   Particles     five systems: muzzle flash, tread dust, sparks,
 *                 debris and a thruster trail.
 *   Widgets       health and heat Gauges, score and wave TextBoxes,
 *                 all tweened in on entry.
 *   GameTime      P pauses everything; dying drops into slow motion.
 *   EventBus      the game publishes SWITCH_TO_GAME_OVER rather than
 *                 knowing what scene comes next.
 *   Interpolated  turret easing, enemy spawn pops, HUD slide-in.
 *   Markov        generates the run's callsign.
 */

#ifndef ARENA_SCENE_HPP
#define ARENA_SCENE_HPP

#include "../../include/scenes/Scene.hpp"
#include "../../include/scenes/SceneManager.hpp"

#include "../../include/framework/Camera.hpp"
#include "../../include/framework/ParticleSystemManager.hpp"

#include "../../include/gui/Gauge.hpp"
#include "../../include/gui/TextBox.hpp"
#include "../../include/gui/WidgetManager.hpp"
#include "../../include/utilities/markov.hpp"

#include "Entities.hpp"
#include "RunState.hpp"

namespace arena
{

class ArenaScene : public Scene
{
private:
    Camera                m_camera;
    ArenaFloor            m_floor;
    ParticleSystemManager m_particles;
    WidgetManager         m_hud;

    Tank                 m_tank;
    std::vector<Bullet>  m_bullets;
    std::vector<Enemy>   m_enemies;
    std::vector<Pickup>  m_pickups;

    std::shared_ptr<Gauge>   m_health_gauge;
    std::shared_ptr<Gauge>   m_heat_gauge;
    std::shared_ptr<TextBox> m_score_text;
    std::shared_ptr<TextBox> m_wave_text;
    std::shared_ptr<TextBox> m_status_text;

    sf::Vector2f m_mouse_world;
    bool         m_firing{false};

    float m_wave_timer{0.0f};
    float m_spawn_timer{0.0f};
    int   m_alive_target{0};
    float m_death_timer{-1.0f};
    bool  m_dead{false};
    float m_dust_timer{0.0f};

    void build_hud();
    void build_particles();

    void start_wave(int wave);
    void spawn_enemy();
    void fire();
    void handle_collisions();
    void kill_enemy(std::size_t index);
    void die();

    void update_hud();

    [[nodiscard]] std::shared_ptr<ParticleSystem> system(const std::string& key);

public:
    ArenaScene();

    void on_enter() override;
    void on_exit() override;

    void begin_frame() override;
    void handle_event(const sf::Event& event, sf::Vector2f mouse_pos) override;
    void update(sf::Time delta_time) override;
    void update_realtime(sf::Time delta_time) override;
    void render(sf::RenderTarget& target) override;

    [[nodiscard]] sf::Color get_clear_color() const override;
};

}   // namespace arena

#endif
