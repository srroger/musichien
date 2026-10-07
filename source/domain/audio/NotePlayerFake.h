#pragma once

// =====================================================================================================================
// Musichien - NotePlayerFake
//
// A test double. It records what it was asked to play instead of making any sound.
//
// Why this file lives in the domain and not in a test folder: several test executables need it, and
// duplicating a fake is how two test suites slowly start testing different things.
//
// Usage in a test:
//
//     NotePlayerFake player;
//     ExerciseRunner runner{ player };
//     runner.startNextExercise();
//     EXPECT_EQ( 2, player.playedMelodies().size() );
// =====================================================================================================================

#include "domain/audio/NotePlayer.h"

#include <array>
#include <cstddef>
#include <vector>

namespace musichien::domain
{

class NotePlayerFake final : public NotePlayer
{
public:
    // Structure describing one call to playMelody or playChord.
    struct PlayedGroup
    {
        std::vector<Note> notes;
        std::chrono::milliseconds gap{ 0 };
    };

    // Un appel a playMelodyOverDrone : la melodie ET le bourdon, gardes ENSEMBLE.
    //
    // Et c'est le point : ce que le domaine demande, c'est leur simultaneite. Un fake qui les enregistrerait dans
    // deux listes separees ne pourrait pas la verifier - et c'est justement elle qui fait la question posee.
    struct PlayedOverDrone
    {
        std::vector<Note> melody;
        std::vector<Note> drone;
        std::chrono::milliseconds gap{ 0 };

        // L'encadrement demande : c'est lui qui dit combien de temps le bourdon sonne SEUL, avant et apres la
        // melodie. Un test peut donc verifier que le centre est installe avant la couleur.
        DroneFraming framing{};
    };

    // Un appel a playPhraseOverDrone : la melodie, la duree de CHAQUE pas, et le bourdon.
    //
    // Les durees sont gardees telles quelles, sans etre reduites a une moyenne : c'est ce qui permet a un test de dire
    // « la note longue est bien restee longue », qui est exactement ce qu'une phrase a de plus qu'une gamme.
    struct PlayedPhraseOverDrone
    {
        std::vector<Note> melody;
        std::vector<std::chrono::milliseconds> durations;
        std::vector<Note> drone;
        std::chrono::milliseconds gap{ 0 };
        DroneFraming framing{};
    };

    explicit NotePlayerFake( std::chrono::milliseconds p_noteDuration = std::chrono::milliseconds{ 600 } )
      : m_noteDuration{ p_noteDuration }
    {
    }

    void playNote( const Note & p_note ) override
    {
        m_playedNotes.push_back( p_note );
    }

    void playMelody( std::span<const Note> p_notes, std::chrono::milliseconds p_gap ) override
    {
        m_playedMelodies.push_back( PlayedGroup{ std::vector<Note>{ p_notes.begin(), p_notes.end() }, p_gap } );
    }

    void playChord( std::span<const Note> p_notes ) override
    {
        m_playedChords.push_back( PlayedGroup{ std::vector<Note>{ p_notes.begin(), p_notes.end() },
                                               std::chrono::milliseconds{ 0 } } );
    }

    // Ce que le joueur a joue, puis la reponse : les DEUX, dans cet ordre et gardes ENSEMBLE.
    //
    // L'ordre est tout ce qu'un test doit pouvoir verifier - c'est lui que Roger a demande de changer (« d'abord l'accord
    // appuye, PUIS l'accord voulu ») - et deux listes separees ne le diraient pas.
    struct PlayedChordPair
    {
        std::vector<Note> first;
        std::vector<Note> second;
        std::chrono::milliseconds gap{ 0 };
    };

    void playChordThenChord( std::span<const Note> p_first,
                             std::span<const Note> p_second,
                             std::chrono::milliseconds p_gap ) override
    {
        m_chordPairs.push_back( PlayedChordPair{ std::vector<Note>{ p_first.begin(), p_first.end() },
                                                 std::vector<Note>{ p_second.begin(), p_second.end() },
                                                 p_gap } );
    }

    void playMelodyOverDrone( std::span<const Note> p_melody,
                              std::span<const Note> p_drone,
                              std::chrono::milliseconds p_noteDuration,
                              std::chrono::milliseconds p_gap,
                              DroneFraming p_framing = {} ) override
    {
        (void)p_noteDuration;

        m_melodiesOverDrones.push_back( PlayedOverDrone{ std::vector<Note>{ p_melody.begin(), p_melody.end() },
                                                         std::vector<Note>{ p_drone.begin(), p_drone.end() },
                                                         p_gap,
                                                         p_framing } );
    }

    void playPhraseOverDrone( std::span<const Note> p_melody,
                              std::span<const std::chrono::milliseconds> p_durations,
                              std::span<const Note> p_drone,
                              std::chrono::milliseconds p_gap,
                              DroneFraming p_framing = {} ) override
    {
        m_phrasesOverDrones.push_back( PlayedPhraseOverDrone{
          std::vector<Note>{ p_melody.begin(), p_melody.end() },
          std::vector<std::chrono::milliseconds>{ p_durations.begin(), p_durations.end() },
          std::vector<Note>{ p_drone.begin(), p_drone.end() },
          p_gap,
          p_framing } );
    }

    void playMistakeCue() override
    {
        ++m_mistakeCueCount;
    }

    // OU EN EST LE SON, pour un test : la position est POSEE, et non mesuree.
    //
    // Sans cela, un fake repondrait toujours zero et aucun test ne pourrait dire qu'un curseur AVANCE - ni qu'il recule
    // quand le son repart de zero. Voir ExerciseSessionController::rhythmPositionInBeats.
    void setPlayedMilliseconds( std::chrono::milliseconds p_position ) { m_playedMilliseconds = p_position; }

    [[nodiscard]] std::chrono::milliseconds playedMilliseconds() const override { return m_playedMilliseconds; }

    // Le wouf du chien, compte a part : c'est ce qui permet a un test de dire QUAND il aboie - a la fin d'une partie, et
    // pas pendant.
    void playDogBark() override
    {
        ++m_dogBarkCount;
    }

    // Le tic du compte et la fanfare de victoire, comptes a part comme le wouf : c'est ce qui permet a un test de dire
    // QUAND ils sonnent - a la fin d'une partie, et pas pendant.
    //
    // Le DERNIER progres recu est garde : le tic doit MONTER avec le chiffre, et « il a sonne » ne suffirait pas a le
    // verifier.
    void playScoreTick( int p_progressPercent ) override
    {
        ++m_scoreTickCount;
        m_lastScoreTickProgress = p_progressPercent;
    }

    void playVictoryFanfare() override
    {
        ++m_victoryFanfareCount;
    }

    // Le metronome et la batterie sont des sons A PART : les compter separement est ce qui permet a un test de dire
    // "le clic ET la caisse claire ont ete demandes", et donc de tenir le bug du mixage ferme.
    void playMetronomeClick( bool p_accented, double p_gain = 1.0 ) override
    {
        (void)p_gain;

        ++m_metronomeClickCount;

        if( p_accented )
        {
            ++m_accentedClickCount;
        }
    }

    void playDrum( Drum p_drum ) override
    {
        ++m_drumCounts.at( static_cast<std::size_t>( p_drum ) );
    }

    void playDrumAt( Drum p_drum, double p_positionMs ) override
    {
        ++m_drumCounts.at( static_cast<std::size_t>( p_drum ) );

        m_drumPositions.push_back( p_positionMs );
    }

    void stopAll() override
    {
        ++m_stopCount;
    }

    [[nodiscard]] std::chrono::milliseconds noteDuration() const override { return m_noteDuration; }

    // -----------------------------------------------------------------------------------------------------------------
    // Le metronome : le fake le SIMULE, et c'est ce qui permet a un test de verifier la logique du jeu - le bon temps,
    // le bon accent, la bonne duree - sans dependre d'une carte son ni d'une horloge.
    // -----------------------------------------------------------------------------------------------------------------
    void startMetronome( double p_bpm, int p_beatsPerBar ) override
    {
        m_metronomeBpm = p_bpm;
        m_metronomeBeatsPerBar = p_beatsPerBar;
        m_isMetronomeRunning = true;
        m_metronomeBeatIndex = 0;
        m_metronomeElapsedMs = 0.0;

        ++m_metronomeStartCount;
    }

    void stopMetronome() override
    {
        m_isMetronomeRunning = false;

        ++m_metronomeStopCount;
    }

    [[nodiscard]] std::int64_t metronomeBeatIndex() const override { return m_metronomeBeatIndex; }

    [[nodiscard]] bool isMetronomeBeatAccented() const override
    {
        return ( m_metronomeBeatsPerBar > 0 ) && ( ( m_metronomeBeatIndex % m_metronomeBeatsPerBar ) == 0 );
    }

    [[nodiscard]] double metronomeElapsedMs() const override { return m_metronomeElapsedMs; }

    // -----------------------------------------------------------------------------------------------------------------
    // Observability, for the assertions of a test
    // -----------------------------------------------------------------------------------------------------------------
    [[nodiscard]] const std::vector<Note> & playedNotes() const noexcept { return m_playedNotes; }
    [[nodiscard]] const std::vector<PlayedGroup> & playedMelodies() const noexcept { return m_playedMelodies; }
    [[nodiscard]] const std::vector<PlayedGroup> & playedChords() const noexcept { return m_playedChords; }

    [[nodiscard]] const std::vector<PlayedChordPair> & playedChordPairs() const noexcept { return m_chordPairs; }

    // Les appels « melodie sur bourdon », avec les deux voix : c'est ce qu'un test lit pour verifier que le bourdon
    // a bien ete demande EN MEME TEMPS que la melodie.
    [[nodiscard]] const std::vector<PlayedOverDrone> & melodiesOverDrones() const noexcept
    {
        return m_melodiesOverDrones;
    }

    // Les appels « phrase sur bourdon », avec la duree de chaque pas : c'est ce qu'un test lit pour verifier qu'une
    // phrase n'a pas ete jouee comme une gamme reguliere.
    [[nodiscard]] const std::vector<PlayedPhraseOverDrone> & phrasesOverDrones() const noexcept
    {
        return m_phrasesOverDrones;
    }
    [[nodiscard]] int mistakeCueCount() const noexcept { return m_mistakeCueCount; }

    // Le nombre de woufs demandes. Un chien qui aboie a chaque question serait pire que pas de chien du tout : c'est ce
    // compteur qui permet de le verifier.
    [[nodiscard]] int dogBarkCount() const noexcept { return m_dogBarkCount; }

    [[nodiscard]] int scoreTickCount() const noexcept { return m_scoreTickCount; }

    // Le dernier progres annonce par l'ecran pendant que le compte grimpe, de 0 a 100.
    [[nodiscard]] int lastScoreTickProgress() const noexcept { return m_lastScoreTickProgress; }

    [[nodiscard]] int victoryFanfareCount() const noexcept { return m_victoryFanfareCount; }

    [[nodiscard]] int stopCount() const noexcept { return m_stopCount; }

    [[nodiscard]] int metronomeClickCount() const noexcept { return m_metronomeClickCount; }
    [[nodiscard]] int accentedClickCount() const noexcept { return m_accentedClickCount; }

    [[nodiscard]] bool isMetronomeRunning() const noexcept { return m_isMetronomeRunning; }
    [[nodiscard]] double metronomeBpm() const noexcept { return m_metronomeBpm; }
    [[nodiscard]] int metronomeBeatsPerBar() const noexcept { return m_metronomeBeatsPerBar; }
    [[nodiscard]] int metronomeStartCount() const noexcept { return m_metronomeStartCount; }
    [[nodiscard]] int metronomeStopCount() const noexcept { return m_metronomeStopCount; }

    // Fait AVANCER le temps du metronome comme le ferait le flux audio : un temps de plus, et tant de millisecondes
    // ecoulees depuis le premier.
    void advanceMetronomeTo( std::int64_t p_beatIndex, double p_elapsedMs )
    {
        m_metronomeBeatIndex = p_beatIndex;
        m_metronomeElapsedMs = p_elapsedMs;
    }

    [[nodiscard]] int drumCount() const noexcept
    {
        int total = 0;

        for( const int count : m_drumCounts )
        {
            total += count;
        }

        return total;
    }

    [[nodiscard]] int drumCount( Drum p_drum ) const noexcept
    {
        return m_drumCounts.at( static_cast<std::size_t>( p_drum ) );
    }

    // Les positions demandees, en millisecondes : c'est ce qu'un test lit pour verifier qu'une syncope tombe ENTRE
    // deux temps, et non « quelque part ».
    [[nodiscard]] const std::vector<double> & drumPositions() const noexcept { return m_drumPositions; }

    void clear()
    {
        m_playedNotes.clear();
        m_playedMelodies.clear();
        m_playedChords.clear();
        m_chordPairs.clear();
        m_melodiesOverDrones.clear();
        m_phrasesOverDrones.clear();
        m_mistakeCueCount = 0;
        m_dogBarkCount = 0;
        m_scoreTickCount = 0;
        m_lastScoreTickProgress = 0;
        m_victoryFanfareCount = 0;
        m_stopCount = 0;
        m_metronomeClickCount = 0;
        m_accentedClickCount = 0;
        m_drumCounts = {};

        m_isMetronomeRunning = false;
        m_metronomeBeatIndex = 0;
        m_metronomeElapsedMs = 0.0;
        m_metronomeStartCount = 0;
        m_metronomeStopCount = 0;
    }

private:
    std::chrono::milliseconds m_noteDuration;
    std::vector<Note> m_playedNotes;
    std::vector<PlayedGroup> m_playedMelodies;
    std::vector<PlayedGroup> m_playedChords;

    // Les paires « ce qu'il a joue, puis la reponse » - voir PlayedChordPair.
    std::vector<PlayedChordPair> m_chordPairs;

    // La position du son, posee par un test : voir setPlayedMilliseconds.
    std::chrono::milliseconds m_playedMilliseconds{ 0 };
    std::vector<PlayedOverDrone> m_melodiesOverDrones;
    std::vector<PlayedPhraseOverDrone> m_phrasesOverDrones;
    int m_mistakeCueCount{ 0 };
    int m_dogBarkCount{ 0 };

    int m_scoreTickCount{ 0 };
    int m_lastScoreTickProgress{ 0 };
    int m_victoryFanfareCount{ 0 };
    int m_stopCount{ 0 };
    int m_metronomeClickCount{ 0 };
    int m_accentedClickCount{ 0 };
    std::array<int, DRUM_COUNT> m_drumCounts{};
    std::vector<double> m_drumPositions;

    bool m_isMetronomeRunning{ false };
    double m_metronomeBpm{ 90.0 };
    int m_metronomeBeatsPerBar{ 4 };
    std::int64_t m_metronomeBeatIndex{ 0 };
    double m_metronomeElapsedMs{ 0.0 };
    int m_metronomeStartCount{ 0 };
    int m_metronomeStopCount{ 0 };
};

}    // namespace musichien::domain
