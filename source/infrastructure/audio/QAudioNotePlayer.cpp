#include "infrastructure/audio/QAudioNotePlayer.h"

#include "domain/music/Note.h"

#include <QAudioDevice>
#include <QMediaDevices>

#include <algorithm>
#include <array>
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

void QAudioNotePlayer::setTuning( domain::TuningContext p_tuning )
{
    m_tuning = p_tuning;
}

void QAudioNotePlayer::playNote( const domain::Note & p_note )
{
    ensureAudioOutputIsOpen();

    if( !m_synthesizer.has_value() )
    {
        return;
    }

    const std::span<const domain::Note> notes{ &p_note, 1 };

    playSamples( renderNoteFor( notes, p_note, noteDuration() ) );
}

void QAudioNotePlayer::playMelody( std::span<const domain::Note> p_notes,
                                   std::chrono::milliseconds p_gap )
{
    ensureAudioOutputIsOpen();

    if( !m_synthesizer.has_value() )
    {
        return;
    }

    playSamples( renderMelodyFor( p_notes, noteDuration(), p_gap ) );
}

void QAudioNotePlayer::playChord( std::span<const domain::Note> p_notes )
{
    ensureAudioOutputIsOpen();

    if( !m_synthesizer.has_value() )
    {
        return;
    }

    playSamples( renderChordFor( p_notes, noteDuration() ) );
}

void QAudioNotePlayer::playChordFor( std::span<const domain::Note> p_notes,
                                     std::chrono::milliseconds p_duration )
{
    ensureAudioOutputIsOpen();

    if( !m_synthesizer.has_value() )
    {
        return;
    }

    playSamples( renderChordFor( p_notes, p_duration ) );
}

void QAudioNotePlayer::useInstruments( std::vector<domain::SampledInstrument> p_instruments,
                                       std::vector<domain::Waveform> p_waveforms )
{
    m_instruments = std::move( p_instruments );

    m_waveforms = std::move( p_waveforms );

    m_instrumentIndex = 0;

    m_lastPlayedNotes.clear();
}

std::size_t QAudioNotePlayer::timbreIndexFor( std::span<const domain::Note> p_notes )
{
    const std::size_t timbreCount = m_instruments.size() + m_waveforms.size();

    // The same question is the same NOTES, whatever order the melody played them in: a falling fifth and its
    // feedback chord share the same two notes. Comparing them as an ORDERED sequence would re-draw the instrument
    // between the melody and the chord - the guitar turning into a saxophone in front of the player, which is
    // exactly the bug Roger saw.
    const bool sameQuestion = ( p_notes.size() == m_lastPlayedNotes.size() )
                              && std::is_permutation( p_notes.begin(), p_notes.end(), m_lastPlayedNotes.begin() );

    if( !sameQuestion )
    {
        m_lastPlayedNotes.assign( p_notes.begin(), p_notes.end() );

        std::uniform_int_distribution<std::size_t> distribution{ 0, timbreCount - 1 };

        m_instrumentIndex = distribution( m_instrumentRandomEngine );
    }

    return m_instrumentIndex;
}

std::vector<float> QAudioNotePlayer::renderNoteFor( std::span<const domain::Note> p_sequence,
                                                    const domain::Note & p_note,
                                                    std::chrono::milliseconds p_duration )
{
    const std::size_t timbreCount = m_instruments.size() + m_waveforms.size();

    if( timbreCount == 0 )
    {
        return m_synthesizer->renderNote( p_note, p_duration, m_tuning );
    }

    const std::size_t index = timbreIndexFor( p_sequence );

    if( index < m_instruments.size() )
    {
        return m_instruments.at( index ).renderNote( p_note, p_duration, m_audioFormat.sampleRate(), m_tuning );
    }

    return m_synthesizer->renderWaveNote( p_note, m_waveforms.at( index - m_instruments.size() ), p_duration, m_tuning );
}

std::vector<float> QAudioNotePlayer::renderChordFor( std::span<const domain::Note> p_notes,
                                                     std::chrono::milliseconds p_duration )
{
    const std::size_t timbreCount = m_instruments.size() + m_waveforms.size();

    if( timbreCount == 0 )
    {
        return m_synthesizer->renderChord( p_notes, p_duration, m_tuning );
    }

    const std::size_t index = timbreIndexFor( p_notes );

    if( index < m_instruments.size() )
    {
        return m_instruments.at( index ).renderChord( p_notes, p_duration, m_audioFormat.sampleRate(), m_tuning );
    }

    return m_synthesizer->renderWaveChord( p_notes,
                                           m_waveforms.at( index - m_instruments.size() ),
                                           p_duration,
                                           m_tuning );
}

std::vector<float> QAudioNotePlayer::renderMelodyFor( std::span<const domain::Note> p_notes,
                                                      std::chrono::milliseconds p_noteDuration,
                                                      std::chrono::milliseconds p_gap )
{
    const std::size_t timbreCount = m_instruments.size() + m_waveforms.size();

    if( timbreCount == 0 )
    {
        return m_synthesizer->renderMelody( p_notes, p_noteDuration, p_gap, m_tuning );
    }

    const std::size_t index = timbreIndexFor( p_notes );

    if( index < m_instruments.size() )
    {
        return m_instruments.at( index ).renderMelody( p_notes,
                                                       p_noteDuration,
                                                       p_gap,
                                                       m_audioFormat.sampleRate(),
                                                       m_tuning );
    }

    return m_synthesizer->renderWaveMelody( p_notes,
                                            m_waveforms.at( index - m_instruments.size() ),
                                            p_noteDuration,
                                            p_gap,
                                            m_tuning );
}

void QAudioNotePlayer::playMistakeCue()
{
    ensureAudioOutputIsOpen();

    if( !m_synthesizer.has_value() )
    {
        return;
    }

    // A short burst that replaces whatever was playing: a cue is a punctuation mark, and hearing it over
    // the interval it is commenting on would be confusing.
    playSamples( m_synthesizer->renderMistakeCue( domain::ToneSynthesizer::MISTAKE_CUE_DURATION ) );
}

void QAudioNotePlayer::playTapCue()
{
    ensureAudioOutputIsOpen();

    if( !m_synthesizer.has_value() )
    {
        return;
    }

    // Une note aiguë TRÈS courte, rendue par la synthèse et non par un échantillon : 60 ms, là où une note
    // échantillonnée dure 700 ms et porterait tout un timbre. Un clic de menu n'est pas un événement musical.
    std::vector<float> samples =
      m_synthesizer->renderNote( domain::Note{ 88 }, std::chrono::milliseconds{ 60 } );

    // Et discret : c'est le volume d'un accusé de réception, pas celui d'une réponse. Assez pour être entendu,
    // pas assez pour occuper l'oreille.
    constexpr float TAP_GAIN = 0.22F;

    for( float & sample : samples )
    {
        sample *= TAP_GAIN;
    }

    playSamples( std::move( samples ) );
}

void QAudioNotePlayer::playGreeting()
{
    ensureAudioOutputIsOpen();

    if( !m_synthesizer.has_value() )
    {
        return;
    }

    // L'arpège, tel que Roger l'a décrit : "à la Zelda, montant, sur des degrés un peu éthérés".
    //
    // Do, sol, do : une quinte et une octave, aucune tierce. C'est exactement ce qui rend un accord ouvert - il
    // n'y a pas de tierce pour dire majeur ou mineur, donc rien à comprendre, seulement quelque chose qui monte
    // et qui flotte. Une tierce aurait été un accord ; ceci est un appel.
    const std::array<domain::Note, 3> greetingNotes{ domain::Note{ 72 }, domain::Note{ 79 }, domain::Note{ 84 } };

    // Un souffle entre les notes : sans lui, trois notes jouées bout à bout sonnent comme une seule note qui
    // change de hauteur.
    constexpr std::chrono::milliseconds GREETING_GAP{ 40 };

    std::vector<float> samples;

    const bool hasPiano = !m_instruments.empty();

    if( hasPiano )
    {
        // Le PIANO et non le tirage au hasard : l'accueil doit être reconnaissable d'un lancement à l'autre, et
        // c'est le premier instrument chargé.
        samples = m_instruments.front().renderMelody( greetingNotes,
                                                      noteDuration(),
                                                      GREETING_GAP,
                                                      m_audioFormat.sampleRate(),
                                                      m_tuning );
    }
    else
    {
        samples = m_synthesizer->renderMelody( greetingNotes, noteDuration(), GREETING_GAP, m_tuning );
    }

    // Et surtout, DISCRET. C'est le volume qui répond au vrai reproche : au niveau des exercices, une
    // introduction n'est plus une introduction, c'est une fanfare.
    constexpr float GREETING_GAIN = 0.35F;

    for( float & sample : samples )
    {
        sample *= GREETING_GAIN;
    }

    playSamples( std::move( samples ) );
}

}    // namespace musichien::infrastructure
