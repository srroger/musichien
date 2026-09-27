#include "ui/MicrophoneController.h"

#include "domain/audio/PitchDetector.h"
#include "domain/exercise/PlayerPreferences.h"
#include "domain/music/Note.h"
#include "domain/music/Temperament.h"

#include <QCoreApplication>
#include <QPermission>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>

namespace musichien::ui
{

namespace
{

// The pitch ratio maps a frequency onto the 0..1 band the screen uses to place the ball on the staff. It is
// logarithmic, because an octave is a doubling, not a fixed number of hertz: 120 Hz and 240 Hz must land the same
// distance apart as 600 Hz and 1200 Hz.
constexpr double LOWEST_VISIBLE_HZ = 60.0;
constexpr double HIGHEST_VISIBLE_HZ = 1200.0;

[[nodiscard]] double pitchRatioFor( double p_frequencyHz )
{
    if( p_frequencyHz <= 0.0 )
    {
        return 0.0;
    }

    const double clamped = std::clamp( p_frequencyHz, LOWEST_VISIBLE_HZ, HIGHEST_VISIBLE_HZ );

    return std::log( clamped / LOWEST_VISIBLE_HZ ) / std::log( HIGHEST_VISIBLE_HZ / LOWEST_VISIBLE_HZ );
}

// Turns a frequency into a human label: the closest note name, then the rounded hertz. Silence is an em-dash, not a
// note, and a pitch outside the MIDI range is reported honestly as a number.
// The note a frequency is closest to: the MIDI number that rounds to the nearest semitone. Nothing when there is no
// pitch at all, or when the pitch falls outside the playable range. The diapason decides where A4 sits.
[[nodiscard]] std::optional<domain::Note> nearestNoteFor( double p_frequencyHz, double p_referencePitchHz )
{
    if( p_frequencyHz <= 0.0 )
    {
        return std::nullopt;
    }

    const double semitonesFromA4 = std::round( static_cast<double>( domain::SEMITONES_PER_OCTAVE )
                                               * std::log2( p_frequencyHz / p_referencePitchHz ) );

    const auto midiNumber = static_cast<std::int32_t>( domain::REFERENCE_MIDI_NUMBER + semitonesFromA4 );

    if( midiNumber < domain::Note::MINIMUM_MIDI_NUMBER || midiNumber > domain::Note::MAXIMUM_MIDI_NUMBER )
    {
        return std::nullopt;
    }

    return domain::Note{ midiNumber };
}

// How far from the nearest note counts as in tune. These are a tuner's usual bands, and they are generous on purpose:
// five cents is the ear's own limit on a held note, twenty is audibly off while still being the right note.
constexpr double IN_TUNE_CENTS = 5.0;
constexpr double OFF_CENTS = 20.0;

// 0 in tune, 1 close, 2 off. A screen turns this into a colour; the threshold itself is a musical judgement, so it
// lives here rather than in the QML.
[[nodiscard]] int tuningStateFor( bool p_hasPitch, double p_cents )
{
    if( !p_hasPitch )
    {
        return 0;
    }

    const double magnitude = std::abs( p_cents );

    if( magnitude <= IN_TUNE_CENTS )
    {
        return 0;
    }

    if( magnitude <= OFF_CENTS )
    {
        return 1;
    }

    return 2;
}

[[nodiscard]] QString noteLabelFor( double p_frequencyHz, double p_referencePitchHz )
{
    if( p_frequencyHz <= 0.0 )
    {
        return QStringLiteral( "\u2014" );
    }

    const std::optional<domain::Note> note = nearestNoteFor( p_frequencyHz, p_referencePitchHz );

    if( !note.has_value() )
    {
        return QStringLiteral( "%1 Hz" ).arg( p_frequencyHz, 0, 'f', 0 );
    }

    return QStringLiteral( "%1  %2 Hz" ).arg( QString::fromStdString( note->name() ) ).arg( p_frequencyHz, 0, 'f', 0 );
}

}    // namespace

MicrophoneController::MicrophoneController( QStringList p_deviceNames,
                                            DetectorFactory p_factory,
                                            musichien::domain::PlayerPreferences * p_preferences,
                                            QObject * p_parent )
  : QObject{ p_parent }
  , m_deviceNames{ std::move( p_deviceNames ) }
  , m_factory{ std::move( p_factory ) }
  , m_preferences{ p_preferences }
{
    // An empty list would leave the ComboBox with nothing to show. The message says what to LOOK AT: on a desktop
    // this is almost always a sound card whose active profile has no input - the microphone exists, ALSA sees it, and
    // the audio server simply does not expose it. Telling the player that is worth more than saying "nothing here".
    if( m_deviceNames.isEmpty() )
    {
        m_deviceNames = { tr( "Aucune entrée audio détectée — vérifie le profil de ta carte son (entrée stéréo)" ) };
    }
}

MicrophoneController::~MicrophoneController() = default;

void MicrophoneController::selectDevice( int p_deviceIndex )
{
    if( p_deviceIndex < 0 || p_deviceIndex >= m_deviceNames.size() )
    {
        return;
    }

    m_currentDeviceIndex = p_deviceIndex;

    m_detector.reset();

    if( m_isListening )
    {
        ensureDetector();
        m_detector->start( [this]( float p_frequencyHz ) { onPitch( p_frequencyHz ); } );
    }

    emit currentDeviceIndexChanged();
}

void MicrophoneController::startTest()
{
    // The microphone is a RUNTIME permission on Android; on the desktop it is always granted and the callback fires
    // immediately. Asking here, at the moment of use, is the whole point: never at launch, never for nothing.
    QCoreApplication::instance()->requestPermission( QMicrophonePermission{}, [this]( const QPermission & p_permission ) {
        if( p_permission.status() != Qt::PermissionStatus::Granted )
        {
            return;
        }

        ensureDetector();

        if( m_detector )
        {
            m_detector->start( [this]( float p_frequencyHz ) { onPitch( p_frequencyHz ); } );
            m_isListening = true;

            emit isListeningChanged();
        }
    } );
}

void MicrophoneController::stopTest()
{
    if( m_detector )
    {
        m_detector->stop();
    }

    m_isListening = false;

    emit isListeningChanged();

    onPitch( 0.0F );
}

void MicrophoneController::ensureDetector()
{
    if( m_detector )
    {
        return;
    }

    if( m_factory )
    {
        m_detector = m_factory( m_currentDeviceIndex );
    }
}

void MicrophoneController::onPitch( float p_frequencyHz )
{
    m_detectedFrequencyHz = static_cast<double>( p_frequencyHz );

    const double referencePitch = ( m_preferences != nullptr ) ? m_preferences->storedReferencePitch() : 440.0;

    m_detectedPitchRatio = pitchRatioFor( m_detectedFrequencyHz );
    m_detectedNoteLabel = noteLabelFor( m_detectedFrequencyHz, referencePitch );

    // The continuous MIDI position of the voice: 69 is A4, 69.5 is halfway to the next note. The staff draws the
    // ball from this, so that a note sung perfectly in tune lands exactly on its line or space.
    m_detectedMidi = m_detectedFrequencyHz > 0.0
                       ? static_cast<double>( domain::REFERENCE_MIDI_NUMBER )
                           + ( static_cast<double>( domain::SEMITONES_PER_OCTAVE )
                               * std::log2( m_detectedFrequencyHz / referencePitch ) )
                       : 0.0;

    // The tuner part: how far the voice is from the note it is closest to. This is what makes the page useful
    // outside the game - checking a guitar string, or hearing how flat yesterday's cold left the voice.
    const std::optional<domain::Note> nearest = nearestNoteFor( m_detectedFrequencyHz, referencePitch );

    if( nearest.has_value() )
    {
        // The tuner honours the chosen tuning AND the chosen diapason. In equal temperament the reference is the note
        // itself; in the others it depends on the root - and this is the SAME calculation the audio layer will use to
        // play intervals, so the tuner and the game will never disagree.
        const domain::Temperament temperament = ( m_preferences != nullptr )
                                                  ? m_preferences->storedTemperament()
                                                  : domain::Temperament::Equal;

        const domain::Note root = ( m_preferences != nullptr ) ? m_preferences->storedTuningRoot()
                                                               : domain::Note{ 60 };

        m_detectedCents = domain::centsBetween( m_detectedFrequencyHz,
                                                domain::frequencyFor( *nearest, root, temperament, referencePitch ) );
    }
    else
    {
        m_detectedCents = 0.0;
    }

    m_detectedTuningState = tuningStateFor( nearest.has_value(), m_detectedCents );

    emit detectedFrequencyHzChanged();
    emit detectedPitchRatioChanged();
    emit detectedMidiChanged();
    emit detectedNoteLabelChanged();
    emit detectedCentsChanged();
    emit detectedTuningStateChanged();
}

}    // namespace musichien::ui
