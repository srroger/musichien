#include "ui/ScaleTrainingController.h"

#include "ui/ModeDescription.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace musichien::ui
{

namespace
{

// COMBIEN DE NOMS SONT PROPOSES. Quatre : assez pour qu'il faille entendre, assez peu pour qu'un bouton reste une cible.
constexpr std::size_t CHOICE_COUNT = 4;

// La fenetre des toniques, celle des questions de mode : c'est la MEME oreille, donc les memes hauteurs. Un bourdon aigu
// ne tient pas sous une gamme - il se bat avec elle au lieu de la poser.
constexpr std::int32_t LOWEST_TONIC_MIDI = 38;
constexpr std::int32_t HIGHEST_TONIC_MIDI = 45;

// La melodie est posee DEUX OCTAVES au-dessus du bourdon : le bourdon tient les graves, et une melodie qui partagerait son
// octave se battrait avec lui.
constexpr std::int32_t MELODY_OCTAVE_OFFSET = 24;

// Une quinte, en demi-tons : le bourdon est la tonique ET sa quinte - deux notes tenues, jamais une seule.
constexpr std::int32_t FIFTH_IN_SEMITONES = 7;

constexpr std::chrono::milliseconds NOTE_DURATION{ 420 };
constexpr std::chrono::milliseconds NOTE_GAP{ 110 };

// L'ENCADREMENT DU BOURDON : le temps qu'il sonne seul avant la gamme, et apres. C'est celui du domaine, ecrit une fois
// pour que l'ecran puisse le LIRE (voir playbackLeadInMs) au lieu de le supposer.
constexpr domain::DroneFraming DRONE_FRAMING{};

}    // namespace

ScaleTrainingController::ScaleTrainingController( domain::NotePlayer & p_notePlayer, QObject * p_parent )
  : QObject( p_parent )
  , m_notePlayer( p_notePlayer )
  , m_randomEngine( std::random_device{}() )
{
}

QString ScaleTrainingController::verdict() const
{
    if( !m_hasAnswered )
    {
        // AVANT LA REPONSE, LE NOM N'EST PAS DIT : c'est la reponse. Un verdict qui parlerait tout seul apprendrait au
        // joueur a attendre le texte au lieu d'ecouter la gamme.
        return {};
    }

    const QString name = describeScaleName( m_scale );

    return m_wasCorrect ? tr( "Bien joué : %1." ).arg( name ) : tr( "C'était : %1." ).arg( name );
}

void ScaleTrainingController::start()
{
    m_askedCount = 0;
    m_correctCount = 0;

    drawQuestion();

    emit scoreChanged();
}

void ScaleTrainingController::next()
{
    drawQuestion();
}

void ScaleTrainingController::answer( int p_index )
{
    // Une reponse arrivant deux fois, ou hors liste, ne change rien : la premiere compte, et une seule.
    if( m_hasAnswered || ( p_index < 0 ) || ( p_index >= static_cast<int>( m_choices.size() ) ) )
    {
        return;
    }

    m_answeredIndex = p_index;
    m_hasAnswered = true;
    m_wasCorrect = ( p_index == m_correctIndex );

    ++m_askedCount;

    if( m_wasCorrect )
    {
        ++m_correctCount;
    }

    emit questionChanged();
    emit scoreChanged();
}

void ScaleTrainingController::playAgain()
{
    playCurrentScale();
}

void ScaleTrainingController::drawQuestion()
{
    std::uniform_int_distribution<std::size_t> scaleDraw{ 0, domain::SCALE_COUNT - 1 };
    std::uniform_int_distribution<std::int32_t> tonicDraw{ LOWEST_TONIC_MIDI, HIGHEST_TONIC_MIDI };

    m_scale = domain::scaleFromIndex( scaleDraw( m_randomEngine ) );
    m_tonic = domain::Note{ tonicDraw( m_randomEngine ) };

    // LE CERCLE, REPERE SUR SA PROPRE TONIQUE : une gamme ne s'entend que depuis son centre, donc la tonique est en haut.
    // C'est la seule difference avec une question de mode - la, deux modes s'enchainent et la roue doit les comparer.
    m_circle = describeScaleCircle( m_scale, m_tonic.pitchClassIndex(), m_tonic.pitchClassIndex() );

    // LES NOMS PROPOSES : le bon, puis TROIS leurres tires parmi les autres, sans doublon. L'ordre est melange ensuite -
    // sinon le bon nom garderait toujours la meme place.
    std::array<domain::Scale, CHOICE_COUNT> drawn{};
    drawn.at( 0 ) = m_scale;

    std::size_t filled = 1;

    while( filled < CHOICE_COUNT )
    {
        const domain::Scale candidate = domain::scaleFromIndex( scaleDraw( m_randomEngine ) );

        const bool alreadyDrawn = std::ranges::any_of(
          drawn.begin(), drawn.begin() + static_cast<std::ptrdiff_t>( filled ), [candidate]( domain::Scale p_drawn ) { return p_drawn == candidate; } );

        if( !alreadyDrawn )
        {
            drawn.at( filled ) = candidate;
            ++filled;
        }
    }

    std::ranges::shuffle( drawn, m_randomEngine );

    m_choices.clear();

    for( std::size_t index = 0; index < drawn.size(); ++index )
    {
        m_choices.append( describeScaleName( drawn.at( index ) ) );

        if( drawn.at( index ) == m_scale )
        {
            m_correctIndex = static_cast<int>( index );
        }
    }

    m_answeredIndex = -1;
    m_hasAnswered = false;
    m_wasCorrect = false;

    emit questionChanged();

    playCurrentScale();
}

int ScaleTrainingController::playbackLeadInMs() const noexcept
{
    // LE BOURDON SONNE SEUL CE TEMPS-LA avant la premiere note : c'est le `leadIn` du domaine, et l'ecran l'attend pour
    // faire partir sa tete. Le lire ici plutot que de le deviner ailleurs, c'est ce qui garantit que les deux partent
    // ensemble le jour ou cette valeur changera.
    return static_cast<int>( DRONE_FRAMING.leadIn.count() );
}

int ScaleTrainingController::playbackNoteStepMs() const noexcept
{
    // Une note, puis le silence qui la separe de la suivante : c'est le pas de la tete.
    return static_cast<int>( ( NOTE_DURATION + NOTE_GAP ).count() );
}

void ScaleTrainingController::playCurrentScale()
{
    const domain::ScaleDegrees degrees = domain::scaleDegreeOffsets( m_scale );

    std::vector<domain::Note> melody;

    melody.reserve( degrees.count + 1 );

    for( std::size_t degree = 0; degree < degrees.count; ++degree )
    {
        melody.push_back( m_tonic.transposedBy( MELODY_OCTAVE_OFFSET + degrees.offsets.at( degree ) ) );
    }

    // ELLE SE REFERME SUR SA TONIQUE, une octave plus haut : sans cette note la gamme reste en l'air, et c'est pourtant la
    // derniere note qui NOMME le centre - la ou le bourdon le donne.
    melody.push_back( m_tonic.transposedBy( MELODY_OCTAVE_OFFSET + domain::SEMITONES_PER_OCTAVE ) );

    // LE BOURDON : la tonique et sa quinte, le meme geste que pour les modes. Il installe un centre sans colorer lui-meme,
    // ce qui est exactement ce qu'il faut pour entendre une gamme par-dessus.
    const std::array<domain::Note, 2> drone{ m_tonic, m_tonic.transposedBy( FIFTH_IN_SEMITONES ) };

    m_notePlayer.playMelodyOverDrone( melody, drone, NOTE_DURATION, NOTE_GAP, DRONE_FRAMING );

    emit playbackStarted();
}

}    // namespace musichien::ui
