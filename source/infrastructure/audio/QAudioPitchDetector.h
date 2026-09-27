#pragma once

#include "domain/audio/PitchDetector.h"

#include <QAudioDevice>

#include <memory>

namespace musichien::infrastructure
{

// The REAL detector: a microphone read through QAudioSource, and a pitch estimated with the YIN algorithm. This is
// the port's concrete answer, and it is chosen by the application at start up - the desktop tests still get the
// honest silence of NullPitchDetector.
//
// The heavy Qt machinery (source, device, buffer) lives behind a pimpl, so the header stays lean and the destructor
// can be out-of-line.
class QAudioPitchDetector final : public musichien::domain::PitchDetector
{
public:
    // Listens to the DEFAULT input, the one the system is already pointing at. Use the other constructor to pick one.
    QAudioPitchDetector();

    // Listens to ONE specific input, chosen from QMediaDevices::audioInputs() by the settings screen.
    explicit QAudioPitchDetector( QAudioDevice p_device );

    QAudioPitchDetector( const QAudioPitchDetector & ) = delete;
    QAudioPitchDetector & operator=( const QAudioPitchDetector & ) = delete;
    QAudioPitchDetector( QAudioPitchDetector && ) = delete;
    QAudioPitchDetector & operator=( QAudioPitchDetector && ) = delete;

    ~QAudioPitchDetector() override;

    void start( PitchCallback p_callback ) override;

    void stop() override;

private:
    struct Impl;

    std::unique_ptr<Impl> m_impl;
};

}    // namespace musichien::infrastructure
