#include "domain/audio/FastFourierTransform.h"

#include <cmath>
#include <cstddef>
#include <numbers>
#include <utility>

namespace musichien::domain
{

bool FastFourierTransform::supportsSize( std::size_t p_size ) noexcept
{
    return ( p_size > 0 ) && ( ( p_size & ( p_size - 1 ) ) == 0 );
}

std::size_t FastFourierTransform::nextSize( std::size_t p_size ) noexcept
{
    std::size_t size = 1;

    while( size < p_size )
    {
        size <<= 1;
    }

    return size;
}

void FastFourierTransform::apply( std::vector<double> & p_real, std::vector<double> & p_imaginary, bool p_inverse )
{
    const std::size_t size = p_real.size();

    if( ( size < 2 ) || ( p_imaginary.size() != size ) || !supportsSize( size ) )
    {
        return;
    }

    // Le tri par inversion de bits : la transformee iterative lit ses entrees dans un ordre qui melange les indices,
    // comme les cartes d'un jeu qu'on bat en separant les paires.
    for( std::size_t index = 1, reversed = 0; index < size; ++index )
    {
        std::size_t bit = size >> 1;

        while( ( bit != 0 ) && ( ( reversed & bit ) != 0 ) )
        {
            reversed ^= bit;
            bit >>= 1;
        }

        reversed ^= bit;

        if( index < reversed )
        {
            std::swap( p_real[index], p_real[reversed] );
            std::swap( p_imaginary[index], p_imaginary[reversed] );
        }
    }

    // Le signe de l'exposant : une transformee directe tourne dans un sens, l'inverse dans l'autre.
    const double sign = p_inverse ? 1.0 : -1.0;

    // Puis les etapes : des paires de deux, puis de quatre, puis de huit... jusqu'a la taille entiere.
    for( std::size_t length = 2; length <= size; length <<= 1 )
    {
        const double angle = sign * 2.0 * std::numbers::pi / static_cast<double>( length );
        const double stepReal = std::cos( angle );
        const double stepImaginary = std::sin( angle );

        for( std::size_t start = 0; start < size; start += length )
        {
            double twiddleReal = 1.0;
            double twiddleImaginary = 0.0;

            for( std::size_t offset = 0; offset < ( length / 2 ); ++offset )
            {
                const std::size_t even = start + offset;
                const std::size_t odd = even + ( length / 2 );

                // Une multiplication complexe, ecrite en clair : le produit de la seconde moitie par la rotation.
                const double rotatedReal = ( p_real[odd] * twiddleReal ) - ( p_imaginary[odd] * twiddleImaginary );
                const double rotatedImaginary =
                  ( p_real[odd] * twiddleImaginary ) + ( p_imaginary[odd] * twiddleReal );

                p_real[odd] = p_real[even] - rotatedReal;
                p_imaginary[odd] = p_imaginary[even] - rotatedImaginary;
                p_real[even] += rotatedReal;
                p_imaginary[even] += rotatedImaginary;

                // Et la rotation avance d'un pas, sans jamais rappeler cosinus ni sinus.
                const double nextReal = ( twiddleReal * stepReal ) - ( twiddleImaginary * stepImaginary );

                twiddleImaginary = ( twiddleReal * stepImaginary ) + ( twiddleImaginary * stepReal );
                twiddleReal = nextReal;
            }
        }
    }

    if( p_inverse )
    {
        const double scale = 1.0 / static_cast<double>( size );

        for( std::size_t index = 0; index < size; ++index )
        {
            p_real[index] *= scale;
            p_imaginary[index] *= scale;
        }
    }
}

}    // namespace musichien::domain
