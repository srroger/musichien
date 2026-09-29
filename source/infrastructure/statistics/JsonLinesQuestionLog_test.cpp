#include "infrastructure/statistics/JsonLinesQuestionLog.h"

#include <QFile>
#include <QIODevice>
#include <QString>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace musichien::infrastructure
{

// ---------------------------------------------------------------------------------------------------------------------
// Le journal, sur un vrai fichier
//
// Un dossier TEMPORAIRE par test : ce fichier ecrit pour de bon, et il ne doit rien laisser derriere lui ni toucher au
// journal du developpeur.
// ---------------------------------------------------------------------------------------------------------------------

namespace
{

[[nodiscard]] std::chrono::system_clock::time_point secondsAfterEpoch( std::int64_t p_seconds )
{
    return std::chrono::system_clock::time_point{ std::chrono::seconds{ p_seconds } };
}

[[nodiscard]] domain::QuestionRecord recordOf( std::chrono::system_clock::time_point p_askedAt,
                                               domain::QuestionKind p_kind,
                                               std::int32_t p_target,
                                               domain::QuestionOutcome p_outcome )
{
    domain::QuestionRecord record;

    record.askedAt = p_askedAt;
    record.kind = p_kind;
    record.target = p_target;
    record.direction = domain::IntervalDirection::Descending;
    record.outcome = p_outcome;
    record.attemptCount = 2;
    record.replayCount = 1;

    return record;
}

// Le chemin d'un journal neuf, dans un dossier temporaire qui vit le temps du test.
[[nodiscard]] QString temporaryLogPath( const QTemporaryDir & p_directory )
{
    return p_directory.filePath( QStringLiteral( "questions.jsonl" ) );
}

}    // namespace

TEST( JsonLinesQuestionLogTest, what_is_written_comes_back_whole )
{
    QTemporaryDir directory;

    ASSERT_TRUE( directory.isValid() );

    JsonLinesQuestionLog log{ temporaryLogPath( directory ) };

    const auto askedAt = secondsAfterEpoch( 1700000000 );

    log.append( recordOf( askedAt, domain::QuestionKind::NamedInterval, 7, domain::QuestionOutcome::CorrectFirstTry ) );

    log.append( recordOf( askedAt + std::chrono::seconds{ 60 },
                          domain::QuestionKind::Chord,
                          3,
                          domain::QuestionOutcome::Failed ) );

    const std::vector<domain::QuestionRecord> records = log.since( secondsAfterEpoch( 0 ) );

    ASSERT_EQ( 2U, records.size() );

    // Tout est revenu : la date, le genre, la cible, la direction, le verdict, les essais et les reecoutes. Un champ
    // perdu au passage serait une statistique fausse, et personne ne s'en apercevrait avant des semaines.
    EXPECT_EQ( askedAt, records.at( 0 ).askedAt );
    EXPECT_EQ( domain::QuestionKind::NamedInterval, records.at( 0 ).kind );
    EXPECT_EQ( 7, records.at( 0 ).target );
    EXPECT_EQ( domain::IntervalDirection::Descending, records.at( 0 ).direction );
    EXPECT_EQ( domain::QuestionOutcome::CorrectFirstTry, records.at( 0 ).outcome );
    EXPECT_EQ( 2, records.at( 0 ).attemptCount );
    EXPECT_EQ( 1, records.at( 0 ).replayCount );

    // Et l'ordre d'ecriture est l'ordre de lecture : un journal est une HISTOIRE, pas un ensemble.
    EXPECT_EQ( askedAt + std::chrono::seconds{ 60 }, records.at( 1 ).askedAt );
    EXPECT_EQ( domain::QuestionKind::Chord, records.at( 1 ).kind );
    EXPECT_EQ( domain::QuestionOutcome::Failed, records.at( 1 ).outcome );
}

TEST( JsonLinesQuestionLogTest, a_second_session_does_not_erase_the_first )
{
    QTemporaryDir directory;

    ASSERT_TRUE( directory.isValid() );

    const QString path = temporaryLogPath( directory );

    JsonLinesQuestionLog first{ path };
    first.append( recordOf( secondsAfterEpoch( 1000 ), domain::QuestionKind::NamedInterval, 7, domain::QuestionOutcome::Failed ) );

    // Un DEUXIEME journal sur le meme fichier : c'est ce qui se passe a chaque lancement de l'application, et c'est le
    // test qui compte le plus - un journal qui s'ecraserait effacerait tout l'historique a chaque demarrage.
    JsonLinesQuestionLog second{ path };
    second.append( recordOf( secondsAfterEpoch( 2000 ), domain::QuestionKind::NamedInterval, 5, domain::QuestionOutcome::CorrectFirstTry ) );

    EXPECT_EQ( 2U, second.since( secondsAfterEpoch( 0 ) ).size() );
}

TEST( JsonLinesQuestionLogTest, an_absent_journal_is_a_first_launch_not_an_error )
{
    QTemporaryDir directory;

    ASSERT_TRUE( directory.isValid() );

    // Aucun fichier n'a ete ecrit : lire doit rendre une liste vide, sans planter et sans bruit.
    const JsonLinesQuestionLog log{ temporaryLogPath( directory ) };

    EXPECT_TRUE( log.since( secondsAfterEpoch( 0 ) ).empty() );

    // Et le chemin reste celui qu'on lui a donne, pour que l'application puisse le montrer.
    EXPECT_EQ( temporaryLogPath( directory ), log.filePath() );
}

TEST( JsonLinesQuestionLogTest, a_broken_line_is_skipped_and_the_rest_is_kept )
{
    QTemporaryDir directory;

    ASSERT_TRUE( directory.isValid() );

    const QString path = temporaryLogPath( directory );

    JsonLinesQuestionLog log{ path };

    log.append( recordOf( secondsAfterEpoch( 1000 ), domain::QuestionKind::NamedInterval, 7, domain::QuestionOutcome::Failed ) );

    // Une ligne abimee : un plantage en pleine ecriture, ou un fichier ouvert et edite a la main. Elle coute SA ligne,
    // et rien d'autre.
    {
        QFile file{ path };

        ASSERT_TRUE( file.open( QIODevice::WriteOnly | QIODevice::Append ) );

        file.write( "ceci n'est pas du json\n" );
    }

    log.append( recordOf( secondsAfterEpoch( 3000 ), domain::QuestionKind::Chord, 1, domain::QuestionOutcome::Revealed ) );

    const std::vector<domain::QuestionRecord> records = log.since( secondsAfterEpoch( 0 ) );

    ASSERT_EQ( 2U, records.size() );

    EXPECT_EQ( domain::QuestionKind::NamedInterval, records.at( 0 ).kind );
    EXPECT_EQ( domain::QuestionKind::Chord, records.at( 1 ).kind );
    EXPECT_EQ( domain::QuestionOutcome::Revealed, records.at( 1 ).outcome );
}

TEST( JsonLinesQuestionLogTest, the_reading_filter_keeps_only_the_recent_questions )
{
    QTemporaryDir directory;

    ASSERT_TRUE( directory.isValid() );

    JsonLinesQuestionLog log{ temporaryLogPath( directory ) };

    log.append( recordOf( secondsAfterEpoch( 1000 ), domain::QuestionKind::NamedInterval, 7, domain::QuestionOutcome::Failed ) );
    log.append( recordOf( secondsAfterEpoch( 5000 ), domain::QuestionKind::NamedInterval, 7, domain::QuestionOutcome::Failed ) );

    // Seules les questions posees APRES l'instant demande reviennent : c'est ce qui donnera les filtres « sept jours »,
    // « trente jours » et « tout » de la page de statistiques, sans qu'aucun d'eux ait besoin d'exister ici.
    EXPECT_EQ( 1U, log.since( secondsAfterEpoch( 2000 ) ).size() );
    EXPECT_EQ( 2U, log.since( secondsAfterEpoch( 0 ) ).size() );
    EXPECT_EQ( 0U, log.since( secondsAfterEpoch( 6000 ) ).size() );
}

TEST( JsonLinesQuestionLogTest, a_clear_leaves_nothing_behind )
{
    QTemporaryDir directory;

    ASSERT_TRUE( directory.isValid() );

    const QString path = temporaryLogPath( directory );

    JsonLinesQuestionLog log{ path };

    log.append( recordOf( secondsAfterEpoch( 1000 ), domain::QuestionKind::NamedInterval, 7, domain::QuestionOutcome::Failed ) );

    ASSERT_EQ( 1U, log.since( secondsAfterEpoch( 0 ) ).size() );

    log.clear();

    EXPECT_TRUE( log.since( secondsAfterEpoch( 0 ) ).empty() );

    // Et le FICHIER n'est plus la : une remise a zero qui laisserait l'ancien journal derriere elle ne serait pas une
    // remise a zero, et quelqu'un qui ouvre le dossier le verrait tout de suite.
    EXPECT_FALSE( QFile::exists( path ) );

    // Enfin, ecrire apres une remise a zero repart d'un journal PROPRE, et pas d'un fichier a moitie efface.
    log.append( recordOf( secondsAfterEpoch( 2000 ), domain::QuestionKind::Chord, 1, domain::QuestionOutcome::CorrectFirstTry ) );

    const std::vector<domain::QuestionRecord> records = log.since( secondsAfterEpoch( 0 ) );

    ASSERT_EQ( 1U, records.size() );
    EXPECT_EQ( domain::QuestionKind::Chord, records.at( 0 ).kind );
}

}    // namespace musichien::infrastructure
