/**
 * Sound.cpp  -  Cinder
 */

#include "Sound.hpp"

#include "../../include/core/AssetCache.hpp"
#include "../../include/core/Random.hpp"

#include <algorithm>
#include <cmath>

namespace cinder
{

namespace
{
    constexpr std::size_t VOICES       = 16;
    constexpr float       MUSIC_VOLUME = 0.45f;
    constexpr float       LOOP_EASE    = 4.0f;    // per second
}

SoundBank::SoundBank()
{
    // Construct the asset cache first, so it outlives this bank: statics
    // are destroyed in reverse order, and our sounds point into it.
    (void)Assets::sounds();

    m_music_ready = m_music.openFromFile(path("music_loop"));
    if(m_music_ready) m_music.setLooping(true);
}

SoundBank& SoundBank::get()
{
    static SoundBank bank;
    return bank;
}

std::string SoundBank::path(const std::string& name)
{
    return "assets/cinder/" + name + ".wav";
}

void SoundBank::play(const std::string& name, float volume, float pitch_jitter)
{
    const sf::SoundBuffer* buffer = Assets::sounds().try_get(path(name));
    if(!buffer) return;

    // Reuse a voice that has finished; otherwise take a new one until the
    // pool is full, then steal the first.
    sf::Sound* voice = nullptr;
    for(auto& v : m_voices)
    {
        if(v->getStatus() != sf::SoundSource::Status::Playing)
        {
            voice = v.get();
            break;
        }
    }
    if(!voice)
    {
        if(m_voices.size() < VOICES)
        {
            m_voices.push_back(std::make_unique<sf::Sound>(*buffer));
            voice = m_voices.back().get();
        }
        else
        {
            voice = m_voices.front().get();
            voice->stop();
        }
    }

    voice->setBuffer(*buffer);
    voice->setVolume(std::clamp(volume, 0.0f, 1.0f) * 100.0f);
    voice->setPitch(1.0f + (pitch_jitter > 0.0f ? Random::real(-pitch_jitter, pitch_jitter) : 0.0f));
    voice->play();
}

void SoundBank::set_loop(const std::string& name, float volume)
{
    auto it = m_loops.find(name);
    if(it == m_loops.end())
    {
        const sf::SoundBuffer* buffer = Assets::sounds().try_get(path(name));
        if(!buffer) return;

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

void SoundBank::set_music(bool playing)
{
    if(!m_music_ready) return;
    m_music_target = playing ? MUSIC_VOLUME : 0.0f;
    if(playing && m_music.getStatus() != sf::SoundSource::Status::Playing) m_music.play();
}

void SoundBank::toggle_music_mute()
{
    m_music_on = !m_music_on;
}

void SoundBank::update(float dt)
{
    float const step = std::min(1.0f, dt * LOOP_EASE);

    for(auto& [name, loop] : m_loops)
    {
        loop.volume += (loop.target - loop.volume) * step;
        loop.sound->setVolume(loop.volume * 100.0f);
    }

    if(m_music_ready)
    {
        float const target = m_music_on ? m_music_target : 0.0f;
        m_music_volume += (target - m_music_volume) * std::min(1.0f, dt * 1.5f);
        m_music.setVolume(m_music_volume * 100.0f);
    }
}

}
