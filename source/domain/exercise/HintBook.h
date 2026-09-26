#pragma once

// =====================================================================================================================
// Musichien - HintBook
//
// What to hum when an interval will not come: a snatch of music the player already knows.
//
// > "Star Wars. Everyone knows the fifth because everyone has heard the fifth."
//
// A hint is CONTENT, not code: the book is filled from a data file, so adding a piece of music to an
// interval never means recompiling. This type is where the domain holds the result.
//
// It is deliberately a plain lookup with no rule about WHEN a hint is shown: that belongs to the
// session, which knows how many attempts have been made. A book that decided when to be read would be
// two responsibilities in one.
// =====================================================================================================================

#include "domain/music/Interval.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace musichien::domain
{

// A memory hook for one interval, heard one way.
struct IntervalHint
{
    // The emoji and the words together, ready to display: "🚗 Retour vers le Futur".
    //
    // ONE string rather than two fields: the emoji and the words are one idea, their order varies from
    // one hint to the next, and splitting them would only add a rule to get wrong.
    std::string label;

    // Every piece of music that could stand for this interval. Kept whole even though only the label is
    // shown for now: varying the hint is the entire reason the list exists.
    std::vector<std::string> songs;
};

class HintBook
{
public:
    void add( const Interval & p_interval, IntervalDirection p_direction, IntervalHint p_hint );

    // The hint for an interval heard a given way, when the book has one.
    //
    // A HARMONIC interval is looked up as an ASCENDING one, and that is a decision rather than a
    // shortcut: the songs in the book are melodies, the ear meets them from the bottom up, and asking
    // the data for a third case would mean writing every hint twice.
    [[nodiscard]] std::optional<IntervalHint> hintFor( const Interval & p_interval,
                                                       IntervalDirection p_direction ) const;

    [[nodiscard]] std::size_t hintCount() const noexcept { return m_hints.size(); }

private:
    // Keyed by the DISTANCE and the direction, never by the name: the name of an interval is derived
    // from its distance, and keying on it would let two spellings of the same interval become two
    // entries - with one of them silently never found.
    std::map<std::pair<std::int32_t, IntervalDirection>, IntervalHint> m_hints;
};

}    // namespace musichien::domain
