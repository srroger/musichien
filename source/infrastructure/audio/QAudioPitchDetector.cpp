#include "infrastructure/audio/QAudioPitchDetector.h"

#include <QAudioFormat>
#include <QAudioSource>
#include <QIODevice>
#include <QMediaDevices>
#include <QObject>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace musichien::infrastructure
{

namespace
{

constexpr std::int32_t SAMPLE_RATE = 44100;
constexpr std::int32_t CHANNEL_COUNT = 1;

// One YIN window: long enough to resolve a low note, short enough to feel alive.
constexpr std::size_t WINDOW_SIZE = 2048;

// The human voice, in hertz: the detector refuses to look outside this range, which is what keeps a rumble from
// being read as a note.
constexpr double MIN_FREQUENCY_HZ = 60.0;
constexpr double MAX_FREQUENCY_HZ = 1200.0;

// The YIN cumulative-difference threshold. Lower is stricter; 0.15 is the textbook value for the voice.
constexpr double YIN_THRESHOLD = 0.15;

// An Int16 root-mean-square below this is silence, not a note: it stops the detector from reporting garbage when
// nobody sings.
constexpr double SILENCE_RMS = 300.0;

// Estimates the fundamental frequency with YIN. Returns 0.0 when nothing stable is found.
//
// The idea: the signal repeats itself at the period. The difference function d(tau) measures how well the window
// agrees with itself shifted by tau; the first deep dip is the period, and the cumulative normalisation makes that
// dip scale-free - a quiet note and a loud one land on the same answer.
[[nodiscard]] double estimatePitch( const std::vector<double> & p_window, double p_sampleRate )
{
    const auto tauMin = static_cast<std::size_t>( p_sampleRate / MAX_FREQUENCY_HZ );
    const auto tauMax = static_cast<std::size_t>( p_sampleRate / MIN_FREQUENCY_HZ );

    std::vector<double> difference( tauMax + 1, 0.0 );

    for( std::size_t tau = tauMin; tau <= tauMax; ++tau )
    {
        double sum = 0.0;

        for( std::size_t sample = 0; sample + tau < WINDOW_SIZE; ++sample )
        {
            const double diff = p_window.at( sample ) - p_window.at( sample + tau );

            sum += diff * diff;
        }

        difference.at( tau ) = sum;
    }

    // The cumulative mean normalised difference: cmnd(tau) = difference(tau) * tau / sum(difference up to tau).
    std::vector<double> cmnd( tauMax + 1, 0.0 );

    cmnd.at( 0 ) = 1.0;

    double runningSum = 0.0;

    for( std::size_t tau = 1; tau <= tauMax; ++tau )
    {
        runningSum += difference.at( tau );

        cmnd.at( tau ) = difference.at( tau ) * static_cast<double>( tau ) / runningSum;
    }

    // The first local minimum below the threshold is the period. Looking for a minimum, rather than the first value
    // under the threshold, avoids reporting the shoulder of the dip.
    std::size_t tau = tauMin;

    while( tau + 1 <= tauMax
           && ( cmnd.at( tau ) >= YIN_THRESHOLD || cmnd.at( tau ) >= cmnd.at( tau + 1 ) ) )
    {
        ++tau;
    }

    if( tau >= tauMax )
    {
        return 0.0;
    }

    // Parabolic interpolation: the true dip sits between whole samples, and this finds it. The formula is the
    // standard one for a parabola through three points.
    const double below = cmnd.at( tau - 1 );
    const double here = cmnd.at( tau );
    const double above = cmnd.at( tau + 1 );

    const double denominator = 2.0 * ( ( 2.0 * here ) - above - below );

    auto refinedTau = static_cast<double>( tau );

    if( std::abs( denominator ) > 1e-12 )
    {
        refinedTau += ( below - above ) / denominator;
    }

    return p_sampleRate / refinedTau;
}

}    // namespace

class QAudioPitchDetector::Impl
{
public:
    explicit Impl( QAudioDevice p_device )
      : m_device{ std::move( p_device ) }
    {
        m_format.setSampleRate( SAMPLE_RATE );
        m_format.setChannelCount( CHANNEL_COUNT );
        m_format.setSampleFormat( QAudioFormat::Int16 );
    }

private:
    friend class QAudioPitchDetector;

    QAudioDevice m_device;
    QAudioFormat m_format;
    std::unique_ptr<QAudioSource> m_source;
    QIODevice * m_io{ nullptr };
    musichien::domain::PitchDetector::PitchCallback m_callback;
    std::vector<double> m_window;

    // The last GOOD pitch, and how many frames have been silent since. A voice drops out for a few frames and YIN
    // can read the octave: both would make the ball blink, so the last good pitch is held for a moment and the
    // octave is folded back.
    double m_lastFrequency{ 0.0 };

    std::int32_t m_silenceFrames{ 0 };
};

QAudioPitchDetector::QAudioPitchDetector()
  : QAudioPitchDetector{ QMediaDevices::defaultAudioInput() }
{
}

QAudioPitchDetector::QAudioPitchDetector( QAudioDevice p_device )
  : m_impl{ std::make_unique<Impl>( std::move( p_device ) ) }
{
}

QAudioPitchDetector::~QAudioPitchDetector()
{
    stop();
}

void QAudioPitchDetector::start( musichien::domain::PitchDetector::PitchCallback p_callback )
{
    stop();

    Impl & impl = *m_impl;

    impl.m_callback = std::move( p_callback );
    impl.m_window.clear();
    impl.m_window.reserve( WINDOW_SIZE );

    if( impl.m_device.isNull() )
    {
        return;
    }

    impl.m_source = std::make_unique<QAudioSource>( impl.m_device, impl.m_format );

    // A small buffer means small chunks. On the desktop the default is a whole second at a time, which makes the ball
    // jump once a second instead of gliding; twenty milliseconds is the good middle ground between latency and load.
    impl.m_source->setBufferSize( SAMPLE_RATE * static_cast<int>( sizeof( std::int16_t ) ) * 20 / 1000 );

    impl.m_io = impl.m_source->start();

    if( impl.m_io == nullptr )
    {
        impl.m_source = nullptr;

        return;
    }

    // Every chunk of samples is turned into pitch on the spot, on the audio thread's own rhythm. A window is
    // reported, then slid forward by half, so the answer refreshes roughly twice per window.
    QObject::connect( impl.m_io, &QIODevice::readyRead, [this]() {
        Impl & impl = *m_impl;

        if( impl.m_io == nullptr )
        {
            return;
        }

        const QByteArray bytes = impl.m_io->readAll();
        const auto * samples = reinterpret_cast<const std::int16_t *>( bytes.constData() );    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        const std::size_t count = static_cast<std::size_t>( bytes.size() ) / sizeof( std::int16_t );

        for( std::size_t index = 0; index < count; ++index )
        {
            impl.m_window.push_back( static_cast<double>( samples[index] ) / 32768.0 );

            if( impl.m_window.size() < WINDOW_SIZE )
            {
                continue;
            }

            double sumSquared = 0.0;

            for( const double value : impl.m_window )
            {
                sumSquared += value * value;
            }

            const double rms = std::sqrt( sumSquared / static_cast<double>( WINDOW_SIZE ) );

            double frequencyHz = 0.0;

            if( rms * 32768.0 > SILENCE_RMS )
            {
                frequencyHz = estimatePitch( impl.m_window, SAMPLE_RATE );
            }

            // Post-traitement de robustesse, pour la VOIX.
            //
            // Deux defauts classiques d'un estimateur de periode sur un son riche en harmoniques :
            //   * l'erreur d'octave : il lit 2x (ou 1/2) la vraie periode, donc la fondamentale saute d'une octave ;
            //   * le dropout : une frame sans reponse claire, qui vaut "silence" pendant quelques millisecondes.
            // L'un comme l'autre font clignoter la boule, alors que le chanteur, lui, tient sa note.
            if( frequencyHz > 0.0 )
            {
                // Replie l'octave : un saut soudain d'un facteur deux n'est pas la voix, c'est l'estimateur.
                if( impl.m_lastFrequency > 0.0 )
                {
                    const double ratio = frequencyHz / impl.m_lastFrequency;

                    if( ratio > 1.8 )
                    {
                        frequencyHz /= 2.0;
                    }
                    else if( ratio < 0.55 )
                    {
                        frequencyHz *= 2.0;
                    }
                }

                impl.m_lastFrequency = frequencyHz;
                impl.m_silenceFrames = 0;
            }
            else if( impl.m_lastFrequency > 0.0 && impl.m_silenceFrames < 5 )
            {
                // Un bref silence n'est pas la fin de la note : on tient la derniere bonne hauteur une fraction de
                // seconde (5 frames, environ 115 ms), et la boule ne s'eteint plus en plein milieu.
                frequencyHz = impl.m_lastFrequency;
                ++impl.m_silenceFrames;
            }

            if( impl.m_callback )
            {
                impl.m_callback( static_cast<float>( frequencyHz ) );
            }

            // The callback may have stopped the detector (the tuner stops itself once it has heard enough): the
            // buffer and the device are then gone, and touching them again is a use-after-free.
            if( impl.m_io == nullptr )
            {
                return;
            }

            impl.m_window.erase( impl.m_window.begin(),
                                 impl.m_window.begin() + static_cast<std::ptrdiff_t>( WINDOW_SIZE / 2 ) );
        }
    } );
}

void QAudioPitchDetector::stop()
{
    Impl & impl = *m_impl;

    if( impl.m_source )
    {
        impl.m_source->stop();
        impl.m_io = nullptr;

        // deleteLater rather than resetting to null: stop() may be called FROM the readyRead slot itself - the tuner
        // stops itself the moment it has heard enough. Destroying the QAudioSource, and the QIODevice whose slot is
        // on the stack right now, would be a use-after-free. Deferring the deletion to the event loop is the safe way.
        impl.m_source->deleteLater();
        impl.m_source.release();
    }

    impl.m_callback = nullptr;
    impl.m_window.clear();
    impl.m_lastFrequency = 0.0;
    impl.m_silenceFrames = 0;
}

}    // namespace musichien::infrastructure
