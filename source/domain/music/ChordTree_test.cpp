#include "domain/music/ChordTree.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <set>
#include <string>
#include <vector>

namespace musichien::domain
{

TEST( ChordTreeTest, the_degrees_say_the_same_thing_as_the_intervals )
{
    // LE TEST QUI TIENT TOUT ENSEMBLE. Les degres et les demi-tons sont deux ecritures de la meme verite, ecrites dans
    // deux fichiers : le jour ou l'une bouge sans l'autre, c'est ici que ca se voit. Une table de theorie musicale qui se
    // trompe apprend le FAUX, et personne ne s'en apercevrait a l'oreille avant des semaines.
    for( const ChordQuality quality : chordLearningOrder() )
    {
        std::vector<std::int32_t> fromDegrees;

        for( const ChordDegree & degree : chordDegrees( quality ) )
        {
            fromDegrees.push_back( semitonesOfDegree( degree ) );
        }

        const std::span<const std::int32_t> intervals = chordIntervals( quality );

        EXPECT_EQ( std::vector<std::int32_t>( intervals.begin(), intervals.end() ), fromDegrees )
          << "les degres et les intervalles divergent pour " << chordQualityName( quality );
    }
}

TEST( ChordTreeTest, every_chord_knows_its_degrees )
{
    for( const ChordQuality quality : chordLearningOrder() )
    {
        const std::span<const ChordDegree> degrees = chordDegrees( quality );

        EXPECT_FALSE( degrees.empty() ) << chordQualityName( quality );

        // La tonique est TOUJOURS la premiere, et toujours juste : c'est ce qui definit l'accord.
        EXPECT_EQ( 1, degrees.front().degree );
        EXPECT_EQ( 0, degrees.front().alteration );
        EXPECT_EQ( 0, semitonesOfDegree( degrees.front() ) );

        // Et les degres montent : un accord se lit du grave vers l'aigu.
        for( std::size_t index = 1; index < degrees.size(); ++index )
        {
            EXPECT_LT( semitonesOfDegree( degrees.at( index - 1 ) ), semitonesOfDegree( degrees.at( index ) ) )
              << chordQualityName( quality );
        }
    }
}

TEST( ChordTreeTest, the_label_reads_like_a_musician_says_it )
{
    EXPECT_EQ( "1 3 5", chordDegreesLabel( ChordQuality::Major ) );
    EXPECT_EQ( "1 b3 5", chordDegreesLabel( ChordQuality::Minor ) );
    EXPECT_EQ( "1 3 5 b7", chordDegreesLabel( ChordQuality::DominantSeventh ) );

    // Le DOUBLE bemol du diminue sept : c'est la seule ecriture juste, et l'ecrire « 6 » ferait perdre le fait que c'est
    // une septieme.
    EXPECT_EQ( "1 b3 b5 bb7", chordDegreesLabel( ChordQuality::DiminishedSeventh ) );

    EXPECT_EQ( "1 3 5 9", chordDegreesLabel( ChordQuality::Add9 ) );
}

TEST( ChordTreeTest, the_tree_branches_from_one_root_and_covers_every_colour )
{
    const std::span<const ChordNode> tree = chordTree();

    ASSERT_EQ( CHORD_QUALITY_COUNT, tree.size() );

    // UNE racine, et une seule : c'est un arbre, pas une foret.
    int rootCount = 0;

    for( const ChordNode & node : tree )
    {
        if( node.isRoot() )
        {
            ++rootCount;
        }
    }

    EXPECT_EQ( 1, rootCount );

    // Et chaque couleur y figure UNE fois : deux fois la meme, et le joueur ne saurait plus par ou passer.
    std::set<int> seen;

    for( const ChordNode & node : tree )
    {
        EXPECT_TRUE( seen.insert( static_cast<int>( node.quality ) ).second )
          << chordQualityName( node.quality ) << " apparait deux fois dans l'arbre";
    }

    EXPECT_EQ( CHORD_QUALITY_COUNT, seen.size() );
}

TEST( ChordTreeTest, a_parent_always_comes_before_its_children )
{
    const std::span<const ChordNode> tree = chordTree();

    // Le parent doit venir AVANT son enfant dans la liste : c'est ce qui permet a l'ecran de poser les noeuds en lignes,
    // et de dessiner le trait qui les relie avant l'enfant. Un arbre ou l'on remonterait la liste marcherait encore, mais
    // la lecture, elle, perdrait son sens.
    for( const ChordNode & node : tree )
    {
        if( node.isRoot() )
        {
            continue;
        }

        const auto parent = std::ranges::find_if( tree, [&node]( const ChordNode & p_candidate ) {
            return p_candidate.quality == node.parent;
        } );

        ASSERT_NE( tree.end(), parent )
          << "le parent de " << chordQualityName( node.quality ) << " n'est pas dans l'arbre";

        // Et il est a UN geste, pas a trois : une branche qui sauterait deux etapes ne montrerait pas le chemin.
        EXPECT_EQ( parent->depth + 1, node.depth )
          << chordQualityName( node.quality ) << " n'est pas a un geste de son parent";

        // Le geste est nomme : une branche sans legende n'apprend rien.
        EXPECT_FALSE( node.mutation.empty() ) << chordQualityName( node.quality );
    }
}

}    // namespace musichien::domain
