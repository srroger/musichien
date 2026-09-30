#include "domain/music/PhraseBook.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <random>

namespace
{

using namespace musichien::domain;

// Une phrase courte, dont le TEMPO sert de marque : c'est ce qui permet de dire laquelle le livre a rendue, sans
// comparer des notes.
[[nodiscard]] Phrase phraseWithTempo( Mode p_mode, std::int32_t p_bpm )
{
    Phrase phrase;
    phrase.mode = p_mode;
    phrase.bpm = p_bpm;
    phrase.steps = { PhraseStep{ .degree = 1, .beats = 2 },
                     PhraseStep{ .degree = 3, .beats = 1 },
                     PhraseStep{ .degree = 1, .beats = 1 } };

    return phrase;
}

}    // namespace

TEST( PhraseBookTest, a_mode_without_a_phrase_answers_an_empty_list )
{
    // Un mode sans phrase n'est pas une erreur : le livre dit non, et l'ecran en fait ce qu'il veut.
    const PhraseBook book;

    EXPECT_TRUE( book.phrasesFor( Mode::Dorian ).empty() );
    EXPECT_EQ( 0U, book.phraseCountFor( Mode::Dorian ) );
    EXPECT_EQ( 0U, book.phraseCount() );
}

TEST( PhraseBookTest, the_phrases_are_kept_by_mode )
{
    PhraseBook book;

    book.add( phraseWithTempo( Mode::Dorian, 72 ) );
    book.add( phraseWithTempo( Mode::Lydian, 60 ) );
    book.add( phraseWithTempo( Mode::Dorian, 80 ) );

    EXPECT_EQ( 3U, book.phraseCount() );
    EXPECT_EQ( 2U, book.phraseCountFor( Mode::Dorian ) );
    EXPECT_EQ( 1U, book.phraseCountFor( Mode::Lydian ) );
    EXPECT_TRUE( book.phrasesFor( Mode::Locrian ).empty() );

    // Et l'ordre du contenu est garde : le livre ne retrie pas ce que l'oreille a choisi.
    EXPECT_EQ( 72, book.phrasesFor( Mode::Dorian ).at( 0 ).bpm );
    EXPECT_EQ( 80, book.phrasesFor( Mode::Dorian ).at( 1 ).bpm );
}

TEST( PhraseBookTest, a_drawn_phrase_always_belongs_to_the_mode_that_was_asked )
{
    // Le tirage se fait sur les indices des phrases DU MODE : une phrase d'un autre mode serait un defaut que rien
    // n'attraperait plus tard, puisque rien en aval ne reverifie le mode.
    PhraseBook book;

    book.add( phraseWithTempo( Mode::Lydian, 66 ) );
    book.add( phraseWithTempo( Mode::Dorian, 72 ) );
    book.add( phraseWithTempo( Mode::Dorian, 74 ) );

    std::mt19937 randomEngine{ 20261002U };

    for( int draw = 0; draw < 20; ++draw )
    {
        const std::optional<Phrase> phrase = book.drawPhraseFor( Mode::Dorian, randomEngine );

        ASSERT_TRUE( phrase.has_value() );
        EXPECT_EQ( Mode::Dorian, phrase->mode );
        EXPECT_TRUE( ( phrase->bpm == 72 ) || ( phrase->bpm == 74 ) );
    }

    // Et les DEUX phrases finissent par sortir. Un tirage qui n'en rendrait qu'une serait biaise, et rien d'autre ne le
    // dirait : c'est exactement le genre de defaut qu'un livre doit avoir fini de payer une fois pour toutes.
    bool sawSlowOne = false;
    bool sawFastOne = false;

    for( int draw = 0; draw < 100; ++draw )
    {
        const std::optional<Phrase> phrase = book.drawPhraseFor( Mode::Dorian, randomEngine );

        ASSERT_TRUE( phrase.has_value() );

        sawSlowOne = sawSlowOne || ( phrase->bpm == 72 );
        sawFastOne = sawFastOne || ( phrase->bpm == 74 );
    }

    EXPECT_TRUE( sawSlowOne );
    EXPECT_TRUE( sawFastOne );
}

TEST( PhraseBookTest, drawing_from_an_empty_mode_answers_nothing )
{
    const PhraseBook book;

    std::mt19937 randomEngine{ 1U };

    EXPECT_FALSE( book.drawPhraseFor( Mode::Locrian, randomEngine ).has_value() );
}
