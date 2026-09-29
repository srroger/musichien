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

// Un demi-ton d'ecart, c'est 6 % : cette tolerance est donc quatre fois plus stricte que ce que l'oreille demande.
//
// Elle ne s'elargit plus avec la hauteur, et c'est le progres de cette version : l'affinage par le spectre corrige la
// perte de finesse du comptage dans les aigus. Le si0 d'une basse et le do le plus haut d'un piano sont desormais lus
// a mieux d'un pour cent, comme les notes medium.
[[nodiscard]] double toleranceFor( double p_frequencyHz )
{
    (void)p_frequencyHz;

    return 0.01;
}

void expectReadAs( double p_expectedHz )
{
    const double estimated = PitchEstimator::estimate( voice( p_expectedHz ), SAMPLE_RATE );

    EXPECT_NEAR( estimated, p_expectedHz, p_expectedHz * toleranceFor( p_expectedHz ) )
      << "une note de " << p_expectedHz << " Hz a ete lue " << estimated << " Hz";
}

}    // namespace

TEST( PitchEstimatorTest, a_pure_tone_is_read_at_its_own_frequency )
{
    for( const double frequencyHz : { 82.4, 110.0, 220.0, 440.0, 880.0, 2093.0, 4186.01 } )
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
    for( const double frequencyHz : { 660.0, 880.0, 1000.0, 1200.0, 1500.0, 2000.0, 3000.0, 4000.0 } )
    {
        const double estimated = PitchEstimator::estimate( voice( frequencyHz ), SAMPLE_RATE );

        EXPECT_GT( estimated, frequencyHz * 0.9 ) << frequencyHz << " Hz lu " << estimated;
        EXPECT_LT( estimated, frequencyHz * 1.1 ) << frequencyHz << " Hz lu " << estimated;
    }
}

TEST( PitchEstimatorTest, a_sung_note_is_read_through_its_harmonics )
{
    // Du si0 d'une basse cinq cordes au do le plus haut d'un piano : toute la tessiture des instruments.
    expectReadAs( 30.87 );
    expectReadAs( 41.20 );
    expectReadAs( 55.00 );
    expectReadAs( 82.41 );
    expectReadAs( 110.00 );
    expectReadAs( 220.00 );
    expectReadAs( 440.00 );
    expectReadAs( 880.00 );
    expectReadAs( 1046.50 );
    expectReadAs( 2093.00 );
    expectReadAs( 2637.02 );
    expectReadAs( 3135.96 );
    expectReadAs( 3520.00 );
    expectReadAs( 4186.01 );
}

// Une note aigue etait refusee des qu'elle sortait de la plage de recherche : l'accordeur se taisait au moment ou la
// note devenait interessante. Mieux vaut une hauteur approximative qu'aucune, donc plus rien n'est rejete.
TEST( PitchEstimatorTest, a_note_beyond_the_old_ceiling_is_still_read )
{
    const double estimated = PitchEstimator::estimate( voice( 1700.0 ), SAMPLE_RATE );

    EXPECT_GT( estimated, 0.0 );
    EXPECT_NEAR( estimated, 1700.0, 1700.0 * toleranceFor( 1700.0 ) );
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
    expectReadAs( PitchEstimator::MAXIMUM_FREQUENCY_HZ - 200.0 );
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
