#include "../../include/scenes/SceneManager.hpp"

SceneManager::SceneManager() :
    m_render_texture(m_base_resolution),
    m_render_sprite(m_render_texture.getTexture()),
    m_camera(sf::Vector2f(WINDOW_WIDTH, WINDOW_HEIGHT),
             sf::Vector2f(float(WINDOW_WIDTH)/2.f, float(WINDOW_HEIGHT)/2.f))
    
{
    m_render_window.create(sf::VideoMode({SCALED_WIDTH, SCALED_HEIGHT}), GAME_NAME,
            sf::Style::Default, sf::State::Windowed);
    m_render_texture.setView(m_camera.get_view());
    m_camera.set_transition(TransitionFunction::Linear);
    m_camera.set_transition_duration(0.07);

    m_render_texture.setSmooth(false);
    m_render_sprite.setScale(m_resolution_scale);
    m_render_sprite.setPosition({0, 0});
}

void SceneManager::add_scene(std::string key, std::shared_ptr<Scene> scene)
{
    m_scenes[key] = scene;
}

void SceneManager::delete_scene(std::string key)
{
    m_scenes.erase(key);
}

void SceneManager::run_scene(int frame_per_seconds)
{
    // RUN USES THE FIXED TIME STEPS APPROACH
    sf::Time time_since_last_update = sf::Time::Zero;
    sf::Time time_per_frame = sf::seconds(1.f / frame_per_seconds);

    if(!m_current_scene) return; // Leave method if no current scene

    m_current_scene->on_enter();

    while(m_render_window.isOpen())
    {
        m_current_scene->process_events();
        
        bool repaint = false;

        time_since_last_update += m_clock.restart();
        while(time_since_last_update > time_per_frame)
        {
            time_since_last_update -= time_per_frame;
            repaint = true;
            m_current_scene->update(time_per_frame);
        }
        if(repaint)
        {
            m_current_scene->render();
        }
    }
}

void SceneManager::swap_scene(std::string key, SceneTransitionType transition_type)
{
    m_current_scene = m_scenes[key];
}

