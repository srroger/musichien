#include "domain/exercise/PlayerLevel.h"

#include "domain/exercise/LearningOrder.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <initializer_list>

namespace musichien::domain
{

// ---------------------------------------------------------------------------------------------------------------------
// The level of the player
//
// A level answers one question - "where does this player start?" - and it must answer it without ever making
// the game unplayable. Those are the two things tested here.
// ---------------------------------------------------------------------------------------------------------------------

namespace
{

constexpr std::initializer_list<PlayerLevel> EVERY_LEVEL{ PlayerLevel::Beginner,
                                                          PlayerLevel::Fluent,
                                                          PlayerLevel::Advanced,
                                                          PlayerLevel::BeyondTheOctave };

}    // namespace

TEST( PlayerLevelTest, a_level_widens_the_palette_the_player_starts_with )
{
    const std::size_t beginnerPalette = sessionSettingsFor( PlayerLevel::Beginner ).startingPaletteSize;
    const std::size_t fluentPalette = sessionSettingsFor( PlayerLevel::Fluent ).startingPaletteSize;
    const std::size_t advancedPalette = sessionSettingsFor( PlayerLevel::Advanced ).startingPaletteSize;

    // The order is what the level IS: a player who says he is further along must not be handed the beginner's
    // two intervals, or the first session is a formality and the application is closed before the third
    // question.
    EXPECT_LT( beginnerPalette, fluentPalette );
    EXPECT_LT( fluentPalette, advancedPalette );

    // And "I know them up to the octave" is not a figure of speech: it is literally every SIMPLE interval,
    // which is what the learning order holds first.
    EXPECT_EQ( 12U, advancedPalette );
    EXPECT_LE( advancedPalette, learningOrderIntervals().size() );
}

TEST( PlayerLevelTest, every_level_produces_a_playable_session )
{
    for( const PlayerLevel level : EVERY_LEVEL )
    {
        const SessionSettings settings = sessionSettingsFor( level );

        // A palette of zero or one could not ask a question at all. That is the one thing a level must never
        // do, whatever it is tuned for.
        EXPECT_GE( settings.startingPaletteSize, 2U );
        EXPECT_LE( settings.startingPaletteSize, learningOrderIntervals().size() );

        EXPECT_GE( settings.successesBeforeWidening, 1U );

        // And a level changes WHERE a player starts, never HOW the game is played: same ten questions, same
        // lives, same scoring.
        EXPECT_EQ( SessionSettings{}.questionCount, settings.questionCount );
        EXPECT_EQ( SessionSettings{}.lives, settings.lives );
    }
}

TEST( PlayerLevelTest, an_unknown_level_is_the_beginner_one )
{
    // Reading a preference means reading a file a curious player can edit. A value that is not a level must
    // cost him a setting, never a crash on start up.
    EXPECT_EQ( PlayerLevel::BeyondTheOctave, playerLevelFromIndex( 3 ) );
    EXPECT_EQ( PlayerLevel::Beginner, playerLevelFromIndex( 99 ) );

    EXPECT_EQ( PlayerLevel::Advanced, playerLevelFromIndex( 2 ) );
}

}    // namespace musichien::domain
