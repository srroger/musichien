#include "infrastructure/content/JsonHintBook.h"

#include "domain/music/Interval.h"

#include <nlohmann/json.hpp>

#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>

namespace musichien::infrastructure
{

namespace
{

// The two ways of hearing an interval a hint is written for, with the key each one uses in the file.
//
// A table rather than two blocks of duplicated code: the third case - a harmonic interval - needs no
// entry, because the domain looks it up as an ascending one. See HintBook::hintFor.
struct DirectionEntry
{
    const char * key;
    domain::IntervalDirection direction;
};

constexpr std::array<DirectionEntry, 2> DIRECTION_ENTRIES{
  DirectionEntry{ .key = "ascendant", .direction = domain::IntervalDirection::Ascending },
  DirectionEntry{ .key = "descendant", .direction = domain::IntervalDirection::Descending } };

// Reads one direction of one interval, and adds the hint to the book when the file has one.
void readDirection( const nlohmann::json & p_intervalEntry,
                    const DirectionEntry & p_directionEntry,
                    const domain::Interval & p_interval,
                    domain::HintBook & p_hintBook )
{
    const auto hintEntry = p_intervalEntry.find( p_directionEntry.key );

    if( hintEntry == p_intervalEntry.end() )
    {
        return;
    }

    const auto label = hintEntry->find( "indice" );

    // A null 'indice' is not an error: it is how the file says "no hint for this one yet". Nothing is
    // reported, because nothing is wrong.
    if( ( label == hintEntry->end() ) || !label->is_string() )
    {
        return;
    }

    domain::IntervalHint hint;
    hint.label = label->get<std::string>();

    const auto songs = hintEntry->find( "chansons" );

    if( ( songs != hintEntry->end() ) && songs->is_array() )
    {
        for( const nlohmann::json & song : *songs )
        {
            if( song.is_string() )
            {
                hint.songs.push_back( song.get<std::string>() );
            }
        }
    }

    p_hintBook.add( p_interval, p_directionEntry.direction, std::move( hint ) );
}

}    // namespace

domain::HintBook readHintBook( std::string_view p_jsonText )
{
    domain::HintBook hintBook;

    // 'false' as the third argument means "do not throw": a content file is edited by hand, and a
    // missing comma must not be able to prevent the application from starting.
    const nlohmann::json document = nlohmann::json::parse( p_jsonText, nullptr, false );

    if( document.is_discarded() )
    {
        std::cerr << "Musichien: the interval hints are not valid JSON. The game continues without them.\n";

        return hintBook;
    }

    const auto intervalEntries = document.find( "intervalles" );

    if( ( intervalEntries == document.end() ) || !intervalEntries->is_array() )
    {
        std::cerr << "Musichien: the interval hints have no 'intervalles' list. The game continues "
                     "without them.\n";

        return hintBook;
    }

    for( const nlohmann::json & intervalEntry : *intervalEntries )
    {
        const auto semitones = intervalEntry.find( "demi_tons" );

        // 'demi_tons' is the key the domain understands; 'id' is only a label for the human editing the
        // file, and is deliberately ignored here.
        if( ( semitones == intervalEntry.end() ) || !semitones->is_number_integer() )
        {
            std::cerr << "Musichien: an interval hint has no usable 'demi_tons' and was skipped.\n";

            continue;
        }

        const domain::Interval interval = domain::intervalFromSemitones( semitones->get<std::int32_t>() );

        for( const DirectionEntry & directionEntry : DIRECTION_ENTRIES )
        {
            readDirection( intervalEntry, directionEntry, interval, hintBook );
        }
    }

    return hintBook;
}

}    // namespace musichien::infrastructure
