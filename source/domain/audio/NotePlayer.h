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

    // DEUX accords, l'un APRES l'autre, dans un seul rendu : le premier est ce que le joueur a joue, le second est la
    // reponse.
    //
    // Roger, apres avoir joue une question d'accords : « quand on clique sur un accord et qu'on se trompe, on re-entend
    // directement le bon accord. Je changerais ca : entendre d'abord l'accord appuye, PUIS l'accord voulu. C'est moins
    // perturbant. »
    //
    // Il a raison, et la raison est plus profonde que le confort : entendre la REPONSE avant d'avoir entendu sa propre
    // erreur efface l'ECART entre les deux - et cet ecart est toute la lecon. L'oreille doit pouvoir se dire « voila ce
    // que j'ai cru, voila ce qui etait », dans cet ordre.
    //
    // UN SEUL APPEL, et non deux : un nouveau son REMPLACE le precedent (voir QAudioNotePlayer::playSamples), donc deux
    // appels ne feraient entendre que le second - exactement ce qu'on cherche a corriger.
    //
    // Un corps par defaut, comme playChordFor : un adaptateur qui ne sait pas enchainer joue la REPONSE, qui est la
    // partie a ne pas manquer.
    virtual void playChordThenChord( std::span<const Note> p_first,
                                     std::span<const Note> p_second,
                                     std::chrono::milliseconds p_gap )
    {
        (void)p_first;
        (void)p_gap;

        playChord( p_second );
    }

    // Fait ENTENDRE un instrument, pour qu'on puisse le CHOISIR : la gamme demandee, puis l'accord demande, avec CE
    // timbre et aucun autre.
    //
    // -------------------------------------------------------------------------------------------------------------
    // Pourquoi cette methode est dans le port, et pas dans l'adaptateur
    //
    // Choisir un timbre se fait a l'oreille, et l'oreille ne peut pas choisir ce qu'elle n'a pas entendu : la page des
    // reglages offre onze instruments, et un nom ne dit rien de ce qu'on entendra. C'est Roger qui a demande ce bouton -
    // « pour l'utilisateur, c'est un peu complique de choisir son instrument car c'est complique de l'entendre ».
    //
    // La demande passe par le PORT parce que c'est la seule facon de rester testable : un NotePlayerFake enregistre la
    // gamme ET l'accord, donc un test verifie la musique entendue sans carte son. L'ADAPTATEUR, lui, garde ce qui n'est
    // pas a nous : rendre les deux dans UN SEUL tampon, car deux appels successifs se superposeraient et l'accord
    // sonnerait PAR-DESSUS la gamme.
    //
    // p_instrumentIndex est un index du domaine (INSTRUMENT_NAMES), jamais un rang interne a l'adaptateur.
    //
    // -------------------------------------------------------------------------------------------------------------
    // Un corps par defaut, comme playChordFor : un adaptateur qui n'a qu'un timbre joue quand meme la gamme et l'accord
    // - il ne montre simplement pas de difference, ce qui est exactement ce qu'il a a montrer.
    virtual void playInstrumentPreview( std::span<const Note> p_scale,
                                        std::span<const Note> p_chord,
                                        std::size_t p_instrumentIndex,
                                        std::chrono::milliseconds p_noteDuration,
                                        std::chrono::milliseconds p_gap )
    {
        (void)p_instrumentIndex;

        playMelody( p_scale, p_gap );

        // L'accord est TENU bien plus longtemps qu'une note - cinq fois, comme dans l'adaptateur : c'est une couleur qu'on
        // ecoute, pas un pas qu'on enchaine, et il faut le temps de l'entendre battre.
        playChordFor( p_chord, p_noteDuration * 5 );
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

    // Le petit wouf du chien qui raconte une anecdote.
    //
    // Un corps par defaut, comme le clic de menu, et il TOMBE DESSUS a dessein : Roger a donne les deux solutions dans la
    // meme phrase - un son de chien « doux et tres court », « ou sinon, juste le meme petit son que tu avais sur les
    // boutons de la difficulte ». Un adaptateur qui n'a pas d'aboiement continuera donc de repondre quelque chose.
    virtual void playDogBark() { playTapCue(); }

    // LE TIC DU COMPTE QUI GRIMPE, ET LA FANFARE DE VICTOIRE.
    //
    // Roger : « une animation sur les nombres ... et un bruitage de jeux video gling gling gling, ou de machine a sous
    // ... et un bruitage ou melodie ou accord de victoire ». Et il l'assume pour ce que c'est : « c'est juste un
    // bruitage pour rendre le jeu moins austere, et faire appel a des biais cognitifs d'addiction, comme dans les
    // machines a sous ».
    //
    // Deux CORPS PAR DEFAUT, comme playTapCue et playDogBark : un adaptateur sans retour sonore n'a rien a implementer,
    // et un test qui ne compte que les notes n'a rien a entendre. Ce qui appartient au DOMAINE est qu'un gain soit
    // AUDIBLE - jamais ce qu'il sonne.
    //
    // p_progressPercent dit OU EN EST le compte, de 0 a 100 : c'est ce qui permet au tic de MONTER avec le chiffre. Le
    // domaine n'en fait rien, mais l'adaptateur a besoin de le savoir, et le lui redemander a l'ecran serait une
    // dependance de plus pour rien.
    virtual void playScoreTick( int p_progressPercent ) { (void)p_progressPercent; }

    virtual void playVictoryFanfare() {}

    // LE TIMBRE D'UNE SESSION, choisi une fois et garde jusqu'au bout.
    //
    // Roger a mis le doigt sur une incoherence en ecoutant : le timbre changeait a CHAQUE question, donc un joueur qui
    // entendait un saxo sur une seconde et un piano sur une quinte comparait deux choses differentes - alors que la
    // question porte sur l'intervalle. Un timbre par session, et l'oreille ne juge plus que ce qu'on lui demande.
    //
    // Un corps par defaut, comme playChordFor : un adaptateur qui n'a pas de timbre a choisir n'a rien a faire ici.
    virtual void beginTimbreForSession() {}

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

    // Garde le MEME TIMBRE pour la lecture suivante, meme si les notes changent.
    //
    // C'est ce qu'une comparaison demande, et l'adaptateur ne peut pas le deviner : deux modes n'ont pas les memes notes,
    // donc la regle « memes notes, meme timbre » ne s'applique pas, et le bourdon d'un vamp change de centre par nature.
    // Sans cela, la guitare devenait un saxophone ENTRE LES DEUX PASSAGES - et Roger l'a entendu : « il faudrait que ca
    // utilise les memes instruments, ca evite le bruit de la difference d'instrument, l'exercice est deja difficile ».
    //
    // Un corps par defaut, comme playTapCue : un adaptateur qui n'a qu'un timbre n'a rien a garder.
    virtual void holdTimbre() {}

    // Stops everything immediately. Called when the screen is left or the application goes to the
    // background: an audio stream left open on a phone is a battery drain and a bug.
    virtual void stopAll() = 0;

    // Duration used by playNote and by the chord rendering.
    [[nodiscard]] virtual std::chrono::milliseconds noteDuration() const = 0;
};

}    // namespace musichien::domain
