#include "domain/exercise/QuestionStatistics.h"

#include <algorithm>
#include <cmath>

namespace musichien::domain
{

namespace
{

// Ce que le filtre laisse passer.
[[nodiscard]] bool matches( const QuestionRecord & p_record, const StatisticsFilter & p_filter ) noexcept
{
    if( p_record.askedAt < p_filter.since )
    {
        return false;
    }

    // Un genre demande : tout le reste passe a la trappe. C'est ce qui permet de dire « je veux voir le chant » ou
    // « seulement les accords », sans que l'ecran ait a filtrer quoi que ce soit lui-meme.
    //
    // L'ACCES SE FAIT PAR L'OPERATEUR, et non par value() : ce fichier marque ses fonctions noexcept, et value() peut
    // lever - donc la fonction entiere devenait une fonction qui peut quitter par une exception, ce qu'un noexcept
    // interdit. L'ecriture ci-dessous dit la meme chose sans en avoir l'air.
    return !p_filter.kind.has_value() || ( p_record.kind == *p_filter.kind );
}

void accumulate( QuestionStatistics & p_statistics, const QuestionRecord & p_record ) noexcept
{
    ++p_statistics.questionCount;

    p_statistics.totalAttemptCount += p_record.attemptCount;
    p_statistics.totalReplayCount += p_record.replayCount;

    switch( p_record.outcome )
    {
        case QuestionOutcome::CorrectFirstTry:
            ++p_statistics.correctFirstTryCount;
            break;

        case QuestionOutcome::CorrectAfterRetries:
            ++p_statistics.correctAfterRetriesCount;
            break;

        case QuestionOutcome::Failed:
            ++p_statistics.failedCount;
            break;

        case QuestionOutcome::Revealed:
            ++p_statistics.revealedCount;
            break;
    }
}

// Le pourcentage, de 0 a 100. Sans rien a mesurer, il vaut ZERO : un ecran qui afficherait NaN, ou pire 100 %, pour
// une question qui n'a jamais ete posee mentirait dans les deux cas.
[[nodiscard]] int percentOf( std::size_t p_part, std::size_t p_whole ) noexcept
{
    if( p_whole == 0 )
    {
        return 0;
    }

    const double ratio = static_cast<double>( p_part ) / static_cast<double>( p_whole );

    return static_cast<int>( std::lround( ratio * 100.0 ) );
}

}    // namespace

int QuestionStatistics::successPercent() const noexcept
{
    return percentOf( correctCount(), questionCount );
}

int QuestionStatistics::firstTryPercent() const noexcept
{
    return percentOf( correctFirstTryCount, questionCount );
}

double QuestionStatistics::averageReplays() const noexcept
{
    if( questionCount == 0 )
    {
        return 0.0;
    }

    return static_cast<double>( totalReplayCount ) / static_cast<double>( questionCount );
}

QuestionStatistics computeStatistics( std::span<const QuestionRecord> p_records, const StatisticsFilter & p_filter )
{
    QuestionStatistics statistics;

    for( const QuestionRecord & record : p_records )
    {
        if( matches( record, p_filter ) )
        {
            accumulate( statistics, record );
        }
    }

    return statistics;
}

std::vector<TargetStatistics> statisticsByTarget( std::span<const QuestionRecord> p_records,
                                                  const StatisticsFilter & p_filter )
{
    std::vector<TargetStatistics> byTarget;

    for( const QuestionRecord & record : p_records )
    {
        if( !matches( record, p_filter ) )
        {
            continue;
        }

        // Une cible, c'est le TRIPLE (genre, cible, direction) : une sixte montante et une sixte descendante sont deux
        // exercices differents, et les confondre effacerait exactement ce que ces statistiques doivent montrer.
        const auto sameTarget = [&record]( const TargetStatistics & p_entry ) {
            return ( p_entry.kind == record.kind ) && ( p_entry.target == record.target )
                   && ( p_entry.direction == record.direction );
        };

        auto found = std::ranges::find_if( byTarget, sameTarget );

        if( found == byTarget.end() )
        {
            TargetStatistics entry;
            entry.kind = record.kind;
            entry.target = record.target;
            entry.direction = record.direction;

            byTarget.push_back( entry );

            found = std::prev( byTarget.end() );
        }

        accumulate( found->statistics, record );
    }

    // De la PLUS FAIBLE a la mieux reussie : les points faibles en tete, et c'est l'ordre dans lequel un joueur veut
    // les lire. A taux egal, celle qu'on a le plus jouee passe devant - une cible vue trente fois dit plus qu'une cible
    // vue trois fois.
    std::ranges::sort( byTarget, []( const TargetStatistics & p_left, const TargetStatistics & p_right ) {
        if( p_left.statistics.successPercent() != p_right.statistics.successPercent() )
        {
            return p_left.statistics.successPercent() < p_right.statistics.successPercent();
        }

        if( p_left.statistics.questionCount != p_right.statistics.questionCount )
        {
            return p_left.statistics.questionCount > p_right.statistics.questionCount;
        }

        // Et un ordre TOTAL en dernier recours : sans lui, deux cibles egales en tout pourraient changer d'ordre d'un
        // appel a l'autre, et un ecran qui bouge tout seul donne l'impression que les chiffres changent.
        if( p_left.kind != p_right.kind )
        {
            return p_left.kind < p_right.kind;
        }

        if( p_left.target != p_right.target )
        {
            return p_left.target < p_right.target;
        }

        return p_left.direction < p_right.direction;
    } );

    return byTarget;
}

std::chrono::seconds playTimeOf( std::span<const QuestionRecord> p_records, std::chrono::seconds p_idleGap )
{
    if( p_records.empty() )
    {
        return std::chrono::seconds{ 0 };
    }

    // Les instants, TRIES. Le journal est ecrit dans l'ordre, mais une fonction ne doit pas dependre de l'ordre de ce
    // qu'on lui donne : un journal lu a l'envers produirait une duree negative, ce qui serait une drole de statistique.
    std::vector<std::chrono::system_clock::time_point> instants;
    instants.reserve( p_records.size() );

    for( const QuestionRecord & record : p_records )
    {
        instants.push_back( record.askedAt );
    }

    std::ranges::sort( instants );

    std::chrono::seconds total{ 0 };

    auto blockStart = instants.front();
    auto blockEnd = instants.front();

    for( const auto instant : instants )
    {
        if( ( instant - blockEnd ) > p_idleGap )
        {
            // La pause est trop longue : le bloc est fini, on le compte, et un nouveau commence.
            total += std::chrono::duration_cast<std::chrono::seconds>( blockEnd - blockStart ) + ASSUMED_QUESTION_DURATION;
            blockStart = instant;
        }

        blockEnd = instant;
    }

    // Le dernier bloc compte aussi - et c'est meme le plus souvent le seul.
    total += std::chrono::duration_cast<std::chrono::seconds>( blockEnd - blockStart ) + ASSUMED_QUESTION_DURATION;

    return total;
}

}    // namespace musichien::domain
