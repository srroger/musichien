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

    // Lives of the session. Empty means no limit: it is what the training mode of the first version
    // will use. The first playable loop keeps it finite, because a rule nobody can feel is a rule
    // nobody can judge.
    std::optional<std::int32_t> lives{ 5 };

    // Range the lowest note of a question is drawn from.
    //
    // The root moves from one question to the next ON PURPOSE. A session where every interval starts
    // on the same note teaches the sound of that note as much as the interval, and a player would then
    // recognise the key rather than the distance.
    std::int32_t lowestRootMidiNumber{ 55 };

    std::int32_t highestRootMidiNumber{ 67 };

    // The highest note the application will play: the root is chosen so that the upper note stays
    // below it. C6 leaves the whole range comfortable on a phone speaker.
    std::int32_t highestPlayableMidiNumber{ 84 };

    // Silence left between the two notes of a question, as heard.
    //
    // A musical value rather than a technical one: too short and the two notes sound like one glide,
    // too long and the first note is forgotten before the second one arrives. It lives here, with the
    // other rules, rather than in the screen, so that it can be tuned without touching the interface.
    std::chrono::milliseconds melodicGap{ 300 };
};

// A question, as the screen needs it.
struct Question
{
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

    // An interval of the palette, drawn evenly.
    [[nodiscard]] Interval drawTarget();

    // A root note that leaves room for the interval above it.
    [[nodiscard]] std::int32_t drawRootMidiNumber( const Interval & p_target );

    // Gives a new interval to the player, or takes the newest one back when they struggle.
    void widenPalette();
    void narrowPalette();

    std::mt19937 m_randomEngine;
    SessionSettings m_settings;
    std::vector<Interval> m_palette;
    SessionScore m_score;

    Question m_currentQuestion;
    SessionState m_state{ SessionState::Asking };
    std::size_t m_questionNumber{ 1 };
    bool m_lastAnswerWasCorrect{ false };
    std::optional<Interval> m_lastAnswer;
};

}    // namespace musichien::domain
