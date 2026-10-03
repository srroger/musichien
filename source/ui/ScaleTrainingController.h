#pragma once

// =====================================================================================================================
// Musichien - ScaleTrainingController
//
// LA PAGE DES GAMMES : une gamme s'affiche dans le cercle, s'entend sur un bourdon, et le joueur la NOMME.
//
// DEDIEE AUX CONNAISSEURS qui veulent pratiquer - c'est le seul endroit du jeu ou l'on suppose la connaissance. Ailleurs on
// enseigne ; ici on entretient. Elle ne partage donc RIEN avec le moteur de questions : pas de session, pas de vies, pas
// de journal, aucun score sauvegarde. C'est ce qui la rend inoffensive - elle peut tomber sans rien casser autour.
//
// CE QU'ELLE REUTILISE, EN REVANCHE : le cercle (ModeCircle), qui ne sait rien de ce qu'il dessine, et la lecture melodie
// sur bourdon, qui marche pour n'importe quelle gamme.
// =====================================================================================================================

#include "domain/audio/NotePlayer.h"
#include "domain/music/Scale.h"

#include <QObject>
#include <QString>
#include <QVariantList>

#include <array>
#include <random>

namespace musichien::ui
{

class ScaleTrainingController : public QObject
{
    Q_OBJECT

public:
    explicit ScaleTrainingController( domain::NotePlayer & p_notePlayer, QObject * p_parent = nullptr );

    // Le cercle : douze pastilles, celles de la gamme allumees, la tonique en haut.
    Q_PROPERTY( QVariantList circle READ circle NOTIFY questionChanged )

    // Les noms proposes, dans un ordre TIRE a chaque question : un bouton qui garderait sa place apprendrait au joueur OU
    // cliquer au lieu de ce qu'il entend.
    Q_PROPERTY( QVariantList choices READ choices NOTIFY questionChanged )

    // Ce que le joueur a repondu, et ce que c'etait - une fois la reponse donnee seulement.
    Q_PROPERTY( bool hasAnswered READ hasAnswered NOTIFY questionChanged )
    Q_PROPERTY( int answeredIndex READ answeredIndex NOTIFY questionChanged )
    Q_PROPERTY( int correctIndex READ correctIndex NOTIFY questionChanged )
    Q_PROPERTY( bool wasLastAnswerCorrect READ wasLastAnswerCorrect NOTIFY questionChanged )
    Q_PROPERTY( QString verdict READ verdict NOTIFY questionChanged )

    // Le compte de la seance. Ce n'est pas une partie - pas de vies, pas de fin - c'est un entrainement, et le joueur doit
    // pouvoir mesurer ce qu'il vient de faire.
    Q_PROPERTY( int askedCount READ askedCount NOTIFY scoreChanged )
    Q_PROPERTY( int correctCount READ correctCount NOTIFY scoreChanged )

    [[nodiscard]] QVariantList circle() const { return m_circle; }
    [[nodiscard]] QVariantList choices() const { return m_choices; }
    [[nodiscard]] bool hasAnswered() const noexcept { return m_hasAnswered; }
    [[nodiscard]] int answeredIndex() const noexcept { return m_answeredIndex; }
    [[nodiscard]] int correctIndex() const noexcept { return m_correctIndex; }
    [[nodiscard]] bool wasLastAnswerCorrect() const noexcept { return m_wasCorrect; }
    [[nodiscard]] QString verdict() const;

    [[nodiscard]] int askedCount() const noexcept { return m_askedCount; }
    [[nodiscard]] int correctCount() const noexcept { return m_correctCount; }

    // OUVRIR LA PAGE : une gamme au hasard, jouee. C'est le seul geste d'entree.
    Q_INVOKABLE void start();

    // LA REECOUTE, autant de fois qu'on veut : c'est le coeur de l'entrainement - on ne devine pas, on ecoute jusqu'a
    // entendre.
    Q_INVOKABLE void playAgain();

    Q_INVOKABLE void answer( int p_index );

    // La question suivante. Rien n'empeche de repondre faux : il n'y a pas de fin, et c'est voulu.
    Q_INVOKABLE void next();

signals:
    void questionChanged();

    void scoreChanged();

    // Le son part : l'ecran peut alors faire ce qu'il veut de son dessin, et il le fait sans deviner quand.
    void playbackStarted();

private:
    void drawQuestion();

    void playCurrentScale();

    domain::NotePlayer & m_notePlayer;

    domain::Scale m_scale{ domain::Scale::PentatonicMinor };

    domain::Note m_tonic{ 38 };

    QVariantList m_circle;

    QVariantList m_choices;

    int m_correctIndex{ 0 };
    int m_answeredIndex{ -1 };
    bool m_hasAnswered{ false };
    bool m_wasCorrect{ false };

    int m_askedCount{ 0 };
    int m_correctCount{ 0 };

    std::mt19937 m_randomEngine;
};

}    // namespace musichien::ui
