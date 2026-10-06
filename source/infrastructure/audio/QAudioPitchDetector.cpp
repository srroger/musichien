#include "infrastructure/audio/QAudioPitchDetector.h"

#include "domain/audio/PitchEstimator.h"
#include "domain/audio/VoicePreFilter.h"

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

// Le taux demande au peripherique, et le seul repli acceptable : si la carte refuse ce format et n'annonce rien, on
// estime avec celui-ci plutot que de tout arreter. Ce qui COMPTE est le taux REELLEMENT ouvert, lu plus bas sur la
// QAudioSource : l'estimer avec une constante quand la carte tourne a 48 kHz decalait toutes les notes d'un
// demi-ton et demi.
constexpr std::int32_t PREFERRED_SAMPLE_RATE = 44100;

constexpr std::int32_t CHANNEL_COUNT = 1;

// An Int16 root-mean-square below this is silence, not a note: it stops the detector from reporting garbage when
// nobody sings.
constexpr double SILENCE_RMS = 300.0;

// Le taux REELLEMENT accepte par la carte : le format demande n'est pas toujours celui obtenu - beaucoup de
// telephones ne travaillent qu'a 48 kHz - et estimer la hauteur avec le mauvais taux decale chaque note d'un
// demi-ton et demi.
[[nodiscard]] double openedSampleRate( const QAudioSource & p_source )
{
    const std::int32_t sampleRate = p_source.format().sampleRate();

    return ( sampleRate > 0 ) ? static_cast<double>( sampleRate ) : static_cast<double>( PREFERRED_SAMPLE_RATE );
}

}    // namespace

class QAudioPitchDetector::Impl
{
public:
    explicit Impl( QAudioDevice p_device )
      : m_device{ std::move( p_device ) }
    {
        m_format.setSampleRate( PREFERRED_SAMPLE_RATE );
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

    // Le taux REELLEMENT ouvert par le peripherique, lu sur la source une fois qu'elle existe. Une carte qui tourne a
    // 48 kHz alors qu'on l'estime a 44,1 kHz fait lire toutes les notes un demi-ton et demi trop bas.
    double m_sampleRate{ static_cast<double>( PREFERRED_SAMPLE_RATE ) };

    // The last GOOD pitch, and how many frames have been silent since. A voice drops out for a few frames and YIN
    // can read the octave: both would make the ball blink, so the last good pitch is held for a moment and the
    // octave is folded back.
    double m_lastFrequency{ 0.0 };

    std::int32_t m_silenceFrames{ 0 };

    // LE PRE-TRAITEMENT DE VOIX, eteint tant que personne ne le demande. Voir VoicePreFilter : il ne sert QU'au chant,
    // et la capture melange les deux usages - c'est donc ce drapeau qui decide, jamais l'adaptateur.
    musichien::domain::VoicePreFilter m_voiceFilter;
    bool m_voicePreFilterEnabled{ false };
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
    impl.m_window.reserve( domain::PitchEstimator::WINDOW_SIZE );

    if( impl.m_device.isNull() )
    {
        return;
    }

    impl.m_source = std::make_unique<QAudioSource>( impl.m_device, impl.m_format );

    // Le taux que la carte a REELLEMENT accepte.
    impl.m_sampleRate = openedSampleRate( *impl.m_source );

    // Le passe-haut se recale sur ce taux : il ne peut pas etre regle avant de le connaitre.
    impl.m_voiceFilter.configure( impl.m_sampleRate );
    impl.m_voiceFilter.reset();

    // A small buffer means small chunks. On the desktop the default is a whole second at a time, which makes the ball
    // jump once a second instead of gliding; twenty milliseconds is the good middle ground between latency and load.
    impl.m_source->setBufferSize( static_cast<int>( impl.m_sampleRate ) * static_cast<int>( sizeof( std::int16_t ) ) * 20
                                  / 1000 );

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
            double value = static_cast<double>( samples[index] ) / 32768.0;

            // LE PASSE-HAUT AVANT TOUT LE RESTE. Le filtre agit ECHANTILLON par ECHANTILLON, donc avant la fenetre :
            // quand YIN regarde les 4096 echantillons, le grondement n'y est deja plus.
            if( impl.m_voicePreFilterEnabled )
            {
                value = impl.m_voiceFilter.processSample( value );
            }

            impl.m_window.push_back( value );

            if( impl.m_window.size() < domain::PitchEstimator::WINDOW_SIZE )
            {
                continue;
            }

            double sumSquared = 0.0;

            for( const double windowValue : impl.m_window )
            {
                sumSquared += windowValue * windowValue;
            }

            const double rms = std::sqrt( sumSquared / static_cast<double>( domain::PitchEstimator::WINDOW_SIZE ) );

            double frequencyHz = 0.0;

            // LE GATE ADAPTATIF, puis le seuil de silence FIXE : les deux doivent dire oui. Le gate suit le bruit de
            // fond du lieu, ce que le seuil fixe ne sait pas faire - dans une piece bruyante, le bruit passe au-dessus
            // de lui et serait estime comme une note.
            const bool isVoice =
              !impl.m_voicePreFilterEnabled || impl.m_voiceFilter.isVoiceLevel( rms );

            if( isVoice && ( rms * 32768.0 > SILENCE_RMS ) )
            {
                frequencyHz = domain::PitchEstimator::estimate( impl.m_window, impl.m_sampleRate );
            }

            if( frequencyHz > 0.0 )
            {
                // Ce qu'on ne fait PLUS : replier l'octave quand la hauteur saute de plus d'un facteur deux.
                //
                // Ce repli partait d'une bonne intention - une erreur d'octave de l'estimateur - et produisait le
                // defaut que Roger a entendu : en montant, la hauteur REELLE finissait par franchir le facteur deux,
                // le repli la divisait, et la valeur corrigee devenait la reference de la comparaison suivante. La
                // detection se retrouvait VERROUILLEE une octave en dessous, et plus la voix montait, plus l'affichage
                // restait bas. Une frame mal lue est moins grave qu'un estimateur qui s'enferme : on ne corrige plus.
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
                                 impl.m_window.begin()
                                   + static_cast<std::ptrdiff_t>( domain::PitchEstimator::WINDOW_SIZE / 2 ) );
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
        QAudioSource * const deferred = impl.m_source.release();
        deferred->deleteLater();
    }

    impl.m_callback = nullptr;
    impl.m_window.clear();
    impl.m_lastFrequency = 0.0;
    impl.m_silenceFrames = 0;
}

void QAudioPitchDetector::setVoicePreFilterEnabled( bool p_enabled )
{
    Impl & impl = *m_impl;

    impl.m_voicePreFilterEnabled = p_enabled;

    // Le filtre repart d'un etat PROPRE a chaque changement : l'activer au milieu d'une prise ferait entendre le
    // transitoire du passe-haut, et le plancher de bruit doit etre REMESURE la ou l'on est - pas herite du reglage
    // precedent.
    impl.m_voiceFilter.reset();
}

}    // namespace musichien::infrastructure
