#pragma once

// =====================================================================================================================
// Musichien - NotePlayer
//
// The domain expresses WHAT it needs: make a note audible. It has no idea whether the sound comes
// from QAudioSink, from Oboe on Android, or from a loudspeaker driven by a microcontroller.
//
// This is the dependency inversion principle in practice: the interface lives in the domain, the
// implementation lives in the infrastructure. Consequence: every piece of logic that uses a
// NotePlayer is testable with NotePlayerFake, on a machine with no sound card at all.
//
// See docs/ARCHITECTURE.md.
// =====================================================================================================================

#include "domain/music/Note.h"
#include "domain/music/Temperament.h"

#include <chrono>
#include <span>

namespace musichien::domain
{

class NotePlayer
{
public:
    virtual ~NotePlayer() = default;

    NotePlayer() = default;
    NotePlayer( const NotePlayer & ) = delete;
    NotePlayer & operator=( const NotePlayer & ) = delete;
    NotePlayer( NotePlayer && ) = delete;
    NotePlayer & operator=( NotePlayer && ) = delete;

    // Plays a single note, for its natural duration.
    virtual void playNote( const Note & p_note ) = 0;

    // Plays the notes one after another, the way a melodic interval is heard.
    // p_gap is the silence left between two notes: without a gap, two notes sound like a glide.
    virtual void playMelody( std::span<const Note> p_notes, std::chrono::milliseconds p_gap ) = 0;

    // Plays the notes at the same time, the way a harmonic interval or a chord is heard.
    virtual void playChord( std::span<const Note> p_notes ) = 0;

    // Plays the short cue that marks a mistake.
    //
    // A cue is NOT an interval, and the domain says so here rather than leaving the distinction to the
    // adapter: it has no pitch, it is not something to be recognised, and it must never be mistaken for
    // one of the sounds being taught. What it sounds like is the adapter's business - that a mistake is
    // AUDIBLE is the domain's.
    virtual void playMistakeCue() = 0;

    // Un clic de menu : un son très court, très doux, qui ne dit rien d'autre que "ta main a été entendue".
    //
    // Il a un CORPS PAR DÉFAUT, et c'est délibéré : un appareil sans retour sonore n'a rien à implémenter, et un
    // test qui ne s'intéresse pas au clic n'a rien à écrire non plus. Le feedback d'un bouton ne mérite pas
    // d'obliger tous les adaptateurs du projet à répondre.
    virtual void playTapCue() {}

    // The tuning every following note is heard in, until it is called again. Equal temperament and a 440 Hz diapason
    // are the defaults, which is exactly what a NotePlayer that never receives this call keeps doing.
    //
    // A default body, like playTapCue: a test that only counts the notes played has no tuning to care about, and
    // must not be forced to write one.
    virtual void setTuning( TuningContext p_tuning ) { (void)p_tuning; }

    // Stops everything immediately. Called when the screen is left or the application goes to the
    // background: an audio stream left open on a phone is a battery drain and a bug.
    virtual void stopAll() = 0;

    // Duration used by playNote and by the chord rendering.
    [[nodiscard]] virtual std::chrono::milliseconds noteDuration() const = 0;
};

}    // namespace musichien::domain
