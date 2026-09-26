#include "domain/exercise/HintBook.h"

#include <utility>

namespace musichien::domain
{

void HintBook::add( const Interval & p_interval, IntervalDirection p_direction, IntervalHint p_hint )
{
    // insert_or_assign rather than insert: a later entry replaces an earlier one for the same pair.
    // Content files get edited, and an edit that silently did nothing would be a mystery to whoever made
    // it.
    m_hints.insert_or_assign( { p_interval.semitones(), p_direction }, std::move( p_hint ) );
}

std::optional<IntervalHint> HintBook::hintFor( const Interval & p_interval,
                                               IntervalDirection p_direction ) const
{
    const IntervalDirection lookedUpDirection =
      ( p_direction == IntervalDirection::Harmonic ) ? IntervalDirection::Ascending : p_direction;

    const auto hint = m_hints.find( { p_interval.semitones(), lookedUpDirection } );

    if( hint == m_hints.end() )
    {
        // A hint is content, and content is always incomplete: an interval without a hint is a gap to
        // fill, not a mistake to report. Hence an empty optional rather than an assertion.
        return std::nullopt;
    }

    return hint->second;
}

}    // namespace musichien::domain