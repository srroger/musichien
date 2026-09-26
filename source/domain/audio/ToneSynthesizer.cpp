#include "domain/audio/ToneSynthesizer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <numbers>
#include <random>
#include <ranges>
#include <span>

namespace musichien::domain
{

namespace
{

// Full turn, in radians, used to convert a frequency into an angular step.
constexpr double TWO_PI = 2.0 * std::numbers::pi;

// Seed of the noise of the mistake cue.
//
// Fixed, and deliberately so: the domain owns no entropy source of its own, and the same cue must come
// out of every run and every machine. A cue that changed each time would also be impossible to test.
constexpr std::uint32_t MISTAKE_CUE_SEED = 20260926U;

// How fast the noise burst dies out, as an exponent applied over its whole length.
//
// Chosen so that the end of the burst is around a thousandth of its start: silence, to the ear, without
// needing a separate fade. The envelope is applied on top of it all the same - see renderMistakeCue.
constexpr double MISTAKE_CUE_DECAY = 6.0;

}    // namespace

ToneSynthesizer::ToneSynthesizer( std::int32_t p_sampleRate )
  : m_sampleRate{ p_sampleRate }
{
}

std::size_t ToneSynthesizer::sampleCountFor( std::chrono::milliseconds p_duration ) const noexcept
{
    if( ( m_sampleRate <= 0 ) || ( p_duration.count() <= 0 ) )
    {
        return 0;
    }

    const double durationInSeconds =
      std::chrono::duration_cast<std::chrono::duration<double>>( p_duration ).count();

    const double exactSampleCount = durationInSeconds * static_cast<double>( m_sampleRate );

    return static_cast<std::size_t>( std::llround( exactSampleCount ) );
}

std::vector<float> ToneSynthesizer::renderNote( const Note & p_note,
                                                std::chrono::milliseconds p_duration ) const
{
    const std::size_t sampleCount = sampleCountFor( p_duration );

    std::vector<float> samples( sampleCount, 0.0F );

    if( ( sampleCount == 0 ) || !p_note.isValid() )
    {
        return samples;
    }

    const double angularIncrement =
      TWO_PI * p_note.frequencyHz() / static_cast<double>( m_sampleRate );

    for( const std::size_t sampleIndex : std::views::iota( std::size_t{ 0 }, sampleCount ) )
    {
        // The phase is computed from the sample index instead of being accumulated. An accumulated
        // phase drifts over a long note, because floating point addition is not exact.
        const double phase = angularIncrement * static_cast<double>( sampleIndex );

        samples.at( sampleIndex ) = static_cast<float>( std::sin( phase ) );
    }

    applyEnvelope( samples );

    normalisePeak( samples );

    return samples;
}

std::vector<float> ToneSynthesizer::renderChord( std::span<const Note> p_notes,
                                                 std::chrono::milliseconds p_duration ) const
{
    const std::size_t sampleCount = sampleCountFor( p_duration );

    std::vector<float> mixedSamples( sampleCount, 0.0F );

    if( ( sampleCount == 0 ) || p_notes.empty() )
    {
        return mixedSamples;
    }

    // Every note is rendered on its own and then added. Mixing this way keeps the code readable, and
    // a defect in one voice cannot stay invisible because another voice masked it.
    for( const Note & note : p_notes )
    {
        const std::vector<float> noteSamples = renderNote( note, p_duration );

        std::ranges::transform( noteSamples, mixedSamples, mixedSamples.begin(), std::plus<>{} );
    }

    // Summing voices can exceed the maximum amplitude. Without this second normalisation a chord
    // would clip, which sounds like distortion and would mislead the ear.
    normalisePeak( mixedSamples );

    return mixedSamples;
}

std::vector<float> ToneSynthesizer::renderMelody( std::span<const Note> p_notes,
                                                  std::chrono::milliseconds p_noteDuration,
                                                  std::chrono::milliseconds p_gap ) const
{
    std::vector<float> melodySamples;

    const std::size_t gapSampleCount = sampleCountFor( p_gap );

    melodySamples.reserve( p_notes.size() * ( sampleCountFor( p_noteDuration ) + gapSampleCount ) );

    for( const Note & note : p_notes )
    {
        const std::vector<float> noteSamples = renderNote( note, p_noteDuration );

        melodySamples.insert( melodySamples.end(), noteSamples.begin(), noteSamples.end() );

        // The gap is silence, and it is not optional: two notes played back to back with no silence
        // sound like one continuous glide, which makes the interval impossible to hear.
        melodySamples.insert( melodySamples.end(), gapSampleCount, 0.0F );
    }

    return melodySamples;
}

void ToneSynthesizer::applyEnvelope( std::span<float> p_samples ) const
{
    const std::size_t sampleCount = p_samples.size();

    if( sampleCount == 0 )
    {
        return;
    }

    // Each fade may take at most half of the buffer, otherwise the two fades would overlap and the
    // note would never reach its full level: a short note would sound like a click by itself.
    const std::size_t maximumFadeSampleCount = sampleCount / 2;

    const std::size_t attackSampleCount =
      std::min( sampleCountFor( ATTACK_DURATION ), maximumFadeSampleCount );

    const std::size_t releaseSampleCount =
      std::min( sampleCountFor( RELEASE_DURATION ), maximumFadeSampleCount );

    // Fade in: the gain starts at 0, so the very first sample of the buffer is silence.
    //
    // The two loops below walk SUBVIEWS of the buffer rather than addressing samples by index, for two
    // independent reasons:
    //
    //   * indexing a span is an unchecked access, which the clang-tidy configuration of this project
    //     refuses (cppcoreguidelines-pro-bounds-avoid-unchecked-container-access);
    //   * std::span::at(), the checked accessor used here before, is C++26 and is absent from the
    //     libc++ shipped with the Android NDK's Clang 18.
    //
    // 'first' and 'last' cannot leave the buffer - the callers clamp both counts to half of it - and
    // the gain is counted alongside the samples instead of being derived from an index.
    std::size_t attackIndex = 0;

    for( float & sample : p_samples.first( attackSampleCount ) )
    {
        sample *= static_cast<float>( attackIndex ) / static_cast<float>( attackSampleCount );

        ++attackIndex;
    }

    // Fade out: counted from the end, so the very last sample is exactly silence.
    std::size_t samplesFromTheEnd = releaseSampleCount;

    for( float & sample : p_samples.last( releaseSampleCount ) )
    {
        --samplesFromTheEnd;

        sample *= static_cast<float>( samplesFromTheEnd ) / static_cast<float>( releaseSampleCount );
    }
}

void ToneSynthesizer::normalisePeak( std::span<float> p_samples )
{
    normalisePeakTo( p_samples, TARGET_PEAK_AMPLITUDE );
}

void ToneSynthesizer::normalisePeakTo( std::span<float> p_samples, float p_targetPeak )
{
    if( p_samples.empty() )
    {
        return;
    }

    const auto absoluteValues = p_samples | std::views::transform( []( float p_sample ) {
                                    return std::abs( p_sample );
                                } );

    const float peak = std::ranges::max( absoluteValues );

    if( peak <= 0.0F )
    {
        return;
    }

    const float gain = p_targetPeak / peak;

    std::ranges::for_each( p_samples, [gain]( float & p_sample ) { p_sample *= gain; } );
}

std::vector<float> ToneSynthesizer::renderMistakeCue( std::chrono::milliseconds p_duration ) const
{
    const std::size_t sampleCount = sampleCountFor( p_duration );

    std::vector<float> samples( sampleCount, 0.0F );

    if( sampleCount == 0 )
    {
        return samples;
    }

    // The seed is fixed: see the header. The draw itself is uniform over the whole range of a sample,
    // which is the simplest way to get something with no pitch and no memory.
    // The constant seed is the POINT here, and the check cannot know that: a cue has to be the same
    // every time, and a cue that changed would be impossible to recognise.
    // NOLINTNEXTLINE(bugprone-random-generator-seed, cert-msc32-c, cert-msc51-cpp)
    std::mt19937 noiseEngine{ MISTAKE_CUE_SEED };

    std::uniform_real_distribution<float> amplitudeDistribution{ -1.0F, 1.0F };

    for( const std::size_t sampleIndex : std::views::iota( std::size_t{ 0 }, sampleCount ) )
    {
        // A decay over the whole burst, so that it reads as a brief "thud" rather than a hiss.
        const float progress = static_cast<float>( sampleIndex ) / static_cast<float>( sampleCount );

        const auto decay =
          static_cast<float>( std::exp( -MISTAKE_CUE_DECAY * static_cast<double>( progress ) ) );

        samples.at( sampleIndex ) = amplitudeDistribution( noiseEngine ) * decay;
    }

    // The same fade in and fade out as a note, and for the same reason: a burst that starts or stops
    // abruptly adds a CLICK of its own, which is the artefact the envelope exists to remove.
    applyEnvelope( samples );

    // And a LOWER target than a note, because noise at the same peak sounds louder than a tone.
    normalisePeakTo( samples, MISTAKE_CUE_PEAK_AMPLITUDE );

    return samples;
}

}    // namespace musichien::domain
