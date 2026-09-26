#include "ui/ExerciseSessionController.h"

#include "domain/audio/NotePlayerFake.h"

#include <QVariantList>
#include <QVariantMap>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

namespace musichien::ui
{

// ---------------------------------------------------------------------------------------------------------------------
// The view model of the exercise screen
//
// It runs with the fake note player, so nothing here needs a sound card or a phone. What is checked is
// what this class is responsible for: what the screen would HEAR, and what it would DISPLAY.
//
// The seed of a session is drawn from the entropy of the machine, so a test can never know WHICH
// interval will be asked. It does not need to: every property below holds for any interval, and asking
// the session what was heard is exactly what the screen does.
// ---------------------------------------------------------------------------------------------------------------------

namespace
{

constexpr std::size_t SESSION_QUESTION_COUNT = 10;

// The interval the session just asked, as the screen reads it.
[[nodiscard]] std::int32_t heardDistance( const ExerciseSessionController & p_controller )
{
    return p_controller.heardInterval().value( "semitones" ).toInt();
}

void answerCorrectly( ExerciseSessionController & p_controller )
{
    p_controller.answer( heardDistance( p_controller ) );
}

}    // namespace

TEST( ExerciseSessionControllerTest, starting_a_session_asks_the_first_question )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer };

    EXPECT_FALSE( controller.running() );

    controller.startSession();

    EXPECT_TRUE( controller.running() );
    EXPECT_TRUE( controller.isAsking() );
    EXPECT_FALSE( controller.isFinished() );
    EXPECT_EQ( 1, controller.questionNumber() );
    EXPECT_EQ( SESSION_QUESTION_COUNT, controller.questionCount() );

    // The question is HEARD, not only displayed: two notes, one after the other.
    ASSERT_EQ( 1, notePlayer.playedMelodies().size() );
    EXPECT_EQ( 2, notePlayer.playedMelodies().front().notes.size() );
}

TEST( ExerciseSessionControllerTest, every_choice_is_ready_to_display )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer };

    controller.startSession();

    const QVariantList choices = controller.choices();

    // A question with a single choice is not a question, and the right answer is always one of them.
    ASSERT_GE( choices.size(), 2 );

    const QVariantMap firstChoice = choices.first().toMap();

    // The screen reads names and identifiers and composes neither, so both have to be filled.
    EXPECT_FALSE( firstChoice.value( "identifier" ).toString().isEmpty() );
    EXPECT_FALSE( firstChoice.value( "name" ).toString().isEmpty() );
    EXPECT_TRUE( firstChoice.contains( "intervalClass" ) );
}

TEST( ExerciseSessionControllerTest, the_right_answer_scores_and_the_verdict_is_heard )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer };

    controller.startSession();
    answerCorrectly( controller );

    EXPECT_TRUE( controller.wasLastAnswerCorrect() );
    EXPECT_TRUE( controller.isFeedbackVisible() );
    EXPECT_FALSE( controller.isAsking() );
    EXPECT_EQ( domain::SessionScore::BASE_XP_PER_SUCCESS, controller.experience() );

    // The verdict is played AGAIN: a screen that only says "wrong" teaches nothing, and the same holds
    // for a screen that only says "right".
    EXPECT_EQ( 2, notePlayer.playedMelodies().size() );
}

TEST( ExerciseSessionControllerTest, a_wrong_answer_costs_a_life_and_asks_again )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer };

    controller.startSession();

    controller.answer( heardDistance( controller ) + 1 );

    EXPECT_FALSE( controller.wasLastAnswerCorrect() );
    EXPECT_TRUE( controller.isAsking() );
    EXPECT_FALSE( controller.isFeedbackVisible() );

    // Nothing earned, and one life gone: a mistake costs an attempt, never experience.
    EXPECT_EQ( 0, controller.experience() );
    EXPECT_EQ( 4, controller.lives() );

    // The wrong answer is remembered, so that the verdict can be read against it.
    EXPECT_EQ( heardDistance( controller ) + 1,
               controller.answeredInterval().value( "semitones" ).toInt() );
}

TEST( ExerciseSessionControllerTest, the_answer_is_offered_after_three_wrong_ones )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer };

    controller.startSession();

    const std::int32_t wrongDistance = heardDistance( controller ) + 1;

    controller.answer( wrongDistance );
    controller.answer( wrongDistance );

    EXPECT_FALSE( controller.isHelpAvailable() );

    controller.answer( wrongDistance );

    EXPECT_TRUE( controller.isHelpAvailable() );

    controller.revealAnswer();

    EXPECT_TRUE( controller.isFeedbackVisible() );

    // Help teaches, and it pays nothing: making it profitable would turn it into a way to farm.
    EXPECT_EQ( 0, controller.experience() );
}

TEST( ExerciseSessionControllerTest, a_perfect_session_earns_its_star )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer };

    controller.startSession();

    for( std::size_t question = 0; question < SESSION_QUESTION_COUNT; ++question )
    {
        answerCorrectly( controller );
        controller.continueToNextQuestion();
    }

    EXPECT_TRUE( controller.isFinished() );

    // The question number stops at the last question rather than going one past it.
    EXPECT_EQ( SESSION_QUESTION_COUNT, controller.questionNumber() );

    EXPECT_TRUE( controller.starEarned() );
}

TEST( ExerciseSessionControllerTest, replaying_counts_and_costs_experience )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer };

    controller.startSession();

    controller.replay();

    EXPECT_EQ( 2, notePlayer.playedMelodies().size() );

    answerCorrectly( controller );

    // Listening again is allowed, and not free: a correct answer after a replay is worth less than one
    // heard right away.
    EXPECT_LT( controller.experience(), domain::SessionScore::BASE_XP_PER_SUCCESS );
}

TEST( ExerciseSessionControllerTest, leaving_the_loop_releases_the_sound_and_the_session )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer };

    controller.startSession();

    controller.stopSession();

    EXPECT_FALSE( controller.running() );
    EXPECT_TRUE( controller.choices().isEmpty() );
    EXPECT_GE( notePlayer.stopCount(), 1 );

    // And the bench is back: nothing of the session is left behind.
    EXPECT_FALSE( controller.isFinished() );
    EXPECT_EQ( 0, controller.experience() );
}

}    // namespace musichien::ui