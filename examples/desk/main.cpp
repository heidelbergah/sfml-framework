// Desk prototype for Mud Cats Story.
//
// The forward base's table, with papers you can pick up, drag around and
// read. Each page is a Frame; DeskItem draws it as a physical sheet.
//
//     make EXAMPLE=desk run

#include "../../include/scenes/SceneManager.hpp"

#include "DeskScene.hpp"

int main()
{
    SceneManager scenes("Mud Cats - desk");
    scenes.add_scene("desk", std::make_shared<desk::DeskScene>());
    scenes.swap_scene("desk", SceneTransitionType::INSTANT);
    scenes.run(60);
}
