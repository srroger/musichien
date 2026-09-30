#pragma once

// =====================================================================================================================
// Musichien - NotePlayer
//
// The domain expresses WHAT it needs: make a note audible. It has no idea whether the sound comes
// from QAudioSink, from Oboe on Android, or from a loudspeaker driven by a microcontroller.
//
// This is the dependency inversion principle in practice: the interface lives in the domain, the
// implementation lives in the infrastructure. Consequence: every piece of logic that uses a
// NotePlayer is testable with NotePlayerFake, on a machine with no sound card at all.
//
// See docs/ARCHITECTURE.md.
// =====================================================================================================================

#include "domain/audio/DrumSynthesizer.h"
#include "domain/audio/ToneSynthesizer.h"
#include "domain/music/Note.h"
#include "domain/music/Temperament.h"

#include <chrono>
#include <cstdint>
#include <span>

namespace musichien::domain
{

class NotePlayer
{
public:
    virtual ~NotePlayer() = default;

    NotePlayer() = default;
    NotePlayer( const NotePlayer & ) = delete;
    NotePlayer & operator=( const NotePlayer & ) = delete;
    NotePlayer( NotePlayer && ) = delete;
    NotePlayer & operator=( NotePlayer && ) = delete;

    // Plays a single note, for its natural duration.
    virtual void playNote( const Note & p_note ) = 0;

    // Plays the notes one after another, the way a melodic interval is heard.
    // p_gap is the silence left between two notes: without a gap, two notes sound like a glide.
    virtual void playMelody( std::span<const Note> p_notes, std::chrono::milliseconds p_gap ) = 0;

    // Plays the notes at the same time, the way a harmonic interval or a chord is heard.
    virtual void playChord( std::span<const Note> p_notes ) = 0;

    // Plays the notes together for an EXPLICIT duration: the long, sustained chord that lets the ear count the
    // beating between two frequencies - which is exactly how a temperament difference is heard. The regular
    // playChord uses the natural note duration.
    //
    // A default body, like playTapCue: an adapter that has no use for a sustained chord keeps the natural duration.
    virtual void playChordFor( std::span<const Note> p_notes, std::chrono::milliseconds p_duration )
    {
        (void)p_duration;

        playChord( p_notes );
    }

    // Plays a melody heard OVER a drone held under it: the two sound TOGETHER, from the first note to the last.
    //
    // -------------------------------------------------------------------------------------------------------------
    // Pourquoi le domaine demande cela, et pourquoi c'est une question de MUSIQUE avant d'etre d'audio
    //
    // Un mode n'est pas un jeu de notes, c'est un jeu de notes PLUS UN CENTRE : les sept memes notes sur re sont du
    // re dorien, sur si bemol elles sont du si bemol majeur. Sans centre, l'oreille entend une gamme et aucun mode -
    // donc « ecoute cette gamme et nomme le mode » n'est pas une question difficile, c'est une question sans reponse.
    //
    // Le bourdon EST ce centre. C'est pour cela que le domaine le demande ici, plutot que de laisser chaque
    // adaptateur decider d'en jouer un : le bourdon n'est pas un ornement de restitution, c'est la moitie de la
    // question posee au joueur.
    //
    // Ce qui reste a l'adaptateur : le TIMBRE et le niveau relatif des deux voix, exactement comme pour un clic de
    // metronome. Ce que le domaine decide : que les deux s'entendent ENSEMBLE - deux appels successifs, un accord
    // puis une melodie, donneraient deux questions au lieu d'une.
    //
    // A default body, like playChordFor: an adapter with no use for a drone falls back on playing the melody, which
    // is what it would have done alone. A test that only checks the MELODY still gets it.
    virtual void playMelodyOverDrone( std::span<const Note> p_melody,
                                      std::span<const Note> p_drone,
                                      std::chrono::milliseconds p_noteDuration,
                                      std::chrono::milliseconds p_gap,
                                      DroneFraming p_framing = {} )
    {
        (void)p_drone;
        (void)p_noteDuration;
        (void)p_framing;

        playMelody( p_melody, p_gap );
    }

    // Une PHRASE sur un bourdon : les memes notes que ci-dessus, mais CHACUNE avec sa duree.
    //
    // C'est la difference entre une gamme et une phrase, et elle n'est pas cosmetique : les degres disent la couleur, et
    // les DUREES disent la musique. Jouer une phrase en notes uniformes ferait entendre autre chose que la phrase que
    // l'oreille avait choisie - et ce serait pourtant celle-la qu'on lui aurait fait ecouter.
    //
    // Un corps par defaut, comme playChordFor : un adaptateur qui ignore les phrases joue la melodie avec la premiere
    // duree pour toutes. La phrase perd son rythme, mais elle reste audible, et rien ne casse.
    virtual void playPhraseOverDrone( std::span<const Note> p_melody,
                                      std::span<const std::chrono::milliseconds> p_durations,
                                      std::span<const Note> p_drone,
                                      std::chrono::milliseconds p_gap,
                                      DroneFraming p_framing = {} )
    {
        // Le nom de la variable ne peut pas etre celui de la methode qu'elle appelle : le masquage ferait du repli un
        // appel a la variable elle-meme.
        const std::chrono::milliseconds fallbackDuration = p_durations.empty() ? noteDuration() : p_durations.front();

        playMelodyOverDrone( p_melody, p_drone, fallbackDuration, p_gap, p_framing );
    }

    // Plays the short cue that marks a mistake.
    //
    // A cue is NOT an interval, and the domain says so here rather than leaving the distinction to the
    // adapter: it has no pitch, it is not something to be recognised, and it must never be mistaken for
    // one of the sounds being taught. What it sounds like is the adapter's business - that a mistake is
    // AUDIBLE is the domain's.
    virtual void playMistakeCue() = 0;

    // Un clic de menu : un son très court, très doux, qui ne dit rien d'autre que "ta main a été entendue".
    //
    // Il a un CORPS PAR DÉFAUT, et c'est délibéré : un appareil sans retour sonore n'a rien à implémenter, et un
    // test qui ne s'intéresse pas au clic n'a rien à écrire non plus. Le feedback d'un bouton ne mérite pas
    // d'obliger tous les adaptateurs du projet à répondre.
    virtual void playTapCue() {}

    // Le clic du métronome : un temps simple, ou le PREMIER temps d'une mesure (accentué). Un corps par défaut, comme
    // le clic de menu : un adaptateur sans métronome se contente du clic ordinaire.
    virtual void playMetronomeClick( bool p_accented )
    {
        (void)p_accented;
        playTapCue();
    }

    // -------------------------------------------------------------------------------------------------------------
    // LE METRONOME, en tant qu'HORLOGE
    //
    // Le domaine dit CE QU'IL VEUT - battre a tel tempo, sur telle mesure - et l'adaptateur le fait battre au rythme
    // de son propre flux audio, ou chaque clic tombe sur un ECHANTILLON exact.
    //
    // Pourquoi cela ne peut pas etre un QTimer : entre deux tics d'un timer d'interface il y a le rendu de QML, les
    // evenements du systeme et le ramasse-miettes. Le clic tombe donc la ou le thread a bien voulu, avec plusieurs
    // millisecondes de gigue - et c'est cette gigue qui s'entend comme une instabilite. Un metronome juste compte des
    // echantillons.
    //
    // Les cinq methodes ont un corps par defaut, comme playTapCue : un adaptateur sans horloge audio n'a rien a
    // implementer, et un test qui ne parle pas de rythme n'a rien a ecrire.
    // -------------------------------------------------------------------------------------------------------------

    // Demarre le metronome. Rappeler cette methode pendant qu'il bat le RECALE sur l'instant present : c'est ce qui
    // rend un changement de tempo immediat et propre.
    virtual void startMetronome( double p_bpm, int p_beatsPerBar )
    {
        (void)p_bpm;
        (void)p_beatsPerBar;
    }

    virtual void stopMetronome() {}

    // Ou en est le metronome du point de vue de l'OREILLE : le rang du temps en cours, s'il est accentue, et combien de
    // millisecondes se sont ecoulees depuis le premier temps.
    //
    // C'est ce temps-la qui juge une frappe et qui remplit l'affichage - jamais une horloge d'interface, qui
    // mesurerait autre chose que ce que le joueur entend.
    [[nodiscard]] virtual std::int64_t metronomeBeatIndex() const { return 0; }

    [[nodiscard]] virtual bool isMetronomeBeatAccented() const { return false; }

    [[nodiscard]] virtual double metronomeElapsedMs() const { return 0.0; }

    // Frappe un élément de la batterie. Un corps par défaut, comme le clic : un adaptateur sans batterie ne fait rien.
    virtual void playDrum( Drum p_drum )
    {
        (void)p_drum;
    }

    // Frappe un element de la batterie a une POSITION, en millisecondes depuis le premier temps du metronome en cours.
    //
    // C'est ce qui permet a une cellule rythmique d'etre jouee JUSTE : une frappe decalee - une syncope - doit tomber
    // entre deux temps, et un QTimer d'interface ne sait pas viser un instant. Ici, la position se traduit en
    // echantillons, et le son est pose exactement la.
    //
    // Une position deja passee se joue tout de suite : mieux vaut un son en retard qu'un son absent.
    virtual void playDrumAt( Drum p_drum, double p_positionMs )
    {
        (void)p_positionMs;
        playDrum( p_drum );
    }

    // The tuning every following note is heard in, until it is called again. Equal temperament and a 440 Hz diapason
    // are the defaults, which is exactly what a NotePlayer that never receives this call keeps doing.
    //
    // A default body, like playTapCue: a test that only counts the notes played has no tuning to care about, and
    // must not be forced to write one.
    virtual void setTuning( TuningContext p_tuning ) { (void)p_tuning; }

    // Stops everything immediately. Called when the screen is left or the application goes to the
    // background: an audio stream left open on a phone is a battery drain and a bug.
    virtual void stopAll() = 0;

    // Duration used by playNote and by the chord rendering.
    [[nodiscard]] virtual std::chrono::milliseconds noteDuration() const = 0;
};

}    // namespace musichien::domain
