#include "ui/StatisticsController.h"

#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <gtest/gtest.h>

#include <chrono>

namespace musichien::ui
{

// ---------------------------------------------------------------------------------------------------------------------
// La page de statistiques, vue depuis QML
//
// Ce fichier verifie ce que la PAGE recoit : les chiffres, les quatorze barres, les parts du camembert et les points
// faibles. L'agregation elle-meme est testee dans le domaine ; ici, c'est le DECOUPAGE qui est en jeu - les jours, les
// genres, et ce qui doit rester invisible.
// ---------------------------------------------------------------------------------------------------------------------

namespace
{

[[nodiscard]] domain::QuestionRecord recordOf( std::chrono::system_clock::time_point p_askedAt,
                                               domain::QuestionKind p_kind,
                                               bool p_correct )
{
    domain::QuestionRecord record;

    record.askedAt = p_askedAt;
    record.kind = p_kind;
    record.outcome = p_correct ? domain::QuestionOutcome::CorrectFirstTry : domain::QuestionOutcome::Failed;

    return record;
}

}    // namespace

TEST( StatisticsControllerTest, an_empty_journal_says_so_instead_of_showing_zeros )
{
    domain::QuestionLogFake log;

    StatisticsController controller{ log };

    controller.refresh();

    // « 0 % de reussite » sur un journal vide n'informe pas : la page doit pouvoir dire qu'elle n'a rien a raconter.
    EXPECT_FALSE( controller.hasHistory() );
    EXPECT_EQ( 0, controller.questionCount() );
    EXPECT_EQ( 0, controller.playingDayStreak() );

    // Et l'histogramme garde ses QUATORZE jours, meme vides : c'est ce qui lui donne son echelle, et une barre a zero se
    // lirait comme un jour absent si elle manquait.
    EXPECT_EQ( 14, controller.lastDays().size() );

    // Aucun genre, donc aucune part de camembert - et surtout aucun angle calcule sur une division par zero.
    EXPECT_TRUE( controller.kinds().isEmpty() );
    EXPECT_TRUE( controller.weakestTargets().isEmpty() );
}

TEST( StatisticsControllerTest, today_shows_up_in_the_histogram_and_the_streak )
{
    domain::QuestionLogFake log;

    const auto now = std::chrono::system_clock::now();

    log.append( recordOf( now - std::chrono::minutes{ 3 }, domain::QuestionKind::NamedInterval, true ) );
    log.append( recordOf( now - std::chrono::minutes{ 2 }, domain::QuestionKind::NamedInterval, true ) );
    log.append( recordOf( now - std::chrono::minutes{ 1 }, domain::QuestionKind::NamedInterval, false ) );

    // Et une HIER : la flamme doit compter deux jours.
    log.append( recordOf( now - std::chrono::hours{ 26 }, domain::QuestionKind::Chord, true ) );

    StatisticsController controller{ log };

    controller.refresh();

    EXPECT_TRUE( controller.hasHistory() );
    EXPECT_EQ( 4, controller.questionCount() );
    EXPECT_EQ( 75, controller.successPercent() );
    EXPECT_EQ( 2, controller.playingDayStreak() );

    // Le dernier jour de l'histogramme est AUJOURD'HUI, et il porte les trois questions du jour.
    ASSERT_EQ( 14, controller.lastDays().size() );

    const QVariantMap today = controller.lastDays().last().toMap();

    EXPECT_TRUE( today.value( QStringLiteral( "isToday" ) ).toBool() );
    EXPECT_EQ( 3, today.value( QStringLiteral( "questionCount" ) ).toInt() );
    EXPECT_EQ( 2, today.value( QStringLiteral( "correctCount" ) ).toInt() );

    // L'echelle se prend sur la journee la plus chargee, donc la plus haute barre vaut 1.
    EXPECT_DOUBLE_EQ( 1.0, today.value( QStringLiteral( "heightRatio" ) ).toDouble() );

    // Le temps de jeu : trois questions rapprochees valent deux minutes plus le temps de la derniere, et la question
    // d'hier forme un second bloc. Deux minutes et quarante secondes, donc « 2 min ».
    EXPECT_EQ( QStringLiteral( "2 min" ), controller.playTimeText() );

    // Le camembert : deux genres travailles, donc deux parts - et elles doivent faire un tour complet, sans trou.
    ASSERT_EQ( 2, controller.kinds().size() );

    double totalSweep = 0.0;

    for( const QVariant & part : controller.kinds() )
    {
        totalSweep += part.toMap().value( QStringLiteral( "sweepAngle" ) ).toDouble();
    }

    EXPECT_DOUBLE_EQ( 360.0, totalSweep );
}

TEST( StatisticsControllerTest, a_target_seen_once_is_not_a_weakness )
{
    domain::QuestionLogFake log;

    const auto now = std::chrono::system_clock::now();

    log.append( recordOf( now, domain::QuestionKind::NamedInterval, false ) );

    StatisticsController controller{ log };

    controller.refresh();

    // Une question ratee une fois n'est pas un point faible, c'est un hasard - et le SEUIL appartient au domaine.
    EXPECT_TRUE( controller.weakestTargets().isEmpty() );

    log.append( recordOf( now, domain::QuestionKind::NamedInterval, false ) );
    log.append( recordOf( now, domain::QuestionKind::NamedInterval, false ) );

    controller.refresh();

    // Trois fois, et c'est un point faible - nomme et chiffre.
    ASSERT_EQ( 1, controller.weakestTargets().size() );

    const QVariantMap weakness = controller.weakestTargets().first().toMap();

    EXPECT_EQ( 0, weakness.value( QStringLiteral( "successPercent" ) ).toInt() );

    // Le NOM vient du domaine : la page ne nomme rien elle-meme, elle affiche ce qu'on lui donne.
    EXPECT_FALSE( weakness.value( QStringLiteral( "name" ) ).toString().isEmpty() );
}

}    // namespace musichien::ui
