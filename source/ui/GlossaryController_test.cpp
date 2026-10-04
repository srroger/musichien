#include "ui/GlossaryController.h"

#include <QString>
#include <QVariantMap>

#include <gtest/gtest.h>

namespace musichien::ui
{

// ---------------------------------------------------------------------------------------------------------------------
// Le glossaire, vu depuis son controleur
//
// Deux choses seulement sont decidables ici, et les deux se testent sans interface : l'ORDRE, qui doit etre celui d'un
// dictionnaire francais, et la LETTRE DE SECTION, qui doit ranger « Echelle » sous E.
//
// Le reste - une liste, des mots, des definitions - n'est pas une regle, c'est du contenu.
// ---------------------------------------------------------------------------------------------------------------------

// DONNES DANS LE DESORDRE, et c'est tout le point : un fichier de contenu est rempli par un auteur qui ajoute toujours le
// mot nouveau A LA FIN. La page doit s'en moquer, et c'est le controleur qui s'en charge.
TEST( GlossaryControllerTest, the_words_come_out_in_dictionary_order )
{
    const std::vector<domain::GlossaryEntry> entries{
      { "Quinte", "..." },
      { "Accord", "..." },
      { "Becarre", "..." },
      { "Mode", "..." },
    };

    const GlossaryController controller{ entries };

    ASSERT_EQ( controller.termCount(), 4 );

    EXPECT_EQ( controller.entries().at( 0 ).toMap().value( QStringLiteral( "word" ) ).toString(), QString( "Accord" ) );
    EXPECT_EQ( controller.entries().at( 1 ).toMap().value( QStringLiteral( "word" ) ).toString(), QString( "Becarre" ) );
    EXPECT_EQ( controller.entries().at( 2 ).toMap().value( QStringLiteral( "word" ) ).toString(), QString( "Mode" ) );
    EXPECT_EQ( controller.entries().at( 3 ).toMap().value( QStringLiteral( "word" ) ).toString(), QString( "Quinte" ) );
}

// « ECHELLE » SE RANGE SOUS E, ET PAS APRES LE V.
//
// C'est tout l'interet de la cle de tri. Un tri par point de code mettrait tous les mots accentues a la FIN du
// dictionnaire : « Echelle » arriverait apres « Vibrato », et le glossaire aurait une section « E accentue » qui
// n'existe dans aucun dictionnaire papier.
TEST( GlossaryControllerTest, an_accented_word_files_under_its_plain_letter )
{
    EXPECT_EQ( GlossaryController::sectionLetterFor( QString::fromUtf8( "Échelle" ) ), QString( "E" ) );
    EXPECT_EQ( GlossaryController::sectionLetterFor( QString::fromUtf8( "bémol" ) ), QString( "B" ) );
    EXPECT_EQ( GlossaryController::sectionLetterFor( QString::fromUtf8( "Altération" ) ), QString( "A" ) );
    EXPECT_EQ( GlossaryController::sectionLetterFor( QString( "octave" ) ), QString( "O" ) );
}

// LE TRI PORTE SUR LES SONS, PAS SUR LES SIGNES : « Demi-ton » se range a « demiton », donc entre « Degre » et
// « Diapason ». Un dictionnaire qui trierait sur le tiret rangerait les mots composes ailleurs que leurs voisins.
TEST( GlossaryControllerTest, a_hyphen_does_not_change_where_a_word_lands )
{
    const std::vector<domain::GlossaryEntry> entries{
      { "Diapason", "..." },
      { "Demi-ton", "..." },
      { "Degre", "..." },
    };

    const GlossaryController controller{ entries };

    ASSERT_EQ( controller.termCount(), 3 );

    EXPECT_EQ( controller.entries().at( 0 ).toMap().value( QStringLiteral( "word" ) ).toString(), QString( "Degre" ) );
    EXPECT_EQ( controller.entries().at( 1 ).toMap().value( QStringLiteral( "word" ) ).toString(), QString( "Demi-ton" ) );
    EXPECT_EQ( controller.entries().at( 2 ).toMap().value( QStringLiteral( "word" ) ).toString(), QString( "Diapason" ) );
}

// LA LETTRE NE SE POSE QU'UNE FOIS, et c'est le controleur qui le dit : la page n'a pas a comparer deux lignes pour
// savoir ou poser un separateur. Cette comparaison est la MEME regle que le tri, et la refaire en QML serait deux
// occasions de se desynchroniser.
TEST( GlossaryControllerTest, the_section_letter_is_flagged_once_per_letter )
{
    const std::vector<domain::GlossaryEntry> entries{
      { "Accord", "..." },
      { "Armure", "..." },
      { "Basse", "..." },
    };

    const GlossaryController controller{ entries };

    ASSERT_EQ( controller.entries().size(), 3 );

    EXPECT_TRUE( controller.entries().at( 0 ).toMap().value( QStringLiteral( "startsSection" ) ).toBool() );
    EXPECT_FALSE( controller.entries().at( 1 ).toMap().value( QStringLiteral( "startsSection" ) ).toBool() );
    EXPECT_TRUE( controller.entries().at( 2 ).toMap().value( QStringLiteral( "startsSection" ) ).toBool() );

    EXPECT_EQ( controller.entries().at( 0 ).toMap().value( QStringLiteral( "sectionLetter" ) ).toString(), QString( "A" ) );
    EXPECT_EQ( controller.entries().at( 2 ).toMap().value( QStringLiteral( "sectionLetter" ) ).toString(), QString( "B" ) );
}

// UN GLOSSAIRE VIDE EST VIDE, et pas une page qui plante : c'est exactement ce qui arrive quand le fichier de contenu
// manque ou ne se lit pas, et la page doit quand meme s'ouvrir.
TEST( GlossaryControllerTest, an_empty_glossary_is_simply_empty )
{
    const GlossaryController controller{ {} };

    EXPECT_EQ( controller.termCount(), 0 );
    EXPECT_TRUE( controller.entries().isEmpty() );
}

}    // namespace musichien::ui