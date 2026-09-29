#pragma once

// =====================================================================================================================
// Musichien - PlayerLevel
//
// What the player already knows, in three rough steps.
//
// ---------------------------------------------------------------------------------------------------------------------
// Why it exists, and why it is not a setting
//
// The first session must not be a formality. A player who already hears a fifth and a third would otherwise
// be asked to tell two intervals apart for three questions in a row - and would close the application before
// discovering the rest. A player who already knows the intervals up to the octave, and for whom minor and major chords
// are no longer a problem, has to be asked something else.
//
// It is deliberately NOT a setting, though: it is the first piece of the PROFILE - the thing the application
// remembers about a player - and the difference matters. A setting is changed on a whim and explained in a
// screen full of them; a profile is asked once, and puts the player on a path.
//
// The level does not change the RULES of the game: same ten questions, same lives, same scoring. It changes
// where the player starts, and how fast the palette widens. That is all, and that is enough.
// =====================================================================================================================

#include "domain/exercise/ExerciseSession.h"

#include <cstddef>
#include <string_view>

namespace musichien::domain
{

enum class PlayerLevel : std::size_t
{
    // Never played: two intervals nobody confuses, widening slowly.
    Beginner = 0,

    // Hears the obvious colours: a third, a fourth, a fifth.
    Fluent = 1,

    // Hears every simple interval, and wants the whole palette at once.
    Advanced = 2,

    // Wants the intervals BEYOND the octave as well: the ninth, the tenth, and the rest of the same colours
    // heard one octave higher. The learning order puts them all at the end, so the palette of this level is the
    // twelve simple intervals followed by the first compound ones.
    BeyondTheOctave = 3,

    // The whole palette at once, and no help at all.
    //
    // This is the level where the player stops being taught and starts MEASURING himself: every interval the
    // application knows is on the map from the first question, and nothing is ever handed to him - neither the
    // hint that nudges on the first mistake, nor the button that gives the answer away after three.
    //
    // Nothing else changes. Same ten questions, same lives, same scoring: a level decides where the player
    // starts and what he is handed, never how the game is counted.
    Master = 4
};

// How many levels there are, which is what a screen offering them needs to know.
inline constexpr std::size_t PLAYER_LEVEL_COUNT = 5;

// The rules of a session for a player of this level.
[[nodiscard]] SessionSettings sessionSettingsFor( PlayerLevel p_level );

// The level a stored number means, and Beginner for anything that is not a level.
//
// Reading a preference is reading a file a human can edit, and a corrupted value must cost the player his
// settings, never a crash on start up.
[[nodiscard]] PlayerLevel playerLevelFromIndex( std::size_t p_index ) noexcept;

}    // namespace musichien::domain
