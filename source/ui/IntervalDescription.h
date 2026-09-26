#pragma once

// =====================================================================================================================
// Musichien - describeInterval
//
// Turns a domain interval into the plain map a screen can read.
//
// It lives on its own because TWO screens now need the same description - the bench of the landing
// screen and the feedback of an exercise - and two hand written copies of a data shape is exactly how
// a screen ends up reading a key the other one does not fill.
//
// Every key is filled every time, and the screen reads them without checking. An EMPTY map is what
// says "there is nothing to name", and it is what a single note produces.
//
//   * 'identifier' is the stable name a save file would keep: P1, m3, M10...
//   * 'name' is the one a human reads: "Perfect unison", "Minor third"...
//   * the rest describes the MODEL rather than the interval, and exists so that the model can be seen
//     while it is being built.
// =====================================================================================================================

#include "domain/music/Interval.h"

#include <QVariantMap>

namespace musichien::ui
{

[[nodiscard]] QVariantMap describeInterval( const domain::Interval & p_interval );

}    // namespace musichien::ui
