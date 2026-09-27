#pragma once

// =====================================================================================================================
// Musichien - AudioMixer
//
// A tiny software mixer: a pull-mode QIODevice the audio sink reads from, holding the sounds that are playing.
//
// ---------------------------------------------------------------------------------------------------------------------
// Why it exists
//
// The first version PUSHED one buffer at a time and stopped everything before each one. That is right for an ear
// training interval - two overlapping notes would make the interval impossible to name - and completely wrong the
// moment two sounds must be heard TOGETHER: the metronome click and a drum hit, a drum roll, a backing pattern.
// Roger heard it exactly: "je lance le metronome et si j'appuie sur la batterie au meme moment que le bip, le bip du
// metronome ne joue pas".
//
// So the sounds are mixed instead. The sink PULLS from this device whenever it needs samples, and this device sums
// whatever is still playing.
//
// ---------------------------------------------------------------------------------------------------------------------
// Thread safety, and why it is not a detail
//
// The audio backend pulls from its own thread while the interface pushes from the main thread. Every access to the
// voices is therefore guarded, and the guard is the reason this class is not just a vector plus a loop.
// =====================================================================================================================

#include <QIODevice>

#include <cstddef>
#include <mutex>
#include <vector>

namespace musichien::infrastructure
{

// Note: NOT final, unlike the rest of the project. A test needs to subclass it to reach readData directly, because
// QIODevice::read() fills a buffer of its own and would make the test a test about Qt rather than about the mix.
class AudioMixer : public QIODevice
{
public:
    AudioMixer( int p_sampleRate, int p_channelCount, QObject * p_parent = nullptr );

    // Adds a sound to the mix. Replaces NOTHING: this is the whole point of the class.
    void play( std::vector<float> p_samples, float p_gain = 1.0F );

    // Drops everything that is playing, at once.
    void clear();

    // True while at least one sound is still playing: the caller stops the sink when it goes false.
    [[nodiscard]] bool isPlaying() const;

    // How many sounds are being mixed right now. Exposed because it is what a test needs to see, and because it
    // makes the mixer's behaviour readable from the outside.
    [[nodiscard]] std::size_t voiceCount() const;

protected:
    // Fills the buffer the backend asked for. Always returns complete frames: an empty answer would be read as
    // "nothing to play", and the stream would stall, so silence is written instead.
    qint64 readData( char * p_data, qint64 p_maximumByteCount ) override;

    // A mixer is a source: it is never written to.
    qint64 writeData( const char * p_data, qint64 p_byteCount ) override;

private:
    struct Voice
    {
        std::vector<float> samples;
        std::size_t position{ 0 };
        float gain{ 1.0F };
    };

    mutable std::mutex m_mutex;
    std::vector<Voice> m_voices;
    int m_sampleRate{ 0 };
    int m_channelCount{ 1 };
};

}    // namespace musichien::infrastructure
