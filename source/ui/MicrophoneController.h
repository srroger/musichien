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

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

namespace musichien::domain
{
class PitchDetector;
}

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
    Q_PROPERTY( QString detectedNoteLabel READ detectedNoteLabel NOTIFY detectedNoteLabelChanged )

public:
    // Builds a detector for the input at p_deviceIndex. The index matches inputDeviceNames, and the factory is the
    // application's way of handing over a QAudioPitchDetector without this view model ever seeing Qt Multimedia.
    using DetectorFactory = std::function<std::unique_ptr<musichien::domain::PitchDetector>( int p_deviceIndex )>;

    MicrophoneController( QStringList p_deviceNames, DetectorFactory p_factory, QObject * p_parent = nullptr );

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
    [[nodiscard]] QString detectedNoteLabel() const { return m_detectedNoteLabel; }

    Q_INVOKABLE void selectDevice( int p_deviceIndex );
    Q_INVOKABLE void startTest();
    Q_INVOKABLE void stopTest();

signals:
    void currentDeviceIndexChanged();
    void isListeningChanged();
    void detectedFrequencyHzChanged();
    void detectedPitchRatioChanged();
    void detectedNoteLabelChanged();

private:
    void onPitch( float p_frequencyHz );
    void ensureDetector();

    QStringList m_deviceNames;
    DetectorFactory m_factory;
    int m_currentDeviceIndex{ 0 };
    bool m_isListening{ false };
    double m_detectedFrequencyHz{ 0.0 };
    double m_detectedPitchRatio{ 0.0 };
    QString m_detectedNoteLabel;
    std::unique_ptr<musichien::domain::PitchDetector> m_detector;
};

}    // namespace musichien::ui
