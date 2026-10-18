/**
 * MenuScene.hpp  -  Void Drift
 */

#ifndef MENU_SCENE_HPP
#define MENU_SCENE_HPP

#include "Scene.hpp"
#include "../framework/EventBus.hpp"
#include "../gui/WidgetManager.hpp"
#include "../gui/Button.hpp"
#include "../Starfield.hpp"

class MenuScene : public Scene
{
private:
    sf::Vector2f m_resolution_scale = {static_cast<float>(SCALE), static_cast<float>(SCALE)};

    sf::Font      m_font;
    WidgetManager m_widgets;
    Starfield     m_stars;

    sf::Text* m_title    = nullptr;
    sf::Text* m_tagline  = nullptr;
    sf::Text* m_controls = nullptr;
    sf::Text* m_hiscore  = nullptr;

    float m_pulse = 0.f;

    ListenerHandle m_to_game_handle;

public:
    MenuScene();
    ~MenuScene();

    void on_enter()       override;
    void on_exit()        override;
    void process_events() override;
    void update(sf::Time dt) override;
    void render()         override;
};

#endif
