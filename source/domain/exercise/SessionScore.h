#pragma once

// =====================================================================================================================
// Musichien - SessionScore
//
// What a session earns and what it costs: experience, the current series, and the lives.
//
// ---------------------------------------------------------------------------------------------------------------------
// The two rules that shape this class
//
//   * AN ERROR NEVER TAKES EXPERIENCE AWAY. It costs a life, and a life is a budget of ATTEMPTS, not a
//     judgement. Removing experience already earned would punish a player for trying, which is the
//     opposite of what an application whose success criterion is "will I open it again tomorrow?"
//     should do.
//
//   * A HELPED QUESTION EARNS NOTHING. When the answer is revealed, the player has learned something,
//     but has not recognised anything. Giving experience would make asking for help profitable, and
//     help would stop being a last resort.
//
// ---------------------------------------------------------------------------------------------------------------------
// Infinite lives are a VALUE, not a special case
//
// The number of lives is an std::optional: an empty one means there is no limit. This is how the
// training mode of the first version works - no life is ever lost - while the model already holds
// everything the adventure mode will need. Turning it on is a number, never a rewrite.
// =====================================================================================================================

#include <cstddef>
#include <cstdint>
#include <optional>

namespace musichien::domain
{

class SessionScore
{
public:
    // Experience earned by any correct answer, before the series bonus and the replay penalty.
    static constexpr std::int32_t BASE_XP_PER_SUCCESS = 10;

    // Added for each successive correct answer. The bonus is what makes a series feel like a series.
    static constexpr std::int32_t XP_PER_STREAK_STEP = 2;

    // Ceiling of the series bonus: without it, a long enough series would make a single answer worth
    // more than the whole rest of the session.
    static constexpr std::int32_t MAXIMUM_STREAK_BONUS = 10;

    // Taken away for each replay of the interval before answering.
    //
    // Replaying is always allowed - nobody should be stuck - but it is not free, or the player would
    // simply listen five times and answer on the sixth.
    static constexpr std::int32_t XP_LOST_PER_REPLAY = 2;

    // A correct answer is never worth nothing: the penalty reduces the reward, it does not erase it.
    static constexpr std::int32_t MINIMUM_XP_PER_SUCCESS = 1;

    // Share of first try answers a session needs, in percent, to deserve its star.
    static constexpr std::int32_t STAR_FIRST_TRY_PERCENTAGE = 80;

    // An empty p_remainingLives means "as many as you like".
    explicit SessionScore( std::optional<std::int32_t> p_remainingLives = std::nullopt ) noexcept;

    // A question answered correctly.
    //
    // p_replayCount is how many times the interval was replayed, and p_attemptCount how many wrong
    // answers came before: zero means it was heard right the first time.
    void registerSuccess( std::int32_t p_replayCount, std::int32_t p_attemptCount ) noexcept;

    // A wrong answer. Ends nothing: the same question is asked again, which is how a learner gets to
    // try. Costs one life when there is a limit.
    void registerError() noexcept;

    // The answer was revealed. The question is over, and it brought nothing but the lesson.
    void registerHelpedQuestion() noexcept;

    [[nodiscard]] std::int32_t experience() const noexcept { return m_experience; }
    [[nodiscard]] std::int32_t streak() const noexcept { return m_streak; }

    // La meilleure serie de la session : le plus haut que le compteur soit monte, meme s'il est retombe depuis.
    [[nodiscard]] std::int32_t longestStreak() const noexcept { return m_longestStreak; }

    // Empty when the session has no limit on mistakes.
    [[nodiscard]] std::optional<std::int32_t> remainingLives() const noexcept { return m_remainingLives; }

    // True only when there IS a limit and it has been reached.
    [[nodiscard]] bool isOutOfLives() const noexcept;

    // Questions that are over, one way or the other.
    [[nodiscard]] std::size_t completedQuestionCount() const noexcept { return m_completedQuestionCount; }

    // Questions answered correctly without a single wrong attempt.
    [[nodiscard]] std::size_t firstTrySuccessCount() const noexcept { return m_firstTrySuccessCount; }

    // Questions whose answer had to be revealed.
    [[nodiscard]] std::size_t helpedQuestionCount() const noexcept { return m_helpedQuestionCount; }

    // Experience a correct answer is worth right now, given the series already earned and how many
    // times the interval was replayed. Exposed because it is a rule of the game, and because a screen
    // that wants to show "+12" must show the number the model actually gives.
    [[nodiscard]] std::int32_t experienceForSuccess( std::int32_t p_replayCount ) const noexcept;

    // The session deserves its star.
    //
    // Per session, and not per interval: knowing WHICH interval is mastered requires a memory that
    // survives the session, and that is the next step of the route. This is the version that can be
    // earned and lost within one sitting, which is exactly what the game feel needs to be judged.
    [[nodiscard]] bool hasEarnedStar() const noexcept;

private:
    std::int32_t m_experience{ 0 };
    std::int32_t m_streak{ 0 };
    std::int32_t m_longestStreak{ 0 };
    std::optional<std::int32_t> m_remainingLives;
    std::size_t m_completedQuestionCount{ 0 };
    std::size_t m_firstTrySuccessCount{ 0 };
    std::size_t m_helpedQuestionCount{ 0 };
};

}    // namespace musichien::domain
