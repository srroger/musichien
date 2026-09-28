#include "domain/exercise/QuestionRecord.h"

namespace musichien::domain
{

QuestionOutcome outcomeOf( bool p_wasCorrect, bool p_wasRevealed, std::int32_t p_attemptCount ) noexcept
{
    if( p_wasRevealed )
    {
        // La reponse a ete donnee : ce n'est ni une reussite ni un echec, et l'ordre des tests importe - une question
        // revelee arrive TOUJOURS avant l'autre cas, meme si le joueur avait fini par trouver.
        return QuestionOutcome::Revealed;
    }

    if( !p_wasCorrect )
    {
        return QuestionOutcome::Failed;
    }

    // Du premier coup, ou non : c'est la difference entre savoir et finir par trouver, et c'est la seule information
    // qui dit si un intervalle est ACQUIS.
    return ( p_attemptCount <= 1 ) ? QuestionOutcome::CorrectFirstTry : QuestionOutcome::CorrectAfterRetries;
}

}    // namespace musichien::domain
