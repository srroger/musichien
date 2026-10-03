#include "ui/CourseController.h"

#include "domain/music/Interval.h"

#include <array>
#include <chrono>
#include <utility>

namespace musichien::ui
{

namespace
{

// LA NOTE DE DEPART DES EXEMPLES, et l'ecart entre deux notes d'un intervalle.
//
// Meme valeur que le banc d'essai des intervalles, et ce n'est pas une coincidence : un cours qui ferait entendre le
// meme intervalle autrement qu'en exercice apprendrait au joueur a reconnaitre un timbre qui n'est pas celui du jeu.
constexpr std::int32_t EXERCISE_ROOT_MIDI_NUMBER = 60;

constexpr std::chrono::milliseconds MELODIC_GAP{ 280 };

// Le nom d'une carte, tel que le QML le compare. Des MOTS et non des entiers : c'est le QML qui choisit la forme
// dessinee, et un `kind` lisible vaut mieux qu'un numero a retenir des deux cotes.
constexpr const char * KIND_TEXT = "text";
constexpr const char * KIND_PLAY = "play";
constexpr const char * KIND_LISTEN = "listen";
constexpr const char * KIND_TRY = "try";
constexpr const char * KIND_ANNEXE = "annexe";

[[nodiscard]] const char * kindName( domain::CourseBlock::Kind p_kind ) noexcept
{
    switch( p_kind )
    {
        case domain::CourseBlock::Kind::PlayInterval:
            return KIND_PLAY;
        case domain::CourseBlock::Kind::Listen:
            return KIND_LISTEN;
        case domain::CourseBlock::Kind::TryExercise:
            return KIND_TRY;
        case domain::CourseBlock::Kind::Annexe:
            return KIND_ANNEXE;
        case domain::CourseBlock::Kind::Text:
        default:
            return KIND_TEXT;
    }
}

// Une carte, traduite pour le QML.
//
// TOUS les champs partent, meme ceux que la carte n'utilise pas : le QML choisit ce qu'il dessine d'apres `kind`, et un
// champ absent y vaudrait `undefined` pour TOUTES les cartes a la fois.
[[nodiscard]] QVariantMap describeBlock( const domain::CourseBlock & p_block )
{
    QVariantMap description;

    description.insert( QStringLiteral( "kind" ), QString::fromUtf8( kindName( p_block.kind ) ) );
    description.insert( QStringLiteral( "markdown" ), QString::fromStdString( p_block.markdown ) );
    description.insert( QStringLiteral( "semitones" ), p_block.semitones );
    description.insert( QStringLiteral( "direction" ), static_cast<int>( p_block.direction ) );
    description.insert( QStringLiteral( "caption" ), QString::fromStdString( p_block.caption ) );
    description.insert( QStringLiteral( "source" ), QString::fromStdString( p_block.source ) );
    description.insert( QStringLiteral( "url" ), QString::fromStdString( p_block.url ) );
    description.insert( QStringLiteral( "cardTitle" ), QString::fromStdString( p_block.title ) );
    description.insert( QStringLiteral( "listenFor" ), QString::fromStdString( p_block.listenFor ) );
    description.insert( QStringLiteral( "annexeName" ), QString::fromStdString( p_block.annexeName ) );

    return description;
}

}    // namespace

CourseController::CourseController( domain::NotePlayer & p_notePlayer,
                                    std::vector<domain::Course> p_courses,
                                    QObject * p_parent )
  : QObject{ p_parent }
  , m_notePlayer{ p_notePlayer }
  , m_courses{ std::move( p_courses ) }
{
    // Le catalogue est construit UNE fois : la page l'affiche, elle ne le compose jamais.
    for( const domain::Course & course : m_courses )
    {
        QVariantMap entry;

        entry.insert( QStringLiteral( "title" ), QString::fromStdString( course.title ) );
        entry.insert( QStringLiteral( "subtitle" ), QString::fromStdString( course.subtitle ) );
        entry.insert( QStringLiteral( "chapter" ), course.chapter );
        entry.insert( QStringLiteral( "order" ), course.order );
        entry.insert( QStringLiteral( "blockCount" ), static_cast<int>( course.blocks.size() ) );

        m_library.append( entry );
    }
}

QVariantList CourseController::library() const
{
    return m_library;
}

QString CourseController::title() const
{
    if( m_readingIndex < 0 )
    {
        return {};
    }

    return QString::fromStdString( m_courses.at( static_cast<std::size_t>( m_readingIndex ) ).title );
}

QString CourseController::subtitle() const
{
    if( m_readingIndex < 0 )
    {
        return {};
    }

    return QString::fromStdString( m_courses.at( static_cast<std::size_t>( m_readingIndex ) ).subtitle );
}

QVariantList CourseController::blocks() const
{
    QVariantList blockList;

    if( m_readingIndex < 0 )
    {
        return blockList;
    }

    // L'ORDRE EST CELUI DU FICHIER, et c'est tout l'interet d'avoir garde une SEQUENCE : l'interface n'a rien a
    // retrouver, elle dessine ce qu'on lui donne dans l'ordre ou on le lui donne.
    for( const domain::CourseBlock & block : m_courses.at( static_cast<std::size_t>( m_readingIndex ) ).blocks )
    {
        blockList.append( describeBlock( block ) );
    }

    return blockList;
}

void CourseController::open( int p_index )
{
    // Un index qui n'existe pas ne fait RIEN. Une page ne doit pas pouvoir planter sur un clic de trop, et c'est le
    // seul endroit ou la liste des cours rencontre un entier venu de l'exterieur.
    if( ( p_index < 0 ) || ( p_index >= static_cast<int>( m_courses.size() ) ) )
    {
        return;
    }

    m_readingIndex = p_index;

    emit courseChanged();
}

void CourseController::close()
{
    // Quitter la page ne doit jamais laisser un son derriere elle : sur un telephone, c'est une batterie qui se vide.
    stopPlayback();

    m_readingIndex = -1;

    emit courseChanged();
}

void CourseController::playInterval( int p_semitones, int p_direction )
{
    const domain::Note rootNote{ EXERCISE_ROOT_MIDI_NUMBER };
    const domain::Note upperNote = rootNote.transposedBy( p_semitones );

    const auto direction = static_cast<domain::IntervalDirection>( p_direction );

    // LE SENS EST DANS L'ORDRE DES NOTES, et c'est le seul endroit ou il se dit : le domaine connait une MAGNITUDE, et la
    // direction appartient a ce qu'on joue - le fichier d'origine le dit deja.
    //
    // Roger l'a entendu tout de suite : « le sol -> do me fait encore l'ascendant ». Il avait raison, et la cause etait un
    // copier-coller du banc d'essai des intervalles, qui ne joue jamais qu'en montant parce qu'il compare des couleurs.
    const std::array<domain::Note, 2> notes = ( direction == domain::IntervalDirection::Descending )
                                                ? std::array<domain::Note, 2>{ upperNote, rootNote }
                                                : std::array<domain::Note, 2>{ rootNote, upperNote };

    if( direction == domain::IntervalDirection::Harmonic )
    {
        m_notePlayer.playChord( notes );
    }
    else
    {
        m_notePlayer.playMelody( notes, MELODIC_GAP );
    }
}

void CourseController::stopPlayback()
{
    m_notePlayer.stopAll();
}

}    // namespace musichien::ui
