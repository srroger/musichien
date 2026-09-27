#include "domain/exercise/AnecdoteBook.h"

#include <utility>

namespace musichien::domain
{

void AnecdoteBook::add( AnecdoteKind p_kind, std::string p_text )
{
    m_anecdotes.push_back( Anecdote{ .kind = p_kind, .text = std::move( p_text ) } );
}

std::optional<Anecdote> AnecdoteBook::random( std::mt19937 & p_randomEngine ) const
{
    if( m_anecdotes.empty() )
    {
        return std::nullopt;
    }

    std::uniform_int_distribution<std::size_t> distribution{ 0, m_anecdotes.size() - 1 };

    return m_anecdotes.at( distribution( p_randomEngine ) );
}

std::vector<std::string> AnecdoteBook::texts() const
{
    std::vector<std::string> result;
    result.reserve( m_anecdotes.size() );

    for( const Anecdote & anecdote : m_anecdotes )
    {
        result.push_back( anecdote.text );
    }

    return result;
}

}    // namespace musichien::domain
