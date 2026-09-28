#include "domain/music/Note.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <string_view>

namespace musichien::domain
{

namespace
{

// English names of the twelve pitch classes, using the sharp notation.
// Kept private to this translation unit: it is an implementation detail, not part of the interface.
constexpr std::array<std::string_view, SEMITONES_PER_OCTAVE> PITCH_CLASS_NAMES{
  "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

}    // namespace

double Note::frequencyHz() const noexcept
{
    const auto semitoneDistance = static_cast<double>( m_midiNumber - REFERENCE_MIDI_NUMBER );

    // Equal temperament: every semitone multiplies the frequency by the twelfth root of two.
    return REFERENCE_FREQUENCY_HZ * std::pow( 2.0, semitoneDistance / SEMITONES_PER_OCTAVE );
}

std::string Note::name() const
{
    return std::format( "{}{}", pitchClassName(), octave() );
}

std::string Note::pitchClassName() const
{
    return std::string{ PITCH_CLASS_NAMES.at( static_cast<std::size_t>( pitchClassIndex() ) ) };
}

Note Note::transposedBy( std::int32_t p_semitones ) const noexcept
{
    // Clamped rather than wrapped: transposing out of the playable range is a programming error we
    // prefer to see as a silent clamp than as an undefined behaviour in an audio callback.
    const std::int32_t transposedMidiNumber = std::clamp(
      m_midiNumber + p_semitones, MINIMUM_MIDI_NUMBER, MAXIMUM_MIDI_NUMBER );

    return Note{ transposedMidiNumber };
}

}    // namespace musichien::domain
