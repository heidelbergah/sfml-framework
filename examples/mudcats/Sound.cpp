/**
 * Sound.cpp  -  Mud Cats
 */

#include "Sound.hpp"

#include "Settings.hpp"

#include "../../include/core/AssetCache.hpp"
#include "../../include/core/Random.hpp"

#include <algorithm>
#include <cmath>

namespace mudcats
{

namespace
{
    constexpr std::size_t VOICES       = 40;
    constexpr float       MUSIC_VOLUME = 0.6f;
    constexpr float       HEARING      = 700.0f;    // pixels until a sound is gone
    constexpr int         MAX_PAIN     = 3;         // men heard screaming at once
    constexpr int         MAX_SHOUTS   = 8;         // men heard shouting at once
    constexpr float       VOICE_GAIN   = 0.8f;

    bool starts(const std::string& s, const char* prefix) { return s.rfind(prefix, 0) == 0; }

    // 2 a man in pain, 1 a man shouting, 0 anything else.
    int kind_of(const std::string& name)
    {
        for(const char* p : {"scream", "groan", "sob", "death_cry"}) if(starts(name, p)) return 2;
        for(const char* p : {"grunt", "medic_call", "bark", "charge_cry", "war_cry", "war_roar"}) if(starts(name, p)) return 1;
        return 0;
    }

}

SoundBank::SoundBank()
{
    (void)Assets::sounds();     // outlive us: see Cinder's SoundBank

    m_music_ready = m_calm.openFromFile(path("music_calm_loop"));
    if(m_music_ready)
    {
        m_calm.setLooping(true);
        m_calm.setVolume(0.0f);
    }
    m_forest_ready = m_forest.openFromFile(path("music_forest_loop"));
    if(m_forest_ready)
    {
        m_forest.setLooping(true);
        m_forest.setVolume(0.0f);
    }
    m_charge_ready = m_charge.openFromFile(path("music_charge_loop"));
    if(m_charge_ready)
    {
        m_charge.setLooping(true);
        m_charge.setVolume(0.0f);
    }
    m_town_ready = m_town.openFromFile(path("music_town_loop"));
    if(m_town_ready)
    {
        m_town.setLooping(true);
        m_town.setVolume(0.0f);
    }
    m_front_ready = m_front.openFromFile(path("front_loop"));
    if(m_front_ready)
    {
        m_front.setLooping(true);
        m_front.setVolume(0.0f);
    }
}

SoundBank& SoundBank::get()
{
    static SoundBank bank;
    return bank;
}

std::string SoundBank::path(const std::string& name)
{
    return "assets/mudcats/" + name + ".wav";
}

const std::vector<std::string>& SoundBank::variants(const std::string& name)
{
    auto it = m_variants.find(name);
    if(it != m_variants.end()) return it->second;

    std::vector<std::string> found;
    if(std::filesystem::exists(path(name))) found.push_back(name);
    for(int i = 1; i < 10; ++i)
    {
        std::string const v = name + "_" + std::to_string(i);
        if(!std::filesystem::exists(path(v))) break;
        found.push_back(v);
    }
    return m_variants.emplace(name, std::move(found)).first->second;
}

float SoundBank::duck() const
{
    return 1.0f - 0.8f * deafness();
}

float SoundBank::deafness() const
{
    if(m_deaf <= 0.0f) return 0.0f;
    return std::clamp(m_deaf / std::max(0.1f, m_deaf_total) * 1.6f, 0.0f, 1.0f);
}

float SoundBank::attenuation(sf::Vector2f pos) const
{
    float const d = std::hypot(pos.x - m_listener.x, pos.y - m_listener.y);
    float const k = std::clamp(1.0f - d / HEARING, 0.0f, 1.0f);
    return k * k * 0.85f + (d < 60.0f ? 0.15f : 0.15f * k);
}

void SoundBank::play(const std::string& name, float volume, float pitch_jitter, float pitch)
{
    const auto& names = variants(name);
    if(names.empty() || volume <= 0.01f) return;
    const sf::SoundBuffer* buffer = Assets::sounds().try_get(path(names[static_cast<std::size_t>(Random::integer(0, static_cast<int>(names.size()) - 1))]));
    if(!buffer) return;

    int const kind = kind_of(name);
    float const asked = volume;
    Voice* voice = nullptr;
    if(kind > 0)
    {
        // A crowd, not a sum: past a few at once, the quietest of them gives way, or this one isn't heard.
        int playing = 0;
        Voice* quietest = nullptr;
        for(auto& v : m_voices)
        {
            if(v.kind != kind || v.sound->getStatus() != sf::SoundSource::Status::Playing) continue;
            ++playing;
            if(!quietest || v.level < quietest->level) quietest = &v;
        }
        if(playing >= (kind == 2 ? MAX_PAIN : MAX_SHOUTS))
        {
            if(!quietest || quietest->level >= asked) return;
            quietest->sound->stop();
            voice = quietest;
        }
        volume *= VOICE_GAIN / (1.0f + 0.2f * static_cast<float>(std::min(playing, 4)));
    }

    if(!voice)
        for(auto& v : m_voices)
            if(v.sound->getStatus() != sf::SoundSource::Status::Playing) { voice = &v; break; }
    if(!voice)
    {
        if(m_voices.size() < VOICES)
        {
            m_voices.push_back({std::make_unique<sf::Sound>(*buffer), 0, 0.0f});
            voice = &m_voices.back();
        }
        else
        {
            // Steal the quietest voice.
            voice = &*std::min_element(m_voices.begin(), m_voices.end(),
                [](const Voice& a, const Voice& b) { return a.sound->getVolume() < b.sound->getVolume(); });
            voice->sound->stop();
        }
    }

    bool const ringing = name == "tinnitus";
    float const bus = volume_of(kind == 0 ? Bus::EFFECTS : Bus::VOICES);
    voice->kind = kind;
    voice->level = asked;
    voice->sound->setBuffer(*buffer);
    voice->sound->setVolume(std::clamp(volume * (ringing ? volume_of(Bus::MASTER) : duck() * bus), 0.0f, 1.0f) * 100.0f);
    voice->sound->setPitch(pitch * (1.0f + (pitch_jitter > 0.0f ? Random::real(-pitch_jitter, pitch_jitter) : 0.0f)));
    voice->sound->play();
}

void SoundBank::play_at(const std::string& name, sf::Vector2f pos, float volume, float pitch_jitter, float pitch)
{
    play(name, volume * attenuation(pos), pitch_jitter, pitch);
}

void SoundBank::shot_at(const std::string& near, const std::string& far, sf::Vector2f pos, float volume)
{
    float const d = std::hypot(pos.x - m_listener.x, pos.y - m_listener.y);
    if(d < 260.0f || far.empty()) play(near, volume * attenuation(pos), 0.05f);
    else play(far, volume * std::clamp(1.0f - (d - 260.0f) / 900.0f, 0.15f, 1.0f) * 0.8f, 0.06f);
}

void SoundBank::set_loop(const std::string& name, float volume)
{
    auto it = m_loops.find(name);
    if(it == m_loops.end())
    {
        if(volume <= 0.0f) return;
        const sf::SoundBuffer* buffer = std::filesystem::exists(path(name)) ? Assets::sounds().try_get(path(name)) : nullptr;
        if(!buffer)
        {
            m_loops.emplace(name, Loop{});      // remembered as missing: don't look again
            return;
        }
        Loop loop;
        loop.sound = std::make_unique<sf::Sound>(*buffer);
        loop.sound->setLooping(true);
        loop.sound->setVolume(0.0f);
        loop.sound->play();
        it = m_loops.emplace(name, std::move(loop)).first;
    }
    it->second.target = std::clamp(volume, 0.0f, 1.0f);
}

void SoundBank::stop_loops()
{
    for(auto& [name, loop] : m_loops) loop.target = 0.0f;
}

void SoundBank::start_music()
{
    m_music_fade = 1.0f;
    if(m_playing) return;
    m_playing = true;
    if(m_music_ready)
    {
        if(m_forest_ready) m_forest.stop();
        if(m_charge_ready) m_charge.stop();
        if(m_town_ready) m_town.stop();
        if(m_calm_wanted) { m_calm.stop(); m_calm.play(); }
    }
    if(m_front_ready) m_front.play();
}

void SoundBank::set_combat_track(Track track)
{
    if(static_cast<int>(track) == m_track) return;
    m_battle = false;
    m_battle_mix = 0.0f;
    // Whichever combat track is going stops; the new one starts from its top when it's wanted.
    if(m_forest_ready) m_forest.stop();
    if(m_charge_ready) m_charge.stop();
    if(m_town_ready) m_town.stop();
    m_track = static_cast<int>(track);
    m_pitch = m_pitch_target = 1.0f;
}

void SoundBank::stop_music()
{
    m_music_fade = 0.0f;
}

void SoundBank::deafen(float seconds)
{
    if(seconds > m_deaf)
    {
        m_deaf = seconds;
        m_deaf_total = seconds;
        play("tinnitus", std::min(0.8f, seconds * 0.25f));
    }
}

void SoundBank::update(float dt)
{
    m_deaf = std::max(0.0f, m_deaf - dt);
    float const d = duck();

    float const step = std::min(1.0f, dt * 5.0f);
    for(auto& [name, loop] : m_loops)
    {
        if(!loop.sound) continue;
        loop.volume += (loop.target - loop.volume) * step;
        loop.sound->setVolume(loop.volume * d * volume_of(Bus::EFFECTS) * 100.0f);
    }

    m_intensity += (m_intensity_target - m_intensity) * std::min(1.0f, dt * 0.6f);
    m_front_volume += (m_front_target - m_front_volume) * std::min(1.0f, dt * 1.0f);

    m_music_level += (m_music_fade - m_music_level) * std::min(1.0f, dt * 0.8f);
    float const fade = m_music_level;

    // Into the combat track when the fighting gets going; out again only
    // after it has been quiet for a while, so it doesn't flap.
    if(m_intensity_target > 0.45f) { m_battle = true; m_quiet = 0.0f; }
    else if(m_intensity_target < 0.25f)
    {
        m_quiet += dt;
        if(m_quiet > 8.0f) m_battle = false;
    }
    float const mix_rate = m_battle ? 0.9f : 0.25f;     // in hard, out slow
    m_battle_mix = std::clamp(m_battle_mix + (m_battle ? 1.0f : -1.0f) * dt * mix_rate, 0.0f, 1.0f);

    float const on = m_music_on ? MUSIC_VOLUME * fade * d * volume_of(Bus::MUSIC) : 0.0f;

    // The calm piece is the title's, and nothing else's: the rest of the
    // time, between the fighting, there's no music at all.
    m_calm_level += ((m_calm_wanted ? 1.0f : 0.0f) - m_calm_level) * std::min(1.0f, dt * 0.8f);
    if(m_music_ready)
    {
        m_calm.setVolume(on * m_calm_level * (1.0f - m_battle_mix) * 100.0f);
        if(m_calm_level < 0.01f && !m_calm_wanted && m_calm.getStatus() == sf::SoundSource::Status::Playing) m_calm.pause();
        if(m_calm_wanted && m_playing && m_calm.getStatus() != sf::SoundSource::Status::Playing) m_calm.play();
    }

    bool const have = m_track == static_cast<int>(Track::FOREST) ? m_forest_ready
                    : (m_track == static_cast<int>(Track::TOWN) ? m_town_ready : (m_track == static_cast<int>(Track::CHARGE) && m_charge_ready));
    if(have)
    {
        sf::Music& combat = m_track == static_cast<int>(Track::FOREST) ? m_forest : (m_track == static_cast<int>(Track::TOWN) ? m_town : m_charge);
        float const gain = 1.1f;
        if(m_battle_mix > 0.0f && combat.getStatus() != sf::SoundSource::Status::Playing && m_playing) combat.play();
        if(m_battle_mix <= 0.0f && combat.getStatus() == sf::SoundSource::Status::Playing) combat.stop();
        combat.setVolume(std::min(100.0f, on * m_battle_mix * gain * 100.0f));
        m_pitch += (m_pitch_target - m_pitch) * std::min(1.0f, dt * 0.5f);
        combat.setPitch(m_pitch);

        // A loop that has to keep time with the music (the meadow's drums): the same speed,
        // and pulled back onto the beat if it drifts.
        if(!m_locked.empty())
        {
            auto it = m_loops.find(m_locked);
            if(it != m_loops.end() && it->second.sound)
            {
                sf::Sound& loop = *it->second.sound;
                loop.setPitch(m_pitch);
                if(combat.getStatus() == sf::SoundSource::Status::Playing)
                {
                    float const length = loop.getBuffer().getDuration().asSeconds();
                    float const want = std::fmod(combat.getPlayingOffset().asSeconds(), length);
                    float const have_at = loop.getPlayingOffset().asSeconds();
                    float drift = std::abs(want - have_at);
                    drift = std::min(drift, length - drift);
                    if(drift > 0.03f) loop.setPlayingOffset(sf::seconds(want));
                }
            }
        }
    }
    if(m_front_ready) m_front.setVolume(m_front_volume * d * fade * volume_of(Bus::EFFECTS) * 100.0f);
}

// ---------------------------------------------------------------- the player's levels

float SoundBank::volume(Bus bus) const
{
    return Settings::get().volume[static_cast<int>(bus)];
}

float SoundBank::volume_of(Bus bus) const
{
    return bus == Bus::MASTER ? volume(Bus::MASTER) : volume(Bus::MASTER) * volume(bus);
}

void SoundBank::set_volume(Bus bus, float value)
{
    Settings::get().volume[static_cast<int>(bus)] = std::clamp(std::round(value * 20.0f) / 20.0f, 0.0f, 1.0f);
}

void SoundBank::save_settings() const
{
    Settings::get().save();
}

}
