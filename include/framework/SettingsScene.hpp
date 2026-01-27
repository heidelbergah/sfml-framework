#ifndef SETTINGSSCENE_HPP
#define SETTINGSSCENE_HPP

#include "Scene.hpp"

class SettingsScene : public Scene
{
private:
    // Add your data

public:
    SettingsScene();

    void on_enter() override;

    void on_exit() override;

    void handle_event() override;

    void update(sf::Time delta_time) override;

    void render() override;
};

#endif // SETTINGSSCENE_HPP
