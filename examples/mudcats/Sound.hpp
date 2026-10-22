/**
 * Sound.hpp  -  Mud Cats
 *
 * All of the demo's audio.
 *
 *   play("rifle")              picks rifle_1, rifle_2 or rifle_3 at random,
 *                              so repeated shots never sound identical
 *   play_at("mg", pos)         quieter with distance from the listener
 *   set_loop("flamer_loop", v) a loop whose volume eases toward a target
 *   set_intensity(0..1)        crossfades the calm and combat music, which
 *                              run in lockstep so the switch is seamless
 *   deafen(seconds)            a shell landed close: everything drops away
 *                              under a ringing tone, then slowly returns
 *
 * Missing files and a missing audio device are both harmless.
 */

#ifndef MUDCATS_SOUND_HPP
#define MUDCATS_SOUND_HPP

#include <SFML/Audio/Music.hpp>
#include <SFML/Audio/Sound.hpp>
#include <SFML/System/Vector2.hpp>

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

    std::vector<std::unique_ptr<sf::Sound>> m_voices;
    std::map<std::string, Loop>             m_loops;
    std::map<std::string, std::vector<std::string>> m_variants;

    sf::Music m_calm;
    sf::Music m_combat;
    sf::Music m_front;
    bool  m_music_ready{false};
    bool  m_front_ready{false};
    bool  m_music_on{true};
    bool  m_playing{false};
    float m_intensity{0.0f};
    float m_intensity_target{0.0f};
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

public:
    static SoundBank& get();

    void play(const std::string& name, float volume = 1.0f, float pitch_jitter = 0.0f);
    void play_at(const std::string& name, sf::Vector2f pos, float volume = 1.0f, float pitch_jitter = 0.04f);

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
    void set_intensity(float value) { m_intensity_target = value; }
    void set_front(float volume) { m_front_target = volume; }
    void toggle_music_mute() { m_music_on = !m_music_on; }
    [[nodiscard]] bool music_muted() const { return !m_music_on; }

    void deafen(float seconds);
    [[nodiscard]] float deafness() const;

    void update(float dt);
};

}

#endif
