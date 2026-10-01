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

// LE PLAFOND d'une session cadre e : la palette part de la difficulte du NIVEAU CHOISI, peut grandir vers celle du niveau
// AU-DESSUS, et ne va pas plus loin. Le PLANCHER, lui, est deja celui du niveau - voir narrowPalette, qui redescend sur une
// erreur sans jamais passer sous ce avec quoi on a commence.
//
// C'EST LE RUBBER-BANDING DE ROGER : « je pensais garder la progression mais la plafonner au niveau supérieur non ? Et
// redescendre la difficulte quand le joueur se trompe. Un peu comme en arcade des jeux de combat, la difficulte du CPU
// augmente avec les combats, mais quand il perd elle rediminue. » Une partie qui reussit s'approche du niveau suivant sans
// l'atteindre ; une partie qui rate retombe sur ses appuis.
//
// POURQUOI CE PLAFOND EXISTE, ET PAS CELUI DU NIVEAU COURANT : le premier jet plafonnait a la palette du niveau de depart,
// ce qui SUPPRIMAIT toute progression dans la partie. Roger l'a corrige le jour meme - il veut la montee, bornee.
void capPaletteAtNextLevel( SessionSettings & p_settings, PlayerLevel p_level ) noexcept
{
    const auto levelIndex = static_cast<std::size_t>( p_level );

    if( levelIndex + 1 >= PLAYER_LEVEL_COUNT )
    {
        // Le DERNIER palier n'a pas de suivant : sa partie ne plafonne rien - et il n'y a de toute facon plus rien a
        // elargir, puisque « Je maitrise » ouvre tout des le depart.
        return;
    }

    const SessionSettings nextLevel = sessionSettingsFor( playerLevelFromIndex( levelIndex + 1 ) );

    p_settings.maximumPaletteSize = nextLevel.startingPaletteSize;
    p_settings.maximumChordQualityCount = nextLevel.startingChordQualityCount;
    p_settings.maximumModeCount = nextLevel.startingModeCount;
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

std::int32_t arcadeExperience( std::int32_t p_baseExperience,
                               std::int32_t p_livesLost,
                               std::size_t p_completedQuestions,
                               std::size_t p_totalQuestions ) noexcept
{
    const double multiplied = static_cast<double>( p_baseExperience ) * arcadeMultiplier( p_livesLost );

    // LA PART JOUEE : ce que la partie a couvert de sa longueur. Une partie terminee vaut 1 ; une partie perdue au huitieme
    // des vingt-cinq vaut 0,32. Voir l'en-tete pour WHY ce facteur existe.
    const double completion = ( p_totalQuestions == 0 )
                                ? 1.0
                                : std::min( 1.0, static_cast<double>( p_completedQuestions ) / static_cast<double>( p_totalQuestions ) );

    return static_cast<std::int32_t>( std::lround( multiplied * completion ) );
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

    // LE PLAFOND : une Arcade part de son niveau, monte vers le suivant, et s'y arrete. Voir capPaletteAtNextLevel.
    capPaletteAtNextLevel( settings, p_level );

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

    // ET LE PLAFOND, comme l'Arcade : un Entrainement part de son niveau et monte vers le suivant, sans le depasser.
    capPaletteAtNextLevel( settings, p_level );

    return settings;
}

}    // namespace musichien::domain
