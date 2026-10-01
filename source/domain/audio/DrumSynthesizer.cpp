#include "domain/audio/DrumSynthesizer.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>
#include <ranges>
#include <span>

namespace musichien::domain
{

namespace
{

// Fixed, like the mistake cue: the domain owns no entropy source, and the same hit must come out of every run.
constexpr std::uint32_t DRUM_NOISE_SEED = 20260927U;

[[nodiscard]] std::size_t sampleCountFor( double p_seconds, std::int32_t p_sampleRate )
{
    return static_cast<std::size_t>( std::llround( p_seconds * static_cast<double>( p_sampleRate ) ) );
}

// An exponential fade over the whole buffer: a struck drum dies out, it does not stop.
//
// Written with ranges rather than by index: libc++ 18 - the one the Android NDK ships - has no span::at(), and the
// project refuses unchecked indexing. See docs/BUILD_AND_SETUP.md, the pitfalls table.
void applyExponentialDecay( std::span<float> p_samples, double p_decayRate )
{
    if( p_samples.empty() )
    {
        return;
    }

    const auto sampleCount = static_cast<double>( p_samples.size() );

    std::size_t sampleIndex = 0;

    std::ranges::for_each( p_samples, [&sampleIndex, sampleCount, p_decayRate]( float & p_sample ) {
        const double progress = static_cast<double>( sampleIndex ) / sampleCount;

        p_sample *= static_cast<float>( std::exp( -p_decayRate * progress ) );

        ++sampleIndex;
    } );
}

// Peak normalisation, so every piece of the kit speaks at the same level whatever its waveform.
void normalisePeakTo( std::span<float> p_samples, float p_targetPeak )
{
    float peak = 0.0F;

    for( const float sample : p_samples )
    {
        peak = std::max( peak, std::abs( sample ) );
    }

    if( peak <= 0.0F )
    {
        return;
    }

    const float gain = p_targetPeak / peak;

    for( float & sample : p_samples )
    {
        sample *= gain;
    }
}

}    // namespace

DrumSynthesizer::DrumSynthesizer( std::int32_t p_sampleRate )
  : m_sampleRate{ p_sampleRate }
{
}

std::vector<float> DrumSynthesizer::renderDrum( Drum p_drum ) const
{
    switch( p_drum )
    {
        case Drum::Kick: {
            // A falling sine, 100 Hz down to 40 Hz: the pitch drop IS the thump of the beater against the skin.
            const std::size_t count = sampleCountFor( 0.18, m_sampleRate );

            std::vector<float> samples( count, 0.0F );

            double phase = 0.0;

            for( std::size_t sampleIndex = 0; sampleIndex < count; ++sampleIndex )
            {
                const double progress = static_cast<double>( sampleIndex ) / static_cast<double>( count );

                const double frequency = 100.0 - ( 60.0 * progress );

                phase += ( 2.0 * std::numbers::pi * frequency ) / static_cast<double>( m_sampleRate );

                samples.at( sampleIndex ) = static_cast<float>( std::sin( phase ) );
            }

            applyExponentialDecay( samples, 5.0 );
            normalisePeakTo( samples, 0.9F );

            return samples;
        }

        case Drum::Snare: {
            // Noise for the wires, plus a little tone for the shell: that mix is what separates a snare from a clap.
            const std::size_t count = sampleCountFor( 0.22, m_sampleRate );

            std::vector<float> samples( count, 0.0F );

            std::mt19937 engine{ DRUM_NOISE_SEED };
            std::uniform_real_distribution<float> distribution{ -1.0F, 1.0F };

            for( std::size_t sampleIndex = 0; sampleIndex < count; ++sampleIndex )
            {
                const double timeSeconds = static_cast<double>( sampleIndex ) / static_cast<double>( m_sampleRate );

                const float noise = distribution( engine );
                const auto tone = static_cast<float>( std::sin( 2.0 * std::numbers::pi * 180.0 * timeSeconds ) );

                samples.at( sampleIndex ) = ( 0.7F * noise ) + ( 0.3F * tone );
            }

            applyExponentialDecay( samples, 4.0 );
            normalisePeakTo( samples, 0.85F );

            return samples;
        }

        case Drum::HiHat: {
            // A first difference turns white noise into high-frequency hiss: the tick that keeps the time.
            const std::size_t count = sampleCountFor( 0.06, m_sampleRate );

            std::vector<float> samples( count, 0.0F );

            std::mt19937 engine{ DRUM_NOISE_SEED + 1U };
            std::uniform_real_distribution<float> distribution{ -1.0F, 1.0F };

            float previous = 0.0F;

            for( std::size_t sampleIndex = 0; sampleIndex < count; ++sampleIndex )
            {
                const float noise = distribution( engine );

                samples.at( sampleIndex ) = noise - previous;

                previous = noise;
            }

            applyExponentialDecay( samples, 6.0 );
            normalisePeakTo( samples, 0.7F );

            return samples;
        }

        case Drum::Tom: {
            // A damped sine, no pitch drop: the round, singing body of a tom.
            const std::size_t count = sampleCountFor( 0.30, m_sampleRate );

            std::vector<float> samples( count, 0.0F );

            for( std::size_t sampleIndex = 0; sampleIndex < count; ++sampleIndex )
            {
                const double timeSeconds = static_cast<double>( sampleIndex ) / static_cast<double>( m_sampleRate );

                samples.at( sampleIndex ) = static_cast<float>( std::sin( 2.0 * std::numbers::pi * 160.0 * timeSeconds ) );
            }

            applyExponentialDecay( samples, 4.0 );
            normalisePeakTo( samples, 0.85F );

            return samples;
        }
    }

    return {};
}

}    // namespace musichien::domain
