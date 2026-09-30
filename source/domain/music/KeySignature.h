#pragma once

// =====================================================================================================================
// Musichien - KeySignature (l'armure)
//
// Ce qu'une TONALITE est, en chiffres : de combien de quintes elle est faite, combien d'accidents elle porte et dans quel
// sens, et quelle est sa relative mineure. C'est le socle de l'ecran du cercle des quintes, la page de reference du
// pilier harmonie.
//
// ---------------------------------------------------------------------------------------------------------------------
// Tout se calcule, sauf les NOMS
//
// Le cercle EST une suite de quintes : depuis do, +1 quinte donne sol (un diese), +2 donnent re (deux dieses) ; dans
// l'autre sens, -1 donne fa (un bemol), -2 si bemol (deux bemols). L'armure est donc un NOMBRE, et la relative mineure
// est la meme tonalite vue trois quintes plus bas. Deux regles, aucun tableau.
//
// Restent les NOMS. Ils ne se calculent pas : la bemol et sol diese sont la meme touche et deux notes differentes, et
// c'est l'orthographe de la tonalite qui choisit. Un tableau de noms est donc la seule table ecrite a la main de ce
// fichier - et c'est honnete : ce n'est pas une regle de musique, c'est une convention d'ecriture.
// =====================================================================================================================

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace musichien::domain
{

// Le nombre de tonalites que le cercle montre : douze, une par case.
inline constexpr std::size_t CIRCLE_KEY_COUNT = 12;

// La quinte la plus basse et la plus haute du cercle : de re bemol (cinq bemols) a fa diese (six dieses), ce qui donne
// les douze tonalites qu'un musicien lit le plus souvent, et laisse de cote les enharmonies rares.
inline constexpr std::int32_t LOWEST_CIRCLE_FIFTHS = -5;
inline constexpr std::int32_t HIGHEST_CIRCLE_FIFTHS = 6;

struct KeySignature
{
    // Le rang sur le cercle : +1 est une quinte plus haut, -1 une quinte plus bas, 0 est do majeur.
    std::int32_t fifths{ 0 };

    [[nodiscard]] bool isValid() const noexcept
    {
        return ( fifths >= LOWEST_CIRCLE_FIFTHS ) && ( fifths <= HIGHEST_CIRCLE_FIFTHS );
    }

    // Ce qu'un musicien lit : « 1 diese », « 3 bemols ». NEGATIF pour les bemols, POSITIF pour les dieses, et zero pour
    // do majeur : un seul nombre, et le signe EST le sens.
    [[nodiscard]] std::int32_t accidentalCount() const noexcept;

    // Le nom de la tonique, tel qu'il s'ecrit dans une armure : « do », « mi♭ », « fa♯ ».
    [[nodiscard]] std::string_view name() const;

    // Le nom de la RELATIVE MINEURE : la meme ARMURE, une tierce mineure plus bas.
    //
    // C'est ce qu'un musicien ecrit dans la case du cercle a cote du nom majeur - « do / la mineur », « sol / mi
    // mineur » - et c'est donc un NOM, pas un rang. La confusion serait facile : la relative mineure n'a pas un autre
    // nombre d'accidents, elle a le MEME, et c'est justement ce qui la rend relative.
    [[nodiscard]] std::string_view relativeMinorName() const;
};

// Les douze tonalites du cercle, de cinq bemols a six dieses : l'ordre de la roue, et rien d'autre.
[[nodiscard]] std::span<const KeySignature> circleOfFifths();

// La qualite d'un accord construit sur un degre d'une gamme MAJEURE : I, IV et V majeurs, ii, iii et vi mineurs, vii
// diminue. C'est la meme suite pour toutes les tonalites - c'est meme, a la lettre, la definition d'une tonalite
// majeure : une gamme dont le cinquième degre est une dominante.
enum class DegreeQuality
{
    Major,
    Minor,
    Diminished
};

[[nodiscard]] constexpr DegreeQuality degreeQuality( std::int32_t p_degree ) noexcept
{
    switch( p_degree )
    {
        case 1:
        case 4:
        case 5:
            return DegreeQuality::Major;

        case 2:
        case 3:
        case 6:
            return DegreeQuality::Minor;

        default:
            return DegreeQuality::Diminished;
    }
}

// Le chiffre romain d'un degre : majuscules pour un accord majeur, minuscules pour un mineur, et un petit cercle pour le
// diminue. C'est la notation que tout musicien lit, et elle dit la qualite sans un mot.
[[nodiscard]] std::string_view romanNumeral( std::int32_t p_degree ) noexcept;

}    // namespace musichien::domain
