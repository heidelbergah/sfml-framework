#ifndef SCENE_MANAGER_HPP
#define SCENE_MANAGER_HPP

#include "../Includes.hpp"
#include "Scene.hpp"

class GameScene;
class CutScene;
class MenuScene;
class SettingsScene;

enum class SceneType
{
    Game,
    Cutscene,
    Menu,
    Settings
};

class SceneManager
{
private:
    sf::Vector2u m_base_resolution = {WINDOW_WIDTH, WINDOW_HEIGHT};
    std::shared_ptr<sf::Vector2f> m_resolution_scale;
    
    sf::ContextSettings m_context_settings;
    std::shared_ptr<sf::RenderWindow> m_window;
    std::shared_ptr<sf::RenderTexture> m_whole_screen_texture;
    std::shared_ptr<sf::Sprite> m_whole_screen_sprite;
    std::shared_ptr<sf::Clock> m_clock;
    const std::string GAME_NAME = "My Game";
    
    // Game systems
    std::shared_ptr<Camera> m_camera;
    
    // Scene management
    std::unordered_map<std::string, std::shared_ptr<Scene>> m_scenes;
    std::shared_ptr<Scene> m_current_scene = nullptr;
    
    void setup_window();
    void setup_scene_resources(std::shared_ptr<Scene> scene);

public:
    SceneManager();
    ~SceneManager();
    
    void add_scene(SceneType scene, const std::string& key);
    void run_scene(const std::string& key);
    void swap_scene(const std::string& key);
    void delete_scene(const std::string& key);
    
    std::shared_ptr<Scene> get_current_scene() { return m_current_scene; }
    std::shared_ptr<Camera> get_camera() { return m_camera; }
    std::shared_ptr<sf::RenderWindow> get_window() { return m_window; }
};

#endif
