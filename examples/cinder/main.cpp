/**
 * main.cpp  -  Cinder
 *
 * A small game about the last spark in a world gone to ash. Colour only
 * comes back where it is warm. Three levels, a title and an ending.
 *
 *     make EXAMPLE=cinder run
 *
 * Colours are from Ashen by ficd (MIT): https://codeberg.org/ficd/ashen
 * Sounds and music are synthesised by tools/sfx/generate_cinder.py.
 */

#include "../../include/scenes/SceneManager.hpp"

#include "PlayScene.hpp"
#include "Screens.hpp"

int main()
{
    SceneManager sm("Cinder");

    // Only the hottest colours -- yellow and gold -- should glow.
    sm.enable_bloom("shaders");
    sm.get_bloom().set_threshold(0.72f);
    sm.get_bloom().set_soft_knee(0.2f);
    sm.get_bloom().set_intensity(0.55f);
    sm.set_fade_duration(0.4f);

    sm.add_scene("menu", std::make_shared<cinder::MenuScene>());
    sm.add_scene("play", std::make_shared<cinder::PlayScene>());
    sm.add_scene("end",  std::make_shared<cinder::EndScene>());

    sm.swap_scene("menu", SceneTransitionType::INSTANT);
    sm.run(60);
    return 0;
}
