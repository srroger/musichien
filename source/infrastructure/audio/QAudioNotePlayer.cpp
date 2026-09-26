#include "infrastructure/audio/QAudioNotePlayer.h"

#include "domain/music/Note.h"

#include <QAudioDevice>
#include <QMediaDevices>

#include <algorithm>
#include <format>
#include <iostream>
#include <iterator>
#include <utility>

namespace musichien::infrastructure
{

namespace
{

// Duration of a single note, as heard in an exercise.
constexpr std::chrono::milliseconds DEFAULT_NOTE_DURATION{ 700 };

// Extra room given to the sink buffer, so that a single write always fits.
constexpr int BUFFER_MARGIN_BYTES = 4096;

// The synthesizer already normalises its own output, so the sink must not attenuate it further.
constexpr double SINK_VOLUME = 1.0;

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

    // Start from what the device prefers, INCLUDING its channel layout, and only force what the
    // synthesizer really needs: 32 bit float samples.
    //
    // Asking for a mono stream was a mistake. On this machine the sound then came out of the left
    // channel only, because the backend did not upmix it. Using the layout the device announces, and
    // duplicating the mono signal over it, gives a properly centred sound.
    QAudioFormat requestedFormat = outputDevice.preferredFormat();
    requestedFormat.setSampleFormat( QAudioFormat::Float );

    if( !outputDevice.isFormatSupported( requestedFormat ) )
    {
        // Fall back to the raw preferred format. It may not be Float, which is checked below.
        requestedFormat = outputDevice.preferredFormat();
    }

    m_audioSink = std::make_unique<QAudioSink>( outputDevice, requestedFormat );

    // A sink only reveals the format it REALLY opened once its stream has been started. It is
    // therefore opened once here and stopped immediately.
    //
    // This matters twice over: using the requested sample rate instead of the real one would make
    // every note out of tune, and using the requested channel count would put the sound on the
    // wrong channels.
    m_audioSink->start();
    m_audioFormat = m_audioSink->format();
    m_audioSink->stop();

    if( m_audioFormat.sampleFormat() != QAudioFormat::Float )
    {
        std::cerr << "Musichien: the audio device does not accept 32 bit float samples (it wants "
                  << static_cast<int>( m_audioFormat.sampleFormat() ) << "). Playback stays silent.\n";

        // Assigned rather than reset(): QAudioSink has a reset() method of its own, so a call to
        // "m_audioSink.reset()" would read as if it acted on the sound stream.
        m_audioSink = nullptr;
        m_outputDescription = "unsupported sample format";

        return;
    }

    // The synthesizer is created from the REAL sample rate of the device. This single line is what
    // keeps every note in tune.
    m_synthesizer.emplace( m_audioFormat.sampleRate() );

    m_outputDescription = std::format( "{} ({} Hz, {} channel(s), sample format {})",
                                       outputDevice.description().toStdString(),
                                       m_audioFormat.sampleRate(),
                                       m_audioFormat.channelCount(),
                                       static_cast<int>( m_audioFormat.sampleFormat() ) );

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

    // The synthesizer produces MONO samples, which is musically correct: a single tone is a single
    // signal. Turning that into the channel layout of the device is the job of this adapter.
    //
    // The same sample is written to EVERY channel. Letting the audio backend upmix a mono stream is
    // what produced a sound heard only from the left channel.
    const auto channelCount = static_cast<std::size_t>( std::max( 1, m_audioFormat.channelCount() ) );

    std::vector<float> channelSamples;
    channelSamples.reserve( m_currentSamples.size() * channelCount );

    for( const float monoSample : m_currentSamples )
    {
        std::fill_n( std::back_inserter( channelSamples ), channelCount, monoSample );
    }

    const auto byteCount = static_cast<qint64>( channelSamples.size() )
                           * static_cast<qint64>( sizeof( float ) );

    // The whole buffer is handed over in a single write, so the sink must be able to hold it. Without
    // this, a write longer than the internal buffer would be truncated and the note cut in the middle.
    m_audioSink->setBufferSize( static_cast<int>( byteCount ) + BUFFER_MARGIN_BYTES );
    m_audioSink->setVolume( SINK_VOLUME );

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
      reinterpret_cast<const char *>( channelSamples.data() ),    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
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
