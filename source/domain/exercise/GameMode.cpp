#include "domain/exercise/GameMode.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace musichien::domain
{

namespace
{

// Ferme une famille : ses genres recoivent une part NULLE, donc le tirage ne les choisit plus jamais.
//
// Ecrit ici, a un seul endroit, parce que la correspondance « famille -> parts » est exactement le genre de table qu'on
// oublie d'etendre quand un genre apparait. Le test qui parcourt les familles la surveille.
void closeFamily( SessionSettings & p_settings, QuestionFamily p_family ) noexcept
{
    switch( p_family )
    {
        case QuestionFamily::Interval:
            p_settings.namedIntervalQuestionShare = 0;
            p_settings.singQuestionShare = 0;
            p_settings.directionQuestionShare = 0;
            break;

        case QuestionFamily::Chord:
            p_settings.chordQuestionShare = 0;
            break;

        case QuestionFamily::Mode:
            p_settings.modeColourQuestionShare = 0;
            p_settings.modeNameQuestionShare = 0;
            p_settings.modeVampQuestionShare = 0;
            p_settings.foreignNoteQuestionShare = 0;
            break;
    }
}

// LE PLAFOND d'une session cadre e : la palette ne grandit plus au-dela de ce que le NIVEAU donne au depart.
//
// C'est ce qui rend a l'Arcade et a l'Entrainement la difficulte qu'on a choisie. Une partie de vingt-cinq questions
// elargit deux fois plus qu'une partie de dix, et Roger l'a senti tout de suite en debutant - « j'ai testé le mode arcade
// en débutant. Et je galère ». Le jeu libre, lui, n'appelle jamais cette fonction : il garde le droit d'aller loin.
void capPaletteAtLevel( SessionSettings & p_settings ) noexcept
{
    p_settings.maximumPaletteSize = p_settings.startingPaletteSize;
    p_settings.maximumChordQualityCount = p_settings.startingChordQualityCount;
    p_settings.maximumModeCount = p_settings.startingModeCount;
}

}    // namespace

double arcadeMultiplier( std::int32_t p_livesLost ) noexcept
{
    if( p_livesLost <= 0 )
    {
        return 2.0;
    }

    if( p_livesLost < 5 )
    {
        return 1.2;
    }

    return 1.0;
}

std::int32_t arcadeExperience( std::int32_t p_baseExperience, std::int32_t p_livesLost ) noexcept
{
    const double multiplied = static_cast<double>( p_baseExperience ) * arcadeMultiplier( p_livesLost );

    return static_cast<std::int32_t>( std::lround( multiplied ) );
}

std::vector<QuestionTarget> arcadePlan( std::uint32_t p_seed )
{
    std::vector<QuestionTarget> plan;
    plan.reserve( ARCADE_QUESTION_COUNT );

    // Les INTERVALLES ouvrent la partie : c'est le geste le plus ancien et le plus sur du jeu, et commencer par lui met
    // le joueur en confiance avant les couleurs. Le plan dit COMBIEN, le tirage dit LESQUELS.
    for( std::size_t index = 0; index < ARCADE_INTERVAL_QUESTION_COUNT; ++index )
    {
        plan.push_back( QuestionTarget{ QuestionKind::NamedInterval, DRAWN_TARGET, IntervalDirection::Ascending } );
    }

    // Les ACCORDS ensuite : plusieurs notes a la fois, apres une seule.
    for( std::size_t index = 0; index < ARCADE_CHORD_QUESTION_COUNT; ++index )
    {
        plan.push_back( QuestionTarget{ QuestionKind::Chord, DRAWN_TARGET, IntervalDirection::Ascending } );
    }

    // Les six questions d'harmonie « normales », deux par genre, MELANGEES : les jouer par blocs ferait trois series de
    // deux, et la variete est ce qui empeche de repondre par habitude plutot que par oreille.
    std::vector<QuestionTarget> modeQuestions{
      QuestionTarget{ QuestionKind::ModeColour, DRAWN_TARGET, IntervalDirection::Ascending },
      QuestionTarget{ QuestionKind::ModeColour, DRAWN_TARGET, IntervalDirection::Ascending },
      QuestionTarget{ QuestionKind::ModeVamp, DRAWN_TARGET, IntervalDirection::Ascending },
      QuestionTarget{ QuestionKind::ModeVamp, DRAWN_TARGET, IntervalDirection::Ascending },
      QuestionTarget{ QuestionKind::ModeName, DRAWN_TARGET, IntervalDirection::Ascending },
      QuestionTarget{ QuestionKind::ModeName, DRAWN_TARGET, IntervalDirection::Ascending },
    };

    std::mt19937 engine{ p_seed };
    std::shuffle( modeQuestions.begin(), modeQuestions.end(), engine );

    plan.insert( plan.end(), modeQuestions.begin(), modeQuestions.end() );

    // Et LE BOSS, en dernier : la note etrangere. Une partie d'Arcade ne se termine jamais sur autre chose. C'est le
    // seul endroit du jeu ou cette question est IMPOSEE, et c'est ce qui en fait un final plutot qu'un tirage.
    plan.push_back( QuestionTarget{ QuestionKind::ForeignNote, DRAWN_TARGET, IntervalDirection::Ascending } );

    return plan;
}

SessionSettings arcadeSettingsFor( PlayerLevel p_level, std::uint32_t p_seed )
{
    SessionSettings settings = sessionSettingsFor( p_level );

    settings.questionCount = ARCADE_QUESTION_COUNT;
    settings.lives = ARCADE_STARTING_LIVES;
    settings.plannedQuestions = arcadePlan( p_seed );

    // LE PLAFOND : une Arcade fait la difficulte de son niveau, pas plus. Voir capPaletteAtLevel.
    capPaletteAtLevel( settings );

    return settings;
}

SessionSettings trainingSettingsFor( PlayerLevel p_level, QuestionFamily p_family )
{
    SessionSettings settings = sessionSettingsFor( p_level );

    settings.questionCount = 10;

    // Tout ce qui n'est pas la famille demandee est FERME. La famille ouverte, elle, garde ses sous-parts telles que le
    // joueur les a reglees : c'est ce qui lui permet de dire « des intervalles, mais sans chanter ».
    for( std::size_t index = 0; index < QUESTION_FAMILY_COUNT; ++index )
    {
        const auto family = static_cast<QuestionFamily>( index );

        if( family != p_family )
        {
            closeFamily( settings, family );
        }
    }

    // ET LE PLAFOND, comme l'Arcade : un Entrainement ne depasse pas la difficulte de son niveau.
    capPaletteAtLevel( settings );

    return settings;
}

}    // namespace musichien::domain
