#pragma once

// =====================================================================================================================
// Musichien - IntervalPlaybackController
//
// The view model of the landing screen, and the ONLY bridge between the interface and the domain.
//
// It is deliberately thin, and that is the point:
//   * it does not build an interval, it asks the domain to build it;
//   * it does not name an interval, it asks the domain for the name;
//   * it does not produce a sound, it asks the domain port to play it.
//
// Consequence: this class contains no rule of the game, and no detail about the audio stack. It holds
// references only, so it can be created on the stack in main() and then injected into QML.
//
// See docs/CODE_CONVENTIONS.md, section "QML", and docs/ARCHITECTURE.md.
// =====================================================================================================================

#include "domain/audio/NotePlayer.h"
#include "domain/music/Note.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace musichien::ui
{

class IntervalPlaybackController final : public QObject
{
    Q_OBJECT

    // Every interval the domain supports, described as plain maps for the interface.
    //
    // The list comes from the DOMAIN, never from the screen: the interface offers the buttons the
    // domain says exist, so it can neither invent an interval the domain would refuse to name, nor
    // forget one it knows. Constant, because it is built once and never changes afterwards.
    Q_PROPERTY( QVariantList supportedIntervals READ supportedIntervals CONSTANT )

    // What the domain says about the interval heard last, as a map of the same shape as one entry of
    // 'supportedIntervals'. EMPTY until an interval is played: a single note has nothing to name.
    Q_PROPERTY( QVariantMap lastPlayedInterval READ lastPlayedInterval NOTIFY lastPlayedIntervalChanged )

    // Whether the two notes are sounded together or one after the other.
    //
    // This is the ONE choice the interface is allowed to make, because it describes how to listen
    // rather than what the answer is: it changes nothing about the interval itself.
    Q_PROPERTY( bool harmonicPlayback READ harmonicPlayback WRITE setHarmonicPlayback NOTIFY
                  harmonicPlaybackChanged )

public:
    explicit IntervalPlaybackController( domain::NotePlayer & p_notePlayer, QObject * p_parent = nullptr );

    [[nodiscard]] QVariantList supportedIntervals() const;
    [[nodiscard]] QVariantMap lastPlayedInterval() const;

    [[nodiscard]] bool harmonicPlayback() const;
    void setHarmonicPlayback( bool p_harmonicPlayback );

    // Plays an interval at a given distance above the root note, and tells the interface what was
    // heard. The distance is the ONLY thing the interface provides: the name, the quality, the class
    // and the number all come back from the domain.
    Q_INVOKABLE void playInterval( int p_semitones );

    // Plays a single note: the very first thing a beginner learns to recognise.
    Q_INVOKABLE void playSingleNote();

    // Plays the interval as a SUSTAINED chord, six seconds long: long enough for the ear to count the beating
    // between the two frequencies, which is exactly how a temperament difference becomes audible.
    Q_INVOKABLE void playSustainedInterval( int p_semitones );

    // Stops every sound. Called when the screen is left: an audio stream left open on a phone drains
    // the battery.
    Q_INVOKABLE void stopPlayback();

signals:
    void lastPlayedIntervalChanged();
    void harmonicPlaybackChanged();

private:
    // Plays an interval and describes it, so that what is displayed and what is played can never
    // disagree.
    void playAndDescribe( const domain::Note & p_rootNote, const domain::Note & p_upperNote );

    void setLastPlayedInterval( QVariantMap p_description );

    domain::NotePlayer & m_notePlayer;
    QVariantList m_supportedIntervals;
    QVariantMap m_lastPlayedInterval;
    bool m_harmonicPlayback{ false };
};

}    // namespace musichien::ui
