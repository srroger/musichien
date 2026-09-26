#include "domain/music/Interval.h"

#include <iterator>
#include <ranges>
#include <string>
#include <string_view>

namespace musichien::domain
{

namespace
{

// English name of an interval quality, used to build "Perfect fifth" or "Major ninth".
//
// This table is ordered by IntervalQuality, so it MUST follow the enum. An ordered array is the
// simplest thing that works here, and the declared size makes the compiler check the count.
constexpr std::array<std::string_view, 5> QUALITY_NAMES{ "Perfect",
                                                         "Major",
                                                         "Minor",
                                                         "Augmented",
                                                         "Diminished" };

// The letter musicians write in front of the number: P5, M3, m7, A4, d5.
constexpr std::array<std::string_view, 5> QUALITY_LETTERS{ "P", "M", "m", "A", "d" };

// Name of an interval number, from the unison to the fifteenth.
//
// Irregular enough that it has to be written down rather than computed: the eighth is an "octave"
// and not an "eighth", and the twelfth is a "twelfth" and not a "tenth second". Indexed by the
// number minus one.
//
// Fifteen entries is exactly what the type can produce: see MAXIMUM_INTERVAL_SEMITONES, two whole
// octaves, the widest interval the application ever uses. A test checks that no distance can reach
// past this table.
constexpr std::array<std::string_view, 15> NUMBER_NAMES{ "unison",         // 1
                                                         "second",         // 2
                                                         "third",          // 3
                                                         "fourth",         // 4
                                                         "fifth",          // 5
                                                         "sixth",          // 6
                                                         "seventh",        // 7
                                                         "octave",         // 8, a perfect eighth is an octave
                                                         "ninth",          // 9
                                                         "tenth",          // 10
                                                         "eleventh",       // 11
                                                         "twelfth",        // 12
                                                         "thirteenth",     // 13
                                                         "fourteenth",     // 14
                                                         "fifteenth" };    // 15, exactly two octaves

// Builds the table of the twelve simple intervals. constexpr rather than a run time initialisation:
// the answer choices of an exercise must never be recomputed while the player is waiting.
[[nodiscard]] constexpr std::array<Interval, SEMITONES_PER_OCTAVE> buildSimpleIntervals() noexcept
{
    std::array<Interval, SEMITONES_PER_OCTAVE> intervals{};

    // std::views::iota replaces "for (int index = 0; ...)" with something that cannot go wrong.
    for( const std::int32_t semitones : std::views::iota( 0, SEMITONES_PER_OCTAVE ) )
    {
        intervals.at( static_cast<std::size_t>( semitones ) ) = Interval{ semitones };
    }

    return intervals;
}

constexpr std::array<Interval, SEMITONES_PER_OCTAVE> SIMPLE_INTERVALS{ buildSimpleIntervals() };

// Builds the table of every interval the application supports, from the unison to the fifteenth.
//
// Same reasoning as above: constexpr, because this list is the source of the answer choices, and an
// exercise must never wait for it to be computed.
[[nodiscard]] constexpr std::array<Interval, SUPPORTED_INTERVAL_COUNT> buildSupportedIntervals() noexcept
{
    std::array<Interval, SUPPORTED_INTERVAL_COUNT> intervals{};

    for( const std::int32_t semitones : std::views::iota( 0, MAXIMUM_INTERVAL_SEMITONES + 1 ) )
    {
        intervals.at( static_cast<std::size_t>( semitones ) ) = Interval{ semitones };
    }

    return intervals;
}

constexpr std::array<Interval, SUPPORTED_INTERVAL_COUNT> SUPPORTED_INTERVALS{ buildSupportedIntervals() };

}    // namespace

std::string Interval::name() const
{
    // Both lookups are guaranteed to succeed by construction: the quality indexes a five entry table
    // and the number stays between one and fifteen, as the class can never exceed seven and at most
    // two octaves can be added. A test pins that down.
    const auto qualitySlot = static_cast<std::size_t>( quality() );
    const auto numberSlot = static_cast<std::size_t>( number() - 1 );

    std::string fullName{ QUALITY_NAMES.at( qualitySlot ) };
    fullName += ' ';
    fullName += NUMBER_NAMES.at( numberSlot );

    return fullName;
}

std::string Interval::identifier() const
{
    const auto qualitySlot = static_cast<std::size_t>( quality() );

    std::string identifier{ QUALITY_LETTERS.at( qualitySlot ) };
    identifier += std::to_string( number() );

    return identifier;
}

Interval intervalBetween( const Note & p_firstNote, const Note & p_secondNote )
{
    // The distance is signed, the interval is not: going up a fifth and going down a fifth are the
    // same interval, and the constructor keeps only the magnitude.
    return Interval{ distanceInSemitones( p_firstNote, p_secondNote ) };
}

Interval intervalFromSemitones( std::int32_t p_semitones )
{
    // Turned into an Interval, a raw semitone count becomes a validated, named musical object: the
    // magnitude is reduced to what the application supports, the quality follows from the class, and
    // the number from the class and the octaves. This named entry point is what the exercise
    // generator will use, instead of building an Interval by hand at every call site.
    return Interval{ p_semitones };
}

std::vector<Interval> intervalsOf( std::span<const Note> p_notes )
{
    if( p_notes.size() < 2 )
    {
        return {};
    }

    std::vector<Interval> intervals;
    intervals.reserve( p_notes.size() - 1 );

    // A window of two consecutive notes, walked with iterators.
    //
    // std::views::adjacent<2> used to express this, with no index at all. It is a C++23 view that the
    // libc++ shipped with the Android NDK (Clang 18) does not provide, and one single code path shared
    // by every platform is worth more than the shorter syntax here: it is the path the unit tests
    // exercise on the development machine, for the phone as well.
    //
    // Iterators rather than indices, because indexing a span is an unchecked access, which the
    // clang-tidy configuration of this project refuses. 'std::next' never reaches past the end: the
    // loop stops as soon as it would. See docs/BUILD_AND_SETUP.md.
    for( auto firstNote = p_notes.begin(); firstNote != p_notes.end(); ++firstNote )
    {
        const auto secondNote = std::next( firstNote );

        if( secondNote == p_notes.end() )
        {
            break;
        }

        intervals.push_back( intervalBetween( *firstNote, *secondNote ) );
    }

    return intervals;
}

const std::array<Interval, SEMITONES_PER_OCTAVE> & allSimpleIntervals()
{
    return SIMPLE_INTERVALS;
}

const std::array<Interval, SUPPORTED_INTERVAL_COUNT> & allSupportedIntervals()
{
    return SUPPORTED_INTERVALS;
}

}    // namespace musichien::domain
