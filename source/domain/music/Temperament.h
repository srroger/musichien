#pragma once

// =====================================================================================================================
// Musichien - Temperament
//
// How the twelve notes of an octave are TUNED. Equal temperament is what this application uses, and what it MUST use
// by default: every semitone has the same size, an interval always keeps the same ratio whatever the key, and the ear
// learns the intervals the music around it is actually built on. It is also the only temperament that needs no root.
//
// The others are the older ones, and they are not a museum piece. They do not divide the octave evenly: every note is
// reached by a chain of PURE fifths (Pythagorean, ratio 3/2) or by small whole-number ratios (just intonation). The
// intervals are then genuinely in tune - a fifth with almost no beating, a third that locks in - at the price of
// depending on a ROOT note, and of a few howling intervals far from it.
//
// This is what a singer does without thinking: he does not sing a tempered fifth, he sings a just one, adjusting it
// to the chord underneath. Hearing the difference is part of ear training, and the harmonica is a fine example - its
// tuning is deliberately NOT equal, so that its chords ring.
//
// Le DERNIER paragraphe est le vocabulaire, et il compte : le diapason est la hauteur absolue du la de reference
// (440 Hz par la norme ISO 16, 442 Hz dans les orchestres, 415 Hz pour le baroque), et le temperament est la facon de
// REPARTIR les douze notes dans l'octave. Ce sont deux choix independants, et la page Accordeur les separe.
//
// Consequence for the code: a frequency is no longer a property of a note, it is a property of a note HEARD FROM a
// root. Every call site that used Note::frequencyHz() alone becomes a call to frequencyFor().
// =====================================================================================================================

#include "domain/music/Note.h"

#include <array>
#include <string_view>

namespace musichien::domain
{

enum class Temperament
{
    Equal,          // the modern one: twelve equal semitones, no root, no surprise
    Pythagorean,    // a chain of pure fifths: perfect fifths and fourths, wide thirds
    Just            // small whole-number ratios: pure triads, at the price of a root
};

// One name per temperament, in the order of the enum. The settings screen reads them from here, so that the list and
// the enum can never drift apart.
//
// Les noms sont ceux de la litterature musicale francaise, et pas des raccourcis : « temperature egal » est le nom du
// systeme (on lit aussi « gamme temperee »), « pythagoricien » renvoie au cycle des quintes pures, et le temperament
// juste est rattache a Zarlino, dont la gamme naturelle est la reference historique.
inline constexpr std::array<std::string_view, 3> TEMPERAMENT_NAMES{ "Tempérament égal", "Pythagoricien", "Juste (Zarlino)" };

// The tuning a note is HEARD in: which temperament, and at what diapason. This is the context that travels from the
// settings screen down to the audio layer, so that the sound follows the choice.
//
// The ROOT is deliberately NOT here: it is the note an interval is heard FROM, and the audio layer always knows it -
// it is the first note it plays. A context is a global choice; a root is a property of the question.
struct TuningContext
{
    Temperament temperament{ Temperament::Equal };
    double referencePitchHz{ REFERENCE_FREQUENCY_HZ };
};

// Frequency of a note, HEARD IN THE CONTEXT of a root note.
//
// Equal temperament ignores the root - that is the whole point of it - and the first branch says so. The others are
// built from it: the fifth above the root is 3/2 for good, whatever the root is.
//
// p_referencePitchHz is the A4 diapason, 440 Hz by default. Some instruments - wind ones in particular - are
// deliberately tuned a little sharp to sound brighter, and the tuner must be able to follow them: every frequency
// is simply scaled by reference / 440.
[[nodiscard]] double frequencyFor( Note p_note,
                                   Note p_root,
                                   Temperament p_temperament,
                                   double p_referencePitchHz = REFERENCE_FREQUENCY_HZ ) noexcept;

// The distance between two frequencies, in CENTS. Twelve hundred cents make an octave, so the number is the same
// size in every register - which is what an ear, and every tuner ever built, actually needs to judge a note.
//
// A value of zero means the frequencies are identical. Positive means the first one is higher. The result is
// deliberately NOT rounded here: how close is "in tune" is a judgement, and it belongs to the caller.
//
// Either frequency being zero or negative is silence, not a note, and gives back zero.
[[nodiscard]] double centsBetween( double p_frequencyHz, double p_referenceHz ) noexcept;

}    // namespace musichien::domain
