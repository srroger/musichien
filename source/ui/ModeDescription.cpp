#include "ui/ModeDescription.h"

#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <array>
#include <cstddef>
#include <string_view>

namespace musichien::ui
{

namespace
{

// Les noms francais, dans l'ORDRE DE L'ENUMERATION, avec la phrase qui dit ce qui colore chaque mode.
//
// Une seule table, lue dans un seul sens : un nom et sa caracteristique ne peuvent pas se desynchroniser. L'ordre est
// celui de la clarte, donc celui de Mode - c'est le meme ordre que les boutons du banc d'essai.
//
// LES DEGRES SONT ECRITS EN MOTS, et c'est une correction, pas un gout. La notation « 5 chapeau » (un chiffre suivi d'un
// accent circonflexe COMBINANT, U+0302) s'affiche correctement dans un editeur de texte et se casse a l'ecran : la
// police de l'application rend l'accent comme un caractere separe, et Roger a lu « 5` diminuée » sur son telephone. Un
// mot ne peut pas se casser.
constexpr std::array<const char *, domain::MODE_COUNT> MODE_DISPLAY_NAMES{
  "Lydien", "Ionien", "Mixolydien", "Dorien", "Éolien", "Phrygien", "Locrien" };

constexpr std::array<const char *, domain::MODE_COUNT> MODE_CHARACTERISTICS{
  "la quarte augmentée",
  "la gamme majeure : la référence",
  "la septième mineure, la septième « de dominante »",
  "la sixte majeure, la sixte qui éclaire le mineur",
  "la gamme mineure naturelle : la référence des ombres",
  "la seconde mineure, une tension dès la première marche",
  "la quinte diminuée : rien n'y repose vraiment" };

}    // namespace

QVariantMap describeMode( domain::Mode p_mode )
{
    const auto index = static_cast<std::size_t>( domain::modeIndex( p_mode ) );

    // La clarte : 1 pour le premier de l'ordre de couleur, 0 pour le dernier.
    //
    // CALCULEE et jamais ecrite a la main : l'ordre de l'enumeration EST l'ordre de couleur (voir Mode.h), donc un mode
    // ajoute ou deplace ne peut pas laisser un ecran mentir sur sa clarte.
    const double brightness =
      1.0 - ( static_cast<double>( index ) / static_cast<double>( domain::MODE_COUNT - 1 ) );

    const std::string_view identifier = domain::modeIdentifier( p_mode );

    QVariantMap description;

    description.insert( QStringLiteral( "index" ), static_cast<int>( index ) );
    description.insert( QStringLiteral( "identifier" ),
                        QString::fromUtf8( identifier.data(), static_cast<int>( identifier.size() ) ) );
    description.insert( QStringLiteral( "name" ), QString::fromUtf8( MODE_DISPLAY_NAMES.at( index ) ) );
    description.insert( QStringLiteral( "characteristic" ), QString::fromUtf8( MODE_CHARACTERISTICS.at( index ) ) );
    description.insert( QStringLiteral( "brightness" ), brightness );

    return description;
}

QVariantList describeAllModes()
{
    QVariantList modes;

    for( std::size_t index = 0; index < domain::MODE_COUNT; ++index )
    {
        modes.append( describeMode( static_cast<domain::Mode>( index ) ) );
    }

    return modes;
}

}    // namespace musichien::ui
