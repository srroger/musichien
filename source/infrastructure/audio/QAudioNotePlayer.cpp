#include "infrastructure/audio/QAudioNotePlayer.h"

#include "domain/music/Note.h"

#include <QAudioDevice>
#include <QMediaDevices>

#include <format>
#include <iostream>
#include <utility>

namespace musichien::infrastructure
{

namespace
{

// Duration of a single note, as heard in an exercise.
constexpr std::chrono::milliseconds DEFAULT_NOTE_DURATION{ 700 };

// Sample rate asked from the audio device. 48000 Hz is the native rate of Android devices and is
// supported almost everywhere.
constexpr int REQUESTED_SAMPLE_RATE = 48000;

// One channel is enough for a single tone, and halves the amount of data to push.
constexpr int REQUESTED_CHANNEL_COUNT = 1;

// Extra room given to the sink buffer, so that a single write always fits.
constexpr int BUFFER_MARGIN_BYTES = 4096;

}    // namespace

QAudioNotePlayer::QAudioNotePlayer() = default;

QAudioNotePlayer::~QAudioNotePlayer()
{
    // The sink is still alive here: members are destroyed after the destructor body.
    stopAll();
}

std::chrono::milliseconds QAudioNotePlayer::noteDuration() const
{
    return DEFAULT_NOTE_DURATION;
}

void QAudioNotePlayer::prepareAudioOutput()
{
    ensureAudioOutputIsOpen();
}

bool QAudioNotePlayer::isAudioOutputAvailable() const noexcept
{
    return m_synthesizer.has_value();
}

std::string QAudioNotePlayer::audioOutputDescription() const
{
    return m_outputDescription;
}

void QAudioNotePlayer::ensureAudioOutputIsOpen()
{
    if( m_audioSink )
    {
        return;
    }

    const QAudioDevice outputDevice = QMediaDevices::defaultAudioOutput();

    if( outputDevice.isNull() )
    {
        m_outputDescription = "no audio output device";
        std::cerr << "Musichien: no audio output device: the notes will stay silent.\n";
        return;
    }

    QAudioFormat requestedFormat;
    requestedFormat.setSampleRate( REQUESTED_SAMPLE_RATE );
    requestedFormat.setChannelCount( REQUESTED_CHANNEL_COUNT );
    requestedFormat.setSampleFormat( QAudioFormat::Float );

    // A device that cannot honour the request is asked what it would accept instead. Losing the
    // sample rate is an inconvenience; playing nothing at all would be a bug.
    if( !outputDevice.isFormatSupported( requestedFormat ) )
    {
        const QAudioFormat preferredFormat = outputDevice.preferredFormat();

        requestedFormat = preferredFormat;
        requestedFormat.setChannelCount( REQUESTED_CHANNEL_COUNT );
        requestedFormat.setSampleFormat( QAudioFormat::Float );

        std::cerr << "Musichien: the audio device does not support " << REQUESTED_SAMPLE_RATE
                  << " Hz, falling back to " << requestedFormat.sampleRate() << " Hz\n";
    }

    m_audioFormat = requestedFormat;

    // The synthesizer is created from the real sample rate of the device. This single line is what
    // keeps every note in tune: generating at 44100 Hz and playing at 48000 Hz would shift the pitch.
    m_synthesizer.emplace( m_audioFormat.sampleRate() );

    m_audioSink = std::make_unique<QAudioSink>( outputDevice, m_audioFormat );

    m_outputDescription = std::format( "{} ({} Hz, {} channel(s))",
                                       outputDevice.description().toStdString(),
                                       m_audioFormat.sampleRate(),
                                       m_audioFormat.channelCount() );

    std::cerr << "Musichien: audio output opened on " << m_outputDescription << "\n";
}

void QAudioNotePlayer::playSamples( std::vector<float> p_samples )
{
    ensureAudioOutputIsOpen();

    if( !m_audioSink || p_samples.empty() )
    {
        return;
    }

    // A new note interrupts the previous one: in an ear training exercise, two overlapping notes make
    // the interval impossible to identify.
    stopAll();

    m_currentSamples = std::move( p_samples );

    const auto byteCount = static_cast<qint64>( m_currentSamples.size() )
                           * static_cast<qint64>( sizeof( float ) );

    // The whole buffer is handed over in a single write, so the sink must be able to hold it. Without
    // this, a write longer than the internal buffer would be truncated and the note cut in the middle.
    m_audioSink->setBufferSize( static_cast<int>( byteCount ) + BUFFER_MARGIN_BYTES );

    m_audioOutputDevice = m_audioSink->start();

    if( m_audioOutputDevice == nullptr )
    {
        std::cerr << "Musichien: the audio output could not be started.\n";
        return;
    }

    // QIODevice::write only accepts a "const char *", even for binary data. This is the documented Qt
    // idiom for pushing raw audio, and the only place in the project where a cast of this kind is
    // needed. It is therefore silenced locally rather than globally.
    const qint64 writtenByteCount = m_audioOutputDevice->write(
      reinterpret_cast<const char *>( m_currentSamples.data() ),    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
      byteCount );

    if( writtenByteCount < byteCount )
    {
        std::cerr << "Musichien: only " << writtenByteCount << " of " << byteCount
                  << " bytes could be written to the audio output.\n";
    }
}

void QAudioNotePlayer::stopAll()
{
    m_currentSamples.clear();

    if( m_audioSink )
    {
        // stop() also releases the audio device, which matters on a phone: an output left open drains
        // the battery.
        m_audioSink->stop();
    }

    m_audioOutputDevice = nullptr;
}

void QAudioNotePlayer::playNote( const domain::Note & p_note )
{
    ensureAudioOutputIsOpen();

    if( !m_synthesizer.has_value() )
    {
        return;
    }

    playSamples( m_synthesizer->renderNote( p_note, noteDuration() ) );
}

void QAudioNotePlayer::playMelody( std::span<const domain::Note> p_notes,
                                   std::chrono::milliseconds p_gap )
{
    ensureAudioOutputIsOpen();

    if( !m_synthesizer.has_value() )
    {
        return;
    }

    playSamples( m_synthesizer->renderMelody( p_notes, noteDuration(), p_gap ) );
}

void QAudioNotePlayer::playChord( std::span<const domain::Note> p_notes )
{
    ensureAudioOutputIsOpen();

    if( !m_synthesizer.has_value() )
    {
        return;
    }

    playSamples( m_synthesizer->renderChord( p_notes, noteDuration() ) );
}

}    // namespace musichien::infrastructure
