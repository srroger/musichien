// =====================================================================================================================
// Musichien - Rhythm
//
// The other half of the ear: pitch says WHAT, rhythm says WHEN. This is the pure, testable core of the rhythm game -
// a metronome's beats and the judgement of a tap against them. No Qt, no clock, no sound: times go in as plain
// numbers, a judgement comes out.
//
// The windows are deliberately generous: the first loop must FEEL generous, and they will tighten as the player
// improves - the same logic as the guided mode that turns itself off. See note 05 of the vault, section 9.
// =====================================================================================================================

#pragma once

#include <cstddef>

namespace musichien::domain
{

// How close a tap has to land to a beat to count, in milliseconds. Large on purpose.
inline constexpr double PERFECT_WINDOW_MS = 80.0;
inline constexpr double GOOD_WINDOW_MS = 200.0;

// How well a tap landed.
enum class HitQuality
{
    Perfect,
    Good,
    Miss
};

// The length of one beat, in milliseconds, at a tempo in beats per minute.
[[nodiscard]] double beatDurationMs( double p_bpm ) noexcept;

// The time of the nth beat (0 is the first), in milliseconds from the start of the metronome.
[[nodiscard]] double beatTimeMs( double p_bpm, std::size_t p_beatIndex ) noexcept;

// Le battement a venir, vu depuis une horloge : QUEL battement ce sera, et QUAND il doit sonner.
struct BeatSchedule
{
    // Le rang du battement a venir, compte depuis l'origine de la grille.
    std::size_t beatIndex{ 0 };
    // Le delai a respecter avant de le jouer. Jamais negatif : un battement deja du se joue tout de suite.
    double delayMs{ 0.0 };
};

// Le battement a venir, sachant que p_elapsedMs se sont ecoulees depuis l'origine de la grille.
//
// Un metronome bat depuis une ORIGINE, et non depuis le battement qu'il vient de jouer : c'est toute la difference
// entre un metronome juste et un metronome qui traine. Un QTimer repetitif repart de l'instant ou il a TIRE, donc
// chaque battement un peu en retard decale tous les suivants, et le retard s'ADDITIONNE - un demi-millieme de seconde
// par temps suffit a s'entendre au bout d'une minute. Ici, l'echeance se calcule depuis l'origine : le retard d'un
// battement ne se reporte jamais sur le suivant.
//
// Une echeance trop depassee pour etre rattrapee - l'application est passee en arriere-plan, le telephone a dormi, la
// boucle d'evenements s'est figee - ne se rattrape pas non plus : rejouer dix battements d'un coup ferait un bruit de
// mitrailleuse. La grille se RECALE alors sur l'instant present, et le metronome repart juste a partir de la.
//
// Cette fonction est ici, dans le domaine, plutot que dans le controleur qui possede le timer, pour une seule raison :
// elle est PURE. Le temps entre en parametre, le delai sort, et un test peut donc dire sans attendre une seconde
// qu'un battement en retard ne decale pas ceux qui suivent.
[[nodiscard]] BeatSchedule planNextBeat( double p_bpm, std::size_t p_beatIndex, double p_elapsedMs ) noexcept;

// The quality of a tap that landed within a given distance of the beat it was aiming at.
//
// Exposed on its own because the metronome game measures that distance against the beat GRID, and the pattern game
// measures it against the onset of a rhythmic CELL: two different questions, one set of windows. Keeping the judgement
// here is what stops the two from drifting apart.
[[nodiscard]] HitQuality judgeDistance( double p_distanceMs ) noexcept;

// The quality of a tap that landed at p_tapMs, against the nearest beat of a metronome at p_bpm.
//
// The nearest beat is the one whose time is closest to the tap; a tap halfway between two beats is a Miss whichever
// way it is measured, and the function says so honestly.
[[nodiscard]] HitQuality judgeTap( double p_tapMs, double p_bpm ) noexcept;

}    // namespace musichien::domain
