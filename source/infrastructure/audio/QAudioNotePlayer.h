#pragma once

// =====================================================================================================================
// Musichien - QAudioNotePlayer
//
// Plays notes through the audio output of the operating system.
//
// This is an ADAPTER: it implements the port declared by the domain, and it knows everything the
// domain must ignore - QAudioSink, sample formats, buffers, devices, failure to open a sound card.
//
// The dependency arrow still points towards the domain:
//
//     infrastructure  ---implements--->  domain::NotePlayer
//
// Swapping QAudioSink for Oboe on Android will therefore only touch this file.
// See docs/ARCHITECTURE.md.
// =====================================================================================================================

#include "domain/audio/NotePlayer.h"
#include "domain/audio/SampledInstrument.h"
#include "domain/audio/ToneSynthesizer.h"

#include <QAudioFormat>
#include <QAudioSink>
#include <QIODevice>

#include <chrono>
#include <memory>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <vector>

namespace musichien::infrastructure
{

class QAudioNotePlayer final : public domain::NotePlayer
{
public:
    QAudioNotePlayer();
    ~QAudioNotePlayer() override;

    QAudioNotePlayer( const QAudioNotePlayer & ) = delete;
    QAudioNotePlayer & operator=( const QAudioNotePlayer & ) = delete;
    QAudioNotePlayer( QAudioNotePlayer && ) = delete;
    QAudioNotePlayer & operator=( QAudioNotePlayer && ) = delete;

    void playNote( const domain::Note & p_note ) override;
    void playMelody( std::span<const domain::Note> p_notes, std::chrono::milliseconds p_gap ) override;
    void playChord( std::span<const domain::Note> p_notes ) override;
    void playMistakeCue() override;

    void stopAll() override;

    // The sampled instruments, when there are any. They become the sound of the EXERCISES - a real piano, a real
    // guitar, a real saxophone - while the synthesiser keeps the mistake cue, which must not be beautiful.
    //
    // The instrument is drawn at RANDOM from the list, and that is a decision rather than a shortcut: an interval
    // heard only on a piano has not been heard, and each instrument has its own harmonics and therefore its own
    // colour. The day the player chooses his instrument explicitly, this becomes a preference - the drawing here
    // is what makes the variety exist in the meantime.
    //
    // Passing an empty list is legal and means "no samples": the synthesiser then plays everything.
    void useInstruments( std::vector<domain::SampledInstrument> p_instruments );

    [[nodiscard]] std::chrono::milliseconds noteDuration() const override;

    // Opens the audio output immediately, instead of waiting for the first note.
    //
    // A machine without a usable sound card must be diagnosed at start up, not the moment the player
    // taps a button in the middle of an exercise.
    void prepareAudioOutput();

    // False when no audio output could be opened.

    //
    // Playback then becomes a silent no-op instead of a crash: a development machine without a sound
    // card, or a CI machine, must still be able to run the application and its tests.
    [[nodiscard]] bool isAudioOutputAvailable() const noexcept;

    // Description of the audio output in use, for the diagnostic log of the application.
    [[nodiscard]] std::string audioOutputDescription() const;

private:
    // Opens the audio output on first use, and decides the sample format once and for all.
    void ensureAudioOutputIsOpen();

    // Replaces whatever is playing by a new buffer of samples.
    void playSamples( std::vector<float> p_samples );

    std::unique_ptr<QAudioSink> m_audioSink;

    // Owned by the sink: it must not be deleted here.
    QIODevice * m_audioOutputDevice{ nullptr };

    // Created once the real sample rate of the device is known, because the synthesizer must generate
    // samples at the very rate the device consumes them. Otherwise every note would be out of tune.
    // The instrument the next listening will use. Drawn at random from m_instruments, but STABLE as long as the
    // question does not change: being played back on a different instrument would turn "listen again" into a
    // different question, and the verdict into a trap.
    [[nodiscard]] const domain::SampledInstrument & instrumentFor( std::span<const domain::Note> p_notes );

    // Empty when there are no samples, in which case every buffer comes from the synthesiser.
    std::vector<domain::SampledInstrument> m_instruments;

    std::size_t m_instrumentIndex{ 0 };

    // What was played last, which is how "the same question" is recognised.
    std::vector<domain::Note> m_lastPlayedNotes;

    std::mt19937 m_instrumentRandomEngine{ std::random_device{}() };

    std::optional<domain::ToneSynthesizer> m_synthesizer;

    QAudioFormat m_audioFormat;
    std::vector<float> m_currentSamples;
    std::string m_outputDescription{ "not opened yet" };
};

}    // namespace musichien::infrastructure
