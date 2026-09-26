#include "domain/music/Interval.h"

#include <algorithm>
#include <ranges>


namespace musichien::domain
{

namespace
{

// Definition of one simple interval.
//
// Note that this struct has NO prefix on its members: it is a plain data aggregate, and the
// convention deliberately keeps public members unprefixed. See docs/CODE_CONVENTIONS.md.
struct IntervalDefinition
{
    IntervalQuality  quality;
    std::string_view shortIdentifier;
    std::string_view longName;
};

// The twelve simple intervals, indexed by their number of semitones.
// Designated initializers are used on purpose: the order of the members cannot be mixed up.
constexpr std::array< IntervalDefinition, SEMITONES_PER_OCTAVE > SIMPLE_INTERVAL_DEFINITIONS{
    IntervalDefinition{ .quality = IntervalQuality::Perfect, .shortIdentifier = "P1", .longName = "Perfect unison" },
    IntervalDefinition{ .quality = IntervalQuality::Minor, .shortIdentifier = "m2", .longName = "Minor second" },
    IntervalDefinition{ .quality = IntervalQuality::Major, .shortIdentifier = "M2", .longName = "Major second" },
    IntervalDefinition{ .quality = IntervalQuality::Minor, .shortIdentifier = "m3", .longName = "Minor third" },
    IntervalDefinition{ .quality = IntervalQuality::Major, .shortIdentifier = "M3", .longName = "Major third" },
    IntervalDefinition{ .quality = IntervalQuality::Perfect, .shortIdentifier = "P4", .longName = "Perfect fourth" },
    IntervalDefinition{ .quality = IntervalQuality::Augmented, .shortIdentifier = "A4", .longName = "Augmented fourth" },
    IntervalDefinition{ .quality = IntervalQuality::Perfect, .shortIdentifier = "P5", .longName = "Perfect fifth" },
    IntervalDefinition{ .quality = IntervalQuality::Minor, .shortIdentifier = "m6", .longName = "Minor sixth" },
    IntervalDefinition{ .quality = IntervalQuality::Major, .shortIdentifier = "M6", .longName = "Major sixth" },
    IntervalDefinition{ .quality = IntervalQuality::Minor, .shortIdentifier = "m7", .longName = "Minor seventh" },
    IntervalDefinition{ .quality = IntervalQuality::Major, .shortIdentifier = "M7", .longName = "Major seventh" }
};


// Builds the table of the twelve simple intervals. constexpr rather than a run time initialisation:
// the answer choices of an exercise must never be recomputed while the player is waiting.
[[nodiscard]] constexpr std::array< Interval, SEMITONES_PER_OCTAVE > buildSimpleIntervals() noexcept
{
    std::array< Interval, SEMITONES_PER_OCTAVE > intervals{};

    // std::views::iota replaces "for (int index = 0; ...)" with something that cannot go wrong.
    for ( const std::int32_t semitones : std::views::iota( 0, SEMITONES_PER_OCTAVE ) )
    {
        const auto definitionIndex = static_cast< std::size_t >( semitones );

        intervals.at( definitionIndex ) = Interval{ semitones,
                                                    SIMPLE_INTERVAL_DEFINITIONS.at( definitionIndex ).quality };
    }

    return intervals;
}


constexpr std::array< Interval, SEMITONES_PER_OCTAVE > SIMPLE_INTERVALS{ buildSimpleIntervals() };

}    // namespace


std::string Interval::name() const
{
    // m_semitones is already normalised by the constructor, so it can index the reference table
    // directly. No bounds check is needed beyond the one that std::array::at already performs.
    const auto definitionIndex = static_cast< std::size_t >( m_semitones );

    return std::string{ SIMPLE_INTERVAL_DEFINITIONS.at( definitionIndex ).longName };
}

std::string_view Interval::identifier() const noexcept
{
    const auto definitionIndex = static_cast< std::size_t >( m_semitones );

    return SIMPLE_INTERVAL_DEFINITIONS.at( definitionIndex ).shortIdentifier;
}


Interval intervalBetween( const Note & p_firstNote, const Note & p_secondNote )
{
    const std::int32_t semitoneDistance = distanceInSemitones( p_firstNote, p_secondNote );

    // The constructor normalises the distance, so its sign never has to be handled here.
    const Interval normalisedInterval{ semitoneDistance, IntervalQuality::Perfect };

    return intervalFromSemitones( normalisedInterval.semitones() );
}

Interval intervalFromSemitones( std::int32_t p_semitones )
{
    // Reducing first is what makes the lookup always succeed, whatever the sign of the distance.
    const std::int32_t simpleSemitones = semitonesInSimpleForm( p_semitones );
    const auto definitionIndex = static_cast< std::size_t >( simpleSemitones );

    return Interval{ simpleSemitones, SIMPLE_INTERVAL_DEFINITIONS.at( definitionIndex ).quality };
}


std::vector< Interval > intervalsOf( std::span< const Note > p_notes )
{
    if ( p_notes.size() < 2 )
    {
        return {};
    }

    std::vector< Interval > intervals;
    intervals.reserve( p_notes.size() - 1 );

    // A sliding window of two notes: no index, no manual increment, no off-by-one possible.
    for ( const auto & notePair : p_notes | std::views::adjacent< 2 > )
    {
        const auto & [ firstNote, secondNote ] = notePair;

        intervals.push_back( intervalBetween( firstNote, secondNote ) );
    }

    return intervals;
}

const std::array< Interval, 12 > & allSimpleIntervals()
{
    return SIMPLE_INTERVALS;
}


}    // namespace musichien::domain
