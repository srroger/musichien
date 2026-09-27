#pragma once

// =====================================================================================================================
// Musichien - ExerciseSession
//
// The loop itself: ask a question, let the player answer, say what was heard, move on.
//
// It owns the palette of the player, the score, and the current question, and it decides everything a
// session decides. It does NOT decide three things, on purpose:
//
//   * it never produces a sound: playing belongs to the NotePlayer port, and the session asks for
//     nothing;
//   * it never waits: the domain has no clock, so the pause between the feedback and the next question
//     belongs to the screen, which calls advance() when it is ready;
//   * it never decides how anything looks.
//
// Consequence: a whole session can be played in a unit test, in microseconds, with no sound card, no
// phone and no waiting. That is the only reason the rules below can be changed with any confidence.
//
// See docs/ARCHITECTURE.md, and note 16 of the vault for the design behind the rules.
// =====================================================================================================================

#include "domain/exercise/SessionScore.h"
#include "domain/music/Interval.h"
#include "domain/music/Note.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <span>
#include <vector>

namespace musichien::domain
{

// Everything that can be tuned in a session, in one place.
//
// These are RULES OF THE GAME, not constants of the code: they will end up in a data file, because
// tuning how hard an exercise is must never require a recompilation. Written here for now, with the
// values of the first playable loop.
struct SessionSettings
{
    // How many questions a session asks before it is over.
    std::size_t questionCount{ 10 };

    // How many intervals the player starts with, out of the learning order.
    std::size_t startingPaletteSize{ 2 };

    // The largest grid offered. The grid is min(palette, this value): it grows with the palette until
    // it reaches this size, and stops there. Beyond it the palette keeps growing, so new intervals
    // appear inside a grid that stays readable.
    std::size_t choiceCount{ 6 };

    // Consecutive correct answers needed before a new interval joins the palette.
    std::size_t successesBeforeWidening{ 3 };

    // Wrong answers on the same question before the answer can be revealed.
    std::int32_t wrongAttemptsBeforeHelp{ 3 };

    // Wrong answers on the same question before the MEMORY HINT appears.
    //
    // ONE, and that is the point: a hint is not a last resort, it is what turns a mistake into a
    // connection. Waiting for three mistakes before helping would mean waiting for a player to be
    // discouraged, and the hint is a nudge where the "Réponse" button is a rescue.
    std::int32_t wrongAttemptsBeforeHint{ 1 };

    // Whether the two aids are offered at all: the hint that nudges on the first mistake, and the button
    // that gives the answer away after three.
    //
    // ONE flag for both, because "no aid" is one decision and not two: a mode where the player measures
    // himself against the whole palette cannot afford either. Cutting only one of them would leave the
    // other to give away the same answer.
    //
    // It is a RULE and not a setting of the screen, which is why it lives here with the others: the day
    // the rules move to a data file, "Master offers no help" is a line of that file, not a line of QML.
    bool aidsAllowed{ true };

    // Lives of the session. Empty means no limit: it is what the training mode of the first version
    // will use. The first playable loop keeps it finite, because a rule nobody can feel is a rule
    // nobody can judge.
    std::optional<std::int32_t> lives{ 5 };

    // Range both notes of a question have to stay inside, whatever the direction.
    //
    // Two octaves wide, and wider than the window below on purpose: it is the window that decides how
    // much variety the player hears, and this range is only the safety net around it.
    std::int32_t lowestPlayableMidiNumber{ 40 };

    std::int32_t highestPlayableMidiNumber{ 84 };

    // The window the note a question STARTS ON is drawn from.
    //
    // Two octaves, and the range above narrows it further depending on the size of the interval and its
    // direction. The first version used a single octave, which meant two questions in five started on
    // the same note - and Roger heard it, and said so.
    std::int32_t lowestRootMidiNumber{ 50 };

    std::int32_t highestRootMidiNumber{ 74 };

    // How often each direction is drawn, out of their total.
    //
    // ALL THREE, and this is the point: an interval heard only upwards is half an interval.
    //
    //   * descending is the SAME distance heard the other way, and it is a separate skill: the ear that
    //     recognises a rising fifth does not automatically recognise a falling one;
    //   * the harmonic form drops the melody altogether and leaves only the colour, which is the
    //     hardest of the three - hence the smallest share.
    //
    // The three are also what the statistics will have to separate: "I recognise fifths" means nothing
    // if it does not say in which direction.
    std::int32_t ascendingShare{ 50 };
    std::int32_t descendingShare{ 30 };
    std::int32_t harmonicShare{ 20 };

    // Share of questions, in percent, that ask the DIRECTION instead of the interval: "does it go up or down?".
    //
    // ZERO by default, so that the original game is untouched unless it is asked for. This is the guided mode of
    // the next step, and it is a RULE like the rest - a share the settings can set to fifty, never a flag the
    // screen flips.
    std::int32_t directionQuestionShare{ 0 };

    // Share of questions, in percent, that ask the player to SING the interval instead of naming it.
    //
    // Twenty by default: enough that the voice shows up regularly in a session, small enough that the listening
    // core stays the main game. Like the direction share, it is a RULE - the mixed session is one setting, not a
    // separate screen.
    std::int32_t singQuestionShare{ 20 };

    // Silence left between the two notes of a question, as heard.
    //
    // A musical value rather than a technical one: too short and the two notes sound like one glide,
    // too long and the first note is forgotten before the second one arrives. It lives here, with the
    // other rules, rather than in the screen, so that it can be tuned without touching the interface.
    std::chrono::milliseconds melodicGap{ 300 };
};

// What a question asks the player.
//
// The original game asks for the NAME of an interval. The guided mode asks for the DIRECTION: the interval is
// played, and the player only has to say whether it went up or down - a smaller question, but the one a beginner
// answers first.
enum class QuestionKind
{
    NamedInterval,
    Direction,
    Sing
};

// A question, as the screen needs it.
struct Question
{
    // What the question asks. The screen reads it to know whether to show the circle or the two directions.
    QuestionKind kind{ QuestionKind::NamedInterval };

    // The note the interval is played from.
    std::int32_t rootMidiNumber{ 60 };

    // The interval being asked. Everything the feedback says is derived from this, never from what the
    // player answered.
    Interval target;

    // What is offered. The right answer is one of them, always, and it is never the only one.
    std::vector<Interval> choices;

    // How the two notes are sounded. Only ascending is generated for now; the field is here because
    // the guided mode of the next step works on the direction, and a question that cannot express one
    // would have to be rewritten.
    IntervalDirection direction{ IntervalDirection::Ascending };

    // Times the player asked to hear the interval again. The first listening is not one of them: it is
    // how the question is asked.
    std::int32_t replayCount{ 0 };

    // Wrong answers given on this question so far.
    std::int32_t wrongAttemptCount{ 0 };
};

enum class SessionState
{
    Asking,      // the player is choosing
    Feedback,    // the answer is known, and is being shown
    Finished     // no more questions
};

class ExerciseSession
{
public:
    // The seed is provided, never drawn here: the domain owns no entropy source, so the same seed
    // always produces the same session, which is what makes every rule above testable.
    explicit ExerciseSession( std::uint32_t p_seed, SessionSettings p_settings = {} );

    [[nodiscard]] const Question & currentQuestion() const noexcept { return m_currentQuestion; }
    [[nodiscard]] SessionState state() const noexcept { return m_state; }
    [[nodiscard]] const SessionScore & score() const noexcept { return m_score; }
    [[nodiscard]] const SessionSettings & settings() const noexcept { return m_settings; }

    // Intervals the player is currently up against, from the learning order.
    [[nodiscard]] std::span<const Interval> palette() const noexcept { return m_palette; }

    // Number of the question being asked, starting at one. Clamped to the last question once the
    // session is over, so that a screen can print "10 / 10" without a special case.
    [[nodiscard]] std::size_t questionNumber() const noexcept { return m_questionNumber; }

    [[nodiscard]] bool isFinished() const noexcept { return m_state == SessionState::Finished; }

    // True once the answer may be revealed: the player has tried enough.
    [[nodiscard]] bool isHelpAvailable() const noexcept;

    // True once the memory hint may be shown: the player has tried, and a snatch of music might unblock
    // them.
    //
    // Distinct from isHelpAvailable, and the difference is the whole design: a hint NUDGES ("remember
    // Star Wars?"), help GIVES UP ("it was a fifth"). The first should arrive early, the second late.
    [[nodiscard]] bool isHintAvailable() const noexcept;

    // True while the player is still allowed to hear the interval again.
    [[nodiscard]] bool canReplay() const noexcept { return m_state == SessionState::Asking; }

    [[nodiscard]] bool wasLastAnswerCorrect() const noexcept { return m_lastAnswerWasCorrect; }

    // What the player answered last, when there is something to show.
    [[nodiscard]] std::optional<Interval> lastAnswer() const noexcept { return m_lastAnswer; }

    // The player asked to hear the interval again. Counted, and nothing else: playing it is the job of
    // the adapter, which calls this so that the count matches what was really heard.
    void registerReplay() noexcept;

    // The player chose an interval, given as a distance in semitones. Returns whether it was right.
    // A wrong answer does not end the question: it is asked again, which is how one gets to try.
    bool answer( std::int32_t p_semitones );

    // The player says which way the interval went, on a guided question. Returns whether it was right.
    //
    // Refused on a question that asked for a name: the two answers are different languages, and accepting a
    // direction where an interval was expected would let a lucky tap score by accident.
    bool answerDirection( IntervalDirection p_direction );

    // The player SANG the interval, on a sung question. Returns whether it was right. Nothing was picked from a
    // grid - the voice is the answer, so there is no interval to record as "chosen".
    bool answerSung( bool p_isCorrect );

    // The player gave up on this question and asked to see the answer. Worth nothing, and it costs
    // nothing: help is not a mistake.
    void revealAnswer();

    // Leaves the feedback and starts the next question, or finishes the session. Called by the screen,
    // which owns the pause.
    void advance();

    // The star of the session: only a session that was played to the end, with no revealed answer, and
    // mostly right on the first try.
    [[nodiscard]] bool hasEarnedStar() const noexcept;

private:
    // Builds the next question from the palette, the settings and the engine.
    [[nodiscard]] Question buildQuestion();

    // Whether the next question asks for a name or a direction, drawn from the settings.
    [[nodiscard]] QuestionKind drawKind();

    // What a right or a wrong answer produces, whatever its form: the score moves, the palette widens or the grid
    // closes in, and the question passes to feedback or the session ends.
    bool resolveAnswer( bool p_isCorrect, std::optional<Interval> p_answer );

    // An interval of the palette, drawn evenly.
    [[nodiscard]] Interval drawTarget();

    // How the question is sounded: one way among ascending, descending and both at once.
    [[nodiscard]] IntervalDirection drawDirection();

    // A root note that leaves room for the interval, in the direction the question will be played.
    [[nodiscard]] std::int32_t drawRootMidiNumber( const Interval & p_target,
                                                   IntervalDirection p_direction );

    // Gives a new interval to the player, or takes the newest one back when they struggle.
    void widenPalette();
    void narrowPalette();

    std::mt19937 m_randomEngine;
    SessionSettings m_settings;
    std::vector<Interval> m_palette;
    SessionScore m_score;

    // Wrong answers in a row. Two of them, and the next question becomes a guided one - a smaller question the
    // player can still answer, which is help that does not announce itself.
    //
    // Declared BEFORE m_currentQuestion, and that order is not decorative: buildQuestion() reads it while it
    // initialises m_currentQuestion, so it must already exist.
    std::size_t m_consecutiveErrors{ 0 };

    Question m_currentQuestion;
    SessionState m_state{ SessionState::Asking };
    std::size_t m_questionNumber{ 1 };
    bool m_lastAnswerWasCorrect{ false };
    std::optional<Interval> m_lastAnswer;
};

}    // namespace musichien::domain
