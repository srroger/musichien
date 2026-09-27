#pragma once

#include "domain/exercise/AnecdoteBook.h"

#include <string_view>

namespace musichien::infrastructure
{

// Reads the anecdotes from a JSON document. The document is TEXT in, and the book is CONTENT out: this is the only
// place that knows the shape of the file.
[[nodiscard]] domain::AnecdoteBook readAnecdoteBook( std::string_view p_json );

}    // namespace musichien::infrastructure
