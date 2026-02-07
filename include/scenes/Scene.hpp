#ifndef SCENE_HPP
#define SCENE_HPP

#include "../Includes.hpp"

class Scene
{
protected:
    std::shared_ptr<sf::RenderWindow> m_render_window;
    std::shared_ptr<sf::RenderTexture> m_render_texture;
    std::shared_ptr<sf::Sprite> m_render_sprite;

    void load_manager_resources(sf::RenderWindow& rw,
                                sf::RenderTexture& rt,
                                sf::Sprite rs);

public:
    virtual ~Scene() = default;

    virtual void on_enter();
    virtual void on_exit();
    virtual void process_events();
    virtual void update(sf::Time delta_time);
    virtual void render();
};

#endif // SCENE_HPP
