#ifndef SCENE_MANAGER_HPP
#define SCENE_MANAGER_HPP

#include "Scene.hpp"

#include "../framework/ParticleSystemManager.hpp"
#include "../framework/ShaderManager.hpp"
#include "../framework/Camera.hpp"
#include "../framework/EventBus.hpp"

enum class SceneTransitionType
{
    INSTANT,
    FADE
};

class Scene;

class SceneManager
{
private:
    sf::Vector2u m_base_resolution = {WINDOW_WIDTH, WINDOW_HEIGHT}; // native resolution
    sf::Vector2f m_resolution_scale = {static_cast<float>(SCALE), static_cast<float>(SCALE)}; // scale for display resolution
    
    sf::RenderWindow m_render_window;
    sf::RenderTexture m_render_texture;
    sf::Sprite m_render_sprite;
    sf::Clock m_clock;

    std::unordered_map<std::string, std::shared_ptr<Scene>> m_scenes;
    std::shared_ptr<Scene> m_current_scene;

    bool m_pending_scene_switch = false;
    std::string m_next_scene_key;
    SceneTransitionType m_next_transition_type;

    ListenerHandle m_switch_to_game_handle;
    ListenerHandle m_switch_to_menu_handle;

    const std::string GAME_NAME = "undefined";

    void setup_event_listeners();
    void cleanup_event_listeners();
    void perform_fade_transition(std::shared_ptr<Scene> next_scene);
    void display_texture_to_window();

public:
    SceneManager();
    ~SceneManager();

    void add_scene(std::string key, std::shared_ptr<Scene> scene);

    void delete_scene(std::string key);

    void run_scene(int frame_per_seconds);

    void swap_scene(std::string key, SceneTransitionType transition_type);

    void request_scene_switch(std::string key, SceneTransitionType transition_type);

};

#endif // SCENE_MANAGER_HPP
