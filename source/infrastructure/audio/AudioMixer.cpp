#include "infrastructure/audio/AudioMixer.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace musichien::infrastructure
{

AudioMixer::AudioMixer( int p_sampleRate, int p_channelCount, QObject * p_parent )
  : QIODevice{ p_parent }
  , m_sampleRate{ p_sampleRate }
  , m_channelCount{ std::max( 1, p_channelCount ) }
{
    // A sequential source: the sink reads it, and there is nothing to seek in a stream of sound.
    open( QIODevice::ReadOnly );
}

void AudioMixer::play( std::vector<float> p_samples, float p_gain )
{
    if( p_samples.empty() )
    {
        return;
    }

    const std::lock_guard<std::mutex> lock{ m_mutex };

    m_voices.push_back( Voice{ std::move( p_samples ), 0, p_gain } );
}

void AudioMixer::clear()
{
    const std::lock_guard<std::mutex> lock{ m_mutex };

    m_voices.clear();
}

bool AudioMixer::isPlaying() const
{
    const std::lock_guard<std::mutex> lock{ m_mutex };

    return !m_voices.empty();
}

std::size_t AudioMixer::voiceCount() const
{
    const std::lock_guard<std::mutex> lock{ m_mutex };

    return m_voices.size();
}

qint64 AudioMixer::readData( char * p_data, qint64 p_maximumByteCount )
{
    if( ( p_data == nullptr ) || ( p_maximumByteCount <= 0 ) || ( m_sampleRate <= 0 ) )
    {
        return 0;
    }

    const auto channelCount = static_cast<std::size_t>( m_channelCount );

    const auto bytesPerFrame = static_cast<qint64>( sizeof( float ) * channelCount );

    const qint64 frameCount = p_maximumByteCount / bytesPerFrame;

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): the sink hands over bytes, the mixer works in floats.
    auto * output = reinterpret_cast<float *>( p_data );

    {
        const std::lock_guard<std::mutex> lock{ m_mutex };

        for( qint64 frameIndex = 0; frameIndex < frameCount; ++frameIndex )
        {
            float mixed = 0.0F;

            for( Voice & voice : m_voices )
            {
                if( voice.position < voice.samples.size() )
                {
                    mixed += voice.samples.at( voice.position ) * voice.gain;

                    ++voice.position;
                }
            }

            // Several sounds at once can leave the [-1, 1] range. Clipping is deliberate here: the alternative would
            // be a note that turns down the whole mix when a drum is added, which the ear reads as a glitch.
            mixed = std::clamp( mixed, -1.0F, 1.0F );

            for( std::size_t channel = 0; channel < channelCount; ++channel )
            {
                output[( static_cast<std::size_t>( frameIndex ) * channelCount ) + channel] = mixed;
            }
        }

        // A voice that reached the end of its own buffer is done and is dropped. Doing it AFTER the frame loop keeps
        // the sum above simple: a finished voice contributes nothing anyway.
        std::erase_if( m_voices, []( const Voice & p_voice ) {
            return p_voice.position >= p_voice.samples.size();
        } );
    }

    return frameCount * bytesPerFrame;
}

qint64 AudioMixer::writeData( const char * p_data, qint64 p_byteCount )
{
    (void)p_data;
    (void)p_byteCount;

    return 0;
}

}    // namespace musichien::infrastructure
