/**
 * main.cpp  -  Tetris
 *
 * A Tetris clone built entirely on top of the framework: no engine
 * changes, only scenes, assets and a shader.
 *
 * Guideline rules, seven-bag, SRS with wall kicks, hold, five-piece
 * preview, ghost, hard drop, lock delay with move resets, T-spins,
 * back-to-back and combos.
 */

#include "../../include/scenes/SceneManager.hpp"

#include "GameOverScene.hpp"
#include "MenuScene.hpp"
#include "PlayScene.hpp"

int main()
{
    SceneManager sm("Tetris");

    // Restrained: the gruvbox palette is already fairly bright, so the
    // threshold sits high and only the clear flashes really bloom.
    sm.enable_bloom("shaders");
    sm.get_bloom().set_threshold(0.80f);
    sm.get_bloom().set_soft_knee(0.18f);
    sm.get_bloom().set_intensity(0.42f);

    sm.set_fade_duration(0.25f);

    sm.add_scene("menu",     std::make_shared<tetris::MenuScene>());
    sm.add_scene("game",     std::make_shared<tetris::PlayScene>());
    sm.add_scene("gameover", std::make_shared<tetris::GameOverScene>());

    sm.swap_scene("menu", SceneTransitionType::INSTANT);

    sm.run(60);
    return 0;
}
