#include "domain/music/PhraseBook.h"

#include <cstdint>
#include <utility>

namespace musichien::domain
{

void PhraseBook::add( Phrase p_phrase )
{
    m_phrases[p_phrase.mode].push_back( std::move( p_phrase ) );
}

std::span<const Phrase> PhraseBook::phrasesFor( Mode p_mode ) const noexcept
{
    const auto entry = m_phrases.find( p_mode );

    if( entry == m_phrases.end() )
    {
        return {};
    }

    return entry->second;
}

std::optional<Phrase> PhraseBook::drawPhraseFor( Mode p_mode, std::mt19937 & p_randomEngine ) const
{
    const std::span<const Phrase> phrases = phrasesFor( p_mode );

    if( phrases.empty() )
    {
        return std::nullopt;
    }

    // Le tirage se fait sur les INDICES, et la borne est celle du nombre de phrases : tirer sur les sept modes pour
    // tomber sur celui qui est vide serait une facon detournee de ne pas repondre.
    const auto lastIndex = static_cast<std::int32_t>( phrases.size() ) - 1;

    std::uniform_int_distribution<std::int32_t> distribution{ 0, lastIndex };

    return phrases[static_cast<std::size_t>( distribution( p_randomEngine ) )];
}

std::size_t PhraseBook::phraseCount() const noexcept
{
    std::size_t count = 0;

    for( const auto & entry : m_phrases )
    {
        count += entry.second.size();
    }

    return count;
}

std::size_t PhraseBook::phraseCountFor( Mode p_mode ) const noexcept
{
    return phrasesFor( p_mode ).size();
}

}    // namespace musichien::domain
