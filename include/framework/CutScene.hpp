#ifndef CUTSCENE_HPP
#define CUTSCENE_HPP

#include "Scene.hpp"

class CutScene : public Scene
{
private:
    // Add your data

public:
    CutScene();

    void on_enter() override;

    void on_exit() override;

    void handle_event() override;

    void update(sf::Time delta_time) override;

    void render() override;
};

#endif // CUTSCENE_HPP
