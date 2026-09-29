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

// La cadence du RAFRAICHISSEMENT de l'affichage. Vingt millisecondes : cinquante fois par seconde, ce qui suffit a
// faire suivre le curseur de la mesure a l'oeil, et ce qui ne coute rien puisqu'il ne fait que repeindre. Sa precision
// n'a aucune importance - c'est le flux audio qui fait battre le metronome.
constexpr int DISPLAY_REFRESH_MS = 20;

}    // namespace

RhythmController::RhythmController( domain::NotePlayer & p_notePlayer, QObject * p_parent )
  : QObject{ p_parent }
  , m_notePlayer{ p_notePlayer }
{
    m_clock.start();

    // LE SON NE DEPEND PLUS D'ICI.
    //
    // Ce timer ne fait que REPEINDRE : il lit le temps du flux audio et met la page a jour. Sa precision n'a donc
    // aucune importance, et c'est tout l'interet du changement - le metronome reste juste meme quand l'interface est
    // occupee a peindre, parce que ce n'est plus elle qui le fait battre.
    m_displayTimer.setTimerType( Qt::PreciseTimer );
    m_displayTimer.setInterval( DISPLAY_REFRESH_MS );

    QObject::connect( &m_displayTimer, &QTimer::timeout, this, &RhythmController::refreshFromAudioClock );
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
        // Le tempo change : la grille repart d'ICI, dans le flux. Un nouveau tempo s'attend a partir de maintenant, et
        // le recaler sur le demarrage rejouerait le passe.
        m_notePlayer.startMetronome( static_cast<double>( m_bpm ), m_beatsPerBar );

        // Le dernier temps vu appartenait a l'ancienne grille : on repart de zero pour l'affichage et pour les cellules.
        m_lastBeatIndex = -1;
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
    m_lastBeatIndex = -1;

    // LE SON : c'est le flux audio qui bat, a partir de cet instant. Le temps 0 tombe a la position courante du flux,
    // et tout se compte en echantillons depuis la.
    m_notePlayer.startMetronome( static_cast<double>( m_bpm ), m_beatsPerBar );

    // L'AFFICHAGE suit le flux, lui, a sa propre cadence : il ne fait que repeindre.
    m_displayTimer.start();

    // Et une fois tout de suite, pour que le premier temps s'affiche sans attendre le premier rafraichissement : le
    // metronome sonne deja, et une page qui montrerait encore l'ancien temps serait en retard sur lui.
    refreshFromAudioClock();

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

    m_displayTimer.stop();

    // Et le son s'arrete dans le FLUX : le mixer retire les clics deja planifies, sans quoi un temps sonnerait encore
    // apres l'arret - le pire des deux mondes.
    m_notePlayer.stopMetronome();

    emit isRunningChanged();
}

void RhythmController::tap()
{
    if( m_isRunning )
    {
        // The game: the tap is judged against the nearest beat.
        //
        // LE TEMPS VIENT DU FLUX AUDIO, et non d'une horloge d'interface : c'est le temps que le joueur ENTEND, celui
        // du clic qui vient de sonner - et non celui du moment ou la page a bien voulu traiter l'evenement.
        const double elapsedMs = m_notePlayer.metronomeElapsedMs();

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

void RhythmController::refreshFromAudioClock()
{
    // LE TEMPS VIENT DU FLUX, et le compteur de temps de l'interface ne fait que le SUIVRE.
    //
    // C'est ce qui remplace onBeat : plus aucun son n'est declenche ici. Le clic a deja sonne dans le flux audio, a
    // l'echantillon pres ; ce qui reste a faire, c'est de dire ce que l'oreille est en train d'entendre.
    const std::int64_t beatIndex = m_notePlayer.metronomeBeatIndex();

    // LE TEMPS NE SE TRAITE QU'UNE FOIS. Le rafraichissement passe vingt fois par seconde : sans cette garde, les
    // frappes d'une cellule partiraient vingt fois par temps.
    if( beatIndex == m_lastBeatIndex )
    {
        return;
    }

    // Le tout premier temps d'une grille : il n'a pas de precedent pour annoncer ses frappes.
    const bool isFirstBeat = ( m_lastBeatIndex < 0 );

    m_lastBeatIndex = beatIndex;

    m_beatInBar = static_cast<int>( beatIndex % m_beatsPerBar );

    emit beatInBarChanged();

    // Les frappes du temps SUIVANT sont posees MAINTENANT : avec un temps d'avance, le mixer a tout le loisir de les
    // ecrire a leur position exacte. Planifier le temps en cours serait trop tard - il vient de sonner.
    schedulePatternHitsForBeat( beatIndex + 1 );

    // Et au tout premier temps, celles du temps en cours aussi : sans quoi la premiere frappe de la cellule manquerait,
    // faute d'un temps precedent pour l'annoncer.
    if( isFirstBeat )
    {
        schedulePatternHitsForBeat( beatIndex );
    }
}

double RhythmController::positionOfBeat( std::int64_t p_beatIndex ) const noexcept
{
    const double beatMs = domain::beatDurationMs( static_cast<double>( m_bpm ) );

    // La position du temps n depuis le PREMIER TEMPS DU METRONOME : c'est cette base-la que le lecteur audio attend,
    // parce que c'est elle qu'il traduit en echantillons.
    return static_cast<double>( p_beatIndex ) * beatMs;
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

void RhythmController::schedulePatternHitsForBeat( std::int64_t p_beatIndex )
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

    // La mesure qui contient ce temps commence ici : les frappes d'une cellule sont ecrites DEPUIS le premier temps de
    // la mesure, il faut donc les ramener sur la base du metronome.
    const std::int64_t measureStart = ( p_beatIndex / m_beatsPerBar ) * m_beatsPerBar;

    const auto beatInBar = static_cast<double>( p_beatIndex - measureStart );

    for( const domain::RhythmHit & hit : pattern->hits() )
    {
        // Les frappes de CE temps, et seulement elles : le temps voisin s'occupe des siennes.
        if( ( hit.beat < beatInBar ) || ( hit.beat >= beatInBar + 1.0 ) )
        {
            continue;
        }

        // LA POSITION, et non « dans un instant ».
        //
        // C'est ce qui fait qu'une syncope tombe ENTRE deux temps : la position se traduit en echantillons, et le mixer
        // la pose exactement la. Un QTimer::singleShot, lui, visait l'instant ou le thread d'interface serait revenu -
        // ce qui n'est pas un instant musical.
        m_notePlayer.playDrumAt( hit.drum, positionOfBeat( measureStart ) + ( hit.beat * beatMs ) );
    }
}

}    // namespace musichien::ui
