/**
 * main.cpp
 *
 * Framework demo entry point.
 *
 * SceneManager owns the window, the render target and the loop. Add
 * your scenes, pick the first one, and run.
 *
 *     SceneManager sm("Void Drift");
 *     sm.add_scene("menu",     std::make_shared<MenuScene>());
 *     sm.add_scene("game",     std::make_shared<GameScene>());
 *     sm.add_scene("perks",    std::make_shared<PerkScene>());
 *     sm.add_scene("gameover", std::make_shared<GameOverScene>());
 *     sm.swap_scene("menu", SceneTransitionType::INSTANT);
 *     sm.run(60);
 *
 * Scenes switch themselves by publishing an EventType -- SceneManager
 * subscribes to SWITCH_TO_GAME, SWITCH_TO_MAIN_MENU, SWITCH_TO_SETTINGS,
 * SWITCH_TO_GAME_OVER and SWITCH_TO_PERK_SELECT and maps them to the
 * keys "game", "menu", "settings", "gameover" and "perks".
 */

#include "../../include/scenes/SceneManager.hpp"
#include "DemoScene.hpp"

int main()
{
    SceneManager sm("SFML Framework Demo");

    // Optional. Falls back to unbloomed rendering if the shaders or the
    // GPU are unavailable.
    sm.enable_bloom("shaders");

    sm.add_scene("demo", std::make_shared<DemoScene>());
    sm.swap_scene("demo", SceneTransitionType::INSTANT);

    sm.run(60);
    return 0;
}
