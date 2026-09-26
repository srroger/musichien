#include "domain/exercise/LearningOrder.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <set>
#include <vector>

namespace musichien::domain
{

// ---------------------------------------------------------------------------------------------------------------------
// The learning order
//
// The order is a hand written musical decision, and a hand written list is a list that drifts. These
// tests are what turn it from a comment into an invariant: an interval missing from the order would be
// an interval the player could never be taught, and nothing else in the project would report it.
// ---------------------------------------------------------------------------------------------------------------------

TEST( LearningOrderTest, every_supported_interval_appears_exactly_once )
{
    const std::span<const Interval> order = learningOrderIntervals();

    EXPECT_EQ( SUPPORTED_INTERVAL_COUNT, order.size() );

    // Uniqueness, and not only the count: the same interval twice would mean another one missing, and
    // the count alone would not see it.
    std::set<std::int32_t> distances;
    for( const Interval & interval : order )
    {
        distances.insert( interval.semitones() );
    }

    EXPECT_EQ( SUPPORTED_INTERVAL_COUNT, distances.size() );

    for( const Interval & interval : allSupportedIntervals() )
    {
        EXPECT_NE( order.end(), std::ranges::find( order, interval ) )
          << "the order forgets " << interval.identifier();
    }
}

TEST( LearningOrderTest, the_first_two_intervals_are_the_ones_nobody_confuses )
{
    const std::span<const Interval> order = learningOrderIntervals();

    ASSERT_GE( order.size(), 2 );

    // The first session of a player has to be a success: it starts on the two intervals that are heard
    // without any training at all, the octave and the fifth.
    EXPECT_EQ( 12, order.at( 0 ).semitones() );
    EXPECT_EQ( 7, order.at( 1 ).semitones() );
}

TEST( LearningOrderTest, a_palette_always_holds_at_least_one_interval )
{
    // A session with no interval in it could not ask a single question, so the floor is part of the
    // contract rather than something the caller has to remember.
    EXPECT_EQ( 1, beginnerPalette( 0 ).size() );
}

TEST( LearningOrderTest, asking_for_more_than_the_order_holds_returns_the_whole_order )
{
    const std::vector<Interval> palette = beginnerPalette( 1000 );

    EXPECT_EQ( SUPPORTED_INTERVAL_COUNT, palette.size() );
}

TEST( LearningOrderTest, a_palette_is_a_prefix_of_the_order )
{
    const std::span<const Interval> order = learningOrderIntervals();

    const std::vector<Interval> palette = beginnerPalette( 5 );

    ASSERT_EQ( 5, palette.size() );

    // A prefix, and not a selection: the progression is predictable on purpose, so that a new interval
    // is always "the next one" rather than a surprise.
    for( std::size_t index = 0; index < palette.size(); ++index )
    {
        EXPECT_EQ( order.at( index ), palette.at( index ) );
    }
}

}    // namespace musichien::domain
