#include "domain/exercise/AnswerGrid.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <ranges>

namespace musichien::domain
{

namespace
{

// Distance between two values, never negative. Written out rather than called std::abs: the intent is
// the distance, not the absolute value of a subtraction.
[[nodiscard]] constexpr std::int32_t distanceBetween( std::int32_t p_left, std::int32_t p_right ) noexcept
{
    return ( p_left >= p_right ) ? ( p_left - p_right ) : ( p_right - p_left );
}

// Position d'un intervalle dans le cercle des quintes.
//
// Une quinte fait SEPT demi-tons, donc l'index d'un intervalle dans le cercle s'obtient en multipliant sa classe
// par sept, modulo douze : do, sol, ré, la, mi, si, fa dièse... Sept et douze sont premiers entre eux, ce qui
// garantit que chaque classe a sa place et une seule - la table n'a donc pas besoin d'être écrite.
[[nodiscard]] constexpr std::int32_t circleOfFifthsPosition( const Interval & p_interval ) noexcept
{
    constexpr std::int32_t SEMITONES_IN_A_FIFTH = 7;
    constexpr std::int32_t SEMITONES_IN_AN_OCTAVE = 12;

    return ( SEMITONES_IN_A_FIFTH * p_interval.intervalClass() ) % SEMITONES_IN_AN_OCTAVE;
}

}    // namespace

std::int32_t AnswerGrid::plausibilityDistance( const Interval & p_target, const Interval & p_other )
{
    const std::int32_t sizeDistance =
      distanceBetween( p_target.semitones(), p_other.semitones() );

    const std::int32_t classDistance =
      distanceBetween( p_target.intervalClass(), p_other.intervalClass() );

    // The smaller of the two, never the larger: a wrong answer is plausible as soon as it is close in
    // one of the two senses. A minor tenth is a plausible wrong answer for a minor third even though
    // it is an octave away, because it is the same colour.
    return std::min( sizeDistance, classDistance );
}

std::vector<Interval> AnswerGrid::build( std::span<const Interval> p_palette,
                                         const Interval & p_target,
                                         std::size_t p_choiceCount,
                                         std::mt19937 & p_randomEngine )
{
    // Every interval of the palette, except the right answer: a grid never offers the same interval
    // twice, and never offers the answer as one of its own wrong answers.
    std::vector<Interval> candidates;

    std::ranges::copy_if( p_palette, std::back_inserter( candidates ), [&p_target]( const Interval & p_candidate ) { return !( p_candidate == p_target ); } );

    if( candidates.empty() )
    {
        // A palette that holds nothing but the target cannot produce a question. The session is
        // responsible for never asking for one; returning the single choice rather than asserting
        // keeps this function total, and lets a caller fail loudly somewhere it can be handled.
        return std::vector<Interval>{ p_target };
    }

    // Closest first. The order is made deterministic rather than left to the draw: two candidates at
    // the same plausibility must come out in the same order on every machine, otherwise the same seed
    // would produce two different grids.
    std::ranges::sort( candidates,
                       [&p_target]( const Interval & p_left, const Interval & p_right ) {
                           const std::int32_t leftDistance = plausibilityDistance( p_target, p_left );
                           const std::int32_t rightDistance = plausibilityDistance( p_target, p_right );

                           if( leftDistance != rightDistance )
                           {
                               return leftDistance < rightDistance;
                           }

                           return p_left.semitones() < p_right.semitones();
                       } );

    const std::size_t maximumChoiceCount = candidates.size() + 1;

    const std::size_t choiceCount = std::clamp( p_choiceCount, MINIMUM_CHOICE_COUNT, maximumChoiceCount );

    const std::size_t wantedDistractorCount = choiceCount - 1;

    // The draw happens INSIDE the plausible neighbourhood, never across the whole palette: that is
    // what makes the same wrong answer vary from one question to the next without ever becoming
    // absurd.
    candidates.resize( std::min( candidates.size(),
                                 std::max( DISTRACTOR_POOL_SIZE, wantedDistractorCount ) ) );

    // Shuffled so that a grid does not always offer the same wrong answers in the same order: the
    // player must not be able to recognise a question by the shape of the buttons.
    std::ranges::shuffle( candidates, p_randomEngine );

    std::vector<Interval> choices;
    choices.reserve( choiceCount );
    choices.push_back( p_target );

    std::ranges::copy( candidates | std::views::take( wantedDistractorCount ),
                       std::back_inserter( choices ) );

    // Et rangés SELON LE CERCLE DES QUINTES - ce qui est le contraire d'un mélange.
    //
    // Roger : "les intervalles placés au bon endroit du cercle". Il a mis le doigt sur quelque chose que la
    // grille ne faisait pas : elle était mélangée à chaque question, y compris l'emplacement du bon bouton, pour
    // que le joueur ne puisse pas reconnaître une question à la forme des boutons. C'était prudent, et c'était
    // une occasion perdue - un bouton qui change de place ne peut pas devenir un REPÈRE.
    //
    // Ici, la position porte une information : deux intervalles voisins dans la grille sont voisins en musique,
    // et le joueur l'apprend sans qu'on le lui dise jamais. C'est exactement le genre de savoir qui se passe de
    // mots.
    //
    // L'aléatoire reste là où il a du sens : dans QUELS leurres sont proposés, et lequel des plus plausibles est
    // tiré. Jamais dans l'endroit où ils s'affichent.
    std::ranges::sort( choices, []( const Interval & p_left, const Interval & p_right ) {
        const std::int32_t leftPosition = circleOfFifthsPosition( p_left );
        const std::int32_t rightPosition = circleOfFifthsPosition( p_right );

        if( leftPosition != rightPosition )
        {
            return leftPosition < rightPosition;
        }

        // Deux intervalles de la même classe - une tierce mineure et une dixième mineure - partagent leur place
        // dans le cercle : le plus petit des deux passe en premier, pour que la grille reste lisible.
        return p_left.semitones() < p_right.semitones();
    } );

    return choices;
}

std::array<std::vector<Interval>, CIRCLE_OF_FIFTHS_SLOT_COUNT> layoutOnCircle(
  std::span<const Interval> p_choices )
{
    std::array<std::vector<Interval>, CIRCLE_OF_FIFTHS_SLOT_COUNT> slots{};

    for( const Interval & choice : p_choices )
    {
        // Deux intervalles de la MEME CLASSE - une tierce majeure et une dixieme majeure - visent la meme place,
        // et c'est voulu : c'est la meme couleur, un octave plus haut. Tous les deux y sont poses, parce que le
        // joueur doit pouvoir DESIGNE l'un ou l'autre : c'est la seule facon de lui demander de les distinguer.
        std::vector<Interval> & intervals = slots.at( circleOfFifthsSlot( choice ) );

        intervals.push_back( choice );
    }

    // Du plus petit au plus grand, dans chaque case : le simple en tete, ses composes a sa suite. Un intervalle
    // donne garde ainsi la meme extremite de la case quelle que soit la question, ce qui donne a l'oeil un
    // repere qui ne bouge pas.
    for( std::vector<Interval> & intervals : slots )
    {
        std::ranges::sort( intervals, []( const Interval & p_left, const Interval & p_right ) {
            return p_left.semitones() < p_right.semitones();
        } );
    }

    return slots;
}

}    // namespace musichien::domain
