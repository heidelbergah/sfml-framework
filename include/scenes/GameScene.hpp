#ifndef GAME_SCENE_HPP
#define GAME_SCENE_HPP

#include "Scene.hpp"
#include "../framework/ParticleSystemManager.hpp"
#include "../framework/ShaderManager.hpp"
#include "../framework/EventBus.hpp"
#include "../gui/WidgetManager.hpp"

class GameScene : public Scene
{
private:
    WidgetManager m_widgets;
    ParticleSystemManager m_particle_system_manager;
    ShaderManager m_shader_manager;

    ListenerHandle m_pause_handle;

public:
    GameScene();

    void on_enter() override;
    void on_exit() override;
    void process_events() override;
    void update(sf::Time delta_time) override;
    void render() override;

};

#endif // GAME_SCENE_HPP
