#include "ui/IntervalPlaybackController.h"

#include "domain/music/Note.h"

#include <array>
#include <chrono>
#include <utility>

namespace musichien::ui
{

namespace
{

// Middle C, the reference note of every exercise of the first world.
constexpr std::int32_t ROOT_MIDI_NUMBER = 60;

// A perfect fifth above the root: seven semitones.
constexpr std::int32_t PERFECT_FIFTH_SEMITONES = 7;

// Silence left between the two notes of a melodic interval.
//
// Long enough to hear two distinct notes, short enough to still hear them as one interval. This value
// is a musical decision, not a technical one: it will be tuned by ear.
constexpr std::chrono::milliseconds MELODIC_GAP{ 280 };

}    // namespace

IntervalPlaybackController::IntervalPlaybackController( domain::NotePlayer & p_notePlayer,
                                                        QObject * p_parent )
  : QObject{ p_parent }
  , m_notePlayer{ p_notePlayer }
{
}

QString IntervalPlaybackController::lastPlayedIntervalName() const
{
    return m_lastPlayedIntervalName;
}

void IntervalPlaybackController::setLastPlayedIntervalName( QString p_intervalName )
{
    if( m_lastPlayedIntervalName == p_intervalName )
    {
        return;
    }

    m_lastPlayedIntervalName = std::move( p_intervalName );

    emit lastPlayedIntervalChanged();
}

void IntervalPlaybackController::playSingleNote()
{
    m_notePlayer.playNote( domain::Note{ ROOT_MIDI_NUMBER } );

    setLastPlayedIntervalName( QString{} );
}

void IntervalPlaybackController::playPerfectFifth()
{
    const domain::Note rootNote{ ROOT_MIDI_NUMBER };
    const domain::Note upperNote = rootNote.transposedBy( PERFECT_FIFTH_SEMITONES );

    const std::array<domain::Note, 2> fifthNotes{ rootNote, upperNote };

    m_notePlayer.playChord( fifthNotes );

    // The interval is identified by the DOMAIN. This class only displays the result: if the naming
    // rule changed one day, it would change in the domain and in its tests, not here.
    const domain::Interval interval = domain::intervalBetween( rootNote, upperNote );

    setLastPlayedIntervalName( QString::fromStdString( interval.name() ) );
}

void IntervalPlaybackController::playMelodicFifth()
{
    const domain::Note rootNote{ ROOT_MIDI_NUMBER };
    const domain::Note upperNote = rootNote.transposedBy( PERFECT_FIFTH_SEMITONES );

    const std::array<domain::Note, 2> fifthNotes{ rootNote, upperNote };

    m_notePlayer.playMelody( fifthNotes, MELODIC_GAP );

    const domain::Interval interval = domain::intervalBetween( rootNote, upperNote );

    setLastPlayedIntervalName( QString::fromStdString( interval.name() ) );
}

void IntervalPlaybackController::stopPlayback()
{
    m_notePlayer.stopAll();
}

}    // namespace musichien::ui
