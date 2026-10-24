/**
 * Sound.cpp  -  Mud Cats
 */

#include "Sound.hpp"

#include "../../include/core/AssetCache.hpp"
#include "../../include/core/Random.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace mudcats
{

namespace
{
    constexpr std::size_t VOICES       = 40;
    constexpr float       MUSIC_VOLUME = 0.42f;
    constexpr float       HEARING      = 700.0f;    // pixels until a sound is gone
}

SoundBank::SoundBank()
{
    (void)Assets::sounds();     // outlive us: see Cinder's SoundBank

    m_music_ready = m_calm.openFromFile(path("music_calm_loop")) && m_metal.openFromFile(path("music_combat_loop"));
    if(m_music_ready)
    {
        m_calm.setLooping(true);
        m_metal.setLooping(true);
        m_calm.setVolume(0.0f);
        m_metal.setVolume(0.0f);
    }
    // Misanthropy, cut to one clean cycle and brought up to level by
    // tools/sfx/make_hill_loop.py.
    m_dread_ready = m_dread.openFromFile("assets/mudcats/music_hill_loop.ogg");
    if(m_dread_ready)
    {
        m_dread.setLooping(true);
        m_dread.setVolume(0.0f);
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

void SoundBank::play(const std::string& name, float volume, float pitch_jitter)
{
    const auto& names = variants(name);
    if(names.empty() || volume <= 0.01f) return;
    const sf::SoundBuffer* buffer = Assets::sounds().try_get(path(names[static_cast<std::size_t>(Random::integer(0, static_cast<int>(names.size()) - 1))]));
    if(!buffer) return;

    sf::Sound* voice = nullptr;
    for(auto& v : m_voices)
        if(v->getStatus() != sf::SoundSource::Status::Playing) { voice = v.get(); break; }
    if(!voice)
    {
        if(m_voices.size() < VOICES)
        {
            m_voices.push_back(std::make_unique<sf::Sound>(*buffer));
            voice = m_voices.back().get();
        }
        else
        {
            // Steal the quietest voice.
            voice = std::min_element(m_voices.begin(), m_voices.end(),
                [](const auto& a, const auto& b) { return a->getVolume() < b->getVolume(); })->get();
            voice->stop();
        }
    }

    bool const ringing = name == "tinnitus";
    voice->setBuffer(*buffer);
    voice->setVolume(std::clamp(volume * (ringing ? 1.0f : duck()), 0.0f, 1.0f) * 100.0f);
    voice->setPitch(1.0f + (pitch_jitter > 0.0f ? Random::real(-pitch_jitter, pitch_jitter) : 0.0f));
    voice->play();
}

void SoundBank::play_at(const std::string& name, sf::Vector2f pos, float volume, float pitch_jitter)
{
    play(name, volume * attenuation(pos), pitch_jitter);
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
        m_calm.stop();
        m_metal.stop();
        if(m_dread_ready) m_dread.stop();
        m_calm.play();
    }
    if(m_front_ready) m_front.play();
}

void SoundBank::set_dread(bool on)
{
    if(on == m_use_dread) return;
    // Whichever combat track is going stops; the other starts from its top when it's wanted.
    m_metal.stop();
    if(m_dread_ready) m_dread.stop();
    m_use_dread = on;
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
        loop.sound->setVolume(loop.volume * d * 100.0f);
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

    if(m_music_ready)
    {
        float const on = m_music_on ? MUSIC_VOLUME * fade * d : 0.0f;
        sf::Music& combat = m_use_dread && m_dread_ready ? m_dread : m_metal;
        float const gain = 1.1f;
        if(m_battle_mix > 0.0f && combat.getStatus() != sf::SoundSource::Status::Playing && m_playing) combat.play();
        if(m_battle_mix <= 0.0f && combat.getStatus() == sf::SoundSource::Status::Playing) combat.stop();
        m_calm.setVolume(on * (1.0f - m_battle_mix) * 100.0f);
        combat.setVolume(std::min(100.0f, on * m_battle_mix * gain * 100.0f));
    }
    if(m_front_ready) m_front.setVolume(m_front_volume * d * fade * 100.0f);
}

}
