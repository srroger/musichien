#include "ui/StatisticsController.h"

#include "domain/music/Chord.h"
#include "domain/music/Interval.h"
#include "domain/rhythm/RhythmPattern.h"
#include "ui/ModeDescription.h"

#include <QChar>
#include <QDate>
#include <QDateTime>
#include <QVariantMap>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <set>
#include <string>
#include <string_view>

namespace musichien::ui
{

namespace
{

// La page regarde les QUATORZE derniers jours pour l'histogramme : deux semaines, c'est ce qui tient sur la largeur d'un
// telephone sans que les barres deviennent des traits.
constexpr int HISTOGRAM_DAY_COUNT = 14;

// Le temps de jeu « recent » : la semaine en cours, ce qui repond a « est-ce que je joue en ce moment ? ».
constexpr qint64 RECENT_PLAY_TIME_DAYS = 7;

// Combien de points faibles la page montre. Six, et pas vingt : une liste de vingt ne se lit pas, elle se survole.
constexpr std::size_t WEAKEST_TARGET_COUNT = 6;

constexpr qint64 MINUTES_PER_HOUR = 60;

// La date LOCALE d'un instant.
//
// C'est le seul endroit de tout ce chantier ou un fuseau horaire entre en jeu, et c'est pour cela qu'il est ICI : une
// question posee a 23 h appartient a la journee d'hier soir, et non a celle d'aujourd'hui en temps universel. Le domaine
// ne peut pas le savoir, et il n'a pas a le savoir.
[[nodiscard]] QDate localDateOf( std::chrono::system_clock::time_point p_instant )
{
    const qint64 milliseconds =
      std::chrono::duration_cast<std::chrono::milliseconds>( p_instant.time_since_epoch() ).count();

    return QDateTime::fromMSecsSinceEpoch( milliseconds ).date();
}

// Une duree, en clair : « 3 h 25 », « 12 min », « moins d'une minute ».
[[nodiscard]] QString describeDuration( std::chrono::seconds p_duration )
{
    const qint64 totalMinutes = std::chrono::duration_cast<std::chrono::minutes>( p_duration ).count();

    if( totalMinutes < 1 )
    {
        return StatisticsController::tr( "moins d'une minute" );
    }

    if( totalMinutes < MINUTES_PER_HOUR )
    {
        return StatisticsController::tr( "%1 min" ).arg( totalMinutes );
    }

    const qint64 hours = totalMinutes / MINUTES_PER_HOUR;
    const qint64 minutes = totalMinutes % MINUTES_PER_HOUR;

    if( minutes == 0 )
    {
        return StatisticsController::tr( "%1 h" ).arg( hours );
    }

    return StatisticsController::tr( "%1 h %2" ).arg( hours ).arg( minutes, 2, 10, QChar{ '0' } );
}

// Le nom d'un genre de question, tel qu'un joueur le lit.
[[nodiscard]] QString describeKind( domain::QuestionKind p_kind )
{
    switch( p_kind )
    {
        case domain::QuestionKind::NamedInterval:
            return StatisticsController::tr( "Intervalles" );

        case domain::QuestionKind::Direction:
            return StatisticsController::tr( "Sens" );

        case domain::QuestionKind::Sing:
            return StatisticsController::tr( "Chant" );

        case domain::QuestionKind::Rhythm:
            return StatisticsController::tr( "Rythme" );

        case domain::QuestionKind::Chord:
            return StatisticsController::tr( "Accords" );

        // Les deux questions d'harmonie, nommees SEPAREMENT : c'est tout l'interet de les avoir separees dans le domaine.
        // Un joueur qui entend les couleurs et ne sait pas les nommer doit pouvoir le lire sur cette page.
        case domain::QuestionKind::ModeColour:
            return StatisticsController::tr( "Modes : couleur" );

        case domain::QuestionKind::ModeName:
            return StatisticsController::tr( "Modes : nom" );
    }

    return {};
}

// Le nom lisible d'une cible : ce que l'ecran affiche dans la liste des points faibles.
//
// Le nom vient du DOMAINE - Interval, Chord, RhythmPattern : l'ecran ne nomme rien lui-meme, il traduit ce que le
// domaine lui donne, et c'est ce qui garantit que deux ecrans diront la meme chose.
[[nodiscard]] QString describeTarget( const domain::TargetStatistics & p_target )
{
    switch( p_target.kind )
    {
        case domain::QuestionKind::Chord: {
            const std::string_view name =
              domain::chordQualityName( static_cast<domain::ChordQuality>( p_target.target ) );

            return QString::fromUtf8( name.data(), static_cast<int>( name.size() ) );
        }

        case domain::QuestionKind::Rhythm: {
            const std::vector<domain::RhythmPattern> & patterns = domain::allRhythmPatterns();

            if( ( p_target.target < 0 ) || ( static_cast<std::size_t>( p_target.target ) >= patterns.size() ) )
            {
                return StatisticsController::tr( "Cellule rythmique" );
            }

            const std::string_view name = patterns.at( static_cast<std::size_t>( p_target.target ) ).name();

            return QString::fromUtf8( name.data(), static_cast<int>( name.size() ) );
        }

        case domain::QuestionKind::ModeColour:
        case domain::QuestionKind::ModeName: {
            // La cible d'une question d'harmonie est l'INDEX du mode pose. Un index hors bornes ne peut venir que d'un
            // journal edite a la main : il coute un nom, jamais la page.
            //
            // Le SIGNE d'abord, la borne ensuite : convertir un index negatif en size_t en ferait un tres grand nombre,
            // et la borne passerait alors pour la mauvaise raison.
            constexpr auto MODE_COUNT = static_cast<std::int32_t>( domain::MODE_COUNT );

            if( p_target.target < 0 )
            {
                return StatisticsController::tr( "Mode" );
            }

            if( p_target.target >= MODE_COUNT )
            {
                return StatisticsController::tr( "Mode" );
            }

            // Le nom vient de la MEME description que les boutons de l'ecran d'exercice : la page de statistiques et le
            // jeu ne peuvent donc pas appeler le meme mode de deux facons.
            const QVariantMap description = describeMode( static_cast<domain::Mode>( p_target.target ) );

            return description.value( QStringLiteral( "name" ) ).toString();
        }

        case domain::QuestionKind::NamedInterval:
        case domain::QuestionKind::Direction:
        case domain::QuestionKind::Sing: {
            const QString name = QString::fromStdString( domain::Interval{ p_target.target }.name() );

            // La fleche dit le SENS, et elle compte : une sixte montante et une sixte descendante ne se ratent pas pour
            // la meme raison, et les confondre effacerait exactement ce que cette page doit montrer.
            const QString arrow = ( p_target.direction == domain::IntervalDirection::Ascending )
                                    ? QStringLiteral( " ↑" )
                                    : QStringLiteral( " ↓" );

            return name + arrow;
        }
    }

    return {};
}

}    // namespace

StatisticsController::StatisticsController( const domain::QuestionLog & p_log, QObject * p_parent )
  : QObject{ p_parent }
  , m_log{ p_log }
{
}

void StatisticsController::refresh()
{
    // TOUT le journal, d'un coup. Le lire entier prend quelques millisecondes pour quelques milliers de lignes, et c'est
    // ce qui permet a la page de dire la verite sur la duree TOTALE. Le jour ou ce fichier sera gros, on lira par
    // tranches - mais ce jour n'est pas arrive, et complexifier maintenant serait payer pour rien.
    m_records = m_log.since( std::chrono::system_clock::time_point{} );

    domain::StatisticsFilter everything;
    everything.since = std::chrono::system_clock::time_point{};

    m_overall = domain::computeStatistics( m_records, everything );

    m_playTime = domain::playTimeOf( m_records );
    m_playTimeText = describeDuration( m_playTime );

    // Le temps de jeu de la SEMAINE : deux chiffres valent mieux qu'un, parce que le total parle du passe et la semaine
    // du present.
    const auto weekStart = std::chrono::system_clock::now() - ( std::chrono::hours{ 24 } * RECENT_PLAY_TIME_DAYS );

    std::vector<domain::QuestionRecord> recentRecords;

    for( const domain::QuestionRecord & record : m_records )
    {
        if( record.askedAt >= weekStart )
        {
            recentRecords.push_back( record );
        }
    }

    m_recentPlayTimeText = describeDuration( domain::playTimeOf( recentRecords ) );

    // LA FLAMME : combien de jours d'affilee le joueur a pose au moins une question, en partant d'aujourd'hui.
    //
    // Aujourd'hui ne compte que s'il a joue AUJOURD'HUI : un joueur qui a bien travaille la semaine derniere mais pas
    // depuis deux jours a une flamme de zero, et c'est voulu - une flamme dit « viens jouer maintenant », pas « tu as
    // bien travaille ». C'est la meme regle que dans les jeux qui l'ont rendue celebre, et elle ne vaut que parce qu'elle
    // est severe.
    std::set<QDate> playedDays;

    for( const domain::QuestionRecord & record : m_records )
    {
        playedDays.insert( localDateOf( record.askedAt ) );
    }

    m_playingDayStreak = 0;

    for( QDate day = QDate::currentDate(); playedDays.contains( day ); day = day.addDays( -1 ) )
    {
        ++m_playingDayStreak;
    }

    // L'HISTOGRAMME : les quatorze derniers jours, du plus ANCIEN au plus recent a l'ecran - un graphique se lit de gauche
    // a droite, et le temps va dans ce sens.
    m_lastDays.clear();

    const QDate today = QDate::currentDate();

    std::array<int, HISTOGRAM_DAY_COUNT> questionsPerDay{};
    std::array<int, HISTOGRAM_DAY_COUNT> correctPerDay{};

    for( const domain::QuestionRecord & record : m_records )
    {
        const qint64 daysAgo = localDateOf( record.askedAt ).daysTo( today );

        if( ( daysAgo < 0 ) || ( daysAgo >= HISTOGRAM_DAY_COUNT ) )
        {
            continue;
        }

        const auto index = static_cast<std::size_t>( HISTOGRAM_DAY_COUNT - 1 - daysAgo );

        questionsPerDay.at( index ) += 1;

        if( record.isCorrect() )
        {
            correctPerDay.at( index ) += 1;
        }
    }

    // La journee la plus chargee donne l'echelle. Le maximum est borne a UN au moins : sans cela, une semaine vide
    // diviserait par zero, et une page de statistiques n'a pas le droit de planter sur un joueur qui n'a rien fait.
    const int busiestDay = std::max( *std::ranges::max_element( questionsPerDay ), 1 );

    for( std::size_t index = 0; index < questionsPerDay.size(); ++index )
    {
        const QDate day = today.addDays( -static_cast<int>( questionsPerDay.size() - 1 - index ) );

        QVariantMap described;

        // Le libelle du jour vient de la LOCALE du telephone : sur un appareil en francais, « lun. ».
        described.insert( QStringLiteral( "label" ), day.toString( QStringLiteral( "ddd" ) ) );
        described.insert( QStringLiteral( "dayOfMonth" ), day.day() );
        described.insert( QStringLiteral( "questionCount" ), questionsPerDay[index] );
        described.insert( QStringLiteral( "correctCount" ), correctPerDay[index] );
        described.insert( QStringLiteral( "heightRatio" ),
                          static_cast<double>( questionsPerDay[index] ) / static_cast<double>( busiestDay ) );
        described.insert( QStringLiteral( "isToday" ), day == today );

        m_lastDays.append( described );
    }

    // LA REPARTITION par genre : ce que le joueur travaille vraiment, et ce qu'il delaisse sans le savoir.
    m_kinds.clear();

    const std::array<domain::QuestionKind, 7> kinds{ domain::QuestionKind::NamedInterval,
                                                     domain::QuestionKind::Direction,
                                                     domain::QuestionKind::Sing,
                                                     domain::QuestionKind::Rhythm,
                                                     domain::QuestionKind::Chord,
                                                     domain::QuestionKind::ModeColour,
                                                     domain::QuestionKind::ModeName };

    QVariantList parts;

    for( const domain::QuestionKind kind : kinds )
    {
        domain::StatisticsFilter filter = everything;
        filter.kind = kind;

        const domain::QuestionStatistics statistics = domain::computeStatistics( m_records, filter );

        // Un genre que le joueur n'a jamais rencontre ne figure pas dans le camembert : une part de zero degre ne se voit
        // pas, et une legende qui annonce « Rythme 0 % » ne fait qu'occuper de la place sur un ecran de telephone.
        if( statistics.questionCount == 0 )
        {
            continue;
        }

        QVariantMap described;

        described.insert( QStringLiteral( "name" ), describeKind( kind ) );
        described.insert( QStringLiteral( "questionCount" ), static_cast<int>( statistics.questionCount ) );
        described.insert( QStringLiteral( "successPercent" ), statistics.successPercent() );

        parts.append( described );
    }

    // Les ANGLES du camembert sont calcules ICI plutot que dans l'ecran : un ecran qui les calculerait lui-meme devrait
    // les calculer deux fois - une pour la forme, une pour la legende - et finirait par les faire diverger.
    double startAngle = 90.0;    // on commence en haut, comme une horloge, et on tourne dans le sens des aiguilles

    for( const QVariant & part : parts )
    {
        QVariantMap described = part.toMap();

        const double share = static_cast<double>( described.value( QStringLiteral( "questionCount" ) ).toInt() )
                             / static_cast<double>( m_overall.questionCount );

        const double sweep = share * 360.0;

        described.insert( QStringLiteral( "sharePercent" ), static_cast<int>( std::lround( share * 100.0 ) ) );
        described.insert( QStringLiteral( "startAngle" ), startAngle );
        described.insert( QStringLiteral( "sweepAngle" ), sweep );

        startAngle += sweep;

        m_kinds.append( described );
    }

    // LES POINTS FAIBLES, du plus faible au moins faible : c'est la raison d'etre de toute cette page. « 72 % de
    // reussite » ne dit rien a personne ; « tu rates les sixtes » dit tout.
    m_weakestTargets.clear();

    const std::vector<domain::TargetStatistics> byTarget = domain::statisticsByTarget( m_records, everything );

    for( const domain::TargetStatistics & target : byTarget )
    {
        // Une cible vue une ou deux fois n'est pas un point faible, c'est un hasard : le SEUIL vit dans le domaine, et la
        // page s'y tient comme n'importe qui d'autre.
        if( target.statistics.questionCount < domain::MINIMUM_OBSERVATIONS_FOR_A_WEAKNESS )
        {
            continue;
        }

        QVariantMap described;

        described.insert( QStringLiteral( "name" ), describeTarget( target ) );
        described.insert( QStringLiteral( "successPercent" ), target.statistics.successPercent() );
        described.insert( QStringLiteral( "questionCount" ), static_cast<int>( target.statistics.questionCount ) );

        m_weakestTargets.append( described );

        if( m_weakestTargets.size() >= static_cast<int>( WEAKEST_TARGET_COUNT ) )
        {
            break;
        }
    }

    emit statisticsChanged();
}

int StatisticsController::questionCount() const noexcept
{
    return static_cast<int>( m_overall.questionCount );
}

int StatisticsController::successPercent() const noexcept
{
    return m_overall.successPercent();
}

int StatisticsController::firstTryPercent() const noexcept
{
    return m_overall.firstTryPercent();
}

}    // namespace musichien::ui
