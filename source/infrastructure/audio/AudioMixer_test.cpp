#include "infrastructure/audio/AudioMixer.h"

#include <QObject>

#include <gtest/gtest.h>

#include <cstddef>
#include <vector>

namespace musichien::infrastructure
{

namespace
{

constexpr int TEST_SAMPLE_RATE = 48000;
constexpr int TEST_CHANNELS = 1;

// A mixer whose readData can be called directly.
//
// Qt's QIODevice::read() fills a buffer of its own, so it asks the device for FAR more frames than the caller wanted.
// A test written against read() would therefore be a test about Qt's buffering, not about the mix - which is exactly
// what the first version of this file got wrong.
class TestableMixer final : public AudioMixer
{
public:
    using AudioMixer::AudioMixer;
    using AudioMixer::readData;

    [[nodiscard]] std::vector<float> readFrames( std::size_t p_frameCount )
    {
        std::vector<float> frames( p_frameCount, 0.0F );

        const auto byteCount = static_cast<qint64>( p_frameCount * sizeof( float ) );

        const qint64 readByteCount = readData( reinterpret_cast<char *>( frames.data() ), byteCount );

        EXPECT_EQ( byteCount, readByteCount );

        return frames;
    }
};

}    // namespace

TEST( AudioMixerTest, two_sounds_are_heard_together )
{
    TestableMixer mixer{ TEST_SAMPLE_RATE, TEST_CHANNELS };

    // Half a level each, so that the sum is exactly 1 and cannot be confused with a clamp.
    mixer.play( std::vector<float>( 4, 0.5F ) );
    mixer.play( std::vector<float>( 4, 0.5F ) );

    for( const float frame : mixer.readFrames( 4 ) )
    {
        EXPECT_FLOAT_EQ( 1.0F, frame );
    }
}

TEST( AudioMixerTest, a_new_sound_does_not_replace_the_one_playing )
{
    TestableMixer mixer{ TEST_SAMPLE_RATE, TEST_CHANNELS };

    // The bug Roger heard: a drum hit on the same instant as the metronome click silenced the click.
    mixer.play( std::vector<float>( 8, 0.3F ) );

    const std::vector<float> firstHalf = mixer.readFrames( 4 );

    mixer.play( std::vector<float>( 4, 0.4F ) );

    const std::vector<float> secondHalf = mixer.readFrames( 4 );

    for( const float frame : firstHalf )
    {
        EXPECT_FLOAT_EQ( 0.3F, frame );
    }

    for( const float frame : secondHalf )
    {
        EXPECT_FLOAT_EQ( 0.7F, frame );
    }
}

TEST( AudioMixerTest, silence_is_written_when_nothing_plays )
{
    TestableMixer mixer{ TEST_SAMPLE_RATE, TEST_CHANNELS };

    EXPECT_FALSE( mixer.isPlaying() );

    // Rien a lire : le peripherique doit le savoir, sinon il reste en veille au lieu de rendre l'appareil.
    EXPECT_EQ( 0, mixer.bytesAvailable() );

    // The buffer must still be FILLED: returning nothing would be read as "no data" and stall the stream.
    for( const float frame : mixer.readFrames( 16 ) )
    {
        EXPECT_FLOAT_EQ( 0.0F, frame );
    }
}

TEST( AudioMixerTest, a_sound_announces_itself_to_the_device )
{
    TestableMixer mixer{ TEST_SAMPLE_RATE, TEST_CHANNELS };

    // C'EST LE BUG QUI A RENDU L'APPLICATION MUETTE : un QIODevice sequentiel qui n'annonce RIEN a lire est lu comme
    // vide par le peripherique, qui reste alors en veille (IdleState) sans jamais venir chercher d'echantillons. Le
    // son ne sort jamais, et rien ne plante : l'application est simplement silencieuse.
    EXPECT_EQ( 0, mixer.bytesAvailable() );

    mixer.play( std::vector<float>( 4, 0.5F ) );

    // Une voix de quatre echantillons, un canal, des flottants de quatre octets.
    EXPECT_EQ( static_cast<qint64>( 4 * sizeof( float ) ), mixer.bytesAvailable() );

    // Et le signal qui reveille le peripherique a bien ete emis.
    int readyReadCount = 0;

    const auto connection = QObject::connect( &mixer, &QIODevice::readyRead, [&readyReadCount]() {
        ++readyReadCount;
    } );

    mixer.play( std::vector<float>( 4, 0.5F ) );

    EXPECT_EQ( 1, readyReadCount );

    QObject::disconnect( connection );

    // Une fois la voix consommee, plus rien a annoncer.
    mixer.readFrames( 4 );

    EXPECT_EQ( 0, mixer.bytesAvailable() );
}

TEST( AudioMixerTest, a_finished_sound_stops_playing )
{
    TestableMixer mixer{ TEST_SAMPLE_RATE, TEST_CHANNELS };

    mixer.play( std::vector<float>( 2, 1.0F ) );

    EXPECT_TRUE( mixer.isPlaying() );

    const std::vector<float> frames = mixer.readFrames( 8 );

    EXPECT_FLOAT_EQ( 1.0F, frames.at( 0 ) );
    EXPECT_FLOAT_EQ( 1.0F, frames.at( 1 ) );

    // Past its end, the sound contributes nothing any more.
    for( std::size_t index = 2; index < frames.size(); ++index )
    {
        EXPECT_FLOAT_EQ( 0.0F, frames.at( index ) );
    }

    EXPECT_FALSE( mixer.isPlaying() );
}

TEST( AudioMixerTest, everything_can_be_dropped_at_once )
{
    TestableMixer mixer{ TEST_SAMPLE_RATE, TEST_CHANNELS };

    mixer.play( std::vector<float>( 64, 1.0F ) );

    mixer.clear();

    EXPECT_FALSE( mixer.isPlaying() );
    EXPECT_EQ( 0, mixer.voiceCount() );
}

TEST( AudioMixerTest, the_gain_is_applied_to_the_voice )
{
    TestableMixer mixer{ TEST_SAMPLE_RATE, TEST_CHANNELS };

    mixer.play( std::vector<float>( 4, 0.25F ), 2.0F );

    for( const float frame : mixer.readFrames( 4 ) )
    {
        EXPECT_FLOAT_EQ( 0.5F, frame );
    }
}

TEST( AudioMixerTest, a_mono_sound_reaches_every_channel )
{
    TestableMixer mixer{ TEST_SAMPLE_RATE, 2 };

    mixer.play( std::vector<float>( 2, 0.4F ) );

    const std::vector<float> frames = mixer.readFrames( 4 );

    // Two frames of two channels: left and right carry the same signal, or the sound comes out of one ear.
    EXPECT_FLOAT_EQ( 0.4F, frames.at( 0 ) );
    EXPECT_FLOAT_EQ( 0.4F, frames.at( 1 ) );
    EXPECT_FLOAT_EQ( 0.4F, frames.at( 2 ) );
    EXPECT_FLOAT_EQ( 0.4F, frames.at( 3 ) );
}

TEST( AudioMixerTest, a_loud_mix_is_clamped_rather_than_wrapped )
{
    TestableMixer mixer{ TEST_SAMPLE_RATE, TEST_CHANNELS };

    mixer.play( std::vector<float>( 4, 0.9F ) );
    mixer.play( std::vector<float>( 4, 0.9F ) );

    for( const float frame : mixer.readFrames( 4 ) )
    {
        EXPECT_FLOAT_EQ( 1.0F, frame );
    }
}

}    // namespace musichien::infrastructure
