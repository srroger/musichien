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
#include "domain/audio/ToneSynthesizer.h"

#include <QAudioFormat>
#include <QAudioSink>
#include <QIODevice>

#include <chrono>
#include <memory>
#include <optional>
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
    void stopAll() override;

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
    std::optional<domain::ToneSynthesizer> m_synthesizer;

    QAudioFormat m_audioFormat;
    std::vector<float> m_currentSamples;
    std::string m_outputDescription{ "not opened yet" };
};

}    // namespace musichien::infrastructure
