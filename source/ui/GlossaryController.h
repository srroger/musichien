#pragma once

// =====================================================================================================================
// Musichien - GlossaryController
//
// LE GLOSSAIRE, VU PAR LA PAGE : une liste, et des lettres de section.
//
// Il ne lit aucun fichier : il recoit les mots du domaine et les range comme un DICTIONNAIRE FRANCAIS. Cette regle vit
// ici et non dans le domaine, parce qu'elle est d'AFFICHAGE - elle depend de la casse, des accents et de la langue - et
// qu'elle se teste sans interface.
//
// Roger, 04/10/2026 : « c'est range par ordre alphabetique, ca suffit. Normalement on devrait savoir utiliser un
// dictionnaire. [...] Pas de recherche, c'est inutile. » Il n'y a donc AUCUN champ de recherche ici, et ce n'est pas un
// oubli : trouver un mot dans un dictionnaire EST une competence, et l'application la laisse au joueur.
// =====================================================================================================================

#include "domain/lesson/Glossary.h"

#include <QObject>
#include <QString>
#include <QVariantList>

#include <vector>

namespace musichien::ui
{

class GlossaryController : public QObject
{
    Q_OBJECT

public:
    explicit GlossaryController( std::vector<domain::GlossaryEntry> p_entries, QObject * p_parent = nullptr );

    // Une ligne par mot : le mot, sa definition, de quoi dessiner les separateurs, et rien d'autre.
    Q_PROPERTY( QVariantList entries READ entries CONSTANT )
    Q_PROPERTY( int termCount READ termCount CONSTANT )

    [[nodiscard]] QVariantList entries() const { return m_entries; }

    [[nodiscard]] int termCount() const { return static_cast<int>( m_entries.size() ); }

    // LA LETTRE DE SECTION D'UN MOT : son initiale, SANS ACCENT, en capitale.
    //
    // « Echelle » se range sous E, jamais apres le V : un tri par point de code mettrait tous les mots accentues a la
    // fin, et un dictionnaire francais qui commencerait par A pour finir par les mots en E accentue serait une drole de
    // dictionnaire.
    //
    // Publiee et statique pour etre TESTEE directement : la page ne fait que l'afficher.
    [[nodiscard]] static QString sectionLetterFor( const QString & p_word );

private:
    QVariantList m_entries;
};

}    // namespace musichien::ui