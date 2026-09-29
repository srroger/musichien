#include "infrastructure/content/JsonTunerGuide.h"

#include <gtest/gtest.h>

namespace musichien::infrastructure
{

TEST( JsonTunerGuideTest, a_text_follows_its_temperament_and_not_its_order )
{
    const domain::TunerGuide guide = readTunerGuide( R"({
        "temperaments": [
            { "id": "just", "texte": "le juste" },
            { "id": "equal", "texte": "l'egal" }
        ],
        "diapason": "le diapason",
        "noteDeReference": "la tonique",
        "modeEmploi": [ "un", "deux" ]
    })" );

    // Le fichier designe un temperament par sa VALEUR, donc l'ordre des entrees n'a aucune importance : c'est ce qui
    // permet de reorganiser le fichier sans changer une ligne de code.
    EXPECT_EQ( "l'egal", guide.temperamentText( domain::Temperament::Equal ) );
    EXPECT_EQ( "le juste", guide.temperamentText( domain::Temperament::Just ) );

    EXPECT_EQ( "le diapason", guide.diapasonText() );
    EXPECT_EQ( "la tonique", guide.referenceNoteText() );

    ASSERT_EQ( 2U, guide.howToSteps().size() );
    EXPECT_EQ( "un", guide.howToSteps().front() );
}

TEST( JsonTunerGuideTest, a_broken_file_costs_the_explanations_and_nothing_else )
{
    const domain::TunerGuide guide = readTunerGuide( "{ ceci n'est pas du JSON" );

    EXPECT_TRUE( guide.isEmpty() );
}

TEST( JsonTunerGuideTest, a_temperament_the_domain_does_not_know_is_skipped )
{
    const domain::TunerGuide guide = readTunerGuide( R"({
        "temperaments": [
            { "id": "mesotonique", "texte": "pas encore implemente" },
            { "id": "equal", "texte": "l'egal" }
        ]
    })" );

    // Un fichier qui parle d'un temperament inconnu n'est pas une faute : l'entree est ignoree et le reste se lit.
    // C'est ce qui permettra un jour d'ajouter le mesotonique sans casser les fichiers deja ecrits.
    EXPECT_EQ( 1U, guide.temperamentCount() );
    EXPECT_EQ( "l'egal", guide.temperamentText( domain::Temperament::Equal ) );
    EXPECT_TRUE( guide.temperamentText( domain::Temperament::Pythagorean ).empty() );
}

TEST( JsonTunerGuideTest, a_missing_field_leaves_an_empty_text_not_an_error )
{
    const domain::TunerGuide guide = readTunerGuide( R"({ "diapason": "seulement ceci" })" );

    EXPECT_EQ( "seulement ceci", guide.diapasonText() );
    EXPECT_TRUE( guide.referenceNoteText().empty() );
    EXPECT_EQ( 0U, guide.temperamentCount() );
    EXPECT_FALSE( guide.isEmpty() );
}

}    // namespace musichien::infrastructure
