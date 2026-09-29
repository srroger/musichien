#include "infrastructure/content/JsonTunerGuide.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>

namespace musichien::infrastructure
{

namespace
{

// The temperament names as they appear in the file, mapped to the domain values. An unknown name is not an error: the
// entry is simply skipped, and the rest of the file still reads.
[[nodiscard]] std::optional<domain::Temperament> temperamentFrom( const std::string & p_name )
{
    if( p_name == "equal" )
    {
        return domain::Temperament::Equal;
    }

    if( p_name == "pythagorean" )
    {
        return domain::Temperament::Pythagorean;
    }

    if( p_name == "just" )
    {
        return domain::Temperament::Just;
    }

    return std::nullopt;
}

// Une chaine lue d'un document, ou une chaine vide : jamais d'exception, jamais de valeur inventee.
[[nodiscard]] std::string stringOrEmpty( const nlohmann::json & p_object, const char * p_key )
{
    const auto value = p_object.find( p_key );

    if( ( value == p_object.end() ) || !value->is_string() )
    {
        return {};
    }

    return value->get<std::string>();
}

}    // namespace

domain::TunerGuide readTunerGuide( std::string_view p_json )
{
    domain::TunerGuide guide;

    const nlohmann::json document = nlohmann::json::parse( p_json, nullptr, false );

    // A malformed file is a file someone is still editing: nothing is reported, nothing is lost but the explanations.
    if( document.is_discarded() || !document.is_object() )
    {
        return guide;
    }

    const auto temperaments = document.find( "temperaments" );

    if( ( temperaments != document.end() ) && temperaments->is_array() )
    {
        for( const nlohmann::json & entry : *temperaments )
        {
            if( !entry.is_object() )
            {
                continue;
            }

            const auto parsed = temperamentFrom( stringOrEmpty( entry, "id" ) );

            if( parsed.has_value() )
            {
                guide.setTemperamentText( *parsed, stringOrEmpty( entry, "texte" ) );
            }
        }
    }

    guide.setDiapasonText( stringOrEmpty( document, "diapason" ) );
    guide.setReferenceNoteText( stringOrEmpty( document, "noteDeReference" ) );

    const auto howTo = document.find( "modeEmploi" );

    if( ( howTo != document.end() ) && howTo->is_array() )
    {
        for( const nlohmann::json & step : *howTo )
        {
            if( step.is_string() )
            {
                guide.addHowToStep( step.get<std::string>() );
            }
        }
    }

    return guide;
}

}    // namespace musichien::infrastructure
