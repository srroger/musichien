#include "domain/music/Chord.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <set>
#include <vector>

namespace musichien::domain
{

// ---------------------------------------------------------------------------------------------------------------------
// Les accords, comme regles
//
// Tout ce qui compte ici est une liste de distances depuis une tonique : il n'y a rien a ecouter, rien a mesurer, et
// donc rien qui puisse echouer une fois sur cinq. Ce fichier doit rester de ce genre-la.
// ---------------------------------------------------------------------------------------------------------------------

namespace
{

// Les memes donnees, ecrites une fois de plus - a la main, et expres.
//
// Un test qui lirait chordIntervals pour verifier chordIntervals ne verifierait rien : il faut une deuxieme source,
// humaine et lisible, pour que la comparaison ait un sens. C'est exactement le role de ce tableau.
struct ExpectedChord
{
    ChordQuality quality;
    std::vector<std::int32_t> semitones;
};

const std::vector<ExpectedChord> & expectedChords()
{
    static const std::vector<ExpectedChord> chords{
      // Les six triades : deux couleurs de base, deux suspendues, deux tendues.
      { ChordQuality::Major, { 0, 4, 7 } },
      { ChordQuality::Minor, { 0, 3, 7 } },
      { ChordQuality::Sus4, { 0, 5, 7 } },
      { ChordQuality::Sus2, { 0, 2, 7 } },
      { ChordQuality::Diminished, { 0, 3, 6 } },
      { ChordQuality::Augmented, { 0, 4, 8 } },

      // Les trois septiemes que l'oreille rencontre partout.
      { ChordQuality::DominantSeventh, { 0, 4, 7, 10 } },
      { ChordQuality::MajorSeventh, { 0, 4, 7, 11 } },
      { ChordQuality::MinorSeventh, { 0, 3, 7, 10 } },

      // Puis les couleurs plus riches, ou un seul demi-ton separe parfois deux accords.
      { ChordQuality::Sixth, { 0, 4, 7, 9 } },
      { ChordQuality::HalfDiminished, { 0, 3, 6, 10 } },
      { ChordQuality::DiminishedSeventh, { 0, 3, 6, 9 } },
      { ChordQuality::MinorMajorSeventh, { 0, 3, 7, 11 } },

      // Et les deux qui sortent de l'octave - dont la seule du jeu a porter CINQ notes.
      { ChordQuality::Add9, { 0, 4, 7, 14 } },
      { ChordQuality::Ninth, { 0, 4, 7, 10, 14 } },
    };

    return chords;
}

}    // namespace

TEST( ChordTest, every_quality_has_the_intervals_a_musician_would_write )
{
    ASSERT_EQ( CHORD_QUALITY_COUNT, expectedChords().size() );

    for( const ExpectedChord & expected : expectedChords() )
    {
        const std::span<const std::int32_t> intervals = chordIntervals( expected.quality );

        EXPECT_EQ( expected.semitones.size(), intervals.size() ) << chordQualityName( expected.quality );

        for( std::size_t index = 0; index < intervals.size(); ++index )
        {
            EXPECT_EQ( expected.semitones.at( index ), intervals[index] )
              << chordQualityName( expected.quality ) << ", note " << index;
        }
    }
}

TEST( ChordTest, every_chord_starts_on_its_root_and_goes_up )
{
    for( const ChordQuality quality : chordLearningOrder() )
    {
        const std::span<const std::int32_t> intervals = chordIntervals( quality );

        ASSERT_FALSE( intervals.empty() ) << chordQualityName( quality );

        // La tonique est la premiere note, toujours : c'est la convention du projet, et sans elle l'exercice de
        // construction (retrouver les intervalles depuis la tonique) n'aurait pas de point de depart.
        EXPECT_EQ( CHORD_ROOT_SEMITONES, intervals.front() ) << chordQualityName( quality );

        for( std::size_t index = 1; index < intervals.size(); ++index )
        {
            EXPECT_GT( intervals[index], intervals[index - 1] ) << chordQualityName( quality );
        }
    }
}

TEST( ChordTest, the_number_of_notes_goes_from_three_to_five_and_never_beyond )
{
    for( const ChordQuality quality : chordLearningOrder() )
    {
        const std::size_t notes = chordNoteCount( quality );

        EXPECT_EQ( chordIntervals( quality ).size(), notes );

        // Trois au minimum - c'est ce qui fait un accord - et cinq au maximum : la neuvieme est la seule du jeu a en
        // avoir cinq, et au-dela l'oreille ne compte plus les notes, elle entend une masse.
        EXPECT_GE( notes, 3U ) << chordQualityName( quality );
        EXPECT_LE( notes, 5U ) << chordQualityName( quality );
    }

    // Les trois reperes du parcours : la triade, la septieme, et la seule a cinq notes.
    EXPECT_EQ( 3U, chordNoteCount( ChordQuality::Major ) );
    EXPECT_EQ( 4U, chordNoteCount( ChordQuality::DominantSeventh ) );
    EXPECT_EQ( 5U, chordNoteCount( ChordQuality::Ninth ) );
}

TEST( ChordTest, the_learning_order_holds_every_quality_exactly_once )
{
    const std::span<const ChordQuality> order = chordLearningOrder();

    EXPECT_EQ( CHORD_QUALITY_COUNT, order.size() );

    // Toute qualite est dans la liste, et une seule fois. Une liste ecrite a la main est une liste qui derive : une
    // qualite absente serait une couleur que le joueur n'entendrait jamais, et rien d'autre ne le dirait.
    std::set<ChordQuality> seen;

    for( const ChordQuality quality : order )
    {
        EXPECT_TRUE( seen.insert( quality ).second ) << chordQualityName( quality );
    }

    EXPECT_EQ( CHORD_QUALITY_COUNT, seen.size() );

    // L'ordre suit l'enumeration, qui est elle-meme l'ordre de difficulte : c'est ce qui permet a une palette d'etre
    // un simple PREFIXE, et a un rang de vouloir dire "plus difficile".
    for( std::size_t index = 0; index < order.size(); ++index )
    {
        EXPECT_EQ( index, static_cast<std::size_t>( order[index] ) );
    }

    // Et les deux premieres sont les deux couleurs de base : c'est ce qu'un debutant entend en premier, et c'est ce
    // que Roger a demande ("majeur, mineur" d'abord).
    EXPECT_EQ( ChordQuality::Major, order.front() );
    EXPECT_EQ( ChordQuality::Minor, order[1] );
}

TEST( ChordTest, the_palette_is_a_prefix_of_the_order_and_never_empty )
{
    // Un debutant commence par deux couleurs, majeur et mineur.
    const std::vector<ChordQuality> beginner = beginnerChordPalette( 2 );

    ASSERT_EQ( 2U, beginner.size() );
    EXPECT_EQ( ChordQuality::Major, beginner.at( 0 ) );
    EXPECT_EQ( ChordQuality::Minor, beginner.at( 1 ) );

    // Zero, ou un nombre absurde, ne donne pas une palette vide : une question qui n'offrirait rien a choisir ne
    // serait pas une question.
    EXPECT_EQ( 1U, beginnerChordPalette( 0 ).size() );

    // Et demander plus que l'ordre n'en contient rend l'ordre entier, jamais davantage.
    EXPECT_EQ( CHORD_QUALITY_COUNT, beginnerChordPalette( 999 ).size() );
    EXPECT_EQ( CHORD_QUALITY_COUNT, beginnerChordPalette( CHORD_QUALITY_COUNT ).size() );
}

TEST( ChordTest, the_notes_of_a_chord_are_its_root_transposed_by_its_intervals )
{
    const Chord chord{ ChordQuality::Minor, 60 };

    const std::vector<Note> notes = chord.notes();

    ASSERT_EQ( 3U, notes.size() );

    // Do mineur : do, mib, sol. Le mi bemol est bien la, et le mi naturel n'y est pas : c'est la tierce mineure qui se
    // joue, et une erreur d'un demi-ton serait inaudible a l'ecriture et parfaitement audible a l'oreille.
    EXPECT_EQ( 60, notes.at( 0 ).midiNumber() );
    EXPECT_EQ( 63, notes.at( 1 ).midiNumber() );
    EXPECT_EQ( 67, notes.at( 2 ).midiNumber() );

    // Et une septieme de dominante a quatre notes, la plus aigue a dix demi-tons de la tonique.
    const std::vector<Note> seventh = Chord{ ChordQuality::DominantSeventh, 60 }.notes();

    ASSERT_EQ( 4U, seventh.size() );
    EXPECT_EQ( 70, seventh.at( 3 ).midiNumber() );
}

TEST( ChordTest, every_quality_has_a_short_label_that_fits_a_phone )
{
    std::set<std::string_view> labels;

    for( const ChordQuality quality : chordLearningOrder() )
    {
        const std::string_view label = chordQualityShortLabel( quality );

        EXPECT_FALSE( label.empty() ) << chordQualityName( quality );

        // COURT, et c'est la contrainte qui compte : quinze etiquettes doivent tenir sur l'ecran d'un telephone, en
        // trois colonnes. Six caracteres est la limite qu'on s'est donnee, et "Half-diminished" n'y entrerait pas.
        EXPECT_LE( label.size(), 6U ) << chordQualityName( quality );

        // Et AUCUN doublon : deux accords qui partageraient leur etiquette seraient deux reponses pour un seul bouton,
        // donc une question a laquelle on ne peut pas repondre.
        EXPECT_TRUE( labels.insert( label ).second ) << label << " est utilise deux fois";
    }

    EXPECT_EQ( CHORD_QUALITY_COUNT, labels.size() );
}

TEST( ChordTest, a_name_is_readable_and_a_suffix_is_stuck_to_the_root )
{
    EXPECT_EQ( "Major", chordQualityName( ChordQuality::Major ) );
    EXPECT_EQ( "Minor", chordQualityName( ChordQuality::Minor ) );

    // Le majeur ne s'annonce pas : "C" EST un do majeur, et un suffixe vide dit exactement ca.
    EXPECT_TRUE( chordQualitySymbolSuffix( ChordQuality::Major ).empty() );
    EXPECT_EQ( "m", chordQualitySymbolSuffix( ChordQuality::Minor ) );
    EXPECT_EQ( "7", chordQualitySymbolSuffix( ChordQuality::DominantSeventh ) );

    // Aucune qualite n'est muette : un ecran qui n'aurait rien a afficher serait un ecran qui invente.
    for( const ChordQuality quality : chordLearningOrder() )
    {
        EXPECT_FALSE( chordQualityName( quality ).empty() ) << static_cast<std::size_t>( quality );
    }
}

}    // namespace musichien::domain
