#include "infrastructure/audio/QAudioNotePlayer.h"

#include "domain/music/Note.h"

#include <QAudioDevice>
#include <QDebug>
#include <QMediaDevices>
#include <QTimer>

#include <algorithm>
#include <array>
#include <format>
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

// LE TAMPON DE SORTIE, EN OCTETS, ET CE N'EST PLUS QU'UN REPLI.
//
// Il a longtemps ete la valeur IMPOSEE - seize kilooctets, une quarantaine de millisecondes - parce que sa taille est la
// latence entre un clic demande et un clic entendu. Ce raisonnement etait juste pour le haut-parleur du telephone, et
// FAUX ailleurs : la bonne taille depend de la ROUTE, et Android la connait (voir ensureAudioOutputIsOpen). Il ne sert
// donc plus que le jour ou la plateforme ne repond rien, ce qui n'arrive pas sur les appareils connus.
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

// Le silence entre la gamme et l'accord d'un APERCU d'instrument. Assez long pour que l'oreille quitte la gamme, assez
// court pour que l'ecoute reste UNE ecoute : on choisit un timbre, on ne compare pas deux morceaux.
constexpr std::chrono::milliseconds PREVIEW_SILENCE{ 400 };

// L'accord d'un apercu est tenu cinq fois la duree d'une note de gamme : la duree d'une petite phrase. C'est une couleur
// qu'on ecoute - il faut le temps de l'entendre battre - et non un pas qu'on enchaine. Le corps par defaut du port tient
// le meme temps, et il doit le tenir : un adaptateur qui n'a qu'un timbre fait entendre la meme chose que celui qui en a
// onze, simplement sans la difference.
constexpr std::int32_t PREVIEW_CHORD_DURATION_MULTIPLIER = 5;

// LE BRUITAGE D'UN GAIN QUI S'AFFICHE : le tic du compte, et la fanfare qui le conclut.
//
// Roger les a demandes comme des BRUITAGES, et c'est le mot juste : « c'est juste un bruitage pour rendre le jeu moins
// austere, et faire appel a des biais cognitifs d'addiction, comme dans les machines a sous ». Rien de musical la-dedans
// - et c'est pour cela que la forme d'onde est CARREE : ses harmoniques impaires sonnent creux et brillant, exactement
// ce qu'un jeu video fait sonner depuis quarante ans, la ou une corde frappee sonnerait un instrument.
//
// LE TIC est TRES court - 45 ms - parce qu'un bruitage plus long deviendrait une note, et qu'une note, elle, se
// reconnait. Et son HAUTEUR MONTE avec le chiffre : le tic grimpe, et l'oreille entend le gain grandir avant de le lire.
constexpr std::int32_t SCORE_TICK_LOWEST_MIDI = 79;
constexpr std::int32_t SCORE_TICK_HIGHEST_MIDI = 91;
constexpr std::chrono::milliseconds SCORE_TICK_DURATION{ 45 };

// Discret : le tic sonne des dizaines de fois en une seconde, donc fort il deviendrait vite insupportable. C'est le
// volume d'un compteur qui tourne, pas celui d'une reponse.
constexpr float SCORE_TICK_GAIN = 0.16F;

// LA FANFARE DE VICTOIRE : un accord parfait MAJEUR, monte.
//
// Majeur, et c'est toute la difference avec l'aperge de l'accueil : celui-la est OUVERT - do, sol, do, sans tierce - pour
// ne rien affirmer et laisser flotter. Ici la TIERCE MAJEURE dit « gagne », et c'est exactement ce qu'on veut dire.
constexpr std::chrono::milliseconds VICTORY_NOTE_DURATION{ 150 };
constexpr std::chrono::milliseconds VICTORY_GAP{ 25 };
constexpr float VICTORY_GAIN = 0.32F;

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
    closeAudioOutput();

    ensureAudioOutputIsOpen();

    if( !isAudioOutputAvailable() )
    {
        qWarning().noquote() << "Musichien: the audio device changed and no output could be opened. Playback stays silent "
                                "until one appears.";
    }
}

void QAudioNotePlayer::closeAudioOutput()
{
    // TOUT ce qui sonnait s'arrete ici : le mix est vide, donc rien ne sera repris a la reouverture. Une note coupee
    // par la mise en veille ne doit pas ressortir plus tard, hors de son exercice.
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
        qWarning().noquote() << "Musichien: no audio output device: the notes will stay silent.";
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
        qWarning().noquote() << "Musichien: the audio device does not accept 32 bit float samples (it wants"
                             << static_cast<int>( m_audioFormat.sampleFormat() ) << "). Playback stays silent.";

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

    // LE FILET SOUS LA CORDE, et il manquait.
    //
    // Une sortie qui s'arrete TOUTE SEULE alors qu'il restait quelque chose a jouer a ECHOUE : Android a refuse
    // l'ouverture, le plafond de huit flux est atteint, la liaison a lache. Le QAudioSink reste alors en place, muet, et
    // TOUT ce qui suit reste muet - c'est le « d'un coup j'ai perdu le son » de Roger. Un silence DEFINITIF pour une
    // panne PASSAGERE est le pire des deux mondes.
    //
    // On la jette donc, et la note suivante en reconstruit une neuve : la panne coute UN son au lieu de tous ceux qui
    // suivent.
    //
    // ICI, et pas plus haut : ce branchement a d'abord ete pose juste apres la creation du FLUX, ou il ne pouvait pas
    // fonctionner - le CONTEXTE de la connexion est le mixeur, et le mixeur n'existait pas encore. Qt l'a dit tout seul,
    // sur l'appareil : « QObject::connect(QAudioSink, Unknown): invalid nullptr parameter ». Un filet qui ne se branche
    // pas est pire qu'aucun filet : il rassure.
    //
    // Le contexte est le mixeur, et non `this` : cet adaptateur n'est pas un QObject, et le mixeur lui appartient - il
    // vit donc exactement aussi longtemps. La reparation est DIFFEREE d'un tour de boucle, parce qu'elle DETRUIT l'objet
    // meme qui est en train d'emettre ce signal.
    QObject::connect( m_audioSink.get(),
                      &QAudioSink::stateChanged,
                      m_mixer.get(),
                      [this]( QtAudio::State p_state ) {
                          if( ( p_state != QtAudio::StoppedState ) || ( m_mixer == nullptr ) || !m_mixer->isPlaying() )
                          {
                              return;
                          }

                          qWarning() << "Musichien: the audio output stopped on its own while sound was pending; "
                                        "rebuilding it for the next note.";

                          QTimer::singleShot( 0, m_mixer.get(), [this]() { closeAudioOutput(); } );
                      } );

    // LE TAMPON DE SORTIE N'EST PLUS IMPOSE, ET C'EST UNE REPARATION.
    //
    // Roger : « quand je branche mes ecouteurs bluetooth, ca saccade, ca gresille ». Nous demandions seize kilooctets,
    // soit une quarantaine de millisecondes : un nombre juste pour le haut-parleur du telephone, et FAUX pour un
    // casque Bluetooth.
    //
    // Une liaison Bluetooth a son propre tampon, et Android le sait - c'est meme pour cela que
    // 'AudioTrack.getMinBufferSize' depend de la ROUTE. Mais Qt n'applique ce minimum que sur un appareil SANS faible
    // latence (voir QAndroidAudioSink::start) : sur un Pixel, il garde notre chiffre tel quel. Un flux trop maigre
    // pour la liaison meurt de faim entre deux remplissages, et cela s'entend exactement comme Roger le decrit.
    //
    // On ne demande donc plus RIEN : la plateforme choisit son minimum pour la route du moment, et c'est elle qui
    // sait. Le tampon REEL est relu dans la foulee, parce que c'est LUI qui dit la latence de sortie.
    const qsizetype requestedBufferBytes = m_audioSink->bufferSize();

    m_outputBufferBytes = ( requestedBufferBytes > 0 ) ? requestedBufferBytes : SINK_BUFFER_BYTES;

    const auto bytesPerFrame =
      static_cast<qsizetype>( sizeof( float ) ) * std::max( 1, m_audioFormat.channelCount() );

    const auto bufferMilliseconds =
      ( bytesPerFrame > 0 )
        ? static_cast<int>( ( m_outputBufferBytes / bytesPerFrame ) * 1000 / std::max( 1, m_audioFormat.sampleRate() ) )
        : 0;

    m_outputDescription = std::format( "{} ({} Hz, {} channel(s), sample format {}, buffer {} ms)",
                                       outputDevice.description().toStdString(),
                                       m_audioFormat.sampleRate(),
                                       m_audioFormat.channelCount(),
                                       static_cast<int>( m_audioFormat.sampleFormat() ),
                                       bufferMilliseconds );

    // qInfo et NON std::cerr : sur Android, la sortie d'erreur n'arrive PAS dans logcat - c'est verifie sur l'appareil,
    // et c'est ecrit dans le Vault. Or c'est precisement LA ligne qu'on veut pouvoir lire depuis le telephone le jour
    // ou un son se comporte mal : quelle sortie, quel taux, et surtout quel tampon.
    qInfo().noquote() << QString::fromStdString( "Musichien: audio output opened on " + m_outputDescription );
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
    //
    // -------------------------------------------------------------------------------------------------------------
    // UNE MINUTE, ET NON UN QUART DE SECONDE, et c'est un SON PERDU qui l'a appris.
    //
    // Roger : « tout allait bien puis d'un coup j'ai perdu le son ». La cause est mesuree, et elle est sans appel :
    //
    //     E AAudioService: openStream(): exceeded max streams per process 8 >= 8
    //     AAudioStreamBuilder_openStream() returns -896 = AAUDIO_ERROR_INTERNAL
    //
    // ANDROID NE LAISSE PAS OUVRIR PLUS DE HUIT FLUX AUDIO PAR PROCESSUS. Or chaque `start()` d'un QAudioSink ouvre un
    // flux AAudio, et chaque `stop()` le ferme - mais la FERMETURE EST ASYNCHRONE. Avec un quart de seconde de patience,
    // chaque note rendait l'appareil puis le reprenait : 93 flux ouverts en cinq minutes, une poignee encore en train de
    // se fermer a chaque instant, et au neuvieme tout echoue. Une fois le plafond atteint, plus rien ne passe : le son
    // ne revient jamais, et c'est exactement ce que Roger a vecu.
    //
    // Le remede est de ne plus JOUER AU YO-YO. Une minute de patience laisse le flux ouvert pendant toute une seance -
    // les questions s'enchainent bien plus vite que cela - donc un seul flux est ouvert, et le plafond n'est jamais
    // approche. L'appareil est rendu tout de meme : apres une vraie minute de silence, et immediatement a la mise en
    // arriere-plan (voir closeAudioOutput), qui est le vrai cas de la batterie.
    QTimer::singleShot( 60000, m_mixer.get(), [this]() {
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

void QAudioNotePlayer::playInstrumentPreview( std::span<const domain::Note> p_scale,
                                              std::span<const domain::Note> p_chord,
                                              std::size_t p_instrumentIndex,
                                              std::chrono::milliseconds p_noteDuration,
                                              std::chrono::milliseconds p_gap )
{
    ensureAudioOutputIsOpen();

    if( !m_synthesizer.has_value() )
    {
        return;
    }

    // Le timbre est IMPOSE, et rien d'autre n'est touche : ni le tirage, ni le timbre retenu pour la session. Ecouter ce
    // qu'un instrument donnerait ne doit pas decider a la place du joueur de ce qu'il entendra ensuite.
    const auto sampleRate = static_cast<std::size_t>( std::max( 1, m_audioFormat.sampleRate() ) );
    const auto silenceSampleCount = static_cast<std::size_t>( sampleRate * PREVIEW_SILENCE.count() / 1000 );

    std::vector<float> preview = renderMelodyWithIndex( p_scale, p_noteDuration, p_gap, p_instrumentIndex );

    preview.insert( preview.end(), silenceSampleCount, 0.0F );

    const std::vector<float> chord =
      renderChordWithIndex( p_chord, p_noteDuration * PREVIEW_CHORD_DURATION_MULTIPLIER, p_instrumentIndex );

    preview.insert( preview.end(), chord.begin(), chord.end() );

    playSamples( preview );
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

void QAudioNotePlayer::useEnabledInstruments( std::vector<bool> p_enabled )
{
    m_enabledTimbre = std::move( p_enabled );

    // Le timbre retenu pour la session peut tres bien avoir ete decoche entre-temps : le tirage suivant en choisira un
    // autre, et c'est tout ce qu'il y a a faire. Rien n'est reinitialise ici - c'est le TIRAGE qui verifie.
}

bool QAudioNotePlayer::isTimbreEnabled( std::size_t p_timbreIndex ) const
{
    if( p_timbreIndex >= m_enabledTimbre.size() )
    {
        return true;
    }

    return m_enabledTimbre.at( p_timbreIndex );
}

const domain::SampledInstrument * QAudioNotePlayer::instrumentAt( std::size_t p_timbreIndex ) const
{
    if( p_timbreIndex >= m_instruments.size() )
    {
        return nullptr;
    }

    const domain::SampledInstrument & instrument = m_instruments.at( p_timbreIndex );

    // Vide : la ressource n'a pas pu etre lue. Le rang reste le sien - c'est ce qui garde les index du domaine justes -
    // mais il n'y a rien a jouer, et la synthese s'en charge.
    return instrument.isEmpty() ? nullptr : &instrument;
}

std::size_t QAudioNotePlayer::timbreIndexFor( std::span<const domain::Note> p_notes )
{
    // Le joueur a pu DEMANDER de garder le timbre pour la lecture qui suit : c'est ce que fait une comparaison de deux
    // modes, dont les deux passages n'ont pas les memes notes - et un vamp deplace meme le bourdon. La demande vaut pour
    // UNE lecture, et elle est consommee ici, comme un jeton.
    //
    // CE BLOC A MANQUE, et c'est le bug du clair-obscur. holdTimbre() posait son drapeau pour le BOURDON seul, et
    // timbreIndexFor ne le lisait pas : l'adaptateur re-tirait donc un timbre, et les deux modes d'une meme question
    // sonnaient sur deux instruments differents - exactement ce que la comparaison ne doit pas faire entendre.
    if( m_holdTimbre )
    {
        m_holdTimbre = false;

        if( isTimbreEnabled( m_instrumentIndex ) )
        {
            return m_instrumentIndex;
        }
    }

    // The same question is the same NOTES, whatever order the melody played them in: a falling fifth and its
    // feedback chord share the same two notes. Comparing them as an ORDERED sequence would re-draw the instrument
    // between the melody and the chord - the guitar turning into a saxophone in front of the player.
    const bool sameQuestion =
      !m_lastPlayedNotes.empty() && ( p_notes.size() == m_lastPlayedNotes.size() )
      && std::is_permutation( p_notes.begin(), p_notes.end(), m_lastPlayedNotes.begin() );

    // Les notes entendues sont retenues dans TOUS les cas : c'est ce qui permet a la question suivante de savoir si elle a
    // change, y compris quand le timbre, lui, ne change plus.
    m_lastPlayedNotes.assign( p_notes.begin(), p_notes.end() );

    // ET LE TIMBRE DE LA SESSION NE BOUGE PLUS : voir beginTimbreForSession. Une nouvelle question ne retire donc plus de
    // timbre, et l'oreille ne compare plus que ce qu'on lui demande - l'intervalle, jamais l'instrument.
    //
    // Sauf s'il vient d'etre DECOCHE : garder un son que le joueur vient de refuser serait la seule facon d'entendre
    // encore ce qu'il ne veut plus. Le tirage, plus bas, en choisira un autre - et c'est la seule chose a faire ici.
    if( ( sameQuestion || m_timbreIsHeldForSession ) && isTimbreEnabled( m_instrumentIndex ) )
    {
        return m_instrumentIndex;
    }

    const std::size_t timbreCount = m_instruments.size() + m_waveforms.size();

    if( timbreCount == 0 )
    {
        return m_instrumentIndex;
    }

    // Le tirage porte sur les timbres ACCEPTES, et il tire un RANG parmi eux plutot que de batir une liste : une
    // allocation a chaque question serait payee a chaque question, pour un resultat identique.
    std::size_t allowedCount = 0;

    for( std::size_t index = 0; index < timbreCount; ++index )
    {
        if( isTimbreEnabled( index ) )
        {
            ++allowedCount;
        }
    }

    // Aucun timbre accepte : l'interface l'interdit - la derniere case ne peut pas s'eteindre - mais un fichier de
    // reglages peut encore le dire. Plutot que de tirer parmi ce que le joueur a refuse, on continue ce qu'on jouait.
    if( allowedCount == 0 )
    {
        return m_instrumentIndex;
    }

    std::uniform_int_distribution<std::size_t> distribution{ 0, allowedCount - 1 };

    std::size_t wantedRank = distribution( m_instrumentRandomEngine );

    for( std::size_t index = 0; index < timbreCount; ++index )
    {
        if( !isTimbreEnabled( index ) )
        {
            continue;
        }

        if( wantedRank == 0 )
        {
            m_instrumentIndex = index;

            break;
        }

        --wantedRank;
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

    if( const domain::SampledInstrument * instrument = instrumentAt( index ) )
    {
        return instrument->renderNote( p_note, p_duration, m_audioFormat.sampleRate(), m_tuning );
    }

    if( index >= m_instruments.size() + m_waveforms.size() )
    {
        return m_synthesizer->renderNote( p_note, p_duration, m_tuning );
    }

    return m_synthesizer->renderWaveNote( p_note, m_waveforms.at( index - m_instruments.size() ), p_duration, m_tuning );
}

std::vector<float> QAudioNotePlayer::renderChordFor( std::span<const domain::Note> p_notes,
                                                     std::chrono::milliseconds p_duration )
{
    if( m_instruments.empty() && m_waveforms.empty() )
    {
        return m_synthesizer->renderChord( p_notes, p_duration, m_tuning );
    }

    return renderChordWithIndex( p_notes, p_duration, timbreIndexFor( p_notes ) );
}

std::vector<float> QAudioNotePlayer::renderChordWithIndex( std::span<const domain::Note> p_notes,
                                                           std::chrono::milliseconds p_duration,
                                                           std::size_t p_timbreIndex )
{
    if( const domain::SampledInstrument * instrument = instrumentAt( p_timbreIndex ) )
    {
        return instrument->renderChord( p_notes, p_duration, m_audioFormat.sampleRate(), m_tuning );
    }

    // Un index au-dela des deux listes retombe sur la synthese, comme un appareil sans echantillons : mieux vaut un
    // accord synthetise qu'un apercu muet.
    if( p_timbreIndex >= m_instruments.size() + m_waveforms.size() )
    {
        return m_synthesizer->renderChord( p_notes, p_duration, m_tuning );
    }

    return m_synthesizer->renderWaveChord( p_notes,
                                           m_waveforms.at( p_timbreIndex - m_instruments.size() ),
                                           p_duration,
                                           m_tuning );
}

std::vector<float> QAudioNotePlayer::renderMelodyFor( std::span<const domain::Note> p_notes,
                                                      std::chrono::milliseconds p_noteDuration,
                                                      std::chrono::milliseconds p_gap )
{
    if( m_instruments.empty() && m_waveforms.empty() )
    {
        return m_synthesizer->renderMelody( p_notes, p_noteDuration, p_gap, m_tuning );
    }

    return renderMelodyWithIndex( p_notes, p_noteDuration, p_gap, timbreIndexFor( p_notes ) );
}

std::vector<float> QAudioNotePlayer::renderMelodyWithIndex( std::span<const domain::Note> p_notes,
                                                            std::chrono::milliseconds p_noteDuration,
                                                            std::chrono::milliseconds p_gap,
                                                            std::size_t p_timbreIndex )
{
    if( const domain::SampledInstrument * instrument = instrumentAt( p_timbreIndex ) )
    {
        return instrument->renderMelody( p_notes, p_noteDuration, p_gap, m_audioFormat.sampleRate(), m_tuning );
    }

    // Un index au-dela des deux listes retombe sur la synthese : mieux vaut une gamme synthetisee qu'un apercu muet.
    if( p_timbreIndex >= m_instruments.size() + m_waveforms.size() )
    {
        return m_synthesizer->renderMelody( p_notes, p_noteDuration, p_gap, m_tuning );
    }

    return m_synthesizer->renderWaveMelody( p_notes,
                                            m_waveforms.at( p_timbreIndex - m_instruments.size() ),
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

        // LA MELODIE PAR UN INSTRUMENT, quand il y en a un.
        //
        // C'est la correction du 01/10/2026 : le bourdon etait deja ENREGISTRE - un chœur, des cordes - pendant que la
        // melodie restait synthetisee. Deux matieres qui ne s'accordent pas, et Roger l'a entendu tout de suite : « les voix
        // pour les modes, je trouve ca un peu bizarre ».
        //
        // Le REPLI reste la synthese, et il n'est pas decoratif : un appareil dont les echantillons manquent doit quand
        // meme entendre la question.
        const std::size_t timbreIndex = timbreIndexFor( p_melody );

        if( const domain::SampledInstrument * instrument = instrumentAt( timbreIndex ) )
        {
            const std::vector<float> melodySamples =
              instrument->renderMelody( p_melody, p_noteDuration, p_gap, m_audioFormat.sampleRate(), m_tuning );

            playSamples( m_synthesizer->mixRenderedMelodyOverDrone( melodySamples, droneSamples, p_framing ) );

            return;
        }

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

        // LA PHRASE PAR UN INSTRUMENT, comme la gamme : voir playMelodyOverDrone. C'est ce qui fait qu'un mode s'entend
        // d'une seule matiere, que la question joue une phrase ou une gamme.
        const std::size_t timbreIndex = timbreIndexFor( p_melody );

        if( const domain::SampledInstrument * instrument = instrumentAt( timbreIndex ) )
        {
            const std::vector<float> melodySamples =
              instrument->renderMelody( p_melody, p_durations, p_gap, m_audioFormat.sampleRate(), m_tuning );

            playSamples( m_synthesizer->mixRenderedMelodyOverDrone( melodySamples, droneSamples, p_framing ) );

            return;
        }

        playSamples(
          m_synthesizer->mixMelodyOverDrone( p_melody, droneSamples, p_durations, p_gap, m_tuning, p_framing ) );

        return;
    }

    playSamples( m_synthesizer->renderMelodyOverDrone( p_melody, p_drone, p_durations, p_gap, m_tuning, p_framing ) );
}

void QAudioNotePlayer::holdTimbre()
{
    // Le timbre de la lecture QUI SUIT sera celui-ci, et la demande est consommee par elle.
    //
    // Le drapeau de SESSION n'est PLUS touche, et c'est le fond du bug du clair-obscur : il l'etait, donc le second mode
    // d'une comparaison rendait la session au tirage - et les deux modes sonnaient sur deux instruments, precisement la
    // ou le meme timbre est ce qui rend la comparaison possible.
    m_holdTimbre = true;
}

void QAudioNotePlayer::beginTimbreForSession()
{
    // UN timbre pour toute la session, et un nouveau a chaque session : le tirage a lieu ICI, une fois, puis
    // timbreIndexFor ne le touche plus.
    //
    // Roger a entendu le probleme avant de le nommer : « les instruments parfois ca rend bizarre dans certains
    // intervalles ». Le timbre changeait a chaque question, donc une seconde mineure et une quinte n'etaient pas jouees
    // par le meme instrument - et l'oreille comparait deux choses au lieu d'une.
    m_holdTimbre = false;
    m_timbreIsHeldForSession = false;
    m_lastPlayedNotes.clear();

    const std::size_t timbreCount = m_instruments.size() + m_waveforms.size();

    if( timbreCount == 0 )
    {
        // Rien a choisir : la synthese jouera, comme partout ou un echantillon manque.
        return;
    }

    std::uniform_int_distribution<std::size_t> distribution{ 0, timbreCount - 1 };

    m_instrumentIndex = distribution( m_instrumentRandomEngine );

    // Et le timbre est TENU : c'est la seule chose qui distingue ce tirage des autres.
    m_timbreIsHeldForSession = true;
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

    m_mixer->setOutputLatencyFrames( m_outputBufferBytes / std::max<std::int64_t>( 1, bytesPerFrame ) );

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

void QAudioNotePlayer::useDogBarks( std::vector<std::vector<float>> p_samples )
{
    m_dogBarks = std::move( p_samples );
}

void QAudioNotePlayer::playDogBark()
{
    ensureAudioOutputIsOpen();

    // Un aboiement s'entend, et il doit s'entendre : c'est le chien qui annonce qu'il a quelque chose a dire. Il reste
    // pourtant SOUS le niveau d'une note, parce qu'il ne fait pas partie de la musique.
    constexpr float DOG_BARK_GAIN = 0.45F;

    // Les variantes TOURNENT : l'index avance a chaque aboiement, et repart au debut quand il a fait le tour. Roger
    // voulait « trois autres qui tourneraient » - et une rotation vaut mieux qu'un tirage au hasard, qui peut tomber
    // deux fois de suite sur le meme wouf et donner l'impression qu'il n'y en a qu'un.
    for( std::size_t attempt = 0; attempt < m_dogBarks.size(); ++attempt )
    {
        const std::size_t index = m_nextDogBark % m_dogBarks.size();
        ++m_nextDogBark;

        // Un fichier qui n'a pas ete lu ne compte pas comme une phrase : on passe au suivant.
        if( !m_dogBarks[index].empty() )
        {
            mixSamples( m_dogBarks[index], DOG_BARK_GAIN );

            return;
        }
    }

    // Pas un seul aboiement charge : le clic de menu prend sa place. Roger avait donne les deux solutions dans la MEME
    // phrase - « un son doux et tres court de chien... ou sinon, juste le meme petit son que tu avais sur les boutons »
    // - donc tomber sur l'une quand l'autre manque est exactement ce qu'il a demande.
    playTapCue();
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

void QAudioNotePlayer::playScoreTick( int p_progressPercent )
{
    ensureAudioOutputIsOpen();

    if( !m_synthesizer.has_value() )
    {
        return;
    }

    // OU EN EST LE COMPTE, ramene dans les bornes : c'est l'ecran qui l'annonce, et un ecran ne doit pas pouvoir faire
    // sortir de la gamme de hauteurs prevue.
    const int progress = std::clamp( p_progressPercent, 0, 100 );

    const std::int32_t midiNumber = SCORE_TICK_LOWEST_MIDI
                                    + ( ( SCORE_TICK_HIGHEST_MIDI - SCORE_TICK_LOWEST_MIDI ) * progress / 100 );

    std::vector<float> samples = m_synthesizer->renderWaveNote( domain::Note{ midiNumber },
                                                                domain::Waveform::Square,
                                                                SCORE_TICK_DURATION );

    for( float & sample : samples )
    {
        sample *= SCORE_TICK_GAIN;
    }

    // AJOUTE, et non substitué : les tics se suivent a quelques dizaines de millisecondes, et le suivant ne doit pas
    // COUPER le precedent - une machine a sous ne s'interrompt pas entre deux crans.
    mixSamples( std::move( samples ) );
}

void QAudioNotePlayer::playVictoryFanfare()
{
    ensureAudioOutputIsOpen();

    if( !m_synthesizer.has_value() )
    {
        return;
    }

    // DO, MI, SOL, DO : l'accord parfait majeur, monte. La tierce est la tout ce qui compte - sans elle, l'aperge serait
    // ouvert, et un accord ouvert ne dit pas qu'on a gagne.
    const std::array<domain::Note, 4> fanfare{ domain::Note{ 72 }, domain::Note{ 76 }, domain::Note{ 79 }, domain::Note{ 84 } };

    std::vector<float> samples = m_synthesizer->renderWaveMelody( fanfare,
                                                                  domain::Waveform::Square,
                                                                  VICTORY_NOTE_DURATION,
                                                                  VICTORY_GAP );

    for( float & sample : samples )
    {
        sample *= VICTORY_GAIN;
    }

    // AJOUTE aussi : la fanfare arrive sur le dernier tic, et les deux doivent s'entendre ensemble - le tic comme la
    // virgule, la fanfare comme la phrase.
    mixSamples( std::move( samples ) );
}

}    // namespace musichien::infrastructure
