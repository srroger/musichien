#include "domain/audio/ToneSynthesizer.h"

#include "domain/audio/NotePlayerFake.h"
#include "domain/music/Note.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <ranges>
#include <span>
#include <vector>

namespace musichien::domain
{

namespace
{

// Sample rate used by every test: the one Android devices use natively.
constexpr std::int32_t TEST_SAMPLE_RATE = 48000;

// Peak level actually present in a buffer.
[[nodiscard]] float peakAmplitudeOf( std::span<const float> p_samples )
{
    if( p_samples.empty() )
    {
        return 0.0F;
    }

    const auto absoluteValues = p_samples | std::views::transform( []( float p_sample ) {
                                    return std::abs( p_sample );
                                } );

    return std::ranges::max( absoluteValues );
}

// Counts how many times the signal crosses zero.
//
// This is how a test verifies the PITCH without any audio hardware: a sine of frequency f played for
// one second crosses zero 2 * f times. It checks the whole chain note -> frequency -> waveform.
[[nodiscard]] std::size_t countZeroCrossings( std::span<const float> p_samples )
{
    if( p_samples.size() < 2 )
    {
        return 0;
    }

    std::size_t crossingCount = 0;

    for( const auto & [previousSample, currentSample] : p_samples | std::views::adjacent<2> )
    {
        if( ( previousSample >= 0.0F ) != ( currentSample >= 0.0F ) )
        {
            ++crossingCount;
        }
    }

    return crossingCount;
}

}    // namespace

// ---------------------------------------------------------------------------------------------------------------------
// Length of the rendered buffers
// ---------------------------------------------------------------------------------------------------------------------

TEST( ToneSynthesizerTest, sample_count_matches_the_requested_duration )
{
    const ToneSynthesizer synthesizer{ TEST_SAMPLE_RATE };

    EXPECT_EQ( 0, synthesizer.sampleCountFor( std::chrono::milliseconds{ 0 } ) );
    EXPECT_EQ( 24000, synthesizer.sampleCountFor( std::chrono::milliseconds{ 500 } ) );
    EXPECT_EQ( 48000, synthesizer.sampleCountFor( std::chrono::milliseconds{ 1000 } ) );
}

TEST( ToneSynthesizerTest, rendered_note_has_the_requested_length )
{
    const ToneSynthesizer synthesizer{ TEST_SAMPLE_RATE };

    const std::vector<float> samples =
      synthesizer.renderNote( Note{ 69 }, std::chrono::milliseconds{ 500 } );

    EXPECT_EQ( 24000, samples.size() );
}

// ---------------------------------------------------------------------------------------------------------------------
// The envelope: what removes the click
// ---------------------------------------------------------------------------------------------------------------------

TEST( ToneSynthesizerTest, rendered_note_starts_and_ends_at_silence )
{
    const ToneSynthesizer synthesizer{ TEST_SAMPLE_RATE };

    const std::vector<float> samples =
      synthesizer.renderNote( Note{ 69 }, std::chrono::milliseconds{ 500 } );

    ASSERT_FALSE( samples.empty() );

    // A note starting or stopping on a non zero value is exactly what produces an audible click.
    EXPECT_FLOAT_EQ( 0.0F, samples.front() );
    EXPECT_FLOAT_EQ( 0.0F, samples.back() );
}

TEST( ToneSynthesizerTest, a_note_shorter_than_both_fades_still_has_an_envelope )
{
    const ToneSynthesizer synthesizer{ TEST_SAMPLE_RATE };

    // 20 ms is shorter than the attack (12 ms) plus the release (40 ms) added together.
    const std::vector<float> shortSamples =
      synthesizer.renderNote( Note{ 69 }, std::chrono::milliseconds{ 20 } );

    ASSERT_FALSE( shortSamples.empty() );

    EXPECT_FLOAT_EQ( 0.0F, shortSamples.front() );
    EXPECT_FLOAT_EQ( 0.0F, shortSamples.back() );
}

// ---------------------------------------------------------------------------------------------------------------------
// Loudness: a note and a chord must be comparable by ear
// ---------------------------------------------------------------------------------------------------------------------

TEST( ToneSynthesizerTest, a_note_peaks_at_the_target_amplitude )
{
    const ToneSynthesizer synthesizer{ TEST_SAMPLE_RATE };

    const std::vector<float> samples =
      synthesizer.renderNote( Note{ 69 }, std::chrono::milliseconds{ 500 } );

    EXPECT_NEAR( ToneSynthesizer::TARGET_PEAK_AMPLITUDE, peakAmplitudeOf( samples ), 1.0e-5F );
}

TEST( ToneSynthesizerTest, a_chord_is_as_loud_as_a_single_note )
{
    const ToneSynthesizer synthesizer{ TEST_SAMPLE_RATE };

    const std::vector<Note> chord{ Note{ 60 }, Note{ 64 }, Note{ 67 } };

    const std::vector<float> noteSamples =
      synthesizer.renderNote( Note{ 60 }, std::chrono::milliseconds{ 500 } );

    const std::vector<float> chordSamples =
      synthesizer.renderChord( chord, std::chrono::milliseconds{ 500 } );

    // Without normalising the mix, summing three voices would either clip or be three times louder
    // than a single note. The player would then hear the loudness instead of the interval.
    EXPECT_NEAR( ToneSynthesizer::TARGET_PEAK_AMPLITUDE, peakAmplitudeOf( chordSamples ), 1.0e-5F );
    EXPECT_NEAR( peakAmplitudeOf( noteSamples ), peakAmplitudeOf( chordSamples ), 1.0e-5F );
}

// ---------------------------------------------------------------------------------------------------------------------
// The pitch
// ---------------------------------------------------------------------------------------------------------------------

TEST( ToneSynthesizerTest, A4_crosses_zero_880_times_in_one_second )
{
    const ToneSynthesizer synthesizer{ TEST_SAMPLE_RATE };

    // MIDI 69 is A4, the reference note at 440 Hz.
    const std::vector<float> samples =
      synthesizer.renderNote( Note{ 69 }, std::chrono::milliseconds{ 1000 } );

    EXPECT_NEAR( 880, countZeroCrossings( samples ), 2 );
}

TEST( ToneSynthesizerTest, an_octave_higher_crosses_zero_twice_as_often )
{
    const ToneSynthesizer synthesizer{ TEST_SAMPLE_RATE };

    const std::vector<float> lowerSamples =
      synthesizer.renderNote( Note{ 69 }, std::chrono::milliseconds{ 500 } );

    const std::vector<float> upperSamples =
      synthesizer.renderNote( Note{ 81 }, std::chrono::milliseconds{ 500 } );

    const auto lowerCrossingCount = static_cast<double>( countZeroCrossings( lowerSamples ) );
    const auto upperCrossingCount = static_cast<double>( countZeroCrossings( upperSamples ) );

    ASSERT_GT( lowerCrossingCount, 0.0 );

    EXPECT_NEAR( 2.0, upperCrossingCount / lowerCrossingCount, 0.02 );
}

// ---------------------------------------------------------------------------------------------------------------------
// Degenerate cases must not crash and must not produce noise
// ---------------------------------------------------------------------------------------------------------------------

TEST( ToneSynthesizerTest, a_note_outside_the_playable_range_produces_silence )
{
    const ToneSynthesizer synthesizer{ TEST_SAMPLE_RATE };

    // MIDI 200 does not exist: the renderer must return silence, not an arbitrary frequency.
    const std::vector<float> samples =
      synthesizer.renderNote( Note{ 200 }, std::chrono::milliseconds{ 200 } );

    ASSERT_FALSE( samples.empty() );

    EXPECT_FLOAT_EQ( 0.0F, peakAmplitudeOf( samples ) );
}

TEST( ToneSynthesizerTest, a_melody_contains_the_silence_between_its_notes )
{
    const ToneSynthesizer synthesizer{ TEST_SAMPLE_RATE };

    const std::vector<Note> melody{ Note{ 60 }, Note{ 67 } };

    constexpr std::chrono::milliseconds noteDuration{ 300 };
    constexpr std::chrono::milliseconds gap{ 100 };

    const std::vector<float> melodySamples = synthesizer.renderMelody( melody, noteDuration, gap );

    const std::size_t expectedLength = melody.size() * ( synthesizer.sampleCountFor( noteDuration ) + synthesizer.sampleCountFor( gap ) );

    EXPECT_EQ( expectedLength, melodySamples.size() );

    // The melody must be longer than the two notes alone, otherwise the gap was not inserted and the
    // two notes would sound like one continuous glide.
    EXPECT_GT( melodySamples.size(), 2 * synthesizer.sampleCountFor( noteDuration ) );
}

// ---------------------------------------------------------------------------------------------------------------------
// The test double itself
// ---------------------------------------------------------------------------------------------------------------------

TEST( NotePlayerFakeTest, records_what_it_is_asked_to_play )
{
    NotePlayerFake player;

    const std::vector<Note> interval{ Note{ 60 }, Note{ 67 } };

    player.playNote( Note{ 60 } );
    player.playMelody( interval, std::chrono::milliseconds{ 120 } );
    player.playChord( interval );
    player.stopAll();

    EXPECT_EQ( 1, player.playedNotes().size() );

    ASSERT_EQ( 1, player.playedMelodies().size() );
    EXPECT_EQ( 2, player.playedMelodies().front().notes.size() );
    EXPECT_EQ( 120, player.playedMelodies().front().gap.count() );

    ASSERT_EQ( 1, player.playedChords().size() );

    EXPECT_EQ( 1, player.stopCount() );
}

}    // namespace musichien::domain
