/**
 * Audio.hpp  -  Showcase
 *
 * Sound for the oscilloscope window: a small chiptune generated in code,
 * and an optional microphone tap. Both write what they produce into a
 * SampleRing, which the scope reads from on the main thread.
 *
 * SFML pulls stream data and delivers recorded data on its own audio
 * thread, which is why the ring is locked.
 */

#ifndef SHOWCASE_AUDIO_HPP
#define SHOWCASE_AUDIO_HPP

#include <SFML/Audio/SoundRecorder.hpp>
#include <SFML/Audio/SoundStream.hpp>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

namespace showcase
{

/*
 * Fixed-size history of recent samples, newest last, in -1..1.
 */
class SampleRing
{
private:
    mutable std::mutex m_mutex;
    std::vector<float> m_samples;
    std::size_t        m_write{0};
    std::uint64_t      m_total{0};

public:
    explicit SampleRing(std::size_t capacity = 16384);

    void push(const std::int16_t* samples, std::size_t count, float gain = 1.0f);
    void clear();

    /*
     * Copies the most recent `count` samples into `out`, oldest first.
     */
    void latest(std::vector<float>& out, std::size_t count) const;

    /*
     * Samples pushed so far. Lets a reader tell whether anything new
     * has arrived since it last looked.
     */
    [[nodiscard]] std::uint64_t total() const;
};

enum class Voice
{
    SQUARE,
    TRIANGLE,
    SAW
};

/*
 * A two-part loop, a lead and a triangle bass, generated sample by
 * sample. render() is deterministic for a given position, so the same
 * music can be produced for display even when no audio device is
 * pulling it.
 */
class ChipSynth : public sf::SoundStream
{
private:
    SampleRing&               m_ring;
    std::vector<std::int16_t> m_buffer;
    std::uint64_t             m_position{0};
    std::atomic<Voice>        m_voice{Voice::SQUARE};

    bool onGetData(Chunk& data) override;
    void onSeek(sf::Time time_offset) override;

public:
    static constexpr unsigned SAMPLE_RATE = 44100;

    explicit ChipSynth(SampleRing& ring);
    ~ChipSynth() override;

    void  set_voice(Voice voice);
    Voice get_voice() const;

    /*
     * Writes `count` samples of the loop starting at sample `start`.
     */
    static void render(std::int16_t* out, std::size_t count, std::uint64_t start, Voice voice);
};

/*
 * Feeds the microphone into a ring.
 */
class MicTap : public sf::SoundRecorder
{
private:
    SampleRing& m_ring;

    bool onProcessSamples(const std::int16_t* samples, std::size_t sample_count) override;

public:
    explicit MicTap(SampleRing& ring);
    ~MicTap() override;
};

}

#endif
