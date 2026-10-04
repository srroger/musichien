#include "ui/ModeDescription.h"

#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <gtest/gtest.h>

namespace musichien::ui
{

// ---------------------------------------------------------------------------------------------------------------------
// Les degres d'un mode, tels qu'un musicien les lit
//
// Roger, sur le boss : « j'ecrirai dans la ligne juste en dessous, les notes qu'il y a dedans en degre [...] ecrit avec
// les memes couleurs de degrade que les boutons, et le rouge pour la note caracteristique. Histoire d'avoir un repere
// pour l'utilisateur. »
//
// Le compte est verifie sur le MIXOLYDIEN, parce que c'est l'exemple que Roger a donne lui-meme : « 1 2 3 4 5 6 7b ».
// ---------------------------------------------------------------------------------------------------------------------

namespace
{

[[nodiscard]] QVariantList degreesOf( domain::Mode p_mode )
{
    return describeMode( p_mode ).value( QStringLiteral( "degrees" ) ).toList();
}

[[nodiscard]] QString labelOf( const QVariantList & p_degrees, std::size_t p_index )
{
    return p_degrees.at( static_cast<int>( p_index ) ).toMap().value( QStringLiteral( "label" ) ).toString();
}

[[nodiscard]] bool isCharacteristic( const QVariantList & p_degrees, std::size_t p_index )
{
    return p_degrees.at( static_cast<int>( p_index ) )
      .toMap()
      .value( QStringLiteral( "isCharacteristic" ) )
      .toBool();
}

}    // namespace

TEST( ModeDescriptionTest, the_mixolydian_seventh_is_flat_and_characteristic )
{
    const QVariantList degrees = degreesOf( domain::Mode::Mixolydian );

    ASSERT_EQ( degrees.size(), 7U );

    // Les six premiers degres sont ceux de la gamme majeure : rien a signaler.
    EXPECT_EQ( labelOf( degrees, 0 ), QString( "1" ) );
    EXPECT_EQ( labelOf( degrees, 5 ), QString( "6" ) );
    EXPECT_FALSE( isCharacteristic( degrees, 5 ) );

    // ET LE SEPTIEME EST UN 7b - exactement ce que Roger a ecrit - et c'est LUI qui est caracteristique.
    EXPECT_EQ( labelOf( degrees, 6 ), QString::fromUtf8( "7♭" ) );
    EXPECT_TRUE( isCharacteristic( degrees, 6 ) );
}

TEST( ModeDescriptionTest, a_reference_mode_has_no_characteristic_note )
{
    // L'IONIEN EST LA GAMME MAJEURE ELLE-MEME : aucun degre n'est altere, et aucun n'est mis en avant. C'est la reponse du
    // domaine - modeCharacteristicDegree vaut ZERO pour elle - et l'ecran ne doit pas inventer une note a souligner.
    const QVariantList degrees = degreesOf( domain::Mode::Ionian );

    ASSERT_EQ( degrees.size(), 7U );

    for( std::size_t index = 0; index < 7; ++index )
    {
        EXPECT_FALSE( isCharacteristic( degrees, index ) );
    }

    EXPECT_EQ( labelOf( degrees, 6 ), QString( "7" ) );
}

}    // namespace musichien::ui
