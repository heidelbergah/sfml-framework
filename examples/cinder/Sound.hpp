/**
 * Sound.hpp  -  Cinder
 *
 * All of Cinder's audio in one place.
 *
 *   play("ignite")                 a one-shot, from a small pool of voices
 *   set_loop("wind_loop", 0.6)     a loop whose volume eases toward a target,
 *                                  so hazards fade in and out instead of
 *                                  cutting on and off
 *   music                          one ambient bed, toggled with M
 *
 * Buffers come from the framework's asset cache. Missing files and a
 * missing audio device are both harmless: the game just plays silently.
 */

#ifndef CINDER_SOUND_HPP
#define CINDER_SOUND_HPP

#include <SFML/Audio/Music.hpp>
#include <SFML/Audio/Sound.hpp>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace cinder
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

    std::vector<std::unique_ptr<sf::Sound>> m_voices;
    std::map<std::string, Loop>             m_loops;

    sf::Music m_music;
    bool      m_music_ready{false};
    bool      m_music_on{true};
    float     m_music_volume{0.0f};
    float     m_music_target{0.0f};

    SoundBank();

    static std::string path(const std::string& name);

public:
    static SoundBank& get();

    /*
     * `volume` is 0-1. `pitch_jitter` varies the pitch randomly by up to
     * that fraction, so repeated sounds do not grate.
     */
    void play(const std::string& name, float volume = 1.0f, float pitch_jitter = 0.0f);

    /*
     * Target volume for a looping sound. 0 lets it fade out; it keeps
     * running silently so it never restarts with a click.
     */
    void set_loop(const std::string& name, float volume);
    void stop_loops();

    void set_music(bool playing);
    void toggle_music_mute();
    [[nodiscard]] bool music_muted() const { return !m_music_on; }

    /*
     * Eases loop and music volumes. Call once per frame with real time.
     */
    void update(float dt);
};

}

#endif
