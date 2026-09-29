#include "domain/audio/PitchEstimator.h"

#include "domain/audio/FastFourierTransform.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace musichien::domain
{

namespace
{

// La hauteur affinee par le SPECTRE.
//
// Compter des echantillons perd sa finesse des que l'oscillation devient courte : a 4000 Hz elle ne fait plus que
// onze points, et un seul point d'ecart vaut deja plus d'un demi-ton. Le spectre, lui, separe les frequences de
// (taux / taille de la transformee) pres - soit cinq hertz pour les 8192 points employes ici - ce qui est bien plus
// fin dans les aigus.
//
// On ne cherche JAMAIS ailleurs qu'autour de la hauteur deja trouvee : c'est ce qui interdit de tomber sur une
// harmonique, ce que ferait une recherche du plus fort pic du spectre - et c'est le piege classique, celui qui fait
// afficher un la aigu quand la voix chante un la medium.
[[nodiscard]] double refinedWithSpectrum( double p_coarseHz, const std::vector<double> & p_power, double p_sampleRateHz, std::size_t p_transformSize )
{
    const double binWidth = p_sampleRateHz / static_cast<double>( p_transformSize );

    // Le spectre n'est interessant que s'il est PLUS FIN que ce que le comptage a donne. Dans les graves il ne l'est
    // pas, et c'est alors le comptage qui gagne : un bon accordeur prend la mesure la plus fine des deux.
    constexpr double FINER_THAN_COARSE = 0.005;

    if( binWidth > ( p_coarseHz * FINER_THAN_COARSE ) )
    {
        return p_coarseHz;
    }

    const auto centre = static_cast<std::size_t>( std::lround( p_coarseHz / binWidth ) );

    if( ( centre < 2 ) || ( ( centre + 2 ) >= static_cast<std::size_t>( p_power.size() ) ) )
    {
        return p_coarseHz;
    }

    // Un quart de la hauteur cherchee de chaque cote. C'est large - et c'est necessaire : dans les aigus, le comptage
    // se trompe volontiers de plusieurs pour cent, et une fenetre trop etroite ne saurait pas le rattraper. Un quart
    // reste tres loin d'une octave, donc une harmonique ne peut pas s'y trouver.
    const std::size_t span = std::max<std::size_t>( 2, centre / 4 );
    const std::size_t lastBin = p_power.size() - 2;

    if( ( centre < span ) || ( ( centre + span ) > lastBin ) )
    {
        return p_coarseHz;
    }

    std::size_t best = centre;

    for( std::size_t bin = centre - span; bin <= ( centre + span ); ++bin )
    {
        if( p_power.at( bin ) > p_power.at( best ) )
        {
            best = bin;
        }
    }

    if( ( best == 0 ) || ( best >= lastBin ) )
    {
        return p_coarseHz;
    }

    // La parabole, comme pour le comptage : le vrai pic tombe entre deux points du spectre.
    const double below = p_power.at( best - 1 );
    const double here = p_power.at( best );
    const double above = p_power.at( best + 1 );

    const double denominator = 2.0 * ( ( 2.0 * here ) - above - below );

    double refinedBin = static_cast<double>( best );

    if( std::abs( denominator ) > 1e-30 )
    {
        refinedBin += ( below - above ) / denominator;
    }

    if( refinedBin <= 0.0 )
    {
        return p_coarseHz;
    }

    return refinedBin * binWidth;
}

}    // namespace

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

    // ---- La transformee, une seule fois, pour deux usages ----
    //
    // Sa taille est le DOUBLE de la fenetre : l'autocorrelation se calcule par transformee, et une taille doublee
    // evite que la fin de la fenetre ne se replie sur son debut - le repliement circulaire, qui ferait croire a une
    // periodicite qui n'existe pas.
    const std::size_t transformSize = FastFourierTransform::nextSize( 2 * WINDOW_SIZE );

    std::vector<double> real( transformSize, 0.0 );
    std::vector<double> imaginary( transformSize, 0.0 );

    for( std::size_t index = 0; index < WINDOW_SIZE; ++index )
    {
        real.at( index ) = p_window[index];
    }

    FastFourierTransform::apply( real, imaginary, false );

    // Le spectre de puissance est mis de cote : c'est lui qui affinera la hauteur, a la fin.
    std::vector<double> power( ( transformSize / 2 ) + 1, 0.0 );

    for( std::size_t bin = 0; bin <= ( transformSize / 2 ); ++bin )
    {
        power.at( bin ) = ( real.at( bin ) * real.at( bin ) ) + ( imaginary.at( bin ) * imaginary.at( bin ) );
    }

    // ---- L'autocorrelation, par transformee ----
    //
    // C'est ce qui remplace la double boucle qui coutait le decalage le plus long multiplie par la fenetre entiere :
    // la transformee du spectre de puissance redonne la correlation du signal avec lui-meme pour TOUS les decalages a
    // la fois. Le cout passe de N carre a N log N, et c'est ce qui rend les notes graves abordables.
    for( std::size_t bin = 0; bin < transformSize; ++bin )
    {
        const std::size_t mirrored = std::min( bin, transformSize - bin );    // la symetrie d'un spectre reel

        real.at( bin ) = power.at( mirrored );
        imaginary.at( bin ) = 0.0;
    }

    FastFourierTransform::apply( real, imaginary, true );    // desormais real[tau] = la correlation au decalage tau

    // ---- La fonction de difference, developpee ----
    //
    // d(tau) = somme des (x[i] - x[i+tau]) au carre
    //        = somme des x[i] au carre + somme des x[i+tau] au carre - 2 * correlation(tau)
    //
    // Les deux sommes se lisent dans un cumul, donc l'accordeur ne paie que ce que la transformee a deja calcule.
    std::vector<double> cumulative( WINDOW_SIZE + 1, 0.0 );

    for( std::size_t index = 0; index < WINDOW_SIZE; ++index )
    {
        cumulative.at( index + 1 ) = cumulative.at( index ) + ( p_window[index] * p_window[index] );
    }

    const double energy = cumulative.at( WINDOW_SIZE );

    std::vector<double> difference( tauMax + 1, 0.0 );

    for( std::size_t tau = 1; tau <= tauMax; ++tau )
    {
        const double head = cumulative.at( WINDOW_SIZE - tau );    // les carres d'avant le decalage
        const double tail = energy - cumulative.at( tau );         // ceux d'apres

        difference.at( tau ) = head + tail - ( 2.0 * real.at( tau ) );
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

    // Et l'affinage par le spectre, qui prend le relais la ou compter des echantillons n'est plus assez fin.
    return refinedWithSpectrum( p_sampleRateHz / refinedTau, power, p_sampleRateHz, transformSize );
}

}    // namespace musichien::domain
