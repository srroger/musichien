#include "domain/exercise/LearningOrder.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <vector>

namespace musichien::domain
{

namespace
{

// The order, written once, as distances in semitones rather than as names.
//
// Distances rather than names on purpose: the name of an interval is derived from its distance by the
// domain, so writing the order with distances removes any chance of a name and a distance drifting
// apart here. The comment on each line says which interval it is.
constexpr std::array<std::int32_t, SUPPORTED_INTERVAL_COUNT> LEARNING_ORDER_SEMITONES{
  12,    // P8  - the octave
  7,     // P5  - the fifth
  4,     // M3  - the major third
  3,     // m3  - the minor third
  5,     // P4  - the fourth
  2,     // M2  - the major second
  1,     // m2  - the minor second
  9,     // M6  - the major sixth
  8,     // m6  - the minor sixth
  11,    // M7  - the major seventh
  10,    // m7  - the minor seventh
  6,     // A4  - the tritone
  0,     // P1  - the unison, last of the simple ones
  16,    // M10 - and now the compounds, same colours one octave higher
  15,    // m10
  19,    // P12
  17,    // P11
  14,    // M9
  13,    // m9
  18,    // A11
  21,    // M13
  20,    // m13
  23,    // M14
  22,    // m14
  24,    // P15 - two whole octaves: the widest the application goes
};

// The order as intervals, built once.
//
// Built from the distances above rather than written a second time: there is exactly one place where
// the order lives, and this is the translation of it into the domain's own type.
[[nodiscard]] const std::vector<Interval> & orderedIntervals()
{
    static const std::vector<Interval> ORDER = [] {
        std::vector<Interval> intervals;
        intervals.reserve( LEARNING_ORDER_SEMITONES.size() );

        std::ranges::transform( LEARNING_ORDER_SEMITONES, std::back_inserter( intervals ), []( std::int32_t p_semitones ) { return intervalFromSemitones( p_semitones ); } );

        return intervals;
    }();

    return ORDER;
}

}    // namespace

std::span<const Interval> learningOrderIntervals()
{
    return orderedIntervals();
}

std::vector<Interval> beginnerPalette( std::size_t p_intervalCount )
{
    const std::span<const Interval> order = learningOrderIntervals();

    // Never empty: a palette with no interval in it could not produce a single question, and the
    // caller would have to check. The floor is what makes "how many intervals do you know?" a
    // question that always has an answer.
    const std::size_t intervalCount = std::clamp( p_intervalCount, std::size_t{ 1 }, order.size() );

    const std::span<const Interval> firstIntervals = order.first( intervalCount );

    return std::vector<Interval>{ firstIntervals.begin(), firstIntervals.end() };
}

}    // namespace musichien::domain