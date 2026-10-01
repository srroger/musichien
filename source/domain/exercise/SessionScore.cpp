#include "domain/exercise/SessionScore.h"

#include <algorithm>

namespace musichien::domain
{

SessionScore::SessionScore( std::optional<std::int32_t> p_remainingLives ) noexcept
  : m_remainingLives{ p_remainingLives }
{
}

std::int32_t SessionScore::experienceForSuccess( std::int32_t p_replayCount ) const noexcept
{
    // The series bonus uses the series BEFORE this answer: the first correct answer of a session is
    // worth the base, and the bonus appears from the second one on. Paying for an answer with the
    // series it is about to create would make the very first answer of a session worth more than any
    // other, for no reason a player could see.
    const std::int32_t streakBonus =
      std::min( m_streak * XP_PER_STREAK_STEP, MAXIMUM_STREAK_BONUS );

    const std::int32_t replayPenalty = std::max( 0, p_replayCount ) * XP_LOST_PER_REPLAY;

    return std::max( MINIMUM_XP_PER_SUCCESS, BASE_XP_PER_SUCCESS + streakBonus - replayPenalty );
}

void SessionScore::registerSuccess( std::int32_t p_replayCount, std::int32_t p_attemptCount ) noexcept
{
    m_experience += experienceForSuccess( p_replayCount );

    ++m_streak;

    // La plus longue serie DE LA SESSION, et pas seulement la derniere.
    //
    // C'est ce que la Survie garde : « ta meilleure serie » n'a de sens que si elle survit a l'erreur qui l'a cassee. La
    // derniere serie, elle, retombe a chaque erreur et ne raconte rien.
    m_longestStreak = std::max( m_longestStreak, m_streak );

    ++m_completedQuestionCount;

    if( p_attemptCount <= 0 )
    {
        ++m_firstTrySuccessCount;
    }
}

void SessionScore::registerError() noexcept
{
    // The series goes DOWN A STEP, but never collapses to zero at once: it is a ladder, not a switch. An error
    // halves the streak, exactly like the Devil May Cry style that fades without ever dropping dead. Experience is
    // left untouched, as always.
    m_streak /= 2;

    if( m_remainingLives.has_value() )
    {
        // operator* rather than value(): value() throws when the optional is empty, and a rule of the
        // game must not be able to throw. The check above is what makes it safe.
        m_remainingLives = std::max( 0, *m_remainingLives - 1 );
    }
}

void SessionScore::registerHelpedQuestion() noexcept
{
    // An empty series rather than a broken one, because a helped question is neither a success nor a
    // mistake: the player did not know, and asked. Keeping the series alive would make asking for help
    // a way to protect a bonus.
    m_streak = 0;

    ++m_completedQuestionCount;
    ++m_helpedQuestionCount;
}

bool SessionScore::isOutOfLives() const noexcept
{
    // operator* rather than value(), for the same reason as above: this function is noexcept, and
    // value() can throw.
    return m_remainingLives.has_value() && ( *m_remainingLives <= 0 );
}

bool SessionScore::hasEarnedStar() const noexcept
{
    if( m_completedQuestionCount == 0 )
    {
        return false;
    }

    // One single helped question is enough to lose the star: the star is there to say "I recognised
    // these by ear", and a revealed answer is exactly the moment that sentence became false.
    if( m_helpedQuestionCount > 0 )
    {
        return false;
    }

    const std::size_t firstTryCount = m_firstTrySuccessCount;

    // Multiplication rather than a division: comparing shares without rounding, and without ever
    // dividing by a count that could be zero.
    return ( firstTryCount * 100 ) >= ( m_completedQuestionCount * STAR_FIRST_TRY_PERCENTAGE );
}

}    // namespace musichien::domain
