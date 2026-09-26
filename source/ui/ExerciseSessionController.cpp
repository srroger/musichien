#include "ui/ExerciseSessionController.h"

#include "domain/music/Interval.h"
#include "ui/IntervalDescription.h"

#include <QString>

#include <array>
#include <optional>
#include <random>
#include <utility>

namespace musichien::ui
{

namespace
{

// What lives() returns when the session puts no limit on mistakes.
//
// A sentinel rather than zero, because zero lives IS a state - it means the session is over - and the
// screen has to be able to tell the two apart.
constexpr int NO_LIFE_LIMIT = -1;

}    // namespace

ExerciseSessionController::ExerciseSessionController( domain::NotePlayer & p_notePlayer, QObject * p_parent )
  : QObject{ p_parent }
  , m_notePlayer{ p_notePlayer }
{
}

bool ExerciseSessionController::running() const noexcept
{
    return m_session != nullptr;
}

int ExerciseSessionController::questionNumber() const noexcept
{
    return ( m_session != nullptr ) ? static_cast<int>( m_session->questionNumber() ) : 0;
}

int ExerciseSessionController::questionCount() const noexcept
{
    return ( m_session != nullptr ) ? static_cast<int>( m_session->settings().questionCount ) : 0;
}

QVariantList ExerciseSessionController::choices() const
{
    return m_choices;
}

bool ExerciseSessionController::isAsking() const noexcept
{
    return ( m_session != nullptr ) && ( m_session->state() == domain::SessionState::Asking );
}

bool ExerciseSessionController::isFinished() const noexcept
{
    return ( m_session != nullptr ) && m_session->isFinished();
}

bool ExerciseSessionController::isFeedbackVisible() const noexcept
{
    return ( m_session != nullptr ) && ( m_session->state() == domain::SessionState::Feedback );
}

bool ExerciseSessionController::wasLastAnswerCorrect() const noexcept
{
    return ( m_session != nullptr ) && m_session->wasLastAnswerCorrect();
}

bool ExerciseSessionController::isHelpAvailable() const noexcept
{
    return ( m_session != nullptr ) && m_session->isHelpAvailable();
}

QVariantMap ExerciseSessionController::heardInterval() const
{
    if( m_session == nullptr )
    {
        return QVariantMap{};
    }

    return describeInterval( m_session->currentQuestion().target );
}

QVariantMap ExerciseSessionController::answeredInterval() const
{
    if( m_session == nullptr )
    {
        return QVariantMap{};
    }

    const std::optional<domain::Interval> answer = m_session->lastAnswer();

    // operator* rather than value(): an empty optional is what "nothing to show" means here, and it is
    // already handled.
    return answer.has_value() ? describeInterval( *answer ) : QVariantMap{};
}

int ExerciseSessionController::experience() const noexcept
{
    return ( m_session != nullptr ) ? static_cast<int>( m_session->score().experience() ) : 0;
}

int ExerciseSessionController::streak() const noexcept
{
    return ( m_session != nullptr ) ? static_cast<int>( m_session->score().streak() ) : 0;
}

int ExerciseSessionController::lives() const noexcept
{
    if( m_session == nullptr )
    {
        return NO_LIFE_LIMIT;
    }

    // operator* and not value(): value() throws when the optional is empty, and a getter of a view
    // model has no business being able to throw.
    const std::optional<std::int32_t> remainingLives = m_session->score().remainingLives();

    return remainingLives.has_value() ? static_cast<int>( *remainingLives ) : NO_LIFE_LIMIT;
}

bool ExerciseSessionController::hasUnlimitedLives() const noexcept
{
    return ( m_session == nullptr ) || !m_session->score().remainingLives().has_value();
}

bool ExerciseSessionController::starEarned() const noexcept
{
    return ( m_session != nullptr ) && m_session->hasEarnedStar();
}

void ExerciseSessionController::startSession()
{
    // The seed is drawn HERE, in the interface layer, and never inside the domain.
    //
    // That is what keeps a session reproducible from its seed in a test, and it is also why the rules
    // of the game can be replayed exactly when something goes wrong. The domain owns no entropy
    // source, on purpose.
    std::random_device entropySource;

    m_session = std::make_unique<domain::ExerciseSession>( entropySource() );

    emit runningChanged();

    refreshChoices();

    playCurrentQuestion();

    emit scoreChanged();
    emit sessionChanged();
}

void ExerciseSessionController::stopSession()
{
    stopPlayback();

    m_session.reset();

    m_choices.clear();

    emit runningChanged();
    emit questionChanged();
    emit sessionChanged();
    emit scoreChanged();
}

void ExerciseSessionController::replay()
{
    if( ( m_session == nullptr ) || !m_session->canReplay() )
    {
        return;
    }

    // Counted by the session, played here: the count has to follow what was really heard, and only
    // the screen knows that.
    m_session->registerReplay();

    playCurrentQuestion();

    emit scoreChanged();
    emit sessionChanged();
}

void ExerciseSessionController::answer( int p_semitones )
{
    if( ( m_session == nullptr ) || !isAsking() )
    {
        // An answer arriving twice, or after the question is over, changes nothing. The session says
        // so itself; stopping here keeps the signals from firing for nothing.
        return;
    }

    m_session->answer( p_semitones );

    // Always, and not only when the question is over: a wrong answer closes the grid in, and the
    // screen must show the grid that exists rather than the one it had a moment ago.
    refreshChoices();

    // When the question is over - right or wrong or revealed - the interval is heard AGAIN. The
    // verdict is something to listen to, not only something to read: that single replay is what turns
    // a mistake into a lesson.
    if( m_session->state() != domain::SessionState::Asking )
    {
        playCurrentQuestion();
    }

    emit scoreChanged();
    emit sessionChanged();
}

void ExerciseSessionController::revealAnswer()
{
    if( ( m_session == nullptr ) || !isAsking() )
    {
        return;
    }

    m_session->revealAnswer();

    refreshChoices();

    playCurrentQuestion();

    emit scoreChanged();
    emit sessionChanged();
}

void ExerciseSessionController::continueToNextQuestion()
{
    if( ( m_session == nullptr ) || ( m_session->state() != domain::SessionState::Feedback ) )
    {
        return;
    }

    m_session->advance();

    refreshChoices();

    if( !m_session->isFinished() )
    {
        playCurrentQuestion();
    }

    emit scoreChanged();
    emit sessionChanged();
}

void ExerciseSessionController::stopPlayback()
{
    m_notePlayer.stopAll();
}

void ExerciseSessionController::refreshChoices()
{
    if( m_session == nullptr )
    {
        return;
    }

    QVariantList choices;

    for( const domain::Interval & choice : m_session->currentQuestion().choices )
    {
        choices.append( describeInterval( choice ) );
    }

    m_choices = std::move( choices );

    emit questionChanged();
}

void ExerciseSessionController::playCurrentQuestion()
{
    if( m_session == nullptr )
    {
        return;
    }

    const domain::Question & question = m_session->currentQuestion();

    const domain::Note rootNote{ question.rootMidiNumber };
    const domain::Note upperNote = rootNote.transposedBy( question.target.semitones() );

    if( question.direction == domain::IntervalDirection::Harmonic )
    {
        const std::array<domain::Note, 2> notes{ rootNote, upperNote };

        m_notePlayer.playChord( notes );

        return;
    }

    if( question.direction == domain::IntervalDirection::Descending )
    {
        // A falling interval is the SAME interval: only the order of the two notes changes. Playing
        // them in the wrong order would change what is heard, and the interval the player is asked to
        // name would no longer be the one the domain named.
        const std::array<domain::Note, 2> descendingNotes{ upperNote, rootNote };

        m_notePlayer.playMelody( descendingNotes, m_session->settings().melodicGap );

        return;
    }

    const std::array<domain::Note, 2> ascendingNotes{ rootNote, upperNote };

    m_notePlayer.playMelody( ascendingNotes, m_session->settings().melodicGap );
}

}    // namespace musichien::ui