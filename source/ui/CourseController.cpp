#include "ui/CourseController.h"

#include "domain/music/Interval.h"
#include "domain/music/Mode.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
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

// ":: chante" - la carte qui ouvre l'outil de chant. C'est une PORTE, et non une donnee : elle ne porte qu'une distance.
constexpr const char * KIND_SING = "sing";
constexpr const char * KIND_ANNEXE = "annexe";

// ":: image" - une illustration du propos. Le QML dessine l'image, puis sa legende : rien de plus.
constexpr const char * KIND_IMAGE = "image";

// ":: serie" - la serie harmonique d'une note, jouee. Le jeu la joue, l'ecran la nomme.
constexpr const char * KIND_SERIES = "serie";

// ":: bourdon" - le bourdon du jeu, tenu : la tonique et sa quinte, sans tierce.
constexpr const char * KIND_DRONE = "bourdon";

// ":: cycle" - la chaine des quintes, parcourue.
constexpr const char * KIND_CYCLE = "cycle";

// ":: gamme" - la gamme d'un mode, sur le bourdon.
constexpr const char * KIND_SCALE = "gamme";

// ":: schéma" - un DESSIN. Le QML le peint d'apres son nom, puis ecrit sa legende : le controleur, lui, ne sait meme
// pas qu'il y a un dessin.
constexpr const char * KIND_SCHEMA = "schema";

// LA QUINTE, EN DEMI-TONS. Elle sert au bourdon ET a la chaine des quintes : une seule definition, donc pas deux
// valeurs a tenir d'accord.
constexpr std::int32_t FIFTH_IN_SEMITONES = 7;

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
        case domain::CourseBlock::Kind::SingInterval:
            return KIND_SING;
        case domain::CourseBlock::Kind::Annexe:
            return KIND_ANNEXE;
        case domain::CourseBlock::Kind::Image:
            return KIND_IMAGE;
        case domain::CourseBlock::Kind::HarmonicSeries:
            return KIND_SERIES;
        case domain::CourseBlock::Kind::Drone:
            return KIND_DRONE;
        case domain::CourseBlock::Kind::FifthCycle:
            return KIND_CYCLE;
        case domain::CourseBlock::Kind::ModeScale:
            return KIND_SCALE;
        case domain::CourseBlock::Kind::Schema:
            return KIND_SCHEMA;
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
    description.insert( QStringLiteral( "imageName" ), QString::fromStdString( p_block.imageName ) );
    description.insert( QStringLiteral( "fifthCount" ), p_block.fifthCount );
    description.insert( QStringLiteral( "modeIndex" ), p_block.modeIndex );
    description.insert( QStringLiteral( "schemaName" ), QString::fromStdString( p_block.schemaName ) );

    return description;
}

// LES COURS D'ABORD, LES OS A MACHER ENSUITE.
//
// Roger : « tu as mis en avant l'os a macher, alors que c'est plutot le cours qu'il faudrait mettre en avant ». L'ORDRE de
// la liste dit la meme chose que les couleurs, et il n'a rien d'alphabetique : un cours est NUMEROTE (chapitre, ordre), une
// annexe ne l'est pas et se rattache au chapitre d'un autre. Les cours montent donc en premier, et une annexe se range a la
// fin - c'est une annexe, pas une lecon, et la liste doit le dire avant qu'on ait lu un mot.
[[nodiscard]] bool libraryOrder( const domain::Course & p_left, const domain::Course & p_right )
{
    const bool leftIsAnnexe = ( p_left.chapter == 0 );
    const bool rightIsAnnexe = ( p_right.chapter == 0 );

    if( leftIsAnnexe != rightIsAnnexe )
    {
        return !leftIsAnnexe;
    }

    if( p_left.chapter != p_right.chapter )
    {
        return p_left.chapter < p_right.chapter;
    }

    if( p_left.order != p_right.order )
    {
        return p_left.order < p_right.order;
    }

    return p_left.title < p_right.title;
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
    //
    // Il est TRIE avant tout le reste, et le tri porte sur m_courses : la liste et l'ouverture partagent donc le meme
    // ordre, et un index ne peut pas designer deux cours differents selon qui le lit.
    std::ranges::sort( m_courses, libraryOrder );

    for( std::size_t index = 0; index < m_courses.size(); ++index )
    {
        const domain::Course & course = m_courses.at( index );

        QVariantMap entry;

        // LE NUMERO, DEVANT : « 1. La quinte juste ». Roger : « je mettrais bien un "1. La quinte juste" pour bien
        // rappeler que c'est la quinte juste ».
        entry.insert( QStringLiteral( "title" ), displayTitleFor( index ) );
        entry.insert( QStringLiteral( "subtitle" ), QString::fromStdString( course.subtitle ) );
        entry.insert( QStringLiteral( "chapter" ), course.chapter );
        entry.insert( QStringLiteral( "order" ), course.order );
        entry.insert( QStringLiteral( "blockCount" ), static_cast<int>( course.blocks.size() ) );

        // UN OS A MACHER N'EST PAS UN COURS, et la liste doit le dire d'un coup d'oeil - Roger : « je mettrais bien les
        // os a macher d'une autre couleur que les cours ».
        //
        // La difference est deja dans les fichiers, et elle n'est pas inventee ici : un COURS est numerote (chapitre et
        // ordre), une ANNEXE ne l'est pas - elle appartient a un chapitre auquel elle se rattache, et son en-tete porte
        // une 'famille' a la place. Aucun champ nouveau, donc, et aucun fichier a reviser.
        entry.insert( QStringLiteral( "isAnnexe" ), course.chapter == 0 );

        m_library.append( entry );
    }

    // UN RENVOI QUI NE MENE NULLE PART SE DIT AU DEMARRAGE, et non au clic.
    //
    // Le contrat veut qu'une carte « :: annexe » cite le TITRE de l'annexe ; ecrire le nom du fichier est l'erreur
    // naturelle, et c'est celle que ce specimen a commise. Elle ne produit aucun plantage, aucune erreur de compilation,
    // et meme pas une carte morte bien visible : juste une carte qui ne fait rien. Le journal de demarrage est donc le
    // seul endroit ou elle se voit - et c'est le meme esprit que « Musichien: N courses read » : l'application dit ce
    // qu'elle a compris, pas seulement ce qu'elle a lu.
    for( const domain::Course & course : m_courses )
    {
        for( const domain::CourseBlock & block : course.blocks )
        {
            if( block.kind != domain::CourseBlock::Kind::Annexe )
            {
                continue;
            }

            const bool isResolved =
              std::ranges::any_of( m_courses, [&block]( const domain::Course & p_candidate ) {
                  return p_candidate.title == block.annexeName;
              } );

            if( !isResolved )
            {
                std::cerr << "Musichien: '" << course.title << "' points to an annexe named '" << block.annexeName
                          << "', and no course carries that title.\n";
            }
        }
    }
}

QString CourseController::displayTitleFor( std::size_t p_index ) const
{
    const domain::Course & course = m_courses.at( p_index );

    // UN OS A MACHER N'EST PAS NUMEROTE. Son chapitre vaut zero - c'est deja ce qui le range a la fin du catalogue - et
    // lui coller un numero ferait croire qu'il est une lecon.
    if( course.chapter <= 0 )
    {
        return QString::fromStdString( course.title );
    }

    // LE RANG PARMI LES COURS NUMEROTES, et non l'indice dans le catalogue : compter les annexes decalerait tous les
    // numeros d'un coup.
    //
    // Le numero est CALCULE, jamais ecrit dans le fichier : un titre qui porterait son propre numero se desynchroniserait
    // le jour ou deux lecons s'echangent. L'auteur ecrit le titre, et l'ordre du catalogue fait le reste.
    std::size_t rank = 0;

    for( std::size_t index = 0; index <= p_index; ++index )
    {
        if( m_courses.at( index ).chapter > 0 )
        {
            ++rank;
        }
    }

    return QStringLiteral( "%1. %2" ).arg( rank ).arg( QString::fromStdString( course.title ) );
}

void CourseController::openAnnexe( const QString & p_name )
{
    // Le TITRE, compare sans tenir compte de la casse ni des espaces autour : un auteur ecrit ce qu'il veut, et une
    // majuscule ne doit pas fermer une porte.
    const QString wanted = p_name.trimmed();

    for( std::size_t index = 0; index < m_courses.size(); ++index )
    {
        if( QString::fromStdString( m_courses.at( index ).title ).trimmed().compare( wanted, Qt::CaseInsensitive ) == 0 )
        {
            open( static_cast<int>( index ) );

            return;
        }
    }

    // Rien trouve : on ne fait rien, et c'est deliberé. Le constructeur a deja dit AU DEMARRAGE que ce renvoi ne mene
    // nulle part - le joueur, lui, ne doit pas voir un message d'erreur a cause d'une faute de frappe dans un cours.
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

    // LE MEME TITRE QUE DANS LE CATALOGUE, numero compris : la page d'une lecon et sa ligne dans la liste doivent dire la
    // MEME chose, sinon on ne sait plus laquelle on vient d'ouvrir.
    return displayTitleFor( static_cast<std::size_t>( m_readingIndex ) );
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

    const domain::Course & course = m_courses.at( static_cast<std::size_t>( m_readingIndex ) );

    // LA NOTE COMPLETE, quand on l'a demandee - ou quand le cours n'a AUCUNE section : un fichier sans « ## » est une
    // page unique, et refuser de l'afficher serait une facon elegante de ne rien montrer du tout.
    if( ( m_showingWholeNote ) || ( course.sections.empty() ) )
    {
        for( const domain::CourseBlock & block : course.blocks )
        {
            blockList.append( describeBlock( block ) );
        }

        return blockList;
    }

    // SINON, LA PAGE. L'ordre des blocs est celui du fichier, et l'interface n'a rien a retrouver : elle dessine ce
    // qu'on lui donne dans l'ordre ou on le lui donne.
    const domain::CourseSection & section = course.sections.at( static_cast<std::size_t>( m_sectionIndex ) );

    for( const domain::CourseBlock & block : section.blocks )
    {
        blockList.append( describeBlock( block ) );
    }

    return blockList;
}

int CourseController::sectionCount() const noexcept
{
    if( m_readingIndex < 0 )
    {
        return 0;
    }

    return static_cast<int>( m_courses.at( static_cast<std::size_t>( m_readingIndex ) ).sections.size() );
}

QString CourseController::sectionTitle() const
{
    if( ( m_readingIndex < 0 ) || m_showingWholeNote )
    {
        return {};
    }

    const domain::Course & course = m_courses.at( static_cast<std::size_t>( m_readingIndex ) );

    if( ( m_sectionIndex < 0 ) || std::cmp_greater_equal( m_sectionIndex, course.sections.size() ) )
    {
        return {};
    }

    return QString::fromStdString( course.sections.at( static_cast<std::size_t>( m_sectionIndex ) ).title );
}

void CourseController::nextSection()
{
    // AU BOUT, ON NE DEBORDE PAS : la derniere page reste la derniere. C'est la note complete qui prend la suite, et
    // c'est a l'ecran de la proposer - pas au modele de la glisser.
    if( m_sectionIndex + 1 < sectionCount() )
    {
        ++m_sectionIndex;

        emit courseChanged();
    }
}

void CourseController::previousSection()
{
    if( m_sectionIndex > 0 )
    {
        --m_sectionIndex;

        emit courseChanged();
    }
}

void CourseController::setShowingWholeNote( bool p_showingWholeNote )
{
    if( m_showingWholeNote == p_showingWholeNote )
    {
        return;
    }

    m_showingWholeNote = p_showingWholeNote;

    emit courseChanged();
}

void CourseController::open( int p_index )
{
    // Un index qui n'existe pas ne fait RIEN. Une page ne doit pas pouvoir planter sur un clic de trop, et c'est le
    // seul endroit ou la liste des cours rencontre un entier venu de l'exterieur.
    if( ( p_index < 0 ) || std::cmp_greater_equal( p_index, m_courses.size() ) )
    {
        return;
    }

    m_readingIndex = p_index;

    // ON COMMENCE AU DEBUT, ET PAR PAGES : un cours rouvert repart de sa premiere page. Reprendre a la page laissee
    // serait une autre fonctionnalite - et un lecteur qui rouvre une lecon veut la relire, pas la reprendre au milieu.
    m_sectionIndex = 0;
    m_showingWholeNote = false;

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

void CourseController::playHarmonicSeries()
{
    // LES PREMIERS RANGS DE LA SERIE HARMONIQUE D'UN DO, joues l'un apres l'autre.
    //
    // Les rangs, en demi-tons au-dessus du fondamental :
    //   1.  0  do   le fondamental, celui qu'on croit entendre seul
    //   2. 12  do   l'octave
    //   3. 19  sol  LA QUINTE - celle qui nous occupe, et deja presente dans le do
    //   4. 24  do
    //   5. 28  mi   la tierce majeure, plus haut donc plus faible : c'est de la que vient sa couleur
    //   6. 31  sol
    //
    // ON S'ARRETE AU SIXIEME, et ce n'est pas une commodite : le septieme rang est FAUX - ni la, ni si bemol - et il
    // jetterait le doute sur une demonstration dont le sujet est justement que l'oreille reconnait tous les autres.
    //
    // EN MELODIE, l'une apres l'autre, et jamais empilees : un accord dirait « ces notes vont ensemble », alors que ce
    // qu'on veut montrer est que le sol ARRIVE dans le do - il y etait deja.
    static constexpr std::array<std::int32_t, 6> HARMONIC_SEMITONES{ 0, 12, 19, 24, 28, 31 };

    std::vector<domain::Note> notes;
    notes.reserve( HARMONIC_SEMITONES.size() );

    for( const std::int32_t semitones : HARMONIC_SEMITONES )
    {
        notes.emplace_back( EXERCISE_ROOT_MIDI_NUMBER + semitones );
    }

    m_notePlayer.playMelody( notes, MELODIC_GAP );
}

void CourseController::playDrone()
{
    // LE BOURDON DU JEU, et c'est la MEME formule que les questions de couleur : la tonique, et sa quinte.
    //
    // LA QUINTE OUVERTE, sans tierce : un centre qui ne colore rien lui-meme, et qui laisse donc entendre la couleur de
    // ce qu'on pose dessus. Avec une tierce, le bourdon dirait deja majeur ou mineur, et le chapitre des modes
    // n'aurait plus rien a faire entendre.
    //
    // Meme note de depart que partout ailleurs dans les cours (le do du milieu), pour que deux lecons ne fassent pas
    // entendre deux centres differents sans le dire.
    static constexpr std::chrono::milliseconds DRONE_DURATION{ 5000 };

    const std::array<domain::Note, 2> drone{ domain::Note{ EXERCISE_ROOT_MIDI_NUMBER },
                                             domain::Note{ EXERCISE_ROOT_MIDI_NUMBER + FIFTH_IN_SEMITONES } };

    m_notePlayer.playChordFor( drone, DRONE_DURATION );
}

void CourseController::playFifthCycle( int p_fifthCount )
{
    // LE CERCLE DES QUINTES, ENTENDU : on monte de quinte en quinte, et CHAQUE NOTE EST RAMENEE dans l'octave de depart.
    //
    // Ce repli est tout : sans lui, la douzieme quinte serait sept octaves plus haut et l'oreille n'entendrait qu'une
    // fusee. Repliee, la suite revient a son point de depart - et c'est LA, exactement la, que le cercle se dessine.
    //
    // Sept quintes montrent d'ou vient une gamme ; douze font le tour complet. C'est le meme geste, arrete plus tot.
    if( p_fifthCount <= 0 )
    {
        return;
    }

    const auto count = static_cast<std::size_t>( p_fifthCount );

    std::vector<domain::Note> cycle;
    cycle.reserve( count );

    for( std::size_t index = 0; index < count; ++index )
    {
        const auto steps = static_cast<std::int32_t>( index ) * FIFTH_IN_SEMITONES;

        cycle.emplace_back( EXERCISE_ROOT_MIDI_NUMBER + ( steps % domain::SEMITONES_PER_OCTAVE ) );
    }

    m_notePlayer.playMelody( cycle, MELODIC_GAP );
}

void CourseController::playModeScale( int p_modeIndex )
{
    // LA GAMME D'UN MODE, SUR LE BOURDON - exactement ce que fait le banc d'essai des modes.
    //
    // LE BOURDON EST CE QUI REND LA COULEUR AUDIBLE : sans lui, sept notes sont sept notes ; avec lui, elles sont une
    // COULEUR. Et c'est la meme formule que partout ailleurs - la tonique et sa quinte, sans tierce.
    if( ( p_modeIndex < 0 ) || std::cmp_greater_equal( p_modeIndex, domain::MODE_COUNT ) )
    {
        return;
    }

    const auto mode = static_cast<domain::Mode>( p_modeIndex );

    const std::vector<domain::Note> scale =
      domain::modeScaleUpAndDown( domain::Note{ EXERCISE_ROOT_MIDI_NUMBER }, mode );

    const std::array<domain::Note, 2> drone{ domain::Note{ EXERCISE_ROOT_MIDI_NUMBER },
                                             domain::Note{ EXERCISE_ROOT_MIDI_NUMBER + FIFTH_IN_SEMITONES } };

    // Meme articulation que le banc d'essai : une demi-seconde par pas, et un silence plus court que la note. Un cours
    // qui articulerait autrement ferait entendre une autre musique que celle du banc.
    m_notePlayer.playMelodyOverDrone(
      scale, drone, std::chrono::milliseconds{ 500 }, std::chrono::milliseconds{ 80 } );
}

void CourseController::stopPlayback()
{
    m_notePlayer.stopAll();
}

}    // namespace musichien::ui
