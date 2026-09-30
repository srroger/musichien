#include "domain/music/Phrase.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <utility>
#include <vector>

namespace musichien::domain
{

namespace
{

// Le degre qui COLORE un mode : celui qui le distingue de ses voisins dans l'ordre de couleur.
//
// Zero quand le mode n'a rien a demontrer : l'ionien et l'eolien sont les deux references, et c'est justement pour cela
// qu'elles servent de reference. Le generateur n'insiste alors sur aucun degre et laisse la marche choisir.
[[nodiscard]] constexpr std::int32_t characteristicDegree( Mode p_mode ) noexcept
{
    switch( p_mode )
    {
        case Mode::Lydian:
            return 4;

        case Mode::Mixolydian:
            return 7;

        case Mode::Dorian:
            return 6;

        case Mode::Phrygian:
            return 2;

        case Mode::Locrian:
            return 5;

        case Mode::Ionian:
        case Mode::Aeolian:
            return 0;
    }

    return 0;
}

// Ramene un degre dans les sept : une marche qui sort du haut revient par le bas, et l'inverse.
//
// Le modulo doit etre rendu POSITIF avant d'etre utilise : en C++, -1 % 7 vaut -1, et un degre negatif indexerait un
// tableau a l'envers.
[[nodiscard]] constexpr std::int32_t wrapDegree( std::int32_t p_degree ) noexcept
{
    return ( ( ( p_degree - 1 ) % 7 ) + 7 ) % 7 + 1;
}

}    // namespace

std::vector<Note> Phrase::notes( Note p_tonic ) const
{
    // Les sept degres du mode, resolus UNE fois : la phrase ne connait que des degres, et c'est le mode qui les traduit.
    const std::vector<Note> scale = notesOfMode( p_tonic, mode );

    std::vector<Note> result;
    result.reserve( steps.size() );

    for( const PhraseStep & step : steps )
    {
        const auto index = static_cast<std::size_t>( wrapDegree( step.degree ) - 1 );

        result.push_back( scale.at( index ) );
    }

    return result;
}

std::vector<std::chrono::milliseconds> Phrase::stepDurations() const
{
    // Un temps vaut une noire, et c'est le tempo qui dit combien de millisecondes dure ce temps. Le plancher a un n'est
    // pas une precaution de style : un bpm nul - un contenu edite a la main peut en porter un - produirait une division
    // par zero, et un pas de zero temps une note d'une longueur nulle. La phrase reste alors audible, simplement lente,
    // ce qui vaut mieux qu'un silence qu'aucun ecran ne saurait expliquer.
    const std::int32_t safeBpm = std::max( std::int32_t{ 1 }, bpm );

    const double millisecondsPerBeat = 60000.0 / static_cast<double>( safeBpm );

    std::vector<std::chrono::milliseconds> durations;
    durations.reserve( steps.size() );

    for( const PhraseStep & step : steps )
    {
        const auto beats = static_cast<double>( std::max( std::int32_t{ 1 }, step.beats ) );

        // L'arrondi est fait ICI, une fois pour toutes : deux appelants qui arrondiraient chacun de leur cote feraient
        // sonner differemment la meme phrase.
        durations.push_back(
          std::chrono::milliseconds{ static_cast<std::int64_t>( std::llround( millisecondsPerBeat * beats ) ) } );
    }

    return durations;
}

Phrase generatePhrase( Mode p_mode, Note p_tonic, std::mt19937 & p_randomEngine, PhraseSettings p_settings )
{
    Phrase phrase;
    phrase.mode = p_mode;
    phrase.tonic = p_tonic;

    // Trois pas au minimum : un depart, un chemin, un retour. En dessous, il n'y a plus de phrase.
    const auto stepCount = std::max( std::int32_t{ 3 }, static_cast<std::int32_t>( p_settings.stepCount ) );

    const std::int32_t characteristic = characteristicDegree( p_mode );

    std::vector<PhraseStep> steps;
    steps.reserve( static_cast<std::size_t>( stepCount ) );

    // 1. ON PART DE LA TONIQUE : le centre est pose avant tout le reste, comme le bourdon l'est sous la phrase.
    steps.push_back( PhraseStep{ .degree = 1, .beats = 1 } );

    // Et la note CARACTERISTIQUE entre tot, et LONGUEMENT.
    //
    // Une couleur posee sur une croche s'entend comme une broderie ; une couleur qui arrive au dernier pas s'entend
    // comme une conclusion. Pour que le mode soit la couleur de la phrase, sa note doit etre la, tot, et durer.
    if( ( characteristic != 0 ) && ( stepCount > 3 ) )
    {
        steps.push_back( PhraseStep{ .degree = characteristic, .beats = p_settings.characteristicBeats } );
    }

    std::uniform_int_distribution<std::int32_t> stepwiseDraw{ 0, 99 };
    std::uniform_int_distribution<std::int32_t> directionDraw{ 0, 1 };
    std::uniform_int_distribution<std::int32_t> durationDraw{ 1, 2 };
    std::uniform_int_distribution<std::int32_t> leapDraw{ 1, 7 };

    // 2. LA MARCHE, par degres conjoints la plupart du temps : une phrase se chante, et ce qui saute ne se chante pas.
    while( static_cast<std::int32_t>( steps.size() ) < ( stepCount - 1 ) )
    {
        const std::int32_t previousDegree = steps.back().degree;

        std::int32_t degree = previousDegree;

        if( stepwiseDraw( p_randomEngine ) < p_settings.stepwiseShare )
        {
            degree = previousDegree + ( ( directionDraw( p_randomEngine ) == 0 ) ? -1 : 1 );
        }
        else
        {
            degree = leapDraw( p_randomEngine );
        }

        steps.push_back( PhraseStep{ .degree = wrapDegree( degree ), .beats = durationDraw( p_randomEngine ) } );
    }

    // 3. ET ON REVIENT SUR LA TONIQUE, toujours.
    //
    // C'est le retour au centre qui fait entendre ou est le centre : une phrase qui s'arrete ailleurs laisse l'oreille en
    // l'air, et un mode dont on ne revient pas a sa tonique n'a pas de centre du tout.
    steps.push_back( PhraseStep{ .degree = 1, .beats = p_settings.characteristicBeats } );

    phrase.steps = std::move( steps );

    return phrase;
}

}    // namespace musichien::domain
