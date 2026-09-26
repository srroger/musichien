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

    void playMistakeCue() override
    {
        ++m_mistakeCueCount;
    }

    void stopAll() override
    {
        ++m_stopCount;
    }

    [[nodiscard]] std::chrono::milliseconds noteDuration() const override { return m_noteDuration; }

    // -----------------------------------------------------------------------------------------------------------------
    // Observability, for the assertions of a test
    // -----------------------------------------------------------------------------------------------------------------
    [[nodiscard]] const std::vector<Note> & playedNotes() const noexcept { return m_playedNotes; }
    [[nodiscard]] const std::vector<PlayedGroup> & playedMelodies() const noexcept { return m_playedMelodies; }
    [[nodiscard]] const std::vector<PlayedGroup> & playedChords() const noexcept { return m_playedChords; }
    [[nodiscard]] int mistakeCueCount() const noexcept { return m_mistakeCueCount; }
    [[nodiscard]] int stopCount() const noexcept { return m_stopCount; }

    void clear()
    {
        m_playedNotes.clear();
        m_playedMelodies.clear();
        m_playedChords.clear();
        m_mistakeCueCount = 0;
        m_stopCount = 0;
    }

private:
    std::chrono::milliseconds m_noteDuration;
    std::vector<Note> m_playedNotes;
    std::vector<PlayedGroup> m_playedMelodies;
    std::vector<PlayedGroup> m_playedChords;
    int m_mistakeCueCount{ 0 };
    int m_stopCount{ 0 };
};

}    // namespace musichien::domain
