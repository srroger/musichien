#include "domain/audio/VoicePreFilter.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace musichien::domain
{
namespace
{

constexpr double SAMPLE_RATE = 44100.0;

TEST( VoicePreFilterTest, RemovesTheConstantOffsetThatRumbleIsMadeOf )
{
    // UN GRONDEMENT EST SOUVENT UN DECALAGE CONTINU : c'est ce que le passe-haut doit retirer. On lui donne un signal
    // constant, et sa sortie doit rejoindre zero au bout de quelques millisecondes.
    VoicePreFilter filter;
    filter.configure( SAMPLE_RATE );

    double last = 0.0;

    for( int index = 0; index < 8000; ++index )
    {
        last = filter.processSample( 1.0 );
    }

    EXPECT_LT( std::abs( last ), 0.05 );
}

TEST( VoicePreFilterTest, KeepsTheNotesOfTheVoice )
{
    // LE CONTRAIRE DU TEST PRECEDENT, ET C'EST LE PLUS IMPORTANT : un la de 440 Hz doit TRAVERSER le filtre. Un
    // passe-haut qui coupe la voix serait pire que pas de filtre du tout.
    VoicePreFilter filter;
    filter.configure( SAMPLE_RATE );

    double peak = 0.0;

    for( int index = 0; index < 8000; ++index )
    {
        const double sample =
          std::sin( 2.0 * std::numbers::pi * 440.0 * static_cast<double>( index ) / SAMPLE_RATE );

        peak = std::max( peak, std::abs( filter.processSample( sample ) ) );
    }

    EXPECT_GT( peak, 0.9 );
}

TEST( VoicePreFilterTest, TheFirstWindowIsNeverAVoice )
{
    // Sans cela, la premiere note chantee serait jugee contre un plancher de zero - donc acceptee, et le bruit avec.
    VoicePreFilter filter;

    EXPECT_FALSE( filter.isVoiceLevel( 0.01 ) );
}

TEST( VoicePreFilterTest, TheBackgroundNoiseIsNotAVoice )
{
    VoicePreFilter filter;

    EXPECT_FALSE( filter.isVoiceLevel( 0.01 ) );    // la fenetre qui pose le plancher
    EXPECT_FALSE( filter.isVoiceLevel( 0.01 ) );
    EXPECT_FALSE( filter.isVoiceLevel( 0.012 ) );
}

TEST( VoicePreFilterTest, ASungNoteStaysAVoiceForAsLongAsItIsHeld )
{
    // LE PIEGE QUE CE TEST FERME : une note tenue dure plusieurs fenetres. Si chacune faisait monter le plancher vers
    // son propre niveau, la note finirait sous le seuil qu'elle a fait monter, et s'exclurait elle-meme en plein milieu.
    VoicePreFilter filter;

    EXPECT_FALSE( filter.isVoiceLevel( 0.01 ) );

    for( int window = 0; window < 200; ++window )
    {
        EXPECT_TRUE( filter.isVoiceLevel( 0.05 ) ) << "fenetre " << window;
    }
}

TEST( VoicePreFilterTest, TheFloorFollowsRisingBackgroundNoise )
{
    // Un lieu qui devient bruyant : le plancher doit monter, sinon le gate laisserait tout passer.
    VoicePreFilter filter;

    EXPECT_FALSE( filter.isVoiceLevel( 0.01 ) );

    for( int window = 0; window < 500; ++window )
    {
        static_cast<void>( filter.isVoiceLevel( 0.02 ) );
    }

    // Le plancher a suivi le bruit : une fenetre qui valait une voix au debut n'en est plus une.
    EXPECT_FALSE( filter.isVoiceLevel( 0.02 ) );
}

}    // namespace
}    // namespace musichien::domain
