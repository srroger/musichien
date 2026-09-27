#pragma once

#include <cstddef>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace musichien::domain
{

// What an anecdote is about. This is what lets the content file carry different flavours, and what lets a screen -
// or a notification - ask for one flavour rather than another.
enum class AnecdoteKind
{
    Lore,         // the world, the dog, the story
    Tip,          // how to train, how to play
    Acoustics,    // how sound and the ear work
    History,      // composers, instruments, music history
    Theory        // a nugget of music theory
};

// One anecdote: a kind and a short text, ready to display. The Morrowind loading screen, but about music.
struct Anecdote
{
    AnecdoteKind kind{ AnecdoteKind::Lore };
    std::string text;
};

// Where the anecdotes live: CONTENT, held by the domain and filled by the infrastructure from a data file. Adding
// one never means recompiling - see assets/content/anecdotes.json.
class AnecdoteBook
{
public:
    void add( AnecdoteKind p_kind, std::string p_text );

    // A random anecdote, or nothing when the book is empty. The engine is passed IN, never created here: the domain
    // holds no entropy source of its own.
    [[nodiscard]] std::optional<Anecdote> random( std::mt19937 & p_randomEngine ) const;

    // Every anecdote's text, in book order. Used by the daily reminder, which must be able to draw a DIFFERENT
    // anecdote on each firing without the application running.
    [[nodiscard]] std::vector<std::string> texts() const;

    [[nodiscard]] std::size_t count() const noexcept { return m_anecdotes.size(); }

private:
    std::vector<Anecdote> m_anecdotes;
};

}    // namespace musichien::domain
