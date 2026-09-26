#pragma once

// =====================================================================================================================
// Musichien - Interval
//
// An interval is the distance between two notes. It is the very first thing the application teaches,
// so this type is deliberately small, total and free of any input/output.
//
// ---------------------------------------------------------------------------------------------------------------------
// The two faces of an interval
//
// Music theory distinguishes two things, and so does this type:
//
//   * the DISTANCE, in semitones, octaves INCLUDED: 14 semitones is a major ninth, not a major
//     second. Throwing the octaves away makes the ninth indistinguishable from the second, and the
//     ninth is one of the colours the application exists to teach;
//
//   * the CLASS, inside a single octave: the colour that makes a ninth sound "like" a second, heard
//     one register higher. 0 to 11 semitones.
//
// Keeping both is what lets the application teach the simple intervals first and the compound ones
// afterwards, while sharing a single notion of colour.
//
// ---------------------------------------------------------------------------------------------------------------------
// How far does an interval go?
//
// The thirteenth is the furthest USEFUL extension: by then all seven degrees of the scale are
// present in the chord, so anything wider only repeats a note already heard. The fifteenth - exactly
// two octaves - is the last interval that still has a name of its own; beyond it, musicians say "two
// octaves and a third" rather than numbering. The application therefore stops at two octaves.
//
// The file also shows the C++26 vocabulary used everywhere else in the project: concepts, std::span,
// std::optional, ranges and pipelines. See docs/CODE_CONVENTIONS.md.
// =====================================================================================================================

#include "domain/music/Note.h"

#include <array>
#include <concepts>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace musichien::domain
{

// ---------------------------------------------------------------------------------------------------------------------
// How the two notes of an interval are sounded.
//
// Note that this is NOT part of Interval: an interval is a magnitude, and the direction belongs to
// what is played. A falling fifth and a rising fifth are the same interval.
// ---------------------------------------------------------------------------------------------------------------------
enum class IntervalDirection
{
    Ascending,     // the second note is higher
    Descending,    // the second note is lower
    Harmonic       // both notes are played together
};

// ---------------------------------------------------------------------------------------------------------------------
// The musical colours an interval can take.
// ---------------------------------------------------------------------------------------------------------------------
enum class IntervalQuality
{
    Perfect,
    Major,
    Minor,
    Augmented,
    Diminished
};

// The widest interval the application ever uses: two whole octaves, that is a fifteenth.
//
// Wider distances are not rejected, they are reduced to this one. Reducing rather than wrapping is
// deliberate: a wrapped distance would silently turn an unplayable request into a valid but WRONG
// interval, which is far worse than a saturated one. No ear training exercise ever asks for more.
inline constexpr std::int32_t MAXIMUM_INTERVAL_SEMITONES = 2 * SEMITONES_PER_OCTAVE;

// Number of letter names inside an octave.
//
// This is the constant that makes compound naming work, and it is SEVEN, not twelve: adding an
// octave turns a unison into an eighth (1 + 7), a second into a ninth (2 + 7), a sixth into a
// thirteenth (6 + 7). There are only seven distinct letter names before they repeat.
inline constexpr std::int32_t DIATONIC_STEPS_PER_OCTAVE = 7;

// ---------------------------------------------------------------------------------------------------------------------
// Free functions of the domain
// ---------------------------------------------------------------------------------------------------------------------

// Reduces any distance in semitones, positive or negative, into the range [0, 11]: the CLASS of an
// interval, its colour heard inside a single octave.
//
// Written by hand rather than with std::abs so that it stays usable in a constant expression.
[[nodiscard]] constexpr std::int32_t semitonesInSimpleForm( std::int32_t p_semitones ) noexcept
{
    const std::int32_t absoluteSemitones = ( p_semitones < 0 ) ? -p_semitones : p_semitones;

    return absoluteSemitones % SEMITONES_PER_OCTAVE;
}

// Magnitude of a distance, reduced to the widest interval the application supports.
[[nodiscard]] constexpr std::int32_t supportedSemitoneMagnitude( std::int32_t p_semitones ) noexcept
{
    const std::int32_t absoluteSemitones = ( p_semitones < 0 ) ? -p_semitones : p_semitones;

    return ( absoluteSemitones > MAXIMUM_INTERVAL_SEMITONES ) ? MAXIMUM_INTERVAL_SEMITONES
                                                              : absoluteSemitones;
}

// ---------------------------------------------------------------------------------------------------------------------
// A named interval: a distance in semitones, octaves INCLUDED.
//
// The quality is not stored: it FOLLOWS from the class. Keeping it as a parameter would allow a
// "major fifth", which is not a thing, and would make the type able to represent something that does
// not exist. One distance, one interval.
// ---------------------------------------------------------------------------------------------------------------------
class Interval
{
public:
    // The default interval is the unison: it is the neutral answer when no note has been played yet.
    // Required so that the answer tables of the exercises can be built as plain arrays.
    constexpr Interval() noexcept = default;

    // The distance is kept as it is, octaves included: 14 semitones is a major ninth.
    //
    // Only the magnitude matters. An interval is a distance, and the direction of the movement is
    // carried by IntervalDirection, on the side of what is played: a falling fifth and a rising
    // fifth are the same interval. A distance wider than the fifteenth is reduced to it, see
    // MAXIMUM_INTERVAL_SEMITONES.
    //
    // explicit, exactly like Note: an implicit conversion from int would let a semitone count pass
    // for an interval anywhere in the code, and that kind of mix-up compiles silently.
    explicit constexpr Interval( std::int32_t p_semitones ) noexcept
      : m_semitones{ supportedSemitoneMagnitude( p_semitones ) }
    {
    }

    // The distance between the two notes, in semitones, octaves included: 14 for a major ninth.
    [[nodiscard]] constexpr std::int32_t semitones() const noexcept { return m_semitones; }

    // The colour heard when the octaves are ignored: 0 to 11 semitones.
    //
    // A ninth and a second share this class, which is exactly what the ear does when it recognises a
    // familiar colour one register higher.
    [[nodiscard]] constexpr std::int32_t intervalClass() const noexcept
    {
        return semitonesInSimpleForm( m_semitones );
    }

    // How many whole octaves the interval spans. Zero for a simple interval.
    [[nodiscard]] constexpr std::int32_t octaveSpan() const noexcept
    {
        return m_semitones / SEMITONES_PER_OCTAVE;
    }

    // The number musicians give it: 1 for a unison, 3 for a third, 9 for a ninth, 15 for two whole
    // octaves. One more number per octave, hence DIATONIC_STEPS_PER_OCTAVE.
    //
    // The simple part comes from the CLASS, not from the distance: in equal temperament the distance
    // alone cannot tell a second from a third, only the spelling can, and six semitones can be
    // written as an augmented fourth or as a diminished fifth. The application names intervals by
    // what is HEARD, so one canonical spelling per distance is enough - the ear does not distinguish
    // the two, and a learner should not be asked to either.
    [[nodiscard]] constexpr std::int32_t number() const noexcept
    {
        return classDefinition().diatonicNumber + ( DIATONIC_STEPS_PER_OCTAVE * octaveSpan() );
    }

    // The colour of the interval: perfect, major, minor, augmented or diminished.
    [[nodiscard]] constexpr IntervalQuality quality() const noexcept
    {
        return classDefinition().quality;
    }

    // True for a simple interval, that is inside a single octave. The unison is a simple interval.
    [[nodiscard]] constexpr bool isSimple() const noexcept { return octaveSpan() == 0; }

    // True for a compound interval: wider than an octave, so with a number above seven.
    [[nodiscard]] constexpr bool isCompound() const noexcept { return octaveSpan() > 0; }

    // English name of the interval, for instance "Perfect fifth" or "Major ninth".
    [[nodiscard]] std::string name() const;

    // Identifier used by the content files and the save file, for instance "P5", "M9" or "P11".
    // A short stable identifier is required: the display name will be translated, this one never is.
    [[nodiscard]] std::string identifier() const;

    [[nodiscard]] friend constexpr bool operator==( const Interval & p_left, const Interval & p_right ) noexcept
    {
        return p_left.m_semitones == p_right.m_semitones;
    }

private:
    // What a single interval class is worth, indexed by its number of semitones inside the octave.
    //
    // This is musical knowledge rather than arithmetic: the semitone span of a number is irregular,
    // a third covers three or four semitones and a sixth eight or nine. A table is the honest way to
    // write it down. The quality is here because it is what turns a class into a name.
    //
    // The members are deliberately NOT prefixed: this is a plain data aggregate, and the convention
    // keeps those unprefixed. See docs/CODE_CONVENTIONS.md.
    struct ClassDefinition
    {
        IntervalQuality quality;
        std::int32_t diatonicNumber;
    };

    // One entry per semitone inside the octave, in order.
    static constexpr std::array<ClassDefinition, SEMITONES_PER_OCTAVE> CLASS_DEFINITIONS{
      ClassDefinition{ .quality = IntervalQuality::Perfect, .diatonicNumber = 1 },      // unison
      ClassDefinition{ .quality = IntervalQuality::Minor, .diatonicNumber = 2 },        // minor second
      ClassDefinition{ .quality = IntervalQuality::Major, .diatonicNumber = 2 },        // major second
      ClassDefinition{ .quality = IntervalQuality::Minor, .diatonicNumber = 3 },        // minor third
      ClassDefinition{ .quality = IntervalQuality::Major, .diatonicNumber = 3 },        // major third
      ClassDefinition{ .quality = IntervalQuality::Perfect, .diatonicNumber = 4 },      // perfect fourth
      ClassDefinition{ .quality = IntervalQuality::Augmented, .diatonicNumber = 4 },    // augmented fourth
      ClassDefinition{ .quality = IntervalQuality::Perfect, .diatonicNumber = 5 },      // perfect fifth
      ClassDefinition{ .quality = IntervalQuality::Minor, .diatonicNumber = 6 },        // minor sixth
      ClassDefinition{ .quality = IntervalQuality::Major, .diatonicNumber = 6 },        // major sixth
      ClassDefinition{ .quality = IntervalQuality::Minor, .diatonicNumber = 7 },        // minor seventh
      ClassDefinition{ .quality = IntervalQuality::Major, .diatonicNumber = 7 } };      // major seventh

    [[nodiscard]] constexpr const ClassDefinition & classDefinition() const noexcept
    {
        return CLASS_DEFINITIONS.at( static_cast<std::size_t>( intervalClass() ) );
    }

    std::int32_t m_semitones{ 0 };
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
