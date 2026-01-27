#include "../../include/framework/SceneManager.hpp"
#include "../../include/framework/GameScene.hpp"
#include "../../include/framework/CutScene.hpp"
#include "../../include/framework/MenuScene.hpp"
#include "../../include/framework/SettingsScene.hpp"

SceneManager::SceneManager()
{
    // Create all shared resources
    m_resolution_scale = std::make_shared<sf::Vector2f>(float(SCALE), float(SCALE));
    m_window = std::make_shared<sf::RenderWindow>();
    m_whole_screen_texture = std::make_shared<sf::RenderTexture>(m_base_resolution);
    m_whole_screen_sprite = std::make_shared<sf::Sprite>(m_whole_screen_texture->getTexture());
    m_clock = std::make_shared<sf::Clock>();
    m_camera = std::make_shared<Camera>(
        sf::Vector2f(WINDOW_WIDTH, WINDOW_HEIGHT),
        sf::Vector2f(WINDOW_WIDTH/2.f, WINDOW_HEIGHT/2.f)
    );
    
    setup_window();
}

SceneManager::~SceneManager()
{
    // Cleanup current scene
    if(m_current_scene) {
        m_current_scene->on_exit();
    }
}

void SceneManager::setup_window()
{
    m_context_settings.antiAliasingLevel = 0;
    m_window->create(sf::VideoMode({SCALED_WIDTH, SCALED_HEIGHT}), GAME_NAME,
                     sf::Style::Default, sf::State::Windowed, m_context_settings);
    
    m_whole_screen_texture->setView(m_camera->get_view());
    m_camera->set_transition(TransitionFunction::Linear);
    m_camera->set_transition_duration(0.07);
    
    m_whole_screen_texture->setSmooth(false);
    m_whole_screen_sprite->setScale(*m_resolution_scale);
    m_whole_screen_sprite->setPosition({0, 0});
}

void SceneManager::setup_scene_resources(std::shared_ptr<Scene> scene)
{
    scene->set_window(m_window);
    scene->set_render_texture(m_whole_screen_texture);
    scene->set_render_sprite(m_whole_screen_sprite);
    scene->set_camera(m_camera);
    scene->set_clock(m_clock);
    scene->set_resolution_scale(m_resolution_scale);
}

void SceneManager::add_scene(SceneType scene, const std::string& key)
{
    std::shared_ptr<Scene> new_scene;
    
    switch(scene)
    {
        case SceneType::Game:
            new_scene = std::make_shared<GameScene>();
            break;
        case SceneType::Cutscene:
            new_scene = std::make_shared<CutScene>();
            break;
        case SceneType::Menu:
            new_scene = std::make_shared<MenuScene>();
            break;
        case SceneType::Settings:
            new_scene = std::make_shared<SettingsScene>();
            break;
    }
    setup_scene_resources(new_scene);
    m_scenes[key] = new_scene;
}

void SceneManager::run_scene(const std::string& key)
{
    auto it = m_scenes.find(key);
    if(it == m_scenes.end()) {
        std::cerr << "Scene '" << key << "' not found!" << std::endl;
        return;
    }
    
    m_current_scene = it->second;
    m_current_scene->run(144);
}

void SceneManager::swap_scene(const std::string& key)
{
    if(m_current_scene)
        m_current_scene->on_exit();
    
    auto it = m_scenes.find(key);
    if(it == m_scenes.end()) {
        std::cerr << "Scene '" << key << "' not found!" << std::endl;
        return;
    }

    m_current_scene = it->second;
    m_current_scene->run(144);
}

void SceneManager::delete_scene(const std::string& key)
{
    // Don't delete current scene
    if(m_current_scene && m_scenes[key] == m_current_scene) {
        std::cerr << "Cannot delete active scene!" << std::endl;
        return;
    }
    m_scenes.erase(key);
}
