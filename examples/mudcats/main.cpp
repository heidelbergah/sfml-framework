/**
 * main.cpp  -  Mud Cats infantry demo
 *
 * Infantry, top-down, in three battles you can fight from either side:
 * a bayonet charge across the Sava meadow, house to house through Kalvo,
 * and an ambush on the Lesna road. Pick one man, and fight it through.
 *
 *     make EXAMPLE=mudcats run
 *
 * or with CMake, on Linux, macOS or Windows: see the README.
 *
 * Sounds and music are synthesised by tools/sfx/generate_mudcats.py.
 */

#include "../../include/scenes/SceneManager.hpp"

#include "Battle.hpp"
#include "Screens.hpp"
#include "Settings.hpp"

#include <filesystem>
#include <system_error>

namespace
{
    /*
     * The game finds everything as assets/... and shaders/..., relative to
     * where it runs. Started from a file manager, or from somewhere else
     * in a terminal, that isn't the right place, so look for it: next to
     * the program, above it (build/bin in this repository), or in a macOS
     * bundle's Resources.
     */
    void find_home(const char* argv0)
    {
        namespace fs = std::filesystem;
        std::error_code e;
        auto has_assets = [&e](const fs::path& dir) { return fs::exists(dir / "assets" / "fonts" / "pixel-font-bitmap.ttf", e); };
        if(has_assets(fs::current_path(e))) return;
        if(!argv0 || !*argv0) return;
        fs::path const exe = fs::absolute(fs::path(argv0), e);
        fs::path const dir = exe.parent_path();
        for(const fs::path& candidate : {dir, dir.parent_path(), dir.parent_path().parent_path(), dir.parent_path() / "Resources"})
            if(has_assets(candidate))
            {
                fs::current_path(candidate, e);
                return;
            }
    }
}

int main(int argc, char** argv)
{
    find_home(argc > 0 ? argv[0] : nullptr);

    mudcats::Settings const& settings = mudcats::Settings::get();
    SceneManager sm("Mud Cats", settings.fullscreen);
    if(!settings.fullscreen && settings.window_scale > 0) settings.apply_display(sm);
    // Keep the picture's shape on a screen that isn't 16:9.
    sm.set_scaling_mode(ScalingMode::FIT);
    if(sf::Image icon; icon.loadFromFile("assets/mudcats/icon.png")) sm.set_icon(icon);

    // Only muzzle flashes, tracers, fire and explosions should glow.
    sm.enable_bloom("shaders");
    sm.get_bloom().set_threshold(0.78f);
    sm.get_bloom().set_soft_knee(0.2f);
    sm.get_bloom().set_intensity(0.5f);
    sm.set_bloom_enabled(false);
    sm.set_fade_duration(0.6f);

    sm.add_scene("title",    std::make_shared<mudcats::TitleScene>());
    sm.add_scene("briefing", std::make_shared<mudcats::BriefingScene>());
    sm.add_scene("battle",   std::make_shared<mudcats::Battle>());
    sm.add_scene("debrief",  std::make_shared<mudcats::DebriefScene>());

    sm.swap_scene("title", SceneTransitionType::INSTANT);
    sm.run(60);
    return 0;
}
