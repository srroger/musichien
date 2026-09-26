#pragma once

// =====================================================================================================================
// Musichien - Interval
//
// An interval is the distance between two notes. It is the very first thing the application teaches,
// so this type is deliberately small, total and free of any input/output.
//
// The file also shows the C++26 vocabulary used everywhere else in the project: concepts, std::span,
// std::optional, ranges and pipelines. See docs/CODE_CONVENTIONS.md.
// =====================================================================================================================

#include "domain/music/Note.h"

#include <array>
#include <concepts>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace musichien::domain
{

// ---------------------------------------------------------------------------------------------------------------------
// How the two notes of an interval are sounded.
// ---------------------------------------------------------------------------------------------------------------------
enum class IntervalDirection
{
    Ascending,     // the second note is higher
    Descending,    // the second note is lower
    Harmonic       // both notes are played together
};

// ---------------------------------------------------------------------------------------------------------------------
// The musical colours an interval can take inside an octave.
// ---------------------------------------------------------------------------------------------------------------------
enum class IntervalQuality
{
    Perfect,
    Major,
    Minor,
    Augmented,
    Diminished
};

// ---------------------------------------------------------------------------------------------------------------------
// Free functions of the domain
// ---------------------------------------------------------------------------------------------------------------------

// Reduces any distance in semitones, positive or negative, into the range [0, 11].
//
// An interval is a musical colour, not a direction: a falling fifth and a rising fifth are the same
// interval. Exposing this normalisation makes it reusable by the exercise generator instead of
// duplicating the rule in several places.
//
// Written by hand rather than with std::abs so that it stays usable in a constant expression.
[[nodiscard]] constexpr std::int32_t semitonesInSimpleForm( std::int32_t p_semitones ) noexcept
{
    const std::int32_t absoluteSemitones = ( p_semitones < 0 ) ? -p_semitones : p_semitones;

    return absoluteSemitones % SEMITONES_PER_OCTAVE;
}

// ---------------------------------------------------------------------------------------------------------------------
// A named interval: a number of semitones plus the quality it carries.
// ---------------------------------------------------------------------------------------------------------------------
class Interval
{
public:
    // The default interval is the unison: it is the neutral answer when no note has been played yet.
    // Required so that the answer tables of the exercises can be built as plain arrays.
    constexpr Interval() noexcept = default;

    // The number of semitones is normalised to its simple form, inside a single octave.
    //
    // An interval describes a musical colour, not a direction: a falling fifth and a rising fifth
    // share the same identity. Normalising in the constructor makes every Interval valid by
    // construction, and removes any possibility of a half-normalised instance somewhere else.
    constexpr Interval( std::int32_t p_semitones, IntervalQuality p_quality ) noexcept
      : m_semitones{ semitonesInSimpleForm( p_semitones ) }
      , m_quality{ p_quality }
    {
    }

    [[nodiscard]] constexpr std::int32_t semitones() const noexcept { return m_semitones; }
    [[nodiscard]] constexpr IntervalQuality quality() const noexcept { return m_quality; }

    // True for a simple interval, that is inside a single octave. The unison is a simple interval.
    [[nodiscard]] constexpr bool isSimple() const noexcept
    {
        return ( m_semitones >= 0 ) && ( m_semitones < SEMITONES_PER_OCTAVE );
    }

    // English name of the interval, for instance "Perfect fifth" or "Major third".
    [[nodiscard]] std::string name() const;

    // Identifier used by the content files and the save file, for instance "P5" or "M3".
    // A short stable identifier is required: the display name will be translated, this one never is.
    [[nodiscard]] std::string_view identifier() const noexcept;

    [[nodiscard]] friend constexpr bool operator==( const Interval & p_left, const Interval & p_right ) noexcept
    {
        return ( p_left.m_semitones == p_right.m_semitones ) && ( p_left.m_quality == p_right.m_quality );
    }

private:
    std::int32_t m_semitones{ 0 };
    IntervalQuality m_quality{ IntervalQuality::Perfect };
};

// ---------------------------------------------------------------------------------------------------------------------
// Concepts
//
// They replace enable_if and produce readable error messages when a constraint is not met.
// ---------------------------------------------------------------------------------------------------------------------

// Anything that exposes its notes as a contiguous sequence, for instance a std::vector<Note>.
template<typename NoteContainer>
concept NoteContainerLike = requires( const NoteContainer & p_container ) {
    { std::span{ p_container } };
    requires std::same_as<std::ranges::range_value_t<NoteContainer>, Note>;
};

// ---------------------------------------------------------------------------------------------------------------------
// Exercise helpers
//
// Everything the exercise generator needs from the domain. Kept free of any input and output.
// ---------------------------------------------------------------------------------------------------------------------

// Builds the interval between two notes. The direction of the movement does not change the result:
// the constructor normalises the distance and the quality comes from the reference table.
[[nodiscard]] Interval intervalBetween( const Note & p_firstNote, const Note & p_secondNote );

// Identifies the interval of a distance in semitones. Always succeeds: the distance is reduced to
// its simple form first, and every simple form has a name.
[[nodiscard]] Interval intervalFromSemitones( std::int32_t p_semitones );

// Turns a sequence of notes into the sequence of the intervals it contains.
// Shows the ranges pipeline style expected everywhere: no index, no manual loop.
[[nodiscard]] std::vector<Interval> intervalsOf( std::span<const Note> p_notes );

// Names of every simple interval, used to build the answer choices of an exercise.
[[nodiscard]] const std::array<Interval, 12> & allSimpleIntervals();

}    // namespace musichien::domain
