#pragma once

// =====================================================================================================================
// Musichien - QAudioNotePlayer
//
// Plays notes through the audio output of the operating system.
//
// This is an ADAPTER: it implements the port declared by the domain, and it knows everything the
// domain must ignore - QAudioSink, sample formats, buffers, devices, failure to open a sound card.
//
// The dependency arrow still points towards the domain:
//
//     infrastructure  ---implements--->  domain::NotePlayer
//
// Swapping QAudioSink for Oboe on Android will therefore only touch this file.
// See docs/ARCHITECTURE.md.
// =====================================================================================================================

#include "domain/audio/NotePlayer.h"
#include "domain/audio/SampledInstrument.h"
#include "domain/audio/ToneSynthesizer.h"
#include "infrastructure/audio/AudioMixer.h"

#include <QAudioFormat>
#include <QAudioSink>
#include <QIODevice>

#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <vector>

namespace musichien::infrastructure
{

class QAudioNotePlayer final : public domain::NotePlayer
{
public:
    QAudioNotePlayer();
    ~QAudioNotePlayer() override;

    QAudioNotePlayer( const QAudioNotePlayer & ) = delete;
    QAudioNotePlayer & operator=( const QAudioNotePlayer & ) = delete;
    QAudioNotePlayer( QAudioNotePlayer && ) = delete;
    QAudioNotePlayer & operator=( QAudioNotePlayer && ) = delete;

    void playNote( const domain::Note & p_note ) override;
    void playMelody( std::span<const domain::Note> p_notes, std::chrono::milliseconds p_gap ) override;
    void playChord( std::span<const domain::Note> p_notes ) override;

    // The sustained chord: the same notes, held for the given duration, so the beating between them can be counted.
    void playChordFor( std::span<const domain::Note> p_notes, std::chrono::milliseconds p_duration ) override;

    // Ce que le joueur a joue, puis la reponse - dans UN SEUL rendu, voir le port pour la raison.
    void playChordThenChord( std::span<const domain::Note> p_first,
                             std::span<const domain::Note> p_second,
                             std::chrono::milliseconds p_gap ) override;

    // Ou en est le son, lu sur le PUITS : voir le port pour la raison. Zero quand le puits a ete recree, ce qui dit
    // « je ne sais pas » plutot que de mentir sur une position.
    [[nodiscard]] std::chrono::milliseconds playedMilliseconds() const override;

    // Fait ENTENDRE un instrument : la gamme, un silence, puis l'accord, tous deux avec CE timbre et aucun autre.
    //
    // TOUT TIENT DANS UN SEUL TAMPON, et c'est la seule facon de faire : deux appels a playSamples se SUPERPOSENT, parce
    // que le mixeur melange les voix au lieu de les enchainer. L'accord sonnerait donc par-dessus la gamme - soit
    // exactement le contraire de ce qu'un apercu doit faire entendre.
    void playInstrumentPreview( std::span<const domain::Note> p_scale,
                                std::span<const domain::Note> p_chord,
                                std::size_t p_instrumentIndex,
                                std::chrono::milliseconds p_noteDuration,
                                std::chrono::milliseconds p_gap ) override;

    // Les timbres du bourdon, charges depuis les ressources.
    //
    // PLUSIEURS peuvent etre offerts, et c'est un TIRAGE qui decide lequel accompagne une question - exactement comme
    // pour les instruments de melodie. Le choix reste stable tant que la question ne change pas : entendre le meme mode
    // sur un autre bourdon serait une autre question.
    //
    // Le bourdon est une donnee de RESTITUTION, pas une regle de musique : le domaine dit QUELLE quinte doit sonner
    // (playMelodyOverDrone), et l'adaptateur choisit avec quel son. Un appareil sans echantillons garde la synthese.
    void useDroneInstruments( std::vector<domain::SampledInstrument> p_drones );

    // La melodie sur un bourdon tenu : les deux voix sont rendues dans UN SEUL tampon, donc elles s'entendent
    // vraiment ensemble. Les jouer l'une apres l'autre donnerait deux questions au lieu d'une.
    //
    // p_framing dit combien de temps le bourdon sonne SEUL avant et apres : c'est ce qui installe le centre avant la
    // couleur, et c'est le domaine qui en fixe les valeurs par defaut.
    void playMelodyOverDrone( std::span<const domain::Note> p_melody,
                              std::span<const domain::Note> p_drone,
                              std::chrono::milliseconds p_noteDuration,
                              std::chrono::milliseconds p_gap,
                              domain::DroneFraming p_framing = {} ) override;

    // Une PHRASE : chaque pas garde SA duree, et le bourdon tient la somme des pas. Meme regle d'assemblage que
    // ci-dessus, et c'est ce qui garantit qu'une phrase et une gamme s'entendent avec le meme bourdon, au meme niveau,
    // decalees de la meme facon.
    void playPhraseOverDrone( std::span<const domain::Note> p_melody,
                              std::span<const std::chrono::milliseconds> p_durations,
                              std::span<const domain::Note> p_drone,
                              std::chrono::milliseconds p_gap,
                              domain::DroneFraming p_framing = {} ) override;

    void playMistakeCue() override;

    // Garde le meme timbre pour la lecture suivante : la demande du domaine, mise en oeuvre ici.
    void holdTimbre() override;

    void beginTimbreForSession() override;
    // Le clic de menu : un accuse de reception, pas une reponse.
    void playTapCue() override;

    // Le clic du metronome : accentue sur le premier temps d'une mesure.
    //
    // NOTE : le metronome de la page Rythme ne passe PLUS par ici. Il bat dans le flux audio (voir startMetronome), a
    // l'echantillon pres. Cette methode reste pour les usages qui veulent un clic immediat, et pour les tests.
    void playMetronomeClick( bool p_accented ) override;

    // -----------------------------------------------------------------------------------------------------------------
    // Le metronome, en tant qu'HORLOGE
    //
    // C'est le mixage qui compte les ECHANTILLONS, et non l'interface qui compte des millisecondes : le clic tombe
    // donc exactement ou le tempo le demande, et la gigue du thread d'interface ne peut plus l'atteindre.
    // -----------------------------------------------------------------------------------------------------------------
    void startMetronome( double p_bpm, int p_beatsPerBar ) override;

    void stopMetronome() override;

    [[nodiscard]] std::int64_t metronomeBeatIndex() const override;
    [[nodiscard]] bool isMetronomeBeatAccented() const override;
    [[nodiscard]] double metronomeElapsedMs() const override;

    // Frappe un element de la batterie, rendu par la synthese.
    void playDrum( domain::Drum p_drum ) override;

    // Frappe un element de la batterie a une position, en millisecondes depuis le premier temps du metronome.
    void playDrumAt( domain::Drum p_drum, double p_positionMs ) override;

    // Les sons de batterie ECHANTILLONNES, dans l'ordre de domain::Drum. Une percussion qui a son echantillon est
    // jouee telle quelle - une vraie peau, une vraie coque, une vraie baguette ; la synthese reste le repli, comme
    // pour les notes.
    void useDrumSamples( std::array<std::vector<float>, domain::DRUM_COUNT> p_samples );

    // Les deux clics du metronome, echantillonnes eux aussi : deux blocs de bois. Vides, la synthese les remplace.
    void useMetronomeClicks( std::vector<float> p_accented, std::vector<float> p_plain );

    // Les woufs du chien. Comme une percussion, ce n'est PAS une note : ils ne se transposent pas et n'ont pas de
    // hauteur musicale, donc les fichiers sont lus pour leurs echantillons seulement.
    //
    // Ils sont QUATRE, et ils TOURNENT a chaque aboiement : le chien ne repete pas la meme phrase deux fois de suite.
    // Un fichier vide est simplement saute - une ressource manquante coute une variante, jamais le wouf entier.
    void useDogBarks( std::vector<std::vector<float>> p_samples );

    // Et il se joue quand le chien ouvre la bouche : une seule fois, sans boucle, par-dessus ce qui joue deja.
    void playDogBark() override;

    // LE BRUITAGE DE GAIN : le tic qui grimpe avec le compte, et la fanfare qui le conclut.
    //
    // Ce sont des BRUITAGES, pas de la musique : ils ne transposent rien, ne s'accordent a rien, et leur seul role est
    // de rendre un gain agreable a regarder s'afficher. Voir le port, NotePlayer::playScoreTick.
    void playScoreTick( int p_progressPercent ) override;

    void playVictoryFanfare() override;

    // Le petit arpège de l'accueil : montant, ouvert, au piano, et VOLONTAIREMENT discret.
    //
    // Il est discret pour deux raisons : un accord de trois notes au niveau des exercices arrive comme une porte qui
    // claque, et l'application n'a rien d'aussi fort a dire au moment ou elle s'ouvre.
    //
    // Ce n'est pas un son du PORT, et c'est une décision : c'est une signature de l'application, pas quelque
    // chose que le domaine a à connaître. Le PROCHAIN son que le jeu joue est un exercice.
    void playGreeting();

    void stopAll() override;

    // The tuning every following note is heard in. The root is always the FIRST note of what is played, which is
    // exactly what the domain's frequencyFor expects: an interval is heard FROM its first note.
    void setTuning( domain::TuningContext p_tuning ) override;

    // The sampled instruments, when there are any. They become the sound of the EXERCISES - a real piano, a real
    // guitar, a real saxophone - while the synthesiser keeps the mistake cue, which must not be beautiful.
    //
    // The instrument is drawn at RANDOM from the list, and that is a decision rather than a shortcut: an interval
    // heard only on a piano has not been heard, and each instrument has its own harmonics and therefore its own
    // colour. Le tirage ne porte que sur les timbres que le joueur accepte - voir useEnabledInstruments - et le
    // filtre appartient au CABLE, pas a cet adaptateur : lui ne connait que des drapeaux.
    //
    // The ORDER of the list is the one the domain names in INSTRUMENT_NAMES, and it is what makes an index mean
    // something: see useEnabledInstruments for what happens when it stops being true.
    //
    // Passing an empty list is legal and means "no samples". p_waveforms lists the PURE WAVEFORMS (sine, sawtooth,
    // square) that join the drawing as additional timbres, so an exercise can be heard with a controlled spectrum.
    // With no samples and no waveform, the synthesiser (the struck string) plays everything.
    void useInstruments( std::vector<domain::SampledInstrument> p_instruments,
                         std::vector<domain::Waveform> p_waveforms );

    // Les timbres que le JOUEUR accepte d'entendre, un drapeau par entree de domain::INSTRUMENT_NAMES.
    //
    // -------------------------------------------------------------------------------------------------------------
    // Pourquoi c'est une liste de drapeaux, et non une liste d'instruments
    //
    // La version precedente recevait la liste des instruments COCHES, et elle etait compactee : decocher le piano
    // faisait glisser tous les rangs d'un cran. Deux consequences, dont une que Roger a vue tout de suite - « il ne
    // joue pas forcement l'instrument en face » :
    //
    //   * le RANG d'un instrument ne veut plus rien dire : l'index demande par le domaine (« joue le 4e ») designait
    //     alors un autre instrument des qu'une case changeait. C'est le bug.
    //   * un instrument DECOCHE disparaissait, donc ne pouvait plus etre ECOUTE - alors que c'est precisement celui
    //     qu'on veut entendre avant de le cocher.
    //
    // Les instruments gardent donc leur place, et la selection ne porte plus que sur le TIRAGE. Une entree peut etre
    // vide (une ressource manquante) et garde quand meme la sienne : c'est ce qui garde les rangs alignes sur ceux du
    // domaine, qui est seul a nommer les instruments.
    //
    // Un drapeau manquant compte comme actif : un adaptateur qui n'a jamais recu de selection joue tout, comme avant.
    void useEnabledInstruments( std::vector<bool> p_enabled );

    [[nodiscard]] std::chrono::milliseconds noteDuration() const override;

    // Opens the audio output immediately, instead of waiting for the first note.
    //
    // A machine without a usable sound card must be diagnosed at start up, not the moment the player
    // taps a button in the middle of an exercise.
    void prepareAudioOutput();

    // Reopens the audio output against whatever device is now default. Called when the operating system says the
    // devices changed - a bluetooth headset plugged or removed - so that the sound follows the player instead of
    // staying on a dead device.
    void reopenAudioOutput();

    // FERME la sortie audio, sans la rouvrir. Tout ce qui sonnait s'arrete avec elle.
    //
    // C'est ce qui REPARE UN CRASH, et il faut le dire ici pour que personne ne la supprime comme une precaution
    // inutile. Roger : « j'ai observe des crash quand je sors de l'application sans la fermer, et que je reviens ».
    // La pile de l'accident est dans le chemin du son - AudioMixer::playAt -> QAudioSink, sur une adresse LIBEREE.
    //
    // Android REND l'appareil audio quand l'application passe en arriere-plan. Le flux qui le tenait survit a ce
    // deuil : au retour, sa premiere note ecrit dans un objet que la plateforme a deja detruit. Le fermer a la mise
    // en veille est la seule facon de ne jamais ecrire dans un flux mort - et il se rouvre tout seul, sur l'appareil
    // du moment, a la premiere note.
    void closeAudioOutput();

    // False when no audio output could be opened.

    //
    // Playback then becomes a silent no-op instead of a crash: a development machine without a sound
    // card, or a CI machine, must still be able to run the application and its tests.
    [[nodiscard]] bool isAudioOutputAvailable() const noexcept;

    // Description of the audio output in use, for the diagnostic log of the application.
    [[nodiscard]] std::string audioOutputDescription() const;

private:
    // Opens the audio output on first use, and decides the sample format once and for all.
    void ensureAudioOutputIsOpen();
    // Starts the sink on the mixer on first need, and hands the device back once the mix falls silent: an output
    // left open on a phone drains the battery.
    void startSinkIfNeeded();
    void stopSinkWhenSilent();

    // Replaces whatever is playing by a new buffer of samples. The right behaviour for a note, a melody, a chord:
    // two overlapping notes would make an interval impossible to name.
    void playSamples( std::vector<float> p_samples, float p_gain = 1.0F );

    // ADDS a buffer to what is already playing. The right behaviour for percussion and for the metronome: a drum hit
    // and a click must be heard TOGETHER, not one instead of the other.
    void mixSamples( std::vector<float> p_samples, float p_gain = 1.0F );

    // Le clic EFFECTIF d'un temps : l'echantillon s'il est la, la synthese sinon. Le mixer n'a pas a connaitre ce
    // repli - il doit recevoir deux sons, un point c'est tout.
    [[nodiscard]] std::vector<float> effectiveClick( bool p_accented );

    std::unique_ptr<QAudioSink> m_audioSink;

    // LA POSITION DU PUITS AU MOMENT OU LE DERNIER SON A COMMENCE, en microsecondes.
    //
    // C'est l'origine de playedMilliseconds : le puits compte depuis son propre demarrage, et ce qu'on veut dire au
    // joueur, c'est « ou en est CE son-la ». Zero quand rien n'a encore ete joue.
    //
    // Seul playSamples la deplace. mixSamples, qui AJOUTE un clic ou une percussion, ne doit surtout pas y toucher : le
    // curseur doit continuer d'avancer pendant que les clics tombent dedans.
    qint64 m_playbackSinkOriginUs{ -1 };

    // Everything the sink reads. Owned here, handed to the sink by start().
    std::unique_ptr<AudioMixer> m_mixer;

    // Whether the sink is currently pulling from the mixer. Tracked because start() on an already running sink
    // restarts it, which would cut the sound being played.
    bool m_isSinkRunning{ false };

    // LE TAMPON DE SORTIE REELLEMENT OBTENU, en octets, tel que la plateforme le donne pour la route du moment.
    //
    // C'est lui qui dit la LATENCE de sortie - le decalage entre ce qui est ecrit et ce qui est entendu - et cette
    // latence sert a juger une frappe. Une valeur inventee ici ferait juger une frappe juste en avance ou en retard.
    //
    // Voir ensureAudioOutputIsOpen : il n'est plus IMPOSE, precisement parce qu'une valeur unique ne peut pas
    // convenir a toutes les routes - un casque Bluetooth n'a pas les memes besoins que le haut-parleur.
    qsizetype m_outputBufferBytes{ 0 };

    // The timbre the next listening will use. Drawn at random among the samples AND the sine (when it is enabled),
    // but STABLE as long as the question does not change: being played back on a different instrument would turn
    // "listen again" into a different question, and the verdict into a trap. Returns an index into m_instruments, or
    // m_instruments.size() for the sine.
    [[nodiscard]] std::size_t timbreIndexFor( std::span<const domain::Note> p_notes );

    // Le joueur accepte-t-il ce timbre ? Lu par le TIRAGE et par lui seul : un apercu joue ce qu'on lui demande, coche
    // ou pas. Un drapeau manquant compte comme actif, parce qu'un adaptateur qui ne connait pas encore les preferences
    // du joueur doit continuer de jouer tout ce qu'il a.
    [[nodiscard]] bool isTimbreEnabled( std::size_t p_timbreIndex ) const;

    // L'instrument de ce rang, ou nul quand il n'y en a pas.
    //
    // SEUL endroit qui sait qu'une entree peut etre VIDE tout en gardant sa place : une ressource qui n'a pas pu etre
    // lue laisse un trou, et le trou doit rester a son rang pour que les index du domaine continuent de designer les
    // memes instruments. Ce que les appelants en font : ils retombent sur la synthese, parce qu'une note silencieuse
    // serait un trou dans l'exercice.
    [[nodiscard]] const domain::SampledInstrument * instrumentAt( std::size_t p_timbreIndex ) const;

    // One note, rendered with the timbre drawn for the given sequence. Falls back on the struck-string synthesiser
    // when there is neither a sample nor the sine.
    [[nodiscard]] std::vector<float> renderNoteFor( std::span<const domain::Note> p_sequence,
                                                    const domain::Note & p_note,
                                                    std::chrono::milliseconds p_duration );

    [[nodiscard]] std::vector<float> renderChordFor( std::span<const domain::Note> p_notes,
                                                     std::chrono::milliseconds p_duration );

    // Les memes, mais avec un TIMBRE IMPOSE : c'est ce qu'un apercu demande. Le tirage et l'enregistrement de la
    // question en cours appartiennent aux versions ci-dessus, et a elles seules - un apercu ne doit pas changer le
    // timbre de la session en cours.
    [[nodiscard]] std::vector<float> renderChordWithIndex( std::span<const domain::Note> p_notes,
                                                           std::chrono::milliseconds p_duration,
                                                           std::size_t p_timbreIndex );

    [[nodiscard]] std::vector<float> renderMelodyFor( std::span<const domain::Note> p_notes,
                                                      std::chrono::milliseconds p_noteDuration,
                                                      std::chrono::milliseconds p_gap );

    [[nodiscard]] std::vector<float> renderMelodyWithIndex( std::span<const domain::Note> p_notes,
                                                            std::chrono::milliseconds p_noteDuration,
                                                            std::chrono::milliseconds p_gap,
                                                            std::size_t p_timbreIndex );

    // The sampled instruments, empty when there are none.
    std::vector<domain::SampledInstrument> m_instruments;

    // Un drapeau par entree de domain::INSTRUMENT_NAMES : les timbres que le joueur accepte d'entendre. Le TIRAGE ne
    // porte que sur ceux-la (voir timbreIndexFor) ; un apercu, lui, ignore ce filtre, parce qu'ecouter ce qu'on n'a pas
    // encore coche est justement a quoi sert le bouton.
    std::vector<bool> m_enabledTimbre;
    // Les timbres du bourdon, vides quand aucun echantillon n'a pu etre lu - auquel cas la synthese prend le relais.
    std::vector<domain::SampledInstrument> m_drones;

    // Le domaine a demande de garder le timbre pour la lecture suivante. Consomme par la lecture qui suit, comme un
    // jeton : une demande, une lecture.
    bool m_holdTimbre{ false };

    // Le timbre est TENU pour toute la session quand ce drapeau est pose : voir beginTimbreForSession. Il est remis a faux
    // par holdTimbre, qui est une decision d'une seule question - une comparaison de deux modes.
    bool m_timbreIsHeldForSession{ false };

    // Le timbre de bourdon tire pour la question en cours, et le bourdon qu'elle accompagnait : c'est ce qui garde le
    // meme son tant que la question ne change pas.
    std::size_t m_droneIndex{ 0 };
    std::vector<domain::Note> m_lastDroneNotes;

    // The pure waveforms that join the drawing, empty when there are none.
    std::vector<domain::Waveform> m_waveforms;

    // The drawn timbre: an index into m_instruments, or m_instruments.size() + a waveform index.
    std::size_t m_instrumentIndex{ 0 };

    // What was played last, which is how "the same question" is recognised.
    std::vector<domain::Note> m_lastPlayedNotes;

    std::mt19937 m_instrumentRandomEngine{ std::random_device{}() };

    std::optional<domain::ToneSynthesizer> m_synthesizer;

    // Created from the same real sample rate, so the drums are in tune with the notes.
    std::optional<domain::DrumSynthesizer> m_drumSynthesizer;

    // One recorded sound per piece, in the order of domain::Drum. Empty when the samples could not be read, in which
    // case the synthesiser plays.
    std::array<std::vector<float>, domain::DRUM_COUNT> m_drumSamples;

    // Les woufs du chien, s'ils ont ete trouves au chargement. Vides, le clic de menu prend leur place ; et c'est
    // l'INDEX qui avance, pas l'ordre du tableau : c'est ce qui les fait tourner au lieu de reprendre le premier.
    std::vector<std::vector<float>> m_dogBarks;
    std::size_t m_nextDogBark{ 0 };

    // The two metronome clicks: the accented one, and the plain one. Empty when they could not be read.
    std::vector<float> m_accentedClick;
    std::vector<float> m_plainClick;

    // Equal temperament at 440 Hz until setTuning says otherwise.
    domain::TuningContext m_tuning;

    QAudioFormat m_audioFormat;
    std::string m_outputDescription{ "not opened yet" };
};

}    // namespace musichien::infrastructure
