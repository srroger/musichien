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
// ---------------------------------------------------------------------------------------------------------------------
// A STRUCK STRING, not a sine wave
//
// The signal is a delay line excited by a short burst of noise - the hammer - with a low pass filter
// inside the loop and a little loss on every round trip. Three things fall out of that model, and all
// three were the reason to adopt it:
//
//   * a sine has NO harmonic at all, so the brain has nothing to hang a low note on. On a phone
//     speaker, which cannot produce a low fundamental at a useful level, a sine simply disappears,
//     while a struck string stays perfectly identifiable. See the sonar note: "rendering a low note
//     audible on a phone is a TIMBRE problem, not a volume one";
//   * the low pass in the loop makes the HIGH harmonics die first, which is what a real string does.
//     The result sounds struck rather than held;
//   * the excitation is NOISE, different for every note. Two notes of an interval therefore start on
//     unrelated waveforms instead of on two sines that begin in phase, add up on the attack and then
//     hollow each other out - the "the chords cancel themselves" the ear picks up immediately.
//
// Every buffer is normalised on its ENERGY, not on its peak, with a ceiling that prevents clipping.
// Peak normalisation gives equal volts; it does not give equal loudness, and this application asks the
// player to compare two sounds. See the sonar note, "normaliser - three things the word mixes up".
// =====================================================================================================================

#include "domain/music/Note.h"
#include "domain/music/Temperament.h"

#include <chrono>
#include <cstdint>
#include <span>
#include <vector>

namespace musichien::domain
{

class ToneSynthesizer
{
public:
    // Fade in of the cue that marks a mistake, which removes the click at its beginning.
    static constexpr std::chrono::milliseconds CUE_ATTACK_DURATION{ 12 };

    // Fade out of everything, which removes the click at the end of a buffer.
    static constexpr std::chrono::milliseconds RELEASE_DURATION{ 40 };

    // Fade in of a NOTE, and it is ten times shorter than the cue's on purpose.
    //
    // A struck string starts with the hammer: a fade of 12 ms would swallow the very attack that makes
    // the sound what it is. Two milliseconds is enough to remove the click of the first sample and short
    // enough to leave the strike intact.
    static constexpr std::chrono::milliseconds STRIKE_ATTACK_DURATION{ 2 };

    // Energy level every buffer of NOTES aims for, before the ceiling below is applied.
    //
    // Energy - the root mean square - and not the peak, because that is what the ear averages. Two sounds
    // with the same peak but different waveforms are heard at different loudnesses, and the player is asked
    // to compare two sounds.
    //
    // The value is what a struck string can actually reach while staying under the ceiling, measured rather
    // than guessed: across the whole range, the onset energy of a note lands between 0.20 and 0.26. A higher
    // target would simply never be reached, and the constant would be a lie.
    static constexpr float TARGET_RMS_AMPLITUDE = 0.24F;

    // The beginning of the note, and it is what the loudness is measured on.
    //
    // Measuring the energy of the WHOLE buffer would be measuring a decay: a note that rings twice as long
    // would come out half as loud, and the treble would sound weaker than the bass for no musical reason at
    // all. What must be equal from one note to the next is the level at the moment the note SOUNDS.
    //
    // Short, too, and for the same reason: the longer this window is, the more of the note's DECAY it
    // contains - and a window full of decay has a tall peak compared to its average, which is what the
    // ceiling below reacts to. Thirty milliseconds is the attack of a piano hammer.
    static constexpr std::chrono::milliseconds NOTE_ONSET_DURATION{ 30 };

    // Hard ceiling on the amplitude of any buffer. A rich waveform can reach its energy target only by
    // exceeding the maximum, so the ceiling always wins: it is what keeps the sound out of the clipping
    // that a phone speaker turns into distortion.
    static constexpr float MAXIMUM_PEAK_AMPLITUDE = 0.92F;

    // Energy level of the cue that marks a mistake, deliberately BELOW the notes.
    //
    // Noise sounds much louder than a pitched sound of the same energy, and a cue is there to be noticed,
    // not to make the player jump. Two sounds meant to carry different weights must not be normalised to
    // the same number.
    static constexpr float MISTAKE_CUE_RMS_AMPLITUDE = 0.10F;

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

    // Strings struck per note, and how far apart they are tuned.
    //
    // A real piano has three strings for the middle of its range, tuned a hair apart. The beating between
    // them is what gives the note its body - and, again, what stops the members of a chord from lining up
    // into a single waveform that cancels itself out.
    static constexpr std::size_t STRING_COUNT = 3;
    static constexpr double STRING_DETUNE_CENTS = 0.9;

    explicit ToneSynthesizer( std::int32_t p_sampleRate );

    [[nodiscard]] std::int32_t sampleRate() const noexcept { return m_sampleRate; }

    // Number of samples a duration represents at this sample rate.
    [[nodiscard]] std::size_t sampleCountFor( std::chrono::milliseconds p_duration ) const noexcept;

    // A single note.
    [[nodiscard]] std::vector<float> renderNote( const Note & p_note,
                                                 std::chrono::milliseconds p_duration,
                                                 TuningContext p_tuning = {} ) const;

    // Several notes sounded at the same time: a harmonic interval, a chord. The tuning's root is the FIRST note,
    // which is the note the interval is heard from.
    [[nodiscard]] std::vector<float> renderChord( std::span<const Note> p_notes,
                                                  std::chrono::milliseconds p_duration,
                                                  TuningContext p_tuning = {} ) const;

    // Several notes sounded one after another, separated by silence: a melodic interval, a scale. Root as above.
    [[nodiscard]] std::vector<float> renderMelody( std::span<const Note> p_notes,
                                                   std::chrono::milliseconds p_noteDuration,
                                                   std::chrono::milliseconds p_gap,
                                                   TuningContext p_tuning = {} ) const;

private:
    // One note at an EXPLICIT frequency. The frequency is computed by the caller - which alone knows the root - and
    // this method only does the arithmetic of a struck string at that frequency.
    [[nodiscard]] std::vector<float> renderNoteAt( const Note & p_note,
                                                   double p_frequencyHz,
                                                   std::chrono::milliseconds p_duration ) const;

    // Adds ONE struck string to a buffer, without touching its level: mixing, enveloping and normalising
    // belong to the caller, which is the only one that knows how many strings are playing.
    //
    // p_seed decides the noise of the hammer, so two different notes - or two strings of the same note -
    // never start on the same waveform. It comes from the note itself, which keeps the whole thing
    // reproducible.
    void mixStruckStringInto( std::span<float> p_samples,
                              double p_frequency,
                              std::uint32_t p_seed ) const;

    // Applies a fade in and a fade out over a buffer already filled with the raw waveform.
    void applyEnvelope( std::span<float> p_samples,
                        std::chrono::milliseconds p_attack,
                        std::chrono::milliseconds p_release ) const;

    [[nodiscard]] static float rootMeanSquare( std::span<const float> p_samples );

    [[nodiscard]] static float peakAmplitude( std::span<const float> p_samples );

    // Scales a buffer on the ENERGY OF ITS BEGINNING, then brings it back down if that made it clip.
    //
    // Not static, unlike the two helpers above: it needs the sample rate to know how long the beginning is.
    void normaliseOnsetEnergyTo( std::span<float> p_samples,
                                 float p_targetRms,
                                 std::chrono::milliseconds p_onsetDuration ) const;

    std::int32_t m_sampleRate{ 0 };
};

}    // namespace musichien::domain
