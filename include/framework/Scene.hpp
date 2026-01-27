#ifndef SCENE_HPP
#define SCENE_HPP

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

class Scene
{
protected:
    // Shared pointers to resources (not owned by Scene)
    std::shared_ptr<sf::RenderWindow> m_window = nullptr;
    std::shared_ptr<sf::RenderTexture> m_render_texture = nullptr;
    std::shared_ptr<sf::Sprite> m_render_sprite = nullptr;
    std::shared_ptr<Camera> m_camera = nullptr;
    std::shared_ptr<sf::Clock> m_clock = nullptr;
    std::shared_ptr<sf::Vector2f> m_resolution_scale = nullptr;
    
public:
    Scene();

    // Lifecycle methods
    virtual void on_enter() = 0;
    virtual void on_exit() = 0;
    
    // Game loop methods
    virtual void handle_event() = 0;
    virtual void update(sf::Time delta_time) = 0;
    virtual void render() = 0;
    
    // Scene's own game loop
    void run(int frame_per_seconds);
    
    // Setters for shared resources
    void set_window(std::shared_ptr<sf::RenderWindow> window) { m_window = window; }
    void set_render_texture(std::shared_ptr<sf::RenderTexture> texture) { m_render_texture = texture; }
    void set_render_sprite(std::shared_ptr<sf::Sprite> sprite) { m_render_sprite = sprite; }
    void set_camera(std::shared_ptr<Camera> camera) { m_camera = camera; }
    void set_clock(std::shared_ptr<sf::Clock> clock) { m_clock = clock; }
    void set_resolution_scale(std::shared_ptr<sf::Vector2f> scale) { m_resolution_scale = scale; }
};

#endif
