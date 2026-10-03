#pragma once

// =====================================================================================================================
// Musichien - Course
//
// A lesson: what the School teaches, held as DATA.
//
// ---------------------------------------------------------------------------------------------------------------------
// Why a SEQUENCE of blocks, and not one big string of Markdown
//
// A lesson is not a page of text with a few links in it. It is a text that carries CARDS: something to
// play, something to listen to, something to try. Keeping the whole file as one string would mean
// finding those cards again later by scanning that string - in the interface, where nothing can be
// tested. Here, the order of the vector IS the order of the file, and it is data.
//
// ---------------------------------------------------------------------------------------------------------------------
// What the domain knows, and what it leaves alone
//
// No path, no Markdown renderer, no Qt. It says WHICH interval to play (a distance in semitones - the
// only thing the domain recognises) and WHERE a card leads (a URL, treated as an opaque string).
// Rendering it is the interface's business, and so is opening it.
//
// See the contract: notes 29 of the Vault, and MarkdownCourse.h in the infrastructure.
// =====================================================================================================================

#include "domain/music/Interval.h"

#include <cstdint>
#include <string>
#include <vector>

namespace musichien::domain
{

// One element of a lesson, in the order its writer put it.
struct CourseBlock
{
    enum class Kind
    {
        // A paragraph of Markdown, displayed as it is.
        Text,

        // ":: jeu" - the game plays this interval: nothing to open, nothing to download, no network.
        PlayInterval,

        // ":: écoute" - a card that LEAVES the application: the real music, elsewhere.
        Listen,

        // ":: essai" - the button that starts the matching exercise.
        TryExercise,

        // ":: annexe" - the way to the long annexe.
        Annexe
    };

    Kind kind{ Kind::Text };

    // Kind::Text - the Markdown source of the paragraph, whole.
    std::string markdown;

    // Kind::PlayInterval and Kind::TryExercise - WHICH interval, as a distance in semitones.
    //
    // Never a name. A name is a label that gets renamed; a distance is what the domain understands,
    // and it is already how the memory hints are keyed. Two spellings of the same interval can never
    // become two entries here.
    std::int32_t semitones{ 0 };

    // Kind::PlayInterval - how it is sounded.
    IntervalDirection direction{ IntervalDirection::Ascending };

    // Kind::PlayInterval - what the card says under its button, as written by the lesson.
    std::string caption;

    // Kind::Listen - the card, whole.
    std::string source;    // "youtube" or "spotify", kept exactly as the file wrote it
    std::string url;
    std::string title;
    std::string listenFor;    // what to hear in it: required by the contract (Mayer's signalling)

    // Kind::Annexe - the name of the annexe, as that file's own front matter gives it.
    std::string annexeName;
};

// A lesson, ready to be shown.
struct Course
{
    std::string title;
    std::string subtitle;

    int chapter{ 0 };
    int order{ 0 };

    // What this lesson teaches, as distances in semitones.
    //
    // This is THE link with the game. The same value the hints are keyed on, so a lesson can name the
    // exercise it belongs to - and feed the review plan - without knowing anything about either.
    std::vector<std::int32_t> concepts;

    // The lesson itself, in order.
    std::vector<CourseBlock> blocks;

    [[nodiscard]] bool isEmpty() const noexcept { return blocks.empty(); }
};

}    // namespace musichien::domain
