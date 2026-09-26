#include "domain/audio/ToneSynthesizer.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <numbers>
#include <ranges>

namespace musichien::domain
{

namespace
{

// Full turn, in radians, used to convert a frequency into an angular step.
constexpr double TWO_PI = 2.0 * std::numbers::pi;

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
    for( const std::size_t sampleIndex : std::views::iota( std::size_t{ 0 }, attackSampleCount ) )
    {
        const float gain = static_cast<float>( sampleIndex ) / static_cast<float>( attackSampleCount );

        p_samples.at( sampleIndex ) *= gain;
    }

    // Fade out: counted from the end, so the very last sample is exactly silence.
    for( const std::size_t samplesFromTheEnd : std::views::iota( std::size_t{ 0 }, releaseSampleCount ) )
    {
        const std::size_t sampleIndex = sampleCount - 1 - samplesFromTheEnd;

        const float gain =
          static_cast<float>( samplesFromTheEnd ) / static_cast<float>( releaseSampleCount );

        p_samples.at( sampleIndex ) *= gain;
    }
}

void ToneSynthesizer::normalisePeak( std::span<float> p_samples )
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

    const float gain = TARGET_PEAK_AMPLITUDE / peak;

    std::ranges::for_each( p_samples, [gain]( float & p_sample ) { p_sample *= gain; } );
}

}    // namespace musichien::domain
