#include "infrastructure/content/MarkdownCourse.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <optional>
#include <string_view>

namespace musichien::infrastructure
{

// ---------------------------------------------------------------------------------------------------------------------
// The contract with a lesson file
//
// Same spirit as JsonHintBook_test: this asserts the SHAPE of a file a human edits by hand, and the cases
// that matter are the ones a typo produces. Here a typo costs ONE CARD and the lesson keeps going - which
// is the whole reason a directive is a single line.
// ---------------------------------------------------------------------------------------------------------------------

namespace
{

// A complete lesson: two paragraphs, and one card of each of the four kinds.
constexpr std::string_view COMPLETE_COURSE = R"(---
titre: La quinte juste
sous-titre: Deux notes qui n'ont rien à se prouver
chapitre: 1
ordre: 1
concepts: 7
---

Un premier paragraphe.

:: jeu | demi_tons:7 | ascendant | do → sol, montant

Un deuxieme paragraphe.

:: écoute | youtube | https://example.org | « Un titre » | la quinte, puis l'octave
:: essai | demi_tons:7
:: annexe | pourquoi-la-quinte-sonne-juste
)";

}    // namespace

TEST( MarkdownCourseTest, a_complete_course_is_read_whole )
{
    const std::optional<musichien::domain::Course> course = readCourse( COMPLETE_COURSE );

    ASSERT_TRUE( course.has_value() );

    EXPECT_EQ( course->title, "La quinte juste" );
    EXPECT_EQ( course->subtitle, "Deux notes qui n'ont rien à se prouver" );
    EXPECT_EQ( course->chapter, 1 );
    EXPECT_EQ( course->order, 1 );

    ASSERT_EQ( course->concepts.size(), 1U );
    EXPECT_EQ( course->concepts.front(), 7 );
}

TEST( MarkdownCourseTest, the_paragraphs_and_the_cards_keep_the_order_of_the_file )
{
    const std::optional<musichien::domain::Course> course = readCourse( COMPLETE_COURSE );

    ASSERT_TRUE( course.has_value() );
    ASSERT_EQ( course->blocks.size(), 6U );

    // THE ORDER IS THE DATA. A lesson is a text that carries cards, and a card that came back in the
    // wrong place would be a lesson teaching the wrong thing at the wrong moment.
    EXPECT_EQ( course->blocks[0].kind, musichien::domain::CourseBlock::Kind::Text );
    EXPECT_EQ( course->blocks[1].kind, musichien::domain::CourseBlock::Kind::PlayInterval );
    EXPECT_EQ( course->blocks[2].kind, musichien::domain::CourseBlock::Kind::Text );
    EXPECT_EQ( course->blocks[3].kind, musichien::domain::CourseBlock::Kind::Listen );
    EXPECT_EQ( course->blocks[4].kind, musichien::domain::CourseBlock::Kind::TryExercise );
    EXPECT_EQ( course->blocks[5].kind, musichien::domain::CourseBlock::Kind::Annexe );

    EXPECT_EQ( course->blocks[0].markdown, "Un premier paragraphe." );
    EXPECT_EQ( course->blocks[1].semitones, 7 );
    EXPECT_EQ( course->blocks[1].caption, "do → sol, montant" );
    EXPECT_EQ( course->blocks[3].url, "https://example.org" );
    EXPECT_EQ( course->blocks[3].listenFor, "la quinte, puis l'octave" );
    EXPECT_EQ( course->blocks[5].annexeName, "pourquoi-la-quinte-sonne-juste" );
}

TEST( MarkdownCourseTest, two_paragraphs_separated_by_a_blank_line_stay_two )
{
    const std::optional<musichien::domain::Course> course = readCourse( "Premier.\n\nDeuxieme.\n" );

    ASSERT_TRUE( course.has_value() );
    ASSERT_EQ( course->blocks.size(), 2U );

    EXPECT_EQ( course->blocks[0].markdown, "Premier." );
    EXPECT_EQ( course->blocks[1].markdown, "Deuxieme." );
}

TEST( MarkdownCourseTest, the_lines_of_one_paragraph_are_kept_together )
{
    // Une phrase coupee en deux lignes pour la lisibilite du FICHIER est UN paragraphe : la replier
    // autrement changerait ce que l'auteur a ecrit.
    const std::optional<musichien::domain::Course> course = readCourse( "Une phrase\ncoupee en deux.\n" );

    ASSERT_TRUE( course.has_value() );
    ASSERT_EQ( course->blocks.size(), 1U );

    EXPECT_EQ( course->blocks[0].markdown, "Une phrase\ncoupee en deux." );
}

TEST( MarkdownCourseTest, a_broken_directive_costs_one_card_and_nothing_else )
{
    // LE CAS QUI JUSTIFIE TOUT LE RESTE : une barre oubliee ne doit pas emporter la lecon.
    constexpr std::string_view CONTENT = "Avant.\n\n"
                                         ":: jeu | ascendant\n\n"
                                         "Apres.\n\n"
                                         ":: essai | demi_tons:7\n";

    const std::optional<musichien::domain::Course> course = readCourse( CONTENT );

    ASSERT_TRUE( course.has_value() );

    // Trois blocs, et non quatre : la carte cassee a disparu, les paragraphes et la carte suivante non.
    ASSERT_EQ( course->blocks.size(), 3U );

    EXPECT_EQ( course->blocks[0].markdown, "Avant." );
    EXPECT_EQ( course->blocks[1].markdown, "Apres." );
    EXPECT_EQ( course->blocks[2].kind, musichien::domain::CourseBlock::Kind::TryExercise );
}

TEST( MarkdownCourseTest, a_listening_card_without_its_advice_is_refused )
{
    // LE CINQUIEME CHAMP EST LE PRINCIPE DE SIGNALISATION DE MAYER, et le contrat en fait une
    // obligation : un lien sans consigne d'ecoute est un lien qu'on ne sait pas ecouter.
    const std::optional<musichien::domain::Course> course =
      readCourse( ":: écoute | youtube | https://example.org | « Un titre »\n" );

    // La carte refusee etait la seule chose du fichier : il ne reste rien a lire.
    EXPECT_FALSE( course.has_value() );
}

TEST( MarkdownCourseTest, a_course_without_a_header_takes_its_title_from_the_first_heading )
{
    const std::optional<musichien::domain::Course> course = readCourse( "# Le nom vient d'ici\n\nDu texte.\n" );

    ASSERT_TRUE( course.has_value() );

    EXPECT_EQ( course->title, "Le nom vient d'ici" );
}

TEST( MarkdownCourseTest, a_file_with_no_content_gives_nothing )
{
    EXPECT_FALSE( readCourse( "" ).has_value() );
    EXPECT_FALSE( readCourse( "\n\n   \n" ).has_value() );
    EXPECT_FALSE( readCourse( "---\ntitre: Rien\n---\n" ).has_value() );
}

TEST( MarkdownCourseTest, several_concepts_are_read_at_once )
{
    const std::optional<musichien::domain::Course> course = readCourse( "---\nconcepts: 7, 12\n---\n\nDu texte.\n" );

    ASSERT_TRUE( course.has_value() );
    ASSERT_EQ( course->concepts.size(), 2U );

    EXPECT_EQ( course->concepts[0], 7 );
    EXPECT_EQ( course->concepts[1], 12 );
}

TEST( MarkdownCourseTest, the_two_spellings_of_a_distance_are_both_accepted )
{
    // 'demi_tons:7' se lit bien a voix haute ; '7' s'ecrit vite. Refuser l'un des deux apprendrait au
    // redacteur une regle qui n'achete rien.
    const std::optional<musichien::domain::Course> course = readCourse( ":: jeu | 7 | ascendant | do → sol\n" );

    ASSERT_TRUE( course.has_value() );
    ASSERT_EQ( course->blocks.size(), 1U );

    EXPECT_EQ( course->blocks[0].semitones, 7 );
}

TEST( MarkdownCourseTest, an_unknown_directive_is_skipped )
{
    const std::optional<musichien::domain::Course> course = readCourse( ":: chanson | du texte\n\nUn paragraphe.\n" );

    ASSERT_TRUE( course.has_value() );
    ASSERT_EQ( course->blocks.size(), 1U );

    EXPECT_EQ( course->blocks[0].markdown, "Un paragraphe." );
}

TEST( MarkdownCourseTest, a_level_two_heading_opens_a_page )
{
    // ROGER : « je verrais plus ca comme plusieurs pages par chapitre ». Les pages ne sont pas inventees : ce sont les
    // « ## » du fichier, et le lecteur les separes en gardant AUSSI la liste plate - c'est elle que lit le « voir la note
    // complete », et les deux sortent du meme passage, donc elles ne peuvent pas diverger.
    constexpr std::string_view CONTENT = "Un chapeau.\n\n"
                                         "## Premiere page\n\n"
                                         "Du texte.\n\n"
                                         ":: essai | demi_tons:7\n\n"
                                         "## Deuxieme page\n\n"
                                         "Encore du texte.\n";

    const std::optional<musichien::domain::Course> course = readCourse( CONTENT );

    ASSERT_TRUE( course.has_value() );
    ASSERT_EQ( course->sections.size(), 3U );

    // Le CHAPEAU n'a pas de titre, et c'est voulu : c'est ce qui precede le premier « ## ».
    EXPECT_EQ( course->sections.at( 0 ).title, "" );
    EXPECT_EQ( course->sections.at( 1 ).title, "Premiere page" );
    EXPECT_EQ( course->sections.at( 2 ).title, "Deuxieme page" );

    // La deuxieme page porte DEUX blocs, et ils sont bien a elle : le titre ne les a pas avales.
    ASSERT_EQ( course->sections.at( 1 ).blocks.size(), 2U );
    EXPECT_EQ( course->sections.at( 1 ).blocks.at( 0 ).kind, musichien::domain::CourseBlock::Kind::Text );
    EXPECT_EQ( course->sections.at( 1 ).blocks.at( 1 ).kind, musichien::domain::CourseBlock::Kind::TryExercise );

    // ET LA LISTE PLATE EST INTACTE : quatre blocs, dans l'ordre du fichier, titres exclus.
    EXPECT_EQ( course->blocks.size(), 4U );
}

TEST( MarkdownCourseTest, a_singing_card_carries_its_distance )
{
    // « :: chante » est une PORTE, et non une donnee : elle ne porte qu'une distance, et l'ecran decide ou elle mene.
    // C'est ce qui permettra de deplacer l'outil de chant sans toucher un seul cours.
    const std::optional<musichien::domain::Course> course = readCourse( ":: chante | demi_tons:7 | Chante le sol\n" );

    ASSERT_TRUE( course.has_value() );
    ASSERT_EQ( course->blocks.size(), 1U );

    EXPECT_EQ( course->blocks.at( 0 ).kind, musichien::domain::CourseBlock::Kind::SingInterval );
    EXPECT_EQ( course->blocks.at( 0 ).semitones, 7 );
    EXPECT_EQ( course->blocks.at( 0 ).caption, "Chante le sol" );
}

// ":: image" PORTE UN NOM DE FICHIER, JAMAIS UNE ADRESSE - et c'est une regle, pas un gout.
//
// L'application n'a pas la permission d'acces au reseau : c'est la charte du projet. Une image venue d'une adresse ne
// s'afficherait donc pas, et le telephone ne le dirait pas non plus - juste un trou dans la page, que personne ne sait
// expliquer. L'image voyage avec le binaire, comme les cours et les indices.
//
// Le nom est celui du fichier, SANS dossier ni extension : le chemin se construit a l'ecran, la ou l'on sait ou vivent
// les images. Un cours qui ecrirait « qrc:/... » casserait le jour ou elles demenagent.
TEST( MarkdownCourseTest, an_image_card_carries_a_file_name_and_its_caption )
{
    const std::optional<musichien::domain::Course> course =
      readCourse( ":: image | pythagore-forgerons | Pythagore et les forgerons\n" );

    ASSERT_TRUE( course.has_value() );
    ASSERT_EQ( course->blocks.size(), 1U );

    EXPECT_EQ( course->blocks.at( 0 ).kind, musichien::domain::CourseBlock::Kind::Image );
    EXPECT_EQ( course->blocks.at( 0 ).imageName, "pythagore-forgerons" );
    EXPECT_EQ( course->blocks.at( 0 ).caption, "Pythagore et les forgerons" );
}

// ":: serie" N'A BESOIN QUE D'UNE LEGENDE. Ce qu'elle joue ne se parametre pas : c'est LA serie harmonique, toujours la
// meme, et un cours n'a pas a choisir jusqu'a quel rang. Six rangs, parce que le septieme est faux - et cette decision
// appartient au jeu, pas au fichier de contenu.
TEST( MarkdownCourseTest, a_series_card_needs_only_a_caption )
{
    const std::optional<musichien::domain::Course> course =
      readCourse( ":: serie | ecoute la serie harmonique d'un do\n" );

    ASSERT_TRUE( course.has_value() );
    ASSERT_EQ( course->blocks.size(), 1U );

    EXPECT_EQ( course->blocks.at( 0 ).kind, musichien::domain::CourseBlock::Kind::HarmonicSeries );
    EXPECT_EQ( course->blocks.at( 0 ).caption, "ecoute la serie harmonique d'un do" );
}

// ":: bourdon" N'A BESOIN QUE D'UNE LEGENDE, comme la serie : ce qu'il fait entendre est LA formule du jeu - la
// tonique et sa quinte, sans tierce - et un cours n'a pas a la choisir.
TEST( MarkdownCourseTest, a_drone_card_needs_only_a_caption )
{
    const std::optional<musichien::domain::Course> course =
      readCourse( ":: bourdon | le bourdon du jeu : le do, et sa quinte\n" );

    ASSERT_TRUE( course.has_value() );
    ASSERT_EQ( course->blocks.size(), 1U );

    EXPECT_EQ( course->blocks.at( 0 ).kind, musichien::domain::CourseBlock::Kind::Drone );
    EXPECT_EQ( course->blocks.at( 0 ).caption, "le bourdon du jeu : le do, et sa quinte" );
}

}    // namespace musichien::infrastructure
