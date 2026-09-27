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
#include "domain/audio/SampledInstrument.h"
#include "domain/exercise/AnswerGrid.h"
#include "domain/exercise/ExerciseSession.h"
#include "domain/exercise/HintBook.h"
#include "domain/exercise/PlayerPreferences.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <cstdint>
#include <functional>
#include <memory>

namespace musichien::ui
{

// Called when a wrong answer should be FELT, not only seen.
//
// An empty function means "this device cannot vibrate", which is the honest description of a development
// machine. Injecting it rather than calling a platform API from here is what keeps this view model
// testable, and what keeps the knowledge of Android out of the interface layer.
using VibrationCallback = std::function<void()>;

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

    // Les memes choix, mais PLACES sur le cercle des quintes : douze cases fixes, dont les vides. C'est ce que
    // l'ecran affiche, et c'est ce qui donne une geographie stable a la grille - une carte, pas une liste.
    Q_PROPERTY( QVariantList gridPositions READ gridPositions NOTIFY questionChanged )

    Q_PROPERTY( bool isAsking READ isAsking NOTIFY sessionChanged )
    Q_PROPERTY( bool isFinished READ isFinished NOTIFY sessionChanged )
    Q_PROPERTY( bool isFeedbackVisible READ isFeedbackVisible NOTIFY sessionChanged )
    Q_PROPERTY( bool wasLastAnswerCorrect READ wasLastAnswerCorrect NOTIFY sessionChanged )
    Q_PROPERTY( bool isHelpAvailable READ isHelpAvailable NOTIFY sessionChanged )

    // Ce que la question en cours demande : 0 pour nommer un intervalle, 1 pour dire dans quel sens il a ete joue.
    // C'est ce que l'ecran lit pour savoir s'il montre le cercle ou les deux boutons monte/descend.
    Q_PROPERTY( int questionKind READ questionKind NOTIFY questionChanged )

    // What the domain says about the interval that was asked, and about the one the player chose. The
    // screen reads names and identifiers, it composes neither.
    Q_PROPERTY( QVariantMap heardInterval READ heardInterval NOTIFY sessionChanged )
    Q_PROPERTY( QVariantMap answeredInterval READ answeredInterval NOTIFY sessionChanged )

    // A snatch of music to remember the interval by, once the player has made a mistake.
    //
    // EMPTY means "nothing to show", for any of three reasons that the screen does not need to tell
    // apart: no mistake yet, the answer already known, or a gap in the content file.
    Q_PROPERTY( QString hintText READ hintText NOTIFY sessionChanged )

    // ---------------------------------------------------------------------------------------------------------------
    // The player
    //
    // Asked ONCE, and remembered. Not a setting: the first piece of the profile. See PlayerLevel.
    // ---------------------------------------------------------------------------------------------------------------
    Q_PROPERTY( bool hasChosenLevel READ hasChosenLevel NOTIFY playerLevelChanged )

    Q_PROPERTY( int playerLevel READ playerLevel NOTIFY playerLevelChanged )

    // The levels to offer, each ready to display: an index and a name. Built here rather than written in the
    // QML, for the same reason the answer grid is: a list that exists twice drifts.
    //
    // Static because it reads nothing of this object: the list of levels is a fact of the domain, and saying
    // so in the signature is cheaper than a comment. Qt hands it to the screen all the same.
    Q_PROPERTY( QVariantList playerLevels READ playerLevels CONSTANT )

    // The instruments the player wants to hear, each with its index, its name and whether it is enabled.
    //
    // See PlayerPreferences: a saxophone at the same level as a piano is aggressive, and a timbre that grates
    // gets an application closed. What the player turns off stays off.
    Q_PROPERTY( QVariantList instruments READ instruments NOTIFY instrumentsChanged )

    Q_PROPERTY( int experience READ experience NOTIFY scoreChanged )
    Q_PROPERTY( int streak READ streak NOTIFY scoreChanged )
    Q_PROPERTY( int lives READ lives NOTIFY scoreChanged )
    Q_PROPERTY( bool hasUnlimitedLives READ hasUnlimitedLives NOTIFY scoreChanged )

    // Only meaningful once the session is over.
    Q_PROPERTY( bool starEarned READ starEarned NOTIFY sessionChanged )

public:
    // Ce que la question en cours demande : nommer un intervalle, ou dire dans quel sens il a ete joue. La valeur
    // est celle du domaine, transposee en entier pour le QML.
    int questionKind() const noexcept;
    // The settings of the session to come, provided by the caller rather than written here: they are
    // data of the game, they will come from the profile of the player, and a test needs to be able to
    // pin them down - a session whose direction is drawn at random cannot be asserted precisely.
    //
    // The hint book arrives the same way, from the content file the application read at start up, and is
    // held BY VALUE: a reference would be a reference to an object whose lifetime this class does not
    // control, which is the shortest path to a crash nobody can reproduce.
    explicit ExerciseSessionController( domain::NotePlayer & p_notePlayer,
                                        domain::SessionSettings p_settings = {},
                                        domain::HintBook p_hintBook = {},
                                        VibrationCallback p_vibrate = {},
                                        domain::PlayerPreferences * p_levelStore = nullptr,
                                        QObject * p_parent = nullptr );

    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] int questionNumber() const noexcept;
    [[nodiscard]] int questionCount() const noexcept;
    [[nodiscard]] QVariantList choices() const;
    [[nodiscard]] QVariantList gridPositions() const;
    [[nodiscard]] bool isAsking() const noexcept;
    [[nodiscard]] bool isFinished() const noexcept;
    [[nodiscard]] bool isFeedbackVisible() const noexcept;
    [[nodiscard]] bool wasLastAnswerCorrect() const noexcept;
    [[nodiscard]] bool isHelpAvailable() const noexcept;
    [[nodiscard]] QVariantMap heardInterval() const;
    [[nodiscard]] QVariantMap answeredInterval() const;
    [[nodiscard]] QString hintText() const;
    [[nodiscard]] bool hasChosenLevel() const noexcept;
    [[nodiscard]] int playerLevel() const noexcept;
    [[nodiscard]] static QVariantList playerLevels();
    [[nodiscard]] QVariantList instruments() const;

    // The flags as the rest of the application needs them: main.cpp filters the loaded instruments with this,
    // which is what keeps the audio adapter from having to know anything about preferences.
    [[nodiscard]] std::vector<bool> enabledInstruments() const { return m_enabledInstruments; }
    [[nodiscard]] int experience() const noexcept;
    [[nodiscard]] int streak() const noexcept;
    [[nodiscard]] int lives() const noexcept;
    [[nodiscard]] bool hasUnlimitedLives() const noexcept;
    [[nodiscard]] bool starEarned() const noexcept;

    // Starts a new session and plays its first question. Called by the "Jouer" button.
    Q_INVOKABLE void startSession();

    // The player says where he is, once. His answer is remembered, and it decides where his sessions start.
    Q_INVOKABLE void choosePlayerLevel( int p_level );

    // Turns one instrument on or off. The last enabled one cannot be turned off: an instrument list with nothing
    // in it is a game with no sound.
    Q_INVOKABLE void setInstrumentEnabled( int p_index, bool p_isEnabled );

    // Leaves the loop and goes back to the bench. Stops the sound first: a stream left open on a phone
    // is a battery drain.
    Q_INVOKABLE void stopSession();

    // The player asks to hear the interval again. Counted by the session, played here.
    Q_INVOKABLE void replay();

    // The player picks an interval, given as a distance in semitones: the screen never sends a name.
    Q_INVOKABLE void answer( int p_semitones );

    // The player says which way the interval went, on a guided question: 0 up, 1 down.
    Q_INVOKABLE void answerDirection( int p_direction );

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

    // The player has just said where he is, or the application has just remembered it.
    void playerLevelChanged();

    // The player has just turned an instrument on or off.
    void instrumentsChanged();

private:
    // Rebuilds the list of choices from the question being asked, and only then notifies. Called
    // whenever the question changes AND whenever the grid closes in after a mistake.
    void refreshChoices();

    // What follows a right or a wrong answer, whatever its form: replay the question one way or the other, shake
    // on a mistake, and tell the screen. Both answer() and answerDirection() end here.
    void processAnswer( bool p_isCorrect );

    // Plays the interval of the question being asked, from its own root note.
    void playCurrentQuestion();

    // Plays the same two notes TOGETHER, whatever direction the question was asked in.
    void playCurrentQuestionAsChord();

    domain::NotePlayer & m_notePlayer;

    // Kept so that every session this controller starts uses the same rules.
    domain::SessionSettings m_settings;

    // The memory hooks, owned here rather than referenced: see the constructor.
    domain::HintBook m_hintBook;

    // Empty when the device cannot vibrate.
    VibrationCallback m_vibrate;

    // May be null: a test, or an application that has nowhere to remember anything, must still run.
    domain::PlayerPreferences * m_levelStore{ nullptr };

    // Read once from the store, then kept here: the screen asks for it on every question, and a settings file
    // has no business being read that often.
    std::optional<domain::PlayerLevel> m_playerLevel;

    // One flag per instrument, in the order of domain::INSTRUMENT_NAMES. Empty means "everything", which is
    // what a first run has and what the screen must show as all enabled.
    std::vector<bool> m_enabledInstruments;

    // Empty until a session starts: the bench is what the application shows before that.
    std::unique_ptr<domain::ExerciseSession> m_session;

    QVariantList m_choices;
};

}    // namespace musichien::ui
