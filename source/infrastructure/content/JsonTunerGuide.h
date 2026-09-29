#pragma once

#include "domain/music/TunerGuide.h"

#include <string_view>

namespace musichien::infrastructure
{

// Reads the tuner page's texts from a JSON document. The document is TEXT in, and the guide is CONTENT out: this is the
// only place that knows the shape of the file.
//
// Le fichier designe un temperament par la VALEUR DU DOMAINE ("equal", "pythagorean", "just"), jamais par son nom a
// l'ecran : un nom se traduit et se reecrit, une valeur ne bouge pas.
[[nodiscard]] domain::TunerGuide readTunerGuide( std::string_view p_json );

}    // namespace musichien::infrastructure
