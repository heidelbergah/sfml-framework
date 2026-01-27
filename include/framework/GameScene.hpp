#ifndef GAMESCENE_HPP
#define GAMESCENE_HPP

#include "Scene.hpp"

class GameScene : public Scene
{
private:
    // Add your data
    WidgetManager m_widgets;
    ParticleSystemManager m_particle_systems;
    ShaderManager m_shaders;

public:
    GameScene();
    
    void on_enter() override;

    void on_exit() override;

    void handle_event() override;

    void update(sf::Time delta_time) override;

    void render() override;
};

#endif // GAMESCENE_HPP
