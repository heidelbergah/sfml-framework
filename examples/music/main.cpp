// Music lab: plays everything in assets/music through live effects, on
// an oscilloscope, to try out how the game's music might sound.
//
//     make EXAMPLE=music run

#include "../../include/scenes/SceneManager.hpp"

#include "MusicScene.hpp"

int main()
{
    SceneManager scenes("Music lab");
    scenes.add_scene("music", std::make_shared<music::MusicScene>());
    scenes.swap_scene("music", SceneTransitionType::INSTANT);
    scenes.run(60);
}
