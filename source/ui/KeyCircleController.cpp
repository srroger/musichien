#include "ui/KeyCircleController.h"

#include "domain/music/KeySignature.h"

#include <QString>

#include <cmath>
#include <cstddef>
#include <span>
#include <string_view>

namespace musichien::ui
{

namespace
{

// Le nom d'une qualite d'accord, tel qu'un joueur le lit. La liste est COURTE et fermee, parce qu'une gamme majeure ne
// porte que trois qualites - c'est meme ce qui la definit.
[[nodiscard]] QString qualityName( domain::DegreeQuality p_quality )
{
    switch( p_quality )
    {
        case domain::DegreeQuality::Major:
            return KeyCircleController::tr( "majeur" );

        case domain::DegreeQuality::Minor:
            return KeyCircleController::tr( "mineur" );

        case domain::DegreeQuality::Diminished:
            return KeyCircleController::tr( "diminué" );
    }

    return {};
}

// L'ARMURE, telle qu'un musicien l'ecrit : « 3 ♯ », « 2 ♭ », et « aucune » pour do majeur.
//
// Le PLURIEL compte : une armure d'un seul diese s'ecrit « 1 ♯ » et se lit « un diese », et le mot juste vaut mieux que
// le nombre nu. Le symbole porte le sens, donc le mot n'a pas a le repeter.
[[nodiscard]] QString armureText( const domain::KeySignature & p_key )
{
    const std::int32_t count = p_key.accidentalCount();

    if( count == 0 )
    {
        return KeyCircleController::tr( "aucune altération" );
    }

    const QString symbol = ( count > 0 ) ? QStringLiteral( "♯" ) : QStringLiteral( "♭" );

    return QStringLiteral( "%1 %2" ).arg( std::abs( count ) ).arg( symbol );
}

[[nodiscard]] QVariantMap describeKey( const domain::KeySignature & p_key, int p_index )
{
    const auto name = p_key.name();

    QVariantMap description;

    description.insert( QStringLiteral( "index" ), p_index );
    description.insert( QStringLiteral( "name" ), QString::fromUtf8( name.data(), static_cast<int>( name.size() ) ) );

    const auto minor = p_key.relativeMinorName();

    description.insert( QStringLiteral( "relativeMinor" ),
                        QString::fromUtf8( minor.data(), static_cast<int>( minor.size() ) ) );
    description.insert( QStringLiteral( "armure" ), armureText( p_key ) );

    // Le nombre d'accidents SIGNE, pour que l'ecran puisse peindre une case plus claire ou plus sombre selon le cote du
    // cercle ou elle se trouve : les bemols d'un cote, les dieses de l'autre, et do au milieu.
    description.insert( QStringLiteral( "accidentals" ), p_key.accidentalCount() );

    return description;
}

}    // namespace

KeyCircleController::KeyCircleController( QObject * p_parent )
  : QObject{ p_parent }
{
    // La liste est construite UNE fois, a partir du domaine, et l'ecran ne la compose jamais : ajouter une tonalite au
    // cercle est ce qui ferait apparaitre une case, et rien d'autre.
    const std::span<const domain::KeySignature> keys = domain::circleOfFifths();

    for( std::size_t index = 0; index < keys.size(); ++index )
    {
        // L'index est borne par la boucle, et l'acces se fait donc par l'operateur : std::span::at() n'existe que dans
        // un C++26 tres recent, et le compilateur du NDK Android ne l'a pas encore. C'est la meme lecon que pour
        // mixMelodyOverDrone, et il a fallu qu'un build Android la repete.
        m_keys.append( describeKey( keys[index], static_cast<int>( index ) ) );
    }
}

QVariantList KeyCircleController::keys() const
{
    return m_keys;
}

QVariantMap KeyCircleController::selectedKey() const
{
    if( ( m_selectedIndex < 0 ) || ( m_selectedIndex >= m_keys.size() ) )
    {
        return {};
    }

    return m_keys.at( m_selectedIndex ).toMap();
}

QVariantList KeyCircleController::degrees() const
{
    QVariantList degrees;

    if( m_selectedIndex < 0 )
    {
        return degrees;
    }

    const auto key = static_cast<std::size_t>( m_selectedIndex );

    const std::span<const domain::KeySignature> keys = domain::circleOfFifths();

    if( key >= keys.size() )
    {
        return degrees;
    }

    // Les sept degres, dans l'ordre : c'est la suite que l'oreille rencontre en montant la gamme, et c'est donc celle
    // qu'un ecran doit montrer.
    for( std::int32_t degree = 1; degree <= 7; ++degree )
    {
        const std::string_view numeral = domain::romanNumeral( degree );

        QVariantMap description;

        description.insert( QStringLiteral( "degree" ), degree );
        description.insert( QStringLiteral( "numeral" ),
                            QString::fromUtf8( numeral.data(), static_cast<int>( numeral.size() ) ) );
        description.insert( QStringLiteral( "quality" ), qualityName( domain::degreeQuality( degree ) ) );

        // La QUALITE est aussi donnee sous forme de nombre, parce qu'un ecran qui peint doit pouvoir trier sans lire du
        // texte traduit.
        description.insert( QStringLiteral( "qualityIndex" ), static_cast<int>( domain::degreeQuality( degree ) ) );

        degrees.append( description );
    }

    return degrees;
}

void KeyCircleController::selectKey( int p_index )
{
    if( ( p_index < 0 ) || ( p_index >= m_keys.size() ) )
    {
        return;
    }

    if( p_index == m_selectedIndex )
    {
        // On ne notifie que si quelque chose change : retoucher la meme case ne doit pas faire clignoter l'ecran.
        return;
    }

    m_selectedIndex = p_index;

    emit selectionChanged();
}

}    // namespace musichien::ui
