#include "domain/audio/FastFourierTransform.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <vector>

namespace musichien::domain
{

namespace
{

constexpr std::size_t SIZE = 1024;

// Un signal pur dont la frequence tombe EXACTEMENT sur un point du spectre : c'est le seul cas ou l'on sait d'avance
// ce que la transformee doit repondre, donc le seul qui prouve vraiment quelque chose.
[[nodiscard]] std::vector<double> sineAtBin( std::size_t p_bin )
{
    std::vector<double> signal( SIZE, 0.0 );

    for( std::size_t index = 0; index < SIZE; ++index )
    {
        signal.at( index ) = std::sin( 2.0 * M_PI * static_cast<double>( p_bin ) * static_cast<double>( index )
                                       / static_cast<double>( SIZE ) );
    }

    return signal;
}

[[nodiscard]] double magnitudeAt( const std::vector<double> & p_real, const std::vector<double> & p_imaginary, std::size_t p_bin )
{
    return std::hypot( p_real.at( p_bin ), p_imaginary.at( p_bin ) );
}

}    // namespace

TEST( FastFourierTransformTest, a_sine_falls_into_its_own_bin_and_nowhere_else )
{
    constexpr std::size_t BIN = 64;

    std::vector<double> real = sineAtBin( BIN );
    std::vector<double> imaginary( SIZE, 0.0 );

    FastFourierTransform::apply( real, imaginary, false );

    // Un sinus d'amplitude 1 met toute son energie dans un point du spectre, et la moitie de la taille en amplitude :
    // l'autre moitie est allee dans le point symetrique, celui des frequences negatives.
    EXPECT_NEAR( magnitudeAt( real, imaginary, BIN ), static_cast<double>( SIZE ) / 2.0, 1e-6 );
    EXPECT_NEAR( magnitudeAt( real, imaginary, SIZE - BIN ), static_cast<double>( SIZE ) / 2.0, 1e-6 );

    // Et partout ailleurs : rien.
    for( std::size_t bin = 0; bin < SIZE; ++bin )
    {
        if( ( bin != BIN ) && ( bin != ( SIZE - BIN ) ) )
        {
            EXPECT_NEAR( magnitudeAt( real, imaginary, bin ), 0.0, 1e-6 ) << "point " << bin;
        }
    }
}

TEST( FastFourierTransformTest, a_constant_signal_is_a_single_line_at_zero )
{
    std::vector<double> real( SIZE, 3.0 );
    std::vector<double> imaginary( SIZE, 0.0 );

    FastFourierTransform::apply( real, imaginary, false );

    // Un signal constant n'a qu'une composante : celle qui ne bouge pas.
    EXPECT_NEAR( real.at( 0 ), 3.0 * static_cast<double>( SIZE ), 1e-6 );

    for( std::size_t bin = 1; bin < SIZE; ++bin )
    {
        EXPECT_NEAR( magnitudeAt( real, imaginary, bin ), 0.0, 1e-6 ) << "point " << bin;
    }
}

TEST( FastFourierTransformTest, the_round_trip_gives_the_signal_back )
{
    std::vector<double> real = sineAtBin( 7 );

    for( std::size_t index = 0; index < SIZE; ++index )
    {
        real.at( index ) += 0.3 * std::cos( 2.0 * M_PI * 30.0 * static_cast<double>( index ) / static_cast<double>( SIZE ) );
    }

    const std::vector<double> original = real;
    std::vector<double> imaginary( SIZE, 0.0 );

    FastFourierTransform::apply( real, imaginary, false );
    FastFourierTransform::apply( real, imaginary, true );

    // Aller et retour : la transformee inverse defait exactement ce que la directe a fait. C'est ce que l'accordeur
    // demande quand il calcule une autocorrelation par transformee.
    for( std::size_t index = 0; index < SIZE; ++index )
    {
        EXPECT_NEAR( real.at( index ), original.at( index ), 1e-9 ) << "echantillon " << index;
        EXPECT_NEAR( imaginary.at( index ), 0.0, 1e-9 ) << "echantillon " << index;
    }
}

TEST( FastFourierTransformTest, the_energy_is_conserved )
{
    std::vector<double> real = sineAtBin( 100 );
    std::vector<double> imaginary( SIZE, 0.0 );

    double energyBefore = 0.0;

    for( const double value : real )
    {
        energyBefore += value * value;
    }

    FastFourierTransform::apply( real, imaginary, false );

    double energyAfter = 0.0;

    for( std::size_t bin = 0; bin < SIZE; ++bin )
    {
        energyAfter += ( real.at( bin ) * real.at( bin ) ) + ( imaginary.at( bin ) * imaginary.at( bin ) );
    }

    // Parseval : l'energie ne se perd pas, elle change seulement de cote. Une transformee fausse d'un facteur la
    // signalerait ici, et nulle part ailleurs.
    EXPECT_NEAR( energyBefore * static_cast<double>( SIZE ), energyAfter, 1e-6 );
}

TEST( FastFourierTransformTest, only_powers_of_two_are_supported )
{
    EXPECT_TRUE( FastFourierTransform::supportsSize( 1 ) );
    EXPECT_TRUE( FastFourierTransform::supportsSize( 1024 ) );
    EXPECT_TRUE( FastFourierTransform::supportsSize( 8192 ) );

    EXPECT_FALSE( FastFourierTransform::supportsSize( 0 ) );
    EXPECT_FALSE( FastFourierTransform::supportsSize( 1000 ) );
    EXPECT_FALSE( FastFourierTransform::supportsSize( 4097 ) );

    EXPECT_EQ( 8192U, FastFourierTransform::nextSize( 4097 ) );
    EXPECT_EQ( 4096U, FastFourierTransform::nextSize( 4096 ) );
}

TEST( FastFourierTransformTest, a_zero_padded_sine_still_shows_its_peak )
{
    // 8192 points dont seuls les premiers portent un signal : c'est exactement ce que fait l'accordeur quand il
    // complete sa fenetre de zeros avant de mesurer. Le pic doit tomber sur le point attendu.
    constexpr std::size_t SIZE = 8192;
    constexpr std::size_t HALF = 4096;
    constexpr double RATE = 44100.0;
    constexpr double FREQUENCY = 4186.01;

    std::vector<double> real( SIZE, 0.0 );
    std::vector<double> imaginary( SIZE, 0.0 );

    for( std::size_t index = 0; index < HALF; ++index )
    {
        real.at( index ) = std::sin( 2.0 * M_PI * FREQUENCY * static_cast<double>( index ) / RATE );
    }

    FastFourierTransform::apply( real, imaginary, false );

    std::size_t best = 0;
    double bestPower = 0.0;

    for( std::size_t bin = 0; bin <= ( SIZE / 2 ); ++bin )
    {
        const double power = ( real.at( bin ) * real.at( bin ) ) + ( imaginary.at( bin ) * imaginary.at( bin ) );

        if( power > bestPower )
        {
            bestPower = power;
            best = bin;
        }
    }

    // 4186 Hz a 44,1 kHz sur 8192 points : le point 778.
    EXPECT_EQ( 778U, best );
    EXPECT_GT( bestPower, 0.0 );
}

TEST( FastFourierTransformTest, a_size_it_cannot_handle_is_left_untouched )
{
    std::vector<double> real{ 1.0, 2.0, 3.0 };
    std::vector<double> imaginary{ 0.0, 0.0, 0.0 };

    FastFourierTransform::apply( real, imaginary, false );

    // Une taille qui n'est pas une puissance de deux : on ne fait rien plutot que de rendre un resultat faux.
    EXPECT_DOUBLE_EQ( 1.0, real.at( 0 ) );
    EXPECT_DOUBLE_EQ( 2.0, real.at( 1 ) );
    EXPECT_DOUBLE_EQ( 3.0, real.at( 2 ) );
}

}    // namespace musichien::domain
