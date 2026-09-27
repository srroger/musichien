#include "domain/rhythm/RhythmPattern.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace musichien::domain
{

RhythmPattern::RhythmPattern( std::string_view p_name, int p_beatsPerBar, std::vector<RhythmHit> p_hits )
  : m_name{ p_name }
  , m_beatsPerBar{ std::max( 1, p_beatsPerBar ) }
  , m_hits{ std::move( p_hits ) }
{
    // Kept in time order, which is what lets the player's taps be compared to the onsets in a single pass.
    std::ranges::sort( m_hits, {}, &RhythmHit::beat );
}

const std::vector<RhythmPattern> & allRhythmPatterns()
{
    // Built once, and never modified: the list is a fact of the domain, not a state of the game.
    static const std::vector<RhythmPattern> patterns{
      // Le binaire : la caisse claire sur les temps 2 et 4, la grosse caisse sur 1 et 3. C'est LA cellule que
      // tout le monde reconnait, et le point de depart logique.
      RhythmPattern{ "Binaire", 4, { { 0.0, Drum::Kick, true }, { 1.0, Drum::Snare }, { 2.0, Drum::Kick }, { 3.0, Drum::Snare } } },

      // La valse : un temps fort et deux temps faibles, "boum tcha tcha".
      RhythmPattern{ "Valse", 3, { { 0.0, Drum::Kick, true }, { 1.0, Drum::Snare }, { 2.0, Drum::Snare } } },

      // Le contretemps : la caisse claire tombe ENTRE les temps. C'est ce qui fait la bossa.
      RhythmPattern{ "Bossa", 4, { { 0.0, Drum::Kick, true }, { 1.5, Drum::Snare }, { 2.0, Drum::Kick }, { 3.5, Drum::Snare } } },

      // Une clave : trois frappes serrees puis deux ecartees, l'ame des musiques afro-cubaines.
      RhythmPattern{ "Clave", 4, { { 0.0, Drum::Snare, true }, { 0.75, Drum::Snare }, { 2.0, Drum::Kick }, { 3.0, Drum::Snare }, { 3.5, Drum::Kick } } },

      // Le shuffle : des croches inegales, la moitie du jazz et du blues. La deuxieme croche est RETARDEE.
      RhythmPattern{ "Shuffle", 4, { { 0.0, Drum::Kick, true }, { 0.66, Drum::HiHat }, { 1.0, Drum::Snare }, { 1.66, Drum::HiHat }, { 2.0, Drum::Kick }, { 2.66, Drum::HiHat }, { 3.0, Drum::Snare }, { 3.66, Drum::HiHat } } },
    };

    return patterns;
}

double distanceToNearestOnsetInBeats( const RhythmPattern & p_pattern, double p_positionInBeats ) noexcept
{
    const auto hits = p_pattern.hits();

    if( hits.empty() )
    {
        return 0.0;
    }

    const double loopLength = static_cast<double>( p_pattern.beatsPerBar() );

    // The position folded back into one loop: the pattern repeats for ever, so a tap at 4.2 beats is a tap at 0.2.
    double position = std::fmod( p_positionInBeats, loopLength );

    if( position < 0.0 )
    {
        position += loopLength;
    }

    double bestDistance = loopLength;

    for( const RhythmHit & hit : hits )
    {
        // The distance is measured the SHORT way round, because the loop has no beginning: a tap just before the
        // downbeat is close to it, not almost a whole bar away from it.
        const double direct = std::abs( position - hit.beat );
        const double wrapped = loopLength - direct;

        bestDistance = std::min( bestDistance, std::min( direct, wrapped ) );
    }

    return bestDistance;
}

}    // namespace musichien::domain
