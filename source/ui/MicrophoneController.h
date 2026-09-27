#pragma once

// =====================================================================================================================
// Musichien - MicrophoneController
//
// The view model of the microphone settings screen: it lists the inputs, lets the player pick one, and runs a LIVE
// test so that the pitch can be seen before it is trusted. The actual detector is a port injected from outside - this
// controller never builds an implementation, exactly like the exercise screen never builds a note player.
//
// It does not decide anything musical: it only displays the pitch the detector reports. Judging an interval against
// a target is another view model's job.
// =====================================================================================================================

#include "domain/audio/SungIntervalDetector.h"

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <random>

namespace musichien::domain
{
class NotePlayer;
class PitchDetector;
class PlayerPreferences;
}    // namespace musichien::domain

namespace musichien::ui
{

class MicrophoneController final : public QObject
{
    Q_OBJECT

    Q_PROPERTY( QStringList inputDeviceNames READ inputDeviceNames CONSTANT )
    Q_PROPERTY( int currentDeviceIndex READ currentDeviceIndex NOTIFY currentDeviceIndexChanged )
    Q_PROPERTY( bool isListening READ isListening NOTIFY isListeningChanged )
    Q_PROPERTY( double detectedFrequencyHz READ detectedFrequencyHz NOTIFY detectedFrequencyHzChanged )
    Q_PROPERTY( double detectedPitchRatio READ detectedPitchRatio NOTIFY detectedPitchRatioChanged )
    Q_PROPERTY( double detectedMidi READ detectedMidi NOTIFY detectedMidiChanged )
    Q_PROPERTY( double detectedStaffFraction READ detectedStaffFraction NOTIFY detectedStaffFractionChanged )
    Q_PROPERTY( QString detectedNoteLabel READ detectedNoteLabel NOTIFY detectedNoteLabelChanged )

    // How far the voice is from the nearest note, in cents, and how good that is. This is what turns the microphone
    // page into a usable tuner: five cents is green, twenty is the edge of "recognisable but off".
    Q_PROPERTY( double detectedCents READ detectedCents NOTIFY detectedCentsChanged )
    Q_PROPERTY( int detectedTuningState READ detectedTuningState NOTIFY detectedTuningStateChanged )

    // --- La question chantee -----------------------------------------------------------------------------------------
    //
    // Deux niveaux, comme Roger les a decrits :
    //   * la cible est JOUEe (debutant) : on entend l'intervalle, puis on le chante ;
    //   * la cible est seulement NOMMEE (avance) : "chante une quinte", et l'oreille se debrouille.
    // La detection, elle, est la meme dans les deux cas : deux notes tenues, et l'ecart entre elles.
    Q_PROPERTY( int singingTargetSemitones READ singingTargetSemitones NOTIFY singingTargetChanged )
    Q_PROPERTY( QString singingTargetLabel READ singingTargetLabel NOTIFY singingTargetChanged )
    Q_PROPERTY( bool isSingingCaptureActive READ isSingingCaptureActive NOTIFY singingCaptureStateChanged )
    Q_PROPERTY( bool hasSungInterval READ hasSungInterval NOTIFY sungIntervalChanged )
    Q_PROPERTY( int sungSemitones READ sungSemitones NOTIFY sungIntervalChanged )

    // 0 tant que rien n'a ete chante, 1 quand l'intervalle est juste, 2 quand il ne l'est pas.
    Q_PROPERTY( int sungVerdict READ sungVerdict NOTIFY sungIntervalChanged )

public:
    // Builds a detector for the input at p_deviceIndex. The index matches inputDeviceNames, and the factory is the
    // application's way of handing over a QAudioPitchDetector without this view model ever seeing Qt Multimedia.
    using DetectorFactory = std::function<std::unique_ptr<musichien::domain::PitchDetector>( int p_deviceIndex )>;

    MicrophoneController( QStringList p_deviceNames,
                          DetectorFactory p_factory,
                          musichien::domain::PlayerPreferences * p_preferences = nullptr,
                          musichien::domain::NotePlayer * p_notePlayer = nullptr,
                          QObject * p_parent = nullptr );

    // Out-of-line, because the detector is only forward-declared here: the unique_ptr cannot destroy it in the
    // header where the type is still incomplete.
    ~MicrophoneController() override;

    MicrophoneController( const MicrophoneController & ) = delete;
    MicrophoneController & operator=( const MicrophoneController & ) = delete;
    MicrophoneController( MicrophoneController && ) = delete;
    MicrophoneController & operator=( MicrophoneController && ) = delete;

    [[nodiscard]] QStringList inputDeviceNames() const { return m_deviceNames; }
    [[nodiscard]] int currentDeviceIndex() const { return m_currentDeviceIndex; }
    [[nodiscard]] bool isListening() const { return m_isListening; }
    [[nodiscard]] double detectedFrequencyHz() const { return m_detectedFrequencyHz; }
    [[nodiscard]] double detectedPitchRatio() const { return m_detectedPitchRatio; }
    [[nodiscard]] double detectedMidi() const { return m_detectedMidi; }
    [[nodiscard]] double detectedStaffFraction() const { return m_detectedStaffFraction; }
    [[nodiscard]] QString detectedNoteLabel() const { return m_detectedNoteLabel; }
    [[nodiscard]] double detectedCents() const { return m_detectedCents; }
    [[nodiscard]] int detectedTuningState() const { return m_detectedTuningState; }

    Q_INVOKABLE void selectDevice( int p_deviceIndex );
    Q_INVOKABLE void startTest();
    Q_INVOKABLE void stopTest();

    // --- La question chantee -----------------------------------------------------------------------------------------

    // Draws a new interval to sing. Called when the page opens, and after every answer.
    Q_INVOKABLE void newSingingQuestion();

    // Plays the interval to sing, for the beginner level. The advanced level simply does not call it.
    Q_INVOKABLE void playSingingTarget();

    // Opens the microphone and listens for two held notes.
    Q_INVOKABLE void startSingingCapture();
    Q_INVOKABLE void stopSingingCapture();

    [[nodiscard]] int singingTargetSemitones() const { return m_singingTargetSemitones; }
    [[nodiscard]] QString singingTargetLabel() const;
    [[nodiscard]] bool isSingingCaptureActive() const { return m_isSingingCaptureActive; }
    [[nodiscard]] bool hasSungInterval() const { return m_sungIntervalDetector.reading().hasInterval(); }
    [[nodiscard]] int sungSemitones() const { return m_sungIntervalDetector.reading().semitones(); }
    [[nodiscard]] int sungVerdict() const;

signals:
    void currentDeviceIndexChanged();
    void isListeningChanged();
    void detectedFrequencyHzChanged();
    void detectedPitchRatioChanged();
    void detectedMidiChanged();
    void detectedStaffFractionChanged();
    void detectedNoteLabelChanged();
    void detectedCentsChanged();
    void detectedTuningStateChanged();

    void singingTargetChanged();
    void singingCaptureStateChanged();
    void sungIntervalChanged();

private:
    void onPitch( float p_frequencyHz );
    void ensureDetector();

    QStringList m_deviceNames;
    DetectorFactory m_factory;
    musichien::domain::PlayerPreferences * m_preferences{ nullptr };
    int m_currentDeviceIndex{ 0 };
    bool m_isListening{ false };
    double m_detectedFrequencyHz{ 0.0 };
    double m_detectedPitchRatio{ 0.0 };
    double m_detectedMidi{ 0.0 };
    double m_detectedStaffFraction{ 0.5 };
    double m_detectedCents{ 0.0 };
    int m_detectedTuningState{ 0 };
    QString m_detectedNoteLabel;

    // La question chantee : l'intervalle tire, la detection, et l'horloge qui mesure le temps entre deux lectures -
    // le detecteur ne possede pas d'horloge, c'est une regle du jeu.
    musichien::domain::SungIntervalDetector m_sungIntervalDetector;
    std::mt19937 m_singingRandomEngine{ std::random_device{}() };
    int m_singingTargetSemitones{ 7 };
    bool m_isSingingCaptureActive{ false };
    QElapsedTimer m_pitchClock;
    musichien::domain::NotePlayer * m_notePlayer{ nullptr };
    std::unique_ptr<musichien::domain::PitchDetector> m_detector;
};

}    // namespace musichien::ui
