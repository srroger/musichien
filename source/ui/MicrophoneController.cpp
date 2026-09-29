#include "ui/MicrophoneController.h"

#include "domain/audio/NotePlayer.h"
#include "domain/audio/PitchDetector.h"
#include "domain/exercise/PlayerPreferences.h"
#include "domain/music/Interval.h"
#include "domain/music/Note.h"
#include "domain/music/StaffPosition.h"
#include "domain/music/Temperament.h"

#include <QCoreApplication>
#include <QPermission>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>

namespace musichien::ui
{

namespace
{

// The pitch ratio maps a frequency onto the 0..1 band the screen uses to place the ball on the staff. It is
// logarithmic, because an octave is a doubling, not a fixed number of hertz: 120 Hz and 240 Hz must land the same
// distance apart as 600 Hz and 1200 Hz.
constexpr double LOWEST_VISIBLE_HZ = 60.0;
constexpr double HIGHEST_VISIBLE_HZ = 1200.0;

[[nodiscard]] double pitchRatioFor( double p_frequencyHz )
{
    if( p_frequencyHz <= 0.0 )
    {
        return 0.0;
    }

    const double clamped = std::clamp( p_frequencyHz, LOWEST_VISIBLE_HZ, HIGHEST_VISIBLE_HZ );

    return std::log( clamped / LOWEST_VISIBLE_HZ ) / std::log( HIGHEST_VISIBLE_HZ / LOWEST_VISIBLE_HZ );
}

// Turns a frequency into a human label: the closest note name, then the rounded hertz. Silence is an em-dash, not a
// note, and a pitch outside the MIDI range is reported honestly as a number.
// The note a frequency is closest to: the MIDI number that rounds to the nearest semitone. Nothing when there is no
// pitch at all, or when the pitch falls outside the playable range. The diapason decides where A4 sits.
[[nodiscard]] std::optional<domain::Note> nearestNoteFor( double p_frequencyHz, double p_referencePitchHz )
{
    if( p_frequencyHz <= 0.0 )
    {
        return std::nullopt;
    }

    const double semitonesFromA4 = std::round( static_cast<double>( domain::SEMITONES_PER_OCTAVE )
                                               * std::log2( p_frequencyHz / p_referencePitchHz ) );

    const auto midiNumber = static_cast<std::int32_t>( domain::REFERENCE_MIDI_NUMBER + semitonesFromA4 );

    if( midiNumber < domain::Note::MINIMUM_MIDI_NUMBER || midiNumber > domain::Note::MAXIMUM_MIDI_NUMBER )
    {
        return std::nullopt;
    }

    return domain::Note{ midiNumber };
}

// How far from the nearest note counts as in tune. These are a tuner's usual bands, and they are generous on purpose:
// five cents is the ear's own limit on a held note, twenty is audibly off while still being the right note.
constexpr double IN_TUNE_CENTS = 5.0;
constexpr double OFF_CENTS = 20.0;

// 0 in tune, 1 close, 2 off. A screen turns this into a colour; the threshold itself is a musical judgement, so it
// lives here rather than in the QML.
[[nodiscard]] int tuningStateFor( bool p_hasPitch, double p_cents )
{
    if( !p_hasPitch )
    {
        return 0;
    }

    const double magnitude = std::abs( p_cents );

    if( magnitude <= IN_TUNE_CENTS )
    {
        return 0;
    }

    if( magnitude <= OFF_CENTS )
    {
        return 1;
    }

    return 2;
}

[[nodiscard]] QString noteLabelFor( double p_frequencyHz, double p_referencePitchHz )
{
    if( p_frequencyHz <= 0.0 )
    {
        return QStringLiteral( "\u2014" );
    }

    const std::optional<domain::Note> note = nearestNoteFor( p_frequencyHz, p_referencePitchHz );

    if( !note.has_value() )
    {
        return QStringLiteral( "%1 Hz" ).arg( p_frequencyHz, 0, 'f', 1 );
    }

    // Une seule decimale, et c'est un choix : deux decimales decriraient une exactitude que la mesure n'a pas. Le
    // dernier chiffre est une LECTURE, pas une verite - alors que la page d'un exercice, elle, affiche deux decimales
    // pour une frequence CALCULEE, qui est exacte. Mieux vaut un chiffre honnete qu'un chiffre flatteur.
    return QStringLiteral( "%1  %2 Hz" )
      .arg( QString::fromStdString( note->name() ) )
      .arg( p_frequencyHz, 0, 'f', 1 );
}

}    // namespace

MicrophoneController::MicrophoneController( QStringList p_deviceNames,
                                            DetectorFactory p_factory,
                                            musichien::domain::PlayerPreferences * p_preferences,
                                            musichien::domain::NotePlayer * p_notePlayer,
                                            QObject * p_parent )
  : QObject{ p_parent }
  , m_deviceNames{ std::move( p_deviceNames ) }
  , m_factory{ std::move( p_factory ) }
  , m_preferences{ p_preferences }
  , m_notePlayer{ p_notePlayer }
{
    // An empty list would leave the ComboBox with nothing to show. The message says what to LOOK AT: on a desktop
    // this is almost always a sound card whose active profile has no input - the microphone exists, ALSA sees it, and
    // the audio server simply does not expose it. Telling the player that is worth more than saying "nothing here".
    if( m_deviceNames.isEmpty() )
    {
        // COURT, et c'est deliberé : ce message est une ENTREE de liste deroulante, et un ComboBox prend la largeur de
        // son texte. Quatre-vingt-cinq caracteres elargissaient tout le dialogue des reglages - bien plus large que
        // l'ecran - et faisaient apparaitre un defilement horizontal. Le conseil reste, en abrégé : c'est lui qui aide.
        m_deviceNames = { tr( "Aucune entrée audio (voir ta carte son)" ) };
    }

    // Une premiere cible des l'ouverture : la page de chant ne doit jamais s'afficher sans rien a chanter.
    newSingingQuestion();
}

MicrophoneController::~MicrophoneController() = default;

void MicrophoneController::selectDevice( int p_deviceIndex )
{
    if( p_deviceIndex < 0 || p_deviceIndex >= m_deviceNames.size() )
    {
        return;
    }

    m_currentDeviceIndex = p_deviceIndex;

    m_detector.reset();

    if( m_isListening )
    {
        ensureDetector();
        m_detector->start( [this]( float p_frequencyHz ) { onPitch( p_frequencyHz ); } );
    }

    emit currentDeviceIndexChanged();
}

void MicrophoneController::startTest()
{
    // The microphone is a RUNTIME permission on Android; on the desktop it is always granted and the callback fires
    // immediately. Asking here, at the moment of use, is the whole point: never at launch, never for nothing.
    QCoreApplication::instance()->requestPermission( QMicrophonePermission{}, [this]( const QPermission & p_permission ) {
        if( p_permission.status() != Qt::PermissionStatus::Granted )
        {
            return;
        }

        ensureDetector();

        if( m_detector )
        {
            m_detector->start( [this]( float p_frequencyHz ) { onPitch( p_frequencyHz ); } );
            m_isListening = true;

            emit isListeningChanged();
        }
    } );
}

void MicrophoneController::stopTest()
{
    if( m_detector )
    {
        m_detector->stop();
    }

    m_isListening = false;

    emit isListeningChanged();

    onPitch( 0.0F );
}

void MicrophoneController::ensureListening()
{
    // Ne rien faire quand le micro est deja ouvert : relancer le peripherique s'entendrait sous la forme d'un clic, et
    // l'ecran qui demande l'accordeur n'a aucune raison de faire ce bruit.
    if( m_isListening )
    {
        return;
    }

    startTest();
}

void MicrophoneController::newSingingQuestion()
{
    // Une petite liste d'intervalles CHANTABLES : on reste dans l'octave, et on ecarte pour l'instant ce que la voix
    // trouve le plus dur (la seconde mineure, le triton). Une grosse tolerance vaut mieux qu'un exercice decourageant.
    constexpr std::array<int, 5> SINGABLE_SEMITONES{ 2, 3, 4, 5, 7 };

    std::uniform_int_distribution<std::size_t> distribution{ 0, SINGABLE_SEMITONES.size() - 1 };

    m_singingTargetSemitones = SINGABLE_SEMITONES.at( distribution( m_singingRandomEngine ) );

    m_sungIntervalDetector.reset();

    emit singingTargetChanged();
    emit sungIntervalChanged();
}

void MicrophoneController::setSingingTarget( int p_semitones )
{
    m_singingTargetSemitones = p_semitones;
    m_sungIntervalDetector.reset();

    emit singingTargetChanged();
    emit sungIntervalChanged();
}

void MicrophoneController::playSingingTarget()
{
    if( m_notePlayer == nullptr )
    {
        return;
    }

    // La tonique de la cible est la note de reference du reglage : pour le tempere egal elle ne change rien, pour
    // les autres elle donne son sens a l'intervalle. Meme source que l'accordeur, donc jamais en desaccord.
    const std::int32_t rootMidi =
      ( m_preferences != nullptr ) ? m_preferences->storedTuningRoot().midiNumber() : 60;

    const std::array<domain::Note, 2> notes{ domain::Note{ rootMidi },
                                             domain::Note{ rootMidi + m_singingTargetSemitones } };

    m_notePlayer->playMelody( notes, std::chrono::milliseconds{ 400 } );
}

void MicrophoneController::startSingingCapture()
{
    m_sungIntervalDetector.reset();
    m_isSingingCaptureActive = true;
    m_pitchClock.start();

    emit sungIntervalChanged();
    emit singingCaptureStateChanged();

    // Le micro s'ouvre s'il ne l'est pas : le jeu ecoute de toute facon, mais un exercice de chant muet serait le
    // pire des echecs - il ferait porter au joueur la faute d'un peripherique ferme.
    ensureListening();
}

void MicrophoneController::stopSingingCapture()
{
    // La CAPTURE s'arrete, le micro reste ouvert : le jeu est bati sur lui, et fermer l'ecoute ici eteindrait aussi
    // l'accordeur, pour toute la session, sans que personne ne l'ait demandé.
    m_isSingingCaptureActive = false;

    emit singingCaptureStateChanged();
}

QString MicrophoneController::singingTargetLabel() const
{
    const QString identifier = QString::fromStdString( domain::Interval{ m_singingTargetSemitones }.identifier() );

    // Le nom francais : une etiquette d'ECRAN, pas une regle du jeu. L'identifiant, lui, vient du domaine, comme sur
    // les boutons du banc d'essai.
    QString frenchName;

    switch( m_singingTargetSemitones )
    {
        case 2:
            frenchName = tr( "seconde majeure" );
            break;
        case 3:
            frenchName = tr( "tierce mineure" );
            break;
        case 4:
            frenchName = tr( "tierce majeure" );
            break;
        case 5:
            frenchName = tr( "quarte juste" );
            break;
        case 7:
            frenchName = tr( "quinte juste" );
            break;
        default:
            frenchName = QString::fromStdString( domain::Interval{ m_singingTargetSemitones }.name() );
            break;
    }

    // Les cibles sont toujours MONTANTES pour l'instant ; le jour ou l'on en tirera des descendantes, le mot suivra.
    return QStringLiteral( "%1 · %2 %3" ).arg( identifier ).arg( frenchName ).arg( tr( "ascendante" ) );
}

void MicrophoneController::startSingingSession()
{
    m_singingQuestionIndex = 0;
    m_singingCorrectCount = 0;

    emit singingQuestionChanged();

    newSingingQuestion();
}

int MicrophoneController::sungVerdict() const
{
    const domain::SungIntervalDetector::Reading & reading = m_sungIntervalDetector.reading();

    if( !reading.hasInterval() )
    {
        return 0;
    }

    // Un demi-ton de tolerance : chanter juste veut dire "la bonne note", pas "le bon cent".
    return ( std::abs( reading.semitones() - m_singingTargetSemitones ) <= 1 ) ? 1 : 2;
}

void MicrophoneController::ensureDetector()
{
    if( m_detector )
    {
        return;
    }

    if( m_factory )
    {
        m_detector = m_factory( m_currentDeviceIndex );
    }
}

void MicrophoneController::onPitch( float p_frequencyHz )
{
    const auto rawFrequency = static_cast<double>( p_frequencyHz );

    // Inertie douce : la VOIX vibre, l'affichage ne doit pas. On garde 80% de la lecture precedente a chaque pas -
    // c'est ce qui donne a la boule son mouvement lisse au lieu d'un tremblement de mesures instables.
    constexpr double DISPLAY_SMOOTHING = 0.2;

    double frequency = rawFrequency;

    if( m_detectedFrequencyHz > 0.0 && rawFrequency > 0.0 )
    {
        frequency = m_detectedFrequencyHz + ( ( rawFrequency - m_detectedFrequencyHz ) * DISPLAY_SMOOTHING );
    }

    m_detectedFrequencyHz = frequency;

    const double referencePitch = ( m_preferences != nullptr ) ? m_preferences->storedReferencePitch() : 440.0;

    m_detectedPitchRatio = pitchRatioFor( m_detectedFrequencyHz );
    m_detectedNoteLabel = noteLabelFor( m_detectedFrequencyHz, referencePitch );

    // The continuous MIDI position of the voice: 69 is A4, 69.5 is halfway to the next note. The staff draws the
    // ball from this, so that a note sung perfectly in tune lands exactly on its line or space.
    m_detectedMidi = m_detectedFrequencyHz > 0.0
                       ? static_cast<double>( domain::REFERENCE_MIDI_NUMBER )
                           + ( static_cast<double>( domain::SEMITONES_PER_OCTAVE )
                               * std::log2( m_detectedFrequencyHz / referencePitch ) )
                       : 0.0;

    // The ball's exact place on the staff: rounded to the nearest note, so a C is always on its space, then folded
    // onto the octave. The cents and the colour carry the fine tuning, the ball carries WHICH note it is.
    const std::int32_t nearestMidi = static_cast<std::int32_t>( std::lround( m_detectedMidi ) );

    m_detectedStaffFraction = ( m_detectedFrequencyHz > 0.0 ) ? domain::StaffPosition::fraction( nearestMidi ) : 0.5;

    // Et de combien d'octaves la note REELLE se trouve ailleurs : c'est ce qui permet a l'ecran de le dire a cote de
    // la boule, et a un accordeur de lire une hauteur ABSOLUE sans quitter la portee des yeux. Zero quand la boule
    // dit la verite entiere.
    m_detectedOctaveShift = ( m_detectedFrequencyHz > 0.0 ) ? domain::StaffPosition::octaveShift( nearestMidi ) : 0;

    // The tuner part: how far the voice is from the note it is closest to. This is what makes the page useful
    // outside the game - checking a guitar string, or hearing how flat yesterday's cold left the voice.
    const std::optional<domain::Note> nearest = nearestNoteFor( m_detectedFrequencyHz, referencePitch );

    if( nearest.has_value() )
    {
        // The tuner honours the chosen tuning AND the chosen diapason. In equal temperament the reference is the note
        // itself; in the others it depends on the root - and this is the SAME calculation the audio layer will use to
        // play intervals, so the tuner and the game will never disagree.
        const domain::Temperament temperament = ( m_preferences != nullptr )
                                                  ? m_preferences->storedTemperament()
                                                  : domain::Temperament::Equal;

        const domain::Note root = ( m_preferences != nullptr ) ? m_preferences->storedTuningRoot()
                                                               : domain::Note{ 60 };

        m_detectedCents = domain::centsBetween( m_detectedFrequencyHz,
                                                domain::frequencyFor( *nearest, root, temperament, referencePitch ) );
    }
    else
    {
        m_detectedCents = 0.0;
    }

    m_detectedTuningState = tuningStateFor( nearest.has_value(), m_detectedCents );

    emit detectedFrequencyHzChanged();
    emit detectedPitchRatioChanged();
    emit detectedMidiChanged();
    emit detectedStaffFractionChanged();
    emit detectedOctaveShiftChanged();
    emit detectedNoteLabelChanged();
    emit detectedCentsChanged();
    emit detectedTuningStateChanged();

    // La question chantee : le detecteur a besoin du TEMPS reel ecoule entre deux lectures, et c'est l'horloge de ce
    // controleur qui le lui donne - un detecteur sans horloge reste une regle pure, donc testable.
    if( m_isSingingCaptureActive )
    {
        const auto elapsedMilliseconds = static_cast<std::int32_t>( m_pitchClock.restart() );

        m_sungIntervalDetector.update( m_detectedFrequencyHz, referencePitch, elapsedMilliseconds );

        if( m_sungIntervalDetector.reading().hasInterval() )
        {
            // La reponse est complete : on rend le micro, on compte, et on avance d'une question.
            stopSingingCapture();

            if( sungVerdict() == 1 )
            {
                ++m_singingCorrectCount;
            }

            ++m_singingQuestionIndex;

            emit singingQuestionChanged();
        }

        emit sungIntervalChanged();
    }
}

}    // namespace musichien::ui
