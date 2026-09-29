#pragma once

// =====================================================================================================================
// Musichien - ChordTree
//
// L'ARBRE DES ACCORDS : comment chaque couleur s'obtient depuis une autre, en UN geste.
//
// Un majeur devient mineur en abaissant sa tierce, un mineur devient demi-diminu en abaissant sa quinte, et ainsi de
// suite : quinze couleurs, quatorze gestes, et une seule racine. C'est ce qui enseigne quelque chose de beaucoup plus
// puissant qu'une liste de cinquante accords a retenir.
//
// ---------------------------------------------------------------------------------------------------------------------
// Pourquoi c'est dans le DOMAINE
//
// Parce que c'est de la THEORIE MUSICALE, et rien d'autre : l'arbre ne parle ni d'ecran ni de pixels, il parle
// d'intervalles. Et parce que cela se TESTE : les degres ecrits ici doivent correspondre aux demi-tons de Chord, et un
// test le verifie sur les quinze couleurs - une table de theorie musicale qui se trompe apprend le faux.
//
// Les DEMI-TONS restent dans Chord, parce que c'est l'audio qui en a besoin ; les DEGRES vivent ici, parce que c'est
// l'oeil et la memoire qui en ont besoin. Les deux doivent dire la meme chose, et c'est le test qui les tient ensemble.
// =====================================================================================================================

#include "domain/music/Chord.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace musichien::domain
{

// Un degre de la gamme, avec son alteration : 1, 3, 5, b3, #5, bb7...
//
// Les degres se comptent comme les musiciens les comptent : 1 pour la tonique, 3 pour la tierce, 7 pour la septieme, 9
// pour la neuvieme - meme quand elle sonne comme une seconde a l'octave.
struct ChordDegree
{
    std::int32_t degree{ 1 };

    // 0 = juste, -1 = bemolissee, -2 = doublement bemolissee, +1 = diese.
    std::int32_t alteration{ 0 };
};

// Les degres d'une couleur, du grave vers l'aigu.
[[nodiscard]] std::span<const ChordDegree> chordDegrees( ChordQuality p_quality ) noexcept;

// La distance d'un degre a la tonique, en demi-tons : 1 -> 0, b3 -> 3, #5 -> 8, bb7 -> 9, 9 -> 14.
//
// C'est la passerelle entre les deux ecritures de la meme verite, et c'est elle que le test emprunte pour comparer les
// degres de ce fichier aux intervalles de Chord.
[[nodiscard]] std::int32_t semitonesOfDegree( ChordDegree p_degree ) noexcept;

// Les degres, en clair : « 1 3 5 », « 1 b3 5 b7 », « 1 b3 b5 bb7 ».
//
// C'est CE QUE L'ARBRE ECRIT sous chaque nom d'accord, et c'est ce qui apprend quelque chose : « Cm7, c'est 1 b3 5 b7 ».
[[nodiscard]] std::string chordDegreesLabel( ChordQuality p_quality );

// Un NOEUD de l'arbre : une couleur, d'ou elle vient, et par quel geste.
struct ChordNode
{
    ChordQuality quality{ ChordQuality::Major };

    // La couleur dont celle-ci descend. La racine se designe elle-meme.
    ChordQuality parent{ ChordQuality::Major };

    // Le geste qui mene du parent a cette couleur, en clair : « tierce mineure », « septieme mineure ajoutee »...
    //
    // C'est le libelle le plus important de tout l'arbre : c'est lui qui transforme une liste en lecon.
    std::string_view mutation;

    // La distance a la racine, en gestes. L'ecran s'en sert pour placer les colonnes.
    std::int32_t depth{ 0 };

    [[nodiscard]] bool isRoot() const noexcept { return depth == 0; }
};

// L'ARBRE, dans l'ORDRE de lecture : la racine, puis chaque sous-arbre en entier avant le suivant (un parcours en
// profondeur).
//
// L'ordre n'est pas cosmetique : c'est ce qui permet a un ecran de poser les noeuds en lignes, sans aucun calcul de
// disposition - un sous-arbre occupe des lignes CONSECUTIVES, donc un enfant est toujours proche de son parent.
[[nodiscard]] std::span<const ChordNode> chordTree() noexcept;

}    // namespace musichien::domain
