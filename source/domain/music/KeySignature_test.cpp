#include "domain/music/KeySignature.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <set>

namespace musichien::domain
{

TEST( KeySignatureTest, the_circle_holds_twelve_keys_from_five_flats_to_six_sharps )
{
    const std::span<const KeySignature> keys = circleOfFifths();

    ASSERT_EQ( 12, keys.size() );

    // Cinq bemols en tete, six dieses en queue, un do majeur au milieu : c'est l'ordre de la roue, et il ne doit pas
    // bouger, parce que l'ecran place ses cases par le RANG.
    EXPECT_EQ( -5, keys.front().fifths );
    EXPECT_EQ( 6, keys.back().fifths );

    // Et chaque rang est unique : un doublon sur la roue serait une case de trop.
    std::set<std::int32_t> ranks;

    for( const KeySignature & key : keys )
    {
        ranks.insert( key.fifths );
    }

    EXPECT_EQ( 12, ranks.size() );
}

TEST( KeySignatureTest, the_accidentals_follow_the_rank_and_its_sign )
{
    // Le nombre d'accidents EST le rang : un seul nombre pour ce qu'un musicien lit comme deux informations, et le
    // SIGNE dit le sens.
    EXPECT_EQ( 0, ( KeySignature{ 0 }.accidentalCount() ) );
    EXPECT_EQ( 3, ( KeySignature{ 3 }.accidentalCount() ) );
    EXPECT_EQ( -4, ( KeySignature{ -4 }.accidentalCount() ) );
    EXPECT_EQ( 6, ( KeySignature{ 6 }.accidentalCount() ) );
    EXPECT_EQ( -5, ( KeySignature{ -5 }.accidentalCount() ) );
}

TEST( KeySignatureTest, the_names_are_the_ones_the_armure_shows )
{
    EXPECT_EQ( "do", ( KeySignature{ 0 }.name() ) );
    EXPECT_EQ( "sol", ( KeySignature{ 1 }.name() ) );
    EXPECT_EQ( "fa", ( KeySignature{ -1 }.name() ) );
    EXPECT_EQ( "si♭", ( KeySignature{ -2 }.name() ) );
    EXPECT_EQ( "fa♯", ( KeySignature{ 6 }.name() ) );

    // Et chaque case du cercle porte un nom : un trou sur la roue serait une case muette.
    for( const KeySignature & key : circleOfFifths() )
    {
        EXPECT_FALSE( key.name().empty() );
    }
}

TEST( KeySignatureTest, the_relative_minor_shares_the_armure_and_only_its_name_differs )
{
    // La regle : do majeur et la mineur ont la MEME armure - aucune alteration - et c'est justement ce qui les rend
    // relatives. Ce qui change est le nom, donc : une tierce mineure plus bas.
    EXPECT_EQ( "la", ( KeySignature{ 0 }.relativeMinorName() ) );
    EXPECT_EQ( "mi", ( KeySignature{ 1 }.relativeMinorName() ) );
    EXPECT_EQ( "ré", ( KeySignature{ -1 }.relativeMinorName() ) );
    EXPECT_EQ( "si♭", ( KeySignature{ -5 }.relativeMinorName() ) );
    EXPECT_EQ( "ré♯", ( KeySignature{ 6 }.relativeMinorName() ) );

    // Et chaque case du cercle porte une relative : une case muette serait une case a moitie lue.
    for( const KeySignature & key : circleOfFifths() )
    {
        EXPECT_FALSE( key.relativeMinorName().empty() );
    }
}

TEST( KeySignatureTest, a_degree_carries_the_same_quality_in_every_major_key )
{
    // C'est la definition meme d'une tonalite majeure, donc cela ne depend pas de la tonique : I, IV et V majeurs,
    // ii, iii et vi mineurs, vii diminue.
    EXPECT_EQ( DegreeQuality::Major, degreeQuality( 1 ) );
    EXPECT_EQ( DegreeQuality::Minor, degreeQuality( 2 ) );
    EXPECT_EQ( DegreeQuality::Minor, degreeQuality( 3 ) );
    EXPECT_EQ( DegreeQuality::Major, degreeQuality( 4 ) );
    EXPECT_EQ( DegreeQuality::Major, degreeQuality( 5 ) );
    EXPECT_EQ( DegreeQuality::Minor, degreeQuality( 6 ) );
    EXPECT_EQ( DegreeQuality::Diminished, degreeQuality( 7 ) );

    // Et la notation porte la qualite, donc l'ecran n'a pas a la repeter a cote.
    EXPECT_EQ( "I", romanNumeral( 1 ) );
    EXPECT_EQ( "ii", romanNumeral( 2 ) );
    EXPECT_EQ( "V", romanNumeral( 5 ) );
    EXPECT_EQ( "vii°", romanNumeral( 7 ) );
}

}    // namespace musichien::domain
