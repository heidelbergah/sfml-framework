/**
 * GameScene.hpp  -  Void Drift
 */

#ifndef GAME_SCENE_HPP
#define GAME_SCENE_HPP

#include "Scene.hpp"
#include "../framework/ParticleSystemManager.hpp"
#include "../framework/Camera.hpp"
#include "../framework/EventBus.hpp"
#include "../gui/WidgetManager.hpp"
#include "../gui/Gauge.hpp"
#include "../gui/TextBox.hpp"

#include "../Ship.hpp"
#include "../Asteroid.hpp"
#include "../Bullet.hpp"
#include "../ScorePickup.hpp"
#include "../Starfield.hpp"
#include "../RunState.hpp"

#include <vector>

class GameScene : public Scene
{
private:
    sf::Vector2f m_resolution_scale = {static_cast<float>(SCALE), static_cast<float>(SCALE)};

    sf::Font m_font;

    Ship                    m_ship;
    std::vector<Asteroid>   m_asteroids;
    std::vector<Bullet>     m_bullets;
    std::vector<ScorePickup>m_pickups;
    Starfield               m_stars;

    Camera                  m_camera;
    ParticleSystemManager   m_psm;
    WidgetManager           m_widgets;

    // HUD
    sf::Text* m_score_text = nullptr;
    sf::Text* m_lives_text = nullptr;
    sf::Text* m_level_text = nullptr;

    // Level state
    int   m_asteroids_remaining = 0;
    float m_level_clear_timer   = 0.f;  // brief pause before advancing
    bool  m_level_cleared       = false;

    // Ghost engine flash timer
    float m_ghost_flash = 0.f;

    ListenerHandle m_to_menu_handle;

    // Helpers
    void spawn_level();
    void spawn_explosion(sf::Vector2f pos, sf::Color col, int count, float speed = 3.f);
    void fire(sf::Vector2f pos, sf::Vector2f vel);
    void handle_collisions();
    void screen_wrap(sf::Vector2f& pos);
    void update_hud();

public:
    GameScene();

    void on_enter()       override;
    void on_exit()        override;
    void process_events() override;
    void update(sf::Time dt) override;
    void render()         override;
};

#endif
