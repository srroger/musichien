#pragma once

// =====================================================================================================================
// Musichien - GameMode
//
// The four ways to play, and the one that pays.
//
// ---------------------------------------------------------------------------------------------------------------------
// Why it exists
//
// The game used to have one door. It now has four, and they do not answer the same question:
//
//   * TRAINING asks « let me work on ONE thing ». It opens a single family - intervals, chords or modes - for ten
//     questions, and it pays NOTHING. It is practice, and practice is measured, not rewarded.
//
//   * ARCADE asks « am I getting better? ». Twenty-five questions, and the DOSAGE is imposed: ten intervals, eight
//     chords, seven modes - the last of them a foreign note, the boss it ends on. This is the only mode that earns
//     experience, and the reason is Roger's: « s'il veut progresser en expérience, il doit impérativement faire le B ».
//     A player cannot farm experience by working only what he is already good at.
//
//   * INFINITE and SURVIVAL are free play. They follow the weights of the settings, they pay nothing, and Survival
//     keeps the player's longest run - the only thing of his that survives a bad evening.
//
//   * REVIEW is the weekly bilan. It pays nothing either; its reward is elsewhere (see the vault).
//
// The three families are INDISSOCIABLE, and that is what the arcade is for: a player who is left alone with three
// training buttons will work the one he likes. The arcade removes the choice without removing the freedom - training is
// still there, it simply does not count.
// =====================================================================================================================

#include "domain/exercise/ExerciseSession.h"
#include "domain/exercise/PlayerLevel.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace musichien::domain
{

// The ways to play. Explicit values, because a screen reads them as numbers.
enum class GameMode : std::size_t
{
    Training = 0,
    Arcade = 1,
    Infinite = 2,
    Survival = 3,
    Review = 4
};

inline constexpr std::size_t GAME_MODE_COUNT = 5;

// Experience is earned in ONE place, and this is the whole rule.
[[nodiscard]] constexpr bool grantsExperience( GameMode p_mode ) noexcept
{
    return p_mode == GameMode::Arcade;
}

// ---------------------------------------------------------------------------------------------------------------------
// The arcade recipe
// ---------------------------------------------------------------------------------------------------------------------

// Dix intervalles, huit accords, sept modes - et les sept modes sont quatre questions differentes « 2 + 2 + 2 + 1 ».
// Voir arcadePlan pour l'ordre : les intervalles d'abord, les accords ensuite, les modes, et la NOTE ETRANGERE en
// dernier - le boss sur lequel la partie se termine.
inline constexpr std::size_t ARCADE_INTERVAL_QUESTION_COUNT = 10;
inline constexpr std::size_t ARCADE_CHORD_QUESTION_COUNT = 8;
inline constexpr std::size_t ARCADE_MODE_QUESTION_COUNT = 7;
inline constexpr std::size_t ARCADE_QUESTION_COUNT = ARCADE_INTERVAL_QUESTION_COUNT + ARCADE_CHORD_QUESTION_COUNT
                                                     + ARCADE_MODE_QUESTION_COUNT;

// Dix coeurs, et c'est ce qui rend le multiplicateur lisible : perdre moins de cinq coeurs, c'est avoir tenu la partie.
inline constexpr std::int32_t ARCADE_STARTING_LIVES = 10;

// Le multiplicateur d'experience de l'Arcade, selon les coeurs perdus.
//
//   * AUCUN coeur perdu : x2. La partie parfaite, et le jeu le dit fort.
//   * MOINS DE CINQ : x1.2. Une belle partie, avec une ou deux erreurs.
//   * CINQ OU PLUS : aucun bonus. On a appris, mais on n'a pas brille - et c'est deja beaucoup.
//
// Expose separement du calcul d'experience : c'est une REGLE du jeu, et un ecran qui veut dire « x2 » doit dire le
// nombre que le modele applique vraiment.
[[nodiscard]] double arcadeMultiplier( std::int32_t p_livesLost ) noexcept;

// L'experience finale d'une Arcade : le score, multiplie par le merite des coeurs, puis par ce qui a ete JOUE.
//
// LE DERNIER FACTEUR EST LE PLUS SEVERE, et c'est une decision de Roger : « il faudrait drastiquement reduire l'experience
// gagnee quand on perd avant la fin ». Une Arcade qui s'arrete au huitieme des vingt-cinq ne rapporte donc QUE huit
// vingt-cinquiemes de ce qu'elle a produit - la partie inachevee ne paie pas comme une partie tenue. Sans ce facteur,
// perdre en boucle rapportait autant qu'une partie complete, et Roger l'a vu : « j'ai perdu plein de fois le mode arcade,
// et ca m'a fait grinder assez d'experience pour passer au niveau suivant ».
//
// p_totalQuestions a zero (theorique) vaut une partie complete : une division par zero n'a pas de sens ici, et refuser de
// payer serait pire que payer.
[[nodiscard]] std::int32_t arcadeExperience( std::int32_t p_baseExperience,
                                             std::int32_t p_livesLost,
                                             std::size_t p_completedQuestions,
                                             std::size_t p_totalQuestions ) noexcept;

// Le PLAN d'une Arcade : vingt-cinq questions dont le DOSAGE est fixe.
//
// Les intervalles et les accords portent DRAWN_TARGET - le plan decide combien, le tirage decide lesquels. Les modes
// disent leur GENRE (couleur, vamp, nom), et la note etrangere ferme la marche.
//
// Le seed est FOURNI, jamais tire ici : le domaine ne possede aucune source d'entropie, et c'est ce qui rend le plan
// reproductible - donc testable.
[[nodiscard]] std::vector<QuestionTarget> arcadePlan( std::uint32_t p_seed );

// Les reglages d'une Arcade pour un joueur de ce niveau : sa palette, son echelle d'aide, et le plan ci-dessus.
[[nodiscard]] SessionSettings arcadeSettingsFor( PlayerLevel p_level, std::uint32_t p_seed );

// Les reglages d'un Entrainement : UNE famille, dix questions, et rien d'autre.
//
// La famille ouverte garde ses SOUS-PARTS (celles qui la detaillent : nommer, chanter, direction pour les intervalles ;
// les quatre questions d'harmonie pour les modes), et toutes les autres familles sont FERMEES. Un entrainement ne paie
// pas d'experience - voir grantsExperience - mais il compte dans les statistiques, et c'est ce qui nourrit le Bilan.
[[nodiscard]] SessionSettings trainingSettingsFor( PlayerLevel p_level, QuestionFamily p_family );

}    // namespace musichien::domain
