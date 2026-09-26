#pragma once

// =====================================================================================================================
// Musichien - DeviceHaptics
//
// A short vibration, when the device can actually produce one.
//
// ---------------------------------------------------------------------------------------------------------------------
// Doing nothing on a desktop is not a fallback
//
// A development machine has nothing to vibrate. The function is therefore TOTAL and safe to call from
// anywhere, which is what lets one code path run on the personal computer and on the phone - and the
// screen still shakes, so the feedback is never missing, only quieter.
//
// ---------------------------------------------------------------------------------------------------------------------
// The permission it costs, and why that was a decision
//
// Android refuses to vibrate without 'android.permission.VIBRATE' declared in the manifest. It is a
// NORMAL permission: no dialog, no data, no hardware other than the motor, and refusing it is not even
// possible for the user. It is declared all the same, because the rule of this project is that no
// permission is taken without being written down, and scripts/build_android.sh fails the build if one
// appears that is not in its allowlist.
// =====================================================================================================================

namespace musichien::infrastructure
{

// A short buzz, the length of a punctuation mark. Silently does nothing when the device cannot vibrate.
void vibrateForMistake();

}    // namespace musichien::infrastructure
