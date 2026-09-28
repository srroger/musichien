#include "domain/rhythm/Rhythm.h"

#include <gtest/gtest.h>

#include <cmath>

namespace musichien::domain
{

TEST( RhythmTest, a_beat_is_one_minute_divided_by_the_tempo )
{
    EXPECT_NEAR( 1000.0, beatDurationMs( 60.0 ), 1e-9 );    // 60 bpm: one second per beat
    EXPECT_NEAR( 500.0, beatDurationMs( 120.0 ), 1e-9 );    // 120 bpm: half a second
    EXPECT_NEAR( 600.0, beatDurationMs( 100.0 ), 1e-9 );    // 100 bpm: 0.6 second
}

TEST( RhythmTest, a_tap_on_the_beat_is_perfect )
{
    EXPECT_EQ( HitQuality::Perfect, judgeTap( 0.0, 120.0 ) );
    EXPECT_EQ( HitQuality::Perfect, judgeTap( 500.0, 120.0 ) );
    EXPECT_EQ( HitQuality::Perfect, judgeTap( 1500.0, 120.0 ) );
}

TEST( RhythmTest, a_tap_near_the_beat_is_good )
{
    // A hundred milliseconds late: inside the good window, outside the perfect one.
    EXPECT_EQ( HitQuality::Good, judgeTap( 100.0, 120.0 ) );

    // And the same distance early.
    EXPECT_EQ( HitQuality::Good, judgeTap( 500.0 - 100.0, 120.0 ) );
}

TEST( RhythmTest, a_tap_between_two_beats_is_a_miss )
{
    // Halfway between the first and second beat of a 500 ms metronome.
    EXPECT_EQ( HitQuality::Miss, judgeTap( 250.0, 120.0 ) );
}

TEST( RhythmTest, the_windows_are_the_boundaries )
{
    EXPECT_EQ( HitQuality::Perfect, judgeTap( PERFECT_WINDOW_MS, 120.0 ) );
    EXPECT_EQ( HitQuality::Good, judgeTap( PERFECT_WINDOW_MS + 1.0, 120.0 ) );
    EXPECT_EQ( HitQuality::Good, judgeTap( GOOD_WINDOW_MS, 120.0 ) );
    EXPECT_EQ( HitQuality::Miss, judgeTap( GOOD_WINDOW_MS + 1.0, 120.0 ) );
}

TEST( RhythmTest, a_still_metronome_judges_everything_as_a_miss )
{
    EXPECT_EQ( HitQuality::Miss, judgeTap( 0.0, 0.0 ) );
}

}    // namespace musichien::domain
