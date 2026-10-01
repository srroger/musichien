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
    // colour. The day the player chooses his instrument explicitly, this becomes a preference - the drawing here
    // is what makes the variety exist in the meantime.
    //
    // Passing an empty list is legal and means "no samples". p_waveforms lists the PURE WAVEFORMS (sine, sawtooth,
    // square) that join the drawing as additional timbres, so an exercise can be heard with a controlled spectrum.
    // With no samples and no waveform, the synthesiser (the struck string) plays everything.
    void useInstruments( std::vector<domain::SampledInstrument> p_instruments,
                         std::vector<domain::Waveform> p_waveforms );

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

    // Everything the sink reads. Owned here, handed to the sink by start().
    std::unique_ptr<AudioMixer> m_mixer;

    // Whether the sink is currently pulling from the mixer. Tracked because start() on an already running sink
    // restarts it, which would cut the sound being played.
    bool m_isSinkRunning{ false };

    // The timbre the next listening will use. Drawn at random among the samples AND the sine (when it is enabled),
    // but STABLE as long as the question does not change: being played back on a different instrument would turn
    // "listen again" into a different question, and the verdict into a trap. Returns an index into m_instruments, or
    // m_instruments.size() for the sine.
    [[nodiscard]] std::size_t timbreIndexFor( std::span<const domain::Note> p_notes );

    // One note, rendered with the timbre drawn for the given sequence. Falls back on the struck-string synthesiser
    // when there is neither a sample nor the sine.
    [[nodiscard]] std::vector<float> renderNoteFor( std::span<const domain::Note> p_sequence,
                                                    const domain::Note & p_note,
                                                    std::chrono::milliseconds p_duration );

    [[nodiscard]] std::vector<float> renderChordFor( std::span<const domain::Note> p_notes,
                                                     std::chrono::milliseconds p_duration );

    [[nodiscard]] std::vector<float> renderMelodyFor( std::span<const domain::Note> p_notes,
                                                      std::chrono::milliseconds p_noteDuration,
                                                      std::chrono::milliseconds p_gap );

    // The sampled instruments, empty when there are none.
    std::vector<domain::SampledInstrument> m_instruments;

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
