#include "domain/music/Temperament.h"

#include <gtest/gtest.h>

#include <cmath>

namespace musichien::domain
{

// ---------------------------------------------------------------------------------------------------------------------
// The temperaments
//
// The rules worth testing are the ones that make a temperament WHAT IT IS: equal ignores the root, a pure fifth is
// 3/2 for good, and an octave is a doubling everywhere. If one of these breaks, the sound is wrong in a way no screen
// will ever show.
// ---------------------------------------------------------------------------------------------------------------------

namespace
{

constexpr Note ROOT{ 60 };    // C4

// A tolerance wide enough for the floating point arithmetic, and far narrower than any audible difference.
constexpr double TOLERANCE = 1e-9;

}    // namespace

TEST( TemperamentTest, equal_temperament_ignores_the_root )
{
    const Note note{ 67 };    // G4

    EXPECT_NEAR( note.frequencyHz(),
                 frequencyFor( note, ROOT, Temperament::Equal ),
                 TOLERANCE );

    // Another root, the same answer: that is the whole point of equal temperament.
    EXPECT_NEAR( frequencyFor( note, ROOT, Temperament::Equal ),
                 frequencyFor( note, Note{ 62 }, Temperament::Equal ),
                 TOLERANCE );
}

TEST( TemperamentTest, a_note_played_from_itself_sounds_at_its_own_frequency )
{
    for( const Temperament temperament : { Temperament::Equal, Temperament::Pythagorean, Temperament::Just } )
    {
        EXPECT_NEAR( ROOT.frequencyHz(), frequencyFor( ROOT, ROOT, temperament ), TOLERANCE );
    }
}

TEST( TemperamentTest, an_octave_is_a_doubling_in_every_temperament )
{
    for( const Temperament temperament : { Temperament::Equal, Temperament::Pythagorean, Temperament::Just } )
    {
        const double octave = frequencyFor( Note{ 72 }, ROOT, temperament );    // C5

        EXPECT_NEAR( 2.0, octave / ROOT.frequencyHz(), TOLERANCE );
    }
}

TEST( TemperamentTest, pythagorean_fifths_and_fourths_are_perfect )
{
    const double root = ROOT.frequencyHz();

    const double fifth = frequencyFor( Note{ 67 }, ROOT, Temperament::Pythagorean );     // G4
    const double fourth = frequencyFor( Note{ 65 }, ROOT, Temperament::Pythagorean );    // F4

    EXPECT_NEAR( 3.0 / 2.0, fifth / root, TOLERANCE );
    EXPECT_NEAR( 4.0 / 3.0, fourth / root, TOLERANCE );
}

TEST( TemperamentTest, pythagorean_thirds_are_wider_than_the_tempered_ones )
{
    const double root = ROOT.frequencyHz();

    const double pythagorean = frequencyFor( Note{ 64 }, ROOT, Temperament::Pythagorean ) / root;    // E4
    const double tempered = frequencyFor( Note{ 64 }, ROOT, Temperament::Equal ) / root;

    // 81/64 against 2^(1/3): the Pythagorean third is the wider one, and that is audible.
    EXPECT_NEAR( 81.0 / 64.0, pythagorean, TOLERANCE );
    EXPECT_GT( pythagorean, tempered );
}

TEST( TemperamentTest, just_triads_use_small_whole_numbers )
{
    const double root = ROOT.frequencyHz();

    const double majorThird = frequencyFor( Note{ 64 }, ROOT, Temperament::Just ) / root;    // E4
    const double minorThird = frequencyFor( Note{ 63 }, ROOT, Temperament::Just ) / root;    // D#4
    const double fifth = frequencyFor( Note{ 67 }, ROOT, Temperament::Just ) / root;         // G4

    EXPECT_NEAR( 5.0 / 4.0, majorThird, TOLERANCE );
    EXPECT_NEAR( 6.0 / 5.0, minorThird, TOLERANCE );
    EXPECT_NEAR( 3.0 / 2.0, fifth, TOLERANCE );
}

TEST( TemperamentTest, a_note_below_the_root_falls_into_the_octave_below )
{
    // One semitone under C4 is B3, just below - not almost a unison.
    const double below = frequencyFor( Note{ 59 }, ROOT, Temperament::Pythagorean );

    EXPECT_LT( below, ROOT.frequencyHz() );
    EXPECT_GT( below, ROOT.frequencyHz() / 2.0 );
}

TEST( TemperamentTest, a_pure_fifth_is_not_the_tempered_one )
{
    const double root = ROOT.frequencyHz();

    const double pure = frequencyFor( Note{ 67 }, ROOT, Temperament::Pythagorean ) / root;
    const double tempered = frequencyFor( Note{ 67 }, ROOT, Temperament::Equal ) / root;

    // The difference is tiny - about 2 cents - but it is real, and it is the reason the feature exists.
    EXPECT_GT( pure, tempered );
    EXPECT_NEAR( std::pow( 2.0, 7.0 / 12.0 ), tempered, TOLERANCE );
}

}    // namespace musichien::domain
