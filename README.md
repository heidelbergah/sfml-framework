SFML 3.0 Framework designed to make game development much more organized and easier to manage and scale

## Next Steps

[x] - Add particles and particle managers
[x] - Create interpolated template for smooth transitional movements
[X] - Update GUI elements to have uploadable textures
[X] - Update Frame element to handle new widget handling
[X] - Add shader supprt + shader manager
[] - Add a scene management system
[] - Add cutscene support
[] - Add camera effects: shake, movement, movie black bars
    [] - Needs refining, mouse location needs to be relative to view
    [] - May need to refactor how mouse position is calculated for GUI widgets
         Also critical that programs can be built WITHOUT a camera, so likely need
         to pass in mouse coordinates which are generated in Game.cpp/hpp
    [] - Issue discoverd with view; mouse position is accurate the closer to top left
         corner of the screen you are, gets less accurate the further away you go.
         Maybe a scaling issue? Unsure. Only the y mouse coords seems effected...
[] - Tilemap manager
    [] - Create WFC algorithm for map generation
    [] - Make JSON load/save manager
[] - Update whole screen texture to ensure eveyrthing is displayed properly when
     moving the camera.


## Optional

[X] - Frame transition function being weird... might not even be necessary to fix :p

