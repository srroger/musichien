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

    // The sustained chord: the same notes, held for the given duration, so the beating between them can be counted.
    void playChordFor( std::span<const domain::Note> p_notes, std::chrono::milliseconds p_duration ) override;
    void playMistakeCue() override;

    // Le clic de menu : un accuse de reception, pas une reponse.
    void playTapCue() override;

    // Le clic du metronome : accentue sur le premier temps d'une mesure.
    void playMetronomeClick( bool p_accented ) override;

    // Frappe un element de la batterie, rendu par la synthese.
    void playDrum( domain::Drum p_drum ) override;

    // Le petit arpège de l'accueil : montant, ouvert, au piano, et VOLONTAIREMENT discret.
    //
    // Roger, après l'avoir entendu : "les sons d'introduction sont un peu forts... ça fait un peu bug, un peu
    // dur à l'oreille". Il avait raison sur les deux points : un accord de trois notes au niveau des exercices
    // arrive comme une porte qui claque, et l'application n'a rien à dire d'aussi fort au moment où elle
    // s'ouvre.
    //
    // Ce n'est pas un son du PORT, et c'est une décision : c'est une signature de l'application, pas quelque
    // chose que le domaine a à connaître. Le PROCHAIN son que le jeu joue est un exercice.
    void playGreeting();

    void stopAll() override;

    // The tuning every following note is heard in. The root is always the FIRST note of what is played, which is
    // exactly what the domain's frequencyFor expects: an interval is heard FROM its first note.
    void setTuning( domain::TuningContext p_tuning ) override;

    // The sampled instruments, when there are any. They become the sound of the EXERCISES - a real piano, a real
    // guitar, a real saxophone - while the synthesiser keeps the mistake cue, which must not be beautiful.
    //
    // The instrument is drawn at RANDOM from the list, and that is a decision rather than a shortcut: an interval
    // heard only on a piano has not been heard, and each instrument has its own harmonics and therefore its own
    // colour. The day the player chooses his instrument explicitly, this becomes a preference - the drawing here
    // is what makes the variety exist in the meantime.
    //
    // Passing an empty list is legal and means "no samples". p_waveforms lists the PURE WAVEFORMS (sine, sawtooth,
    // square) that join the drawing as additional timbres, so an exercise can be heard with a controlled spectrum.
    // With no samples and no waveform, the synthesiser (the struck string) plays everything.
    void useInstruments( std::vector<domain::SampledInstrument> p_instruments,
                         std::vector<domain::Waveform> p_waveforms );

    [[nodiscard]] std::chrono::milliseconds noteDuration() const override;

    // Opens the audio output immediately, instead of waiting for the first note.
    //
    // A machine without a usable sound card must be diagnosed at start up, not the moment the player
    // taps a button in the middle of an exercise.
    void prepareAudioOutput();

    // Reopens the audio output against whatever device is now default. Called when the operating system says the
    // devices changed - a bluetooth headset plugged or removed - so that the sound follows the player instead of
    // staying on a dead device.
    void reopenAudioOutput();

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

    // The timbre the next listening will use. Drawn at random among the samples AND the sine (when it is enabled),
    // but STABLE as long as the question does not change: being played back on a different instrument would turn
    // "listen again" into a different question, and the verdict into a trap. Returns an index into m_instruments, or
    // m_instruments.size() for the sine.
    [[nodiscard]] std::size_t timbreIndexFor( std::span<const domain::Note> p_notes );

    // One note, rendered with the timbre drawn for the given sequence. Falls back on the struck-string synthesiser
    // when there is neither a sample nor the sine.
    [[nodiscard]] std::vector<float> renderNoteFor( std::span<const domain::Note> p_sequence,
                                                    const domain::Note & p_note,
                                                    std::chrono::milliseconds p_duration );

    [[nodiscard]] std::vector<float> renderChordFor( std::span<const domain::Note> p_notes,
                                                     std::chrono::milliseconds p_duration );

    [[nodiscard]] std::vector<float> renderMelodyFor( std::span<const domain::Note> p_notes,
                                                      std::chrono::milliseconds p_noteDuration,
                                                      std::chrono::milliseconds p_gap );

    // The sampled instruments, empty when there are none.
    std::vector<domain::SampledInstrument> m_instruments;

    // The pure waveforms that join the drawing, empty when there are none.
    std::vector<domain::Waveform> m_waveforms;

    // The drawn timbre: an index into m_instruments, or m_instruments.size() + a waveform index.
    std::size_t m_instrumentIndex{ 0 };

    // What was played last, which is how "the same question" is recognised.
    std::vector<domain::Note> m_lastPlayedNotes;

    std::mt19937 m_instrumentRandomEngine{ std::random_device{}() };

    std::optional<domain::ToneSynthesizer> m_synthesizer;

    // Created from the same real sample rate, so the drums are in tune with the notes.
    std::optional<domain::DrumSynthesizer> m_drumSynthesizer;

    // Equal temperament at 440 Hz until setTuning says otherwise.
    domain::TuningContext m_tuning;

    QAudioFormat m_audioFormat;
    std::vector<float> m_currentSamples;
    std::string m_outputDescription{ "not opened yet" };
};

}    // namespace musichien::infrastructure
