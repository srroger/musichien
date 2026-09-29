#include "domain/rhythm/Rhythm.h"

#include <algorithm>
#include <cmath>

namespace musichien::domain
{

double beatDurationMs( double p_bpm ) noexcept
{
    if( p_bpm <= 0.0 )
    {
        return 0.0;
    }

    return 60000.0 / p_bpm;
}

double beatTimeMs( double p_bpm, std::size_t p_beatIndex ) noexcept
{
    return beatDurationMs( p_bpm ) * static_cast<double>( p_beatIndex );
}

BeatSchedule planNextBeat( double p_bpm, std::size_t p_beatIndex, double p_elapsedMs ) noexcept
{
    const double beatMs = beatDurationMs( p_bpm );

    if( beatMs <= 0.0 )
    {
        // Un metronome a l'arret n'a rien a planifier : ni battement, ni delai. Le domaine refuse deja les frappes
        // pour la meme raison, donc les deux disent la meme chose.
        return BeatSchedule{};
    }

    // Une horloge qui n'a pas encore demarre - ou qui a ete remise a zero - se lit comme zero, jamais comme un temps
    // negatif qui ferait sortir l'index de la grille.
    const double elapsedMs = std::max( p_elapsedMs, 0.0 );

    const double delayMs = beatTimeMs( p_bpm, p_beatIndex ) - elapsedMs;

    if( delayMs < -beatMs )
    {
        // Trop tard pour rattraper : la grille se recale, et le battement vise devient le premier dont l'echeance est
        // encore a venir.
        const std::size_t nextBeatIndex = static_cast<std::size_t>( elapsedMs / beatMs ) + 1;

        return BeatSchedule{ nextBeatIndex, beatTimeMs( p_bpm, nextBeatIndex ) - elapsedMs };
    }

    // A l'heure, ou en retard de moins d'un temps : on vise l'echeance d'origine, et le retard ne se reporte pas.
    return BeatSchedule{ p_beatIndex, std::max( delayMs, 0.0 ) };
}

HitQuality judgeDistance( double p_distanceMs ) noexcept
{
    const double distance = std::abs( p_distanceMs );

    if( distance <= PERFECT_WINDOW_MS )
    {
        return HitQuality::Perfect;
    }

    if( distance <= GOOD_WINDOW_MS )
    {
        return HitQuality::Good;
    }

    return HitQuality::Miss;
}

HitQuality judgeTap( double p_tapMs, double p_bpm ) noexcept
{
    const double beatMs = beatDurationMs( p_bpm );

    if( beatMs <= 0.0 )
    {
        return HitQuality::Miss;
    }

    // The nearest beat, measured by rounding the tap onto the grid of beats. A tap halfway between two beats lands
    // exactly on the rounding boundary and is then judged as a Miss by its distance, which is the honest answer.
    const double nearestBeatMs = std::round( p_tapMs / beatMs ) * beatMs;

    return judgeDistance( p_tapMs - nearestBeatMs );
}

}    // namespace musichien::domain
