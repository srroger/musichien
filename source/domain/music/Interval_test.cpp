#include "domain/music/Interval.h"
#include "domain/music/Note.h"

#include <gtest/gtest.h>

#include <vector>

namespace musichien::domain
{

// ---------------------------------------------------------------------------------------------------------------------
// Note
//
// GoogleTest test names are written in snake_case of words because they read as sentences in the
// test report. clang-tidy is therefore disabled on test targets only; see
// cmake/musichienFunctionAddTest.cmake.
// ---------------------------------------------------------------------------------------------------------------------

TEST( NoteTest, A4_is_the_reference_note )
{
    constexpr Note referenceNote{ REFERENCE_MIDI_NUMBER };

    EXPECT_DOUBLE_EQ( REFERENCE_FREQUENCY_HZ, referenceNote.frequencyHz() );
    EXPECT_EQ( "A4", referenceNote.name() );
    EXPECT_EQ( 4, referenceNote.octave() );
}

TEST( NoteTest, an_octave_doubles_the_frequency )
{
    constexpr Note lowerNote{ 60 };    // C4
    constexpr Note upperNote{ 72 };    // C5

    EXPECT_DOUBLE_EQ( 2.0 * lowerNote.frequencyHz(), upperNote.frequencyHz() );
}

TEST( NoteTest, transposition_stays_inside_the_playable_range )
{
    constexpr Note lowestNote{ Note::MINIMUM_MIDI_NUMBER };

    // Transposing down by an octave must clamp instead of wrapping around to a high note.
    const Note transposedNote = lowestNote.transposedBy( -SEMITONES_PER_OCTAVE );

    EXPECT_EQ( Note::MINIMUM_MIDI_NUMBER, transposedNote.midiNumber() );
    EXPECT_TRUE( transposedNote.isValid() );
}

// ---------------------------------------------------------------------------------------------------------------------
// Interval
// ---------------------------------------------------------------------------------------------------------------------

TEST( IntervalTest, a_fifth_is_seven_semitones )
{
    constexpr Note tonic{ 60 };      // C4
    constexpr Note dominant{ 67 };   // G4

    const Interval interval = intervalBetween( tonic, dominant );

    EXPECT_EQ( 7, interval.semitones() );
    EXPECT_EQ( IntervalQuality::Perfect, interval.quality() );
    EXPECT_EQ( "P5", interval.identifier() );
    EXPECT_EQ( "Perfect fifth", interval.name() );
}

TEST( IntervalTest, a_descending_fifth_is_still_a_fifth )
{
    constexpr Note dominant{ 67 };
    constexpr Note tonic{ 60 };

    const Interval interval = intervalBetween( dominant, tonic );

    EXPECT_EQ( "P5", interval.identifier() );
    EXPECT_TRUE( interval.isSimple() );
}

TEST( IntervalTest, every_simple_interval_is_named )
{
    // A ranges pipeline rather than a loop: the intent is visible at a glance.
    const std::array< Interval, 12 > & simpleIntervals = allSimpleIntervals();

    for ( const Interval & interval : simpleIntervals )
    {
        EXPECT_TRUE( interval.isSimple() ) << "interval " << interval.semitones() << " is not simple";
        EXPECT_FALSE( interval.name().empty() );
        EXPECT_FALSE( interval.identifier().empty() );
    }
}

TEST( IntervalTest, intervals_of_a_melody_are_chained )
{
    const std::vector< Note > melody{ Note{ 60 }, Note{ 64 }, Note{ 67 } };    // C, E, G

    const std::vector< Interval > intervals = intervalsOf( melody );

    ASSERT_EQ( 2, intervals.size() );
    EXPECT_EQ( "M3", intervals.at( 0 ).identifier() );
    EXPECT_EQ( "m3", intervals.at( 1 ).identifier() );
}

TEST( IntervalTest, a_single_note_contains_no_interval )
{
    const std::vector< Note > singleNote{ Note{ 60 } };

    EXPECT_TRUE( intervalsOf( singleNote ).empty() );
}

}    // namespace musichien::domain
