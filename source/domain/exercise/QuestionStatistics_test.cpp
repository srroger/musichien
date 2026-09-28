#include "domain/exercise/QuestionStatistics.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <vector>

namespace musichien::domain
{

// ---------------------------------------------------------------------------------------------------------------------
// Les statistiques, comme regles
//
// Rien n'est mesure ici : on donne des LIGNES au domaine, et on lit ce qu'il en dit. C'est le meme contrat que pour le
// rythme - le domaine ne mesure pas le temps, il recoit des dates - et c'est ce qui rend ces tests instantanes et
// parfaitement deterministes.
// ---------------------------------------------------------------------------------------------------------------------

namespace
{

[[nodiscard]] std::chrono::system_clock::time_point daysAgo( int p_days )
{
    return std::chrono::system_clock::now() - ( std::chrono::hours{ 24 } * p_days );
}

// Un filtre qui ne garde que les p_days derniers jours.
[[nodiscard]] StatisticsFilter filterForLastDays( int p_days )
{
    StatisticsFilter filter;
    filter.since = daysAgo( p_days );

    return filter;
}

[[nodiscard]] QuestionRecord recordOf( QuestionKind p_kind,
                                       std::int32_t p_target,
                                       QuestionOutcome p_outcome,
                                       int p_daysAgo = 1 )
{
    QuestionRecord record;

    record.askedAt = daysAgo( p_daysAgo );
    record.kind = p_kind;
    record.target = p_target;
    record.outcome = p_outcome;
    record.attemptCount = ( p_outcome == QuestionOutcome::CorrectFirstTry ) ? 1 : 2;

    return record;
}

}    // namespace

TEST( QuestionRecordTest, the_outcome_of_a_question_distinguishes_knowing_from_finding )
{
    // Du premier coup, c'est SAVOIR...
    EXPECT_EQ( QuestionOutcome::CorrectFirstTry, outcomeOf( true, false, 1 ) );

    // ...et apres s'y etre repris, c'est FINIR PAR TROUVER. Les deux sont des reussites, et les confondre ferait
    // disparaitre la seule information qui dit qu'un intervalle est acquis.
    EXPECT_EQ( QuestionOutcome::CorrectAfterRetries, outcomeOf( true, false, 3 ) );

    // Un echec reste un echec.
    EXPECT_EQ( QuestionOutcome::Failed, outcomeOf( false, false, 1 ) );

    // Et une reponse DONNEE par l'application n'est ni l'un ni l'autre, meme si le joueur avait fini par trouver : la
    // question n'a pas ete repondue, elle a ete revelee.
    EXPECT_EQ( QuestionOutcome::Revealed, outcomeOf( false, true, 1 ) );
    EXPECT_EQ( QuestionOutcome::Revealed, outcomeOf( true, true, 1 ) );
}

TEST( QuestionStatisticsTest, an_empty_log_answers_zero_instead_of_dividing_by_zero )
{
    const std::vector<QuestionRecord> nothing;

    const QuestionStatistics statistics = computeStatistics( nothing, filterForLastDays( 30 ) );

    EXPECT_EQ( 0U, statistics.questionCount );
    EXPECT_EQ( 0, statistics.successPercent() );
    EXPECT_EQ( 0, statistics.firstTryPercent() );
    EXPECT_EQ( 0.0, statistics.averageReplays() );

    // Et le tri par cible d'un journal vide rend une liste vide, pas une entree fantome.
    EXPECT_TRUE( statisticsByTarget( nothing, filterForLastDays( 30 ) ).empty() );
}

TEST( QuestionStatisticsTest, knowing_and_finding_are_counted_apart )
{
    const std::vector<QuestionRecord> records{
      recordOf( QuestionKind::NamedInterval, 7, QuestionOutcome::CorrectFirstTry ),
      recordOf( QuestionKind::NamedInterval, 5, QuestionOutcome::CorrectAfterRetries ),
      recordOf( QuestionKind::NamedInterval, 3, QuestionOutcome::Failed ),
      recordOf( QuestionKind::NamedInterval, 4, QuestionOutcome::Revealed ),
    };

    const QuestionStatistics statistics = computeStatistics( records, filterForLastDays( 30 ) );

    EXPECT_EQ( 4U, statistics.questionCount );

    // Deux reussites sur quatre questions posees : 50 %.
    EXPECT_EQ( 2U, statistics.correctCount() );
    EXPECT_EQ( 50, statistics.successPercent() );

    // Et une seule du premier coup : 25 %. C'est le chiffre qui dit si le joueur SAIT, et il est bien plus severe que
    // le precedent - c'est voulu.
    EXPECT_EQ( 1U, statistics.correctFirstTryCount );
    EXPECT_EQ( 25, statistics.firstTryPercent() );

    EXPECT_EQ( 1U, statistics.failedCount );
    EXPECT_EQ( 1U, statistics.revealedCount );
}

TEST( QuestionStatisticsTest, the_filter_keeps_only_what_was_asked_of_it )
{
    const std::vector<QuestionRecord> records{
      recordOf( QuestionKind::NamedInterval, 7, QuestionOutcome::CorrectFirstTry, 1 ),
      recordOf( QuestionKind::Chord, 0, QuestionOutcome::CorrectFirstTry, 1 ),
      recordOf( QuestionKind::NamedInterval, 5, QuestionOutcome::CorrectFirstTry, 40 ),
    };

    // Sept jours : la question d'il y a quarante jours sort du filtre, les deux autres restent.
    EXPECT_EQ( 2U, computeStatistics( records, filterForLastDays( 7 ) ).questionCount );

    // Trente jours : la plus ancienne reste sortie, et c'est bien le meme resultat - c'est le POINT dans le temps qui
    // filtre, pas le nombre de jours qu'un ecran affiche.
    EXPECT_EQ( 2U, computeStatistics( records, filterForLastDays( 30 ) ).questionCount );

    // Tout : les trois.
    EXPECT_EQ( 3U, computeStatistics( records, filterForLastDays( 3650 ) ).questionCount );

    // Et un genre demande ne laisse passer que lui : les accords ici, donc une seule question.
    StatisticsFilter chordsOnly = filterForLastDays( 3650 );
    chordsOnly.kind = QuestionKind::Chord;

    EXPECT_EQ( 1U, computeStatistics( records, chordsOnly ).questionCount );
}

TEST( QuestionStatisticsTest, a_target_is_its_direction_too )
{
    // Une sixte montante et une sixte descendante sont deux exercices, et les melanger effacerait exactement ce que
    // ces statistiques doivent montrer.
    QuestionRecord ascending = recordOf( QuestionKind::NamedInterval, 9, QuestionOutcome::Failed );
    ascending.direction = IntervalDirection::Ascending;

    QuestionRecord descending = recordOf( QuestionKind::NamedInterval, 9, QuestionOutcome::CorrectFirstTry );
    descending.direction = IntervalDirection::Descending;

    const std::vector<QuestionRecord> records{ ascending, descending };

    const std::vector<TargetStatistics> byTarget = statisticsByTarget( records, filterForLastDays( 30 ) );

    ASSERT_EQ( 2U, byTarget.size() );

    // La ratee d'abord : le tri met les points faibles en tete.
    EXPECT_EQ( IntervalDirection::Ascending, byTarget.at( 0 ).direction );
    EXPECT_EQ( 0, byTarget.at( 0 ).statistics.successPercent() );

    EXPECT_EQ( IntervalDirection::Descending, byTarget.at( 1 ).direction );
    EXPECT_EQ( 100, byTarget.at( 1 ).statistics.successPercent() );
}

TEST( QuestionStatisticsTest, the_weakest_target_comes_first )
{
    const std::vector<QuestionRecord> records{
      // Une cible sue, jouee quatre fois.
      recordOf( QuestionKind::NamedInterval, 12, QuestionOutcome::CorrectFirstTry ),
      recordOf( QuestionKind::NamedInterval, 12, QuestionOutcome::CorrectFirstTry ),
      recordOf( QuestionKind::NamedInterval, 12, QuestionOutcome::CorrectFirstTry ),
      recordOf( QuestionKind::NamedInterval, 12, QuestionOutcome::CorrectFirstTry ),

      // Une cible ratee, jouee trois fois.
      recordOf( QuestionKind::NamedInterval, 2, QuestionOutcome::Failed ),
      recordOf( QuestionKind::NamedInterval, 2, QuestionOutcome::Failed ),
      recordOf( QuestionKind::NamedInterval, 2, QuestionOutcome::Failed ),
    };

    const std::vector<TargetStatistics> byTarget = statisticsByTarget( records, filterForLastDays( 30 ) );

    ASSERT_EQ( 2U, byTarget.size() );

    // La cible ratee passe devant, quel que soit l'ordre d'insertion : c'est tout l'interet de l'ordre.
    EXPECT_EQ( 2, byTarget.at( 0 ).target );
    EXPECT_EQ( 0, byTarget.at( 0 ).statistics.successPercent() );

    EXPECT_EQ( 12, byTarget.at( 1 ).target );
    EXPECT_EQ( 100, byTarget.at( 1 ).statistics.successPercent() );
}

TEST( QuestionStatisticsTest, the_average_of_replays_is_not_rounded_to_an_integer )
{
    std::vector<QuestionRecord> records;

    QuestionRecord once = recordOf( QuestionKind::NamedInterval, 7, QuestionOutcome::CorrectFirstTry );
    once.replayCount = 1;

    records.push_back( once );

    QuestionRecord twice = recordOf( QuestionKind::NamedInterval, 7, QuestionOutcome::CorrectFirstTry );
    twice.replayCount = 2;

    records.push_back( twice );

    const QuestionStatistics statistics = computeStatistics( records, filterForLastDays( 30 ) );

    EXPECT_EQ( 3, statistics.totalReplayCount );

    // 1,5 et non 1 : une moyenne entiere perdrait tout l'interet du chiffre, et c'est justement la difference entre
    // 0,4 et 1,3 reecoutes qui dit si une question est devenue facile.
    EXPECT_DOUBLE_EQ( 1.5, statistics.averageReplays() );
}

TEST( QuestionStatisticsTest, the_play_time_adds_up_the_sessions_and_not_the_pauses )
{
    const auto start = daysAgo( 1 );

    const auto recordAtSecond = [&start]( int p_seconds ) {
        QuestionRecord record;

        record.askedAt = start + std::chrono::seconds{ p_seconds };

        return record;
    };

    // Une premiere session : trois questions en deux minutes.
    const std::vector<QuestionRecord> records{
      recordAtSecond( 0 ),
      recordAtSecond( 60 ),
      recordAtSecond( 120 ),
      // ...une heure de pause, puis une question seule : c'est une AUTRE session.
      recordAtSecond( 3720 ),
    };

    // La premiere session vaut deux minutes plus le temps de sa derniere question (140 s), et la seconde une seule
    // question (20 s).
    EXPECT_EQ( std::chrono::seconds{ 160 }, playTimeOf( records ) );

    // L'ORDRE des lignes ne compte pas : un journal lu a l'envers ne doit pas produire une duree negative.
    const std::vector<QuestionRecord> reversed{ records.at( 3 ), records.at( 2 ), records.at( 1 ), records.at( 0 ) };

    EXPECT_EQ( std::chrono::seconds{ 160 }, playTimeOf( reversed ) );

    // Et un journal vide ne dure pas longtemps.
    EXPECT_EQ( std::chrono::seconds{ 0 }, playTimeOf( std::span<const QuestionRecord>{} ) );
}

}    // namespace musichien::domain
