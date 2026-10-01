#pragma once

// =====================================================================================================================
// Musichien - SampledInstrument
//
// A real instrument, in a few recorded notes: the sampler chooses the CLOSEST recorded note and transposes it by
// playing it faster or slower. Five notes are enough to cover the whole range, because the transposition never
// goes past three semitones.
//
// ---------------------------------------------------------------------------------------------------------------------
// Why samples at all, when there is already a physical model
//
// A struck string is a string. A piano is a string AND a soundboard, a felt hammer, three strings per note and a
// body - and no amount of tuning a lone string will make it a piano: what it lacks is harmonic richness. The samples
// answer that, and they cost 240 KB per note instead of the 40 to 215 MB of a sound bank.
//
// ---------------------------------------------------------------------------------------------------------------------
// What this class does NOT do
//
// It does not read files, and it knows nothing about Qt. A wave file is turned into a SampledNote by a function
// below, which reads BYTES - arithmetic, and nothing else. That is what keeps the whole sampler testable, and it
// is why the reading lives here rather than in the infrastructure: what it consumes is a format, not a device.
//
// It is also not a NotePlayer: it renders buffers, exactly like ToneSynthesizer, and the adapter above it decides
// when to ask.
// =====================================================================================================================

#include "domain/audio/ToneSynthesizer.h"
#include "domain/music/Note.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace musichien::domain
{

// The instruments the game can play, and the ORDER they are loaded in.
//
// That order is a contract, not a detail: a preference is stored as one flag per instrument, so an instrument
// inserted in the middle would silently exchange the choices the player made. New instruments go LAST.
//
// The last three are NOT samples: they are the pure waveforms the synthesiser renders (see Waveform), offered so
// that a change of temperament can be HEARD for what it is. They are listed here so that the settings screen offers
// them exactly like a recording.
inline constexpr std::size_t INSTRUMENT_COUNT = 11;

inline constexpr std::array<const char *, INSTRUMENT_COUNT> INSTRUMENT_NAMES{
  "piano", "guitare", "saxo", "flute", "cordes", "clarinette", "marimba", "harpe", "sinusoïdal", "dent de scie", "carré" };

// Les instruments d'un PREMIER lancement : le piano et la guitare, et rien d'autre. Les timbres restants sont trop
// etranges pour etre imposes tels quels ; c'est une decision de conception, et elle vit ici comme les autres regles du
// projet : un premier lancement doit sonner JUSTE, sans rien demander au joueur.
//
// Le saxo et les trois formes d'onde restent OFFERTS dans les reglages : ils ne sont simplement pas imposes.
[[nodiscard]] std::vector<bool> defaultEnabledInstruments();

// The waveforms offered as instruments, in the same order as their names at the end of INSTRUMENT_NAMES. The adapter
// maps the corresponding flags onto this list.
inline constexpr std::array<Waveform, 3> WAVEFORM_INSTRUMENTS{ Waveform::Sine, Waveform::Sawtooth, Waveform::Square };

// One recorded note: its samples, the note they were recorded at, and the rate they were recorded at.
struct SampledNote
{
    // The MIDI number the recording IS. Everything else is computed from this one.
    std::int32_t rootMidiNumber{ 0 };

    // Mono samples in [-1, 1].
    std::vector<float> samples;

    std::int32_t sampleRate{ 0 };
};

// Reads a WAV file: PCM, 16 bits, mono or stereo, and turns it into one recorded note.
//
// Returns nothing when the file is not a wave file, is compressed, or uses another sample format - which is the
// honest answer, and leaves the caller free to fall back on the synthesiser. Content is a data file a human can
// edit, and a broken one must cost a timbre, never the application.
[[nodiscard]] std::optional<SampledNote> sampledNoteFromWave( std::span<const std::byte> p_bytes,
                                                              std::int32_t p_rootMidiNumber );

class SampledInstrument
{
public:
    // Recorded notes are kept SORTED, which is what lets the closest one be found by a binary search.
    void addNote( SampledNote p_note );

    [[nodiscard]] bool isEmpty() const noexcept { return m_notes.empty(); }

    [[nodiscard]] std::size_t noteCount() const noexcept { return m_notes.size(); }

    // A note, a chord, a melody: the same three calls ToneSynthesizer offers, so that the adapter above can use
    // either one without knowing which. The tuning's root is the first note (or the note itself, for a single note).
    [[nodiscard]] std::vector<float> renderNote( const Note & p_note,
                                                 std::chrono::milliseconds p_duration,
                                                 std::int32_t p_sampleRate,
                                                 TuningContext p_tuning = {} ) const;

    [[nodiscard]] std::vector<float> renderChord( std::span<const Note> p_notes,
                                                  std::chrono::milliseconds p_duration,
                                                  std::int32_t p_sampleRate,
                                                  TuningContext p_tuning = {} ) const;

    [[nodiscard]] std::vector<float> renderMelody( std::span<const Note> p_notes,
                                                   std::chrono::milliseconds p_noteDuration,
                                                   std::chrono::milliseconds p_gap,
                                                   std::int32_t p_sampleRate,
                                                   TuningContext p_tuning = {} ) const;

    // La MEME chose, pour une phrase dont chaque note a sa duree.
    //
    // Ajoutee le 01/10/2026 pour que les phrases modales soient jouees par un INSTRUMENT : elles etaient synthetisees, et
    // posees sur un bourdon enregistre - deux matieres que l'oreille n'accorde pas.
    [[nodiscard]] std::vector<float> renderMelody( std::span<const Note> p_notes,
                                                   std::span<const std::chrono::milliseconds> p_durations,
                                                   std::chrono::milliseconds p_gap,
                                                   std::int32_t p_sampleRate,
                                                   TuningContext p_tuning = {} ) const;

private:
    // The recorded note closest to the one asked for: see the transposition in renderNote.
    [[nodiscard]] const SampledNote & closestNoteTo( const Note & p_note ) const;

    // One note at an EXPLICIT frequency: the caller computed it from the tuning and the root, and this method only
    // chooses the closest recording and plays it back at the speed that reaches that frequency.
    [[nodiscard]] std::vector<float> renderNoteAt( const Note & p_note,
                                                   double p_frequencyHz,
                                                   std::chrono::milliseconds p_duration,
                                                   std::int32_t p_sampleRate ) const;

    std::vector<SampledNote> m_notes;
};

}    // namespace musichien::domain
