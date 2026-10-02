#pragma once

// =====================================================================================================================
// Musichien - ModePreviewController
//
// Le banc d'essai des modes : sept boutons, un par mode, ranges du plus clair au plus sombre.
//
// Il suit exactement le modele de IntervalPlaybackController, et pour la meme raison : c'est une classe FINE. Elle ne
// compose pas une gamme, elle demande au domaine de la composer ; elle ne nomme pas un mode, elle demande au domaine
// son identifiant ; elle ne fabrique aucun son, elle demande au PORT de le jouer.
//
// ---------------------------------------------------------------------------------------------------------------------
// Ce que ce banc d'essai a de particulier, et c'est tout l'interet
//
// Un mode ne s'entend pas tout seul : il lui faut un CENTRE. C'est la raison pour laquelle le banc d'essai ne joue pas
// une gamme nue - il demande au port une melodie SUR UN BOURDON, et c'est le domaine qui decide quelle quinte tenir et
// combien de temps la tenir. Le banc d'essai se contente de dire : « ce mode, s'il te plait ».
//
// Consequence heureuse : ce que le banc d'essai fait entendre est EXACTEMENT ce que l'exercice fera entendre, jusqu'au
// timbre du bourdon, qui est tire par l'adaptateur. Un banc d'essai qui sonnerait autrement que le jeu serait pire
// qu'inutile.
// =====================================================================================================================

#include "domain/audio/NotePlayer.h"
#include "domain/exercise/PlayerPreferences.h"
#include "domain/music/Mode.h"
#include "domain/music/PhraseBook.h"
#include "domain/music/Temperament.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include <random>

namespace musichien::ui
{

class ModePreviewController final : public QObject
{
    Q_OBJECT

    // Les sept modes, dans l'ordre de clarte : la liste vient du DOMAINE, et l'ecran ne fait que la lire.
    Q_PROPERTY( QVariantList modes READ modes CONSTANT )

    // Le mode entendu en dernier, ou une carte vide tant que rien n'a sonne. C'est ce qui permet a l'ecran de colorer
    // le bouton qui a vraiment ete JOUE, et non celui qui a ete touche.
    Q_PROPERTY( QVariantMap lastPlayedMode READ lastPlayedMode NOTIFY lastPlayedModeChanged )

    // La PHRASE qui vient de sonner : ses degres - « 1 4(2) 5 1 » - et son tempo, ou une carte vide quand rien n'a
    // sonne ou que le mode n'a pas de phrase.
    //
    // C'est ce qui relie ce que l'oreille entend a ce qu'un musicien ecrirait : un exercice sur une couleur qu'on ne
    // peut pas relire d'un chiffre n'apprend rien de plus qu'une devinette.
    Q_PROPERTY( QVariantMap lastPlayedPhrase READ lastPlayedPhrase NOTIFY lastPlayedPhraseChanged )

    // Le cercle des quintes DU MODE ENTENDU : ses sept notes allumees, les autres eteintes, la tonique marquee.
    //
    // C'est l'aide visuelle que Roger a demandee. Ici elle ne cache rien : le banc d'essai fait ecouter a loisir, et voir
    // la fenetre de notes pendant qu'on l'entend est exactement ce qui apprend a la reconnaitre.
    Q_PROPERTY( QVariantList playedModeCircle READ playedModeCircle NOTIFY playedModeCircleChanged )

    // COMBIEN DE TEMPS la gamme du dernier mode a sonne, en millisecondes.
    //
    // La roue s'en sert pour faire voyager sa tete de note en note : la duree vient du CONTROLEUR, donc du domaine et de
    // son tempo, et une constante ecrite dans le QML mentirait le jour ou le tempo change.
    Q_PROPERTY( int playbackDurationMs READ playbackDurationMs NOTIFY playedModeCircleChanged )

public:
    explicit ModePreviewController( domain::NotePlayer & p_notePlayer, QObject * p_parent = nullptr );

    [[nodiscard]] QVariantList modes() const;
    [[nodiscard]] QVariantMap lastPlayedMode() const;
    [[nodiscard]] QVariantMap lastPlayedPhrase() const;
    [[nodiscard]] QVariantList playedModeCircle() const;

    [[nodiscard]] int playbackDurationMs() const noexcept { return m_lastScaleDurationMs; }

    // Le temperament et le diapason, donnes par la couche de cablage - comme pour les intervalles.
    void setTuning( domain::TuningContext p_tuning );

    // Fait entendre un mode : la gamme montee puis descendue, sur un bourdon TENU.
    //
    // L'index est celui de la liste ci-dessus, donc aussi celui de domain::Mode : un ecran qui lit la liste ne peut pas
    // demander un mode qui n'existe pas.
    Q_INVOKABLE void playMode( int p_index );

    // Le livre des phrases modales, donne par la couche de cablage comme le temperament et le diapason.
    //
    // Nul tant qu'il n'a pas ete donne, et ce n'est pas un cas d'erreur : un contenu de phrases absent ou casse doit
    // laisser un banc d'essai qui fonctionne, simplement sans phrases.
    void setPhraseBook( const domain::PhraseBook & p_phraseBook );

    // Les REGLAGES du joueur, quand il y en a : le tempo des phrases de mode vit la, et c'est la seule chose que le banc
    // d'essai ait besoin de savoir du profil. Un pointeur nul est normal - un banc d'essai sans profil joue au tempo du
    // contenu.
    void setPreferences( const domain::PlayerPreferences & p_preferences );

    Q_INVOKABLE void stopPlayback();

    // Joue une PHRASE de ce mode, tiree dans le contenu.
    //
    // Rien ne sonne quand le contenu n'a pas de phrase pour ce mode, et ce n'est pas une erreur : c'est un mode que
    // l'atelier n'a pas encore servi. L'ecran, lui, n'offre pas le bouton (voir phraseCountForMode).
    Q_INVOKABLE void playPhraseOfMode( int p_index );

    // Combien de phrases le contenu porte pour ce mode. Un mode sans phrase n'est pas un cas particulier : il repond
    // zero, et l'ecran en tire ce qu'il veut.
    [[nodiscard]] Q_INVOKABLE int phraseCountForMode( int p_index ) const;

signals:
    void lastPlayedModeChanged();
    void lastPlayedPhraseChanged();
    void playedModeCircleChanged();

private:
    domain::NotePlayer & m_notePlayer;
    QVariantList m_modes;
    QVariantMap m_lastPlayedMode;
    QVariantMap m_lastPlayedPhrase;
    QVariantList m_playedModeCircle;

    // La duree de la derniere gamme jouee, en millisecondes. Voir playbackDurationMs.
    int m_lastScaleDurationMs{ 0 };

    // Les reglages du joueur : nul tant qu'ils n'ont pas ete donnes, et c'est un cas normal.
    const domain::PlayerPreferences * m_preferences{ nullptr };

    // Allume le cercle d'un mode sur une tonique donnee, et ne notifie que s'il a change : rejouer le meme mode ne doit
    // pas faire redessiner l'ecran pour rien.
    void showCircleFor( domain::Mode p_mode, std::int32_t p_tonicPitchClass );

    // Un POINTEUR, et non une copie : le livre vit aussi longtemps que l'application, et le copier ici en dupliquerait
    // les trois cents phrases pour rien.
    const domain::PhraseBook * m_phraseBook{ nullptr };

    // Le tirage d'une phrase. Le controleur n'est pas le domaine : il a le droit a une graine tiree au demarrage, et
    // c'est meme ce qui fait qu'une seance ne donne pas toujours la premiere phrase du meme mode.
    std::mt19937 m_randomEngine{ std::random_device{}() };

    // Equal temperament at 440 Hz until the wiring layer says otherwise.
    domain::TuningContext m_tuning;
};

}    // namespace musichien::ui
