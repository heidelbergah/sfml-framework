/**
 * main.cpp
 *
 * Written by James Bouchat
 */

#include "../include/scenes/SceneManager.hpp"
#include "../include/scenes/GameScene.hpp"
#include "../include/scenes/MenuScene.hpp"

int main()
{
    SceneManager sm;
    std::shared_ptr<GameScene> game_scene = std::make_shared<GameScene>();
    std::shared_ptr<MenuScene> menu_scene = std::make_shared<MenuScene>();
    sm.add_scene("menu", menu_scene);
    sm.add_scene("game", game_scene);
    sm.swap_scene("menu", SceneTransitionType::INSTANT);
    sm.run_scene(60);

    return 0;
}
