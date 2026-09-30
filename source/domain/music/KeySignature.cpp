#include "domain/music/KeySignature.h"

#include <array>
#include <cstddef>
#include <string_view>

namespace musichien::domain
{

namespace
{

// LES NOMS, et c'est la seule table ecrite a la main de ce fichier.
//
// Indexee par le rang de quinte DECALE de sept : l'entree 0 est do bemol (sept bemols), l'entree 7 est do majeur, et
// l'entree 14 est do diese (sept dieses). L'orthographe est celle de l'ARMURE, et non celle de la touche : re bemol et
// do diese sont la meme case du clavier, et deux tonalites differentes.
constexpr std::array<std::string_view, 15> KEY_NAMES{ "do♭", "sol♭", "ré♭", "la♭", "mi♭", "si♭", "fa",  "do",
                                                      "sol", "ré",   "la",  "mi",  "si",  "fa♯", "do♯" };

// La quinte la plus basse de la table.
constexpr std::int32_t LOWEST_KEY_FIFTHS = -7;

// Les noms des RELATIVES MINEURES, dans le meme ordre et au meme decalage que les noms majeurs.
//
// « do » et « la » ne sont pas separees par trois quintes : elles partagent la MEME armure, et c'est ce qui fait qu'elles
// sont relatives. Cette table est donc une table de noms, et pas une seconde table de rangs - la seule chose qui change
// est l'orthographe de la tonique.
constexpr std::array<std::string_view, 15> RELATIVE_MINOR_NAMES{ "la♭", "mi♭", "si♭", "fa",  "do",  "sol", "ré", "la",
                                                                 "mi",  "si",  "fa♯", "do♯", "sol♯", "ré♯", "la♯" };

}    // namespace

std::int32_t KeySignature::accidentalCount() const noexcept
{
    // Le nombre EST le rang : sept quintes font sept dieses, et le SIGNE dit le sens. Un seul nombre, donc, pour ce
    // qu'un musicien lit comme deux informations.
    return fifths;
}

std::string_view KeySignature::name() const
{
    const auto index = static_cast<std::size_t>( fifths - LOWEST_KEY_FIFTHS );

    if( index >= KEY_NAMES.size() )
    {
        // Un rang hors de la table ne peut venir que d'un appelant qui a invente son nombre : il coute un nom, jamais
        // l'application.
        return KEY_NAMES.at( static_cast<std::size_t>( -LOWEST_KEY_FIFTHS ) );
    }

    return KEY_NAMES.at( index );
}

std::string_view KeySignature::relativeMinorName() const
{
    // La MEME table que les noms majeurs, et le meme decalage : une relative mineure n'a pas un autre nombre
    // d'accidents, elle a le meme - c'est ce qui la rend relative. Ce qui change est le NOM de la tonique, une tierce
    // mineure plus bas.
    //
    // C'est une table de noms, donc, et pas un calcul : « la » et « do » ne different pas de trois quintes, elles
    // partagent la meme armure, et seule l'orthographe les distingue.
    const auto index = static_cast<std::size_t>( fifths - LOWEST_KEY_FIFTHS );

    if( index >= RELATIVE_MINOR_NAMES.size() )
    {
        return RELATIVE_MINOR_NAMES.at( static_cast<std::size_t>( -LOWEST_KEY_FIFTHS ) );
    }

    return RELATIVE_MINOR_NAMES.at( index );
}

std::span<const KeySignature> circleOfFifths()
{
    // Construite UNE fois, et statique : un span sur une variable locale serait un pointeur vers un cadavre. La boucle
    // plutot que douze valeurs recopiees, parce que l'ordre du cercle est une suite, pas une liste.
    static const std::array<KeySignature, CIRCLE_KEY_COUNT> KEYS = []() {
        std::array<KeySignature, CIRCLE_KEY_COUNT> keys{};

        for( std::size_t index = 0; index < keys.size(); ++index )
        {
            keys.at( index ) = KeySignature{ LOWEST_CIRCLE_FIFTHS + static_cast<std::int32_t>( index ) };
        }

        return keys;
    }();

    return KEYS;
}

std::string_view romanNumeral( std::int32_t p_degree ) noexcept
{
    // Les majuscules disent majeur, les minuscules mineur, et le petit cercle diminue : la notation porte la qualite,
    // donc l'ecran n'a pas a la repeter a cote.
    switch( p_degree )
    {
        case 1:
            return "I";

        case 2:
            return "ii";

        case 3:
            return "iii";

        case 4:
            return "IV";

        case 5:
            return "V";

        case 6:
            return "vi";

        case 7:
            return "vii°";

        default:
            return "";
    }
}

}    // namespace musichien::domain
