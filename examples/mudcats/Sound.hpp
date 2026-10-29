/**
 * Sound.hpp  -  Mud Cats
 *
 * All of the demo's audio.
 *
 *   play("rifle")              picks rifle_1, rifle_2 or rifle_3 at random,
 *                              so repeated shots never sound identical
 *   play_at("mg", pos)         quieter with distance from the listener
 *   set_loop("flamer_loop", v) a loop whose volume eases toward a target
 *   set_intensity(0..1)        how much fighting there is. Past a point the
 *                              combat track comes in, from its top each
 *                              time the fighting starts, and only lets go
 *                              once things have been quiet a while
 *   set_combat_track(t)        which combat track: the meadow's, the town's
 *                              or the forest's, each its own piece
 *   set_calm(on)               the calm piece, which is only the title's
 *   lock_to_music(loop)        a loop kept in time with the combat track
 *   punch_in()                 the combat track at once, at full: for an ambush
 *   deafen(seconds)            a shell landed close: everything drops away
 *                              under a ringing tone, then slowly returns
 *   set_volume(bus, v)         the player's own levels: master, music, effects
 *                              and voices, kept with the rest of the
 *                              Settings between runs
 *
 * Voices are mixed as a crowd, not a sum: only a few men can be heard
 * screaming at once, the loudest of them, and each extra voice makes the
 * rest a little quieter, so ten men burning sound like more of them rather
 * than louder.
 *
 * Missing files and a missing audio device are both harmless.
 */

#ifndef MUDCATS_SOUND_HPP
#define MUDCATS_SOUND_HPP

#include <SFML/Audio/Music.hpp>
#include <SFML/Audio/Sound.hpp>
#include <SFML/System/Vector2.hpp>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace mudcats
{

class SoundBank
{
private:
    struct Loop
    {
        std::unique_ptr<sf::Sound> sound;
        float volume{0.0f};
        float target{0.0f};
    };

    struct Voice
    {
        std::unique_ptr<sf::Sound> sound;
        int   kind{0};              // 0 everything else, 1 a man shouting, 2 a man in pain
        float level{0.0f};          // how loud it was asked for, before the player's levels
    };
    std::vector<Voice> m_voices;
    std::map<std::string, Loop>             m_loops;
    std::map<std::string, std::vector<std::string>> m_variants;

    sf::Music m_calm;
    bool  m_calm_wanted{false};
    float m_calm_level{0.0f};
    std::string m_locked;           // the loop that keeps time with the music, if any
    sf::Music m_front;
    sf::Music m_forest;             // the forest's combat track
    sf::Music m_charge;             // the charge's
    sf::Music m_town;               // Kalvo's
    bool  m_forest_ready{false};
    bool  m_charge_ready{false};
    bool  m_town_ready{false};
    bool  m_music_ready{false};
    bool  m_front_ready{false};
    bool  m_music_on{true};
    bool  m_playing{false};
    float m_intensity{0.0f};
    float m_intensity_target{0.0f};
    bool  m_battle{false};          // the combat track is wanted
    float m_battle_mix{0.0f};       // 0 calm, 1 combat
    float m_quiet{0.0f};            // seconds the fighting has been light
    float m_front_volume{0.0f};
    float m_front_target{0.0f};
    float m_music_fade{0.0f};
    float m_music_level{0.0f};

    sf::Vector2f m_listener;
    float m_deaf{0.0f};
    float m_deaf_total{1.0f};

    SoundBank();

    static std::string path(const std::string& name);
    const std::vector<std::string>& variants(const std::string& name);
    [[nodiscard]] float duck() const;

    int   m_track{0};               // Track, as an int: which combat track is in use
    float m_pitch{1.0f};
    float m_pitch_target{1.0f};

public:
    enum class Bus { MASTER, MUSIC, EFFECTS, VOICES };
    static constexpr int BUSES = 4;

    static SoundBank& get();

    // The player's levels live in the Settings, with everything else they set.
    [[nodiscard]] float volume(Bus bus) const;
    [[nodiscard]] float volume_of(Bus bus) const;        // with the master level in it
    void set_volume(Bus bus, float value);
    void save_settings() const;

    // `pitch` is the speaker's own: each man's voice sits a little higher or lower than the next man's.
    void play(const std::string& name, float volume = 1.0f, float pitch_jitter = 0.0f, float pitch = 1.0f);
    void play_at(const std::string& name, sf::Vector2f pos, float volume = 1.0f, float pitch_jitter = 0.04f, float pitch = 1.0f);

    /*
     * A gunshot: the near sound close by, the far one beyond ~260 pixels.
     */
    void shot_at(const std::string& near, const std::string& far, sf::Vector2f pos, float volume = 1.0f);

    void set_loop(const std::string& name, float volume);
    void stop_loops();

    void set_listener(sf::Vector2f pos) { m_listener = pos; }
    [[nodiscard]] sf::Vector2f listener() const { return m_listener; }
    [[nodiscard]] float attenuation(sf::Vector2f pos) const;

    void start_music();
    void stop_music();
    enum class Track { NONE, CHARGE, TOWN, FOREST };
    void set_combat_track(Track track);
    void set_music_pitch(float pitch) { m_pitch_target = pitch; }     // the charge: slower as they walk, faster as they run
    void set_intensity(float value) { m_intensity_target = value; }
    // The calm piece: wanted on the title, and nowhere else.
    void set_calm(bool wanted) { m_calm_wanted = wanted; }
    // A loop that keeps time with the combat track: its speed, its beat. Empty for none.
    void lock_to_music(const std::string& loop) { m_locked = loop; }
    // The combat track in at once, at full, from its top: an ambush shouldn't fade in.
    void punch_in() { m_battle = true; m_quiet = 0.0f; m_battle_mix = 1.0f; m_intensity_target = std::max(m_intensity_target, 1.0f); }
    void set_front(float volume) { m_front_target = volume; }
    void toggle_music_mute() { m_music_on = !m_music_on; }
    [[nodiscard]] bool music_muted() const { return !m_music_on; }

    void deafen(float seconds);
    [[nodiscard]] float deafness() const;

    void update(float dt);
};

}

#endif
