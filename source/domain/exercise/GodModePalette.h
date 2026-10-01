#pragma once

// =====================================================================================================================
// Musichien - GodModePalette
//
// LE PERIMETRE DE TRAVAIL, choisi par le joueur.
//
// ---------------------------------------------------------------------------------------------------------------------
// Pourquoi un GodMode, et pourquoi il NE remplace PAS les niveaux
//
// Les cinq niveaux disent ce que le joueur SAIT, et le jeu en deduit une progression : une palette qui s'elargit sur les
// reussites et se retrecit sur une erreur. C'est la « conduite accompagnee ».
//
// Le GodMode dit ce que le joueur veut TRAVAILLER, et il le dit lui-meme : Roger l'a voulu comme un dieu - « il a tous
// les pouvoirs ». Il casse deliberement la conduite accompagnee, et c'est pour cela que l'XP, les statistiques et les
// bilans continuent : le joueur a choisi de jouer autrement, pas de sortir du jeu.
//
// Les deux ne se remplacent donc pas. Les cinq niveaux restent, avec leur progression, et ils servent en plus de
// MODELES : ils pre-remplissent les cases du GodMode.
//
// ---------------------------------------------------------------------------------------------------------------------
// AU MOINS DEUX, et ce n'est pas une precaution de confort
//
// Une famille reduite a UN element ne pose plus de question : la reponse serait toujours la meme, et le joueur repondrait
// juste sans ecouter. La regle porte donc sur les familles dont la PART de questions n'est pas nulle - exiger deux
// accords d'un joueur qui n'entend jamais d'accords serait une regle morte, et une regle morte est une regle qu'on
// apprend a ignorer.
//
// ---------------------------------------------------------------------------------------------------------------------
// L'ordre d'apprentissage FILTRE
//
// Un joueur coche un ENSEMBLE : les intervalles qu'il veut travailler. Mais l'ordre dans lequel on les lui montre ne doit
// pas dependre de l'ordre dans lequel il a clique - deux joueurs ayant coche la meme chose verraient deux listes
// differentes, et la grille de reponse changerait d'une partie a l'autre. L'ordre d'apprentissage sert donc de filtre, et
// c'est deja lui qui ordonne tout le reste du jeu.
// =====================================================================================================================

#include "domain/exercise/ExerciseSession.h"
#include "domain/exercise/PlayerLevel.h"
#include "domain/music/Chord.h"
#include "domain/music/Interval.h"
#include "domain/music/Mode.h"

#include <cstddef>
#include <vector>

namespace musichien::domain
{

// Ce que le joueur veut travailler. Les trois familles, et rien d'autre : les parts de questions, les aides et le tempo
// restent des REGLAGES, et le GodMode ne fait que restreindre le perimetre de ce qu'elles posent. Deux ecrans qui
// regleraient la meme chose finiraient par se contredire.
struct GodModePalette
{
    std::vector<Interval> intervals;
    std::vector<ChordQuality> chords;
    std::vector<Mode> modes;
};

// Le plus petit nombre d'elements d'une famille dont la part de questions n'est pas nulle.
inline constexpr std::size_t MINIMUM_GOD_MODE_CHOICES = 2;

// La palette avec laquelle un niveau demarre aujourd'hui : les cinq MODELES du GodMode.
//
// Elle se derive des MEMES fonctions que celles qu'utilise la session, et c'est ce qui garantit que « pre-remplir » ne
// dit pas autre chose que « commencer » - deux facons de repondre a « ou commence un debutant » finiraient par diverger,
// et le joueur verrait des cases cochees qui ne sont pas celles de sa partie.
[[nodiscard]] GodModePalette paletteForLevel( PlayerLevel p_level );

// La meme palette, rangee dans l'ordre d'apprentissage de chaque famille.
[[nodiscard]] GodModePalette orderedPalette( GodModePalette p_palette );

// Vrai si la palette peut poser des questions : au moins deux elements dans chaque famille dont la part n'est pas nulle.
//
// Les parts sont lues dans les reglages du joueur, et non devinees : c'est ce qui rend la regle exacte - on n'exige pas
// deux accords de quelqu'un qui a mis la part des accords a zero.
[[nodiscard]] bool isPlayable( const GodModePalette & p_palette, const SessionSettings & p_settings );

// Les reglages d'une seance en GodMode : les trois listes choisies, FIGEES, et tout le reste repris des reglages du
// joueur - les parts, les vies, le nombre de questions, la fenetre melodique.
[[nodiscard]] SessionSettings sessionSettingsFor( const GodModePalette & p_palette, SessionSettings p_base );

}    // namespace musichien::domain