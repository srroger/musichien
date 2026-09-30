#pragma once

// =====================================================================================================================
// Musichien - Phrase (et son generateur)
//
// Une PHRASE modale : des DEGRES, des durees, et le contexte qui leur donne un sens - un mode, une tonique, un tempo.
//
// ---------------------------------------------------------------------------------------------------------------------
// Pourquoi des DEGRES, et jamais des notes
//
// Parce qu'une phrase ecrite en degres se TRANSPOSE toute seule : « la meme phrase en la dorien » ne demande aucune
// reecriture, et le domaine sait ce qu'un degre vaut dans un mode. C'est la meme decision que partout ailleurs dans le
// projet - le contenu est de la donnee, la regle est dans le domaine - et ici, elle a une consequence pratique : une
// phrase acceptee a l'atelier est une phrase que le jeu peut jouer dans n'importe quelle tonique.
//
// ---------------------------------------------------------------------------------------------------------------------
// Le generateur est DETERMINISTE, et cela n'a rien d'un detail
//
// Il RECOIT sa graine, il ne la tire pas. C'est ce qui rend une phrase reproductible - donc testable, donc verifiable -
// et c'est aussi ce qui permet a l'atelier de rejouer exactement ce qu'il a produit la veille. Une phrase qui changerait
// a chaque appel ne pourrait ni se discuter ni se corriger.
//
// ---------------------------------------------------------------------------------------------------------------------
// Les contraintes, et pourquoi ce sont celles-la
//
// Un generateur naif qui piocherait dans les sept notes produirait des phrases justes et INAUDIBLES : la note qui colore
// le mode y passerait au hasard, au milieu d'une fusee, et la couleur se noierait. Les quatre regles ci-dessous sont donc
// ce qui separe du bruit d'une phrase qui dit quelque chose :
//
//   * commencer et FINIR sur la tonique : c'est le retour au centre qui fait entendre ou il est ;
//   * faire entendre la NOTE CARACTERISTIQUE du mode, et sur une note LONGUE : c'est elle qui porte la couleur ;
//   * marcher par DEGRES CONJOINTS la plupart du temps : une phrase se chante, et une phrase qui saute ne se chante pas ;
//   * garder une tessiture etroite : deux octaves au plus, pour que l'oreille entende une couleur et non une etendue.
//
// Ce que ces regles ne font PAS, et il faut le dire : elles ne remplacent pas un compositeur. Ce generateur propose, et
// c'est l'oreille de Roger qui dispose - c'est tout le sens de l'atelier.
// =====================================================================================================================

#include "domain/music/Mode.h"
#include "domain/music/Note.h"

#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

namespace musichien::domain
{

// Un pas de phrase : un degre du mode, et combien de temps il dure.
struct PhraseStep
{
    // De 1 a 7, comme un musicien les compte.
    std::int32_t degree{ 1 };

    // La duree, en TEMPS. Un temps vaut une noire : c'est le generateur qui pose le tempo, et non le pas.
    std::int32_t beats{ 1 };
};

// Une phrase, avec de quoi la jouer.
struct Phrase
{
    Mode mode{ Mode::Ionian };

    // La tonique de la MELODIE, et celle que le bourdon tient. La phrase reste dans une octave autour de la premiere, et
    // c'est l'appelant qui decide de l'octave ou il la pose.
    Note tonic{ 60 };

    // Le tempo, en battements par minute. Lent au depart, parce qu'une couleur a besoin de temps pour se dire.
    std::int32_t bpm{ 72 };

    std::vector<PhraseStep> steps;

    // Les notes de la phrase, resolues DANS son mode et sa tonique : c'est la seule fonction qui sait traduire des
    // degres en hauteurs, et c'est ce qui garantit qu'une phrase transposee reste la meme phrase.
    [[nodiscard]] std::vector<Note> notes( Note p_tonic ) const;
};

// Ce qui regle une generation. Des valeurs par defaut, parce qu'un appelant qui ne veut rien regler doit obtenir une
// phrase jouable - et le generateur de l'atelier les expose en options de ligne de commande.
struct PhraseSettings
{
    // Le nombre de pas de la phrase. Huit : assez pour dire une couleur, assez court pour se reecouter.
    std::size_t stepCount{ 8 };

    // La proportion de pas conjoints, en pour cent. Haute a dessein : une phrase se chante.
    std::int32_t stepwiseShare{ 70 };

    // Le nombre de temps que dure la note caracteristique. DEUX, et jamais un : une couleur posee sur une croche
    // s'entend comme une broderie, pas comme une couleur.
    std::int32_t characteristicBeats{ 2 };
};

// Une phrase tiree pour un mode, sur une tonique, avec une graine DONNEE.
//
// Le moteur aleatoire est un parametre, comme partout dans ce domaine : le generateur n'a aucune source d'entropie a
// lui, donc la meme graine rend la meme phrase, sur toutes les machines, pour toujours.
[[nodiscard]] Phrase generatePhrase( Mode p_mode,
                                     Note p_tonic,
                                     std::mt19937 & p_randomEngine,
                                     PhraseSettings p_settings = {} );

}    // namespace musichien::domain
