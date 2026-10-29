/**
 * Settings.hpp  -  Mud Cats
 *
 * What the player has set, kept between runs in a small text file:
 *
 *   the four sound levels      master, music, effects, voices (0 to 1)
 *   fullscreen                 or a window, at some whole multiple of
 *   window_scale               960x540 (0: the largest that fits)
 *   seen_how_to                the how-to-play card has been shown once
 *
 * The file lives where each system keeps such things: %APPDATA%\MudCats
 * on Windows, ~/Library/Application Support/MudCats on macOS, and
 * $XDG_CONFIG_HOME/mudcats (or ~/.config/mudcats) everywhere else. A
 * missing or unreadable file just means the defaults.
 */

#ifndef MUDCATS_SETTINGS_HPP
#define MUDCATS_SETTINGS_HPP

#include <string>

class SceneManager;

namespace mudcats
{

struct Settings
{
    static constexpr int BUSES = 4;
    float volume[BUSES]{1.0f, 1.0f, 1.0f, 1.0f};
    bool  fullscreen{false};
    int   window_scale{0};
    bool  seen_how_to{false};

    static Settings& get();
    [[nodiscard]] static std::string path();
    void load();
    void save() const;

    // Puts the window the way the settings say.
    void apply_display(SceneManager& manager) const;
};

}

#endif
