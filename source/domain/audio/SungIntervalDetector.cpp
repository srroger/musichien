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

    // Silence breaks the hold, but does NOT erase the note already accepted: taking a breath in the middle of an
    // exercise must not cost the first note.
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

    // Still the same note? Then the hold continues. Otherwise a new hold starts, from zero: a note that was brushed
    // on the way to another one never had time to count.
    const bool sameNote = ( m_heldMidiNumber != 0 )
                          && ( std::abs( midiNumber - static_cast<double>( m_heldMidiNumber ) ) < SAME_NOTE_SEMITONES );

    if( !sameNote )
    {
        m_heldMidiNumber = static_cast<std::int32_t>( std::lround( midiNumber ) );
        m_heldMilliseconds = 0;
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
