#include "domain/exercise/ExerciseSession.h"

#include "domain/exercise/AnswerGrid.h"
#include "domain/exercise/LearningOrder.h"

#include <algorithm>
#include <span>

namespace musichien::domain
{

ExerciseSession::ExerciseSession( std::uint32_t p_seed, SessionSettings p_settings )
  : m_randomEngine{ p_seed }
  , m_settings{ p_settings }
  , m_palette{ beginnerPalette( m_settings.startingPaletteSize ) }
  , m_score{ m_settings.lives }
  , m_currentQuestion{ buildQuestion() }
{
    // The first question is built HERE rather than on a start() call: a session that exists is a
    // session that is asking something, and the screen therefore never has to handle a state where
    // there is nothing to play.
}

Question ExerciseSession::buildQuestion()
{
    Question question;

    question.target = drawTarget();

    // The direction is drawn BEFORE the root, because the root depends on it: the room an interval
    // needs is on one side or the other.
    question.direction = drawDirection();

    question.rootMidiNumber = drawRootMidiNumber( question.target, question.direction );

    // The grid is built from the PALETTE and the target, and it is clamped to the palette by the
    // AnswerGrid itself: a question can never offer an interval the player has not met.
    question.choices = AnswerGrid::build( m_palette, question.target, m_settings.choiceCount, m_randomEngine );

    return question;
}

IntervalDirection ExerciseSession::drawDirection()
{
    // The shares are read as WEIGHTS rather than as percentages: the draw is made inside their total,
    // whatever that total is. Changing them to 5 / 3 / 2 therefore changes nothing but the proportions,
    // which is what anyone editing them would expect.
    const std::int32_t total =
      m_settings.ascendingShare + m_settings.descendingShare + m_settings.harmonicShare;

    if( total <= 0 )
    {
        // Every share at zero is a configuration mistake. Ascending is what the application teaches
        // first, so it is the safe answer rather than a question that could not be sounded at all.
        return IntervalDirection::Ascending;
    }

    std::uniform_int_distribution<std::int32_t> distribution{ 1, total };

    const std::int32_t draw = distribution( m_randomEngine );

    if( draw <= m_settings.ascendingShare )
    {
        return IntervalDirection::Ascending;
    }

    if( draw <= ( m_settings.ascendingShare + m_settings.descendingShare ) )
    {
        return IntervalDirection::Descending;
    }

    return IntervalDirection::Harmonic;
}

Interval ExerciseSession::drawTarget()
{
    // Every interval of the palette has the same chance, including the newest one. Weighting the draw
    // towards what the player struggles with would be a better exercise and a worse game: it would
    // make the session feel like it is picking on them, and the adaptive palette already does the work
    // of keeping the questions at the right level.
    std::uniform_int_distribution<std::size_t> distribution{ 0, m_palette.size() - 1 };

    return m_palette.at( distribution( m_randomEngine ) );
}

std::int32_t ExerciseSession::drawRootMidiNumber( const Interval & p_target,
                                                  IntervalDirection p_direction )
{
    const std::int32_t intervalSize = p_target.semitones();

    std::int32_t lowestRoot = m_settings.lowestRootMidiNumber;
    std::int32_t highestRoot = m_settings.highestRootMidiNumber;

    // The root is the note the interval is played FROM, so what it needs depends on the direction:
    //
    //   * going up, or sounding the two notes at once, it is the UPPER note that must stay under the
    //     ceiling;
    //   * going down, it is the room BELOW the root that matters.
    //
    // Getting this wrong is silent: the notes would simply be played outside the comfortable range of a
    // phone speaker, which sounds like a slightly odd question rather than like a bug.
    if( ( p_direction == IntervalDirection::Ascending ) || ( p_direction == IntervalDirection::Harmonic ) )
    {
        highestRoot = std::min( highestRoot, m_settings.highestPlayableMidiNumber - intervalSize );
    }

    if( p_direction == IntervalDirection::Descending )
    {
        lowestRoot = std::max( lowestRoot, m_settings.lowestPlayableMidiNumber + intervalSize );
    }

    if( highestRoot < lowestRoot )
    {
        // The settings have been changed to something that cannot hold this interval. Returning the
        // middle of the window keeps the question playable, which is better than a note outside the
        // range, an empty draw, and a question nobody could explain.
        return std::clamp( m_settings.lowestRootMidiNumber,
                           m_settings.lowestPlayableMidiNumber,
                           m_settings.highestPlayableMidiNumber );
    }

    std::uniform_int_distribution<std::int32_t> distribution{ lowestRoot, highestRoot };

    return distribution( m_randomEngine );
}

void ExerciseSession::registerReplay() noexcept
{
    if( !canReplay() )
    {
        // Once the answer is known there is nothing to replay: the feedback plays the interval again
        // by itself, and counting it would cost the player experience they did not spend.
        return;
    }

    ++m_currentQuestion.replayCount;
}

bool ExerciseSession::answer( std::int32_t p_semitones )
{
    if( m_state != SessionState::Asking )
    {
        // An answer arriving after the question is over changes nothing, and returns false. A double
        // tap must not be able to score twice, nor to lose a second life.
        return false;
    }

    const bool isCorrect = ( p_semitones == m_currentQuestion.target.semitones() );

    m_lastAnswer = intervalFromSemitones( p_semitones );
    m_lastAnswerWasCorrect = isCorrect;

    if( isCorrect )
    {
        m_score.registerSuccess( m_currentQuestion.replayCount, m_currentQuestion.wrongAttemptCount );

        // A new interval joins the palette every so many successes in a row. The modulo, rather than a
        // simple comparison, is what makes this happen at every step: without it the condition would
        // stay true for ever after the third success, and the palette would grow on every single
        // correct answer.
        const auto wideningPeriod = static_cast<std::int32_t>( m_settings.successesBeforeWidening );

        if( ( wideningPeriod > 0 ) && ( m_score.streak() % wideningPeriod == 0 ) )
        {
            widenPalette();
        }

        m_state = SessionState::Feedback;

        return true;
    }

    ++m_currentQuestion.wrongAttemptCount;

    m_score.registerError();

    if( m_score.isOutOfLives() )
    {
        // The session is over the moment the last life goes, without a feedback to read: there is
        // nothing left to answer, and pretending otherwise would only delay the summary.
        m_state = SessionState::Finished;

        return false;
    }

    // The grid closes in. Each wrong answer removes the least plausible of the remaining wrong
    // answers, so that the same question gets easier the longer it is struggled with: help the player
    // never has to ask for. See note 16 of the vault, "la grille qui s'aide".
    if( m_currentQuestion.choices.size() > AnswerGrid::MINIMUM_CHOICE_COUNT )
    {
        m_currentQuestion.choices =
          AnswerGrid::build( m_palette, m_currentQuestion.target, m_currentQuestion.choices.size() - 1, m_randomEngine );
    }

    return false;
}

void ExerciseSession::revealAnswer()
{
    if( m_state != SessionState::Asking )
    {
        return;
    }

    m_score.registerHelpedQuestion();

    m_lastAnswerWasCorrect = false;

    // The player asked to be told: they were not ready. The newest interval leaves the palette, so
    // that the questions that follow are asked on ground they can stand on. Help costs nothing, but it
    // does say something, and this is the only place where saying it does not feel like a punishment.
    narrowPalette();

    m_state = SessionState::Feedback;
}

void ExerciseSession::advance()
{
    if( m_state != SessionState::Feedback )
    {
        return;
    }

    const bool everyQuestionWasAsked = ( m_score.completedQuestionCount() >= m_settings.questionCount );

    if( everyQuestionWasAsked || m_score.isOutOfLives() )
    {
        m_state = SessionState::Finished;

        return;
    }

    ++m_questionNumber;

    m_currentQuestion = buildQuestion();

    m_lastAnswer.reset();
    m_lastAnswerWasCorrect = false;

    m_state = SessionState::Asking;
}

bool ExerciseSession::isHelpAvailable() const noexcept
{
    return ( m_state == SessionState::Asking )
           && ( m_currentQuestion.wrongAttemptCount >= m_settings.wrongAttemptsBeforeHelp );
}

bool ExerciseSession::hasEarnedStar() const noexcept
{
    // The session has to have been played to the end. A star earned by running out of lives would
    // reward exactly what the star is meant to discourage.
    const bool everyQuestionWasAsked = ( m_score.completedQuestionCount() >= m_settings.questionCount );

    return everyQuestionWasAsked && m_score.hasEarnedStar();
}

void ExerciseSession::widenPalette()
{
    if( m_palette.size() >= learningOrderIntervals().size() )
    {
        // Everything the application knows is already in play.
        return;
    }

    // Always the NEXT interval of the order, never a drawn one: taking the palette as a prefix of the
    // order is what makes the progression predictable, and a new interval chosen at random would take
    // the player from a second to a fourteenth with no reason they could feel.
    m_palette = beginnerPalette( m_palette.size() + 1 );
}

void ExerciseSession::narrowPalette()
{
    if( m_palette.size() <= m_settings.startingPaletteSize )
    {
        // Never below where the player started: someone already at the beginning would be left with
        // nothing to play with, and the session would have no question to ask.
        return;
    }

    m_palette = beginnerPalette( m_palette.size() - 1 );
}

}    // namespace musichien::domain
