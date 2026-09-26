#include "domain/exercise/AnswerGrid.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <ranges>

namespace musichien::domain
{

namespace
{

// Distance between two values, never negative. Written out rather than called std::abs: the intent is
// the distance, not the absolute value of a subtraction.
[[nodiscard]] constexpr std::int32_t distanceBetween( std::int32_t p_left, std::int32_t p_right ) noexcept
{
    return ( p_left >= p_right ) ? ( p_left - p_right ) : ( p_right - p_left );
}

}    // namespace

std::int32_t AnswerGrid::plausibilityDistance( const Interval & p_target, const Interval & p_other )
{
    const std::int32_t sizeDistance =
      distanceBetween( p_target.semitones(), p_other.semitones() );

    const std::int32_t classDistance =
      distanceBetween( p_target.intervalClass(), p_other.intervalClass() );

    // The smaller of the two, never the larger: a wrong answer is plausible as soon as it is close in
    // one of the two senses. A minor tenth is a plausible wrong answer for a minor third even though
    // it is an octave away, because it is the same colour.
    return std::min( sizeDistance, classDistance );
}

std::vector<Interval> AnswerGrid::build( std::span<const Interval> p_palette,
                                         const Interval & p_target,
                                         std::size_t p_choiceCount,
                                         std::mt19937 & p_randomEngine )
{
    // Every interval of the palette, except the right answer: a grid never offers the same interval
    // twice, and never offers the answer as one of its own wrong answers.
    std::vector<Interval> candidates;

    std::ranges::copy_if( p_palette, std::back_inserter( candidates ), [&p_target]( const Interval & p_candidate ) { return !( p_candidate == p_target ); } );

    if( candidates.empty() )
    {
        // A palette that holds nothing but the target cannot produce a question. The session is
        // responsible for never asking for one; returning the single choice rather than asserting
        // keeps this function total, and lets a caller fail loudly somewhere it can be handled.
        return std::vector<Interval>{ p_target };
    }

    // Closest first. The order is made deterministic rather than left to the draw: two candidates at
    // the same plausibility must come out in the same order on every machine, otherwise the same seed
    // would produce two different grids.
    std::ranges::sort( candidates,
                       [&p_target]( const Interval & p_left, const Interval & p_right ) {
                           const std::int32_t leftDistance = plausibilityDistance( p_target, p_left );
                           const std::int32_t rightDistance = plausibilityDistance( p_target, p_right );

                           if( leftDistance != rightDistance )
                           {
                               return leftDistance < rightDistance;
                           }

                           return p_left.semitones() < p_right.semitones();
                       } );

    const std::size_t maximumChoiceCount = candidates.size() + 1;

    const std::size_t choiceCount = std::clamp( p_choiceCount, MINIMUM_CHOICE_COUNT, maximumChoiceCount );

    const std::size_t wantedDistractorCount = choiceCount - 1;

    // The draw happens INSIDE the plausible neighbourhood, never across the whole palette: that is
    // what makes the same wrong answer vary from one question to the next without ever becoming
    // absurd.
    candidates.resize( std::min( candidates.size(),
                                 std::max( DISTRACTOR_POOL_SIZE, wantedDistractorCount ) ) );

    // Shuffled so that a grid does not always offer the same wrong answers in the same order: the
    // player must not be able to recognise a question by the shape of the buttons.
    std::ranges::shuffle( candidates, p_randomEngine );

    std::vector<Interval> choices;
    choices.reserve( choiceCount );
    choices.push_back( p_target );

    std::ranges::copy( candidates | std::views::take( wantedDistractorCount ),
                       std::back_inserter( choices ) );

    // And shuffled once more, as a whole: the right answer must not sit in the same place either.
    std::ranges::shuffle( choices, p_randomEngine );

    return choices;
}

}    // namespace musichien::domain
