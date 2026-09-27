#pragma once

// =====================================================================================================================
// Musichien - SungIntervalDetector
//
// What the player just SANG, read as an interval: two held notes, and the distance between them.
//
// The hard part is not measuring a pitch - PitchDetector does that. It is deciding WHICH pitches were MEANT. A voice
// glides, wobbles and cracks, so a naive reading would report ten different notes in two seconds. A note therefore
// counts only when it is HELD: steady within a tolerance, and long enough to be a note rather than a slide.
//
// This is a pure rule, deliberately kept out of the screen that displays it: readings and time go in, an answer comes
// out, and the tests below run it on synthetic readings with no microphone in sight.
//
// The voice is also what makes this a MUSICAL rule rather than a signal-processing one: judging an interval means
// comparing a note to another note, exactly like the rest of the domain, and never comparing a frequency to a
// reference frequency.
// =====================================================================================================================

#include <cstdint>

namespace musichien::domain
{

class SungIntervalDetector
{
public:
    // What the detector has understood so far.
    struct Reading
    {
        // The note the singer started on, once one has been held long enough. 0 means "not yet heard".
        std::int32_t firstMidiNumber{ 0 };

        // The note the singer arrived on, once IT has been held long enough. 0 means "not yet".
        std::int32_t secondMidiNumber{ 0 };

        // True once both notes have been heard: the answer is complete.
        [[nodiscard]] bool hasInterval() const noexcept { return ( firstMidiNumber != 0 ) && ( secondMidiNumber != 0 ); }

        // The distance between the two, in semitones. Negative when the singer went DOWN, which matters: a falling
        // fifth and a rising fifth are not the same interval.
        [[nodiscard]] std::int32_t semitones() const noexcept { return secondMidiNumber - firstMidiNumber; }
    };

    // A reading arrives every few milliseconds. The caller says how much time passed since the previous one: the
    // detector has no clock of its own, and a rule of the game must not own one.
    void update( double p_frequencyHz, double p_referencePitchHz, std::int32_t p_elapsedMilliseconds ) noexcept;

    void reset() noexcept;

    [[nodiscard]] const Reading & reading() const noexcept { return m_reading; }

private:
    // How long a note must be held before it counts as the note the singer MEANT. Long on purpose: a whole second
    // and a bit means a wobble, a breath or a slide never reads as a note - the voice gets the time it needs, and
    // the result feels smooth rather than twitchy.
    static constexpr std::int32_t MINIMUM_HOLD_MILLISECONDS = 1200;

    // How fast the tracked pitch follows a reading. 0.3 keeps 70% of the previous estimate at every reading: slow
    // enough to absorb noise, fast enough to follow a real change of note. The tracked pitch is then ROUNDED to the
    // nearest note, which is what gives the tolerance: a reading must move the average half a semitone before the
    // note changes.
    static constexpr double TRACKING_ALPHA = 0.3;

    Reading m_reading;

    // The note being held right now, and for how long. 0 means no note is being held.
    std::int32_t m_heldMidiNumber{ 0 };

    std::int32_t m_heldMilliseconds{ 0 };

    // The tracked pitch, smoothed across readings. This is what absorbs the noise: a single reading never decides a
    // note, the average of the last few does. 0.0 means "not tracking yet".
    double m_trackedMidi{ 0.0 };
};

}    // namespace musichien::domain
