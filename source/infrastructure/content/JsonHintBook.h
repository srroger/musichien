#pragma once

// =====================================================================================================================
// Musichien - readHintBook
//
// Fills a hint book from the JSON text of a content file.
//
// ---------------------------------------------------------------------------------------------------------------------
// Why the TEXT, and not a file name
//
// The content of this project is embedded in the Qt resources so that the phone and the desktop read the
// SAME bytes through the same path. A loader that opened a path itself would work on the development
// machine and find nothing on the device, which is a class of bug this project has already paid for.
// The caller reads the text; this function only understands it.
//
// ---------------------------------------------------------------------------------------------------------------------
// The format, which is a contract with a file a human edits
//
//     {
//       "intervalles": [
//         { "id": "5J", "nom": "quinte juste", "demi_tons": 7,
//           "ascendant":  { "indice": "🚗 Retour vers le Futur", "chansons": [ ... ] },
//           "descendant": { "indice": "🛕 Zelda Time Temple",    "chansons": [ ... ] } }
//       ]
//     }
//
// The interval is looked up by 'demi_tons', NEVER by 'id': a distance is what the domain knows, and
// 'id' is a human readable label that only has to make sense to whoever writes the file. That is also
// why the French keys stay French - they belong to the content, not to the code.
//
// 'indice' may be null, and that is how the file says "there is no hint for this direction yet". A
// missing or unusable entry is SKIPPED and reported, never thrown: a typo in a content file must cost
// one hint, not the application.
// =====================================================================================================================

#include "domain/exercise/HintBook.h"

#include <string_view>

namespace musichien::infrastructure
{

[[nodiscard]] domain::HintBook readHintBook( std::string_view p_jsonText );

}    // namespace musichien::infrastructure
