#include "infrastructure/statistics/JsonLinesQuestionLog.h"

#include <QDateTime>
#include <QFile>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimeZone>

#include <chrono>

namespace musichien::infrastructure
{

namespace
{

// Les cles du fichier, ecrites une fois.
constexpr const char * ASKED_AT_KEY = "at";
constexpr const char * KIND_KEY = "kind";
constexpr const char * TARGET_KEY = "target";
constexpr const char * DIRECTION_KEY = "direction";
constexpr const char * OUTCOME_KEY = "outcome";
constexpr const char * ATTEMPT_COUNT_KEY = "attempts";
constexpr const char * REPLAY_COUNT_KEY = "replays";

[[nodiscard]] qint64 millisecondsOf( std::chrono::system_clock::time_point p_instant ) noexcept
{
    return std::chrono::duration_cast<std::chrono::milliseconds>( p_instant.time_since_epoch() ).count();
}

[[nodiscard]] QJsonObject jsonOf( const domain::QuestionRecord & p_record )
{
    QJsonObject line;

    // Une date ISO 8601 en UTC, avec les millisecondes : c'est le format qu'un humain lit, qu'un tableur comprend, et
    // qu'aucun fuseau horaire ne peut rendre ambigu.
    line.insert( ASKED_AT_KEY,
                 QDateTime::fromMSecsSinceEpoch( millisecondsOf( p_record.askedAt ), QTimeZone::UTC )
                   .toString( Qt::ISODateWithMs ) );

    line.insert( KIND_KEY, static_cast<int>( p_record.kind ) );
    line.insert( TARGET_KEY, p_record.target );
    line.insert( DIRECTION_KEY, static_cast<int>( p_record.direction ) );
    line.insert( OUTCOME_KEY, static_cast<int>( p_record.outcome ) );
    line.insert( ATTEMPT_COUNT_KEY, p_record.attemptCount );
    line.insert( REPLAY_COUNT_KEY, p_record.replayCount );

    return line;
}

[[nodiscard]] domain::QuestionRecord recordOf( const QJsonObject & p_line )
{
    domain::QuestionRecord record;

    const QDateTime askedAt = QDateTime::fromString( p_line.value( ASKED_AT_KEY ).toString(), Qt::ISODateWithMs );

    record.askedAt = std::chrono::system_clock::time_point{ std::chrono::milliseconds{ askedAt.toMSecsSinceEpoch() } };

    // Un champ absent se lit avec sa valeur par defaut, ce qui est exactement ce qu'on veut d'un fichier ecrit par une
    // version plus ancienne : la ligne reste lisible, elle est seulement moins riche.
    record.kind = static_cast<domain::QuestionKind>( p_line.value( KIND_KEY ).toInt( 0 ) );
    record.target = p_line.value( TARGET_KEY ).toInt( 0 );
    record.direction = static_cast<domain::IntervalDirection>( p_line.value( DIRECTION_KEY ).toInt( 0 ) );
    record.outcome = static_cast<domain::QuestionOutcome>( p_line.value( OUTCOME_KEY ).toInt( 0 ) );
    record.attemptCount = p_line.value( ATTEMPT_COUNT_KEY ).toInt( 1 );
    record.replayCount = p_line.value( REPLAY_COUNT_KEY ).toInt( 0 );

    return record;
}

}    // namespace

JsonLinesQuestionLog::JsonLinesQuestionLog( QString p_filePath )
  : m_filePath{ std::move( p_filePath ) }
{
}

void JsonLinesQuestionLog::append( const domain::QuestionRecord & p_record )
{
    QFile file{ m_filePath };

    // AJOUT SEUL. Un fichier absent est cree, un dossier absent fait echouer l'ouverture - et dans ce cas le journal se
    // TAIT plutot que d'interrompre la partie. Une statistique n'est pas une regle du jeu.
    if( !file.open( QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text ) )
    {
        return;
    }

    file.write( QJsonDocument{ jsonOf( p_record ) }.toJson( QJsonDocument::Compact ) );
    file.write( "\n" );
}

std::vector<domain::QuestionRecord> JsonLinesQuestionLog::since( std::chrono::system_clock::time_point p_since ) const
{
    std::vector<domain::QuestionRecord> records;

    QFile file{ m_filePath };

    if( !file.open( QIODevice::ReadOnly | QIODevice::Text ) )
    {
        // Pas de journal : pas de statistiques. Un fichier absent n'est pas une erreur, c'est un premier lancement.
        return records;
    }

    while( !file.atEnd() )
    {
        const QByteArray line = file.readLine();

        const QJsonDocument document = QJsonDocument::fromJson( line );

        // Une ligne illisible est SAUTEE, et pas fatale : un fichier tronque par un plantage, ou ouvert et edite a la
        // main, ne doit pas rendre tout l'historique inaccessible.
        if( !document.isObject() )
        {
            continue;
        }

        const domain::QuestionRecord record = recordOf( document.object() );

        if( record.askedAt >= p_since )
        {
            records.push_back( record );
        }
    }

    return records;
}

}    // namespace musichien::infrastructure
