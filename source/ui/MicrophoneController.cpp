#include "ui/MicrophoneController.h"

#include "domain/audio/PitchDetector.h"
#include "domain/music/Note.h"

#include <QCoreApplication>
#include <QPermission>

#include <algorithm>
#include <cmath>
#include <cstdint>
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
[[nodiscard]] QString noteLabelFor( double p_frequencyHz )
{
    if( p_frequencyHz <= 0.0 )
    {
        return QStringLiteral( "\u2014" );
    }

    const double semitonesFromA4 = std::round( static_cast<double>( domain::SEMITONES_PER_OCTAVE )
                                               * std::log2( p_frequencyHz / domain::REFERENCE_FREQUENCY_HZ ) );

    const std::int32_t midiNumber = domain::REFERENCE_MIDI_NUMBER + static_cast<std::int32_t>( semitonesFromA4 );

    if( midiNumber < domain::Note::MINIMUM_MIDI_NUMBER || midiNumber > domain::Note::MAXIMUM_MIDI_NUMBER )
    {
        return QStringLiteral( "%1 Hz" ).arg( p_frequencyHz, 0, 'f', 0 );
    }

    const domain::Note note{ midiNumber };

    return QStringLiteral( "%1  %2 Hz" ).arg( QString::fromStdString( note.name() ) ).arg( p_frequencyHz, 0, 'f', 0 );
}

}    // namespace

MicrophoneController::MicrophoneController( QStringList p_deviceNames, DetectorFactory p_factory, QObject * p_parent )
  : QObject{ p_parent }
  , m_deviceNames{ std::move( p_deviceNames ) }
  , m_factory{ std::move( p_factory ) }
{
    // An empty list would leave the ComboBox with nothing to show; the screen still needs one line, and "nothing
    // detected" is the honest description of a machine without a microphone.
    if( m_deviceNames.isEmpty() )
    {
        m_deviceNames = { tr( "No microphone detected" ) };
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
    m_detectedPitchRatio = pitchRatioFor( m_detectedFrequencyHz );
    m_detectedNoteLabel = noteLabelFor( m_detectedFrequencyHz );

    emit detectedFrequencyHzChanged();
    emit detectedPitchRatioChanged();
    emit detectedNoteLabelChanged();
}

}    // namespace musichien::ui
