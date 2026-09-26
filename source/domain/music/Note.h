#pragma once

// =====================================================================================================================
// Musichien - Note
//
// A note is identified by its MIDI number, an integer from 0 to 127.
// MIDI 69 is A4, the reference for equal temperament (440 Hz).
//
// This type is a pure value type: no Qt, no input/output, no side effect. It is therefore testable
// in isolation, which is the whole point of the domain module.
// =====================================================================================================================

#include <cstdint>
#include <string>

namespace musichien::domain
{

// Frequency of the reference note A4, in hertz.
inline constexpr double REFERENCE_FREQUENCY_HZ = 440.0;

// MIDI number of the reference note A4.
inline constexpr std::int32_t REFERENCE_MIDI_NUMBER = 69;

// Number of semitones in an octave. In equal temperament they all have the same size.
inline constexpr std::int32_t SEMITONES_PER_OCTAVE = 12;

class Note
{
public:
    // The MIDI range. Anything outside of it is not a playable note.
    static constexpr std::int32_t MINIMUM_MIDI_NUMBER = 0;
    static constexpr std::int32_t MAXIMUM_MIDI_NUMBER = 127;

    // A note is only ever built from an explicit MIDI number: no implicit conversion from int,
    // which would make a mix-up between a note and a semitone count compile silently.
    explicit constexpr Note( std::int32_t p_midiNumber ) noexcept
        : m_midiNumber{ p_midiNumber }
    {
    }

    [[nodiscard]] constexpr std::int32_t midiNumber() const noexcept { return m_midiNumber; }

    // True when the MIDI number is inside the playable range.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return ( m_midiNumber >= MINIMUM_MIDI_NUMBER ) && ( m_midiNumber <= MAXIMUM_MIDI_NUMBER );
    }

    // The octave the note belongs to, following the scientific pitch notation (A4 is in octave 4).
    [[nodiscard]] constexpr std::int32_t octave() const noexcept
    {
        return ( m_midiNumber / SEMITONES_PER_OCTAVE ) - 1;
    }

    // Index of the note inside its octave, from 0 (C) to 11 (B).
    [[nodiscard]] constexpr std::int32_t pitchClassIndex() const noexcept
    {
        return m_midiNumber % SEMITONES_PER_OCTAVE;
    }

    // Frequency of the note in hertz, using equal temperament.
    [[nodiscard]] double frequencyHz() const noexcept;

    // Human readable English name, for instance "C4" or "F#5".
    // The name is deliberately language neutral so that it never needs a translation.
    [[nodiscard]] std::string name() const;

    // Transposes the note by a number of semitones. Used to build exercises.
    [[nodiscard]] Note transposedBy( std::int32_t p_semitones ) const noexcept;

    [[nodiscard]] friend constexpr bool operator==( const Note & p_left, const Note & p_right ) noexcept
    {
        return p_left.m_midiNumber == p_right.m_midiNumber;
    }

    [[nodiscard]] friend constexpr auto operator<=>( const Note & p_left, const Note & p_right ) noexcept
    {
        return p_left.m_midiNumber <=> p_right.m_midiNumber;
    }

private:
    std::int32_t m_midiNumber;
};

// Number of semitones between two notes. Negative when p_to is lower than p_from.
[[nodiscard]] constexpr std::int32_t distanceInSemitones( const Note & p_from, const Note & p_to ) noexcept
{
    return p_to.midiNumber() - p_from.midiNumber();
}

}    // namespace musichien::domain
