#include "infrastructure/content/JsonPhraseBook.h"

#include "domain/music/Mode.h"

#include <gtest/gtest.h>

#include <cstdint>

namespace
{

using musichien::domain::Mode;
using musichien::domain::PhraseBook;
using musichien::infrastructure::readPhraseBook;

// Une phrase de trois pas, ecrite comme le fichier de l'atelier l'ecrit.
constexpr std::string_view ONE_DORIAN_PHRASE = R"({
  "phrases": [
    { "mode": "dorian", "tonique_midi": 50, "bpm": 72,
      "degres": [ { "degre": 1, "duree": 2 }, { "degre": 3, "duree": 1 }, { "degre": 1, "duree": 2 } ] }
  ]
})";

}    // namespace

TEST( JsonPhraseBookTest, a_phrase_is_read_with_its_mode_its_tonic_and_its_degrees )
{
    const PhraseBook book = readPhraseBook( ONE_DORIAN_PHRASE );

    ASSERT_EQ( 1U, book.phraseCount() );
    ASSERT_EQ( 1U, book.phraseCountFor( Mode::Dorian ) );

    const std::span<const musichien::domain::Phrase> phrases = book.phrasesFor( Mode::Dorian );

    EXPECT_EQ( Mode::Dorian, phrases.front().mode );
    EXPECT_EQ( 50, phrases.front().tonic.midiNumber() );
    EXPECT_EQ( 72, phrases.front().bpm );
    ASSERT_EQ( 3U, phrases.front().steps.size() );
    EXPECT_EQ( 3, phrases.front().steps.at( 1 ).degree );
    EXPECT_EQ( 1, phrases.front().steps.at( 1 ).beats );
}

TEST( JsonPhraseBookTest, the_modes_are_kept_apart )
{
    // Le livre range par mode : deux phrases du meme mode ne doivent jamais se compter comme deux modes.
    const PhraseBook book = readPhraseBook( R"({
      "phrases": [
        { "mode": "dorian", "tonique_midi": 50, "degres": [ { "degre": 1, "duree": 1 }, { "degre": 3, "duree": 1 },
          { "degre": 1, "duree": 1 } ] },
        { "mode": "lydian", "tonique_midi": 50, "degres": [ { "degre": 1, "duree": 1 }, { "degre": 4, "duree": 1 },
          { "degre": 1, "duree": 1 } ] }
      ]
    })" );

    EXPECT_EQ( 2U, book.phraseCount() );
    EXPECT_EQ( 1U, book.phraseCountFor( Mode::Dorian ) );
    EXPECT_EQ( 1U, book.phraseCountFor( Mode::Lydian ) );
}

TEST( JsonPhraseBookTest, a_phrase_without_a_tempo_keeps_the_default_one )
{
    // Le tempo est le seul champ facultatif : le domaine a un defaut qui tient debout, et un fichier qui l'oublie ne
    // doit pas perdre sa phrase pour autant.
    const PhraseBook book = readPhraseBook( R"({
      "phrases": [
        { "mode": "dorian", "tonique_midi": 50,
          "degres": [ { "degre": 1, "duree": 1 }, { "degre": 3, "duree": 1 }, { "degre": 1, "duree": 1 } ] }
      ]
    })" );

    ASSERT_EQ( 1U, book.phraseCount() );
    EXPECT_EQ( musichien::domain::Phrase{}.bpm, book.phrasesFor( Mode::Dorian ).front().bpm );
}

TEST( JsonPhraseBookTest, a_broken_phrase_costs_itself_and_nothing_else )
{
    // C'est la regle du contenu : une faute de frappe coute UNE phrase, jamais l'application - et jamais ses voisines.
    const PhraseBook book = readPhraseBook( R"({
      "phrases": [
        { "mode": "dorien", "tonique_midi": 50, "degres": [ { "degre": 1, "duree": 1 } ] },
        { "mode": "lydian", "tonique_midi": 50, "degres": [ { "degre": 1, "duree": 1 }, { "degre": 4, "duree": 1 },
          { "degre": 1, "duree": 1 } ] },
        { "mode": "dorian", "tonique_midi": 50, "degres": [ { "degre": 1, "duree": 1 }, { "degre": 8, "duree": 1 },
          { "degre": 1, "duree": 1 } ] },
        { "mode": "dorian", "degres": [ { "degre": 1, "duree": 1 }, { "degre": 1, "duree": 1 },
          { "degre": 1, "duree": 1 } ] },
        { "mode": "dorian", "tonique_midi": 50, "bpm": 10000,
          "degres": [ { "degre": 1, "duree": 1 }, { "degre": 1, "duree": 1 }, { "degre": 1, "duree": 1 } ] },
        { "mode": "dorian", "tonique_midi": 50,
          "degres": [ { "degre": 1, "duree": 1 }, { "degre": 3, "duree": 1 } ] },
        { "mode": "aeolian", "tonique_midi": 300, "degres": [ { "degre": 1, "duree": 1 }, { "degre": 3, "duree": 1 },
          { "degre": 1, "duree": 1 } ] }
      ]
    })" );

    // Sept entree, une seule valable : la lydienne. Le reste est refuse, chacune pour sa raison.
    EXPECT_EQ( 1U, book.phraseCount() );
    EXPECT_EQ( 1U, book.phraseCountFor( Mode::Lydian ) );
    EXPECT_EQ( 0U, book.phraseCountFor( Mode::Dorian ) );
    EXPECT_EQ( 0U, book.phraseCountFor( Mode::Aeolian ) );
}

TEST( JsonPhraseBookTest, a_file_that_makes_no_sense_costs_the_phrases_and_nothing_more )
{
    EXPECT_EQ( 0U, readPhraseBook( "ceci n'est pas du JSON" ).phraseCount() );
    EXPECT_EQ( 0U, readPhraseBook( "{ \"autre\": [] }" ).phraseCount() );
    EXPECT_EQ( 0U, readPhraseBook( "{}" ).phraseCount() );
    EXPECT_EQ( 0U, readPhraseBook( "" ).phraseCount() );
}
