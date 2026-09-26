#pragma once

// =====================================================================================================================
// Musichien - ExerciseSessionController
//
// The view model of the exercise screen, and the ONLY bridge between the loop and the interface.
//
// Thin by construction, exactly like IntervalPlaybackController:
//   * it never decides what a question is: it asks the session;
//   * it never decides what an answer is worth: it asks the score;
//   * it never names an interval: it asks the domain;
//   * it never invents a sound: it asks the NotePlayer port.
//
// What it DOES own is the two things the domain refuses to do, on purpose:
//
//   * the RANDOMNESS - a session is seeded here, because the domain owns no entropy source of its own,
//     which is what keeps a session reproducible in a test;
//   * the PLAYING of notes, because making a sound is not a rule of the game.
//
// See docs/ARCHITECTURE.md and docs/CODE_CONVENTIONS.md.
// =====================================================================================================================

#include "domain/audio/NotePlayer.h"
#include "domain/exercise/ExerciseSession.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include <cstdint>
#include <memory>

namespace musichien::ui
{

class ExerciseSessionController final : public QObject
{
    Q_OBJECT

    // True while a session exists: the screen shows it instead of the bench.
    Q_PROPERTY( bool running READ running NOTIFY runningChanged )

    Q_PROPERTY( int questionNumber READ questionNumber NOTIFY sessionChanged )
    Q_PROPERTY( int questionCount READ questionCount NOTIFY sessionChanged )

    // What the player can choose, described for the interface. Rebuilt whenever the question changes,
    // and only then: a grid that moved during the feedback would be unreadable.
    Q_PROPERTY( QVariantList choices READ choices NOTIFY questionChanged )

    Q_PROPERTY( bool isAsking READ isAsking NOTIFY sessionChanged )
    Q_PROPERTY( bool isFinished READ isFinished NOTIFY sessionChanged )
    Q_PROPERTY( bool isFeedbackVisible READ isFeedbackVisible NOTIFY sessionChanged )
    Q_PROPERTY( bool wasLastAnswerCorrect READ wasLastAnswerCorrect NOTIFY sessionChanged )
    Q_PROPERTY( bool isHelpAvailable READ isHelpAvailable NOTIFY sessionChanged )

    // What the domain says about the interval that was asked, and about the one the player chose. The
    // screen reads names and identifiers, it composes neither.
    Q_PROPERTY( QVariantMap heardInterval READ heardInterval NOTIFY sessionChanged )
    Q_PROPERTY( QVariantMap answeredInterval READ answeredInterval NOTIFY sessionChanged )

    Q_PROPERTY( int experience READ experience NOTIFY scoreChanged )
    Q_PROPERTY( int streak READ streak NOTIFY scoreChanged )
    Q_PROPERTY( int lives READ lives NOTIFY scoreChanged )
    Q_PROPERTY( bool hasUnlimitedLives READ hasUnlimitedLives NOTIFY scoreChanged )

    // Only meaningful once the session is over.
    Q_PROPERTY( bool starEarned READ starEarned NOTIFY sessionChanged )

public:
    // The settings of the session to come, provided by the caller rather than written here: they are
    // data of the game, they will come from the profile of the player, and a test needs to be able to
    // pin them down - a session whose direction is drawn at random cannot be asserted precisely.
    explicit ExerciseSessionController( domain::NotePlayer & p_notePlayer,
                                        domain::SessionSettings p_settings = {},
                                        QObject * p_parent = nullptr );

    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] int questionNumber() const noexcept;
    [[nodiscard]] int questionCount() const noexcept;
    [[nodiscard]] QVariantList choices() const;
    [[nodiscard]] bool isAsking() const noexcept;
    [[nodiscard]] bool isFinished() const noexcept;
    [[nodiscard]] bool isFeedbackVisible() const noexcept;
    [[nodiscard]] bool wasLastAnswerCorrect() const noexcept;
    [[nodiscard]] bool isHelpAvailable() const noexcept;
    [[nodiscard]] QVariantMap heardInterval() const;
    [[nodiscard]] QVariantMap answeredInterval() const;
    [[nodiscard]] int experience() const noexcept;
    [[nodiscard]] int streak() const noexcept;
    [[nodiscard]] int lives() const noexcept;
    [[nodiscard]] bool hasUnlimitedLives() const noexcept;
    [[nodiscard]] bool starEarned() const noexcept;

    // Starts a new session and plays its first question. Called by the "Jouer" button.
    Q_INVOKABLE void startSession();

    // Leaves the loop and goes back to the bench. Stops the sound first: a stream left open on a phone
    // is a battery drain.
    Q_INVOKABLE void stopSession();

    // The player asks to hear the interval again. Counted by the session, played here.
    Q_INVOKABLE void replay();

    // The player picks an interval, given as a distance in semitones: the screen never sends a name.
    Q_INVOKABLE void answer( int p_semitones );

    // The player asks for the answer, after the session said it may be revealed.
    Q_INVOKABLE void revealAnswer();

    // Leaves the feedback and asks the next question. Called by the screen, which owns the pause: the
    // domain has no clock, and giving it one would make every rule above untestable.
    Q_INVOKABLE void continueToNextQuestion();

    // Stops every sound. Called when the screen is left.
    Q_INVOKABLE void stopPlayback();

signals:
    void runningChanged();
    void questionChanged();
    void sessionChanged();
    void scoreChanged();

    // A wrong answer has just been given, and the question is still being asked.
    //
    // The screen answers it with its body - a shake today, a vibration tomorrow - and the controller
    // has no opinion about that: it knows WHAT happened, not how it should feel. Note that this is a
    // wrong ATTEMPT, not the end of a question: a player who is told the answer has not made a mistake.
    void wrongAnswerGiven();

private:
    // Rebuilds the list of choices from the question being asked, and only then notifies. Called
    // whenever the question changes AND whenever the grid closes in after a mistake.
    void refreshChoices();

    // Plays the interval of the question being asked, from its own root note.
    void playCurrentQuestion();

    // Plays the same two notes TOGETHER, whatever direction the question was asked in.
    void playCurrentQuestionAsChord();

    domain::NotePlayer & m_notePlayer;

    // Kept so that every session this controller starts uses the same rules.
    domain::SessionSettings m_settings;

    // Empty until a session starts: the bench is what the application shows before that.
    std::unique_ptr<domain::ExerciseSession> m_session;

    QVariantList m_choices;
};

}    // namespace musichien::ui
