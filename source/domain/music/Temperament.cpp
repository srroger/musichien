#include "domain/music/Temperament.h"

#include <cmath>
#include <cstddef>

namespace musichien::domain
{

namespace
{

// The ratio of an interval of 0 to 11 semitones, built from a chain of pure fifths.
//
// Every note of the Pythagorean scale is reached by stacking fifths (a ratio of 3/2) and folding the result back into
// one octave (a ratio of 2). The number of fifths needed for a given interval is the modular inverse of seven:
// twelve semitones make seven fifths, so an interval of n semitones is n * 7 fifths, modulo the octave.
[[nodiscard]] double pythagoreanRatioFor( int p_semitones ) noexcept
{
    int fifths = ( 7 * p_semitones ) % 12;

    // The shortest path: a fifth up and a fourth down land on the same note, one octave apart.
    if( fifths > 6 )
    {
        fifths -= 12;
    }

    double ratio = std::pow( 3.0 / 2.0, fifths );

    while( ratio < 1.0 )
    {
        ratio *= 2.0;
    }

    while( ratio >= 2.0 )
    {
        ratio /= 2.0;
    }

    return ratio;
}

// The ratios of the just major scale, one per semitone. They are the small whole numbers the ear calls consonant:
// 5/4 for a major third, 6/5 for a minor third, 3/2 for a fifth - the triad of every common chord.
constexpr std::array<double, 12> JUST_RATIOS{ 1.0,             // unison
                                              16.0 / 15.0,     // minor second
                                              9.0 / 8.0,       // major second
                                              6.0 / 5.0,       // minor third
                                              5.0 / 4.0,       // major third
                                              4.0 / 3.0,       // perfect fourth
                                              45.0 / 32.0,     // tritone
                                              3.0 / 2.0,       // perfect fifth
                                              8.0 / 5.0,       // minor sixth
                                              5.0 / 3.0,       // major sixth
                                              9.0 / 5.0,       // minor seventh
                                              15.0 / 8.0 };    // major seventh

// How many whole octaves separate two semitone counts, rounding towards minus infinity: one semitone BELOW the root
// is one octave down, not zero octaves down.
[[nodiscard]] int octavesBelow( int p_semitones ) noexcept
{
    if( p_semitones >= 0 )
    {
        return p_semitones / 12;
    }

    return -( ( -p_semitones + 11 ) / 12 );
}

}    // namespace

double frequencyFor( Note p_note, Note p_root, Temperament p_temperament ) noexcept
{
    if( p_temperament == Temperament::Equal )
    {
        return p_note.frequencyHz();
    }

    const int semitones = p_note.midiNumber() - p_root.midiNumber();
    const int octaves = octavesBelow( semitones );
    const int withinOctave = semitones - ( octaves * 12 );

    const double intervalRatio = ( p_temperament == Temperament::Pythagorean )
                                   ? pythagoreanRatioFor( withinOctave )
                                   : JUST_RATIOS.at( static_cast<std::size_t>( withinOctave ) );

    // The root's own frequency stays the one of equal temperament: where A4 sits is a DIAPASON choice, not a
    // temperament one - 440 Hz is the same note in every tuning.
    return p_root.frequencyHz() * std::pow( 2.0, static_cast<double>( octaves ) ) * intervalRatio;
}

double centsBetween( double p_frequencyHz, double p_referenceHz ) noexcept
{
    // Silence is not a note that is too low: it is no note at all.
    if( p_frequencyHz <= 0.0 || p_referenceHz <= 0.0 )
    {
        return 0.0;
    }

    constexpr double CENTS_PER_OCTAVE = 1200.0;

    return CENTS_PER_OCTAVE * std::log2( p_frequencyHz / p_referenceHz );
}

}    // namespace musichien::domain
