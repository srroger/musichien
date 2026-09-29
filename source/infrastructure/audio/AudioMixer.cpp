#include "infrastructure/audio/AudioMixer.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <utility>

namespace musichien::infrastructure
{

namespace
{

// Combien d'echantillons le mixer annonce avoir a ecrire pendant que le metronome bat. Cette valeur n'a pas besoin
// d'etre exacte : elle dit seulement au peripherique qu'il y a du travail, pour qu'il ne rende pas l'appareil audio
// entre deux clics. Trop grande, elle ferait lire d'un coup ; trop petite, elle reveillerait le flux pour rien.
constexpr std::size_t METRONOME_AHEAD_FRAMES = 2048;

// Les gains des deux clics. Le premier temps doit s'entendre par-dessus tout le reste ; les autres doivent se faire
// oublier assez pour qu'on n'entende que la pulsation.
constexpr float ACCENTED_CLICK_GAIN = 1.0F;
constexpr float PLAIN_CLICK_GAIN = 0.7F;

}    // namespace

AudioMixer::AudioMixer( int p_sampleRate, int p_channelCount, QObject * p_parent )
  : QIODevice{ p_parent }
  , m_sampleRate{ p_sampleRate }
  , m_channelCount{ std::max( 1, p_channelCount ) }
  , m_grid{ p_sampleRate }
{
    // A sequential source: the sink reads it, and there is nothing to seek in a stream of sound.
    open( QIODevice::ReadOnly );
}

void AudioMixer::play( std::vector<float> p_samples, float p_gain )
{
    playAt( std::move( p_samples ), m_framesWritten.load(), p_gain );
}

void AudioMixer::playAt( std::vector<float> p_samples, std::int64_t p_startFrame, float p_gain )
{
    if( p_samples.empty() )
    {
        return;
    }

    auto source = std::make_shared<const std::vector<float>>( std::move( p_samples ) );

    {
        const std::lock_guard<std::mutex> lock{ m_mutex };

        // Un son dont l'instant est deja passe se joue TOUT DE SUITE : le silence serait pire que le retard, et un
        // appelant qui vise une position toujours derriere lui n'aurait sinon plus aucun son du tout.
        const auto startFrame = std::max( p_startFrame, m_framesWritten.load() );

        m_voices.push_back( Voice{ std::move( source ), 0, p_gain, startFrame } );
    }

    // Et on PREVIENT le peripherique : c'est ce signal qui le fait sortir de sa veille et le faire venir chercher des
    // echantillons. Sans lui, un QIODevice sequentiel reste "vide" a ses yeux et rien ne sort jamais.
    emit readyRead();
}

void AudioMixer::clear()
{
    const std::lock_guard<std::mutex> lock{ m_mutex };

    m_voices.clear();
}

bool AudioMixer::isPlaying() const
{
    const std::lock_guard<std::mutex> lock{ m_mutex };

    return !m_voices.empty();
}

qint64 AudioMixer::bytesAvailable() const
{
    const std::lock_guard<std::mutex> lock{ m_mutex };

    if( m_grid.isRunning() )
    {
        // Le metronome bat : il y a du travail MAINTENANT, ne serait-ce que pour ecrire le silence qui separe deux
        // clics. C'est ce silence qui fait avancer le temps - un flux qui s'endort entre deux clics perdrait la mesure.
        return static_cast<qint64>( METRONOME_AHEAD_FRAMES * static_cast<std::size_t>( m_channelCount ) )
               * static_cast<qint64>( sizeof( float ) );
    }

    std::size_t remainingFrames = 0;

    for( const Voice & voice : m_voices )
    {
        if( voice.position < voice.samples->size() )
        {
            remainingFrames = std::max( remainingFrames, voice.samples->size() - voice.position );
        }
    }

    if( remainingFrames == 0 )
    {
        // Silence : on le dit, et le peripherique peut retourner en veille. C'est ce qui lui permet de rendre
        // l'appareil audio quand plus rien ne joue.
        return QIODevice::bytesAvailable();
    }

    return static_cast<qint64>( remainingFrames * static_cast<std::size_t>( m_channelCount ) * sizeof( float ) );
}

std::size_t AudioMixer::voiceCount() const
{
    const std::lock_guard<std::mutex> lock{ m_mutex };

    return m_voices.size();
}

void AudioMixer::setMetronomeClicks( std::vector<float> p_accented, std::vector<float> p_plain )
{
    const std::lock_guard<std::mutex> lock{ m_mutex };

    m_accentedClick = std::make_shared<const std::vector<float>>( std::move( p_accented ) );
    m_plainClick = std::make_shared<const std::vector<float>>( std::move( p_plain ) );
}

void AudioMixer::startMetronome( double p_bpm, int p_beatsPerBar )
{
    const std::lock_guard<std::mutex> lock{ m_mutex };

    // Le temps 0 tombe a la position COURANTE du flux, et non a une position choisie par l'interface : le metronome
    // demarre donc exactement quand on le lui demande, et tout se compte en echantillons depuis la.
    m_grid.start( p_bpm, p_beatsPerBar, m_framesWritten.load() );
}

void AudioMixer::stopMetronome()
{
    const std::lock_guard<std::mutex> lock{ m_mutex };

    m_grid.stop();

    // Les clics deja planifies sont retires : un temps qui sonnerait apres l'arret serait le pire des deux mondes.
    std::erase_if( m_voices, [this]( const Voice & p_voice ) {
        return ( p_voice.samples == m_accentedClick ) || ( p_voice.samples == m_plainClick );
    } );
}

bool AudioMixer::isMetronomeRunning() const
{
    const std::lock_guard<std::mutex> lock{ m_mutex };

    return m_grid.isRunning();
}

std::int64_t AudioMixer::framesWritten() const
{
    return m_framesWritten.load();
}

void AudioMixer::setOutputLatencyFrames( std::int64_t p_frames )
{
    m_outputLatencyFrames.store( std::max<std::int64_t>( 0, p_frames ) );
}

std::int64_t AudioMixer::listenedFrame() const noexcept
{
    return std::max<std::int64_t>( 0, m_framesWritten.load() - m_outputLatencyFrames.load() );
}

double AudioMixer::metronomeElapsedMs() const
{
    const std::lock_guard<std::mutex> lock{ m_mutex };

    return m_grid.elapsedMsAt( listenedFrame(), m_sampleRate );
}

std::int64_t AudioMixer::metronomeBeatIndex() const
{
    const std::lock_guard<std::mutex> lock{ m_mutex };

    return m_grid.beatIndexAt( listenedFrame() );
}

bool AudioMixer::isMetronomeBeatAccented() const
{
    const std::lock_guard<std::mutex> lock{ m_mutex };

    return m_grid.isAccented( m_grid.beatIndexAt( listenedFrame() ) );
}

qint64 AudioMixer::readData( char * p_data, qint64 p_maximumByteCount )
{
    if( ( p_data == nullptr ) || ( p_maximumByteCount <= 0 ) || ( m_sampleRate <= 0 ) )
    {
        return 0;
    }

    const auto channelCount = static_cast<std::size_t>( m_channelCount );

    const auto bytesPerFrame = static_cast<qint64>( sizeof( float ) * channelCount );

    const qint64 frameCount = p_maximumByteCount / bytesPerFrame;

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): the sink hands over bytes, the mixer works in floats.
    auto * output = reinterpret_cast<float *>( p_data );

    {
        const std::lock_guard<std::mutex> lock{ m_mutex };

        // La position du flux AVANT ce tampon : c'est elle qui donne leur position exacte aux clics qui tombent dedans.
        const std::int64_t firstFrame = m_framesWritten.load();

        // Les temps du metronome qui tombent dans la fenetre a venir sont planifies MAINTENANT, et non au moment ou
        // l'interface le demanderait : c'est ce qui rend le clic exact.
        scheduleMetronomeBeats( firstFrame, frameCount );

        for( qint64 frameIndex = 0; frameIndex < frameCount; ++frameIndex )
        {
            const std::int64_t streamFrame = firstFrame + frameIndex;

            float mixed = 0.0F;

            for( Voice & voice : m_voices )
            {
                // Un son planifie pour un instant a venir attend en silence : c'est ce qui le pose a sa place plutot
                // que de le jouer des qu'il est demande.
                if( streamFrame < voice.startFrame )
                {
                    continue;
                }

                if( voice.position < voice.samples->size() )
                {
                    mixed += voice.samples->at( voice.position ) * voice.gain;

                    ++voice.position;
                }
            }

            // Several sounds at once can leave the [-1, 1] range. Clipping is deliberate here: the alternative would
            // be a note that turns down the whole mix when a drum is added, which the ear reads as a glitch.
            mixed = std::clamp( mixed, -1.0F, 1.0F );

            for( std::size_t channel = 0; channel < channelCount; ++channel )
            {
                output[( static_cast<std::size_t>( frameIndex ) * channelCount ) + channel] = mixed;
            }
        }

        // A voice that reached the end of its own buffer is done and is dropped. Doing it AFTER the frame loop keeps
        // the sum above simple: a finished voice contributes nothing anyway.
        std::erase_if( m_voices, []( const Voice & p_voice ) {
            return p_voice.position >= p_voice.samples->size();
        } );

        // Le flux a avance de tout ce qu'on vient d'ecrire, silence compris : c'est cette ligne qui fait du temps du
        // metronome le temps de l'AUDIO, et le rend impossible a deriver.
        m_framesWritten.store( firstFrame + frameCount );
    }

    return frameCount * bytesPerFrame;
}

void AudioMixer::scheduleMetronomeBeats( std::int64_t p_firstFrame, std::int64_t p_frameCount )
{
    // Appelee sous le verrou, depuis readData : elle ne le reprend donc pas.
    if( !m_grid.isRunning() || ( m_accentedClick == nullptr ) || ( m_plainClick == nullptr ) )
    {
        return;
    }

    const std::int64_t lastFrame = p_firstFrame + p_frameCount;

    domain::MetronomeGrid::Beat beat = m_grid.firstBeatAtOrAfter( p_firstFrame );

    // La borne est celle de la FENETRE, et non de la mesure : un tampon audio enjambe volontiers un temps, et une
    // boucle qui s'arreterait au premier temps perdrait tous les suivants.
    while( beat.frame < lastFrame )
    {
        const float gain = m_grid.isAccented( beat.index ) ? ACCENTED_CLICK_GAIN : PLAIN_CLICK_GAIN;

        m_voices.push_back( Voice{ m_grid.isAccented( beat.index ) ? m_accentedClick : m_plainClick, 0, gain, beat.frame } );

        // Le temps SUIVANT, demande depuis l'echantillon qui suit celui-ci : c'est ce qui evite de replanifier le meme
        // temps deux fois quand deux temps tombent sur le meme echantillon - ce qui arrive a un tempo extreme.
        beat = m_grid.firstBeatAtOrAfter( beat.frame + 1 );
    }
}

qint64 AudioMixer::writeData( const char * p_data, qint64 p_byteCount )
{
    (void)p_data;
    (void)p_byteCount;

    return 0;
}

}    // namespace musichien::infrastructure
