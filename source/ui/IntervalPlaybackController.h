#pragma once

// =====================================================================================================================
// Musichien - IntervalPlaybackController
//
// The view model of the landing screen, and the ONLY bridge between the interface and the domain.
//
// It is deliberately thin, and that is the point:
//   * it does not build an interval, it asks the domain to build it;
//   * it does not produce a sound, it asks the domain port to play it.
//
// Consequently this class contains no rule of the game, and no detail about the audio stack. It holds
// references only, so it can be created on the stack in main() and then injected into QML.
//
// See docs/CODE_CONVENTIONS.md, section "QML", and docs/ARCHITECTURE.md.
// =====================================================================================================================

#include "domain/audio/NotePlayer.h"
#include "domain/music/Interval.h"

#include <QObject>
#include <QString>

namespace musichien::ui
{

class IntervalPlaybackController final : public QObject
{
    Q_OBJECT

    // Name of the last interval played, so that the screen can display what was heard.
    Q_PROPERTY( QString lastPlayedIntervalName READ lastPlayedIntervalName NOTIFY
                  lastPlayedIntervalChanged )

public:
    explicit IntervalPlaybackController( domain::NotePlayer & p_notePlayer, QObject * p_parent = nullptr );

    [[nodiscard]] QString lastPlayedIntervalName() const;

    // Plays a single note: the very first thing a beginner learns to recognise.
    Q_INVOKABLE void playSingleNote();

    // Plays a perfect fifth above the middle C, the two notes at the same time.
    Q_INVOKABLE void playPerfectFifth();

    // Plays the same fifth as two successive notes, which is how it is usually taught first.
    Q_INVOKABLE void playMelodicFifth();

    // Stops every sound. Called when the screen is left: an audio stream left open on a phone drains
    // the battery.
    Q_INVOKABLE void stopPlayback();

signals:
    void lastPlayedIntervalChanged();

private:
    // Updates the displayed name and notifies the interface when it actually changed.
    void setLastPlayedIntervalName( QString p_intervalName );

    domain::NotePlayer & m_notePlayer;
    QString m_lastPlayedIntervalName;
};

}    // namespace musichien::ui
