/**
 * Game.hpp
 *
 * Written by James Bouchat
 */

#ifndef GAME_HPP
#define GAME_HPP

#include "../Includes.hpp"
#include "../gui/Widget.hpp"
#include "../gui/Button.hpp"
#include "../gui/TextBox.hpp"
#include "../gui/Frame.hpp"
#include "../gui/Slider.hpp"
#include "../gui/Gauge.hpp"
#include "../gui/WidgetManager.hpp"

#include "../framework/ParticleSystemManager.hpp"
#include "../framework/ShaderManager.hpp"
#include "../framework/Camera.hpp"
#include "../framework/EventBus.hpp"

class Game
{
private:
    // THIS PRIVATE SECTION MUST REMAIN IN GAME CLASS, DO NOT REMOVE
    sf::Vector2u m_base_resolution = {WINDOW_WIDTH, WINDOW_HEIGHT}; // native resolution
    sf::Vector2f m_resolution_scale = {static_cast<float>(SCALE), static_cast<float>(SCALE)}; // scale for display resolution
 
    sf::ContextSettings m_context_settings;
    sf::RenderWindow m_window;
    sf::RenderTexture m_whole_screen_texture;
    sf::Sprite m_whole_screen_sprite;
    sf::Clock m_clock;
    const std::string GAME_NAME = "undefined";

    void initialize_objects();
    void process_events();
    void update(sf::Time delta_time);
    void render();

private:
    // ADD YOUR DATA HERE
    Camera m_camera;
    WidgetManager m_widgets;
    ParticleSystemManager m_particle_system_manager;
    ShaderManager m_shader_manager;

    ListenerHandle m_pause_handle;

public:
    Game();
    ~Game();

    void run(int frame_per_seconds);

};

#endif
