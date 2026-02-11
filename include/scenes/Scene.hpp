#ifndef SCENE_HPP
#define SCENE_HPP

#include "../Includes.hpp"

class Scene
{
protected:
    sf::RenderWindow* m_render_window;
    sf::RenderTexture* m_render_texture;
    sf::Sprite* m_render_sprite;

    void set_render_resources(sf::RenderWindow* rw,
                                sf::RenderTexture* rt,
                                sf::Sprite* rs);

public:
    virtual ~Scene() = default;

    virtual void on_enter();
    virtual void on_exit();
    virtual void process_events();
    virtual void update(sf::Time delta_time);
    virtual void render();

    friend class SceneManager;
};

#endif // SCENE_HPP
