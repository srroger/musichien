#include "ui/CourseController.h"

#include "domain/audio/NotePlayerFake.h"

#include <QString>
#include <QVariantMap>

#include <gtest/gtest.h>

namespace musichien::ui
{

// ---------------------------------------------------------------------------------------------------------------------
// La page des cours, vue depuis son controleur
//
// Ce que le projet demande a un modele de vue, et rien d'autre : ce qui SERAIT ENTENDU, et ce qui SERAIT AFFICHE. Le
// son est un NotePlayerFake, donc ces tests ne demandent ni carte son ni telephone.
//
// Le cours est construit ICI plutot que lu dans un fichier : ce fichier teste ce que le CONTROLEUR fait de ce que le
// domaine lui donne. La lecture du Markdown a ses propres tests, dans l'infrastructure.
// ---------------------------------------------------------------------------------------------------------------------

namespace
{

[[nodiscard]] domain::Course makeQuintCourse()
{
    domain::Course course;

    course.title = "La quinte juste";
    course.subtitle = "Deux notes qui n'ont rien a se prouver";
    course.chapter = 1;
    course.order = 1;
    course.concepts = { 7 };

    domain::CourseBlock paragraph;
    paragraph.kind = domain::CourseBlock::Kind::Text;
    paragraph.markdown = "Do. Sol. Ensemble.";

    domain::CourseBlock play;
    play.kind = domain::CourseBlock::Kind::PlayInterval;
    play.semitones = 7;
    play.direction = domain::IntervalDirection::Ascending;
    play.caption = "do sol, montant";

    domain::CourseBlock listen;
    listen.kind = domain::CourseBlock::Kind::Listen;
    listen.source = "youtube";
    listen.url = "https://example.org";
    listen.title = "Un titre";
    listen.listenFor = "la quinte, puis l'octave";

    course.blocks.push_back( paragraph );
    course.blocks.push_back( play );
    course.blocks.push_back( listen );

    return course;
}

}    // namespace

TEST( CourseControllerTest, the_library_shows_one_line_per_course )
{
    domain::NotePlayerFake notePlayer;

    const CourseController controller{ notePlayer, { makeQuintCourse() } };

    ASSERT_EQ( controller.library().size(), 1 );

    const QVariantMap entry = controller.library().front().toMap();

    EXPECT_EQ( entry.value( QStringLiteral( "title" ) ).toString(), QString( "La quinte juste" ) );
    EXPECT_EQ( entry.value( QStringLiteral( "blockCount" ) ).toInt(), 3 );
}

TEST( CourseControllerTest, nothing_is_read_until_a_course_is_asked_for )
{
    domain::NotePlayerFake notePlayer;

    CourseController controller{ notePlayer, { makeQuintCourse() } };

    EXPECT_FALSE( controller.isReading() );
    EXPECT_TRUE( controller.blocks().isEmpty() );
    EXPECT_TRUE( controller.title().isEmpty() );
}

TEST( CourseControllerTest, opening_a_course_gives_its_blocks_in_the_order_of_the_file )
{
    domain::NotePlayerFake notePlayer;

    CourseController controller{ notePlayer, { makeQuintCourse() } };

    controller.open( 0 );

    ASSERT_TRUE( controller.isReading() );
    EXPECT_EQ( controller.title(), QString( "La quinte juste" ) );

    const QVariantList blocks = controller.blocks();

    ASSERT_EQ( blocks.size(), 3 );

    // L'ORDRE EST LA DONNEE : un paragraphe, puis une carte a jouer, puis une carte a ecouter.
    EXPECT_EQ( blocks.at( 0 ).toMap().value( QStringLiteral( "kind" ) ).toString(), QString( "text" ) );
    EXPECT_EQ( blocks.at( 1 ).toMap().value( QStringLiteral( "kind" ) ).toString(), QString( "play" ) );
    EXPECT_EQ( blocks.at( 2 ).toMap().value( QStringLiteral( "kind" ) ).toString(), QString( "listen" ) );
}

TEST( CourseControllerTest, a_card_is_translated_into_what_the_page_draws )
{
    domain::NotePlayerFake notePlayer;

    CourseController controller{ notePlayer, { makeQuintCourse() } };

    controller.open( 0 );

    const QVariantMap play = controller.blocks().at( 1 ).toMap();

    EXPECT_EQ( play.value( QStringLiteral( "semitones" ) ).toInt(), 7 );
    EXPECT_EQ( play.value( QStringLiteral( "caption" ) ).toString(), QString( "do sol, montant" ) );

    // La direction voyage en ENTIER, comme le domaine la connait : la traduire deux fois serait deux occasions de se
    // tromper, et le QML transporte deja l'entier.
    EXPECT_EQ( play.value( QStringLiteral( "direction" ) ).toInt(),
               static_cast<int>( domain::IntervalDirection::Ascending ) );

    const QVariantMap listen = controller.blocks().at( 2 ).toMap();

    EXPECT_EQ( listen.value( QStringLiteral( "url" ) ).toString(), QString( "https://example.org" ) );
    EXPECT_EQ( listen.value( QStringLiteral( "listenFor" ) ).toString(), QString( "la quinte, puis l'octave" ) );
}

TEST( CourseControllerTest, a_play_card_makes_the_game_play_it )
{
    // CE QUE LE JEU SAIT JOUER, LE JEU LE JOUE : une carte ':: jeu' ne s'ouvre pas ailleurs, elle SONNE ici.
    domain::NotePlayerFake notePlayer;

    CourseController controller{ notePlayer, { makeQuintCourse() } };

    controller.open( 0 );
    controller.playInterval( 7, static_cast<int>( domain::IntervalDirection::Ascending ) );

    EXPECT_EQ( notePlayer.playedMelodies().size(), 1U );
    EXPECT_TRUE( notePlayer.playedChords().empty() );

    // Et la MEME distance, entendue harmoniquement, sort en accord : c'est la distinction du domaine, et un cours n'a
    // pas a en inventer une autre.
    controller.playInterval( 7, static_cast<int>( domain::IntervalDirection::Harmonic ) );

    EXPECT_EQ( notePlayer.playedChords().size(), 1U );
}

TEST( CourseControllerTest, an_index_that_does_not_exist_opens_nothing )
{
    domain::NotePlayerFake notePlayer;

    CourseController controller{ notePlayer, { makeQuintCourse() } };

    controller.open( -1 );
    controller.open( 7 );

    EXPECT_FALSE( controller.isReading() );
    EXPECT_TRUE( controller.blocks().isEmpty() );
}

TEST( CourseControllerTest, closing_gives_back_an_empty_page )
{
    domain::NotePlayerFake notePlayer;

    CourseController controller{ notePlayer, { makeQuintCourse() } };

    controller.open( 0 );
    controller.close();

    EXPECT_FALSE( controller.isReading() );
    EXPECT_TRUE( controller.blocks().isEmpty() );

    // Le catalogue, lui, reste : on quitte un cours, pas l'Ecole.
    EXPECT_EQ( controller.library().size(), 1 );
}

TEST( CourseControllerTest, a_descending_card_really_descends )
{
    // LE BUG QUE ROGER A ENTENDU : « le sol -> do me fait encore l'ascendant ». La direction n'etait pas lue du tout, et
    // le fichier avait raison de la porter. Un test qui aurait compte les appels n'aurait RIEN vu : ce qui compte est
    // l'ORDRE des deux notes.
    domain::NotePlayerFake notePlayer;

    CourseController controller{ notePlayer, { makeQuintCourse() } };

    controller.playInterval( 7, static_cast<int>( domain::IntervalDirection::Ascending ) );

    ASSERT_EQ( notePlayer.playedMelodies().size(), 1U );
    ASSERT_EQ( notePlayer.playedMelodies().front().notes.size(), 2U );

    // Montant : la note grave d'abord.
    EXPECT_LT( notePlayer.playedMelodies().front().notes.at( 0 ), notePlayer.playedMelodies().front().notes.at( 1 ) );

    controller.playInterval( 7, static_cast<int>( domain::IntervalDirection::Descending ) );

    ASSERT_EQ( notePlayer.playedMelodies().size(), 2U );
    ASSERT_EQ( notePlayer.playedMelodies().back().notes.size(), 2U );

    // Descendant : la note AIGUE d'abord. C'est tout, et c'est tout ce qui manquait.
    EXPECT_GT( notePlayer.playedMelodies().back().notes.at( 0 ), notePlayer.playedMelodies().back().notes.at( 1 ) );
}

TEST( CourseControllerTest, an_annexe_is_told_apart_from_a_course )
{
    // Roger : « je mettrais bien les os a macher d'une autre couleur que les cours ». La difference n'est PAS inventee
    // ici : un cours est NUMEROTE (chapitre et ordre), une annexe ne l'est pas - son en-tete porte une 'famille' a la
    // place. Aucun champ nouveau, donc, et aucun fichier a reviser pour obtenir la distinction.
    domain::NotePlayerFake notePlayer;

    domain::Course annexe = makeQuintCourse();
    annexe.title = "Pourquoi la quinte sonne juste";
    annexe.chapter = 0;

    // L'ANNEXE EST DONNEE EN PREMIER, EXPRES : c'est le TRI qui doit remettre le cours devant, et l'ordre alphabetique
    // aurait fait le contraire (« Pourquoi la quinte... » avant « La quinte juste »).
    const CourseController controller{ notePlayer, { annexe, makeQuintCourse() } };

    ASSERT_EQ( controller.library().size(), 2 );

    EXPECT_EQ( controller.library().at( 0 ).toMap().value( QStringLiteral( "title" ) ).toString(),
               QString( "La quinte juste" ) );
    EXPECT_FALSE( controller.library().at( 0 ).toMap().value( QStringLiteral( "isAnnexe" ) ).toBool() );
    EXPECT_TRUE( controller.library().at( 1 ).toMap().value( QStringLiteral( "isAnnexe" ) ).toBool() );
}

}    // namespace musichien::ui
