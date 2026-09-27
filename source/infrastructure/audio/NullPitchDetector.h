#pragma once

#include "infrastructure/audio/PitchDetector.h"

namespace musichien::infrastructure
{

// The honest "nothing" for a machine with no microphone: no pitch is ever detected, and nothing crashes. The real
// detector is chosen by the application at start up, and this one is what a test or a desktop gets.
class NullPitchDetector final : public PitchDetector
{
public:
    void start( PitchCallback ) override
    {
    }

    void stop() override
    {
    }
};

}    // namespace musichien::infrastructure
