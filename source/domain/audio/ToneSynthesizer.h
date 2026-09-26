#pragma once

// =====================================================================================================================
// Musichien - ToneSynthesizer
//
// Generates PCM audio for notes: plain mono samples in [-1, 1].
//
// This class is PURE. No Qt, no file, no sound card, no clock. It only does arithmetic, therefore the
// entire sound generation is unit tested on any machine. That is what makes it safe to change when
// the sound has to be tuned.
//
// The signal is a sine wave shaped by an envelope. The envelope is not cosmetic: a sine that starts or
// stops abruptly produces an audible CLICK, which would pollute the very thing the application is
// trying to train - the ear.
// =====================================================================================================================

#include "domain/music/Note.h"

#include <chrono>
#include <cstdint>
#include <span>
#include <vector>

namespace musichien::domain
{

class ToneSynthesizer
{
public:
    // Fade in, which removes the click at the beginning of a note.
    static constexpr std::chrono::milliseconds ATTACK_DURATION{ 12 };

    // Fade out, which removes the click at the end of a note.
    static constexpr std::chrono::milliseconds RELEASE_DURATION{ 40 };

    // Peak level of every rendered buffer.
    //
    // Every buffer is normalised to this value, so a single note and a three note chord sound equally
    // loud. That matters a lot here: the player is asked to compare two sounds, and a loudness
    // difference would be a hint that has nothing to do with the interval.
    static constexpr float TARGET_PEAK_AMPLITUDE = 0.75F;

    // Peak level of the cue that marks a mistake, deliberately BELOW the notes.
    //
    // Noise at the same peak as a tone sounds much louder than the tone, and a cue is there to be
    // noticed, not to make the player jump. Two sounds that are meant to carry different weights should
    // not be normalised to the same number.
    static constexpr float MISTAKE_CUE_PEAK_AMPLITUDE = 0.30F;

    // The cue that marks a mistake, as a SHORT NOISE BURST.
    //
    // Noise, and not a note: this is a musical decision before it is a technical one. A cue with a pitch
    // would teach the ear to associate a note with failure, and what a note means is exactly what this
    // application exists to train. A noise burst has no pitch to learn, which is the whole point.
    //
    // The sequence comes from a FIXED seed: the domain owns no entropy source of its own, and two runs
    // must produce the same cue. It is also what makes the burst testable.
    [[nodiscard]] std::vector<float> renderMistakeCue( std::chrono::milliseconds p_duration ) const;

    // How long the cue lasts: short enough to be a punctuation mark rather than a sound of its own.
    static constexpr std::chrono::milliseconds MISTAKE_CUE_DURATION{ 90 };

    explicit ToneSynthesizer( std::int32_t p_sampleRate );

    [[nodiscard]] std::int32_t sampleRate() const noexcept { return m_sampleRate; }

    // Number of samples a duration represents at this sample rate.
    [[nodiscard]] std::size_t sampleCountFor( std::chrono::milliseconds p_duration ) const noexcept;

    // A single note.
    [[nodiscard]] std::vector<float> renderNote( const Note & p_note,
                                                 std::chrono::milliseconds p_duration ) const;

    // Several notes sounded at the same time: a harmonic interval, a chord.
    [[nodiscard]] std::vector<float> renderChord( std::span<const Note> p_notes,
                                                  std::chrono::milliseconds p_duration ) const;

    // Several notes sounded one after another, separated by silence: a melodic interval, a scale.
    [[nodiscard]] std::vector<float> renderMelody( std::span<const Note> p_notes,
                                                   std::chrono::milliseconds p_noteDuration,
                                                   std::chrono::milliseconds p_gap ) const;

private:
    // Applies the fade in and the fade out over a buffer already filled with the raw waveform.
    void applyEnvelope( std::span<float> p_samples ) const;

    // Scales the buffer so that its peak equals p_targetPeak.
    static void normalisePeakTo( std::span<float> p_samples, float p_targetPeak );

    // Scales the buffer so that its peak equals TARGET_PEAK_AMPLITUDE.
    static void normalisePeak( std::span<float> p_samples );

    std::int32_t m_sampleRate{ 0 };
};

}    // namespace musichien::domain
