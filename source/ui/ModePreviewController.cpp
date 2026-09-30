#include "ui/ModePreviewController.h"

#include "ui/ModeDescription.h"

#include <array>
#include <chrono>
#include <vector>

namespace musichien::ui
{

namespace
{

// La tonique du banc d'essai : re 4. C'est celle de tous les tableaux de la note 27 du vault, donc celle qui permet de
// comparer ce qu'on entend a ce qui est ecrit.
constexpr std::int32_t TONIC_MIDI_NUMBER = 62;

// Le bourdon : la tonique deux octaves sous la melodie, et sa QUINTE juste.
//
// Roger a tranche deux fois de suite : une note seule dit « ceci est la tonique », une quinte dit « ceci est le
// CENTRE », et ce n'est pas la meme information a entendre.
constexpr std::int32_t DRONE_ROOT_MIDI_NUMBER = 38;
constexpr std::int32_t FIFTH_IN_SEMITONES = 7;

// Les durees : une note assez longue pour que la couleur se dise, et un silence court pour que la phrase avance. Ce
// sont des decisions MUSICALES, donc elles se reglent a l'oreille - comme celles du banc d'essai des intervalles.
constexpr std::chrono::milliseconds NOTE_DURATION{ 420 };
constexpr std::chrono::milliseconds GAP{ 70 };

}    // namespace

ModePreviewController::ModePreviewController( domain::NotePlayer & p_notePlayer, QObject * p_parent )
  : QObject{ p_parent }
  , m_notePlayer{ p_notePlayer }
{
    // La liste vient du domaine, et elle est construite une seule fois : l'ecran demande les modes, il ne les compose
    // jamais.
    m_modes = describeAllModes();
}

QVariantList ModePreviewController::modes() const
{
    return m_modes;
}

QVariantMap ModePreviewController::lastPlayedMode() const
{
    return m_lastPlayedMode;
}

void ModePreviewController::setTuning( domain::TuningContext p_tuning )
{
    m_tuning = p_tuning;
}

void ModePreviewController::playMode( int p_index )
{
    // Le test du SIGNE d'abord, et la borne ensuite : convertir un index negatif en size_t en ferait un tres grand
    // nombre, et la comparaison suivante passerait pour la mauvaise raison.
    if( p_index < 0 )
    {
        return;
    }

    if( static_cast<std::size_t>( p_index ) >= domain::MODE_COUNT )
    {
        return;
    }

    const auto mode = static_cast<domain::Mode>( p_index );

    const domain::Note tonic{ TONIC_MIDI_NUMBER };

    // La gamme MONTEE puis DESCENDUE.
    //
    // Une couleur s'entend mieux quand elle va et vient, et la descente est ce qui fait entendre ou se trouve le
    // centre : c'est en revenant sur la tonique qu'un mode se dit. Monter seulement laisserait la phrase en l'air.
    const std::vector<domain::Note> ascending = domain::notesOfMode( tonic, mode );

    std::vector<domain::Note> melody = ascending;

    for( auto note = ascending.rbegin() + 1; note != ascending.rend(); ++note )
    {
        melody.push_back( *note );
    }

    const std::array<domain::Note, 2> drone{ domain::Note{ DRONE_ROOT_MIDI_NUMBER },
                                             domain::Note{ DRONE_ROOT_MIDI_NUMBER + FIFTH_IN_SEMITONES } };

    // Le temperament et le diapason sont ceux du joueur, comme partout ailleurs.
    m_notePlayer.setTuning( m_tuning );

    // Le bourdon est demande au PORT, avec sa quinte : c'est le DOMAINE qui decide combien de temps il tient, et
    // l'adaptateur qui choisit avec quel timbre. Le banc d'essai ne dit rien d'autre que le mode qu'il veut entendre -
    // et c'est pour cela que ce qu'il fait entendre est exactement ce que l'exercice fera entendre.
    m_notePlayer.playMelodyOverDrone( melody, drone, NOTE_DURATION, GAP );

    const QVariantMap description = describeMode( mode );

    // On ne notifie que si quelque chose a change : rejouer le meme mode ne doit pas faire clignoter l'ecran.
    if( m_lastPlayedMode != description )
    {
        m_lastPlayedMode = description;

        emit lastPlayedModeChanged();
    }
}

void ModePreviewController::stopPlayback()
{
    m_notePlayer.stopAll();
}

}    // namespace musichien::ui
