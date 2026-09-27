#pragma once

// =====================================================================================================================
// Musichien - PitchDetector
//
// Detects the pitch of the voice from the microphone. A PORT, exactly like NotePlayer: the application asks for a
// pitch, and it does not care whether the answer is a microphone, a desktop no-op or a future audio engine.
//
// The domain owns this port and never sees an implementation: a pitch in hertz is an AUDIO concern, and comparing it
// to the interval asked is the view model's job - the domain owns no clock and no hardware.
// =====================================================================================================================

#include <functional>

namespace musichien::domain
{

class PitchDetector
{
public:
    PitchDetector() = default;

    PitchDetector( const PitchDetector & ) = delete;
    PitchDetector & operator=( const PitchDetector & ) = delete;
    PitchDetector( PitchDetector && ) = delete;
    PitchDetector & operator=( PitchDetector && ) = delete;

    virtual ~PitchDetector() = default;

    // Called every time a stable pitch is detected, with its frequency in hertz.
    using PitchCallback = std::function<void( float p_frequencyHz )>;

    // Starts listening. A later call replaces the previous callback.
    virtual void start( PitchCallback p_callback ) = 0;

    virtual void stop() = 0;
};

}    // namespace musichien::domain
