#include "domain/audio/SungIntervalDetector.h"

#include "domain/music/Note.h"

#include <cmath>

namespace musichien::domain
{

void SungIntervalDetector::reset() noexcept
{
    m_reading = Reading{};
    m_heldMidiNumber = 0;
    m_heldMilliseconds = 0;
    m_trackedMidi = 0.0;
}

void SungIntervalDetector::update( double p_frequencyHz,
                                   double p_referencePitchHz,
                                   std::int32_t p_elapsedMilliseconds ) noexcept
{
    // Once the interval is found it does not move: that IS the answer, and re-reading it at every breath would make
    // it flicker. Only a reset asks for a new one.
    if( m_reading.hasInterval() )
    {
        return;
    }

    // Silence interrupts the hold, but does NOT erase the note already accepted, nor the tracked pitch: taking a
    // breath in the middle of an exercise must not cost the first note.
    if( ( p_frequencyHz <= 0.0 ) || ( p_referencePitchHz <= 0.0 ) )
    {
        m_heldMidiNumber = 0;
        m_heldMilliseconds = 0;

        return;
    }

    const double midiNumber = static_cast<double>( REFERENCE_MIDI_NUMBER )
                              + ( static_cast<double>( SEMITONES_PER_OCTAVE )
                                  * std::log2( p_frequencyHz / p_referencePitchHz ) );

    if( ( midiNumber < 0.0 ) || ( midiNumber > 127.0 ) )
    {
        m_heldMidiNumber = 0;
        m_heldMilliseconds = 0;

        return;
    }

    if( m_heldMidiNumber == 0 )
    {
        // First reading, or a fresh start after a breath: the tracking resumes on this pitch.
        m_trackedMidi = midiNumber;
        m_heldMidiNumber = static_cast<std::int32_t>( std::lround( midiNumber ) );
        m_heldMilliseconds = 0;
    }
    else
    {
        // The tracking: the VOICE wobbles, the tracked pitch must not. No single reading decides a note - the
        // average of the last few does, which is what absorbs the noise and the instability Roger heard.
        m_trackedMidi += ( midiNumber - m_trackedMidi ) * TRACKING_ALPHA;

        // The note is the tracked pitch, rounded. Rounding is the tolerance itself: the average has to move half a
        // semitone before the note changes, so a wobble around a note never reads as two notes.
        const auto note = static_cast<std::int32_t>( std::lround( m_trackedMidi ) );

        if( note != m_heldMidiNumber )
        {
            m_heldMidiNumber = note;
            m_heldMilliseconds = 0;
        }
    }

    m_heldMilliseconds += p_elapsedMilliseconds;

    if( m_heldMilliseconds < MINIMUM_HOLD_MILLISECONDS )
    {
        return;
    }

    // Held long enough: this note counts.
    if( m_reading.firstMidiNumber == 0 )
    {
        m_reading.firstMidiNumber = m_heldMidiNumber;

        return;
    }

    if( m_heldMidiNumber != m_reading.firstMidiNumber )
    {
        m_reading.secondMidiNumber = m_heldMidiNumber;
    }
}

}    // namespace musichien::domain
