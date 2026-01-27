#ifndef MENUSCENE_HPP
#define MENUSCENE_HPP

#include "Scene.hpp"

class MenuScene : public Scene
{
private:
    // Add your data

public:
    MenuScene();

    void on_enter() override;

    void on_exit() override;

    void handle_event() override;

    void update(sf::Time delta_time) override;

    void render() override;
};

#endif // MENUSCENE_HPP
