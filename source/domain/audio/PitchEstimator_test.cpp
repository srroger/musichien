#include "domain/audio/PitchEstimator.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <vector>

namespace musichien::domain
{

namespace
{

constexpr double SAMPLE_RATE = 44100.0;

// Un son pur : la verite la plus simple qu'un estimateur de periode doive retrouver.
[[nodiscard]] std::vector<double> sine( double p_frequencyHz )
{
    std::vector<double> window( PitchEstimator::WINDOW_SIZE, 0.0 );

    for( std::size_t index = 0; index < window.size(); ++index )
    {
        const double time = static_cast<double>( index ) / SAMPLE_RATE;

        window.at( index ) = std::sin( 2.0 * M_PI * p_frequencyHz * time );
    }

    return window;
}

// Une voix : la fondamentale et ses harmoniques, qui s'eteignent en 1/h. C'est ce que YIN doit encaisser en vrai - un
// son ou la periode est aussi celle de tous les multiples.
[[nodiscard]] std::vector<double> voice( double p_frequencyHz )
{
    std::vector<double> window( PitchEstimator::WINDOW_SIZE, 0.0 );

    for( std::size_t index = 0; index < window.size(); ++index )
    {
        const double time = static_cast<double>( index ) / SAMPLE_RATE;

        double value = 0.0;

        for( int harmonic = 1; harmonic <= 8; ++harmonic )
        {
            const double harmonicHz = p_frequencyHz * static_cast<double>( harmonic );

            if( harmonicHz > ( SAMPLE_RATE / 2.4 ) )
            {
                break;
            }

            value += std::sin( 2.0 * M_PI * harmonicHz * time ) / static_cast<double>( harmonic );
        }

        window.at( index ) = value * 0.2;
    }

    return window;
}

// Un demi-ton d'ecart, c'est 6 % : ces tolerances sont donc bien plus strictes que ce que l'oreille demande.
//
// La tolerance s'elargit au-dessus de 1000 Hz, et c'est une limite CONNUE de la methode : la periode y tombe sous la
// trentaine d'echantillons, donc un decalage entier separe deja les notes de plusieurs pour cent, et l'interpolation
// n'en retrouve qu'une partie. Cela reste infiniment mieux que l'octave entiere que l'algorithme lisait auparavant.
constexpr double HIGH_NOTE_HZ = 1000.0;

void expectReadAs( double p_expectedHz )
{
    const double tolerance = ( p_expectedHz > HIGH_NOTE_HZ ) ? 0.03 : 0.01;

    const double estimated = PitchEstimator::estimate( voice( p_expectedHz ), SAMPLE_RATE );

    EXPECT_NEAR( estimated, p_expectedHz, p_expectedHz * tolerance )
      << "une note de " << p_expectedHz << " Hz a ete lue " << estimated << " Hz";
}

}    // namespace

TEST( PitchEstimatorTest, a_pure_tone_is_read_at_its_own_frequency )
{
    for( const double frequencyHz : { 82.4, 110.0, 220.0, 440.0, 880.0 } )
    {
        const double estimated = PitchEstimator::estimate( sine( frequencyHz ), SAMPLE_RATE );

        EXPECT_NEAR( estimated, frequencyHz, frequencyHz * 0.01 ) << frequencyHz << " Hz";
    }
}

// Le cas qui a motive l'extraction de cet estimateur : au-dela du la aigu, il lisait la MOITIE de la frequence. Une
// note aigue est celle dont la periode est courte, donc celle ou une fonction de difference mal normalisee se trompe.
TEST( PitchEstimatorTest, the_high_notes_are_not_read_an_octave_below )
{
    // Aucun de ces cas ne doit donner la moitie : c'est precisement l'erreur d'octave qu'on cherche a interdire.
    for( const double frequencyHz : { 660.0, 880.0, 1000.0, 1200.0, 1500.0 } )
    {
        const double estimated = PitchEstimator::estimate( voice( frequencyHz ), SAMPLE_RATE );

        EXPECT_GT( estimated, frequencyHz * 0.9 ) << frequencyHz << " Hz lu " << estimated;
        EXPECT_LT( estimated, frequencyHz * 1.1 ) << frequencyHz << " Hz lu " << estimated;
    }
}

TEST( PitchEstimatorTest, a_sung_note_is_read_through_its_harmonics )
{
    expectReadAs( 220.0 );
    expectReadAs( 440.0 );
    expectReadAs( 880.0 );
    expectReadAs( 1200.0 );
    expectReadAs( 1500.0 );
}
TEST( PitchEstimatorTest, a_voice_going_up_keeps_going_up )
{
    // Le defaut rapporte par Roger : en montant, l'affichage redescendait. Chaque note doit etre lue plus haut que la
    // precedente - un estimateur qui perd une octave en route fait exactement l'inverse.
    double previous = 0.0;

    for( const double frequencyHz : { 220.0, 330.0, 440.0, 550.0, 660.0, 880.0, 1000.0 } )
    {
        const double estimated = PitchEstimator::estimate( voice( frequencyHz ), SAMPLE_RATE );

        EXPECT_GT( estimated, previous ) << frequencyHz << " Hz lu " << estimated;
        EXPECT_NEAR( estimated, frequencyHz, frequencyHz * 0.02 ) << frequencyHz << " Hz";

        previous = estimated;
    }
}

TEST( PitchEstimatorTest, the_whole_range_is_covered )
{
    expectReadAs( PitchEstimator::MINIMUM_FREQUENCY_HZ + 5.0 );
    expectReadAs( PitchEstimator::MAXIMUM_FREQUENCY_HZ - 100.0 );
}

TEST( PitchEstimatorTest, silence_is_not_a_note )
{
    const std::vector<double> silence( PitchEstimator::WINDOW_SIZE, 0.0 );

    EXPECT_DOUBLE_EQ( 0.0, PitchEstimator::estimate( silence, SAMPLE_RATE ) );
}

TEST( PitchEstimatorTest, a_window_too_short_to_hold_a_period_says_nothing )
{
    const std::vector<double> half( PitchEstimator::WINDOW_SIZE / 2, 0.5 );

    EXPECT_DOUBLE_EQ( 0.0, PitchEstimator::estimate( half, SAMPLE_RATE ) );
}

TEST( PitchEstimatorTest, a_rate_that_makes_no_sense_says_nothing )
{
    EXPECT_DOUBLE_EQ( 0.0, PitchEstimator::estimate( sine( 440.0 ), 0.0 ) );
}

}    // namespace musichien::domain
