/**
 * Audio.cpp  -  Showcase
 */

#include "Audio.hpp"

#include <algorithm>
#include <cmath>

namespace showcase
{

// ------------------------------------------------------------ SampleRing

SampleRing::SampleRing(std::size_t capacity)
    : m_samples(capacity, 0.0f)
{}

void SampleRing::push(const std::int16_t* samples, std::size_t count, float gain)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for(std::size_t i = 0; i < count; ++i)
    {
        float const s = static_cast<float>(samples[i]) / 32768.0f * gain;
        m_samples[m_write] = std::clamp(s, -1.0f, 1.0f);
        m_write = (m_write + 1) % m_samples.size();
    }
    m_total += count;
}

void SampleRing::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::fill(m_samples.begin(), m_samples.end(), 0.0f);
    m_write = 0;
}

void SampleRing::latest(std::vector<float>& out, std::size_t count) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    count = std::min(count, m_samples.size());
    out.resize(count);

    std::size_t const size  = m_samples.size();
    std::size_t const start = (m_write + size - count) % size;
    for(std::size_t i = 0; i < count; ++i)
        out[i] = m_samples[(start + i) % size];
}

std::uint64_t SampleRing::total() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_total;
}

// ------------------------------------------------------------- ChipSynth

namespace
{
    // MIDI note numbers, one per eighth note; 0 is a rest. A minor,
    // four bars of eight.
    constexpr int LEAD[] = {
        69, 72, 76, 81,  79, 76, 72, 76,     // Am
        65, 69, 72, 77,  76, 72, 69,  0,     // F
        67, 72, 76, 79,  81, 79, 76, 72,     // C
        67, 71, 74, 79,  77, 74, 71, 74,     // G
    };

    constexpr int BASS[] = {45, 41, 48, 43};   // one note per bar

    constexpr std::size_t STEPS          = std::size(LEAD);
    constexpr double      STEP_SECONDS   = 0.15;
    constexpr double      STEP_SAMPLES   = STEP_SECONDS * ChipSynth::SAMPLE_RATE;
    constexpr std::size_t CHUNK          = 1024;

    double frequency(int note)
    {
        return 440.0 * std::pow(2.0, (note - 69) / 12.0);
    }

    double oscillate(double phase, Voice voice)
    {
        switch(voice)
        {
            case Voice::SQUARE:   return phase < 0.5 ? 1.0 : -1.0;
            case Voice::TRIANGLE: return 4.0 * std::abs(phase - 0.5) - 1.0;
            case Voice::SAW:      return 2.0 * phase - 1.0;
        }
        return 0.0;
    }
}

ChipSynth::ChipSynth(SampleRing& ring)
    : m_ring(ring), m_buffer(CHUNK)
{
    initialize(1, SAMPLE_RATE, {sf::SoundChannel::Mono});
    setVolume(55.0f);
}

ChipSynth::~ChipSynth()
{
    // SFML requires derived streams to stop the audio thread themselves,
    // before the members it reads from are destroyed.
    stop();
}

void ChipSynth::set_voice(Voice voice)
{
    m_voice = voice;
}

Voice ChipSynth::get_voice() const
{
    return m_voice;
}

void ChipSynth::render(std::int16_t* out, std::size_t count, std::uint64_t start, Voice voice)
{
    for(std::size_t i = 0; i < count; ++i)
    {
        std::uint64_t const n = start + i;
        double const        t = static_cast<double>(n) / SAMPLE_RATE;

        auto const   step    = static_cast<std::size_t>(static_cast<double>(n) / STEP_SAMPLES);
        double const in_step = static_cast<double>(n) / STEP_SAMPLES - static_cast<double>(step);

        int const lead_note = LEAD[step % STEPS];
        int const bass_note = BASS[(step / 8) % std::size(BASS)];

        // Phase from absolute time, so render() needs no state and any
        // position can be produced on demand.
        double lead = 0.0;
        if(lead_note != 0 && in_step < 0.85)
        {
            double const phase    = std::fmod(t * frequency(lead_note), 1.0);
            double const envelope = 0.55 + 0.45 * std::exp(-in_step * 6.0);
            lead = oscillate(phase, voice) * envelope;
        }

        double const bass_phase = std::fmod(t * frequency(bass_note), 1.0);
        double const bass       = oscillate(bass_phase, Voice::TRIANGLE);

        double const mixed = lead * 0.24 + bass * 0.20;
        out[i] = static_cast<std::int16_t>(std::clamp(mixed, -1.0, 1.0) * 32767.0);
    }
}

bool ChipSynth::onGetData(Chunk& data)
{
    render(m_buffer.data(), m_buffer.size(), m_position, m_voice);
    m_position += m_buffer.size();

    m_ring.push(m_buffer.data(), m_buffer.size());

    data.samples     = m_buffer.data();
    data.sampleCount = m_buffer.size();
    return true;   // loops forever
}

void ChipSynth::onSeek(sf::Time time_offset)
{
    m_position = static_cast<std::uint64_t>(time_offset.asSeconds() * SAMPLE_RATE);
}

// ---------------------------------------------------------------- MicTap

MicTap::MicTap(SampleRing& ring)
    : m_ring(ring)
{}

MicTap::~MicTap()
{
    stop();
}

bool MicTap::onProcessSamples(const std::int16_t* samples, std::size_t sample_count)
{
    // Speech and whistling sit well below full scale; a little gain
    // makes them fill the screen.
    m_ring.push(samples, sample_count, 2.5f);
    return true;
}

}
