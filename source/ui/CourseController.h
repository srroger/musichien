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

    // LES CONCEPTS DU COURS OUVERT, en distances - ce que la carte « :: essai » met dans la palette du joueur.
    //
    // Le domaine les portait deja (Course::concepts), ils etaient LUS mais jamais consommes. C'est le lien que
    // Course.h annoncait - « what this lesson teaches, as distances in semitones » - et il sert desormais : l'essai
    // ouvre un entrainement dont la palette contient exactement ce que la lecon enseigne.
    Q_PROPERTY( QVariantList currentConcepts READ currentConcepts NOTIFY courseChanged )

    [[nodiscard]] bool isReading() const noexcept { return m_readingIndex >= 0; }

    [[nodiscard]] QString title() const;

    [[nodiscard]] QString subtitle() const;

    [[nodiscard]] QVariantList blocks() const;

    [[nodiscard]] QVariantList currentConcepts() const;

    // LA PAGE OU L'ON SE TROUVE, ET COMBIEN IL Y EN A.
    //
    // Roger : « je trouve que le cours en forme de grosse note qui descend, c'est bien mais un peu lourd. Je verrais plus
    // ca comme plusieurs pages par chapitre. Et a la fin, on verrait la note complete pour s'y referer. »
    //
    // Les pages ne sont pas inventees ici : ce sont les « ## » du fichier, ecrits par l'auteur. Le controleur ne fait que
    // choisir laquelle on lit, et proposer la note entiere a la fin.
    Q_PROPERTY( int sectionCount READ sectionCount NOTIFY courseChanged )
    Q_PROPERTY( int sectionIndex READ sectionIndex NOTIFY courseChanged )
    Q_PROPERTY( QString sectionTitle READ sectionTitle NOTIFY courseChanged )
    Q_PROPERTY( bool showingWholeNote READ isShowingWholeNote NOTIFY courseChanged )

    [[nodiscard]] int sectionCount() const noexcept;
    [[nodiscard]] int sectionIndex() const noexcept { return m_sectionIndex; }
    [[nodiscard]] QString sectionTitle() const;
    [[nodiscard]] bool isShowingWholeNote() const noexcept { return m_showingWholeNote; }

    // Tourner la page. Au bout, on ne deborde pas : la derniere page propose la note complete, et c'est tout.
    Q_INVOKABLE void nextSection();
    Q_INVOKABLE void previousSection();

    // LA NOTE COMPLETE, pour s'y referer - et le retour a la lecture par pages.
    Q_INVOKABLE void setShowingWholeNote( bool p_showingWholeNote );

    // OUVRIR UN COURS, LE FERMER. Ouvrir un index qui n'existe pas ne fait rien : une page ne doit jamais pouvoir
    // planter sur un clic de trop.
    Q_INVOKABLE void open( int p_index );

    // OUVRIR UNE ANNEXE PAR SON NOM. Le nom est celui que le cours cite, et le contrat du domaine dit lequel : le TITRE
    // de l'annexe, tel que son propre front matter le donne (voir CourseBlock::annexeName).
    //
    // Un nom qui ne correspond a rien ne fait RIEN : une carte morte vaut mieux qu'une page qui plante. Mais un renvoi
    // qui ne mene nulle part se dit AU DEMARRAGE - le constructeur le signale, et c'est la seule facon de voir une
    // faute de frappe qu'aucun compilateur ne peut voir.
    Q_INVOKABLE void openAnnexe( const QString & p_name );

    Q_INVOKABLE void close();

    // CE QUE LE JEU SAIT JOUER, LE JEU LE JOUE : une carte ':: jeu' passe par ici, sans reseau et sans attente.
    //
    // La direction arrive en entier plutot qu'en mot : le QML transporte deja l'entier du domaine, et le traduire deux
    // fois serait deux occasions de se tromper.
    Q_INVOKABLE void playInterval( int p_semitones, int p_direction );

    // LA SERIE HARMONIQUE D'UNE NOTE, du fondamental au sixieme rang.
    //
    // Roger, sur l'os a macher de la quinte : « je mettrais bien un bouton qui joue les notes dont on parle ». Tout ce
    // chapitre repose sur cette serie, et l'ENTENDRE vaut mieux que la lire dans un tableau.
    //
    // Six rangs et pas sept : le septieme est faux, et il n'a rien a faire dans une demonstration dont le sujet est que
    // l'oreille reconnait les autres.
    Q_INVOKABLE void playHarmonicSeries();

    // LE BOURDON DU JEU, TENU CINQ SECONDES : la tonique et sa quinte, sans tierce.
    //
    // Meme formule que les questions de couleur - un centre qui ne colore rien lui-meme. Le chapitre du centre ne peut
    // pas s'enseigner sans lui : un centre qu'on ne TIENT pas ne s'entend pas.
    Q_INVOKABLE void playDrone();

    // LE CERCLE DES QUINTES, PARCOURU : p_fifthCount quintes enchainees, chaque note ramenee dans l'octave de depart.
    //
    // Sept montrent d'ou vient une gamme ; douze font le tour. Le repli dans l'octave est ce qui transforme une fusee
    // montante en CERCLE.
    Q_INVOKABLE void playFifthCycle( int p_fifthCount );

    // LA GAMME D'UN MODE, SUR LE BOURDON : la meme chose que le banc d'essai des modes, et volontairement.
    Q_INVOKABLE void playModeScale( int p_modeIndex );

    // UN ACCORD, JOUE : la tonique du jeu, et les intervalles de sa qualite. C'est la carte ':: accord' du chapitre de
    // la couleur - le majeur et le mineur, entendus avant d'etre nommes.
    Q_INVOKABLE void playChord( int p_quality );

    Q_INVOKABLE void stopPlayback();

signals:
    void courseChanged();

private:
    domain::NotePlayer & m_notePlayer;

    // LE TITRE TEL QU'IL S'AFFICHE, NUMERO COMPRIS - « 1. La quinte juste ».
    //
    // Le numero vient du RANG dans le catalogue, jamais du fichier : un titre qui porterait son propre numero se
    // desynchroniserait le jour ou deux lecons s'echangent. Un os a macher (chapitre zero) n'en porte pas.
    [[nodiscard]] QString displayTitleFor( std::size_t p_index ) const;

    std::vector<domain::Course> m_courses;

    QVariantList m_library;

    // -1 : aucun cours ouvert.
    int m_readingIndex{ -1 };

    // La page en cours, et l'etat « note complete ». Remis a zero a chaque ouverture de cours : on commence au debut, et
    // on commence par pages.
    int m_sectionIndex{ 0 };
    bool m_showingWholeNote{ false };
};

}    // namespace musichien::ui
