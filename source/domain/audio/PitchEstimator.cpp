#include "domain/audio/PitchEstimator.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace musichien::domain
{

double PitchEstimator::estimate( std::span<const double> p_window, double p_sampleRateHz )
{
    if( p_window.size() < WINDOW_SIZE || p_sampleRateHz <= 0.0 )
    {
        return 0.0;
    }

    // Le decalage le plus court interroge est celui de la note la plus aigue, le plus long celui de la plus grave.
    // Deux echantillons au minimum : un decalage d'un seul ne dit rien d'une periode.
    const auto tauMin =
      std::max<std::size_t>( 2, static_cast<std::size_t>( p_sampleRateHz / MAXIMUM_FREQUENCY_HZ ) );
    const auto tauMax =
      std::min( WINDOW_SIZE / 2, static_cast<std::size_t>( p_sampleRateHz / MINIMUM_FREQUENCY_HZ ) );

    if( tauMin >= tauMax )
    {
        return 0.0;
    }

    // La fonction de difference, pour TOUS les decalages - et non a partir du plus petit interroge.
    //
    // C'est le defaut qui a fait lire la moitie de la frequence sur les notes aigues. La normalisation qui suit divise
    // par la somme des differences depuis le debut : les decalages courts laisses a zero rendaient cette somme trop
    // petite, donc la fonction normalisee trop grande, et le premier minimum ne descendait sous le seuil qu'une
    // octave plus bas. Plus la note etait aigue, plus le biais etait fort - exactement ce qu'on entendait.
    std::vector<double> difference( tauMax + 1, 0.0 );

    for( std::size_t tau = 1; tau <= tauMax; ++tau )
    {
        double sum = 0.0;

        for( std::size_t sample = 0; sample + tau < WINDOW_SIZE; ++sample )
        {
            const double delta = p_window[sample] - p_window[sample + tau];

            sum += delta * delta;
        }

        difference[tau] = sum;
    }

    // La fonction de difference moyenne normalisee cumulativement : cmnd(tau) = difference(tau) * tau / somme(tau).
    // La multiplication par tau est ce qui retire son avantage au decalage long : sans elle, deux periodes
    // ressembleraient toujours plus que la periode elle-meme.
    std::vector<double> cmnd( tauMax + 1, 1.0 );

    double runningSum = 0.0;

    for( std::size_t tau = 1; tau <= tauMax; ++tau )
    {
        runningSum += difference[tau];

        cmnd[tau] = ( runningSum > 0.0 ) ? ( difference[tau] * static_cast<double>( tau ) / runningSum ) : 1.0;
    }

    // Le premier minimum local SOUS le seuil est la periode. Chercher un minimum, plutot que la premiere valeur sous
    // le seuil, evite de rapporter l'epaule du creux au lieu de son fond.
    std::size_t tau = tauMin;

    while( ( tau + 1 <= tauMax ) && ( ( cmnd[tau] >= THRESHOLD ) || ( cmnd[tau] >= cmnd[tau + 1] ) ) )
    {
        ++tau;
    }

    if( tau >= tauMax )
    {
        return 0.0;
    }

    // Interpolation parabolique : le vrai creux tombe entre deux echantillons, et la parabole qui passe par les trois
    // points voisins le trouve. Sans elle, une note aigue serait lue par sauts d'un demi-ton ou plus.
    const double below = cmnd[tau - 1];
    const double here = cmnd[tau];
    const double above = cmnd[tau + 1];

    const double denominator = 2.0 * ( ( 2.0 * here ) - above - below );

    double refinedTau = static_cast<double>( tau );

    if( std::abs( denominator ) > 1e-12 )
    {
        refinedTau += ( below - above ) / denominator;
    }

    if( refinedTau <= 0.0 )
    {
        return 0.0;
    }

    const double frequencyHz = p_sampleRateHz / refinedTau;

    // Une hauteur hors de la plage cherchee n'est pas une note : c'est du bruit qui a ressemble a une periode.
    if( ( frequencyHz < MINIMUM_FREQUENCY_HZ ) || ( frequencyHz > MAXIMUM_FREQUENCY_HZ ) )
    {
        return 0.0;
    }

    return frequencyHz;
}

}    // namespace musichien::domain
