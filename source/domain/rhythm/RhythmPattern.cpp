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

namespace
{

// The position folded back into one loop: the pattern repeats for ever, so a tap at 4.2 beats is a tap at 0.2.
[[nodiscard]] double foldIntoLoop( double p_positionInBeats, double p_loopLength ) noexcept
{
    double position = std::fmod( p_positionInBeats, p_loopLength );

    if( position < 0.0 )
    {
        position += p_loopLength;
    }

    return position;
}

// How far a folded position lies from one onset, the SHORT way round: the loop has no beginning, so a tap just
// before the downbeat is close to it, not almost a whole bar away from it.
[[nodiscard]] double loopDistance( double p_position, double p_onset, double p_loopLength ) noexcept
{
    const double direct = std::abs( p_position - p_onset );

    return std::min( direct, p_loopLength - direct );
}

}    // namespace

double distanceToNearestOnsetInBeats( const RhythmPattern & p_pattern, double p_positionInBeats ) noexcept
{
    const auto hits = p_pattern.hits();

    if( hits.empty() )
    {
        return 0.0;
    }

    const double loopLength = static_cast<double>( p_pattern.beatsPerBar() );

    const double position = foldIntoLoop( p_positionInBeats, loopLength );

    double bestDistance = loopLength;

    for( const RhythmHit & hit : hits )
    {
        bestDistance = std::min( bestDistance, loopDistance( position, hit.beat, loopLength ) );
    }

    return bestDistance;
}

std::size_t nearestOnsetIndex( const RhythmPattern & p_pattern, double p_positionInBeats ) noexcept
{
    const auto hits = p_pattern.hits();

    if( hits.empty() )
    {
        return 0;
    }

    const double loopLength = static_cast<double>( p_pattern.beatsPerBar() );

    const double position = foldIntoLoop( p_positionInBeats, loopLength );

    std::size_t bestIndex = 0;
    double bestDistance = loopLength;

    for( std::size_t index = 0; index < hits.size(); ++index )
    {
        const double distance = loopDistance( position, hits[index].beat, loopLength );

        // STRICTLY closer, so that a tie keeps the EARLIER onset: two onsets equidistant from a tap - a courtesy
        // that only an exactly symmetrical cell can produce - must not depend on the order of the comparison.
        if( distance < bestDistance )
        {
            bestDistance = distance;
            bestIndex = index;
        }
    }

    return bestIndex;
}

}    // namespace musichien::domain
