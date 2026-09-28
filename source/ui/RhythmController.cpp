#include "ui/RhythmController.h"

#include <algorithm>
#include <cmath>

namespace musichien::ui
{

namespace
{

constexpr int MINIMUM_BPM = 1;
constexpr int MAXIMUM_BPM = 300;

constexpr int MINIMUM_BEATS_PER_BAR = 1;
constexpr int MAXIMUM_BEATS_PER_BAR = 12;

// Two taps define a tempo when the gap between them is a believable beat. The ceiling is two seconds (30 bpm);
// the floor is a tenth of a second (600 bpm) - a range wide enough for a slow exercise and for a fast drill.
constexpr std::int64_t MINIMUM_TAP_INTERVAL_MS = 100;
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

    // SINGLE SHOT, et c'est le cœur du correctif du 28 septembre 2026. Un timer REPETITIF repart de l'instant ou il a
    // tire : chaque battement joue un peu en retard ajoute son retard a tous les suivants, et le metronome prend une
    // seconde dans la vue du musicien au bout de deux minutes. Ici, chaque battement re-arme le suivant depuis
    // l'horloge de depart (voir scheduleNextBeat), et le retard ne se reporte jamais.
    m_beatTimer.setSingleShot( true );

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
        // Le tempo change : la grille repart d'ICI. Un nouveau tempo s'attend a partir de maintenant - le recaler sur
        // le demarrage rejouerait le passe, et ferait sauter le metronome de plusieurs temps.
        restartBeatGrid();
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
    m_beatIndex = 0;

    // The downbeat sounds at once, so the ear starts on a clean reference. C'est lui qui arme le battement suivant :
    // le demarrage et la marche passent donc par le MEME chemin, et il n'y a pas deux endroits qui planifient.
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

        const double beatMs = domain::beatDurationMs( m_bpm );

        const domain::RhythmPattern * pattern = activePattern();

        // Avec une cellule choisie, la question change : la frappe doit tomber sur une FRAPPE de la cellule, pas sur
        // un temps. C'est tout l'exercice - reproduire une rythmique, et pas seulement battre la mesure.
        const domain::HitQuality quality =
          ( ( pattern != nullptr ) && ( beatMs > 0.0 ) )
            ? domain::judgeDistance( domain::distanceToNearestOnsetInBeats( *pattern, elapsedMs / beatMs ) * beatMs )
            : domain::judgeTap( elapsedMs, m_bpm );

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
    // Le rang GLOBAL du temps, dont la position dans la mesure se DEDUIT. C'est ce qui permet a la mesure de se
    // recompter apres un recalage de la grille, au lieu de suivre un compteur qui aurait derive avec elle.
    const std::int64_t beatIndex = m_beatIndex;
    ++m_beatIndex;

    m_beatInBar = static_cast<int>( beatIndex % m_beatsPerBar );

    m_notePlayer.playMetronomeClick( m_beatInBar == 0 );

    emit beatInBarChanged();

    schedulePatternHitsForBeat( m_beatInBar );

    scheduleNextBeat();
}

// Le battement suivant, vise depuis l'ORIGINE de la grille.
//
// Un QTimer repetitif repart de l'instant ou il a TIRE, et non de l'instant ou il aurait du tirer : chaque battement
// un peu en retard decale donc tous les suivants, et le retard s'additionne. A 90 bpm, un millieme de seconde par temps
// suffit a faire entendre un metronome qui traine au bout d'une minute ; sur un telephone, le retard d'un tir est bien
// plus gros que cela, et Roger l'a entendu.
//
// La decision - quand, et quel battement - appartient au domaine (domain::planNextBeat), ou elle est pure et testee.
// Ici il n'y a plus qu'a obeir : c'est ce qui rend ce correctif verifiable sans l'ecouter.
void RhythmController::scheduleNextBeat()
{
    const domain::BeatSchedule schedule = domain::planNextBeat( static_cast<double>( m_bpm ), static_cast<std::size_t>( m_beatIndex ), static_cast<double>( m_clock.elapsed() ) );

    // Le recalage eventuel de la grille - apres un reveil du telephone, par exemple - remonte par l'index : c'est lui
    // qui sait QUEL battement suivra, et donc OU en est la mesure.
    m_beatIndex = static_cast<std::int64_t>( schedule.beatIndex );

    m_beatTimer.start( static_cast<int>( std::lround( schedule.delayMs ) ) );
}

void RhythmController::restartBeatGrid()
{
    m_clock.restart();

    // Le temps 0 est celui qui vient de sonner : la nouvelle grille commence au temps 1.
    m_beatIndex = 1;

    // Le premier temps de la nouvelle grille est la premiere echeance : on repasse par la meme porte que tous les
    // autres battements, sans avoir a deviner un delai ici.
    scheduleNextBeat();
}

QVariantList RhythmController::patterns() const
{
    QVariantList names;

    // La premiere entree n'est pas une cellule : c'est le metronome nu. Le QML affiche la liste telle quelle et
    // renvoie l'index choisi, donc aucun decalage n'est a gerer dans l'interface.
    names.append( tr( "Métronome seul" ) );

    for( const domain::RhythmPattern & pattern : domain::allRhythmPatterns() )
    {
        names.append( QString::fromUtf8( pattern.name().data(), static_cast<int>( pattern.name().size() ) ) );
    }

    return names;
}

void RhythmController::setCurrentPattern( int p_index )
{
    if( ( p_index < 0 ) || ( p_index > static_cast<int>( domain::allRhythmPatterns().size() ) ) )
    {
        return;
    }

    if( p_index == m_currentPattern )
    {
        return;
    }

    m_currentPattern = p_index;

    emit currentPatternChanged();
}

const domain::RhythmPattern * RhythmController::activePattern() const
{
    if( m_currentPattern <= 0 )
    {
        return nullptr;
    }

    return &domain::allRhythmPatterns().at( static_cast<std::size_t>( m_currentPattern - 1 ) );
}

void RhythmController::schedulePatternHitsForBeat( int p_beatInBar )
{
    const domain::RhythmPattern * pattern = activePattern();

    if( pattern == nullptr )
    {
        return;
    }

    const double beatMs = domain::beatDurationMs( m_bpm );

    if( beatMs <= 0.0 )
    {
        return;
    }

    const double barBeat = static_cast<double>( p_beatInBar );

    for( const domain::RhythmHit & hit : pattern->hits() )
    {
        if( ( hit.beat < barBeat ) || ( hit.beat >= barBeat + 1.0 ) )
        {
            continue;
        }

        const auto delayMs = static_cast<int>( std::lround( ( hit.beat - barBeat ) * beatMs ) );

        if( delayMs <= 0 )
        {
            m_notePlayer.playDrum( hit.drum );

            continue;
        }

        // Une frappe decalee part en differe : c'est ce qu'est une syncope - une frappe ENTRE deux temps.
        //
        // Le controleur est le contexte du tir differe, donc si le metronome s'arrete et que l'objet vit toujours, le
        // tir part quand meme ; c'est voulu, la fin d'une mesure doit s'entendre.
        QTimer::singleShot( delayMs, this, [this, drum = hit.drum]() { m_notePlayer.playDrum( drum ); } );
    }
}

}    // namespace musichien::ui
