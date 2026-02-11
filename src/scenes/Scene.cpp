#include "../../include/scenes/Scene.hpp"

void Scene::set_render_resources(sf::RenderWindow* window,
                                  sf::RenderTexture* texture,
                                  sf::Sprite* sprite)
{
    m_render_window = window;
    m_render_texture = texture;
    m_render_sprite = sprite;
}

void Scene::on_enter() {}

void Scene::on_exit() {}

void Scene::process_events() {}

void Scene::update(sf::Time delta_time) {}

void Scene::render() {}
