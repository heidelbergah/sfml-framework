#include "../../include/scenes/SceneManager.hpp"

SceneManager::SceneManager() :
    m_render_texture(m_base_resolution),
    m_render_sprite(m_render_texture.getTexture())
{
    m_render_window.create(sf::VideoMode({SCALED_WIDTH, SCALED_HEIGHT}), GAME_NAME,
            sf::Style::Default, sf::State::Windowed);
    m_render_texture.setSmooth(false);
    m_render_sprite.setScale(m_resolution_scale);
    m_render_sprite.setPosition({0, 0});

    setup_event_listeners();
}

SceneManager::~SceneManager()
{
    cleanup_event_listeners();
}

void SceneManager::setup_event_listeners()
{
    m_switch_to_game_handle = EventBus::subscribe(
        EventType::SWITCH_TO_GAME,
        [this](const Event& e) {
            request_scene_switch("game", SceneTransitionType::FADE);
        }
    );
    
    m_switch_to_menu_handle = EventBus::subscribe(
        EventType::SWITCH_TO_MAIN_MENU,
        [this](const Event& e) {
            request_scene_switch("menu", SceneTransitionType::FADE);
        }
    );
}

void SceneManager::cleanup_event_listeners()
{
    EventBus::unsubscribe(EventType::SWITCH_TO_GAME, m_switch_to_game_handle);
    EventBus::unsubscribe(EventType::SWITCH_TO_MAIN_MENU, m_switch_to_menu_handle);
}

void SceneManager::add_scene(std::string key, std::shared_ptr<Scene> scene)
{
    scene->set_render_resources(&m_render_window, &m_render_texture, &m_render_sprite);
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
            display_texture_to_window();
        }

        if(m_pending_scene_switch)
        {
            swap_scene(m_next_scene_key, m_next_transition_type);
            m_pending_scene_switch = false;
        }
    }

    if(m_current_scene)
    {
        m_current_scene->on_exit();
    }
}

void SceneManager::swap_scene(std::string key, SceneTransitionType transition_type)
{
    auto it = m_scenes.find(key);
    if(it == m_scenes.end())
    {
        return;
    }

    if(transition_type == SceneTransitionType::FADE)
    {
        perform_fade_transition(it->second);
    }
    else
    {
        // Instant transition
        if(m_current_scene)
        {
            m_current_scene->on_exit();
        }

        m_current_scene = it->second;

        if(m_current_scene)
        {
            m_current_scene->on_enter();
            display_texture_to_window();
        }
    }
}

void SceneManager::perform_fade_transition(std::shared_ptr<Scene> next_scene)
{
    const float fade_duration = 1.4f; // 0.4 seconds for each phase (0.8s total)
    sf::Clock fade_clock;
    
    sf::RectangleShape black_overlay(sf::Vector2f(WINDOW_WIDTH, WINDOW_HEIGHT));
    black_overlay.setFillColor(sf::Color::Black);
    
    // fade out current scene to black
    if(m_current_scene)
    {
        fade_clock.restart();
        while(fade_clock.getElapsedTime().asSeconds() < fade_duration)
        {
            float progress = fade_clock.getElapsedTime().asSeconds() / fade_duration;
            int alpha = static_cast<int>(progress * 255);
            
            m_current_scene->render();
            
            black_overlay.setFillColor(sf::Color(0, 0, 0, alpha));
            m_render_texture.draw(black_overlay);
            display_texture_to_window();
        }
    }
    
    // Switch scenes
    if(m_current_scene)
    {
        m_current_scene->on_exit();
    }
    
    m_current_scene = next_scene;
    
    if(m_current_scene)
    {
        m_current_scene->on_enter();
    }
    
    // fade in new scene from black
    if(m_current_scene)
    {
        fade_clock.restart();
        while(fade_clock.getElapsedTime().asSeconds() < fade_duration)
        {
            float progress = fade_clock.getElapsedTime().asSeconds() / fade_duration;
            int alpha = static_cast<int>((1.0f - progress) * 255);
            
            m_current_scene->render();
            
            black_overlay.setFillColor(sf::Color(0, 0, 0, alpha));
            m_render_texture.draw(black_overlay);
            display_texture_to_window();
        }
    }
}

void SceneManager::request_scene_switch(std::string key, SceneTransitionType transition_type)
{
    m_pending_scene_switch = true;
    m_next_scene_key = key;
    m_next_transition_type = transition_type;
}

void SceneManager::display_texture_to_window()
{
    m_render_texture.display();
    m_render_sprite.setTexture(m_render_texture.getTexture());
    m_render_window.clear();
    m_render_window.draw(m_render_sprite);
    m_render_window.display();
}

