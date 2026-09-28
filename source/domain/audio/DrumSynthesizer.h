// =====================================================================================================================
// Musichien - DrumSynthesizer
//
// The pieces of a basic drum kit, SYNTHESISED from arithmetic: a kick is a falling sine, a snare and a hi-hat are
// shaped noise, a tom is a damped sine. No files, no Qt, no sound card - the same contract as ToneSynthesizer, so
// the whole kit is unit-testable on any machine.
//
// A real drummer has far more (rim shots, ghost notes, open hi-hat, crashes, rides...). These four are the shape of
// the kit; the rest are refinements on the same synthesis.
// =====================================================================================================================

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace musichien::domain
{

// The pieces the game offers. The order is a contract: it is the order of the buttons.
enum class Drum
{
    Kick,     // grosse caisse - the low thump
    Snare,    // caisse claire - the backbeat
    HiHat,    // charleston - the tick that keeps the time
    Tom       // tom - the fill
};

inline constexpr std::size_t DRUM_COUNT = 4;

class DrumSynthesizer
{
public:
    explicit DrumSynthesizer( std::int32_t p_sampleRate );

    // The PCM of one hit, mono, in [-1, 1]. A hit is short and dies on its own: it never loops.
    [[nodiscard]] std::vector<float> renderDrum( Drum p_drum ) const;

private:
    std::int32_t m_sampleRate;
};

}    // namespace musichien::domain
