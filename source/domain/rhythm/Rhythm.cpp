#include "domain/rhythm/Rhythm.h"

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

    const double distance = std::abs( p_tapMs - nearestBeatMs );

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

}    // namespace musichien::domain
