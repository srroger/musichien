#include "domain/music/TunerGuide.h"

#include <algorithm>
#include <utility>

namespace musichien::domain
{

namespace
{

// Le rang d'un temperament dans l'enumeration, qui est aussi celui des noms et des textes. Passer par un tableau de
// correspondance, plutot que par un static_cast, laisse le compilateur refuser un temperament ajoute a l'enumeration
// sans qu'on ait dit ou le ranger.
[[nodiscard]] std::size_t indexOf( Temperament p_temperament ) noexcept
{
    switch( p_temperament )
    {
        case Temperament::Equal:
            return 0;
        case Temperament::Pythagorean:
            return 1;
        case Temperament::Just:
            return 2;
    }

    return 0;
}

}    // namespace

void TunerGuide::setTemperamentText( Temperament p_temperament, std::string p_text )
{
    m_temperamentTexts.at( indexOf( p_temperament ) ) = std::move( p_text );
}

std::string_view TunerGuide::temperamentText( Temperament p_temperament ) const noexcept
{
    return m_temperamentTexts.at( indexOf( p_temperament ) );
}

void TunerGuide::setDiapasonText( std::string p_text )
{
    m_diapasonText = std::move( p_text );
}

void TunerGuide::setReferenceNoteText( std::string p_text )
{
    m_referenceNoteText = std::move( p_text );
}

void TunerGuide::addHowToStep( std::string p_step )
{
    m_howToSteps.push_back( std::move( p_step ) );
}

std::size_t TunerGuide::temperamentCount() const noexcept
{
    return static_cast<std::size_t>(
      std::ranges::count_if( m_temperamentTexts, []( const std::string & p_text ) { return !p_text.empty(); } ) );
}

bool TunerGuide::isEmpty() const noexcept
{
    return ( temperamentCount() == 0 ) && m_diapasonText.empty() && m_referenceNoteText.empty() && m_howToSteps.empty();
}

}    // namespace musichien::domain
