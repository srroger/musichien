#pragma once

// =====================================================================================================================
// Musichien - readCourse
//
// Fills a course from the MARKDOWN TEXT of a lesson file.
//
// ---------------------------------------------------------------------------------------------------------------------
// Why the TEXT, and not a file name
//
// Same reason as every other content reader of this project: the lessons are embedded in the Qt
// resources so that the phone and the desktop read the SAME bytes through the same path. A loader that
// opened a path itself would work on the development machine and find nothing on the device. The
// caller reads the text; this function only understands it.
//
// ---------------------------------------------------------------------------------------------------------------------
// The format, which is a contract with a file a human edits
//
//     ---
//     titre: La quinte juste
//     sous-titre: Deux notes qui n'ont rien à se prouver
//     chapitre: 1
//     ordre: 1
//     concepts: 7
//     ---
//
//     ## Une accroche
//
//     Du texte, en Markdown.
//
//     :: jeu | demi_tons:7 | ascendant | do → sol, montant
//     :: écoute | youtube | https://… | « Un titre » | ce qu'il faut y entendre
//     :: essai | demi_tons:7
//     :: annexe | pourquoi-la-quinte-sonne-juste
//
// A DIRECTIVE IS ONE LINE, fields separated by ' | '. One line can be copied, searched with grep, and
// broken without taking the rest of the file with it: a directive that cannot be read is SKIPPED and
// reported, never thrown. A typo in a lesson must cost one card, not the application.
//
// 'concepts' carries DISTANCES in semitones, never names - the same rule as the interval hints, and
// for the same reason: the domain recognises a distance, and a label only has to make sense to
// whoever writes the file. 'concepts: 7, 12' lists several.
//
// The French keys stay French: they belong to the content, not to the code.
// =====================================================================================================================

#include "domain/lesson/Course.h"

#include <optional>
#include <string_view>

namespace musichien::infrastructure
{

// The course the text describes, or nothing when it holds no usable lesson at all.
//
// A missing front matter is NOT a failure: the title then comes from the first heading, because a
// lesson file that forgot its header is still a lesson worth reading.
[[nodiscard]] std::optional<domain::Course> readCourse( std::string_view p_markdownText );

}    // namespace musichien::infrastructure
