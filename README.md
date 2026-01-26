SFML 3.0 Framework designed to make game development much more organized and easier to manage and scale

## Next Steps

[x] - Add particles and particle managers
[x] - Create interpolated template for smooth transitional movements
[X] - Update GUI elements to have uploadable textures
[X] - Update Frame element to handle new widget handling
[X] - Add shader supprt + shader manager
[X] - Add camera effects: shake, movement

[] - Add a scene management system
[] - Add cutscene support; movie black bars too!
[] - Tilemap manager
    [] - Create WFC algorithm for map generation
    [] - Make JSON load/save manager
[] - Sound/music manager

## BUGS

[] - Shaking mechanics could use a little love
[] - Better interpolation for camera following needs implementation
[] - Shaking needs to stack when multiple sources are shaking the camera... needs to look natural

## Scene Management Gameplan

The scenes and scenemanager should follow a layout somewhat like this:

- General "Scene" class:
    - private:
        - widget manager
        - shader manager
        - particle manager
        - ALL the managers
        - Everything a scene needs to work

    - public:
        - on_open()
        - on_exit()
        - handle_event()
        - update()
        - render()

- "SceneManager":
    - private:
        - unordered map of scenes
    - public:
        - run_scene
        - swap_scenes
        - add_scene
        - delete_scene

Scenes are based heavily on Game.cpp and Game.hpp. Scenes
will also replace Game.cpp and Game.hpp.

Because it's way easier to write code straight into a scene than using
"add" and "remove" functions, i'll likely make children who inherit from
a base Scene class and specialize. A GameScene, SettingsScene, CutsceneScene, etc.

Should be relatively simple to implement. Everything from Game.cpp/hpp needs to
migrate properly, and all scenes should be able to be entered. Shouldn't take more
than a day (mf thinks he's a 10x dev LMAOOOO).

