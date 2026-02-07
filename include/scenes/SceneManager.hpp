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

    Camera m_camera;

    std::unordered_map<std::string, std::shared_ptr<Scene>> m_scenes;
    std::shared_ptr<Scene> m_current_scene;

    const std::string GAME_NAME = "undefined";

public:
    SceneManager();

    void add_scene(std::string key, std::shared_ptr<Scene> scene);

    void delete_scene(std::string key);

    void run_scene(int frame_per_seconds);

    void swap_scene(std::string key, SceneTransitionType transition_type);

};

#endif // SCENE_MANAGER_HPP
