#include "infrastructure/content/JsonHintBook.h"

#include "domain/music/Interval.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace musichien::infrastructure
{

// ---------------------------------------------------------------------------------------------------------------------
// Reading the content file
//
// This is the CONTRACT between the code and a file a human edits. Its shape is asserted here rather than
// discovered on the phone, and the cases that matter are the ones a typo produces: a field that is
// missing, a value that is null, and a document that is not JSON at all.
// ---------------------------------------------------------------------------------------------------------------------

namespace
{

// A file with everything in it: two directions for one interval, and none for the other.
constexpr std::string_view COMPLETE_CONTENT = R"({
  "jeu": "Musichien",
  "intervalles": [
    { "id": "5J", "nom": "quinte juste", "demi_tons": 7,
      "ascendant": { "indice": "🚗 Retour vers le Futur", "chansons": ["Retour vers le Futur", "Favorite Things"] },
      "descendant": { "indice": "🛕 Zelda Time Temple", "chansons": ["Zelda Time Temple"] } },
    { "id": "Octave", "nom": "octave", "demi_tons": 12,
      "ascendant": { "indice": "🌈 Over the Rainbow", "chansons": [] },
      "descendant": { "indice": null, "chansons": [] } }
  ]
})";

}    // namespace

TEST( JsonHintBookTest, a_complete_file_is_read_for_both_directions )
{
    const musichien::domain::HintBook hintBook = readHintBook( COMPLETE_CONTENT );

    // Three hints, not four: the octave descending is null in the file, and a null 'indice' is how the
    // content says "not written yet".
    EXPECT_EQ( 3U, hintBook.hintCount() );

    const std::optional<musichien::domain::IntervalHint> ascending =
      hintBook.hintFor( musichien::domain::Interval{ 7 }, musichien::domain::IntervalDirection::Ascending );

    ASSERT_TRUE( ascending.has_value() );
    EXPECT_EQ( "🚗 Retour vers le Futur", ascending->label );
    ASSERT_EQ( 2U, ascending->songs.size() );
    EXPECT_EQ( "Retour vers le Futur", ascending->songs.at( 0 ) );

    const std::optional<musichien::domain::IntervalHint> descending =
      hintBook.hintFor( musichien::domain::Interval{ 7 }, musichien::domain::IntervalDirection::Descending );

    ASSERT_TRUE( descending.has_value() );
    EXPECT_EQ( "🛕 Zelda Time Temple", descending->label );
}

TEST( JsonHintBookTest, a_null_hint_is_an_absence_and_not_an_error )
{
    const musichien::domain::HintBook hintBook = readHintBook( COMPLETE_CONTENT );

    EXPECT_FALSE( hintBook.hintFor( musichien::domain::Interval{ 12 },
                                    musichien::domain::IntervalDirection::Descending )
                    .has_value() );

    // The other direction of the same interval is there: one missing hint costs one hint.
    EXPECT_TRUE( hintBook.hintFor( musichien::domain::Interval{ 12 },
                                   musichien::domain::IntervalDirection::Ascending )
                   .has_value() );
}

TEST( JsonHintBookTest, a_broken_document_gives_an_empty_book_rather_than_a_crash )
{
    // A missing comma must not be able to stop the application from starting: the player loses the hints,
    // not the game.
    EXPECT_EQ( 0U, readHintBook( "{ this is not JSON" ).hintCount() );
    EXPECT_EQ( 0U, readHintBook( "" ).hintCount() );

    // Valid JSON, wrong shape.
    EXPECT_EQ( 0U, readHintBook( R"({ "intervalles": "not a list" })" ).hintCount() );
    EXPECT_EQ( 0U, readHintBook( "{}" ).hintCount() );
}

TEST( JsonHintBookTest, an_interval_without_a_distance_is_skipped )
{
    // 'demi_tons' is the key the domain understands, and 'id' - however readable - is only a label for
    // whoever edits the file. An entry that has the label but not the distance cannot be used.
    constexpr std::string_view CONTENT_MISSING_DISTANCE = R"({
      "intervalles": [
        { "id": "5J", "nom": "quinte juste",
          "ascendant": { "indice": "🚗 Retour vers le Futur", "chansons": [] } },
        { "id": "6m", "nom": "sixte mineure", "demi_tons": 8,
          "ascendant": { "indice": "🎩 The Entertainer", "chansons": [] } }
      ]
    })";

    const musichien::domain::HintBook hintBook = readHintBook( CONTENT_MISSING_DISTANCE );

    EXPECT_EQ( 1U, hintBook.hintCount() );
    EXPECT_TRUE( hintBook.hintFor( musichien::domain::Interval{ 8 },
                                   musichien::domain::IntervalDirection::Ascending )
                   .has_value() );
}

}    // namespace musichien::infrastructure