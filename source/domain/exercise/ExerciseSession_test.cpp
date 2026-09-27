#include "domain/exercise/ExerciseSession.h"

#include "domain/exercise/LearningOrder.h"

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

TEST( ExerciseSessionTest, both_notes_stay_inside_the_playable_range )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    const SessionSettings & settings = session.settings();

    for( std::size_t index = 0; index < 10; ++index )
    {
        const Question & question = session.currentQuestion();

        const std::int32_t firstNote = question.rootMidiNumber;

        // The second note depends on the DIRECTION, and getting this wrong in the code is silent: the
        // notes would simply be played at the edge of what a phone speaker can do, which sounds like a
        // slightly odd question rather than like a bug.
        const std::int32_t secondNote =
          ( question.direction == IntervalDirection::Descending )
            ? ( firstNote - question.target.semitones() )
            : ( firstNote + question.target.semitones() );

        EXPECT_GE( std::min( firstNote, secondNote ), settings.lowestPlayableMidiNumber );
        EXPECT_LE( std::max( firstNote, secondNote ), settings.highestPlayableMidiNumber );

        answerCorrectly( session );
        session.advance();
    }
}

TEST( ExerciseSessionTest, the_three_directions_all_come_up_in_a_session )
{
    // A long session, so that the draw has every chance to show all three. One interval heard only
    // upwards is half an interval: descending is the same distance heard the other way and a separate
    // skill, and the harmonic form leaves only the colour.
    SessionSettings settings = unlimitedLivesSettings();
    settings.questionCount = 200;

    ExerciseSession session{ TEST_SEED, settings };

    std::size_t ascendingCount = 0;
    std::size_t descendingCount = 0;
    std::size_t harmonicCount = 0;

    for( std::size_t index = 0; index < settings.questionCount; ++index )
    {
        switch( session.currentQuestion().direction )
        {
            case IntervalDirection::Ascending:
                ++ascendingCount;
                break;

            case IntervalDirection::Descending:
                ++descendingCount;
                break;

            case IntervalDirection::Harmonic:
                ++harmonicCount;
                break;
        }

        answerCorrectly( session );
        session.advance();
    }

    EXPECT_GT( ascendingCount, 0 );
    EXPECT_GT( descendingCount, 0 );
    EXPECT_GT( harmonicCount, 0 );
}

// ---------------------------------------------------------------------------------------------------------------------
// The memory hint
//
// The hint is a NUDGE where the "Réponse" button is a rescue, and the whole design is in the timing: too
// early it is the answer in disguise, too late it arrives after the player has given up. The two
// thresholds are therefore asserted against each other, not one at a time.
// ---------------------------------------------------------------------------------------------------------------------

TEST( ExerciseSessionTest, the_hint_waits_for_a_mistake_and_leaves_with_the_question )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    // Nothing before the player has tried.
    EXPECT_FALSE( session.isHintAvailable() );

    answerWrongly( session );

    EXPECT_TRUE( session.isHintAvailable() );

    // The next question starts clean: a hint belongs to a question, not to a session.
    answerCorrectly( session );
    session.advance();

    EXPECT_FALSE( session.isHintAvailable() );
}

TEST( ExerciseSessionTest, the_hint_is_offered_before_the_answer_is )
{
    SessionSettings settings = unlimitedLivesSettings();

    // Enough intervals on the grid that three wrong answers in a row are possible at all: the grid closes
    // in on every mistake, which is the subject of its own test.
    settings.startingPaletteSize = 5;

    ExerciseSession session{ TEST_SEED, settings };

    answerWrongly( session );

    // At the first mistake: the nudge.
    EXPECT_TRUE( session.isHintAvailable() );
    EXPECT_FALSE( session.isHelpAvailable() );

    answerWrongly( session );
    answerWrongly( session );

    // At the third: the rescue.
    EXPECT_TRUE( session.isHelpAvailable() );
}

TEST( ExerciseSessionTest, a_session_without_aids_gives_none_however_hard_the_player_tries )
{
    SessionSettings settings = unlimitedLivesSettings();
    settings.aidsAllowed = false;

    ExerciseSession session{ TEST_SEED, settings };

    answerWrongly( session );
    answerWrongly( session );
    answerWrongly( session );
    answerWrongly( session );

    // Un mode sans filet est un mode sans filet : l'indice ne souffle pas, et la reponse ne se donne pas -
    // meme apres quatre essais, et meme si les seuils de settings disent le contraire. C'est le DOMAINE qui
    // decide, donc aucun ecran ne peut oublier de verifier.
    EXPECT_FALSE( session.isHintAvailable() );
    EXPECT_FALSE( session.isHelpAvailable() );
}

TEST( ExerciseSessionTest, the_whole_palette_stays_whole_however_many_mistakes_are_made )
{
    SessionSettings settings = unlimitedLivesSettings();
    settings.startingPaletteSize = learningOrderIntervals().size();

    ExerciseSession session{ TEST_SEED, settings };

    const std::size_t wholePalette = session.palette().size();

    ASSERT_EQ( learningOrderIntervals().size(), wholePalette );

    // Une erreur retire d'ordinaire le dernier intervalle arrive dans la palette. Dans un mode ou la carte est
    // complete des la premiere question, il n'y a pas de "dernier arrive" : la carte doit rester entiere.
    //
    // C'est narrowPalette qui s'en charge, en ne descendant jamais sous la taille de depart - et c'est
    // exactement le genre de regle qui casse en silence le jour ou quelqu'un la "simplifie".
    answerWrongly( session );
    answerWrongly( session );

    EXPECT_EQ( wholePalette, session.palette().size() );
}

TEST( ExerciseSessionTest, a_guided_question_asks_the_direction_and_takes_a_direction )
{
    SessionSettings settings = unlimitedLivesSettings();

    settings.directionQuestionShare = 100;

    ExerciseSession session{ TEST_SEED, settings };

    // Le mode guide ne peut pas demander un intervalle harmonique : "monte ou descend ?" n'a pas de sens sur un
    // accord.
    EXPECT_EQ( QuestionKind::Direction, session.currentQuestion().kind );
    EXPECT_NE( IntervalDirection::Harmonic, session.currentQuestion().direction );

    // La bonne direction est une bonne reponse.
    EXPECT_TRUE( session.answerDirection( session.currentQuestion().direction ) );

    // Et une direction sur une question qui demandait un nom est un autre langage : elle ne compte pas.
    ExerciseSession namedSession{ TEST_SEED, unlimitedLivesSettings() };

    EXPECT_EQ( QuestionKind::NamedInterval, namedSession.currentQuestion().kind );
    EXPECT_FALSE( namedSession.answerDirection( IntervalDirection::Ascending ) );
}

TEST( ExerciseSessionTest, two_mistakes_turn_the_next_question_guided )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    // Deux erreurs sur la meme question...
    answerWrongly( session );
    answerWrongly( session );

    // ...une bonne reponse pour en sortir...
    answerCorrectly( session );
    session.advance();

    // ...et la question suivante est guidee, sans qu'aucun reglage ne l'ait demande.
    EXPECT_EQ( QuestionKind::Direction, session.currentQuestion().kind );
}

}    // namespace musichien::domain