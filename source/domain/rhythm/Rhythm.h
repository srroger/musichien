// =====================================================================================================================
// Musichien - Rhythm
//
// The other half of the ear: pitch says WHAT, rhythm says WHEN. This is the pure, testable core of the rhythm game -
// a metronome's beats and the judgement of a tap against them. No Qt, no clock, no sound: times go in as plain
// numbers, a judgement comes out.
//
// The windows are deliberately generous: the first loop must FEEL generous, and they will tighten as the player
// improves - the same logic as the guided mode that turns itself off. See note 05 of the vault, section 9.
// =====================================================================================================================

#pragma once

#include <cstddef>

namespace musichien::domain
{

// How close a tap has to land to a beat to count, in milliseconds. Large on purpose.
inline constexpr double PERFECT_WINDOW_MS = 80.0;
inline constexpr double GOOD_WINDOW_MS = 200.0;

// How well a tap landed.
enum class HitQuality
{
    Perfect,
    Good,
    Miss
};

// The length of one beat, in milliseconds, at a tempo in beats per minute.
[[nodiscard]] double beatDurationMs( double p_bpm ) noexcept;

// The time of the nth beat (0 is the first), in milliseconds from the start of the metronome.
[[nodiscard]] double beatTimeMs( double p_bpm, std::size_t p_beatIndex ) noexcept;

// The quality of a tap that landed at p_tapMs, against the nearest beat of a metronome at p_bpm.
//
// The nearest beat is the one whose time is closest to the tap; a tap halfway between two beats is a Miss whichever
// way it is measured, and the function says so honestly.
[[nodiscard]] HitQuality judgeTap( double p_tapMs, double p_bpm ) noexcept;

}    // namespace musichien::domain
