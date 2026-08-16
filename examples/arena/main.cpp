/**
 * main.cpp  -  Ironvale Arena
 *
 * A small top-down tank arena, built to exercise every part of the
 * framework: scenes and fade transitions, tweened widgets, five
 * particle systems, a camera that follows and shakes, bloom, the event
 * bus, pause and slow motion, and Markov-generated callsigns.
 *
 * Scene keys match the EventType values SceneManager already routes, so
 * scenes switch themselves by publishing an event and never need to
 * know about each other.
 */

#include "../../include/scenes/SceneManager.hpp"

#include "ArenaScene.hpp"
#include "GameOverScene.hpp"
#include "MenuScene.hpp"

int main()
{
    SceneManager sm("Ironvale Arena");

    sm.enable_bloom("shaders");
    sm.get_bloom().set_threshold(0.72f);
    sm.get_bloom().set_intensity(0.55f);

    sm.set_fade_duration(0.28f);

    sm.add_scene("menu",     std::make_shared<arena::MenuScene>());
    sm.add_scene("game",     std::make_shared<arena::ArenaScene>());
    sm.add_scene("gameover", std::make_shared<arena::GameOverScene>());

    sm.swap_scene("menu", SceneTransitionType::INSTANT);

    sm.run(60);
    return 0;
}
