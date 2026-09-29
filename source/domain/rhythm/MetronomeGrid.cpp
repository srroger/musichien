#include "domain/rhythm/MetronomeGrid.h"

#include <algorithm>
#include <cmath>

namespace musichien::domain
{

namespace
{

// La duree d'un temps, en echantillons, telle qu'elle se calcule - et elle n'est presque jamais entiere.
[[nodiscard]] double framesPerBeatOf( double p_bpm, int p_sampleRate ) noexcept
{
    if( p_bpm <= 0.0 )
    {
        return 0.0;
    }

    constexpr double SECONDS_PER_MINUTE = 60.0;

    return static_cast<double>( p_sampleRate ) * SECONDS_PER_MINUTE / p_bpm;
}

}    // namespace

MetronomeGrid::MetronomeGrid( int p_sampleRate ) noexcept
  : m_sampleRate{ std::max( 1, p_sampleRate ) }
{
}

void MetronomeGrid::start( double p_bpm, int p_beatsPerBar, std::int64_t p_startFrame ) noexcept
{
    m_framesPerBeat = framesPerBeatOf( p_bpm, m_sampleRate );

    // Un tempo nul, ou une mesure sans temps, n'est pas un metronome : c'est une grille qui ne bat pas. L'arret est
    // donc la bonne reponse, et il evite toute division par zero plus loin.
    if( ( m_framesPerBeat <= 0.0 ) || ( p_beatsPerBar <= 0 ) )
    {
        stop();

        return;
    }

    m_isRunning = true;
    m_startFrame = p_startFrame;
    m_beatsPerBar = p_beatsPerBar;
}

void MetronomeGrid::stop() noexcept
{
    m_isRunning = false;
    m_framesPerBeat = 0.0;
}

bool MetronomeGrid::isAccented( std::int64_t p_beatIndex ) const noexcept
{
    if( m_beatsPerBar <= 0 )
    {
        return false;
    }

    // Le rang est compte depuis le demarrage, donc le reste suffit - et il est positif meme avant le temps 0, ce qui
    // evite un modulo negatif dont le signe dependrait du compilateur.
    return ( p_beatIndex % static_cast<std::int64_t>( m_beatsPerBar ) ) == 0;
}

std::int64_t MetronomeGrid::frameOfBeat( std::int64_t p_beatIndex ) const noexcept
{
    if( m_framesPerBeat <= 0.0 )
    {
        return m_startFrame;
    }

    // L'ARRONDI SE FAIT ICI, UNE FOIS, ET DEPUIS L'INDEX : c'est toute la difference entre un metronome juste et un
    // metronome qui derape lentement.
    return m_startFrame + static_cast<std::int64_t>( std::llround( static_cast<double>( p_beatIndex ) * m_framesPerBeat ) );
}

std::int64_t MetronomeGrid::approximateBeatIndexAt( std::int64_t p_frame ) const noexcept
{
    if( m_framesPerBeat <= 0.0 )
    {
        return 0;
    }

    return static_cast<std::int64_t>(
      std::floor( static_cast<double>( p_frame - m_startFrame ) / m_framesPerBeat ) );
}

MetronomeGrid::Beat MetronomeGrid::firstBeatAtOrAfter( std::int64_t p_frame ) const noexcept
{
    if( !m_isRunning )
    {
        return Beat{ 0, 0 };
    }

    std::int64_t index = std::max<std::int64_t>( 0, approximateBeatIndexAt( p_frame ) );

    // Le rang approche est juste a un temps pres, a cause de l'arrondi : deux corrections suffisent, et elles sont
    // bornees - une boucle non bornee dans un chemin audio serait une bombe a retardement.
    for( int guard = 0; guard < 2; ++guard )
    {
        if( frameOfBeat( index ) >= p_frame )
        {
            if( index > 0 && frameOfBeat( index - 1 ) >= p_frame )
            {
                --index;

                continue;
            }

            break;
        }

        ++index;
    }

    return Beat{ index, frameOfBeat( index ) };
}

std::int64_t MetronomeGrid::beatIndexAt( std::int64_t p_frame ) const noexcept
{
    if( !m_isRunning )
    {
        return 0;
    }

    std::int64_t index = std::max<std::int64_t>( 0, approximateBeatIndexAt( p_frame ) );

    for( int guard = 0; guard < 2; ++guard )
    {
        if( frameOfBeat( index ) <= p_frame )
        {
            break;
        }

        if( index == 0 )
        {
            break;
        }

        --index;
    }

    return index;
}

double MetronomeGrid::elapsedMsAt( std::int64_t p_frame, int p_sampleRate ) const noexcept
{
    if( p_sampleRate <= 0 )
    {
        return 0.0;
    }

    constexpr double MILLISECONDS_PER_SECOND = 1000.0;

    // Négatif avant le temps 0 : une position qui precede le demarrage est un temps qui n'est pas encore arrive, et
    // les appelants le lisent comme tel (le domaine juge une frappe contre une duree, et une duree peut etre negative
    // dans l'abstrait - c'est la position dans la mesure qui ne l'est jamais).
    return ( static_cast<double>( p_frame - m_startFrame ) * MILLISECONDS_PER_SECOND )
           / static_cast<double>( p_sampleRate );
}

}    // namespace musichien::domain
