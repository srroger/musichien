#include "infrastructure/audio/QAudioNotePlayer.h"

#include "domain/music/Note.h"

#include <QAudioDevice>
#include <QMediaDevices>
#include <QTimer>

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

// The synthesizer already normalises its own output, so the sink must not attenuate it further.
constexpr double SINK_VOLUME = 1.0;

// The sink buffer, in bytes. Its size IS the latency: the mixer is pulled ahead by that much, so a metronome click
// triggered now would be heard one buffer later. Sixteen kilobytes is roughly forty milliseconds at 48 kHz in stereo
// float - small enough that the click stays in time, large enough that the stream does not starve.
constexpr int SINK_BUFFER_BYTES = 16384;

// A drum hit is a SHORT sound, where a note lasts: at equal peak it sounds quieter. The samples are already normalised
// by scripts/render_drum_samples.py, so this only puts them at the level of the rest - and NOT above, because the mixer
// clamps, and a clamped kick is a distorted kick. That is what a first version of this constant got wrong.
constexpr float DRUM_GAIN = 1.0F;

// Le clic du metronome. Le premier temps doit s'entendre par-dessus tout le reste ; les autres doivent se faire oublier
// assez pour qu'on n'entende que la pulsation.
constexpr float ACCENTED_CLICK_GAIN = 1.0F;
constexpr float PLAIN_CLICK_GAIN = 0.7F;

// Le repli synthetise, quand les clics echantillonnes manquent.
constexpr float FALLBACK_ACCENTED_CLICK_GAIN = 0.30F;
constexpr float FALLBACK_PLAIN_CLICK_GAIN = 0.18F;

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

void QAudioNotePlayer::reopenAudioOutput()
{
    stopAll();

    // The sink and the two synthesisers were built for the OLD device's sample rate: they are discarded, and the
    // next ensure opens whatever is now the default device and rebuilds them at its own rate.
    //
    // ASSIGNED rather than reset() for the sink and the mixer, exactly as lower down in this file: QAudioSink and
    // QIODevice both have a reset() method of their own, so "m_audioSink.reset()" would read as if it acted on the
    // sound stream rather than on the pointer. clang-tidy says the same thing
    // (readability-ambiguous-smartptr-reset-call), which is how the two lines below were found.
    m_audioSink = nullptr;
    m_synthesizer.reset();
    m_drumSynthesizer.reset();
    m_mixer = nullptr;
    m_isSinkRunning = false;
    m_outputDescription = "not opened yet";

    ensureAudioOutputIsOpen();

    if( !isAudioOutputAvailable() )
    {
        std::cerr << "Musichien: the audio device changed and no output could be opened. Playback stays silent until one appears.\n";
    }
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
    m_drumSynthesizer.emplace( m_audioFormat.sampleRate() );

    // The mixer is built from the REAL format, like the synthesisers: it is what the sink reads, and it is what lets
    // two sounds be heard at once.
    m_mixer = std::make_unique<AudioMixer>( m_audioFormat.sampleRate(), m_audioFormat.channelCount() );

    // A bounded sink buffer, because its size is the delay between a click being asked for and being heard.
    m_audioSink->setBufferSize( SINK_BUFFER_BYTES );

    m_outputDescription = std::format( "{} ({} Hz, {} channel(s), sample format {})",
                                       outputDevice.description().toStdString(),
                                       m_audioFormat.sampleRate(),
                                       m_audioFormat.channelCount(),
                                       static_cast<int>( m_audioFormat.sampleFormat() ) );

    std::cerr << "Musichien: audio output opened on " << m_outputDescription << "\n";
}

void QAudioNotePlayer::startSinkIfNeeded()
{
    if( ( m_audioSink == nullptr ) || ( m_mixer == nullptr ) || m_isSinkRunning )
    {
        return;
    }

    // The sink PULLS from the mixer: nothing is written, the sink asks for what it needs. Starting it on an already
    // running sink would restart the stream and cut the sound being played, hence the flag.
    m_audioSink->setVolume( SINK_VOLUME );
    m_audioSink->start( m_mixer.get() );

    m_isSinkRunning = true;
}

void QAudioNotePlayer::stopSinkWhenSilent()
{
    if( m_mixer == nullptr )
    {
        return;
    }

    // A single shot rather than a timer of our own: when nothing is left to play, the device is handed back. Leaving
    // an output open on a phone drains the battery, which the project has always refused.
    //
    // The mixer is the CONTEXT of the single shot, and that is deliberate: it is owned by this object, so if the
    // player is destroyed the pending call goes with it instead of touching a dangling pointer.
    QTimer::singleShot( 250, m_mixer.get(), [this]() {
        if( ( m_audioSink == nullptr ) || ( m_mixer == nullptr ) )
        {
            return;
        }

        if( !m_mixer->isPlaying() && !m_mixer->isMetronomeRunning() )
        {
            m_audioSink->stop();

            m_isSinkRunning = false;
        }
    } );
}

void QAudioNotePlayer::playSamples( std::vector<float> p_samples, float p_gain )
{
    ensureAudioOutputIsOpen();

    if( ( m_mixer == nullptr ) || p_samples.empty() )
    {
        return;
    }

    // A new note REPLACES the previous one: in an ear training exercise, two overlapping notes make the interval
    // impossible to identify. Percussion goes through mixSamples instead.
    m_mixer->clear();
    m_mixer->play( std::move( p_samples ), p_gain );

    startSinkIfNeeded();
    stopSinkWhenSilent();
}

void QAudioNotePlayer::mixSamples( std::vector<float> p_samples, float p_gain )
{
    ensureAudioOutputIsOpen();

    if( ( m_mixer == nullptr ) || p_samples.empty() )
    {
        return;
    }

    // ADDED to what is playing, never instead of it: the metronome click and a drum hit must be heard together.
    m_mixer->play( std::move( p_samples ), p_gain );

    startSinkIfNeeded();
    stopSinkWhenSilent();
}

void QAudioNotePlayer::stopAll()
{
    if( m_mixer != nullptr )
    {
        m_mixer->clear();
    }

    if( m_audioSink )
    {
        // stop() also releases the audio device, which matters on a phone: an output left open drains
        // the battery.
        m_audioSink->stop();
    }

    m_isSinkRunning = false;
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

void QAudioNotePlayer::useDroneInstruments( std::vector<domain::SampledInstrument> p_drones )
{
    m_drones = std::move( p_drones );

    m_droneIndex = 0;

    m_lastDroneNotes.clear();
}

std::size_t QAudioNotePlayer::timbreIndexFor( std::span<const domain::Note> p_notes )
{
    const std::size_t timbreCount = m_instruments.size() + m_waveforms.size();

    // The same question is the same NOTES, whatever order the melody played them in: a falling fifth and its
    // feedback chord share the same two notes. Comparing them as an ORDERED sequence would re-draw the instrument
    // between the melody and the chord - the guitar turning into a saxophone in front of the player.
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

void QAudioNotePlayer::playMelodyOverDrone( std::span<const domain::Note> p_melody,
                                            std::span<const domain::Note> p_drone,
                                            std::chrono::milliseconds p_noteDuration,
                                            std::chrono::milliseconds p_gap,
                                            domain::DroneFraming p_framing )
{
    ensureAudioOutputIsOpen();

    if( !m_synthesizer.has_value() )
    {
        return;
    }

    // Un bourdon ENREGISTRE, quand il y en a un.
    //
    // Le domaine a deja dit QUELLE quinte doit sonner : p_drone est fait des notes du bourdon, pas d'un numero de
    // timbre. L'echantillonneur les joue donc telles quelles, avec la transposition, la normalisation et le fondu de
    // queue de toutes les notes du jeu - le bourdon n'est pas un cas particulier.
    if( !m_drones.empty() )
    {
        // Le timbre est TIRE, et il reste le meme tant que le bourdon ne change pas : entendre le meme mode sur un
        // autre bourdon serait une autre question. C'est exactement la regle des instruments de melodie.
        //
        // Et le domaine peut DEMANDER de le garder malgre un changement de bourdon : c'est ce que fait une comparaison
        // de deux modes, ou le bourdon d'un vamp se deplace par nature. Roger l'a entendu : « il faudrait que ca utilise
        // les meme instruments, ca evite le bruit de la difference d'instrument ».
        const bool sameDrone = m_holdTimbre
                               || ( ( p_drone.size() == m_lastDroneNotes.size() )
                                    && std::is_permutation( p_drone.begin(), p_drone.end(), m_lastDroneNotes.begin() ) );

        // Consomme : la demande vaut pour UNE lecture, et la suivante retrouve sa liberte.
        m_holdTimbre = false;

        if( !sameDrone )
        {
            m_lastDroneNotes.assign( p_drone.begin(), p_drone.end() );

            std::uniform_int_distribution<std::size_t> distribution{ 0, m_drones.size() - 1 };

            m_droneIndex = distribution( m_instrumentRandomEngine );
        }

        const domain::SampledInstrument & drone = m_drones.at( m_droneIndex );

        // La duree du bourdon vient du DOMAINE, et non d'un calcul ecrit ici : c'est la seule facon de garantir que le
        // bourdon enregistre et celui de la synthese tiennent exactement le meme temps.
        const std::vector<float> droneSamples =
          drone.renderChord( p_drone,
                             domain::droneDurationFor( p_melody.size(), p_noteDuration, p_gap, p_framing ),
                             m_audioFormat.sampleRate(),
                             m_tuning );

        playSamples(
          m_synthesizer->mixMelodyOverDrone( p_melody, droneSamples, p_noteDuration, p_gap, m_tuning, p_framing ) );

        return;
    }

    // Le REPLI, et il n'est pas decoratif : un appareil dont les echantillons n'ont pas pu etre lus doit quand meme
    // entendre la question. La synthese fabrique alors son propre bourdon (Waveform::Organ).
    playSamples( m_synthesizer->renderMelodyOverDrone( p_melody, p_drone, p_noteDuration, p_gap, m_tuning, p_framing ) );
}

void QAudioNotePlayer::playPhraseOverDrone( std::span<const domain::Note> p_melody,
                                            std::span<const std::chrono::milliseconds> p_durations,
                                            std::span<const domain::Note> p_drone,
                                            std::chrono::milliseconds p_gap,
                                            domain::DroneFraming p_framing )
{
    // Exactement le meme chemin que playMelodyOverDrone, a une chose pres : les durees. Recopier la mecanique plutot que
    // d'en inventer une seconde est delibere - une phrase et une gamme doivent sonner du meme bourdon, au meme niveau,
    // decalees de la meme facon, sinon comparer l'une a l'autre serait comparer deux choses differentes.
    ensureAudioOutputIsOpen();

    if( !m_synthesizer.has_value() )
    {
        return;
    }

    if( !m_drones.empty() )
    {
        const bool sameDrone = ( p_drone.size() == m_lastDroneNotes.size() )
                               && std::is_permutation( p_drone.begin(), p_drone.end(), m_lastDroneNotes.begin() );

        if( !sameDrone )
        {
            m_lastDroneNotes.assign( p_drone.begin(), p_drone.end() );

            std::uniform_int_distribution<std::size_t> distribution{ 0, m_drones.size() - 1 };

            m_droneIndex = distribution( m_instrumentRandomEngine );
        }

        const domain::SampledInstrument & drone = m_drones.at( m_droneIndex );

        // La duree du bourdon est la SOMME des pas, silences compris : c'est la meme regle que pour une melodie
        // reguliere, et elle vit dans le domaine pour que les deux bourdons - celui de la synthese et celui des
        // echantillons - ne puissent pas diverger.
        const std::vector<float> droneSamples =
          drone.renderChord( p_drone,
                             domain::droneDurationFor( p_durations, p_gap, p_framing ),
                             m_audioFormat.sampleRate(),
                             m_tuning );

        playSamples(
          m_synthesizer->mixMelodyOverDrone( p_melody, droneSamples, p_durations, p_gap, m_tuning, p_framing ) );

        return;
    }

    playSamples( m_synthesizer->renderMelodyOverDrone( p_melody, p_drone, p_durations, p_gap, m_tuning, p_framing ) );
}

void QAudioNotePlayer::holdTimbre()
{
    m_holdTimbre = true;
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

    // MIXE : un clic de menu doit s'entendre par-dessus ce qui joue deja, pas le remplacer.
    mixSamples( std::move( samples ) );
}

void QAudioNotePlayer::playMetronomeClick( bool p_accented )
{
    ensureAudioOutputIsOpen();

    const std::vector<float> & click = p_accented ? m_accentedClick : m_plainClick;

    // Le bloc de bois d'abord : c'est la sonorite d'un metronome, et c'est ce qui remplace le timbre maigre des
    // premiers temps faibles.
    if( !click.empty() )
    {
        mixSamples( click, p_accented ? ACCENTED_CLICK_GAIN : PLAIN_CLICK_GAIN );

        return;
    }

    // Le repli : la synthese, avec l'ancien timbre.
    if( !m_synthesizer.has_value() )
    {
        return;
    }

    const domain::Note note{ p_accented ? 88 : 72 };

    std::vector<float> samples =
      m_synthesizer->renderNote( note, std::chrono::milliseconds{ 60 } );

    for( float & sample : samples )
    {
        sample *= p_accented ? FALLBACK_ACCENTED_CLICK_GAIN : FALLBACK_PLAIN_CLICK_GAIN;
    }

    // MIXE et non remplace : le metronome doit s'entendre EN MEME TEMPS que la batterie. C'etait le bug - le clic
    // tuait le son de batterie en cours, et reciproquement.
    mixSamples( std::move( samples ) );
}

void QAudioNotePlayer::useDrumSamples( std::array<std::vector<float>, domain::DRUM_COUNT> p_samples )
{
    m_drumSamples = std::move( p_samples );
}

void QAudioNotePlayer::playDrumAt( domain::Drum p_drum, double p_positionMs )
{
    ensureAudioOutputIsOpen();

    if( m_mixer == nullptr )
    {
        return;
    }

    const auto index = static_cast<std::size_t>( p_drum );

    std::vector<float> samples;

    if( ( index < m_drumSamples.size() ) && !m_drumSamples.at( index ).empty() )
    {
        samples = m_drumSamples.at( index );
    }
    else if( m_drumSynthesizer.has_value() )
    {
        samples = m_drumSynthesizer->renderDrum( p_drum );
    }

    if( samples.empty() )
    {
        return;
    }

    // La position demandee se compte depuis le PREMIER TEMPS du metronome, exactement comme le temps que l'oreille
    // entend. L'ecart entre les deux est donc un DELAI, que l'on traduit en echantillons : c'est ce qui pose une
    // syncope ENTRE deux temps, et non « a peu pres ».
    constexpr double MILLISECONDS_PER_SECOND = 1000.0;

    const double delayMs = p_positionMs - m_mixer->metronomeElapsedMs();
    const double framesPerMs = static_cast<double>( m_audioFormat.sampleRate() ) / MILLISECONDS_PER_SECOND;

    const std::int64_t startFrame =
      m_mixer->framesWritten() + static_cast<std::int64_t>( std::llround( delayMs * framesPerMs ) );

    m_mixer->playAt( std::move( samples ), startFrame, DRUM_GAIN );

    startSinkIfNeeded();
}

void QAudioNotePlayer::startMetronome( double p_bpm, int p_beatsPerBar )
{
    ensureAudioOutputIsOpen();

    if( ( m_mixer == nullptr ) || ( m_audioSink == nullptr ) )
    {
        return;
    }

    // Les clics EFFECTIFS, donnes une seule fois : le mixer les rejouera des milliers de fois, et c'est desormais LUI
    // qui les posera, a l'echantillon pres.
    m_mixer->setMetronomeClicks( effectiveClick( true ), effectiveClick( false ) );

    // Le tampon de sortie est le decalage entre ce qui est ECRIT et ce qui est ENTENDU. Sans cette correction, une
    // frappe parfaitement juste serait jugee en avance de tout le tampon, et le joueur apprendrait a jouer en retard.
    const auto channelCount = static_cast<std::int64_t>( std::max( 1, m_audioFormat.channelCount() ) );
    const auto bytesPerFrame = static_cast<std::int64_t>( sizeof( float ) ) * channelCount;

    m_mixer->setOutputLatencyFrames( static_cast<std::int64_t>( SINK_BUFFER_BYTES ) / std::max<std::int64_t>( 1, bytesPerFrame ) );

    m_mixer->startMetronome( p_bpm, p_beatsPerBar );

    startSinkIfNeeded();
}

void QAudioNotePlayer::stopMetronome()
{
    if( m_mixer != nullptr )
    {
        m_mixer->stopMetronome();
    }

    // Et le peripherique peut rendre l'appareil des qu'il n'y a plus rien a jouer : sur un telephone, un flux ouvert
    // entre deux exercices vide la batterie.
    stopSinkWhenSilent();
}

std::int64_t QAudioNotePlayer::metronomeBeatIndex() const
{
    return ( m_mixer != nullptr ) ? m_mixer->metronomeBeatIndex() : 0;
}

bool QAudioNotePlayer::isMetronomeBeatAccented() const
{
    return ( m_mixer != nullptr ) && m_mixer->isMetronomeBeatAccented();
}

double QAudioNotePlayer::metronomeElapsedMs() const
{
    return ( m_mixer != nullptr ) ? m_mixer->metronomeElapsedMs() : 0.0;
}

std::vector<float> QAudioNotePlayer::effectiveClick( bool p_accented )
{
    const std::vector<float> & sampled = p_accented ? m_accentedClick : m_plainClick;

    if( !sampled.empty() )
    {
        return sampled;
    }

    // Le repli : la synthese, avec l'ancien timbre. Un metronome muet serait pire qu'un metronome moins beau.
    if( !m_synthesizer.has_value() )
    {
        return {};
    }

    const domain::Note note{ p_accented ? 88 : 72 };

    std::vector<float> samples = m_synthesizer->renderNote( note, std::chrono::milliseconds{ 60 } );

    const float gain = p_accented ? FALLBACK_ACCENTED_CLICK_GAIN : FALLBACK_PLAIN_CLICK_GAIN;

    for( float & sample : samples )
    {
        sample *= gain;
    }

    return samples;
}

void QAudioNotePlayer::useMetronomeClicks( std::vector<float> p_accented, std::vector<float> p_plain )
{
    m_accentedClick = std::move( p_accented );
    m_plainClick = std::move( p_plain );
}

void QAudioNotePlayer::playDrum( domain::Drum p_drum )
{
    ensureAudioOutputIsOpen();

    const auto index = static_cast<std::size_t>( p_drum );

    // La vraie peau d'abord : c'est ce qui rend une batterie jouable a l'oreille.
    if( ( index < m_drumSamples.size() ) && !m_drumSamples.at( index ).empty() )
    {
        // MIXE, comme le metronome : un roulement de batterie, c'est des sons qui se chevauchent.
        mixSamples( m_drumSamples.at( index ), DRUM_GAIN );

        return;
    }

    // Le repli, quand un echantillon manque : la synthese. Un son moins beau vaut mieux que pas de son.
    if( m_drumSynthesizer.has_value() )
    {
        mixSamples( m_drumSynthesizer->renderDrum( p_drum ), DRUM_GAIN );
    }
}

void QAudioNotePlayer::playGreeting()
{
    ensureAudioOutputIsOpen();

    if( !m_synthesizer.has_value() )
    {
        return;
    }

    // L'arpège : montant, sur des degrés ouverts - « à la Zelda ».
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
