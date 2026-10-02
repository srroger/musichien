#include "ui/IntervalPlaybackController.h"

#include "domain/music/Interval.h"
#include "domain/music/Temperament.h"
#include "ui/IntervalDescription.h"

#include <QString>
#include <QStringList>

#include <array>
#include <chrono>
#include <utility>

namespace musichien::ui
{

namespace
{

// Middle C, the reference note of every exercise of the first world.
constexpr std::int32_t ROOT_MIDI_NUMBER = 60;

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
    // The choices are built once, from the domain's own list, and never again: the screen asks for
    // them, it never composes them.
    for( const domain::Interval & interval : domain::allSupportedIntervals() )
    {
        m_supportedIntervals.append( describeInterval( interval ) );
    }
}

QVariantList IntervalPlaybackController::supportedIntervals() const
{
    return m_supportedIntervals;
}

QVariantMap IntervalPlaybackController::lastPlayedInterval() const
{
    return m_lastPlayedInterval;
}

bool IntervalPlaybackController::harmonicPlayback() const
{
    return m_harmonicPlayback;
}

void IntervalPlaybackController::setHarmonicPlayback( bool p_harmonicPlayback )
{
    if( m_harmonicPlayback == p_harmonicPlayback )
    {
        return;
    }

    m_harmonicPlayback = p_harmonicPlayback;

    emit harmonicPlaybackChanged();
}

QString IntervalPlaybackController::playedFrequencies() const
{
    return m_playedFrequencies;
}

void IntervalPlaybackController::setTuning( domain::TuningContext p_tuning )
{
    m_tuning = p_tuning;
}

void IntervalPlaybackController::setPlayedFrequencies( std::span<const domain::Note> p_notes )
{
    if( p_notes.empty() )
    {
        m_playedFrequencies.clear();

        emit playedFrequenciesChanged();

        return;
    }

    const domain::Note root = p_notes.front();

    QStringList parts;

    for( const domain::Note & note : p_notes )
    {
        const double hertz = domain::frequencyFor( note, root, m_tuning.temperament, m_tuning.referencePitchHz );

        parts.append( QStringLiteral( "%1 · %2 Hz" )
                        .arg( QString::fromStdString( note.name() ) )
                        .arg( hertz, 0, 'f', 2 ) );
    }

    m_playedFrequencies = parts.join( QStringLiteral( "  →  " ) );

    emit playedFrequenciesChanged();
}

void IntervalPlaybackController::setLastPlayedInterval( QVariantMap p_description )
{
    // Only notify when something actually changed: replaying the same interval must not make the
    // screen blink.
    if( m_lastPlayedInterval == p_description )
    {
        return;
    }

    m_lastPlayedInterval = std::move( p_description );

    emit lastPlayedIntervalChanged();
}

void IntervalPlaybackController::playAndDescribe( const domain::Note & p_rootNote,
                                                  const domain::Note & p_upperNote )
{
    const std::array<domain::Note, 2> notes{ p_rootNote, p_upperNote };

    if( m_harmonicPlayback )
    {
        m_notePlayer.playChord( notes );
    }
    else
    {
        m_notePlayer.playMelody( notes, MELODIC_GAP );
    }

    setPlayedFrequencies( notes );

    // The interval is identified by the DOMAIN, and from the two notes that were ACTUALLY played
    // rather than from the distance that was asked for. Should the playable range ever clamp a note,
    // the name displayed would then follow what was really heard instead of quietly lying.
    setLastPlayedInterval( describeInterval( domain::intervalBetween( p_rootNote, p_upperNote ) ) );
}

void IntervalPlaybackController::playInterval( int p_semitones )
{
    const domain::Note rootNote{ ROOT_MIDI_NUMBER };
    const domain::Note upperNote = rootNote.transposedBy( p_semitones );

    playAndDescribe( rootNote, upperNote );
}

void IntervalPlaybackController::playSingleNote()
{
    m_notePlayer.playNote( domain::Note{ ROOT_MIDI_NUMBER } );

    setPlayedFrequencies( std::array<domain::Note, 1>{ domain::Note{ ROOT_MIDI_NUMBER } } );

    // A single note is not an interval: there is nothing to name. The empty description is what tells
    // the screen to fall back to its generic prompt.
    setLastPlayedInterval( QVariantMap{} );
}

void IntervalPlaybackController::playSustainedInterval( int p_semitones )
{
    const domain::Note rootNote{ ROOT_MIDI_NUMBER };
    const domain::Note upperNote = rootNote.transposedBy( p_semitones );

    const std::array<domain::Note, 2> notes{ rootNote, upperNote };

    // Six seconds, played TOGETHER: the two frequencies beat against each other, and the beating is what the ear
    // uses to hear a temperament. A short chord does not give the ear time to count it.
    m_notePlayer.playChordFor( notes, std::chrono::seconds{ 6 } );

    setPlayedFrequencies( notes );

    setLastPlayedInterval( describeInterval( domain::intervalBetween( rootNote, upperNote ) ) );
}

void IntervalPlaybackController::stopPlayback()
{
    m_notePlayer.stopAll();
}

}    // namespace musichien::ui
