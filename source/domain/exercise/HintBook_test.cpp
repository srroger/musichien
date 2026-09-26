#include "domain/exercise/HintBook.h"

#include "domain/music/Interval.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <utility>

namespace musichien::domain
{

// ---------------------------------------------------------------------------------------------------------------------
// The book of memory hints
//
// A hint is content, so what is tested here is the LOOKUP: which hint answers which interval, and what
// happens when there is none. The behaviour of the exercise - WHEN a hint is shown - belongs to the
// session, and is tested there.
// ---------------------------------------------------------------------------------------------------------------------

namespace
{

[[nodiscard]] IntervalHint hintWithLabel( std::string p_label )
{
    return IntervalHint{ std::move( p_label ), {} };
}

}    // namespace

TEST( HintBookTest, a_hint_is_found_by_distance_and_direction )
{
    HintBook hintBook;

    hintBook.add( Interval{ 7 }, IntervalDirection::Ascending, hintWithLabel( "ascending fifth" ) );
    hintBook.add( Interval{ 7 }, IntervalDirection::Descending, hintWithLabel( "descending fifth" ) );

    const std::optional<IntervalHint> ascending = hintBook.hintFor( Interval{ 7 },
                                                                    IntervalDirection::Ascending );

    ASSERT_TRUE( ascending.has_value() );
    EXPECT_EQ( "ascending fifth", ascending->label );

    // The direction is part of the question, so it is part of the key: the same distance heard the other
    // way round is a different thing to remember.
    const std::optional<IntervalHint> descending = hintBook.hintFor( Interval{ 7 },
                                                                     IntervalDirection::Descending );

    ASSERT_TRUE( descending.has_value() );
    EXPECT_EQ( "descending fifth", descending->label );

    EXPECT_EQ( 2U, hintBook.hintCount() );
}

TEST( HintBookTest, a_harmonic_interval_uses_the_ascending_hint )
{
    HintBook hintBook;

    hintBook.add( Interval{ 4 }, IntervalDirection::Ascending, hintWithLabel( "ascending third" ) );

    // A harmonic interval is heard from the bottom up, and the hints are melodies: asking the content
    // for a third case would mean writing every hint twice.
    const std::optional<IntervalHint> harmonic = hintBook.hintFor( Interval{ 4 },
                                                                   IntervalDirection::Harmonic );

    ASSERT_TRUE( harmonic.has_value() );
    EXPECT_EQ( "ascending third", harmonic->label );
}

TEST( HintBookTest, an_interval_without_a_hint_reports_itself_absent )
{
    HintBook hintBook;

    hintBook.add( Interval{ 11 }, IntervalDirection::Ascending, hintWithLabel( "ascending seventh" ) );

    // The seventh descending has no hint in the content file yet, and that must be an absence rather
    // than an error: content is always incomplete.
    EXPECT_FALSE( hintBook.hintFor( Interval{ 11 }, IntervalDirection::Descending ).has_value() );
    EXPECT_FALSE( hintBook.hintFor( Interval{ 5 }, IntervalDirection::Ascending ).has_value() );
}

TEST( HintBookTest, the_songs_come_with_the_hint )
{
    HintBook hintBook;

    hintBook.add( Interval{ 7 },
                  IntervalDirection::Ascending,
                  IntervalHint{ "🚗 Retour vers le Futur", { "Retour vers le Futur", "Favorite Things" } } );

    const std::optional<IntervalHint> hint = hintBook.hintFor( Interval{ 7 },
                                                               IntervalDirection::Ascending );

    ASSERT_TRUE( hint.has_value() );

    // The list is kept whole even though only the label is displayed today: varying the hint is the whole
    // reason it exists.
    ASSERT_EQ( 2U, hint->songs.size() );
    EXPECT_EQ( "Retour vers le Futur", hint->songs.at( 0 ) );
    EXPECT_EQ( "Favorite Things", hint->songs.at( 1 ) );
}

TEST( HintBookTest, a_second_entry_for_the_same_pair_replaces_the_first )
{
    HintBook hintBook;

    hintBook.add( Interval{ 3 }, IntervalDirection::Ascending, hintWithLabel( "first" ) );
    hintBook.add( Interval{ 3 }, IntervalDirection::Ascending, hintWithLabel( "second" ) );

    // Content files get edited. An edit that silently did nothing would be a mystery for whoever made it.
    EXPECT_EQ( 1U, hintBook.hintCount() );

    const std::optional<IntervalHint> hint = hintBook.hintFor( Interval{ 3 },
                                                               IntervalDirection::Ascending );

    ASSERT_TRUE( hint.has_value() );
    EXPECT_EQ( "second", hint->label );
}

}    // namespace musichien::domain