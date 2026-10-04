#include "ui/GlossaryController.h"

#include <QVariantMap>

#include <algorithm>

namespace musichien::ui
{

namespace
{

// LA CLE DE TRI : minuscules, sans accents, sans espaces, sans tirets ni apostrophes.
//
// « Demi-ton » doit se ranger a « demiton », et « A l'oreille » a « aloreille » : un dictionnaire ne trie pas sur les
// SIGNES, il trie sur les SONS. Qt fait ici la seule partie difficile a la main - la decomposition des accents.
[[nodiscard]] QString sortKeyFor( const QString & p_word )
{
    const QString decomposed = p_word.toLower().normalized( QString::NormalizationForm_D );

    QString key;
    key.reserve( decomposed.size() );

    for( const QChar character : decomposed )
    {
        // Decompose, un accent est un caractere A PART, de categorie « marque sans chasse ». Le retirer est exactement
        // ce que « sans accent » veut dire - et ca marche pour e, e, a, c, u comme pour celles qu'on oublie.
        if( character.category() == QChar::Mark_NonSpacing )
        {
            continue;
        }

        if( character.isSpace() || ( character == QLatin1Char( '-' ) ) || ( character == QLatin1Char( '\'' ) ) )
        {
            continue;
        }

        key.append( character );
    }

    return key;
}

}    // namespace

QString GlossaryController::sectionLetterFor( const QString & p_word )
{
    const QString key = sortKeyFor( p_word );

    if( key.isEmpty() )
    {
        return {};
    }

    return key.left( 1 ).toUpper();
}

GlossaryController::GlossaryController( std::vector<domain::GlossaryEntry> p_entries, QObject * p_parent )
  : QObject{ p_parent }
{
    // LE TRI SE FAIT ICI, une fois, et non a chaque affichage. Le fichier peut ranger ses mots n'importe comment - et il
    // le fera, parce qu'un auteur ajoute toujours le mot nouveau A LA FIN.
    std::sort( p_entries.begin(),
               p_entries.end(),
               []( const domain::GlossaryEntry & p_left, const domain::GlossaryEntry & p_right ) {
                   return sortKeyFor( QString::fromStdString( p_left.word ) )
                          < sortKeyFor( QString::fromStdString( p_right.word ) );
               } );

    QString previousLetter;

    for( const domain::GlossaryEntry & entry : p_entries )
    {
        const QString word = QString::fromStdString( entry.word );
        const QString letter = sectionLetterFor( word );

        QVariantMap description;
        description.insert( QStringLiteral( "word" ), word );
        description.insert( QStringLiteral( "definition" ), QString::fromStdString( entry.definition ) );
        description.insert( QStringLiteral( "sectionLetter" ), letter );

        // LA PAGE N'A PAS A COMPARER DEUX LIGNES POUR SAVOIR OU POSER UNE LETTRE, et c'est delibere : cette comparaison
        // est la MEME regle que le tri, et la refaire en QML serait deux occasions de se desynchroniser.
        description.insert( QStringLiteral( "startsSection" ), letter != previousLetter );

        previousLetter = letter;

        m_entries.append( description );
    }
}

}    // namespace musichien::ui