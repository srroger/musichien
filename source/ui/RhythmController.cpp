#include "ui/RhythmController.h"

#include <algorithm>
#include <cmath>

namespace musichien::ui
{

namespace
{

constexpr int MINIMUM_BPM = 30;
constexpr int MAXIMUM_BPM = 300;

constexpr int MINIMUM_BEATS_PER_BAR = 1;
constexpr int MAXIMUM_BEATS_PER_BAR = 12;

// Two taps define a tempo when the gap between them is a believable beat: at least 200 ms (300 bpm) and at most
// two seconds (30 bpm).
constexpr std::int64_t MINIMUM_TAP_INTERVAL_MS = 200;
constexpr std::int64_t MAXIMUM_TAP_INTERVAL_MS = 2000;

}    // namespace

RhythmController::RhythmController( domain::NotePlayer & p_notePlayer, QObject * p_parent )
  : QObject{ p_parent }
  , m_notePlayer{ p_notePlayer }
{
    m_clock.start();

    // The most precise timer Qt has: the beat must land where the ear expects it, and a coarse timer makes the whole
    // page feel drunk. Not sample-accurate, but close enough for the first loop.
    m_beatTimer.setTimerType( Qt::PreciseTimer );

    QObject::connect( &m_beatTimer, &QTimer::timeout, this, &RhythmController::onBeat );
}

void RhythmController::setBpm( int p_bpm )
{
    const int clamped = std::clamp( p_bpm, MINIMUM_BPM, MAXIMUM_BPM );

    if( clamped == m_bpm )
    {
        return;
    }

    m_bpm = clamped;

    if( m_isRunning )
    {
        m_beatTimer.start( static_cast<int>( domain::beatDurationMs( m_bpm ) ) );
    }

    emit bpmChanged();
}

void RhythmController::setBeatsPerBar( int p_beats )
{
    const int clamped = std::clamp( p_beats, MINIMUM_BEATS_PER_BAR, MAXIMUM_BEATS_PER_BAR );

    if( clamped == m_beatsPerBar )
    {
        return;
    }

    m_beatsPerBar = clamped;

    emit beatsPerBarChanged();
}

void RhythmController::start()
{
    if( m_isRunning )
    {
        return;
    }

    m_isRunning = true;
    m_beatInBar = 0;
    m_score = 0;
    m_combo = 0;
    m_lastQuality = 0;

    m_clock.restart();
    m_beatTimer.start( static_cast<int>( domain::beatDurationMs( m_bpm ) ) );

    // The downbeat sounds at once, so the ear starts on a clean reference.
    onBeat();

    emit isRunningChanged();
    emit scoreChanged();
    emit comboChanged();
    emit lastQualityChanged();
}

void RhythmController::stop()
{
    if( !m_isRunning )
    {
        return;
    }

    m_isRunning = false;
    m_beatTimer.stop();

    emit isRunningChanged();
}

void RhythmController::tap()
{
    if( m_isRunning )
    {
        // The game: the tap is judged against the nearest beat.
        const double elapsedMs = static_cast<double>( m_clock.elapsed() );

        const domain::HitQuality quality = domain::judgeTap( elapsedMs, m_bpm );

        switch( quality )
        {
            case domain::HitQuality::Perfect:
                m_score += 3;
                ++m_combo;
                m_lastQuality = 2;
                break;

            case domain::HitQuality::Good:
                m_score += 1;
                ++m_combo;
                m_lastQuality = 1;
                break;

            case domain::HitQuality::Miss:
                m_combo = 0;
                m_lastQuality = 0;
                break;
        }

        emit scoreChanged();
        emit comboChanged();
        emit lastQualityChanged();

        return;
    }

    // The tap tempo: two taps, and the gap between them IS the tempo.
    const std::int64_t nowMs = m_clock.elapsed();

    if( m_lastTapMs >= 0 )
    {
        const std::int64_t intervalMs = nowMs - m_lastTapMs;

        if( ( intervalMs >= MINIMUM_TAP_INTERVAL_MS ) && ( intervalMs <= MAXIMUM_TAP_INTERVAL_MS ) )
        {
            const int inferredBpm = static_cast<int>( std::lround( 60000.0 / static_cast<double>( intervalMs ) ) );

            setBpm( inferredBpm );
        }
    }

    m_lastTapMs = nowMs;
}

void RhythmController::playDrum( int p_drumIndex )
{
    if( ( p_drumIndex < 0 ) || ( static_cast<std::size_t>( p_drumIndex ) >= domain::DRUM_COUNT ) )
    {
        return;
    }

    m_notePlayer.playDrum( static_cast<domain::Drum>( p_drumIndex ) );
}

void RhythmController::onBeat()
{
    m_beatInBar = m_beatInBar % m_beatsPerBar;

    m_notePlayer.playMetronomeClick( m_beatInBar == 0 );

    emit beatInBarChanged();

    ++m_beatInBar;
}

}    // namespace musichien::ui
