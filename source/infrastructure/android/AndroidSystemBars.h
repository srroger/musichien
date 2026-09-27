#pragma once

// =====================================================================================================================
// Musichien - AndroidSystemBars
//
// The look of the Android system bars, forced from CODE rather than from the theme.
//
// Why this exists: from Android 15 (API 35) onwards, `android:statusBarColor` is deprecated and IGNORED by the
// system - the bars are drawn transparent instead, and the application is expected to say what it wants. The theme
// can no longer do it alone, so a small Java class does, and this is the way in.
//
// Compiled on Android only: on the desktop there are no system bars to paint.
// =====================================================================================================================

namespace musichien::infrastructure
{

// Paints the system bars in the game's night blue and makes their icons light. Idempotent, and safe to call again
// whenever the window comes back to the foreground.
void applyAndroidNightSystemBars();

}    // namespace musichien::infrastructure