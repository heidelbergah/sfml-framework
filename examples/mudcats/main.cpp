/**
 * main.cpp  -  Mud Cats infantry demo
 *
 * Infantry, top-down: a Monteran platoon of twenty-five against about forty
 * Rechivii, in the trenches of the Ochre line or the fields at Halvek.
 * Pick one man, and fight it through.
 *
 *     make EXAMPLE=mudcats run
 *
 * Sounds and music are synthesised by tools/sfx/generate_mudcats.py.
 */

#include "../../include/scenes/SceneManager.hpp"

#include "Battle.hpp"
#include "Screens.hpp"

int main()
{
    SceneManager sm("Mud Cats");

    // Only muzzle flashes, tracers, fire and explosions should glow.
    sm.enable_bloom("shaders");
    sm.get_bloom().set_threshold(0.78f);
    sm.get_bloom().set_soft_knee(0.2f);
    sm.get_bloom().set_intensity(0.5f);
    sm.set_bloom_enabled(false);
    sm.set_fade_duration(0.6f);

    sm.add_scene("briefing", std::make_shared<mudcats::BriefingScene>());
    sm.add_scene("battle",   std::make_shared<mudcats::Battle>());
    sm.add_scene("debrief",  std::make_shared<mudcats::DebriefScene>());

    sm.swap_scene("briefing", SceneTransitionType::INSTANT);
    sm.run(60);
    return 0;
}
