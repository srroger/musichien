#pragma once

// =====================================================================================================================
// Musichien - Scale
//
// LES GAMMES QUI NE SONT NI LE MAJEUR, NI SES MODES.
//
// `Mode` est un cas tres particulier : sept modes, une seule gamme, et sept ROTATIONS d'elle. C'est cette parente qui rend
// le cercle, le degre et le vamp parlants - et c'est pour cela qu'on n'y touche pas.
//
// Une gamme, ici, est autre chose : une TABLE. Un nom, une suite de degres en demi-tons depuis la tonique, et rien de
// plus. Pas de rotation, pas de « mode de », pas de comparaison - une pentatonique a CINQ notes, une blues SIX, et aucun
// mode du majeur n'a cette forme.
//
// ELLE VIT A COTE DE Mode, JAMAIS DEDANS : rien ici ne connait Mode, et rien dans Mode ne connait Scale. C'est ce qui
// protege l'arcade, l'entrainement et le bilan, qui reposent tous sur le majeur.
//
// ET CE QU'ELLE N'EST PAS : elle ne sait rien du son, rien de l'ecran, rien de la langue du joueur. Le nom s'affiche
// ailleurs, en francais ; l'identifiant qu'on garde ici est anglais, stable, et fait pour ne jamais changer.
// =====================================================================================================================

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace musichien::domain
{

// Combien de degres au maximum. SEPT, parce que c'est ce que porte le majeur et ses modes, et que les six gammes retenues
// tiennent toutes dedans - cinq, six ou sept notes. Une gamme plus longue - une diminuee en compte huit - demanderait
// d'elargir ce nombre : c'est une decision a prendre le jour ou on l'ajoute, et pas avant.
inline constexpr std::size_t MAX_SCALE_DEGREE_COUNT = 7;

enum class Scale : std::size_t
{
    // Les cinq notes les plus jouees du monde : rock, blues, pop, folk. AUCUN demi-ton, donc aucune note qu'on puisse
    // « rater » : c'est la gamme qui pardonne, et c'est pour cela qu'elle sert des la premiere jam.
    PentatonicMinor = 0,

    // Sa relative : le meme materiau, un autre centre. C'est la pentatonique majeure qu'on entend dans les solos
    // country et dans la pop.
    PentatonicMajor = 1,

    // La pentatonique mineure PLUS LA QUINTE DIMINUEE, la « note bleue ». Six notes, et un son que tout le monde
    // reconnait sans savoir le nommer - c'est la gamme du blues, du rock et du jazz.
    Blues = 2,

    // Le majeur avec une SIXTE MINEURE et une SEPTIEME MAJEURE. Bach, tout le classique, le metal, et une grande part
    // de la musique orientale : c'est la seconde gamme la plus utilisee apres le majeur.
    HarmonicMinor = 3,

    // LE CINQUIEME MODE du mineur harmonique : un majeur a tierce mineure et septieme mineure. Flamenco, espagnol, metal.
    // Il est range ICI comme une gamme a part entiere, et pas comme un mode : il s'entend seul, et c'est ainsi qu'on
    // l'utilise.
    PhrygianDominant = 4,

    // La « gamme jazz », MONTE : un mineur naturel avec une sixte majeure. Ses modes portent le lydien bemol 7 et
    // l'altere, deux couleurs de l'harmonie moderne.
    MelodicMinor = 5,
};

inline constexpr std::size_t SCALE_COUNT = 6;

// Les degres d'une gamme, en demi-tons depuis sa tonique - et COMBIEN il y en a.
//
// Le compte voyage AVEC les degres, et pas a cote : une gamme de cinq notes et une de sept se dessinent pareil, et un
// ecran qui devrait deviner le compte finirait par en inventer un. Le premier degre vaut toujours zero - c'est la
// tonique, par definition - et c'est cette evidence qu'un tableau ecrit a la main peut casser.
struct ScaleDegrees
{
    std::array<std::int32_t, MAX_SCALE_DEGREE_COUNT> offsets{};
    std::size_t count{ 0 };
};

[[nodiscard]] constexpr ScaleDegrees scaleDegreeOffsets( Scale p_scale ) noexcept
{
    switch( p_scale )
    {
        case Scale::PentatonicMinor:
            return ScaleDegrees{ { 0, 3, 5, 7, 10 }, 5 };

        case Scale::PentatonicMajor:
            return ScaleDegrees{ { 0, 2, 4, 7, 9 }, 5 };

        case Scale::Blues:
            // La quinte diminuee (6) s'ajoute au pentatonique mineur : c'est la seule difference, et c'est toute la
            // couleur du blues.
            return ScaleDegrees{ { 0, 3, 5, 6, 7, 10 }, 6 };

        case Scale::HarmonicMinor:
            // La sixte MINEURE (8) et la septieme MAJEURE (11) : c'est le demi-ton entre elles qui fait tout le
            // caractere, et c'est ce que le majeur n'a pas.
            return ScaleDegrees{ { 0, 2, 3, 5, 7, 8, 11 }, 7 };

        case Scale::PhrygianDominant:
            // Tierce mineure (3), seconde mineure (1), septieme mineure (10) : c'est le MINEUR HARMONIQUE vu depuis son
            // cinquieme degre, mais ecrit pour lui-meme - une gamme qui vit seule se lit mieux dans son propre ordre.
            return ScaleDegrees{ { 0, 1, 4, 5, 7, 8, 10 }, 7 };

        case Scale::MelodicMinor:
            // Montee seulement : c'est la forme qu'on emploie, et l'ascendante ET la descendante sont deux gammes.
            return ScaleDegrees{ { 0, 2, 3, 5, 7, 9, 11 }, 7 };
    }

    return ScaleDegrees{};
}

// L'IDENTIFIANT d'une gamme : stable, ANGLAIS, et jamais traduit.
//
// C'est lui qu'un contenu ou un fichier garderait, exactement comme pour les intervalles. Le NOM que le joueur lit
// appartient a l'ecran, dans sa langue - et les deux ne doivent jamais se melanger.
[[nodiscard]] std::string_view scaleIdentifier( Scale p_scale ) noexcept;

// La gamme qu'un rang designe, et la premiere pour tout ce qui n'est pas un rang. Un identifiant se lit dans un fichier
// qu'un humain peut editer : une valeur abimee doit couter un reglage, jamais un plantage.
[[nodiscard]] Scale scaleFromIndex( std::size_t p_index ) noexcept;

}    // namespace musichien::domain
