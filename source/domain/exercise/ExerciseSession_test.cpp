#include "domain/exercise/ExerciseSession.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>

namespace musichien::domain
{

// ---------------------------------------------------------------------------------------------------------------------
// The loop
//
// A whole session is played here, in microseconds, with no sound card and no phone. That is the point
// of having put the loop in the domain: everything below - the grid that closes in, the palette that
// grows, the lives, the star - is checked without anyone having to listen to anything.
// ---------------------------------------------------------------------------------------------------------------------

namespace
{

constexpr std::uint32_t TEST_SEED = 20260926;

// The settings of most tests: no limit on mistakes, so that a test about the GRID is not disturbed by
// a test about the lives.
[[nodiscard]] SessionSettings unlimitedLivesSettings()
{
    SessionSettings settings;
    settings.lives = std::nullopt;
    return settings;
}

[[nodiscard]] std::int32_t targetOf( const ExerciseSession & p_session )
{
    return p_session.currentQuestion().target.semitones();
}

void answerCorrectly( ExerciseSession & p_session )
{
    p_session.answer( targetOf( p_session ) );
}

// Chooses one of the offered intervals that is not the right answer, which is what a player does when
// they are wrong.
void answerWrongly( ExerciseSession & p_session )
{
    const Question & question = p_session.currentQuestion();

    const auto wrongChoice = std::ranges::find_if( question.choices,
                                                   [&question]( const Interval & p_choice ) { return !( p_choice == question.target ); } );

    ASSERT_NE( question.choices.end(), wrongChoice ) << "a grid offered nothing but the right answer";

    p_session.answer( wrongChoice->semitones() );
}

// Plays a whole session, always right, and lets the caller see the result.
void playCorrectly( ExerciseSession & p_session, std::size_t p_questionCount )
{
    for( std::size_t index = 0; index < p_questionCount; ++index )
    {
        answerCorrectly( p_session );
        p_session.advance();
    }
}

}    // namespace

TEST( ExerciseSessionTest, a_session_asks_its_first_question_immediately )
{
    const ExerciseSession session{ TEST_SEED };

    // No start() call to forget: a session that exists is asking something.
    EXPECT_EQ( SessionState::Asking, session.state() );
    EXPECT_EQ( 1, session.questionNumber() );
    EXPECT_FALSE( session.isFinished() );
}

TEST( ExerciseSessionTest, the_right_answer_is_always_offered )
{
    ExerciseSession session{ TEST_SEED };

    for( std::size_t index = 0; index < 10; ++index )
    {
        const Question & question = session.currentQuestion();

        EXPECT_NE( question.choices.end(), std::ranges::find( question.choices, question.target ) );

        // And never alone: a question with a single choice is not a question.
        EXPECT_GE( question.choices.size(), 2 );

        answerCorrectly( session );
        session.advance();
    }
}

TEST( ExerciseSessionTest, a_wrong_answer_asks_the_same_question_again )
{
    ExerciseSession session{ TEST_SEED };

    const std::int32_t firstTarget = targetOf( session );
    const std::size_t firstQuestionNumber = session.questionNumber();

    EXPECT_FALSE( session.answer( firstTarget + 1 ) );
    EXPECT_EQ( SessionState::Asking, session.state() );
    EXPECT_EQ( firstTarget, targetOf( session ) );
    EXPECT_EQ( firstQuestionNumber, session.questionNumber() );
}

TEST( ExerciseSessionTest, a_wrong_answer_makes_the_grid_smaller )
{
    SessionSettings settings = unlimitedLivesSettings();
    settings.startingPaletteSize = 5;

    ExerciseSession session{ TEST_SEED, settings };

    const std::size_t firstGridSize = session.currentQuestion().choices.size();

    ASSERT_EQ( 5, firstGridSize );

    answerWrongly( session );

    // "La grille qui s'aide": each mistake removes the least plausible of the wrong answers, so the
    // same question gets easier the longer it is struggled with. The player is helped without having
    // to ask for anything.
    EXPECT_EQ( firstGridSize - 1, session.currentQuestion().choices.size() );

    // And the right answer is still there: help never removes what the player is looking for.
    EXPECT_NE( session.currentQuestion().choices.end(),
               std::ranges::find( session.currentQuestion().choices, session.currentQuestion().target ) );
}

TEST( ExerciseSessionTest, the_grid_never_shrinks_below_two_choices )
{
    SessionSettings settings = unlimitedLivesSettings();
    settings.startingPaletteSize = 5;

    ExerciseSession session{ TEST_SEED, settings };

    for( std::size_t index = 0; index < 10; ++index )
    {
        answerWrongly( session );
    }

    EXPECT_EQ( 2, session.currentQuestion().choices.size() );
}

TEST( ExerciseSessionTest, the_help_is_offered_only_after_three_wrong_answers )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    EXPECT_FALSE( session.isHelpAvailable() );

    answerWrongly( session );
    answerWrongly( session );

    EXPECT_FALSE( session.isHelpAvailable() );

    answerWrongly( session );

    EXPECT_TRUE( session.isHelpAvailable() );

    // And it takes no life to ask: help is not a mistake.
    EXPECT_EQ( 0, session.score().streak() );
}

TEST( ExerciseSessionTest, the_palette_grows_every_three_successes )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    ASSERT_EQ( 2, session.palette().size() );

    answerCorrectly( session );
    session.advance();

    answerCorrectly( session );
    session.advance();

    ASSERT_EQ( 2, session.palette().size() );

    answerCorrectly( session );

    // Three in a row: one more interval to deal with. Note that the question ALREADY on screen keeps
    // the grid it was asked with, which is right: a grid that changed under the finger of the player
    // who has just answered would be a magic trick, not a progression.
    EXPECT_EQ( 3, session.palette().size() );

    session.advance();

    // The next question is the one that offers the new interval, and a fourth button with it.
    EXPECT_EQ( 3, session.currentQuestion().choices.size() );
}

TEST( ExerciseSessionTest, a_helped_question_takes_the_newest_interval_back )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    answerCorrectly( session );
    session.advance();

    answerCorrectly( session );
    session.advance();

    answerCorrectly( session );
    session.advance();

    ASSERT_EQ( 3, session.palette().size() );

    session.revealAnswer();

    // The player asked to be told: they were not ready for the newest interval, so it steps back out.
    // The questions that follow are asked on ground they can stand on.
    EXPECT_EQ( 2, session.palette().size() );
}

TEST( ExerciseSessionTest, the_palette_never_shrinks_below_where_the_player_started )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    ASSERT_EQ( 2, session.settings().startingPaletteSize );

    session.revealAnswer();

    // Someone already at the beginning would be left with nothing to play with.
    EXPECT_EQ( 2, session.palette().size() );
}

TEST( ExerciseSessionTest, a_helped_question_moves_on_to_the_next_question )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    session.revealAnswer();

    EXPECT_EQ( SessionState::Feedback, session.state() );
    EXPECT_FALSE( session.wasLastAnswerCorrect() );

    session.advance();

    EXPECT_EQ( SessionState::Asking, session.state() );
    EXPECT_EQ( 2, session.questionNumber() );
}

TEST( ExerciseSessionTest, ten_questions_end_the_session )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    playCorrectly( session, 10 );

    EXPECT_TRUE( session.isFinished() );
    EXPECT_EQ( SessionState::Finished, session.state() );

    // The last question number, and not eleven: a screen prints "10 / 10" without a special case.
    EXPECT_EQ( 10, session.questionNumber() );
}

TEST( ExerciseSessionTest, five_wrong_answers_end_a_session_that_has_five_lives )
{
    ExerciseSession session{ TEST_SEED };

    ASSERT_EQ( 5, session.settings().lives.value() );

    for( std::int32_t index = 0; index < 4; ++index )
    {
        answerWrongly( session );
    }

    ASSERT_FALSE( session.isFinished() );
    EXPECT_EQ( 1, session.score().remainingLives().value() );

    answerWrongly( session );

    // The session is over the moment the last life goes, without a feedback to read.
    EXPECT_TRUE( session.isFinished() );
}

TEST( ExerciseSessionTest, a_perfect_session_earns_its_star )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    playCorrectly( session, 10 );

    EXPECT_TRUE( session.hasEarnedStar() );
}

TEST( ExerciseSessionTest, the_star_is_not_given_before_the_last_question )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    playCorrectly( session, 5 );

    ASSERT_FALSE( session.isFinished() );

    // Five flawless answers are not a session. A star that appeared in the middle of one would say
    // "you are done" while there is still work to do.
    EXPECT_FALSE( session.hasEarnedStar() );
}

TEST( ExerciseSessionTest, nothing_changes_once_the_session_is_over )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    playCorrectly( session, 10 );

    ASSERT_TRUE( session.isFinished() );

    const std::int32_t earned = session.score().experience();

    EXPECT_FALSE( session.answer( 0 ) );

    session.advance();

    EXPECT_EQ( earned, session.score().experience() );
    EXPECT_EQ( 10, session.questionNumber() );
}

TEST( ExerciseSessionTest, a_replay_is_counted_then_costs_experience )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    session.registerReplay();
    session.registerReplay();

    EXPECT_EQ( 2, session.currentQuestion().replayCount );

    answerCorrectly( session );

    // Replaying is allowed, and not free: the reward is reduced, never erased.
    EXPECT_LT( session.score().experience(), SessionScore::BASE_XP_PER_SUCCESS );
    EXPECT_GE( session.score().experience(), SessionScore::MINIMUM_XP_PER_SUCCESS );
}

TEST( ExerciseSessionTest, a_replay_is_ignored_once_the_answer_is_known )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    answerCorrectly( session );

    ASSERT_FALSE( session.canReplay() );

    session.registerReplay();

    // The feedback replays the interval by itself; counting that one would charge the player for
    // something they did not ask for.
    EXPECT_EQ( 0, session.currentQuestion().replayCount );
}

TEST( ExerciseSessionTest, an_answer_arriving_after_the_question_is_over_scores_nothing )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    answerCorrectly( session );

    const std::int32_t earned = session.score().experience();

    // The same answer, tapped twice: it must not be able to score twice.
    EXPECT_FALSE( session.answer( targetOf( session ) ) );

    EXPECT_EQ( earned, session.score().experience() );
}

TEST( ExerciseSessionTest, the_root_note_moves_from_one_question_to_the_next )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    std::set<std::int32_t> roots;

    for( std::size_t index = 0; index < 10; ++index )
    {
        roots.insert( session.currentQuestion().rootMidiNumber );

        answerCorrectly( session );
        session.advance();
    }

    // A session that always started on the same note would teach the sound of that note as much as the
    // interval, and the player would end up recognising the key rather than the distance.
    EXPECT_GT( roots.size(), 1 );
}

TEST( ExerciseSessionTest, the_upper_note_never_goes_above_the_ceiling )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    const std::int32_t ceiling = session.settings().highestPlayableMidiNumber;

    for( std::size_t index = 0; index < 10; ++index )
    {
        const Question & question = session.currentQuestion();

        EXPECT_LE( question.rootMidiNumber + question.target.semitones(), ceiling );

        answerCorrectly( session );
        session.advance();
    }
}

}    // namespace musichien::domain