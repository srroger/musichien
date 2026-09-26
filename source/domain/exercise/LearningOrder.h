#pragma once

// =====================================================================================================================
// Musichien - LearningOrder
//
// The order in which the intervals are taught. It is what decides the PALETTE of a beginner: the
// first N intervals of this list, N growing with the experience of the player.
//
// This is a MUSICAL decision, not a technical one, and it is written down here so that it can be
// argued with and tuned rather than buried inside a screen. The principle is contrast: the first two
// intervals are the ones nobody confuses, and the list narrows the gap as it goes.
//
//   * an octave and a fifth are the two intervals a beginner hears without any training at all, which
//     is exactly why they are the right place to start: the very first session must be a success;
//   * the third comes next, and it brings major and minor TOGETHER, because hearing one without the
//     other is not knowing either;
//   * then the fourth, and the seconds, where telling apart one and two semitones is the real work;
//   * the sixths and the sevenths last, because they are the hardest to hold on to;
//   * the tritone sits at the end of the simple intervals: it has no stable character to lean on;
//   * the compound intervals come after all of them, and they are the second lesson of the same
//     colours: a tenth is a third heard one octave higher.
//
// Everything the domain supports appears in the list EXACTLY ONCE. That is the invariant a test
// checks, because a hand written list is a list that drifts: an interval missing from it would be an
// interval the player could never be taught, and nothing else would report it.
// =====================================================================================================================

#include "domain/music/Interval.h"

#include <cstddef>
#include <span>
#include <vector>

namespace musichien::domain
{

// Every interval the application supports, ordered from the most contrasted to the most confusable.
[[nodiscard]] std::span<const Interval> learningOrderIntervals();

// The palette of a player who knows the first p_intervalCount intervals of the order.
//
// The result always contains at least one interval: a session without a palette could not ask a
// single question. Asking for more intervals than the order holds returns the whole order.
[[nodiscard]] std::vector<Interval> beginnerPalette( std::size_t p_intervalCount );

}    // namespace musichien::domain
