#include "domain/exercise/AnecdoteBook.h"

#include <gtest/gtest.h>

#include <random>

namespace musichien::domain
{

// ---------------------------------------------------------------------------------------------------------------------
// The loading-screen anecdotes
//
// An anecdote is CONTENT, and the only rule worth testing is the one the book adds: it never draws from nothing,
// and it never invents a kind.
// ---------------------------------------------------------------------------------------------------------------------

TEST( AnecdoteBookTest, an_empty_book_draws_nothing )
{
    AnecdoteBook book;
    std::mt19937 engine{ 1 };

    EXPECT_EQ( std::nullopt, book.random( engine ) );
}

TEST( AnecdoteBookTest, a_book_draws_only_what_it_holds )
{
    AnecdoteBook book;

    book.add( AnecdoteKind::Acoustics, "une octave, une frequence doublee" );
    book.add( AnecdoteKind::Tip, "chante avant de nommer" );

    std::mt19937 engine{ 1 };

    for( int draw = 0; draw < 100; ++draw )
    {
        const std::optional<Anecdote> anecdote = book.random( engine );

        ASSERT_TRUE( anecdote.has_value() );
        EXPECT_FALSE( anecdote->text.empty() );
    }

    EXPECT_EQ( 2U, book.count() );
}

}    // namespace musichien::domain
