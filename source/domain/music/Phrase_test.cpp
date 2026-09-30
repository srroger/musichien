#include "domain/music/Phrase.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <random>
#include <set>
#include <vector>

namespace musichien::domain
{

TEST( PhraseTest, a_phrase_starts_and_ends_on_the_tonic )
{
    // C'est la seule chose qui fasse entendre un CENTRE : une phrase qui s'arrete ailleurs laisse l'oreille en l'air, et
    // un mode dont on ne revient pas a sa tonique n'a pas de centre du tout.
    for( std::uint32_t seed = 1; seed <= 20; ++seed )
    {
        std::mt19937 engine{ seed };

        const Phrase phrase = generatePhrase( Mode::Dorian, Note{ 60 }, engine );

        ASSERT_FALSE( phrase.steps.empty() );

        EXPECT_EQ( 1, phrase.steps.front().degree );
        EXPECT_EQ( 1, phrase.steps.back().degree );
    }
}

TEST( PhraseTest, the_characteristic_note_is_heard_early_and_long )
{
    // La note qui colore doit etre la, TOT, et LONGUEMENT : c'est elle qui porte la couleur du mode, et c'est elle qui
    // distingue une phrase modale d'une suite de notes justes.
    std::mt19937 engine{ 20261001 };

    const Phrase phrase = generatePhrase( Mode::Lydian, Note{ 60 }, engine );

    ASSERT_GE( phrase.steps.size(), 3U );

    // Le lydien se reconnait a sa quarte augmentee : le quatrieme degre, et il entre au deuxieme pas.
    EXPECT_EQ( 4, phrase.steps.at( 1 ).degree );

    // Et il DURE : une couleur posee sur une croche s'entend comme une broderie, pas comme une couleur.
    EXPECT_GE( phrase.steps.at( 1 ).beats, 2 );
}

TEST( PhraseTest, the_same_seed_always_gives_the_same_phrase )
{
    // Le generateur RECOIT sa graine, il ne la tire pas : c'est ce qui rend une phrase reproductible, donc discutable,
    // donc corrigeable. Une phrase qui changerait a chaque appel ne pourrait ni se relire ni se corriger.
    std::mt19937 firstEngine{ 42 };
    std::mt19937 secondEngine{ 42 };

    const Phrase first = generatePhrase( Mode::Phrygian, Note{ 62 }, firstEngine );
    const Phrase second = generatePhrase( Mode::Phrygian, Note{ 62 }, secondEngine );

    ASSERT_EQ( first.steps.size(), second.steps.size() );

    for( std::size_t index = 0; index < first.steps.size(); ++index )
    {
        EXPECT_EQ( first.steps.at( index ).degree, second.steps.at( index ).degree );
        EXPECT_EQ( first.steps.at( index ).beats, second.steps.at( index ).beats );
    }
}

TEST( PhraseTest, a_phrase_only_uses_the_notes_of_its_mode )
{
    // La contrainte minimale, et celle qui rend une phrase transposee sure : sept degres, sept notes, aucune autre.
    std::mt19937 engine{ 7 };

    const Phrase phrase = generatePhrase( Mode::Aeolian, Note{ 60 }, engine );

    std::set<std::int32_t> scale;

    for( const Note & note : notesOfMode( Note{ 60 }, Mode::Aeolian ) )
    {
        scale.insert( note.pitchClassIndex() );
    }

    for( const Note & note : phrase.notes( Note{ 60 } ) )
    {
        EXPECT_TRUE( scale.contains( note.pitchClassIndex() ) );
    }
}

TEST( PhraseTest, a_transposed_phrase_is_still_the_same_phrase )
{
    // Tout l'interet d'ecrire des DEGRES : la meme phrase se pose sur n'importe quelle tonique et reste elle-meme. Un
    // contenu ecrit en notes ne saurait pas le faire, et c'est cette propriete qui permettra de rejouer une phrase
    // acceptee a l'atelier dans la tonique d'un exercice.
    std::mt19937 engine{ 99 };

    const Phrase phrase = generatePhrase( Mode::Dorian, Note{ 60 }, engine );

    const std::vector<Note> atC = phrase.notes( Note{ 60 } );
    const std::vector<Note> atD = phrase.notes( Note{ 62 } );

    ASSERT_EQ( atC.size(), atD.size() );

    for( std::size_t index = 0; index < atC.size(); ++index )
    {
        EXPECT_EQ( atC.at( index ).midiNumber() + 2, atD.at( index ).midiNumber() );
    }
}

}    // namespace musichien::domain
