// =====================================================================================================================
// Musichien - RhythmController
//
// The view model of the rhythm game and the metronome, and the ONLY bridge between the interface and the domain.
//
// It owns three things the domain refuses to:
//   * the CLOCK - the domain judges a tap against a time it is GIVEN, never a time it measured;
//   * the SOUND - beating the metronome is playing, and playing is the NotePlayer's job;
//   * the SCORE - the series and the points are a game feel of their own, and the domain only says Perfect/Good/Miss.
//
// The metronome is the same loop as the game: start it, and every tap is judged; leave it stopped, and taps are read
// as a tempo (tap tempo). That is what makes the page a musician's tool and a game at once.
// =====================================================================================================================

#pragma once

#include "domain/audio/NotePlayer.h"
#include "domain/rhythm/Rhythm.h"
#include "domain/rhythm/RhythmPattern.h"

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>

#include <cstdint>

namespace musichien::ui
{

class RhythmController final : public QObject
{
    Q_OBJECT

    // The tempo, in beats per minute.
    Q_PROPERTY( int bpm READ bpm WRITE setBpm NOTIFY bpmChanged )

    // How many beats a bar holds - the numerator of the time signature, from 1 to 12. Four by default.
    Q_PROPERTY( int beatsPerBar READ beatsPerBar WRITE setBeatsPerBar NOTIFY beatsPerBarChanged )

    // Whether the metronome is beating.
    Q_PROPERTY( bool isRunning READ isRunning NOTIFY isRunningChanged )

    // The score, the series, and the quality of the last tap (0 = Miss, 1 = Good, 2 = Perfect).
    Q_PROPERTY( int score READ score NOTIFY scoreChanged )
    Q_PROPERTY( int combo READ combo NOTIFY comboChanged )
    Q_PROPERTY( int lastQuality READ lastQuality NOTIFY lastQualityChanged )

    // The beat sounding now, 0 = the downbeat. A screen shows it plus one, so a musician counts 1, 2, 3, 4.
    Q_PROPERTY( int beatInBar READ beatInBar NOTIFY beatInBarChanged )

    // Les cellules rythmiques offertes : "Metronome seul" d'abord, puis les cliches. Le QML affiche la liste telle
    // quelle, et l'index choisi EST celui de la liste - aucune traduction d'index a faire dans l'interface.
    Q_PROPERTY( QVariantList patterns READ patterns CONSTANT )
    Q_PROPERTY( int currentPattern READ currentPattern WRITE setCurrentPattern NOTIFY currentPatternChanged )

public:
    explicit RhythmController( domain::NotePlayer & p_notePlayer, QObject * p_parent = nullptr );

    [[nodiscard]] int bpm() const noexcept { return m_bpm; }
    Q_INVOKABLE void setBpm( int p_bpm );

    [[nodiscard]] int beatsPerBar() const noexcept { return m_beatsPerBar; }
    Q_INVOKABLE void setBeatsPerBar( int p_beats );

    [[nodiscard]] bool isRunning() const noexcept { return m_isRunning; }
    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();

    [[nodiscard]] int score() const noexcept { return m_score; }
    [[nodiscard]] int combo() const noexcept { return m_combo; }
    [[nodiscard]] int lastQuality() const noexcept { return m_lastQuality; }
    [[nodiscard]] int beatInBar() const noexcept { return m_beatInBar; }

    // One tap. Judged against the expected beat when the metronome runs, read as a tempo when it is stopped.
    Q_INVOKABLE void tap();

    // Frappe un élément de la batterie : 0 = grosse caisse, 1 = caisse claire, 2 = charleston, 3 = tom.
    Q_INVOKABLE void playDrum( int p_drumIndex );

    [[nodiscard]] QVariantList patterns() const;

    [[nodiscard]] int currentPattern() const noexcept { return m_currentPattern; }
    Q_INVOKABLE void setCurrentPattern( int p_index );

signals:
    void bpmChanged();
    void beatsPerBarChanged();
    void isRunningChanged();
    void scoreChanged();
    void comboChanged();
    void lastQualityChanged();
    void beatInBarChanged();
    void currentPatternChanged();

private:
    // Le RAFRAICHISSEMENT DE L'AFFICHAGE, et rien d'autre.
    //
    // Il lit le temps du flux audio et met la page a jour : le numero du temps, l'accent, et les frappes de la cellule
    // a venir. Il ne declenche AUCUN son - c'est toute la difference avec ce qu'il remplace, et la raison pour laquelle
    // la page peut etre occupee a peindre sans que le metronome s'en apercoive.
    void refreshFromAudioClock();

    // Joue les frappes de la cellule qui tombent dans le temps p_beatIndex, a leur position EXACTE dans la mesure.
    // Les frappes decalees - les syncopes - partent a leur position, et non « dans un instant ».
    void schedulePatternHitsForBeat( std::int64_t p_beatIndex );

    // La cellule en cours, ou rien quand seul le metronome joue.
    [[nodiscard]] const domain::RhythmPattern * activePattern() const;

    // L'heure d'un temps dans la mesure, en millisecondes depuis le premier temps du metronome : la base des positions
    // de la cellule.
    [[nodiscard]] double positionOfBeat( std::int64_t p_beatIndex ) const noexcept;

    domain::NotePlayer & m_notePlayer;

    // Le rafraichissement de l'affichage. Sa precision n'a AUCUNE importance : il ne fait que repeindre.
    QTimer m_displayTimer;

    // L'horloge du TAP TEMPO, et d'elle seule : deux frappes a la main, et l'ecart entre elles donne le tempo. La ou le
    // metronome a besoin d'echantillons, deux frappes manuelles se contentent d'une horloge monotone.
    QElapsedTimer m_clock;

    // Le dernier temps traite, pour ne le traiter qu'UNE fois : le rafraichissement passe vingt fois par seconde, et
    // une cellule rejouee vingt fois par temps ne serait plus une cellule.
    std::int64_t m_lastBeatIndex{ -1 };

    std::int64_t m_lastTapMs{ -1 };

    int m_bpm{ 90 };
    int m_beatsPerBar{ 4 };
    bool m_isRunning{ false };
    int m_beatInBar{ 0 };
    int m_score{ 0 };
    int m_combo{ 0 };
    int m_lastQuality{ 0 };

    // 0 = le metronome seul, n+1 = la n-ieme cellule de domain::allRhythmPatterns().
    int m_currentPattern{ 0 };
};

}    // namespace musichien::ui
