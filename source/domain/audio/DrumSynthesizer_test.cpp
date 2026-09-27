#include "domain/audio/DrumSynthesizer.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace musichien::domain
{

namespace
{

constexpr std::int32_t TEST_SAMPLE_RATE = 48000;

// How many times the signal crosses zero: a cheap way to tell a low thump from a high hiss without a sound card.
[[nodiscard]] std::size_t countZeroCrossings( const std::vector<float> & p_samples )
{
    std::size_t crossingCount = 0;

    for( std::size_t index = 1; index < p_samples.size(); ++index )
    {
        if( ( p_samples.at( index - 1 ) >= 0.0F ) != ( p_samples.at( index ) >= 0.0F ) )
        {
            ++crossingCount;
        }
    }

    return crossingCount;
}

[[nodiscard]] float peakOf( const std::vector<float> & p_samples )
{
    float peak = 0.0F;

    for( const float sample : p_samples )
    {
        peak = std::max( peak, std::abs( sample ) );
    }

    return peak;
}

}    // namespace

TEST( DrumSynthesizerTest, every_piece_of_the_kit_is_audible_and_normalised )
{
    const DrumSynthesizer kit{ TEST_SAMPLE_RATE };

    for( const Drum drum : { Drum::Kick, Drum::Snare, Drum::HiHat, Drum::Tom } )
    {
        const std::vector<float> hit = kit.renderDrum( drum );

        ASSERT_FALSE( hit.empty() );

        // Loud enough to be heard, and never clipping: that is what the peak normalisation is for.
        EXPECT_GT( peakOf( hit ), 0.5F );
        EXPECT_LE( peakOf( hit ), 1.0F );
    }
}

TEST( DrumSynthesizerTest, the_same_hit_sounds_the_same_every_time )
{
    const DrumSynthesizer kit{ TEST_SAMPLE_RATE };

    // A snare is built from noise, so this is the one that could drift: the seed is fixed for exactly this reason.
    EXPECT_EQ( kit.renderDrum( Drum::Snare ), kit.renderDrum( Drum::Snare ) );
}

TEST( DrumSynthesizerTest, a_kick_is_low_and_a_hihat_is_high )
{
    const DrumSynthesizer kit{ TEST_SAMPLE_RATE };

    const std::size_t kickCrossings = countZeroCrossings( kit.renderDrum( Drum::Kick ) );
    const std::size_t hihatCrossings = countZeroCrossings( kit.renderDrum( Drum::HiHat ) );

    // The kick falls from 100 to 40 Hz, the hi-hat is a hiss: the gap is orders of magnitude, not a nuance.
    EXPECT_LT( kickCrossings, hihatCrossings );
}

}    // namespace musichien::domain
