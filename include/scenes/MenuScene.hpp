#ifndef MENU_SCENE_HPP
#define MENU_SCENE_HPP

#include "Scene.hpp"
#include "../framework/EventBus.hpp"
#include "../gui/WidgetManager.hpp"
#include "../gui/Button.hpp"

class MenuScene : public Scene
{
private:
    ListenerHandle m_switch_to_game_handle;
    
    WidgetManager m_widget_manager;
    sf::Font font;

    sf::Vector2u m_base_resolution = {WINDOW_WIDTH, WINDOW_HEIGHT}; // native resolution
    sf::Vector2f m_resolution_scale = {static_cast<float>(SCALE), static_cast<float>(SCALE)}; // scale for display resolution

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
