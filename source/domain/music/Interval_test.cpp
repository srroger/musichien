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
    constexpr Note tonic{ 60 };       // C4
    constexpr Note dominant{ 67 };    // G4

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
    const std::array<Interval, 12> & simpleIntervals = allSimpleIntervals();

    for( const Interval & interval : simpleIntervals )
    {
        EXPECT_TRUE( interval.isSimple() ) << "interval " << interval.semitones() << " is not simple";
        EXPECT_FALSE( interval.name().empty() );
        EXPECT_FALSE( interval.identifier().empty() );
    }
}

TEST( IntervalTest, intervals_of_a_melody_are_chained )
{
    const std::vector<Note> melody{ Note{ 60 }, Note{ 64 }, Note{ 67 } };    // C, E, G

    const std::vector<Interval> intervals = intervalsOf( melody );

    ASSERT_EQ( 2, intervals.size() );
    EXPECT_EQ( "M3", intervals.at( 0 ).identifier() );
    EXPECT_EQ( "m3", intervals.at( 1 ).identifier() );
}

TEST( IntervalTest, a_single_note_contains_no_interval )
{
    const std::vector<Note> singleNote{ Note{ 60 } };

    EXPECT_TRUE( intervalsOf( singleNote ).empty() );
}

// ---------------------------------------------------------------------------------------------------------------------
// Simple and compound intervals
//
// These tests pin down the boundary the whole model rests on: an interval KEEPS its octaves, so a
// ninth is not a second, and the naming follows the theory as far as the fifteenth.
// ---------------------------------------------------------------------------------------------------------------------

TEST( IntervalTest, an_octave_is_a_perfect_octave )
{
    constexpr Note lowerNote{ 60 };    // C4
    constexpr Note upperNote{ 72 };    // C5

    const Interval interval = intervalBetween( lowerNote, upperNote );

    EXPECT_EQ( 12, interval.semitones() );
    EXPECT_EQ( "P8", interval.identifier() );
    EXPECT_EQ( "Perfect octave", interval.name() );
    EXPECT_TRUE( interval.isCompound() );
    EXPECT_FALSE( interval.isSimple() );
    EXPECT_EQ( 1, interval.octaveSpan() );
    EXPECT_EQ( 8, interval.number() );
}

TEST( IntervalTest, a_ninth_is_not_a_second )
{
    constexpr Note tonic{ 60 };        // C4
    constexpr Note ninthNote{ 74 };    // D5, one octave above the second

    const Interval second = intervalFromSemitones( 2 );
    const Interval ninth = intervalBetween( tonic, ninthNote );

    EXPECT_EQ( "M2", second.identifier() );
    EXPECT_EQ( "M9", ninth.identifier() );
    EXPECT_EQ( "Major ninth", ninth.name() );
    EXPECT_EQ( 9, ninth.number() );

    // Both are the same colour heard one register higher, which is exactly what the class expresses.
    EXPECT_EQ( second.intervalClass(), ninth.intervalClass() );
    EXPECT_TRUE( second.isSimple() );
    EXPECT_TRUE( ninth.isCompound() );
}

TEST( IntervalTest, the_chord_extensions_of_jazz_are_named )
{
    // The eleventh and the thirteenth are what an extended chord is built on, and naming them is the
    // whole reason the model stopped throwing the octaves away.
    const Interval eleventh = intervalFromSemitones( 17 );

    EXPECT_EQ( "P11", eleventh.identifier() );
    EXPECT_EQ( "Perfect eleventh", eleventh.name() );
    EXPECT_EQ( 11, eleventh.number() );

    const Interval thirteenth = intervalFromSemitones( 21 );

    EXPECT_EQ( "M13", thirteenth.identifier() );
    EXPECT_EQ( "Major thirteenth", thirteenth.name() );
    EXPECT_EQ( 13, thirteenth.number() );
}

TEST( IntervalTest, a_falling_octave_is_still_an_octave )
{
    constexpr Note upperNote{ 72 };    // C5
    constexpr Note lowerNote{ 60 };    // C4

    EXPECT_EQ( "P8", intervalBetween( upperNote, lowerNote ).identifier() );
}

TEST( IntervalTest, nothing_reaches_past_the_fifteenth )
{
    // Two whole octaves: the widest interval that still has a name of its own.
    const Interval doubleOctave = intervalFromSemitones( MAXIMUM_INTERVAL_SEMITONES );

    EXPECT_EQ( "P15", doubleOctave.identifier() );
    EXPECT_EQ( "Perfect fifteenth", doubleOctave.name() );
    EXPECT_EQ( 15, doubleOctave.number() );

    // Anything wider is reduced rather than wrapped. Wrapping would silently turn an unplayable
    // request into a valid but WRONG interval, which is far worse than a saturated one.
    EXPECT_EQ( doubleOctave, intervalFromSemitones( 30 ) );
    EXPECT_EQ( doubleOctave, intervalFromSemitones( 127 ) );
    EXPECT_EQ( doubleOctave, intervalFromSemitones( -127 ) );
}

TEST( IntervalTest, every_interval_up_to_the_fifteenth_is_named )
{
    // The naming tables are indexed by the number of the interval, so this test is what guarantees
    // that no distance can ever reach past them: it walks the whole usable range.
    for( std::int32_t semitones = 0; semitones <= MAXIMUM_INTERVAL_SEMITONES; ++semitones )
    {
        const Interval interval = intervalFromSemitones( semitones );

        EXPECT_EQ( semitones, interval.semitones() );
        EXPECT_FALSE( interval.identifier().empty() ) << "semitones = " << semitones;
        EXPECT_FALSE( interval.name().empty() ) << "semitones = " << semitones;
        EXPECT_GE( interval.number(), 1 ) << "semitones = " << semitones;
        EXPECT_LE( interval.number(), 15 ) << "semitones = " << semitones;
    }
}

TEST( IntervalTest, the_supported_list_is_the_whole_usable_range )
{
    const std::array<Interval, SUPPORTED_INTERVAL_COUNT> & intervals = allSupportedIntervals();

    ASSERT_EQ( 25, intervals.size() );

    // The list is the usable range itself, in order, with no hole and no duplicate. A screen builds
    // its answer buttons from it, so a missing entry would silently remove an interval from the game,
    // and a repeated one would offer the same answer twice.
    for( std::size_t index = 0; index < intervals.size(); ++index )
    {
        const Interval & interval = intervals.at( index );

        EXPECT_EQ( static_cast<std::int32_t>( index ), interval.semitones() );
        EXPECT_FALSE( interval.name().empty() );
        EXPECT_FALSE( interval.identifier().empty() );
    }

    // It starts at the unison and stops exactly where the naming stops.
    EXPECT_EQ( "P1", intervals.front().identifier() );
    EXPECT_EQ( "P15", intervals.back().identifier() );
}

}    // namespace musichien::domain
