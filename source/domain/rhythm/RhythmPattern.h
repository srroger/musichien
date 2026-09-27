#pragma once

// =====================================================================================================================
// Musichien - RhythmPattern
//
// A short, recognisable rhythmic CELL - the "cliche" of a style - that the player listens to and then reproduces.
//
// ---------------------------------------------------------------------------------------------------------------------
// Why the unit is the BEAT and not the second
//
// A pattern has to work at 60 bpm and at 180 bpm: what makes it recognisable is WHERE the hits land relative to the
// pulse, never how long they last. So a pattern is written in beats, and the metronome is what turns beats into time.
// A hit on beat 1.5 is the "and" of the second beat, whatever the tempo.
//
// ---------------------------------------------------------------------------------------------------------------------
// Honesty about the styles
//
// The cells below are SIMPLIFIED in the spirit of a style, not transcriptions: they keep the accent and the syncopation
// that make a rock beat or a waltz recognisable, and they are playable by a beginner with four pieces of a kit. A real
// bossa or a real clave has more to it, and that is a conversation for later.
// =====================================================================================================================

#include "domain/audio/DrumSynthesizer.h"

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace musichien::domain
{

// One hit of a rhythmic cell.
struct RhythmHit
{
    // Where the hit lands, in BEATS from the start of the loop. 0 is the downbeat, 0.5 the "and" of the first beat.
    double beat{ 0.0 };

    Drum drum{ Drum::Kick };

    // Whether the hit is struck harder. It is what makes a cell breathe instead of ticking.
    bool accented{ false };
};

class RhythmPattern
{
public:
    RhythmPattern( std::string_view p_name, int p_beatsPerBar, std::vector<RhythmHit> p_hits );

    [[nodiscard]] std::string_view name() const noexcept { return m_name; }

    // How many beats one loop holds: the numerator of the time signature, and the length the loop wraps at.
    [[nodiscard]] int beatsPerBar() const noexcept { return m_beatsPerBar; }

    [[nodiscard]] std::span<const RhythmHit> hits() const noexcept { return m_hits; }

private:
    std::string_view m_name;
    int m_beatsPerBar{ 4 };
    std::vector<RhythmHit> m_hits;
};

// The cells the game offers, from the plainest to the most syncopated. The order is a contract: it is the order of the
// buttons, and a remembered choice points into it.
[[nodiscard]] const std::vector<RhythmPattern> & allRhythmPatterns();

// How far, in BEATS, a position is from the nearest onset of a pattern - the loop wrapping around at its end.
//
// This is the question the "reproduce it" exercise asks: the player's tap is right if it lands near an onset, whatever
// else happens. The controller turns the answer into milliseconds and judges it, because only it knows the tempo, and
// the domain owns no clock.
[[nodiscard]] double distanceToNearestOnsetInBeats( const RhythmPattern & p_pattern, double p_positionInBeats ) noexcept;

}    // namespace musichien::domain
