#include "domain/rhythm/RhythmPattern.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>

namespace musichien::domain
{

TEST( RhythmPatternTest, every_pattern_is_offered_and_in_time_order )
{
    const std::vector<RhythmPattern> & patterns = allRhythmPatterns();

    ASSERT_FALSE( patterns.empty() );

    for( const RhythmPattern & pattern : patterns )
    {
        EXPECT_FALSE( pattern.name().empty() );
        EXPECT_GT( pattern.beatsPerBar(), 0 );
        EXPECT_FALSE( pattern.hits().empty() );

        double previousBeat = -1.0;

        for( const RhythmHit & hit : pattern.hits() )
        {
            // A hit outside the loop could never be reached, and a hit before the previous one would make the
            // comparison of a whole bar wrong.
            EXPECT_GE( hit.beat, 0.0 );
            EXPECT_LT( hit.beat, static_cast<double>( pattern.beatsPerBar() ) );
            EXPECT_GT( hit.beat, previousBeat );

            previousBeat = hit.beat;
        }
    }
}

TEST( RhythmPatternTest, a_tap_on_an_onset_is_exactly_on_it )
{
    for( const RhythmPattern & pattern : allRhythmPatterns() )
    {
        for( const RhythmHit & hit : pattern.hits() )
        {
            EXPECT_DOUBLE_EQ( 0.0, distanceToNearestOnsetInBeats( pattern, hit.beat ) );
        }
    }
}

TEST( RhythmPatternTest, the_loop_wraps_around )
{
    const RhythmPattern & binary = allRhythmPatterns().front();

    // Just after the last beat of the bar, the next thing to come is the downbeat: a tap at 3.9 is 0.1 of a beat from
    // it, not 3.9 away from it.
    EXPECT_NEAR( 0.1, distanceToNearestOnsetInBeats( binary, 3.9 ), 1e-9 );

    // And a position past the end of the loop is folded back into it.
    EXPECT_DOUBLE_EQ( 0.0, distanceToNearestOnsetInBeats( binary, 4.0 ) );
    EXPECT_DOUBLE_EQ( 0.0, distanceToNearestOnsetInBeats( binary, 101.0 ) );
}

TEST( RhythmPatternTest, a_tap_between_two_onsets_is_judged_by_the_nearest )
{
    const RhythmPattern & binary = allRhythmPatterns().front();

    // The binaire has a hit on every beat: half a beat from either side.
    EXPECT_NEAR( 0.5, distanceToNearestOnsetInBeats( binary, 0.5 ), 1e-9 );
    EXPECT_NEAR( 0.5, distanceToNearestOnsetInBeats( binary, 2.5 ), 1e-9 );
}

TEST( RhythmPatternTest, a_syncopated_cell_is_further_from_the_beat )
{
    const RhythmPattern & bossa = allRhythmPatterns().at( 2 );

    // La bossa frappe sur le "et" du deuxieme temps : un tap pile sur le temps 1 tombe a une demi-mesure de frappe de
    // ses voisines (0.0 et 1.5). C'est exactement ce qui rend la syncope difficile - et c'est voulu.
    EXPECT_NEAR( 0.5, distanceToNearestOnsetInBeats( bossa, 1.0 ), 1e-9 );

    // Et ses propres frappes sont exactement dessus.
    EXPECT_DOUBLE_EQ( 0.0, distanceToNearestOnsetInBeats( bossa, 1.5 ) );
}

TEST( RhythmPatternTest, a_cell_without_a_hit_never_reports_a_distance )
{
    const RhythmPattern empty{ "Vide", 4, {} };

    EXPECT_DOUBLE_EQ( 0.0, distanceToNearestOnsetInBeats( empty, 2.0 ) );
}

}    // namespace musichien::domain
