#include "domain/exercise/AnswerGrid.h"

#include "domain/exercise/LearningOrder.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <set>
#include <span>
#include <vector>

namespace musichien::domain
{

// ---------------------------------------------------------------------------------------------------------------------
// The wrong answers
//
// Every test here is about the same idea: an exercise is only as good as its wrong answers. A grid the
// player can solve by elimination teaches nothing, so what is checked is that the wrong answers are
// always CLOSE, always inside the palette, and never a duplicate.
// ---------------------------------------------------------------------------------------------------------------------

namespace
{

// The seed used by every draw: the point of a fixed seed is that a failure can be replayed exactly.
constexpr std::uint32_t TEST_SEED = 20260926;

// Every point of the supported range, so that the properties are checked on the whole domain rather
// than on a couple of friendly examples.
constexpr std::array<std::int32_t, SUPPORTED_INTERVAL_COUNT> EVERY_SUPPORTED_DISTANCE{
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 };

[[nodiscard]] bool offers( const std::vector<Interval> & p_choices, const Interval & p_interval )
{
    return std::ranges::find( p_choices, p_interval ) != p_choices.end();
}

// The plausibility distance the pool of candidates stops at, computed from the rule itself rather than
// written as a number: it is what says "this wrong answer was close enough to be offered".
[[nodiscard]] std::int32_t poolBoundaryFor( const Interval & p_target )
{
    std::vector<std::int32_t> distances;

    for( const Interval & candidate : allSupportedIntervals() )
    {
        if( !( candidate == p_target ) )
        {
            distances.push_back( AnswerGrid::plausibilityDistance( p_target, candidate ) );
        }
    }

    std::ranges::sort( distances );

    return distances.at( AnswerGrid::DISTRACTOR_POOL_SIZE - 1 );
}

// The interval that is not the right answer.
[[nodiscard]] Interval distractorOf( const std::vector<Interval> & p_choices, const Interval & p_target )
{
    const auto distractor = std::ranges::find_if( p_choices,
                                                  [&p_target]( const Interval & p_choice ) { return !( p_choice == p_target ); } );

    return *distractor;
}

}    // namespace

TEST( AnswerGridTest, the_right_answer_is_always_one_of_the_choices )
{
    std::mt19937 randomEngine{ TEST_SEED };

    const std::span<const Interval> palette = learningOrderIntervals();

    for( const std::int32_t distance : EVERY_SUPPORTED_DISTANCE )
    {
        const Interval target = intervalFromSemitones( distance );

        const std::vector<Interval> choices = AnswerGrid::build( palette, target, 4, randomEngine );

        EXPECT_TRUE( offers( choices, target ) ) << "the grid forgets the right answer for "
                                                 << target.identifier();
    }
}

TEST( AnswerGridTest, a_grid_never_offers_the_same_interval_twice )
{
    std::mt19937 randomEngine{ TEST_SEED };

    const std::span<const Interval> palette = learningOrderIntervals();

    for( const std::int32_t distance : EVERY_SUPPORTED_DISTANCE )
    {
        const Interval target = intervalFromSemitones( distance );

        const std::vector<Interval> choices = AnswerGrid::build( palette, target, 6, randomEngine );

        std::set<std::int32_t> distances;
        for( const Interval & choice : choices )
        {
            distances.insert( choice.semitones() );
        }

        EXPECT_EQ( choices.size(), distances.size() );
    }
}

TEST( AnswerGridTest, a_grid_never_leaves_the_palette )
{
    std::mt19937 randomEngine{ TEST_SEED };

    // A player who knows two intervals must never be offered a third one: a question that cannot be
    // answered is not a hard question, it is a bug.
    const std::vector<Interval> palette = beginnerPalette( 3 );

    for( const Interval & target : palette )
    {
        const std::vector<Interval> choices = AnswerGrid::build( palette, target, 6, randomEngine );

        EXPECT_EQ( palette.size(), choices.size() );

        for( const Interval & choice : choices )
        {
            EXPECT_TRUE( offers( palette, choice ) );
        }
    }
}

TEST( AnswerGridTest, a_wrong_answer_is_always_a_close_interval )
{
    const std::span<const Interval> palette = learningOrderIntervals();

    // Every target, and not just a friendly one: the property has to hold on the whole domain. An
    // interval at the very edge of the range is where a rule like this usually breaks.
    for( const std::int32_t distance : EVERY_SUPPORTED_DISTANCE )
    {
        std::mt19937 randomEngine{ TEST_SEED };

        const Interval target = intervalFromSemitones( distance );

        const std::vector<Interval> choices = AnswerGrid::build( palette, target, AnswerGrid::MINIMUM_CHOICE_COUNT, randomEngine );

        ASSERT_EQ( 2, choices.size() );

        const Interval distractor = distractorOf( choices, target );

        // The wrong answer is drawn from the closest intervals, and never from the whole palette: a
        // question whose wrong answers are all absurd is a question the player solves by elimination,
        // and elimination is not what this application trains.
        EXPECT_LE( AnswerGrid::plausibilityDistance( target, distractor ), poolBoundaryFor( target ) )
          << "a grid of two offered a far away wrong answer for " << target.identifier();
    }
}

TEST( AnswerGridTest, the_class_counts_as_much_as_the_size )
{
    const Interval majorThird = intervalFromSemitones( 4 );
    const Interval minorThird = intervalFromSemitones( 3 );
    const Interval majorTenth = intervalFromSemitones( 16 );

    // One semitone apart: the mistake everybody makes first.
    EXPECT_EQ( 1, AnswerGrid::plausibilityDistance( majorThird, minorThird ) );

    // An octave and a second apart, and yet the SAME colour: this is the octave confusion, and it is a
    // mistake a learner really makes. A rule based on size alone would score it 12 and rank it below a
    // unison.
    EXPECT_EQ( 0, AnswerGrid::plausibilityDistance( majorThird, majorTenth ) );
}

TEST( AnswerGridTest, the_same_seed_draws_the_same_grid )
{
    const std::span<const Interval> palette = learningOrderIntervals();
    const Interval target = intervalFromSemitones( 7 );

    std::mt19937 firstEngine{ TEST_SEED };
    std::mt19937 secondEngine{ TEST_SEED };

    const std::vector<Interval> firstGrid = AnswerGrid::build( palette, target, 4, firstEngine );
    const std::vector<Interval> secondGrid = AnswerGrid::build( palette, target, 4, secondEngine );

    // Determinism is what makes every rule of this project testable, and it is a property of the code
    // rather than a happy accident: the engine is provided, never created inside.
    EXPECT_EQ( firstGrid, secondGrid );
}

TEST( AnswerGridTest, a_palette_of_two_offers_two_choices )
{
    std::mt19937 randomEngine{ TEST_SEED };

    const std::vector<Interval> palette = beginnerPalette( 2 );

    const std::vector<Interval> choices = AnswerGrid::build( palette, palette.front(), 6, randomEngine );

    EXPECT_EQ( 2, choices.size() );
}

TEST( AnswerGridTest, a_grid_asking_for_one_choice_still_offers_two )
{
    std::mt19937 randomEngine{ TEST_SEED };

    const std::span<const Interval> palette = learningOrderIntervals();

    // A single choice would not be a question: the floor belongs to the rule, not to the caller.
    const std::vector<Interval> choices = AnswerGrid::build( palette, intervalFromSemitones( 7 ), 1, randomEngine );

    EXPECT_EQ( AnswerGrid::MINIMUM_CHOICE_COUNT, choices.size() );
}

TEST( AnswerGridTest, the_grid_is_laid_out_in_circle_of_fifths_order )
{
    // Roger : "les intervalles places au bon endroit du cercle". La position d'un bouton doit vouloir dire
    // quelque chose, et elle ne le peut que si elle ne change jamais : c'est ce que ce test protege.
    //
    // Une quinte fait sept demi-tons, donc l'ordre attendu est do, sol, re, la, mi, si, fa diese...
    const std::vector<Interval> palette{ Interval{ 0 }, Interval{ 1 }, Interval{ 2 }, Interval{ 3 }, Interval{ 4 }, Interval{ 5 }, Interval{ 6 }, Interval{ 7 }, Interval{ 8 }, Interval{ 9 }, Interval{ 10 }, Interval{ 11 }, Interval{ 12 } };

    // Plusieurs tirages : un ordre qui ne tient qu'avec une graine serait un ordre qui ne tient pas.
    for( std::uint32_t seed = 1; seed <= 20; ++seed )
    {
        std::mt19937 randomEngine{ seed };

        const std::vector<Interval> choices = AnswerGrid::build( palette, Interval{ 7 }, 6, randomEngine );

        std::vector<std::int32_t> positions;

        std::ranges::transform( choices, std::back_inserter( positions ), []( const Interval & p_choice ) {
            return ( 7 * p_choice.intervalClass() ) % 12;
        } );

        EXPECT_TRUE( std::ranges::is_sorted( positions ) ) << "graine " << seed;
    }
}

TEST( AnswerGridTest, a_choice_that_disappears_leaves_a_hole_and_moves_nothing )
{
    // La grille se resserre a chaque erreur : un leurre disparait. La place des AUTRES ne doit pas bouger d'un
    // pouce - c'est toute la difference entre une carte et une liste, et c'est ce qui casse en silence si on
    // range les boutons par position dans la liste plutot que par classe d'intervalle.
    const std::vector<Interval> full{ Interval{ 7 }, Interval{ 4 } };
    const std::vector<Interval> narrowed{ Interval{ 7 } };

    const auto fullLayout = layoutOnCircle( full );
    const auto narrowedLayout = layoutOnCircle( narrowed );

    const std::size_t fifthSlot = circleOfFifthsSlot( Interval{ 7 } );
    const std::size_t thirdSlot = circleOfFifthsSlot( Interval{ 4 } );

    // Deux intervalles differents ne tombent jamais sur la meme case.
    EXPECT_NE( fifthSlot, thirdSlot );

    // La quinte est a la meme place dans les deux cas...
    ASSERT_TRUE( fullLayout.at( fifthSlot ).has_value() );
    EXPECT_TRUE( narrowedLayout.at( fifthSlot ).has_value() );
    EXPECT_EQ( 7, narrowedLayout.at( fifthSlot )->semitones() );

    // ...et la tierce laisse simplement sa case VIDE.
    EXPECT_TRUE( fullLayout.at( thirdSlot ).has_value() );
    EXPECT_FALSE( narrowedLayout.at( thirdSlot ).has_value() );
}

}    // namespace musichien::domain