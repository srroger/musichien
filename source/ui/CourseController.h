#pragma once

// =====================================================================================================================
// Musichien - CourseController
//
// L'ECOLE DES CHIOTS : la page des cours.
//
// Elle lit ce que l'application a lu au demarrage - une liste de cours lus dans les ressources - et en montre UN a la
// fois. Elle ne connait ni fichier, ni Markdown, ni chemin : le domaine lui donne des paragraphes et des cartes, et elle
// les traduit en ce que le QML sait dessiner.
//
// CE QU'ELLE NE FAIT PAS, ET C'EST VOULU : aucun rendu de Markdown (le QML le fait nativement), aucune regle de contenu,
// aucun acces reseau. Une carte qui sort de l'application ouvre un lien EXTERNE - c'est le systeme qui s'en charge, pas
// elle. La regle d'or est tenue ici comme ailleurs : ce que le jeu sait jouer, le jeu le joue.
//
// Voir le contrat, note 29 du Vault, et infrastructure/content/MarkdownCourse.h.
// =====================================================================================================================

#include "domain/audio/NotePlayer.h"
#include "domain/lesson/Course.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <vector>

namespace musichien::ui
{

class CourseController : public QObject
{
    Q_OBJECT

public:
    CourseController( domain::NotePlayer & p_notePlayer,
                      std::vector<domain::Course> p_courses,
                      QObject * p_parent = nullptr );

    // LE CATALOGUE : un element par cours, avec de quoi dessiner une ligne de sommaire.
    //
    // CONSTANT parce qu'il l'est : les cours sont lus une fois, au demarrage, et ne changent plus de la vie de
    // l'application. Un contenu qui change demande un rebuild - c'est le contrat, et il vaut mieux le dire.
    Q_PROPERTY( QVariantList library READ library CONSTANT )

    [[nodiscard]] QVariantList library() const;

    // LE COURS OUVERT. Rien quand aucun ne l'est - et c'est l'etat normal quand on arrive sur la page.
    Q_PROPERTY( bool reading READ isReading NOTIFY courseChanged )
    Q_PROPERTY( QString title READ title NOTIFY courseChanged )
    Q_PROPERTY( QString subtitle READ subtitle NOTIFY courseChanged )
    Q_PROPERTY( QVariantList blocks READ blocks NOTIFY courseChanged )

    [[nodiscard]] bool isReading() const noexcept { return m_readingIndex >= 0; }

    [[nodiscard]] QString title() const;

    [[nodiscard]] QString subtitle() const;

    [[nodiscard]] QVariantList blocks() const;

    // OUVRIR UN COURS, LE FERMER. Ouvrir un index qui n'existe pas ne fait rien : une page ne doit jamais pouvoir
    // planter sur un clic de trop.
    Q_INVOKABLE void open( int p_index );

    Q_INVOKABLE void close();

    // CE QUE LE JEU SAIT JOUER, LE JEU LE JOUE : une carte ':: jeu' passe par ici, sans reseau et sans attente.
    //
    // La direction arrive en entier plutot qu'en mot : le QML transporte deja l'entier du domaine, et le traduire deux
    // fois serait deux occasions de se tromper.
    Q_INVOKABLE void playInterval( int p_semitones, int p_direction );

    Q_INVOKABLE void stopPlayback();

signals:
    void courseChanged();

private:
    domain::NotePlayer & m_notePlayer;

    std::vector<domain::Course> m_courses;

    QVariantList m_library;

    // -1 : aucun cours ouvert.
    int m_readingIndex{ -1 };
};

}    // namespace musichien::ui
