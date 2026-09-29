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

    // A drum hit on the same instant as the metronome click used to silence the click: the two sounds must be MIXED,
    // not queued.
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

// =====================================================================================================================
// Le METRONOME
//
// Ces tests sont la raison d'etre du chantier : ils verifient que le clic tombe sur l'ECHANTILLON exact que le tempo
// demande, et non « quelque part dans le tampon ». Un clic decale d'un seul echantillon passerait encore pour juste a
// l'oreille ; ce sont les tests qui garantissent qu'il ne l'est pas, et qu'il ne le deviendra jamais.
// =====================================================================================================================

TEST( AudioMixerTest, the_click_lands_on_the_sample_the_tempo_asks_for )
{
    TestableMixer mixer{ TEST_SAMPLE_RATE, TEST_CHANNELS };

    // Un clic d'un seul echantillon, pour que sa position se lise sans ambiguite.
    mixer.setMetronomeClicks( { 1.0F }, { 1.0F } );

    // 120 bpm a 48 kHz : un temps tous les 24000 echantillons.
    mixer.startMetronome( 120.0, 4 );

    // Le premier temps tombe a l'echantillon 0 : il sonne des la premiere frame.
    const std::vector<float> firstBlock = mixer.readFrames( 4 );
    EXPECT_FLOAT_EQ( 1.0F, firstBlock.at( 0 ) );
    EXPECT_FLOAT_EQ( 0.0F, firstBlock.at( 1 ) );

    // Rien pendant tout le reste du temps...
    for( const float frame : mixer.readFrames( 23996 ) )
    {
        EXPECT_FLOAT_EQ( 0.0F, frame );
    }

    // ... et le deuxieme temps tombe PILE sur la 24000e frame du flux, donc sur la premiere de ce bloc.
    const std::vector<float> secondBeat = mixer.readFrames( 4 );
    EXPECT_FLOAT_EQ( 0.7F, secondBeat.at( 0 ) ) << "le temps faible porte le gain du clic ordinaire";
    EXPECT_FLOAT_EQ( 0.0F, secondBeat.at( 1 ) );
}

TEST( AudioMixerTest, the_accented_click_marks_the_first_beat_of_every_bar )
{
    TestableMixer mixer{ TEST_SAMPLE_RATE, TEST_CHANNELS };

    // Deux clics IDENTIQUES : c'est le GAIN qui distingue le premier temps des autres, et le test lit donc le gain.
    mixer.setMetronomeClicks( { 1.0F }, { 1.0F } );

    // Trois temps par mesure, et un temps tous les 4800 echantillons (600 bpm : c'est un test, pas de la musique).
    mixer.startMetronome( 600.0, 3 );

    // Un bloc d'un temps pile : sa premiere frame porte le clic, quand il y en a un.
    const auto levelOfNextBeat = [&mixer]() { return mixer.readFrames( 4800 ).at( 0 ); };

    constexpr float ACCENTED = 1.0F;    // le gain du premier temps
    constexpr float PLAIN = 0.7F;       // celui des autres

    // Le premier temps d'une mesure est accentue, les deux suivants ne le sont pas...
    EXPECT_FLOAT_EQ( ACCENTED, levelOfNextBeat() );
    EXPECT_FLOAT_EQ( PLAIN, levelOfNextBeat() );
    EXPECT_FLOAT_EQ( PLAIN, levelOfNextBeat() );

    // ... et la mesure suivante ramene l'accent : le rang des temps est compte depuis le DEMARRAGE, jamais remis a zero
    // par une mesure - c'est ce qui empeche une mesure de se decaler toute seule.
    EXPECT_FLOAT_EQ( ACCENTED, levelOfNextBeat() );
}

TEST( AudioMixerTest, the_beat_is_counted_in_samples_and_not_by_a_clock )
{
    TestableMixer mixer{ TEST_SAMPLE_RATE, TEST_CHANNELS };

    mixer.setMetronomeClicks( { 1.0F }, { 1.0F } );
    mixer.startMetronome( 120.0, 4 );

    EXPECT_EQ( 0, mixer.framesWritten() );
    EXPECT_EQ( 0, mixer.metronomeBeatIndex() );

    // Le temps avance parce que le FLUX avance : c'est toute la difference avec un QTimer, qui avance parce que le
    // thread d'interface a bien voulu.
    mixer.readFrames( 24000 );

    EXPECT_EQ( 24000, mixer.framesWritten() );
    EXPECT_EQ( 1, mixer.metronomeBeatIndex() );

    mixer.readFrames( 24000 );

    EXPECT_EQ( 2, mixer.metronomeBeatIndex() );
}

TEST( AudioMixerTest, stopping_the_metronome_takes_back_the_clicks_that_have_not_sounded )
{
    TestableMixer mixer{ TEST_SAMPLE_RATE, TEST_CHANNELS };

    mixer.setMetronomeClicks( { 1.0F }, { 1.0F } );
    mixer.startMetronome( 120.0, 4 );

    // Le premier temps est passe, le deuxieme est deja planifie.
    mixer.readFrames( 1000 );

    mixer.stopMetronome();

    EXPECT_FALSE( mixer.isMetronomeRunning() );

    for( const float frame : mixer.readFrames( 30000 ) )
    {
        EXPECT_FLOAT_EQ( 0.0F, frame ) << "un clic planifie avant l'arret ne doit pas sonner apres";
    }
}

TEST( AudioMixerTest, what_is_judged_is_what_is_heard_and_not_what_is_written )
{
    TestableMixer mixer{ TEST_SAMPLE_RATE, TEST_CHANNELS };

    mixer.setMetronomeClicks( { 1.0F }, { 1.0F } );
    mixer.startMetronome( 120.0, 4 );

    // Le tampon de sortie : ce que le mixer a ecrit et que l'oreille n'a pas encore entendu.
    mixer.setOutputLatencyFrames( 512 );

    mixer.readFrames( 512 );

    // Les 512 echantillons sont ecrits, mais l'oreille est ENCORE sur le premier temps : une frappe a cet instant-la
    // est une frappe sur le temps, et non un demi-centieme de seconde en avance.
    EXPECT_EQ( 512, mixer.framesWritten() );
    EXPECT_EQ( 0, mixer.metronomeBeatIndex() );
    EXPECT_DOUBLE_EQ( 0.0, mixer.metronomeElapsedMs() );
}

TEST( AudioMixerTest, a_metronome_that_never_started_has_nothing_to_announce )
{
    TestableMixer mixer{ TEST_SAMPLE_RATE, TEST_CHANNELS };

    // Sans metronome et sans son, le flux est vide : le peripherique peut rendre l'appareil audio, et c'est ce qui
    // evite de vider la batterie d'un telephone entre deux exercices.
    EXPECT_FALSE( mixer.isMetronomeRunning() );
    EXPECT_EQ( 0, mixer.bytesAvailable() );
}

}    // namespace musichien::infrastructure
