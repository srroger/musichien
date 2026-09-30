#pragma once

// =====================================================================================================================
// Musichien - describeMode
//
// Turns a domain mode into the plain map a screen can read.
//
// Le nom FRANCAIS vit ici, et non dans le domaine : le domaine dit « Lydian », l'ecran affiche « Lydien ». C'est
// exactement la separation des intervalles - le domaine nomme la chose, l'interface l'ecrit pour un humain.
//
// Chaque cle est remplie a chaque fois, et l'ecran les lit sans verifier :
//
//   * 'index'          le rang du mode, qui est aussi son rang de CLARTE (0 = le plus clair)
//   * 'identifier'     l'identifiant stable, celui du code et des fichiers de contenu (lydian, dorian...)
//   * 'name'           le nom qu'un humain lit, en francais (Lydien, Dorien...)
//   * 'characteristic' la note qui colore le mode, en une phrase courte
//   * 'brightness'     la clarte, de 1 (le plus clair) a 0 (le plus sombre), pour peindre la case
//
// La clarte est CALCULEE a partir du rang, et jamais ecrite a la main : l'ordre de l'enumeration EST l'ordre de
// couleur (voir Mode.h), donc ajouter un mode ou en deplacer un ne peut pas laisser un ecran mentir sur sa clarte.
// =====================================================================================================================

#include "domain/music/Mode.h"
#include "domain/music/Phrase.h"

#include <QVariantList>
#include <QVariantMap>

namespace musichien::ui
{

[[nodiscard]] QVariantMap describeMode( domain::Mode p_mode );

// La difference entre deux modes, telle qu'un verdict la dit : le degre qui a bouge, et son accidental.
//
// « De dorien a ionien : la tierce a monte d'un demi-ton » vaut mieux que deux noms poses cote a cote : c'est la seule
// chose qu'on ait vraiment entendue, et c'est donc la seule qui s'apprenne.
[[nodiscard]] QVariantMap describeModeDifference( domain::Mode p_from, domain::Mode p_to );

// Une phrase, telle qu'un ecran la montre : ses degres - « 1 4(2) 5 1 » - et son tempo.
//
// Les degres sont ecrits COMME UN MUSICIEN LES LIT : un chiffre par pas, et la duree entre parentheses quand elle
// depasse un temps. C'est exactement l'ecriture de l'atelier, et ce n'est pas une coincidence : ce que l'oreille a vu
// en triant doit etre ce qu'elle retrouve en ecoutant, sans quoi les deux ne peuvent pas se relier.
[[nodiscard]] QVariantMap describePhrase( const domain::Phrase & p_phrase );

// Le cercle des quintes vu depuis une tonique : les douze notes, celles du mode allumees et les autres eteintes.
//
// Chaque entree porte ce qu'un dessin doit savoir, et rien de plus :
//
//   * 'name'       le nom de la note, EN FRANCAIS - « ré », « fa♯ » - parce que c'est ainsi qu'un musicien les lit
//   * 'inMode'     vrai pour les sept notes du mode : elles s'allument
//   * 'isTonic'    vrai pour la tonique, qui ouvre le cercle et se marque
//
// L'ORDRE de la liste est l'ordre des quintes en partant de la tonique, et c'est tout le dessin : les sept notes d'un
// mode y sont voisines, donc l'arc allume est toujours d'un seul tenant, et ce qui distingue les modes est la place de
// la tonique dans cet arc. Voir domain::modeCircleNotes.
[[nodiscard]] QVariantList describeModeCircle( domain::Mode p_mode, std::int32_t p_tonicPitchClass );

// Les SEPT notes d'une gamme, dans l'ordre des degres - la ou le cercle les donne dans l'ordre des quintes.
//
// C'est ce qu'il faut pour repondre a une question de NOTE ETRANGERE : le joueur a entendu sept notes monter, et il
// designe la place de l'intrus. L'ordre des DEGRES est donc le seul qui convienne - c'est celui de l'ecoute.
//
// Chaque entree porte 'name' (en francais), 'stepIndex' (0 a 6, l'ordre entendu) et 'isTonic'.
[[nodiscard]] QVariantList describeScale( domain::Mode p_mode, std::int32_t p_tonicPitchClass );

// Le nom francais d'une classe de hauteur. Une seule table pour tous les ecrans, et jamais deux facons d'ecrire « fa♯ ».
[[nodiscard]] QString describeNoteName( std::int32_t p_pitchClassIndex );

// Les sept modes, dans l'ordre de clarte, du plus clair au plus sombre.
//
// Une seule fonction, parce que deux ecrans lisent la meme forme de donnees : le banc d'essai aujourd'hui, et le
// cercle de l'exercice du degrade demain. Deux listes ecrites a la main finiraient par diverger.
[[nodiscard]] QVariantList describeAllModes();

}    // namespace musichien::ui
