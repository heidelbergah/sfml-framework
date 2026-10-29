/**
 * Settings.cpp  -  Mud Cats
 */

#include "Settings.hpp"

#include "../../include/scenes/SceneManager.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace mudcats
{

namespace
{
    const char* const BUS_KEYS[] = {"master", "music", "effects", "voices"};
}

Settings& Settings::get()
{
    static Settings settings = []
    {
        Settings s;
        s.load();
        return s;
    }();
    return settings;
}

std::string Settings::path()
{
    namespace fs = std::filesystem;
    fs::path dir;
#if defined(_WIN32)
    if(const char* appdata = std::getenv("APPDATA")) dir = fs::path(appdata) / "MudCats";
#elif defined(__APPLE__)
    if(const char* home = std::getenv("HOME")) dir = fs::path(home) / "Library" / "Application Support" / "MudCats";
#else
    if(const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) dir = fs::path(xdg) / "mudcats";
    else if(const char* home = std::getenv("HOME")) dir = fs::path(home) / ".config" / "mudcats";
#endif
    if(dir.empty()) dir = ".";
    return (dir / "settings.txt").string();
}

void Settings::load()
{
    std::ifstream in(path());
    std::string line;
    while(std::getline(in, line))
    {
        std::istringstream words(line);
        std::string key;
        float value = 0.0f;
        if(!(words >> key >> value)) continue;
        for(int i = 0; i < BUSES; ++i)
            if(key == BUS_KEYS[i]) volume[i] = std::clamp(value, 0.0f, 1.0f);
        if(key == "fullscreen") fullscreen = value != 0.0f;
        if(key == "window_scale") window_scale = std::clamp(static_cast<int>(value), 0, 8);
        if(key == "seen_how_to") seen_how_to = value != 0.0f;
    }
}

void Settings::save() const
{
    std::error_code ignored;
    std::filesystem::path const file(path());
    if(file.has_parent_path()) std::filesystem::create_directories(file.parent_path(), ignored);
    std::ofstream out(file);
    if(!out) return;
    out << "# Mud Cats: your settings. Sound levels are 0 to 1; a window_scale of 0 is the largest that fits.\n";
    for(int i = 0; i < BUSES; ++i) out << BUS_KEYS[i] << " " << volume[i] << "\n";
    out << "fullscreen " << (fullscreen ? 1 : 0) << "\n";
    out << "window_scale " << window_scale << "\n";
    out << "seen_how_to " << (seen_how_to ? 1 : 0) << "\n";
}

void Settings::apply_display(SceneManager& manager) const
{
    manager.set_window_mode(fullscreen, static_cast<unsigned>(window_scale));
}

}
