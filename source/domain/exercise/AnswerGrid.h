#pragma once

#include <array>
#include <optional>

// =====================================================================================================================
// Musichien - AnswerGrid
//
// Builds the choices offered for a question: the right answer, plus the WRONG answers.
//
// The quality of an exercise is the quality of its wrong answers, and that is the whole reason this
// class exists. A grid of twenty-five choices among which twenty are impossible is EASIER than a grid
// of three well chosen ones, because the player only has to eliminate, never to hear. So the choices
// are always drawn from the intervals CLOSEST to the target, never from the whole list.
//
// ---------------------------------------------------------------------------------------------------------------------
// What "closest" means, and why it is not just a distance in semitones
//
// Two intervals are confusable for two independent reasons, and both have to count:
//
//   * their SIZES are close: a major third and a minor third are one semitone apart, and that is the
//     mistake everybody makes first;
//
//   * their CLASSES are close: a minor third and a minor tenth are twelve semitones apart, and they
//     are still the same mistake, because they share the colour inside the octave. This is the
//     octave confusion a beginner has to unlearn, and an exercise that never offers it never teaches
//     anything about it.
//
// The plausibility of a wrong answer is therefore the SMALLER of the two distances, never the larger.
//
// ---------------------------------------------------------------------------------------------------------------------
// Determinism
//
// The random engine is passed IN, never created here. The domain holds no entropy source of its own,
// which is what keeps this class pure and its tests reproducible: the same seed draws the same grid,
// every time, on every machine.
// =====================================================================================================================

#include "domain/music/Interval.h"

#include <cstddef>
#include <random>
#include <span>
#include <vector>

namespace musichien::domain
{

class AnswerGrid
{
public:
    // The smallest grid that is still a question. With a single choice there would be nothing to
    // decide, so two is the floor.
    static constexpr std::size_t MINIMUM_CHOICE_COUNT = 2;

    // How many of the closest intervals the wrong answers are drawn from.
    //
    // Deliberately larger than any grid the application offers: the draw must be able to vary without
    // ever leaving the plausible neighbourhood.
    static constexpr std::size_t DISTRACTOR_POOL_SIZE = 5;

    // Builds the choices for a target, drawn from a PALETTE: the intervals currently in play.
    //
    // The palette is a parameter rather than the full list of intervals, and that is a pedagogical
    // decision, not a technical one. Offering a compound interval to a player who is still learning
    // the simple ones would be asking a question they have never been taught; the palette is what
    // decides which intervals are fair game, and it grows with the experience of the player.
    //
    // The target must belong to the palette. The count is clamped to what the palette can offer, so
    // the grid can never propose the same interval twice, and never a choice outside the palette.
    [[nodiscard]] static std::vector<Interval> build( std::span<const Interval> p_palette,
                                                      const Interval & p_target,
                                                      std::size_t p_choiceCount,
                                                      std::mt19937 & p_randomEngine );

    // How confusable two intervals are: 0 means "the same colour heard another way", and the value
    // grows as they become easier to tell apart. Exposed for the tests, and because it is the rule
    // itself rather than a step of its implementation.
    [[nodiscard]] static std::int32_t plausibilityDistance( const Interval & p_target,
                                                            const Interval & p_other );
};

// Nombre de places du cercle des quintes : une par classe d'intervalle simple.
inline constexpr std::size_t CIRCLE_OF_FIFTHS_SLOT_COUNT = 12;

// La place d'un intervalle sur le cercle : sept demi-tons par quinte, modulo douze.
//
// Sept et douze sont premiers entre eux, donc chaque classe a une place et une seule : la table n'a pas besoin
// d'etre ecrite, et elle ne peut pas se desynchroniser du cercle reel.
[[nodiscard]] constexpr std::size_t circleOfFifthsSlot( const Interval & p_interval ) noexcept
{
    constexpr std::size_t SEMITONES_IN_A_FIFTH = 7;

    return ( SEMITONES_IN_A_FIFTH * static_cast<std::size_t>( p_interval.intervalClass() ) ) % CIRCLE_OF_FIFTHS_SLOT_COUNT;
}

// Les choix d'une question, PLACES sur le cercle des quintes plutot que ranges les uns derriere les autres.
//
// La difference est tout : une liste rangee met le troisieme choix en troisieme position, donc un intervalle
// donne change de place des que la selection change autour de lui. Une position ABSOLUE fait que la quinte est
// toujours la ou est la quinte - et c'est la seule facon d'apprendre la geographie des intervalles.
//
// UN BOUTON QUI DISPARAIT NE DEPLACE RIEN, et c'est le point delicat. Quand la grille se resserre - un leurre
// retire a chaque erreur - il y a simplement MOINS d'intervalles a placer : les cases liberees redeviennent
// vides, et aucune autre ne bouge. C'est ce qu'on demande a une carte : elle peut se vider, elle ne se
// reorganise pas.
//
// Une case vide est donc une INFORMATION : elle dit qu'il y a une place pour cet intervalle, et que la palette
// ne l'a pas encore.
[[nodiscard]] std::array<std::optional<Interval>, CIRCLE_OF_FIFTHS_SLOT_COUNT> layoutOnCircle(
  std::span<const Interval> p_choices );

}    // namespace musichien::domain
