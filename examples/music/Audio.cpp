/**
 * Audio.cpp  -  Music lab
 */

#include "Audio.hpp"

#include <SFML/Audio/InputSoundFile.hpp>

#include <algorithm>
#include <cmath>

namespace music
{

namespace
{
    constexpr float PI = 3.14159265358979f;

    // Shell shock: how long it takes to clear, and the ringing tone.
    constexpr float SHOCK_SECONDS = 6.0f;
    constexpr float RING_HZ       = 3700.0f;
    constexpr float SHOCK_CUTOFF  = 220.0f;
}

// ================================================================ Track

bool Track::load(const std::filesystem::path& path)
{
    sf::InputSoundFile file;
    if(!file.openFromFile(path)) return false;

    unsigned const channels = file.getChannelCount();
    sample_rate             = file.getSampleRate();
    name                    = path.stem().string();

    std::vector<std::int16_t> raw(static_cast<std::size_t>(file.getSampleCount()));
    std::size_t const read = static_cast<std::size_t>(file.read(raw.data(), raw.size()));
    raw.resize(read);

    std::size_t const frames = channels ? read / channels : 0;
    samples.assign(frames * 2, 0.0f);

    for(std::size_t f = 0; f < frames; ++f)
    {
        float const l = raw[f * channels] / 32768.0f;
        float const r = channels > 1 ? raw[f * channels + 1] / 32768.0f : l;
        samples[f * 2]     = l;
        samples[f * 2 + 1] = r;
    }
    peak = 0.0f;
    for(float v : samples) peak = std::max(peak, std::abs(v));
    if(peak < 1.0e-4f) peak = 1.0f;

    return frames > 0;
}

// =============================================================== Biquad

void Biquad::set_lowpass(float cutoff, float rate, float q)
{
    float const w     = 2.0f * PI * std::clamp(cutoff, 10.0f, rate * 0.45f) / rate;
    float const alpha = std::sin(w) / (2.0f * q);
    float const cosw  = std::cos(w);
    float const a0    = 1.0f + alpha;

    b0 = (1.0f - cosw) / 2.0f / a0;
    b1 = (1.0f - cosw) / a0;
    b2 = b0;
    a1 = -2.0f * cosw / a0;
    a2 = (1.0f - alpha) / a0;
}

void Biquad::set_highpass(float cutoff, float rate, float q)
{
    float const w     = 2.0f * PI * std::clamp(cutoff, 10.0f, rate * 0.45f) / rate;
    float const alpha = std::sin(w) / (2.0f * q);
    float const cosw  = std::cos(w);
    float const a0    = 1.0f + alpha;

    b0 = (1.0f + cosw) / 2.0f / a0;
    b1 = -(1.0f + cosw) / a0;
    b2 = b0;
    a1 = -2.0f * cosw / a0;
    a2 = (1.0f - alpha) / a0;
}

void Biquad::reset()
{
    for(int c = 0; c < 2; ++c) x1[c] = x2[c] = y1[c] = y2[c] = 0.0f;
}

float Biquad::process(float x, int c)
{
    float const y = b0 * x + b1 * x1[c] + b2 * x2[c] - a1 * y1[c] - a2 * y2[c];
    x2[c] = x1[c];
    x1[c] = x;
    y2[c] = y1[c];
    y1[c] = y;
    return y;
}

// ============================================================ Processor

Processor::Processor(Settings& settings)
    : m_settings(settings)
{}

void Processor::set_track(const Track* track)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_track    = track;
    m_position = 0.0;
    m_lowpass.reset();
    m_radio_low.reset();
    m_radio_high.reset();
    m_reported_position = 0.0;
}

double Processor::position_seconds() const
{
    return m_reported_position;
}

void Processor::render(std::int16_t* out, std::size_t frames)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if(!m_track || m_track->frames() == 0)
    {
        std::fill(out, out + frames * 2, std::int16_t{0});
        return;
    }

    Track const& track = *m_track;
    float const  rate  = static_cast<float>(track.sample_rate);
    double const len   = static_cast<double>(track.frames());

    double const seek = m_seek_to.exchange(-1.0);
    if(seek >= 0.0) m_position = std::clamp(seek * rate, 0.0, len - 1.0);

    if(m_shock_requested.exchange(false)) m_shock = 1.0f;

    // Read the settings once per block.
    float const volume  = m_settings.volume;
    float const speed   = std::clamp(m_settings.speed.load(), 0.25f, 2.0f);
    float const hold    = std::clamp(m_settings.sample_rate.load(), 1000.0f, rate) / rate;
    float const levels  = std::pow(2.0f, std::clamp(m_settings.bits.load(), 1.0f, 16.0f) - 1.0f);
    bool const  radio   = m_settings.radio;
    float const target  = std::clamp(m_settings.cutoff.load(), 80.0f, 20000.0f);

    m_radio_high.set_highpass(380.0f, rate);
    m_radio_low.set_lowpass(2900.0f, rate, 0.9f);

    std::uniform_real_distribution<float> noise(-1.0f, 1.0f);

    // The filter is retuned every few frames as the cutoff glides, so
    // turning the knob or a passing shock never clicks.
    constexpr std::size_t RETUNE = 32;

    for(std::size_t f = 0; f < frames; ++f)
    {
        if(f % RETUNE == 0)
        {
            // Shell shock pulls the cutoff down hard, then lets it back up
            // as the shock fades. Interpolated in log space, because that
            // is how pitch is heard.
            float const shocked = std::exp(std::log(target) +
                                           (std::log(SHOCK_CUTOFF) - std::log(target)) *
                                           std::sqrt(m_shock));
            float const glide = 1.0f - std::exp(-float(RETUNE) / (rate * 0.02f));
            m_cutoff_now = std::exp(std::log(m_cutoff_now) +
                                    (std::log(shocked) - std::log(m_cutoff_now)) * glide);
            m_lowpass.set_lowpass(m_cutoff_now, rate);
        }

        // 1. Speed: read between samples, linearly interpolated.
        std::size_t const i0 = static_cast<std::size_t>(m_position);
        std::size_t const i1 = (i0 + 1) % track.frames();
        float const       t  = static_cast<float>(m_position - static_cast<double>(i0));
        float in[2];
        for(int c = 0; c < 2; ++c)
            in[c] = track.samples[i0 * 2 + c] * (1.0f - t) + track.samples[i1 * 2 + c] * t;

        m_position += speed;
        if(m_position >= len) m_position -= len;   // the track loops

        // 2. Sample rate: only take a new sample every so often.
        m_hold_phase += hold;
        if(m_hold_phase >= 1.0f)
        {
            m_hold_phase -= 1.0f;
            m_held[0] = in[0];
            m_held[1] = in[1];
        }

        float s[2] = {m_held[0], m_held[1]};

        for(int c = 0; c < 2; ++c)
        {
            // 3. Bit depth.
            if(levels < 32768.0f) s[c] = std::round(s[c] / track.peak * levels) / levels * track.peak;
        }

        // 4. Radio: mono, a little static, then squeezed through the
        //    narrow band a small speaker can reproduce, and overdriven.
        //    The static goes in before the filters, because real radio
        //    noise is band-limited too.
        if(radio)
        {
            float mono = (s[0] + s[1]) * 0.5f / track.peak;

            mono += noise(m_rng) * 0.05f;
            if(m_rng() % 2600 == 0) mono += noise(m_rng) * 1.2f;   // the odd pop

            mono = m_radio_high.process(mono, 0);
            mono = m_radio_low.process(mono, 0);
            mono = std::tanh(mono * 1.8f) * 0.75f * track.peak;
            s[0] = s[1] = mono;
        }

        // 5. Low-pass (and the shell shock muffle).
        for(int c = 0; c < 2; ++c) s[c] = m_lowpass.process(s[c], c);

        // 6. Shell shock: volume dip and a high ringing tone.
        float gain = volume;
        if(m_shock > 0.0f)
        {
            gain *= 1.0f - 0.65f * m_shock;
            float const ring = std::sin(static_cast<float>(m_ring_phase)) * 0.3f *
                               track.peak * m_shock * m_shock;
            m_ring_phase += 2.0 * PI * RING_HZ / rate;
            if(m_ring_phase > 2.0 * PI) m_ring_phase -= 2.0 * PI;
            s[0] = s[0] * gain + ring;
            s[1] = s[1] * gain + ring;
            m_shock = std::max(0.0f, m_shock - 1.0f / (SHOCK_SECONDS * rate));
        }
        else
        {
            s[0] *= gain;
            s[1] *= gain;
        }

        for(int c = 0; c < 2; ++c)
            out[f * 2 + c] = static_cast<std::int16_t>(std::clamp(s[c], -1.0f, 1.0f) * 32767.0f);
    }

    m_reported_position = m_position / rate;
    m_reported_shock    = m_shock;
}

// ================================================================= Ring

void Ring::push_stereo(const std::int16_t* interleaved, std::size_t frames)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for(std::size_t f = 0; f < frames; ++f)
    {
        float const mono = (interleaved[f * 2] + interleaved[f * 2 + 1]) / 65536.0f;
        m_samples[m_write] = mono;
        m_write = (m_write + 1) % m_samples.size();
    }
    m_total += frames;
}

void Ring::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::fill(m_samples.begin(), m_samples.end(), 0.0f);
}

void Ring::latest(std::vector<float>& out, std::size_t count) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    count = std::min(count, m_samples.size());
    out.resize(count);
    std::size_t const size  = m_samples.size();
    std::size_t const start = (m_write + size - count) % size;
    for(std::size_t i = 0; i < count; ++i) out[i] = m_samples[(start + i) % size];
}

std::uint64_t Ring::total() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_total;
}

// =============================================================== Player

Player::Player(Processor& processor, Ring& ring, unsigned sample_rate)
    : m_processor(processor), m_ring(ring), m_buffer(1024 * 2)
{
    initialize(2, sample_rate, {sf::SoundChannel::FrontLeft, sf::SoundChannel::FrontRight});
}

Player::~Player()
{
    // Stop the audio thread before the buffer it reads goes away.
    stop();
}

bool Player::onGetData(Chunk& data)
{
    std::size_t const frames = m_buffer.size() / 2;
    m_processor.render(m_buffer.data(), frames);
    m_ring.push_stereo(m_buffer.data(), frames);

    data.samples     = m_buffer.data();
    data.sampleCount = m_buffer.size();
    return true;
}

}
