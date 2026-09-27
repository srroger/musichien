#include "domain/exercise/SessionScore.h"

#include "domain/exercise/Rank.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>

namespace musichien::domain
{

// ---------------------------------------------------------------------------------------------------------------------
// What a session earns, and what it costs
//
// Two rules carry the whole class, and both are about NOT punishing:
//
//   * an error costs an attempt, never experience;
//   * a revealed answer earns nothing, and that is its only cost.
//
// These tests exist because those two sentences are easy to write in a comment and easy to break in a
// line of code six months later.
// ---------------------------------------------------------------------------------------------------------------------

namespace
{

// No limit on mistakes: the training mode of the first version.
constexpr std::optional<std::int32_t> UNLIMITED_LIVES{};

}    // namespace

TEST( SessionScoreTest, a_first_correct_answer_earns_the_base_experience )
{
    SessionScore score{ UNLIMITED_LIVES };

    // First try, no replay: nothing to add, nothing to take away.
    score.registerSuccess( 0, 0 );

    EXPECT_EQ( SessionScore::BASE_XP_PER_SUCCESS, score.experience() );
    EXPECT_EQ( 1, score.streak() );
}

TEST( SessionScoreTest, a_series_increases_the_reward )
{
    SessionScore score{ UNLIMITED_LIVES };

    score.registerSuccess( 0, 0 );
    const std::int32_t afterFirst = score.experience();

    score.registerSuccess( 0, 0 );
    const std::int32_t afterSecond = score.experience();

    // The second answer of a series is worth more than the first: that is what makes a series feel
    // like one. The exact size of the bonus is a tuning value, its existence is the rule.
    EXPECT_GT( afterSecond - afterFirst, SessionScore::BASE_XP_PER_SUCCESS );
}

TEST( SessionScoreTest, the_series_bonus_has_a_ceiling )
{
    SessionScore score{ UNLIMITED_LIVES };

    for( std::int32_t index = 0; index < 100; ++index )
    {
        score.registerSuccess( 0, 0 );
    }

    // Without a ceiling, a long enough series would make a single answer worth more than a whole
    // session, and the experience would stop meaning anything.
    EXPECT_EQ( SessionScore::BASE_XP_PER_SUCCESS + SessionScore::MAXIMUM_STREAK_BONUS,
               score.experienceForSuccess( 0 ) );
}

TEST( SessionScoreTest, a_replay_reduces_the_reward_without_ever_erasing_it )
{
    SessionScore score{ UNLIMITED_LIVES };

    const std::int32_t withManyReplays = score.experienceForSuccess( 50 );

    EXPECT_EQ( SessionScore::MINIMUM_XP_PER_SUCCESS, withManyReplays );

    // A correct answer is never worth nothing: replaying is allowed, and expensive, but it never turns
    // a success into a failure.
    EXPECT_GT( withManyReplays, 0 );
}

TEST( SessionScoreTest, a_wrong_answer_never_takes_experience_away )
{
    SessionScore score{ UNLIMITED_LIVES };

    score.registerSuccess( 0, 0 );
    score.registerSuccess( 0, 0 );

    const std::int32_t earned = score.experience();

    score.registerError();
    score.registerError();

    EXPECT_EQ( earned, score.experience() );
}

TEST( SessionScoreTest, a_wrong_answer_ends_the_series )
{
    SessionScore score{ UNLIMITED_LIVES };

    score.registerSuccess( 0, 0 );
    score.registerSuccess( 0, 0 );

    ASSERT_EQ( 2, score.streak() );

    score.registerError();

    EXPECT_EQ( 0, score.streak() );
}

TEST( SessionScoreTest, a_session_without_lives_never_runs_out )
{
    SessionScore score{ UNLIMITED_LIVES };

    for( std::int32_t index = 0; index < 100; ++index )
    {
        score.registerError();
    }

    // The training mode of the first version: mistakes cost the series, and nothing else.
    EXPECT_FALSE( score.isOutOfLives() );
    EXPECT_FALSE( score.remainingLives().has_value() );
}

TEST( SessionScoreTest, a_session_with_lives_runs_out_after_that_many_mistakes )
{
    SessionScore score{ std::optional<std::int32_t>{ 2 } };

    score.registerError();

    EXPECT_EQ( 1, score.remainingLives().value() );
    EXPECT_FALSE( score.isOutOfLives() );

    score.registerError();

    EXPECT_EQ( 0, score.remainingLives().value() );
    EXPECT_TRUE( score.isOutOfLives() );
}

TEST( SessionScoreTest, a_helped_question_earns_nothing_and_breaks_the_series )
{
    SessionScore score{ std::optional<std::int32_t>{ 3 } };

    score.registerSuccess( 0, 0 );

    const std::int32_t earned = score.experience();

    score.registerHelpedQuestion();

    // Nothing earned: revealing the answer is a lesson, not a recognition. And the series ends,
    // otherwise asking for help would become a way to protect a bonus.
    EXPECT_EQ( earned, score.experience() );
    EXPECT_EQ( 0, score.streak() );

    // And it costs no life: help is not a mistake, and it must never be punished like one.
    EXPECT_EQ( 3, score.remainingLives().value() );
}

TEST( SessionScoreTest, the_star_needs_no_help_and_eighty_percent_first_try )
{
    SessionScore score{ UNLIMITED_LIVES };

    // Eight questions right the first time, two of them after a wrong attempt: exactly on the line.
    for( std::int32_t index = 0; index < 8; ++index )
    {
        score.registerSuccess( 0, 0 );
    }

    for( std::int32_t index = 0; index < 2; ++index )
    {
        score.registerSuccess( 0, 1 );
    }

    EXPECT_TRUE( score.hasEarnedStar() );
}

TEST( SessionScoreTest, a_single_revealed_answer_loses_the_star )
{
    SessionScore score{ UNLIMITED_LIVES };

    for( std::int32_t index = 0; index < 9; ++index )
    {
        score.registerSuccess( 0, 0 );
    }

    ASSERT_TRUE( score.hasEarnedStar() );

    score.registerHelpedQuestion();

    // The star says "I recognised these by ear". One revealed answer is the moment that sentence
    // became false, so one is enough.
    EXPECT_FALSE( score.hasEarnedStar() );
}

TEST( SessionScoreTest, a_question_answered_on_the_second_try_does_not_count_as_first_try )
{
    SessionScore score{ UNLIMITED_LIVES };

    for( std::int32_t index = 0; index < 8; ++index )
    {
        score.registerSuccess( 0, 1 );
    }

    ASSERT_EQ( 8, score.completedQuestionCount() );

    // Nothing was revealed, and yet not one question was recognised: the star is about hearing it
    // right, not about eventually getting there.
    EXPECT_EQ( 0, score.firstTrySuccessCount() );
    EXPECT_FALSE( score.hasEarnedStar() );
}

TEST( SessionScoreTest, the_rank_climbs_with_the_streak )
{
    // Le rang, facon Devil May Cry : il monte avec la serie, et il est la REGLE elle-meme - aucune serie n'a un
    // rang ambigu.
    EXPECT_EQ( Rank::D, rankForStreak( 0 ) );
    EXPECT_EQ( Rank::D, rankForStreak( 1 ) );
    EXPECT_EQ( Rank::C, rankForStreak( 2 ) );
    EXPECT_EQ( Rank::B, rankForStreak( 4 ) );
    EXPECT_EQ( Rank::A, rankForStreak( 6 ) );
    EXPECT_EQ( Rank::S, rankForStreak( 8 ) );
    EXPECT_EQ( Rank::SS, rankForStreak( 10 ) );
    EXPECT_EQ( Rank::SSS, rankForStreak( 13 ) );
    EXPECT_EQ( Rank::SSS, rankForStreak( 100 ) );
}

}    // namespace musichien::domain