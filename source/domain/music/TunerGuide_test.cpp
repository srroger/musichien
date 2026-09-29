#include "domain/music/TunerGuide.h"

#include <gtest/gtest.h>

namespace musichien::domain
{

TEST( TunerGuideTest, a_text_goes_to_the_temperament_it_belongs_to )
{
    TunerGuide guide;

    guide.setTemperamentText( Temperament::Just, "le juste" );
    guide.setTemperamentText( Temperament::Equal, "l'egal" );

    // Le guide range par TEMPERAMENT, et non par ordre d'arrivee : c'est tout l'interet de le faire vivre dans le
    // domaine plutot que dans un ecran, qui n'aurait su qu'empiler les textes.
    EXPECT_EQ( "l'egal", guide.temperamentText( Temperament::Equal ) );
    EXPECT_EQ( "le juste", guide.temperamentText( Temperament::Just ) );

    // Et ce qui n'a pas ete lu reste vide, sans exception ni valeur inventee.
    EXPECT_TRUE( guide.temperamentText( Temperament::Pythagorean ).empty() );
}

TEST( TunerGuideTest, an_empty_guide_is_an_ordinary_state )
{
    const TunerGuide guide;

    // Un fichier de contenu manquant ou casse coute les explications, jamais l'application : un guide vide doit donc
    // etre un etat que l'ecran sait reconnaitre, et non une anomalie.
    EXPECT_TRUE( guide.isEmpty() );
    EXPECT_EQ( 0U, guide.temperamentCount() );
    EXPECT_TRUE( guide.howToSteps().empty() );
}

TEST( TunerGuideTest, the_how_to_keeps_its_order_and_fills_the_guide )
{
    TunerGuide guide;

    guide.addHowToStep( "premiere" );
    guide.addHowToStep( "deuxieme" );

    ASSERT_EQ( 2U, guide.howToSteps().size() );
    EXPECT_EQ( "premiere", guide.howToSteps().front() );

    // Un guide n'est plus vide des qu'une seule de ses parties est remplie.
    EXPECT_FALSE( guide.isEmpty() );
}

TEST( TunerGuideTest, the_count_says_how_many_texts_were_read )
{
    TunerGuide guide;

    EXPECT_EQ( 0U, guide.temperamentCount() );

    guide.setTemperamentText( Temperament::Equal, "un" );
    guide.setTemperamentText( Temperament::Pythagorean, "deux" );

    EXPECT_EQ( 2U, guide.temperamentCount() );
}

}    // namespace musichien::domain
