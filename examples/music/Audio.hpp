/**
 * Audio.hpp  -  Music lab
 *
 * Plays a track through a chain of effects that can be changed while it
 * runs, and keeps the last few thousand output samples for the scope.
 *
 * The whole track is decoded into memory up front. That is fine for
 * game music loops of a minute or so, and it is what makes speed
 * changes, seeking and looping trivial: playback is just a position
 * moving through an array.
 *
 * Effects, in the order they are applied:
 *
 *   speed        tape-style: faster is higher, slower is lower
 *   sample rate  sample-and-hold down to a lower rate, for aliasing crunch
 *   bit depth    quantise each sample to fewer levels
 *   radio        band-pass, a little overdrive, mono, crackle
 *   low-pass     muffle everything above a cutoff
 *   shell shock  a temporary muffle, volume dip and ringing tone that fade
 *   volume
 *
 * Parameters are atomics, so the main thread can change them while the
 * audio thread is reading them.
 */

#ifndef MUSIC_AUDIO_HPP
#define MUSIC_AUDIO_HPP

#include <SFML/Audio/SoundStream.hpp>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <random>
#include <string>
#include <vector>

namespace music
{

/*
 * A whole track, decoded to stereo float in -1..1.
 */
struct Track
{
    std::string        name;
    unsigned           sample_rate{44100};
    std::vector<float> samples;   // interleaved left, right
    float              peak{1.0f};

    // Effects that depend on level -- the bit crusher and the shell
    // shock ring -- work relative to the track's own peak, so a quietly
    // mastered track crushes the same way as a loud one.

    [[nodiscard]] std::size_t frames() const { return samples.size() / 2; }
    [[nodiscard]] float       seconds() const { return float(frames()) / float(sample_rate); }

    /*
     * Decodes MP3, OGG, FLAC or WAV. Mono files are duplicated to both
     * channels. Returns false if the file cannot be read.
     */
    bool load(const std::filesystem::path& path);
};

struct Settings
{
    std::atomic<float> volume{0.8f};
    std::atomic<float> speed{1.0f};
    std::atomic<float> sample_rate{44100.0f};
    std::atomic<float> bits{16.0f};
    std::atomic<float> cutoff{20000.0f};   // Hz
    std::atomic<bool>  radio{false};
};

/*
 * Two-pole (RBJ biquad) filter, one per channel.
 */
class Biquad
{
private:
    float b0{1}, b1{0}, b2{0}, a1{0}, a2{0};
    float x1[2]{}, x2[2]{}, y1[2]{}, y2[2]{};

public:
    void set_lowpass(float cutoff, float rate, float q = 0.7071f);
    void set_highpass(float cutoff, float rate, float q = 0.7071f);
    void reset();

    float process(float x, int channel);
};

class Processor
{
private:
    mutable std::mutex m_mutex;
    const Track*       m_track{nullptr};
    Settings&          m_settings;

    double m_position{0.0};          // frames into the track
    float  m_hold_phase{1.0f};
    float  m_held[2]{};

    Biquad m_lowpass;
    Biquad m_radio_low;
    Biquad m_radio_high;
    float  m_cutoff_now{20000.0f};   // smoothed toward the target

    float  m_shock{0.0f};            // 1 just after a blast, decays to 0
    double m_ring_phase{0.0};
    std::atomic<bool> m_shock_requested{false};

    std::minstd_rand m_rng{1921};

    std::atomic<double> m_seek_to{-1.0};
    std::atomic<double> m_reported_position{0.0};
    std::atomic<float>  m_reported_shock{0.0f};

public:
    explicit Processor(Settings& settings);

    void set_track(const Track* track);

    /*
     * Writes `frames` stereo frames (interleaved int16) into `out`.
     * Called from the audio thread, or from the main thread when there
     * is no audio device.
     */
    void render(std::int16_t* out, std::size_t frames);

    void shell_shock() { m_shock_requested = true; }
    void seek(double seconds) { m_seek_to = seconds; }

    [[nodiscard]] double position_seconds() const;
    [[nodiscard]] float  shock_level() const { return m_reported_shock; }
};

/*
 * Fixed-length history of the most recent mono output, for the scope.
 */
class Ring
{
private:
    mutable std::mutex m_mutex;
    std::vector<float> m_samples;
    std::size_t        m_write{0};
    std::uint64_t      m_total{0};

public:
    explicit Ring(std::size_t capacity = 8192) : m_samples(capacity, 0.0f) {}

    void push_stereo(const std::int16_t* interleaved, std::size_t frames);
    void clear();
    void latest(std::vector<float>& out, std::size_t count) const;

    [[nodiscard]] std::uint64_t total() const;
};

/*
 * Streams a Processor to the speakers and tees the result into a Ring.
 */
class Player : public sf::SoundStream
{
private:
    Processor&                m_processor;
    Ring&                     m_ring;
    std::vector<std::int16_t> m_buffer;

    bool onGetData(Chunk& data) override;
    void onSeek(sf::Time) override {}

public:
    Player(Processor& processor, Ring& ring, unsigned sample_rate);
    ~Player() override;
};

}

#endif
