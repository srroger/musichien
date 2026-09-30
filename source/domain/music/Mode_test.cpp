#include "domain/music/Mode.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <set>
#include <span>
#include <vector>

namespace musichien::domain
{

namespace
{

// Les classes de hauteur d'une suite de notes : c'est ce qui permet de comparer deux gammes SANS se soucier de
// l'octave, donc de dire « ce sont bien les memes notes ». Un mode se reconnait a ses classes, pas a ses hauteurs.
[[nodiscard]] std::set<std::int32_t> pitchClassesOf( std::span<const Note> p_notes )
{
    std::set<std::int32_t> classes;

    for( const Note & note : p_notes )
    {
        classes.insert( note.pitchClassIndex() );
    }

    return classes;
}

}    // namespace

TEST( ModeTest, every_mode_has_seven_rising_degrees_inside_one_octave )
{
    for( std::size_t index = 0; index < MODE_COUNT; ++index )
    {
        const auto mode = static_cast<Mode>( index );

        const std::array<std::int32_t, DEGREE_COUNT> offsets = modeDegreeOffsets( mode );

        EXPECT_EQ( 0, offsets.front() ) << modeIdentifier( mode );

        for( std::size_t degree = 1; degree < DEGREE_COUNT; ++degree )
        {
            EXPECT_LT( offsets.at( degree - 1 ), offsets.at( degree ) ) << modeIdentifier( mode );
            EXPECT_LT( offsets.at( degree ), SEMITONES_PER_OCTAVE ) << modeIdentifier( mode );
        }
    }
}

TEST( ModeTest, the_ionian_is_the_major_scale_and_the_aeolian_the_natural_minor_one )
{
    EXPECT_EQ( MAJOR_SCALE_DEGREE_OFFSETS, modeDegreeOffsets( Mode::Ionian ) );

    const std::array<std::int32_t, DEGREE_COUNT> aeolian{ 0, 2, 3, 5, 7, 8, 10 };

    EXPECT_EQ( aeolian, modeDegreeOffsets( Mode::Aeolian ) );
}

TEST( ModeTest, a_mode_is_the_major_scale_seen_from_another_of_its_degrees )
{
    // Le point du §3.2 de la note 27 : les sept modes de la gamme de do sont la MEME gamme, vue depuis chacune de ses
    // sept notes. Les sept toniques sont donc les sept notes de do majeur, et chacune doit rendre le meme jeu de
    // classes de hauteur - sinon un mode ne serait plus une rotation de la gamme majeure.
    const std::array<std::int32_t, MODE_COUNT> tonicMidiNumbers{ 65, 60, 67, 62, 69, 64, 71 };    // F C G D A E B

    const std::set<std::int32_t> reference = pitchClassesOf( notesOfMode( Note{ 60 }, Mode::Ionian ) );

    for( std::size_t index = 0; index < MODE_COUNT; ++index )
    {
        const auto mode = static_cast<Mode>( index );

        const std::vector<Note> notes = notesOfMode( Note{ tonicMidiNumbers.at( index ) }, mode );

        EXPECT_EQ( reference, pitchClassesOf( notes ) ) << modeIdentifier( mode );
    }
}

TEST( ModeTest, d_dorian_holds_exactly_the_notes_of_c_major )
{
    // Le piege du §4 de la note 27, verifie ici comme une EGALITE : do dorien, si bemol majeur et sol mineur
    // partagent les memes notes. Ce qui les separe n'est pas dans ce test - c'est le CENTRE, donc le bourdon, et cela
    // ne se demontre pas avec des ensembles de notes.
    const std::set<std::int32_t> dorianOfD = pitchClassesOf( notesOfMode( Note{ 62 }, Mode::Dorian ) );

    EXPECT_EQ( pitchClassesOf( notesOfMode( Note{ 60 }, Mode::Ionian ) ), dorianOfD );

    // Et la meme chose vue depuis do : do dorien ne contient PAS les notes de do majeur - il en perd une et en gagne
    // une autre. C'est un mode different, pas une autre facon d'ecrire le meme.
    EXPECT_NE( pitchClassesOf( notesOfMode( Note{ 60 }, Mode::Dorian ) ),
               pitchClassesOf( notesOfMode( Note{ 60 }, Mode::Ionian ) ) );
}

TEST( ModeTest, two_neighbouring_modes_in_the_colour_order_differ_by_a_single_note )
{
    // C'est la propriete qui justifie l'ordre de l'enumeration, donc celle de tout l'exercice du degrade : descendre la
    // liste eteint une note et en rallume une autre, et l'ecart s'entend en UN SEUL geste. Si cette propriete tombe,
    // l'ordre de couleur n'est plus qu'une preference.
    for( std::size_t index = 0; index + 1 < MODE_COUNT; ++index )
    {
        const std::set<std::int32_t> lower = pitchClassesOf( notesOfMode( Note{ 60 }, static_cast<Mode>( index ) ) );

        const std::set<std::int32_t> higher = pitchClassesOf( notesOfMode( Note{ 60 }, static_cast<Mode>( index + 1 ) ) );

        std::vector<std::int32_t> onlyInLower;
        std::vector<std::int32_t> onlyInHigher;

        std::ranges::set_difference( lower, higher, std::back_inserter( onlyInLower ) );
        std::ranges::set_difference( higher, lower, std::back_inserter( onlyInHigher ) );

        EXPECT_EQ( 1, onlyInLower.size() ) << modeIdentifier( static_cast<Mode>( index ) );
        EXPECT_EQ( 1, onlyInHigher.size() ) << modeIdentifier( static_cast<Mode>( index ) );
    }
}

TEST( ModeTest, an_identifier_round_trips_and_an_unknown_one_designs_nothing )
{
    for( std::size_t index = 0; index < MODE_COUNT; ++index )
    {
        const auto mode = static_cast<Mode>( index );

        const std::optional<Mode> read = modeFromIdentifier( modeIdentifier( mode ) );

        ASSERT_TRUE( read.has_value() ) << modeIdentifier( mode );
        EXPECT_EQ( mode, read.value() );
    }

    // Un fichier de contenu est un fichier qu'un humain peut ecrire : un identifiant inconnu doit couter la phrase
    // concernee, jamais l'application.
    EXPECT_FALSE( modeFromIdentifier( "doriann" ).has_value() );
    EXPECT_FALSE( modeFromIdentifier( "" ).has_value() );
}

TEST( ModeTest, the_characteristic_note_is_where_a_mode_is_told_apart )
{
    // Les notes qui colorent, verifiees sur les degres : la quarte augmentee du lydien, la septieme mineure du
    // mixolydien, la sixte majeure du dorien, la seconde mineure du phrygien, la quinte diminuee du locrien.
    EXPECT_EQ( 6, degreeOffset( Mode::Lydian, 4 ) );
    EXPECT_EQ( 10, degreeOffset( Mode::Mixolydian, 7 ) );
    EXPECT_EQ( 9, degreeOffset( Mode::Dorian, 6 ) );
    EXPECT_EQ( 1, degreeOffset( Mode::Phrygian, 2 ) );
    EXPECT_EQ( 6, degreeOffset( Mode::Locrian, 5 ) );

    // Et les deux modes de reference n'ont rien a demontrer : c'est justement pour cela qu'ils servent de reference.
    EXPECT_EQ( 4, degreeOffset( Mode::Ionian, 3 ) );
    EXPECT_EQ( 3, degreeOffset( Mode::Aeolian, 3 ) );
}

TEST( ModeTest, moving_the_mode_towards_the_bright_and_the_tonic_down_keeps_the_same_notes )
{
    // La regle sur laquelle un VAMP est bati : monter d'un cran vers le clair, c'est poser la MEME gamme un demi-ton
    // plus bas. Do dorien et si bemol majeur ont exactement les memes sept notes.
    EXPECT_EQ( pitchClassesOf( notesOfMode( Note{ 60 }, Mode::Dorian ) ),
               pitchClassesOf( notesOfMode( Note{ 58 }, Mode::Ionian ) ) );

    // Et l'autre sens, evidemment : descendre vers le sombre, c'est monter la tonique.
    EXPECT_EQ( pitchClassesOf( notesOfMode( Note{ 58 }, Mode::Ionian ) ),
               pitchClassesOf( notesOfMode( Note{ 60 }, Mode::Dorian ) ) );

    // Un pas de DEUX, et pas seulement d'un demi-ton : re mixolydien et do lydien ont les memes notes.
    EXPECT_EQ( pitchClassesOf( notesOfMode( Note{ 62 }, Mode::Mixolydian ) ),
               pitchClassesOf( notesOfMode( Note{ 60 }, Mode::Lydian ) ) );
}

TEST( ModeTest, two_neighbouring_modes_differ_by_exactly_one_note )
{
    // La propriete du §2.2 de la note 27, verifiee ici parce que le domaine s'en sert maintenant pour DIRE un verdict :
    // « la tierce a monte d'un demi-ton » n'a de sens que si une seule note a bouge.
    for( std::size_t index = 0; index + 1 < MODE_COUNT; ++index )
    {
        const ModeDifference difference = modeDifference( static_cast<Mode>( index ), static_cast<Mode>( index + 1 ) );

        EXPECT_NE( 0, difference.degree ) << modeIdentifier( static_cast<Mode>( index ) );
        EXPECT_EQ( 1, std::abs( difference.semitones ) ) << modeIdentifier( static_cast<Mode>( index ) );
    }

    // Et deux fois le meme mode ne different de rien du tout.
    EXPECT_EQ( 0, modeDifference( Mode::Dorian, Mode::Dorian ).degree );
}

TEST( ModeTest, the_learning_order_holds_every_mode_exactly_once_and_starts_with_the_known_ones )
{
    const std::span<const Mode> order = modeLearningOrder();

    ASSERT_EQ( MODE_COUNT, order.size() );

    std::set<std::size_t> seen;

    for( const Mode mode : order )
    {
        seen.insert( modeIndex( mode ) );
    }

    // L'invariant : chaque mode y figure EXACTEMENT UNE FOIS. Une liste ecrite a la main derive, et un mode absent
    // serait un mode qu'on ne pourrait jamais apprendre - sans que rien d'autre ne le signale.
    EXPECT_EQ( MODE_COUNT, seen.size() );

    // Le majeur et le mineur d'abord, et ENSEMBLE : c'est le seul ecart que l'oreille connait deja.
    EXPECT_EQ( Mode::Ionian, order.front() );
    EXPECT_EQ( Mode::Aeolian, order.at( 1 ) );

    // Et le locrien en dernier : c'est le seul mode ou rien ne repose.
    EXPECT_EQ( Mode::Locrian, order.back() );
}

TEST( ModeTest, a_beginner_palette_never_holds_fewer_than_two_modes )
{
    // Une question de couleur compare DEUX modes : une palette d'un seul ne pourrait pas la poser. Le domaine refuse
    // donc cet etat tout de suite, au lieu de laisser la session le decouvrir en tirant une question.
    EXPECT_EQ( 2, beginnerModePalette( 0 ).size() );
    EXPECT_EQ( 2, beginnerModePalette( 1 ).size() );
    EXPECT_EQ( 3, beginnerModePalette( 3 ).size() );

    // Et demander plus que l'ordre n'en contient rend l'ordre entier, sans jamais lever.
    EXPECT_EQ( MODE_COUNT, beginnerModePalette( 99 ).size() );
}

TEST( ModeTest, brightness_follows_the_colour_order )
{
    // Le lydien est le plus clair, le locrien le plus sombre, et la comparaison n'est qu'un rang : c'est ce qui
    // garantit qu'un exercice demandant « plus clair ou plus sombre ? » ne peut pas se tromper de sens.
    EXPECT_TRUE( isBrighterThan( Mode::Lydian, Mode::Locrian ) );
    EXPECT_TRUE( isBrighterThan( Mode::Ionian, Mode::Aeolian ) );
    EXPECT_FALSE( isBrighterThan( Mode::Aeolian, Mode::Ionian ) );

    // Un mode n'est pas plus clair que lui-meme : la comparaison est STRICTE.
    EXPECT_FALSE( isBrighterThan( Mode::Dorian, Mode::Dorian ) );
}

}    // namespace musichien::domain
