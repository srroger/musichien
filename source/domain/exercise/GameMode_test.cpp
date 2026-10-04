#include "domain/exercise/GameMode.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

namespace musichien::domain
{

namespace
{

// Combien de questions d'un genre donne, dans un plan.
[[nodiscard]] std::size_t countOfKind( const std::vector<QuestionTarget> & p_plan, QuestionKind p_kind )
{
    std::size_t count = 0;

    for( const QuestionTarget & target : p_plan )
    {
        if( target.kind == p_kind )
        {
            ++count;
        }
    }

    return count;
}

// Tous les genres de question, pour les tests qui doivent les parcourir.
constexpr std::size_t QUESTION_KIND_COUNT_FOR_TESTS = 9;

}    // namespace

// L'experience ne s'gagne QU'EN ARCADE, et c'est la regle centrale du decoupage : un joueur ne peut pas la cultiver en ne
// travaillant que ce qu'il sait deja faire.
TEST( GameModeTest, only_the_arcade_pays_experience )
{
    EXPECT_TRUE( grantsExperience( GameMode::Arcade ) );

    EXPECT_FALSE( grantsExperience( GameMode::Training ) );
    EXPECT_FALSE( grantsExperience( GameMode::Infinite ) );
    EXPECT_FALSE( grantsExperience( GameMode::Survival ) );
    EXPECT_FALSE( grantsExperience( GameMode::Review ) );
}

// Chaque genre de question appartient a EXACTEMENT une famille, et le commutateur les couvre tous.
TEST( GameModeTest, every_question_kind_has_one_family )
{
    // Les trois familles sont atteintes, et aucune n'est vide.
    bool sawInterval = false;
    bool sawChord = false;
    bool sawMode = false;

    for( std::size_t index = 0; index < QUESTION_KIND_COUNT_FOR_TESTS; ++index )
    {
        const auto kind = static_cast<QuestionKind>( index );

        switch( familyOf( kind ) )
        {
            case QuestionFamily::Interval:
                sawInterval = true;
                break;

            case QuestionFamily::Chord:
                sawChord = true;
                break;

            case QuestionFamily::Mode:
                sawMode = true;
                break;
        }
    }

    EXPECT_TRUE( sawInterval );
    EXPECT_TRUE( sawChord );
    EXPECT_TRUE( sawMode );

    // Et les cas qui comptent, nommes un par un : les trois questions d'intervalle sont des intervalles, l'accord est un
    // accord, les quatre questions d'harmonie sont des modes.
    EXPECT_EQ( QuestionFamily::Interval, familyOf( QuestionKind::NamedInterval ) );
    EXPECT_EQ( QuestionFamily::Interval, familyOf( QuestionKind::Direction ) );
    EXPECT_EQ( QuestionFamily::Interval, familyOf( QuestionKind::Sing ) );
    EXPECT_EQ( QuestionFamily::Chord, familyOf( QuestionKind::Chord ) );
    EXPECT_EQ( QuestionFamily::Mode, familyOf( QuestionKind::ModeColour ) );
    EXPECT_EQ( QuestionFamily::Mode, familyOf( QuestionKind::ModeName ) );
    EXPECT_EQ( QuestionFamily::Mode, familyOf( QuestionKind::ModeVamp ) );
    EXPECT_EQ( QuestionFamily::Mode, familyOf( QuestionKind::ForeignNote ) );
}

[[nodiscard]] std::size_t countOfFamily( const std::vector<QuestionTarget> & p_plan, QuestionFamily p_family )
{
    return static_cast<std::size_t>(
      std::count_if( p_plan.begin(), p_plan.end(), [p_family]( const QuestionTarget & p_target ) {
          return familyOf( p_target.kind ) == p_family;
      } ) );
}

// Le plan d'Arcade tient le DOSAGE qu'il promet : dix, huit, sept - et vingt-cinq en tout.
//
// ET LE DOSAGE SE COMPTE PAR FAMILLE, PAS PAR GENRE.
//
// Le test verifiait « dix fois NamedInterval » - il figeait le GENRE alors qu'il voulait compter les QUESTIONS, et c'est
// ce qui a laisse passer le bug que Roger a trouve : le plan inscrivait le genre en dur, le chant ne sortait plus, et le
// test disait « tout va bien » puisqu'il demandait exactement ce que le code faisait de travers.
TEST( GameModeTest, the_arcade_asks_the_dose_it_promises )
{
    const SessionSettings settings;

    for( std::uint32_t seed = 0; seed < 20; ++seed )
    {
        const std::vector<QuestionTarget> plan = arcadePlan( seed, settings );

        EXPECT_EQ( ARCADE_QUESTION_COUNT, plan.size() );

        // DIX QUESTIONS D'INTERVALLE - quelle que soit leur forme, et c'est tout ce que le plan promet.
        EXPECT_EQ( ARCADE_INTERVAL_QUESTION_COUNT, countOfFamily( plan, QuestionFamily::Interval ) );
        EXPECT_EQ( ARCADE_CHORD_QUESTION_COUNT, countOfKind( plan, QuestionKind::Chord ) );

        // Les sept questions de mode, decomposees : deux couleurs, deux vamps, deux noms, une note etrangere.
        EXPECT_EQ( 2U, countOfKind( plan, QuestionKind::ModeColour ) );
        EXPECT_EQ( 2U, countOfKind( plan, QuestionKind::ModeVamp ) );
        EXPECT_EQ( 2U, countOfKind( plan, QuestionKind::ModeName ) );
        EXPECT_EQ( 1U, countOfKind( plan, QuestionKind::ForeignNote ) );
    }
}

// LE CHANT REVIENT DANS L'ARCADE QUAND LE JOUEUR LE DEMANDE.
//
// Roger, en jouant : « je ne tombe plus sur le jeu du chant ». Le plan inscrivait QuestionKind::NamedInterval EN DUR pour
// ses dix questions d'intervalle, donc la part de chant ne pouvait rien y changer - et rien ne le signalait.
TEST( GameModeTest, the_arcade_sings_when_the_player_asks_for_singing )
{
    SessionSettings settings;
    settings.namedIntervalQuestionShare = 0;
    settings.singQuestionShare = 100;
    settings.directionQuestionShare = 0;

    const std::vector<QuestionTarget> plan = arcadePlan( 20261003, settings );

    // TOUTES les questions d'intervalle sont chantees, et le dosage est intact.
    EXPECT_EQ( ARCADE_INTERVAL_QUESTION_COUNT, countOfKind( plan, QuestionKind::Sing ) );
    EXPECT_EQ( ARCADE_INTERVAL_QUESTION_COUNT, countOfFamily( plan, QuestionFamily::Interval ) );

    // ET LE CHANT NE DEBORDE PAS DE SA FAMILLE : aucun accord, aucun mode n'est devenu chantant.
    EXPECT_EQ( ARCADE_CHORD_QUESTION_COUNT, countOfKind( plan, QuestionKind::Chord ) );
    EXPECT_EQ( 1U, countOfKind( plan, QuestionKind::ForeignNote ) );
}

// LE BOSS : la note etrangere ferme TOUJOURS la marche, et elle n'apparait qu'une fois.
TEST( GameModeTest, the_foreign_note_is_always_the_last_question )
{
    for( std::uint32_t seed = 0; seed < 20; ++seed )
    {
        const std::vector<QuestionTarget> plan = arcadePlan( seed, SessionSettings{} );

        ASSERT_FALSE( plan.empty() );
        EXPECT_EQ( QuestionKind::ForeignNote, plan.back().kind );
        EXPECT_EQ( 1U, countOfKind( plan, QuestionKind::ForeignNote ) );
    }
}

// Le plan ne decide que le GENRE : les intervalles et les accords portent DRAWN_TARGET, donc la cible reste au tirage.
TEST( GameModeTest, the_arcade_plan_only_decides_the_kind )
{
    const std::vector<QuestionTarget> plan = arcadePlan( 7, SessionSettings{} );

    for( const QuestionTarget & target : plan )
    {
        EXPECT_EQ( DRAWN_TARGET, target.target );
    }
}

// Les reglages d'une Arcade : vingt-cinq questions, dix coeurs, et le plan.
TEST( GameModeTest, the_arcade_settings_carry_the_dose_and_the_hearts )
{
    const SessionSettings settings = arcadeSettingsFor( PlayerLevel::Fluent, 3, 10 );

    EXPECT_EQ( ARCADE_QUESTION_COUNT, settings.questionCount );
    ASSERT_TRUE( settings.lives.has_value() );
    EXPECT_EQ( ARCADE_STARTING_LIVES, *settings.lives );
    EXPECT_EQ( ARCADE_QUESTION_COUNT, settings.plannedQuestions.size() );
}

// L'Entrainement ouvre UNE famille et ferme les deux autres - et c'est toute la difference avec une partie ordinaire.
TEST( GameModeTest, the_training_opens_one_family_and_closes_the_rest )
{
    struct FamilyCase
    {
        QuestionFamily family;
        std::vector<QuestionKind> opened;
        std::vector<QuestionKind> closed;
    };

    const std::vector<FamilyCase> cases{
      { QuestionFamily::Interval,
        { QuestionKind::NamedInterval, QuestionKind::Sing },
        { QuestionKind::Chord, QuestionKind::ModeColour, QuestionKind::ModeName, QuestionKind::ModeVamp, QuestionKind::ForeignNote } },
      { QuestionFamily::Chord,
        { QuestionKind::Chord },
        { QuestionKind::NamedInterval, QuestionKind::Sing, QuestionKind::ModeColour, QuestionKind::ModeName, QuestionKind::ModeVamp, QuestionKind::ForeignNote } },
      { QuestionFamily::Mode,
        { QuestionKind::ModeColour, QuestionKind::ModeName, QuestionKind::ModeVamp, QuestionKind::ForeignNote },
        { QuestionKind::NamedInterval, QuestionKind::Sing, QuestionKind::Chord } },
    };

    for( const FamilyCase & testCase : cases )
    {
        // Les reglages de depart ont TOUT ouvert (les quatre parts d'harmonie a dix) : si l'Entrainement ne fermait rien,
        // le test le verrait tout de suite.
        const SessionSettings settings = trainingSettingsFor( PlayerLevel::Fluent, testCase.family );

        EXPECT_EQ( 10U, settings.questionCount );

        for( const QuestionKind kind : testCase.opened )
        {
            EXPECT_TRUE( isKindOpen( settings, kind ) ) << "genre " << static_cast<int>( kind ) << " devrait etre ouvert";
        }

        for( const QuestionKind kind : testCase.closed )
        {
            EXPECT_FALSE( isKindOpen( settings, kind ) ) << "genre " << static_cast<int>( kind ) << " devrait etre ferme";
        }
    }
}

// Le multiplicateur : la partie parfaite double, la belle partie majore, la partie apprise ne majore pas.
TEST( GameModeTest, the_multiplier_rewards_the_untouched_run )
{
    EXPECT_DOUBLE_EQ( 2.0, arcadeMultiplier( 0 ) );

    EXPECT_DOUBLE_EQ( 1.2, arcadeMultiplier( 1 ) );
    EXPECT_DOUBLE_EQ( 1.2, arcadeMultiplier( 4 ) );

    EXPECT_DOUBLE_EQ( 1.0, arcadeMultiplier( 5 ) );
    EXPECT_DOUBLE_EQ( 1.0, arcadeMultiplier( 10 ) );
}

// L'experience finale : le score, le merite des coeurs, ET la part de la partie qui a ete jouee.
TEST( GameModeTest, the_arcade_experience_is_the_score_multiplied )
{
    // Une partie COMPLETE - vingt-cinq sur vingt-cinq - ne subit que le multiplicateur.
    EXPECT_EQ( 200, arcadeExperience( 100, 0, 25, 25 ) );
    EXPECT_EQ( 120, arcadeExperience( 100, 3, 25, 25 ) );
    EXPECT_EQ( 100, arcadeExperience( 100, 7, 25, 25 ) );

    // Et l'arrondi : 125 x 1.2 = 150, et 133 x 1.2 = 159.6 -> 160.
    EXPECT_EQ( 150, arcadeExperience( 125, 2, 25, 25 ) );
    EXPECT_EQ( 160, arcadeExperience( 133, 2, 25, 25 ) );
}

// LA PART JOUEE : une Arcade perdue tot ne paie presque rien. C'est la seconde moitie de la correction du 02/10/2026.
TEST( GameModeTest, an_unfinished_arcade_barely_pays )
{
    // Perdue au huitieme des vingt-cinq, tous les coeurs y sont passes : huit vingt-cinquiemes de cent.
    EXPECT_EQ( 32, arcadeExperience( 100, 10, 8, 25 ) );

    // Et meme SANS avoir perdu un coeur, une partie arretee tot ne paie que sa part.
    EXPECT_EQ( 80, arcadeExperience( 100, 0, 10, 25 ) );

    // La meme partie TERMINEE vaudrait le double. C'est tout l'ecart que Roger voulait : « reduire drastiquement
    // l'experience gagnee quand on perd avant la fin ».
    EXPECT_EQ( 200, arcadeExperience( 100, 0, 25, 25 ) );
}

// LE PLAFOND : l'Arcade et l'Entrainement montent vers le NIVEAU SUIVANT, et s'y arretent. Le jeu libre, lui, ne plafonne
// rien.
TEST( GameModeTest, the_arcade_and_the_training_cap_the_palette_at_the_next_level )
{
    const SessionSettings beginnerArcade = arcadeSettingsFor( PlayerLevel::Beginner, 1, 10 );
    const SessionSettings fluentLevel = sessionSettingsFor( PlayerLevel::Fluent );

    // Un DEBUTANT part de deux intervalles, et peut monter jusqu'aux cinq du niveau au-dessus.
    EXPECT_EQ( 2U, beginnerArcade.startingPaletteSize );
    EXPECT_EQ( fluentLevel.startingPaletteSize, beginnerArcade.maximumPaletteSize );
    EXPECT_GT( beginnerArcade.maximumPaletteSize, beginnerArcade.startingPaletteSize );
    EXPECT_EQ( fluentLevel.startingModeCount, beginnerArcade.maximumModeCount );

    // Le DERNIER palier n'a pas de suivant : sa partie ne plafonne rien.
    const SessionSettings masterArcade = arcadeSettingsFor( PlayerLevel::Master, 1, 10 );
    EXPECT_EQ( 0U, masterArcade.maximumPaletteSize );
    EXPECT_EQ( 0U, masterArcade.maximumModeCount );

    // Et le JEU LIBRE ne plafonne RIEN non plus : ses reglages ne portent aucun maximum.
    const SessionSettings free = sessionSettingsFor( PlayerLevel::Beginner );
    EXPECT_EQ( 0U, free.maximumPaletteSize );
    EXPECT_EQ( 0U, free.maximumChordQualityCount );
    EXPECT_EQ( 0U, free.maximumModeCount );
}

// LE COMPTAGE PAR FAMILLE : ce que l'ecran de fin d'Arcade lit pour dire ou le joueur est fort.
TEST( GameModeTest, the_family_tally_counts_by_family )
{
    FamilyTally tally;

    tally.registerQuestion( QuestionFamily::Interval, true );
    tally.registerQuestion( QuestionFamily::Interval, true );
    tally.registerQuestion( QuestionFamily::Interval, false );
    tally.registerQuestion( QuestionFamily::Mode, true );

    EXPECT_EQ( 3U, tally.askedIn( QuestionFamily::Interval ) );
    EXPECT_EQ( 2U, tally.correctIn( QuestionFamily::Interval ) );
    EXPECT_EQ( 66U, tally.successPercentIn( QuestionFamily::Interval ) );

    EXPECT_EQ( 1U, tally.askedIn( QuestionFamily::Mode ) );
    EXPECT_EQ( 100U, tally.successPercentIn( QuestionFamily::Mode ) );

    // Une famille JAMAIS demandee n'a pas de taux : zero, et l'ecran sait qu'il doit l'exclure plutot qu'afficher « 0 % »,
    // qui serait un mensonge sur un absent.
    EXPECT_EQ( 0U, tally.askedIn( QuestionFamily::Chord ) );
    EXPECT_EQ( 0U, tally.correctIn( QuestionFamily::Chord ) );
    EXPECT_EQ( 0U, tally.successPercentIn( QuestionFamily::Chord ) );
}

}    // namespace musichien::domain
