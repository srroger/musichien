#include "infrastructure/content/JsonAnecdoteBook.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace musichien::infrastructure
{

namespace
{

// The kind names as they appear in the file, mapped to the domain values. An unknown kind is not an error: the
// entry is simply skipped, and the rest of the file still reads.
[[nodiscard]] std::optional<domain::AnecdoteKind> kindFrom( const std::string & p_name )
{
    if( p_name == "lore" )
    {
        return domain::AnecdoteKind::Lore;
    }

    if( p_name == "astuce" )
    {
        return domain::AnecdoteKind::Tip;
    }

    if( p_name == "acoustique" )
    {
        return domain::AnecdoteKind::Acoustics;
    }

    if( p_name == "histoire" )
    {
        return domain::AnecdoteKind::History;
    }

    if( p_name == "theorie" )
    {
        return domain::AnecdoteKind::Theory;
    }

    return std::nullopt;
}

}    // namespace

domain::AnecdoteBook readAnecdoteBook( std::string_view p_json )
{
    domain::AnecdoteBook book;

    const nlohmann::json document = nlohmann::json::parse( p_json, nullptr, false );

    // A malformed file is a file someone is still editing: nothing is reported, nothing is lost but the anecdotes.
    if( document.is_discarded() || !document.contains( "anecdotes" ) || !document.at( "anecdotes" ).is_array() )
    {
        return book;
    }

    for( const nlohmann::json & entry : document.at( "anecdotes" ) )
    {
        const auto text = entry.find( "texte" );
        const auto kind = entry.find( "type" );

        if( ( text == entry.end() ) || !text->is_string() || ( kind == entry.end() ) || !kind->is_string() )
        {
            continue;
        }

        const std::optional<domain::AnecdoteKind> parsedKind = kindFrom( kind->get<std::string>() );

        if( parsedKind.has_value() )
        {
            book.add( *parsedKind, text->get<std::string>() );
        }
    }

    return book;
}

}    // namespace musichien::infrastructure
