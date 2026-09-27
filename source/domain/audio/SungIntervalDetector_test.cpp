#include "domain/audio/SungIntervalDetector.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>

namespace musichien::domain
{

// ---------------------------------------------------------------------------------------------------------------------
// The sung interval
//
// What is tested here is a JUDGEMENT, not a signal: given a series of readings, which notes did the singer mean? The
// readings below are synthetic, so the rule can be checked without a microphone, a singer or a sound card.
// ---------------------------------------------------------------------------------------------------------------------

namespace
{

constexpr std::int32_t FRAME_MILLISECONDS = 20;
constexpr double REFERENCE_PITCH_HZ = 440.0;

// Holds a frequency for a while, frame by frame, the way the microphone would report it.
void hold( SungIntervalDetector & p_detector, double p_frequencyHz, std::int32_t p_milliseconds )
{
    for( std::int32_t elapsed = 0; elapsed < p_milliseconds; elapsed += FRAME_MILLISECONDS )
    {
        p_detector.update( p_frequencyHz, REFERENCE_PITCH_HZ, FRAME_MILLISECONDS );
    }
}

// The frequency of a MIDI note, so that the tests read in NOTES rather than in hertz.
[[nodiscard]] double frequencyOf( std::int32_t p_midiNumber )
{
    return REFERENCE_PITCH_HZ * std::pow( 2.0, static_cast<double>( p_midiNumber - 69 ) / 12.0 );
}

}    // namespace

TEST( SungIntervalDetectorTest, silence_gives_nothing )
{
    SungIntervalDetector detector;

    hold( detector, 0.0, 1000 );

    EXPECT_EQ( 0, detector.reading().firstMidiNumber );
    EXPECT_FALSE( detector.reading().hasInterval() );
}

TEST( SungIntervalDetectorTest, a_held_note_becomes_the_first_one )
{
    SungIntervalDetector detector;

    hold( detector, frequencyOf( 69 ), 300 );    // A4

    EXPECT_EQ( 69, detector.reading().firstMidiNumber );
    EXPECT_FALSE( detector.reading().hasInterval() );
}

TEST( SungIntervalDetectorTest, a_note_held_too_briefly_does_not_count )
{
    SungIntervalDetector detector;

    hold( detector, frequencyOf( 69 ), 100 );    // moins d'un quart de seconde

    EXPECT_EQ( 0, detector.reading().firstMidiNumber );
}

TEST( SungIntervalDetectorTest, two_held_notes_give_the_interval )
{
    SungIntervalDetector detector;

    hold( detector, frequencyOf( 69 ), 300 );    // A4
    hold( detector, frequencyOf( 76 ), 300 );    // E5

    ASSERT_TRUE( detector.reading().hasInterval() );
    EXPECT_EQ( 7, detector.reading().semitones() );    // une quinte, montante
}

TEST( SungIntervalDetectorTest, a_falling_interval_is_negative )
{
    SungIntervalDetector detector;

    hold( detector, frequencyOf( 76 ), 300 );
    hold( detector, frequencyOf( 69 ), 300 );

    ASSERT_TRUE( detector.reading().hasInterval() );
    EXPECT_EQ( -7, detector.reading().semitones() );
}

TEST( SungIntervalDetectorTest, a_note_only_brushed_on_the_way_does_not_count )
{
    SungIntervalDetector detector;

    hold( detector, frequencyOf( 69 ), 300 );
    hold( detector, frequencyOf( 72 ), 80 );    // passe trop vite pour etre une note voulue
    hold( detector, frequencyOf( 76 ), 300 );

    ASSERT_TRUE( detector.reading().hasInterval() );
    EXPECT_EQ( 7, detector.reading().semitones() );
}

TEST( SungIntervalDetectorTest, a_breath_between_the_two_notes_keeps_the_first )
{
    SungIntervalDetector detector;

    hold( detector, frequencyOf( 69 ), 300 );
    hold( detector, 0.0, 200 );                  // le chanteur reprend son souffle
    hold( detector, frequencyOf( 64 ), 300 );    // E4

    ASSERT_TRUE( detector.reading().hasInterval() );
    EXPECT_EQ( -5, detector.reading().semitones() );
}

TEST( SungIntervalDetectorTest, the_answer_does_not_move_once_it_is_found )
{
    SungIntervalDetector detector;

    hold( detector, frequencyOf( 69 ), 300 );
    hold( detector, frequencyOf( 76 ), 300 );
    hold( detector, frequencyOf( 81 ), 300 );    // le chanteur continue : la reponse est deja donnee

    EXPECT_EQ( 7, detector.reading().semitones() );
}

TEST( SungIntervalDetectorTest, a_reset_asks_for_a_new_answer )
{
    SungIntervalDetector detector;

    hold( detector, frequencyOf( 69 ), 300 );
    hold( detector, frequencyOf( 76 ), 300 );

    detector.reset();

    EXPECT_FALSE( detector.reading().hasInterval() );
    EXPECT_EQ( 0, detector.reading().firstMidiNumber );
}

}    // namespace musichien::domain
