#ifndef MENU_SCENE_HPP
#define MENU_SCENE_HPP

#include "Scene.hpp"
#include "../framework/EventBus.hpp"

class MenuScene : public Scene
{
private:
    ListenerHandle m_switch_to_game_handle;

public:
    MenuScene();
    ~MenuScene();

    void on_enter() override;
    void on_exit() override;
    void process_events() override;
    void update(sf::Time delta_time) override;
    void render() override;
};

#endif // MENU_SCENE_HPP
