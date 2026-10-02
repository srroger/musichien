#include "infrastructure/content/JsonPhraseBook.h"

#include "domain/music/Mode.h"
#include "domain/music/Note.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace musichien::infrastructure
{

namespace
{

// Les bornes du contenu, et elles sont des GARDE-FOUS autant que des regles musicales.
//
// Un tempo de 10 000 rendrait des notes de six millisecondes, et une duree de 100 ferait une phrase de deux minutes :
// deux facons d'obtenir un silence incomprehensible plutot qu'une erreur lisible.
constexpr std::int32_t MINIMUM_BPM = 20;
constexpr std::int32_t MAXIMUM_BPM = 400;

constexpr std::int32_t MINIMUM_BEATS = 1;
constexpr std::int32_t MAXIMUM_BEATS = 8;

// Une phrase a besoin de trois pas : un depart, un chemin, un retour. C'est la regle du generateur, et le contenu ne
// peut pas etre plus pauvre que lui.
constexpr std::size_t MINIMUM_STEP_COUNT = 3;

// Lit les pas d'une phrase, ou rien quand un seul est invalide.
[[nodiscard]] std::optional<std::vector<domain::PhraseStep>> readSteps( const nlohmann::json & p_phraseEntry )
{
    const auto steps = p_phraseEntry.find( "degres" );

    if( ( steps == p_phraseEntry.end() ) || !steps->is_array() || ( steps->size() < MINIMUM_STEP_COUNT ) )
    {
        return std::nullopt;
    }

    std::vector<domain::PhraseStep> result;
    result.reserve( steps->size() );

    for( const nlohmann::json & step : *steps )
    {
        const auto degree = step.find( "degre" );
        const auto beats = step.find( "duree" );

        if( ( degree == step.end() ) || !degree->is_number_integer() )
        {
            return std::nullopt;
        }

        if( ( beats == step.end() ) || !beats->is_number_integer() )
        {
            return std::nullopt;
        }

        const std::int32_t degreeValue = degree->get<std::int32_t>();
        const std::int32_t beatsValue = beats->get<std::int32_t>();

        // Un degre est un chiffre de 1 a 7, comme un musicien les compte. Le domaine sait replier un degre hors bornes,
        // et c'est justement pourquoi le contenu doit etre strict : un « 8 » saisi par erreur deviendrait la tonique
        // sans que rien ne le dise.
        if( ( degreeValue < 1 ) || std::cmp_greater( degreeValue, domain::DEGREE_COUNT ) )
        {
            return std::nullopt;
        }

        if( ( beatsValue < MINIMUM_BEATS ) || ( beatsValue > MAXIMUM_BEATS ) )
        {
            return std::nullopt;
        }

        result.push_back( domain::PhraseStep{ .degree = degreeValue, .beats = beatsValue } );
    }

    return result;
}

// Lit une phrase du fichier, ou rien quand elle est illisible.
[[nodiscard]] std::optional<domain::Phrase> readPhrase( const nlohmann::json & p_phraseEntry )
{
    const auto modeName = p_phraseEntry.find( "mode" );

    if( ( modeName == p_phraseEntry.end() ) || !modeName->is_string() )
    {
        std::cerr << "Musichien: a phrase has no usable 'mode' and was skipped.\n";

        return std::nullopt;
    }

    const std::string modeText = modeName->get<std::string>();
    const std::optional<domain::Mode> mode = domain::modeFromIdentifier( modeText );

    if( !mode.has_value() )
    {
        std::cerr << "Musichien: a phrase names a mode the domain does not know, and was skipped: " << modeText
                  << "\n";

        return std::nullopt;
    }

    // La tonique est DEMANDEE, et non devinee : c'est elle qui donne son centre a la phrase, et une phrase modale sans
    // centre ne dit rien.
    const auto tonic = p_phraseEntry.find( "tonique_midi" );

    if( ( tonic == p_phraseEntry.end() ) || !tonic->is_number_integer() )
    {
        std::cerr << "Musichien: a phrase has no usable 'tonique_midi' and was skipped.\n";

        return std::nullopt;
    }

    const domain::Note tonicNote{ tonic->get<std::int32_t>() };

    if( !tonicNote.isValid() )
    {
        std::cerr << "Musichien: a phrase sits on a note outside the playable range and was skipped: "
                  << tonic->get<std::int32_t>() << "\n";

        return std::nullopt;
    }

    const std::optional<std::vector<domain::PhraseStep>> steps = readSteps( p_phraseEntry );

    if( !steps.has_value() )
    {
        std::cerr << "Musichien: a phrase has unusable degrees and was skipped.\n";

        return std::nullopt;
    }

    domain::Phrase phrase;
    phrase.mode = *mode;
    phrase.tonic = tonicNote;
    phrase.steps = *steps;

    // Le tempo est le seul champ FACULTATIF : le domaine a un defaut qui tient debout, et l'atelier l'ecrit toujours.
    const auto bpm = p_phraseEntry.find( "bpm" );

    if( ( bpm != p_phraseEntry.end() ) && bpm->is_number_integer() )
    {
        const std::int32_t bpmValue = bpm->get<std::int32_t>();

        if( ( bpmValue < MINIMUM_BPM ) || ( bpmValue > MAXIMUM_BPM ) )
        {
            std::cerr << "Musichien: a phrase has a tempo outside the playable range and was skipped: " << bpmValue
                      << "\n";

            return std::nullopt;
        }

        phrase.bpm = bpmValue;
    }

    return phrase;
}

}    // namespace

domain::PhraseBook readPhraseBook( std::string_view p_jsonText )
{
    domain::PhraseBook phraseBook;

    // 'false' en troisieme argument veut dire « ne leve pas » : un fichier de contenu est edite a la main, et une
    // virgule oubliee ne doit pas empecher l'application de demarrer.
    const nlohmann::json document = nlohmann::json::parse( p_jsonText, nullptr, false );

    if( document.is_discarded() )
    {
        std::cerr << "Musichien: the modal phrases are not valid JSON. The game continues without them.\n";

        return phraseBook;
    }

    const auto phraseEntries = document.find( "phrases" );

    if( ( phraseEntries == document.end() ) || !phraseEntries->is_array() )
    {
        std::cerr << "Musichien: the modal phrases have no 'phrases' list. The game continues without them.\n";

        return phraseBook;
    }

    for( const nlohmann::json & phraseEntry : *phraseEntries )
    {
        std::optional<domain::Phrase> phrase = readPhrase( phraseEntry );

        if( phrase.has_value() )
        {
            phraseBook.add( std::move( *phrase ) );
        }
    }

    return phraseBook;
}

}    // namespace musichien::infrastructure
