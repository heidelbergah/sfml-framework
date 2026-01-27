/**
 * main.cpp
 *
 * Written by James Bouchat
 */

#include "../include/framework/SceneManager.hpp"

int main()
{
    SceneManager scene_manager;
 
    scene_manager.add_scene(SceneType::Game, "game");
    //scene_manager.add_scene(SceneType::Menu, "menu");
    // scene_manager.add_scene(SceneType::Settings, "settings");
    // scene_manager.add_scene(SceneType::Cutscene, "intro");
    scene_manager.run_scene("game");
    
    return 0;
}
