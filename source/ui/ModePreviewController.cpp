#include "ui/ModePreviewController.h"

#include "domain/exercise/PlayerPreferences.h"
#include "ui/ModeDescription.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <optional>
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

// Le silence entre deux notes d'une phrase : court, pour que la phrase avance. C'est une decision MUSICALE, donc elle se
// regle a l'oreille - comme celles du banc d'essai des intervalles.
constexpr std::chrono::milliseconds GAP{ 70 };

// Le plancher du tempo d'une phrase : sous quarante, elle traine au point de ne plus etre une phrase. C'est la meme
// borne que celle du domaine, et elle sert ici a un seul cas - le tirage de variation qui descendrait sous elle.
constexpr std::int32_t MINIMUM_PHRASE_TEMPO = 40;

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

void ModePreviewController::setPhraseBook( const domain::PhraseBook & p_phraseBook )
{
    m_phraseBook = &p_phraseBook;
}

void ModePreviewController::playMode( int p_index )
{
    // Le test du SIGNE d'abord, et la borne ensuite : convertir un index negatif en size_t en ferait un tres grand
    // nombre, et la comparaison suivante passerait pour la mauvaise raison.
    if( p_index < 0 )
    {
        return;
    }

    if( std::cmp_greater_equal( p_index, domain::MODE_COUNT ) )
    {
        return;
    }

    const auto mode = static_cast<domain::Mode>( p_index );

    const domain::Note tonic{ TONIC_MIDI_NUMBER };

    // Le TEMPO vient du reglage du joueur, et pas d'une constante ecrite ici.
    //
    // Roger a vu le defaut en jouant : « le changement de bpm n'a pas l'air de fonctionner pour les modes ». Il avait
    // raison, et pour une bonne raison : le reglage ne s'appliquait qu'aux PHRASES. Une gamme et une phrase sont la meme
    // chose pour l'oreille - une melodie sur un bourdon - donc un reglage qui ne s'entendirait que sur l'une des deux
    // serait un reglage a moitie fait.
    //
    // La duree d'un pas garde le RAPPORT du banc d'essai : une demi-seconde de note pour un temps a 72 bpm, et un
    // silence six fois plus court. Changer le tempo change la vitesse, jamais l'articulation.
    const std::int32_t bpm = ( m_preferences != nullptr ) ? m_preferences->storedPhraseTempoBpm() : 72;

    const std::int32_t variation = ( m_preferences != nullptr ) ? m_preferences->storedPhraseTempoVariation() : 0;

    std::int32_t playedBpm = bpm;

    if( variation > 0 )
    {
        std::uniform_int_distribution<std::int32_t> distribution{ -variation, variation };

        playedBpm = std::max( MINIMUM_PHRASE_TEMPO, bpm + distribution( m_randomEngine ) );
    }

    const auto millisecondsPerBeat = std::chrono::milliseconds{ 60000 / playedBpm };

    const auto noteDuration = millisecondsPerBeat / 2;
    const auto gap = noteDuration / 6;

    // La gamme MONTEE puis DESCENDUE : la regle vit dans le DOMAINE, parce que l'apercu d'un instrument fait exactement
    // le meme geste. Une couleur s'entend mieux quand elle va et vient, et c'est la descente qui fait entendre ou se
    // trouve le centre : c'est en revenant sur la tonique qu'un mode se dit.
    const std::vector<domain::Note> melody = domain::modeScaleUpAndDown( tonic, mode );

    // LE PAS de la gamme, pour que la roue sache a quelle vitesse parcourir son chemin. Il se calcule a partir des MEMES
    // valeurs que le son qui va suivre, donc le dessin ne peut pas prendre du retard sur la musique.
    m_lastScaleNoteStepMs = static_cast<int>( ( noteDuration + gap ).count() );

    const std::array<domain::Note, 2> drone{ domain::Note{ DRONE_ROOT_MIDI_NUMBER },
                                             domain::Note{ DRONE_ROOT_MIDI_NUMBER + FIFTH_IN_SEMITONES } };

    // Le temperament et le diapason sont ceux du joueur, comme partout ailleurs.
    m_notePlayer.setTuning( m_tuning );

    // Le bourdon est demande au PORT, avec sa quinte : c'est le DOMAINE qui decide combien de temps il tient, et
    // l'adaptateur qui choisit avec quel timbre. Le banc d'essai ne dit rien d'autre que le mode qu'il veut entendre -
    // et c'est pour cela que ce qu'il fait entendre est exactement ce que l'exercice fera entendre.
    m_notePlayer.playMelodyOverDrone( melody, drone, noteDuration, gap, SCALE_FRAMING );

    const QVariantMap description = describeMode( mode );

    // On ne notifie que si quelque chose a change : rejouer le meme mode ne doit pas faire clignoter l'ecran.
    if( m_lastPlayedMode != description )
    {
        m_lastPlayedMode = description;

        emit lastPlayedModeChanged();
    }

    // Et la roue qui va avec : ses sept notes allumees, la tonique marquee.
    showCircleFor( mode, TONIC_MIDI_NUMBER % domain::SEMITONES_PER_OCTAVE );
}

QVariantMap ModePreviewController::lastPlayedPhrase() const
{
    return m_lastPlayedPhrase;
}

QVariantList ModePreviewController::playedModeCircle() const
{
    return m_playedModeCircle;
}

void ModePreviewController::setPreferences( const domain::PlayerPreferences & p_preferences )
{
    m_preferences = &p_preferences;
}

void ModePreviewController::showCircleFor( domain::Mode p_mode, std::int32_t p_tonicPitchClass )
{
    const QVariantList circle = describeModeCircle( p_mode, p_tonicPitchClass );

    if( m_playedModeCircle != circle )
    {
        m_playedModeCircle = circle;

        emit playedModeCircleChanged();
    }
}

int ModePreviewController::phraseCountForMode( int p_index ) const
{
    // Le test du SIGNE avant la borne, comme partout : convertir un index negatif en size_t en ferait un tres grand
    // nombre, et la comparaison suivante passerait pour la mauvaise raison.
    if( ( m_phraseBook == nullptr ) || ( p_index < 0 ) || std::cmp_greater_equal( p_index, domain::MODE_COUNT ) )
    {
        return 0;
    }

    return static_cast<int>( m_phraseBook->phraseCountFor( static_cast<domain::Mode>( p_index ) ) );
}

void ModePreviewController::playPhraseOfMode( int p_index )
{
    if( ( m_phraseBook == nullptr ) || ( p_index < 0 ) || std::cmp_greater_equal( p_index, domain::MODE_COUNT ) )
    {
        return;
    }

    const auto mode = static_cast<domain::Mode>( p_index );

    const std::optional<domain::Phrase> phrase = m_phraseBook->drawPhraseFor( mode, m_randomEngine );

    if( !phrase.has_value() )
    {
        // Aucune phrase pour ce mode : rien ne sonne, et l'ecran n'affiche rien de plus. Ce n'est pas une panne, c'est
        // un contenu qui n'a pas encore servi ce mode - et l'ecran ne devrait meme pas offrir le bouton.
        if( !m_lastPlayedPhrase.isEmpty() )
        {
            m_lastPlayedPhrase = {};

            emit lastPlayedPhraseChanged();
        }

        return;
    }

    m_notePlayer.setTuning( m_tuning );

    // La phrase est jouee TELLE QUE le contenu l'a ecrite : sa tonique, ses DUREES. Ce qui vient du reglage, et de lui
    // seul, est le TEMPO : Roger a demande « de choisir un central et de varier autour de 20-30 bpm », et c'est ce qui
    // casse la monotonie sans changer une seule note.
    //
    // Le centre et l'amplitude, lus AU MOMENT DE JOUER : un reglage change s'entend donc a la phrase suivante, sans
    // qu'aucun cache n'ait a etre tenu a jour.
    const std::int32_t centreBpm = ( m_preferences != nullptr ) ? m_preferences->storedPhraseTempoBpm() : 72;
    const std::int32_t variation = ( m_preferences != nullptr ) ? m_preferences->storedPhraseTempoVariation() : 0;

    std::int32_t bpm = centreBpm;

    if( variation > 0 )
    {
        std::uniform_int_distribution<std::int32_t> distribution{ -variation, variation };

        // Un plancher, parce qu'un tirage qui descendrait sous le plancher du domaine ferait une phrase que personne
        // n'a demandee - et la borne haute, elle, est tenue par la borne du reglage.
        bpm = std::max( MINIMUM_PHRASE_TEMPO, centreBpm + distribution( m_randomEngine ) );
    }

    // Comment la phrase SE JOUE - sa melodie, la duree de chaque pas, le bourdon qui la porte - est calcule par le
    // DOMAINE. C'est ce qui a permis au jeu de poser la meme question sans recopier la moindre ligne, et c'est aussi ce
    // qui garantit qu'une phrase ne sonne pas differemment selon la page qui la joue.
    const domain::PhrasePlayback playback = domain::playbackOf( *phrase, bpm );

    m_notePlayer.playPhraseOverDrone( playback.melody, playback.durations, playback.drone, GAP );

    const QVariantMap description = describePhrase( *phrase );

    if( m_lastPlayedPhrase != description )
    {
        m_lastPlayedPhrase = description;

        emit lastPlayedPhraseChanged();
    }

    // La phrase fait aussi connaitre son MODE : l'ecran n'a pas a deviner lequel a sonne, puisque c'est justement la
    // question que l'exercice posera un jour.
    const QVariantMap modeDescription = describeMode( mode );

    if( m_lastPlayedMode != modeDescription )
    {
        m_lastPlayedMode = modeDescription;

        emit lastPlayedModeChanged();
    }

    // Et la roue, sur la tonique de la PHRASE : c'est celle que l'oreille vient d'entendre sous elle.
    showCircleFor( mode, phrase->tonic.pitchClassIndex() );
}

void ModePreviewController::stopPlayback()
{
    m_notePlayer.stopAll();
}

}    // namespace musichien::ui
