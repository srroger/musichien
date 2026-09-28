#include "ui/ExerciseSessionController.h"

#include "domain/exercise/Weekend.h"
#include "domain/music/Interval.h"
#include "domain/music/Temperament.h"
#include "domain/rhythm/RhythmPattern.h"
#include "ui/IntervalDescription.h"
#include "ui/MicrophoneController.h"

#include <QDate>
#include <QString>

#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <utility>

namespace musichien::ui
{

namespace
{

// What lives() returns when the session puts no limit on mistakes.
//
// A sentinel rather than zero, because zero lives IS a state - it means the session is over - and the
// screen has to be able to tell the two apart.
constexpr int NO_LIFE_LIMIT = -1;

// L'ecart entre deux notes d'un arpege. Assez lent pour que l'oreille entende chaque note, assez court pour qu'on
// reconnaisse encore l'accord d'ou elles viennent.
constexpr std::chrono::milliseconds ARPEGGIO_NOTE_GAP{ 450 };

// Combien de questions FACILES ouvrent un bilan : assez pour se mettre en confiance, pas assez pour lasser.
constexpr std::size_t REVIEW_EASY_QUESTION_COUNT = 3;

// La periode qu'un bilan regarde. Trente jours : ce que le joueur a travaille recemment, et non sa vie entiere - un
// exercice rate il y a six mois n'est plus une faiblesse, c'est un souvenir.
constexpr int REVIEW_PERIOD_DAYS = 30;

// La qualite d'une frappe, dans la convention de l'ecran : 0 = Miss, 1 = Good, 2 = Perfect.
//
// La page Rythme parle deja cette langue, et la question de rythme doit parler la MEME : deux ecrans qui
// nommeraient differemment la meme chose obligeraient le QML a connaitre deux traductions pour un seul verdict.
[[nodiscard]] int screenQualityOf( domain::HitQuality p_quality ) noexcept
{
    switch( p_quality )
    {
        case domain::HitQuality::Perfect:
            return 2;

        case domain::HitQuality::Good:
            return 1;

        case domain::HitQuality::Miss:
            return 0;
    }

    return 0;
}

// La cellule d'une question, avec le meme garde-fou que le domaine : un index venu du domaine est valide, et une
// liste qui changerait un jour ne doit pas transformer une question en exception.
[[nodiscard]] const domain::RhythmPattern & patternOf( const domain::Question & p_question ) noexcept
{
    const std::vector<domain::RhythmPattern> & patterns = domain::allRhythmPatterns();

    const std::size_t index = std::min( p_question.patternIndex, patterns.size() - 1 );

    return patterns.at( index );
}

// Un accord, decrit pour l'ecran : sa couleur, son nom, son symbole ("Cm") et le nom de sa tonique.
//
// L'ecran AFFICHE, il n'assemble rien : un symbole compose dans le QML serait compose autrement le jour ou un deuxieme
// ecran le montrerait, et les deux divergeraient en silence.
[[nodiscard]] QVariantMap describeChord( const domain::Chord & p_chord )
{
    const domain::Note root{ p_chord.rootMidiNumber };

    const std::string rootName = root.pitchClassName();

    const std::string_view suffix = domain::chordQualitySymbolSuffix( p_chord.quality );

    QVariantMap described;

    described.insert( QStringLiteral( "quality" ), static_cast<int>( p_chord.quality ) );

    described.insert( QStringLiteral( "name" ),
                      QString::fromStdString( std::string{ domain::chordQualityName( p_chord.quality ) } ) );

    described.insert( QStringLiteral( "rootName" ), QString::fromStdString( rootName ) );

    // La tonique et son suffixe, colles : "C" et "m" font "Cm", et "C" tout seul est deja un do majeur.
    described.insert( QStringLiteral( "symbol" ), QString::fromStdString( rootName + std::string{ suffix } ) );

    described.insert( QStringLiteral( "noteCount" ), static_cast<int>( domain::chordNoteCount( p_chord.quality ) ) );

    return described;
}

}    // namespace

ExerciseSessionController::ExerciseSessionController( domain::NotePlayer & p_notePlayer,
                                                      domain::SessionSettings p_settings,
                                                      domain::HintBook p_hintBook,
                                                      domain::AnecdoteBook p_anecdoteBook,
                                                      VibrationCallback p_vibrate,
                                                      domain::PlayerPreferences * p_levelStore,
                                                      QObject * p_parent )
  : QObject{ p_parent }
  , m_notePlayer{ p_notePlayer }
  , m_settings{ p_settings }
  , m_hintBook{ std::move( p_hintBook ) }
  , m_anecdoteBook{ std::move( p_anecdoteBook ) }
  , m_vibrate{ std::move( p_vibrate ) }
  , m_levelStore{ p_levelStore }
{
    // What the application remembers about the player is read once, here, and it OVERRIDES the settings the
    // caller passed: a level the player has chosen is a decision, and a decision beats a default.
    if( m_levelStore != nullptr )
    {
        m_playerLevel = m_levelStore->storedLevel();
    }

    if( m_playerLevel.has_value() )
    {
        m_settings = sessionSettingsForLevel( *m_playerLevel );
    }

    // Les parts de question, telles que le profil s'en souvient : elles priment sur les reglages passes au
    // constructeur, exactement comme le niveau a prime sur eux juste avant.
    applyStoredQuestionShares( m_settings );

    // The instruments the player asked for. An empty list is a first run: everything is offered, which is what a
    // fresh installation should sound like.
    if( m_levelStore != nullptr )
    {
        m_enabledInstruments = m_levelStore->storedEnabledInstruments();
    }

    m_enabledInstruments.resize( domain::INSTRUMENT_COUNT, true );

    // La boucle de rythme. Le timer le plus precis que Qt offre, comme la page Rythme : le clic doit tomber ou
    // l'oreille l'attend, et un timer grossier fait tituber toute une mesure.
    m_rhythmTimer.setTimerType( Qt::PreciseTimer );

    QObject::connect( &m_rhythmTimer, &QTimer::timeout, this, &ExerciseSessionController::onRhythmBeat );
}

bool ExerciseSessionController::running() const noexcept
{
    return m_session != nullptr;
}

int ExerciseSessionController::questionNumber() const noexcept
{
    return ( m_session != nullptr ) ? static_cast<int>( m_session->questionNumber() ) : 0;
}

int ExerciseSessionController::questionCount() const noexcept
{
    if( m_session == nullptr )
    {
        return 0;
    }

    // Le mode infini met le nombre de questions a la limite de size_t : le convertir en int le fait retomber sur
    // -1, que l'ecran afficherait tel quel. On le traduit en une sentinelle propre, que l'ecran lit comme "sans
    // fin".
    const std::size_t total = m_session->settings().questionCount;

    return ( total > static_cast<std::size_t>( std::numeric_limits<int>::max() ) ) ? -1 : static_cast<int>( total );
}

QVariantList ExerciseSessionController::gridPositions() const
{
    QVariantList positions;

    if( m_session == nullptr )
    {
        return positions;
    }

    // Les choix ont deja ete decides par la session : lesquels, et combien. Ici on ne fait que les PLACER, ce
    // qui est la seule chose qui manquait pour que la grille devienne une carte.
    const std::array<std::vector<domain::Interval>, domain::CIRCLE_OF_FIFTHS_SLOT_COUNT> layout =
      domain::layoutOnCircle( m_session->currentQuestion().choices );

    // UNE LISTE PLATE DE BOUTONS, et non douze places contenant chacune leurs intervalles.
    //
    // La difference n'est pas cosmetique : c'est ce qui rend l'ecran a la fois juste et simple. Une place porte
    // plusieurs intervalles - une seconde majeure et sa neuvieme, la meme couleur a une octave pres - et l'ecran
    // doit les dessiner empiles dans la case. Une liste imbriquee demandait deux Repeaters l'un dans l'autre, et
    // un seul Repeater ne produit rien en silence : la page se charge, aucun avertissement, aucun bouton.
    //
    // Aplati, chaque bouton sait tout ce qu'il lui faut pour se placer lui-meme :
    //
    //   * 'slot'       : 0 a 11, la place du cercle - c'est ce qui donne l'ANGLE ;
    //   * 'stackIndex' : son rang dans la case, 0 en haut ;
    //   * 'stackSize'  : combien de boutons la case porte, pour savoir comment les repartir ;
    //   * 'isEmpty'    : une place que la palette n'a pas encore, dessinee en anneau.
    for( std::size_t slot = 0; slot < layout.size(); ++slot )
    {
        const std::vector<domain::Interval> & intervals = layout.at( slot );

        if( intervals.empty() )
        {
            QVariantMap position;
            position.insert( QStringLiteral( "slot" ), static_cast<int>( slot ) );
            position.insert( QStringLiteral( "stackIndex" ), 0 );
            position.insert( QStringLiteral( "stackSize" ), 1 );
            position.insert( QStringLiteral( "isEmpty" ), true );

            positions.append( position );

            continue;
        }

        // Du plus petit au plus grand, donc : le simple en tete de case, ses composes colles juste dessous.
        for( std::size_t rank = 0; rank < intervals.size(); ++rank )
        {
            QVariantMap position = describeInterval( intervals.at( rank ) );

            position.insert( QStringLiteral( "slot" ), static_cast<int>( slot ) );
            position.insert( QStringLiteral( "stackIndex" ), static_cast<int>( rank ) );
            position.insert( QStringLiteral( "stackSize" ), static_cast<int>( intervals.size() ) );
            position.insert( QStringLiteral( "isEmpty" ), false );

            positions.append( position );
        }
    }

    return positions;
}

QVariantList ExerciseSessionController::choices() const
{
    return m_choices;
}

bool ExerciseSessionController::isAsking() const noexcept
{
    return ( m_session != nullptr ) && ( m_session->state() == domain::SessionState::Asking );
}

bool ExerciseSessionController::isFinished() const noexcept
{
    return ( m_session != nullptr ) && m_session->isFinished();
}

bool ExerciseSessionController::isFeedbackVisible() const noexcept
{
    return ( m_session != nullptr ) && ( m_session->state() == domain::SessionState::Feedback );
}

bool ExerciseSessionController::wasLastAnswerCorrect() const noexcept
{
    return ( m_session != nullptr ) && m_session->wasLastAnswerCorrect();
}

bool ExerciseSessionController::isHelpAvailable() const noexcept
{
    return ( m_session != nullptr ) && m_session->isHelpAvailable();
}

QVariantMap ExerciseSessionController::heardInterval() const
{
    // Sur une question de rythme ou d'accord, il n'y a pas d'intervalle a decrire : la question porte bien un intervalle
    // tire au hasard - c'est l'ordre des tirages qui veut ca - mais il n'a jamais ete joue, et le montrer serait un
    // mensonge.
    if( ( m_session == nullptr ) || isRhythmQuestion() || isChordQuestion() )
    {
        return QVariantMap{};
    }

    return describeInterval( m_session->currentQuestion().target );
}

QVariantMap ExerciseSessionController::answeredInterval() const
{
    if( m_session == nullptr )
    {
        return QVariantMap{};
    }

    const std::optional<domain::Interval> answer = m_session->lastAnswer();

    // operator* rather than value(): an empty optional is what "nothing to show" means here, and it is
    // already handled.
    return answer.has_value() ? describeInterval( *answer ) : QVariantMap{};
}

QString ExerciseSessionController::hintText() const
{
    // Pas d'indice sur une question de rythme ni sur une question d'accord : les indices sont des souvenirs
    // d'INTERVALLES ("pense a Star Wars"), et le contenu n'en a ni pour une cellule ni pour une couleur d'accord.
    if( ( m_session == nullptr ) || !m_session->isHintAvailable() || isRhythmQuestion() || isChordQuestion() )
    {
        return QString{};
    }

    const domain::Question & question = m_session->currentQuestion();

    // Looked up for the interval that was ASKED, in the direction it was played: a hint is a fact about
    // the question, never about the answer, and never about the button the player pressed.
    const std::optional<domain::IntervalHint> hint = m_hintBook.hintFor( question.target,
                                                                         question.direction );

    if( !hint.has_value() )
    {
        // A gap in the content file - the seventh descending has no hint yet. Nothing is reported to
        // the player: a missing hint is simply absent, not broken.
        return QString{};
    }

    return QString::fromStdString( hint->label );
}

bool ExerciseSessionController::hasChosenLevel() const noexcept
{
    return m_playerLevel.has_value();
}

int ExerciseSessionController::playerLevel() const noexcept
{
    return m_playerLevel.has_value() ? static_cast<int>( *m_playerLevel ) : -1;
}

void ExerciseSessionController::choosePlayerLevel( int p_level )
{
    // Le bouton a repondu : un clic tres court et discret, pour que la main soit entendue.
    m_notePlayer.playTapCue();

    const domain::PlayerLevel level = domain::playerLevelFromIndex( static_cast<std::size_t>( p_level ) );

    m_playerLevel = level;

    // The rules of the session to COME. A session already running is deliberately left alone: changing the
    // difficulty under the feet of a player in the middle of ten questions is not a kindness.
    m_settings = sessionSettingsForLevel( level );

    applyStoredQuestionShares( m_settings );

    if( m_levelStore != nullptr )
    {
        m_levelStore->storeLevel( level );
    }

    emit playerLevelChanged();
}

domain::SessionSettings ExerciseSessionController::sessionSettingsForLevel( domain::PlayerLevel p_level ) const
{
    domain::SessionSettings settings = domain::sessionSettingsFor( p_level );

    // Les parts de question sont des reglages du JOUEUR, pas des consequences de sa difficulte : elles survivent au
    // changement de niveau. C'est une correction plutot qu'un detail - sans elle, quelqu'un qui avait demande des
    // questions de rythme sur mesure les perdait en changeant de niveau, et la session recommencait a en poser a sa
    // place.
    //
    // Ce que le PROFIL en dit, lui, est applique juste apres, par applyStoredQuestionShares : c'est la que ces
    // reglages sont ranges, et le profil fait foi.
    settings.singQuestionShare = m_settings.singQuestionShare;
    settings.rhythmQuestionShare = m_settings.rhythmQuestionShare;
    settings.chordQuestionShare = m_settings.chordQuestionShare;

    return settings;
}

void ExerciseSessionController::applyStoredQuestionShares( domain::SessionSettings & p_settings ) const
{
    if( m_levelStore == nullptr )
    {
        return;
    }

    p_settings.singQuestionShare = m_levelStore->storedSingQuestionShare();
    p_settings.rhythmQuestionShare = m_levelStore->storedRhythmQuestionShare();
    p_settings.chordQuestionShare = m_levelStore->storedChordQuestionShare();
}

QVariantList ExerciseSessionController::playerLevels()
{
    // The names live here, and not in the QML, for the same reason the answer grid is built here: a list that
    // exists twice drifts.
    const auto nameOfLevel = []( domain::PlayerLevel p_level ) {
        switch( p_level )
        {
            case domain::PlayerLevel::Beginner:
                return ExerciseSessionController::tr( "Je débute" );

            case domain::PlayerLevel::Fluent:
                return ExerciseSessionController::tr( "À l'aise" );

            case domain::PlayerLevel::Advanced:
                return ExerciseSessionController::tr( "Jusqu'à l'octave" );

            case domain::PlayerLevel::BeyondTheOctave:
                return ExerciseSessionController::tr( "Les composés" );

            case domain::PlayerLevel::Master:
                return ExerciseSessionController::tr( "Je maîtrise" );
        }

        return QString{};
    };

    QVariantList levels;

    for( std::size_t index = 0; index < domain::PLAYER_LEVEL_COUNT; ++index )
    {
        QVariantMap level;
        level.insert( QStringLiteral( "index" ), static_cast<int>( index ) );
        level.insert( QStringLiteral( "name" ), nameOfLevel( domain::playerLevelFromIndex( index ) ) );

        levels.append( level );
    }

    return levels;
}

QVariantList ExerciseSessionController::instruments() const
{
    QVariantList instruments;

    for( std::size_t index = 0; index < domain::INSTRUMENT_COUNT; ++index )
    {
        QVariantMap instrument;
        instrument.insert( QStringLiteral( "index" ), static_cast<int>( index ) );
        instrument.insert( QStringLiteral( "name" ), QString::fromUtf8( domain::INSTRUMENT_NAMES.at( index ) ) );

        // A list shorter than the instruments this build knows about means "everything": a first run must sound
        // complete, not empty.
        instrument.insert( QStringLiteral( "enabled" ),
                           std::cmp_less( index, m_enabledInstruments.size() ) ? m_enabledInstruments.at( index ) : true );

        instruments.append( instrument );
    }

    return instruments;
}

void ExerciseSessionController::setInstrumentEnabled( int p_index, bool p_isEnabled )
{
    // Le bouton a repondu : un clic tres court et discret, pour que la main soit entendue.
    m_notePlayer.playTapCue();

    // Two tests rather than one: comparing a signed index with an unsigned count in the same
    // expression is exactly the kind of comparison that lets a negative index through.
    if( p_index < 0 )
    {
        return;
    }

    if( std::cmp_greater_equal( p_index, domain::INSTRUMENT_COUNT ) )
    {
        return;
    }

    m_enabledInstruments.resize( domain::INSTRUMENT_COUNT, true );

    m_enabledInstruments.at( static_cast<std::size_t>( p_index ) ) = p_isEnabled;

    // The LAST one cannot be switched off: an instrument list with nothing in it is a game with no sound. The
    // signal is emitted all the same, so that a checkbox which refused to change snaps back where it belongs
    // instead of lying about the state.
    const bool anythingLeft = std::ranges::any_of( m_enabledInstruments, []( bool p_isEnabledFlag ) {
        return p_isEnabledFlag;
    } );

    if( anythingLeft )
    {
        if( m_levelStore != nullptr )
        {
            m_levelStore->storeEnabledInstruments( m_enabledInstruments );
        }
    }
    else
    {
        m_enabledInstruments.at( static_cast<std::size_t>( p_index ) ) = true;
    }

    emit instrumentsChanged();
}

int ExerciseSessionController::experience() const noexcept
{
    return ( m_session != nullptr ) ? static_cast<int>( m_session->score().experience() ) : 0;
}

int ExerciseSessionController::streak() const noexcept
{
    return ( m_session != nullptr ) ? static_cast<int>( m_session->score().streak() ) : 0;
}

int ExerciseSessionController::lives() const noexcept
{
    if( m_session == nullptr )
    {
        return NO_LIFE_LIMIT;
    }

    // operator* and not value(): value() throws when the optional is empty, and a getter of a view
    // model has no business being able to throw.
    const std::optional<std::int32_t> remainingLives = m_session->score().remainingLives();

    return remainingLives.has_value() ? static_cast<int>( *remainingLives ) : NO_LIFE_LIMIT;
}

bool ExerciseSessionController::hasUnlimitedLives() const noexcept
{
    return ( m_session == nullptr ) || !m_session->score().remainingLives().has_value();
}

bool ExerciseSessionController::starEarned() const noexcept
{
    return ( m_session != nullptr ) && m_session->hasEarnedStar();
}

void ExerciseSessionController::startSession()
{
    beginSession( m_settings );
}

void ExerciseSessionController::startInfiniteSession()
{
    domain::SessionSettings settings = m_settings;

    // Le mode infini, c'est le mode qui ne s'arrete jamais : pas de vies, pas de fin, juste enchaner. Une erreur
    // coute du rythme - la serie retombe - mais jamais la partie.
    settings.lives = std::nullopt;
    settings.questionCount = std::numeric_limits<std::size_t>::max();

    beginSession( settings );
}

void ExerciseSessionController::startSurvivalSession()
{
    domain::SessionSettings settings = m_settings;

    // Le survival, c'est l'arcade avec des vies : un nombre de questions sans fin, et la partie s'arrete quand les
    // vies tombent a zero. Les vies restent donc celles du niveau, pas un retour en arriere vers "illimite".
    settings.questionCount = std::numeric_limits<std::size_t>::max();

    beginSession( settings );
}

void ExerciseSessionController::beginSession( domain::SessionSettings p_settings )
{
    // Le bouton a repondu : un clic tres court et discret, pour que la main soit entendue.
    m_notePlayer.playTapCue();

    // The seed is drawn HERE, in the interface layer, and never inside the domain.
    //
    // That is what keeps a session reproducible from its seed in a test, and it is also why the rules
    // of the game can be replayed exactly when something goes wrong. The domain owns no entropy
    // source, on purpose.
    std::random_device entropySource;

    m_session = std::make_unique<domain::ExerciseSession>( entropySource(), p_settings );

    // The end of the previous session has been announced; this one gets its own turn.
    m_sessionEndAnnounced = false;

    emit runningChanged();

    refreshChoices();

    playCurrentQuestion();

    emit scoreChanged();
    emit sessionChanged();
}

void ExerciseSessionController::stopSession()
{
    stopRhythmLoop();

    stopPlayback();

    m_session.reset();

    m_choices.clear();

    emit runningChanged();
    emit questionChanged();
    emit sessionChanged();
    emit scoreChanged();
}

void ExerciseSessionController::replay()
{
    if( ( m_session == nullptr ) || !m_session->canReplay() )
    {
        return;
    }

    // Counted by the session, played here: the count has to follow what was really heard, and only
    // the screen knows that.
    m_session->registerReplay();

    playCurrentQuestion();

    emit scoreChanged();
    emit sessionChanged();
}

void ExerciseSessionController::answer( int p_semitones )
{
    if( ( m_session == nullptr ) || !isAsking() )
    {
        // An answer arriving twice, or after the question is over, changes nothing. The session says
        // so itself; stopping here keeps the signals from firing for nothing.
        return;
    }

    processAnswer( m_session->answer( p_semitones ) );
}

void ExerciseSessionController::answerDirection( int p_direction )
{
    if( ( m_session == nullptr ) || !isAsking() )
    {
        return;
    }

    const auto direction = static_cast<domain::IntervalDirection>( p_direction );

    processAnswer( m_session->answerDirection( direction ) );
}

void ExerciseSessionController::answerSung( bool p_isCorrect )
{
    if( ( m_session == nullptr ) || !isAsking() )
    {
        return;
    }

    processAnswer( m_session->answerSung( p_isCorrect ) );
}

void ExerciseSessionController::setMicrophoneController( MicrophoneController * p_microphone )
{
    m_microphone = p_microphone;
}

void ExerciseSessionController::setQuestionLog( domain::QuestionLog * p_questionLog )
{
    m_questionLog = p_questionLog;
}

int ExerciseSessionController::questionKind() const noexcept
{
    return ( m_session != nullptr ) ? static_cast<int>( m_session->currentQuestion().kind ) : 0;
}

bool ExerciseSessionController::isRhythmQuestion() const noexcept
{
    return ( m_session != nullptr ) && ( m_session->currentQuestion().kind == domain::QuestionKind::Rhythm );
}

QString ExerciseSessionController::rhythmPatternName() const
{
    if( !isRhythmQuestion() )
    {
        return QString{};
    }

    // Le nom vient du domaine, et l'ecran le montre tel quel : "Binaire", "Valse" sont des noms de MUSIQUE, pas des
    // identifiants techniques, et les renommer est une decision de contenu.
    const std::string_view name = patternOf( m_session->currentQuestion() ).name();

    return QString::fromUtf8( name.data(), static_cast<int>( name.size() ) );
}

int ExerciseSessionController::rhythmBpm() const noexcept
{
    return isRhythmQuestion() ? m_session->currentQuestion().bpm : 0;
}

int ExerciseSessionController::rhythmBeatsPerBar() const noexcept
{
    return isRhythmQuestion() ? patternOf( m_session->currentQuestion() ).beatsPerBar() : 0;
}

QVariantList ExerciseSessionController::rhythmHits() const
{
    QVariantList hits;

    if( !isRhythmQuestion() )
    {
        // Aucune frappe a dessiner, plutot que les frappes d'une autre question : une liste vide ne dessine rien, et
        // rien est exactement ce qu'il faut montrer sur une question d'intervalle.
        return hits;
    }

    for( const domain::RhythmHit & hit : patternOf( m_session->currentQuestion() ).hits() )
    {
        QVariantMap described;

        // Ou la frappe tombe, en TEMPS depuis le debut de la boucle : c'est le domaine qui le dit, et l'ecran se
        // contente de placer le trait sur la mesure.
        described.insert( QStringLiteral( "beat" ), hit.beat );
        described.insert( QStringLiteral( "accented" ), hit.accented );
        described.insert( QStringLiteral( "drum" ), static_cast<int>( hit.drum ) );

        hits.append( described );
    }

    return hits;
}

int ExerciseSessionController::rhythmBeatInBar() const noexcept
{
    return m_rhythmBeatInBar;
}

bool ExerciseSessionController::isRhythmPlaying() const noexcept
{
    return m_rhythmIsPlaying;
}

int ExerciseSessionController::rhythmLastQuality() const noexcept
{
    return m_rhythmLastQuality;
}

int ExerciseSessionController::rhythmCoveredOnsets() const noexcept
{
    return m_rhythmCoveredOnsets;
}

int ExerciseSessionController::rhythmOnsetCount() const noexcept
{
    return isRhythmQuestion() ? static_cast<int>( m_session->currentQuestion().coveredOnsets.size() ) : 0;
}

int ExerciseSessionController::rhythmCellDurationMs() const noexcept
{
    if( !isRhythmQuestion() )
    {
        return 0;
    }

    // Une mesure : c'est ce que dure l'ecoute, la reproduction, et la confirmation. L'ecran s'en sert pour laisser au
    // feedback le temps de s'entendre - une pause plus courte couperait la cellule en plein milieu.
    const double beatMs = domain::beatDurationMs( static_cast<double>( m_session->currentQuestion().bpm ) );

    return static_cast<int>( std::lround( beatMs * static_cast<double>( rhythmBeatsPerBar() ) ) );
}

bool ExerciseSessionController::isChordQuestion() const noexcept
{
    return ( m_session != nullptr ) && ( m_session->currentQuestion().kind == domain::QuestionKind::Chord );
}

QVariantMap ExerciseSessionController::heardChord() const
{
    if( !isChordQuestion() )
    {
        return QVariantMap{};
    }

    return describeChord( m_session->currentQuestion().chord );
}

QVariantMap ExerciseSessionController::answeredChord() const
{
    if( ( m_session == nullptr ) || !isChordQuestion() )
    {
        return QVariantMap{};
    }

    const std::optional<domain::ChordQuality> answer = m_session->lastChordAnswer();

    // operator* plutot que value() : un optional vide est ce que "rien a montrer" veut dire ici, et c'est deja gere.
    if( !answer.has_value() )
    {
        return QVariantMap{};
    }

    // Le symbole se construit sur la tonique de la QUESTION : le joueur a repondu une couleur, pas une tonique, et le
    // verdict doit opposer deux couleurs sur la meme note.
    return describeChord( domain::Chord{ .quality = *answer,
                                         .rootMidiNumber = m_session->currentQuestion().chord.rootMidiNumber } );
}

QVariantList ExerciseSessionController::chordChoices() const
{
    QVariantList choices;

    if( ( m_session == nullptr ) || !isChordQuestion() )
    {
        return choices;
    }

    // LE NOM DE LA TONIQUE, une fois pour tous les boutons : « C », « F# », « Bb ».
    //
    // Elle vient du DOMAINE (Note::pitchClassName), comme tous les autres noms de l'application - l'ecran ne nomme rien
    // lui-meme. Et la tonique est celle de la QUESTION : elle ne change pas d'un bouton a l'autre, donc seul le suffixe
    // distingue les couleurs.
    const QString rootName =
      QString::fromStdString( domain::Note{ m_session->currentQuestion().chord.rootMidiNumber }.pitchClassName() );

    for( const domain::ChordQuality quality : m_session->currentQuestion().chordChoices )
    {
        QVariantMap described;

        const std::string_view suffix = domain::chordQualitySymbolSuffix( quality );

        described.insert( QStringLiteral( "quality" ), static_cast<int>( quality ) );

        // LE NOM DE L'ACCORD, en notation anglo-saxonne : « C », « Cm », « Csus4 », « C7 », « Cm7b5 ».
        //
        // C'est la notation des recueils et des vraies partitions, et Roger l'a demandee apres avoir vu « Maj · 3 » a
        // l'ecran - « ce qui est tres bizarre comme notation ». Un musicien lit ca sans y penser, et le nombre de notes a
        // disparu : il n'apprenait rien qu'un musicien ne sache deja.
        described.insert( QStringLiteral( "name" ),
                          rootName + QString::fromUtf8( suffix.data(), static_cast<int>( suffix.size() ) ) );

        // Le nom complet (« Half-diminished ») reste disponible : c'est lui que le verdict ecrit, la ou il y a la place.
        described.insert( QStringLiteral( "fullName" ),
                          QString::fromUtf8( domain::chordQualityName( quality ).data(),
                                             static_cast<int>( domain::chordQualityName( quality ).size() ) ) );

        choices.append( described );
    }

    return choices;
}

void ExerciseSessionController::answerChord( int p_quality )
{
    if( ( m_session == nullptr ) || !isAsking() )
    {
        // Une reponse arrivant deux fois, ou apres la question, ne change rien : la session le dit elle-meme, et
        // s'arreter ici evite d'emettre des signaux pour rien.
        return;
    }

    if( p_quality < 0 || std::cmp_greater_equal( p_quality, domain::CHORD_QUALITY_COUNT ) )
    {
        // Un index hors liste vient d'un ecran qui invente. Il est refuse plutot que converti en une couleur qui
        // n'existe pas.
        return;
    }

    processAnswer( m_session->answerChord( static_cast<domain::ChordQuality>( p_quality ) ) );
}

void ExerciseSessionController::processAnswer( bool p_isCorrect )
{
    // Always, and not only when the question is over: a wrong answer closes the grid in, and the
    // screen must show the grid that exists rather than the one it had a moment ago.
    refreshChoices();

    // Le rythme a deja repondu tout seul : sa cellule est en train de sonner - l'ecoute qui reprend apres un rate, ou
    // la confirmation apres une reussite. Le chemin commun des notes doit donc se taire, sinon deux sons se marcheraient
    // dessus, et c'est la cellule qu'on n'entendrait plus.
    const bool isRhythm = isRhythmQuestion();

    if( p_isCorrect )
    {
        if( !isRhythm )
        {
            // A correct answer is heard again as a CHORD: the two notes together, one block instead of two,
            // which is twice as short - and a genuinely different listen of the same interval, the colour
            // without the melody. Roger asked for it to stop the success from dragging, and the reason to
            // keep it is musical: hearing the interval both ways is what seals it.
            playCurrentQuestionAsChord();
        }
    }
    else
    {
        if( !isRhythm )
        {
            // A wrong answer is heard again IMMEDIATELY, and in its original form: there is something to
            // catch up on, and the melody is what gives the second note its meaning.
            playCurrentQuestion();
        }

        // And it is announced, so that the screen can answer with its BODY - the shake, and the
        // vibration on a device that has a motor. The controller knows what happened; how it should feel
        // is not its job.
        emit wrongAnswerGiven();

        if( !isRhythm )
        {
            // The cue, then the buzz: both are mistakes being made audible and tangible, and neither is the
            // interval the player is being asked to name. See NotePlayer::playMistakeCue for why that
            // distinction matters.
            //
            // Sur une question de rythme, le cue est TU : il couvrirait la cellule, qui est justement ce qu'il faut
            // reentendre. La secousse et la vibration, elles, ne sonnent pas, et restent.
            m_notePlayer.playMistakeCue();
        }

        if( m_vibrate )
        {
            m_vibrate();
        }
    }

    // La question est CONCLUE quand elle est juste, ou quand la session s'arrete sur une derniere vie. Une mauvaise
    // reponse qui laisse la question posee n'est PAS une question conclue : elle se retente, et seule la tentative
    // finale merite une ligne - sinon le journal compterait trois fois la meme question pour une seule lecon.
    if( p_isCorrect || m_session->isFinished() )
    {
        recordCurrentQuestion( p_isCorrect, false );
    }

    announceSessionEndIfNeeded();

    emit scoreChanged();
    emit sessionChanged();
}

void ExerciseSessionController::revealAnswer()
{
    if( ( m_session == nullptr ) || !isAsking() )
    {
        return;
    }

    m_session->revealAnswer();

    // Vue la reponse, et c'est une AIDE : le journal doit le dire ainsi. Un joueur qui demande la reponse n'est pas un
    // joueur qui se trompe, et ses statistiques lui mentiraient s'il y apparaissait comme tel.
    recordCurrentQuestion( false, true );

    refreshChoices();

    if( isRhythmQuestion() )
    {
        // Passer une question de rythme : la cellule est rejouee UNE fois, pour que le joueur entende ce qu'il aurait
        // du reproduire, et la boucle s'arrete - c'est le meme son que la confirmation, et la meme raison.
        m_rhythmCoveredOnsets = coveredOnsetCount();

        stopRhythmLoop();

        playRhythmModelOnce();
    }
    else
    {
        playCurrentQuestion();
    }

    emit scoreChanged();
    emit sessionChanged();
}

void ExerciseSessionController::continueToNextQuestion()
{
    if( ( m_session == nullptr ) || ( m_session->state() != domain::SessionState::Feedback ) )
    {
        return;
    }

    m_session->advance();

    refreshChoices();

    if( !m_session->isFinished() )
    {
        playCurrentQuestion();
    }
    else
    {
        // La session s'arrete : rien ne doit continuer a battre derriere l'ecran de fin.
        stopRhythmLoop();

        persistSessionOutcome();
    }

    announceSessionEndIfNeeded();

    emit scoreChanged();
    emit sessionChanged();
}

int ExerciseSessionController::rank() const noexcept
{
    return ( m_session != nullptr ) ? static_cast<int>( domain::rankForStreak( m_session->score().streak() ) ) : 0;
}

QString ExerciseSessionController::rankLabel() const
{
    const domain::Rank rank = ( m_session != nullptr ) ? domain::rankForStreak( m_session->score().streak() )
                                                       : domain::Rank::D;

    switch( rank )
    {
        case domain::Rank::SSS:
            return QStringLiteral( "SSS" );
        case domain::Rank::SS:
            return QStringLiteral( "SS" );
        case domain::Rank::S:
            return QStringLiteral( "S" );
        case domain::Rank::A:
            return QStringLiteral( "A" );
        case domain::Rank::B:
            return QStringLiteral( "B" );
        case domain::Rank::C:
            return QStringLiteral( "C" );
        case domain::Rank::D:
            return QStringLiteral( "D" );
    }

    return QStringLiteral( "D" );
}

QString ExerciseSessionController::anecdoteText() const
{
    return m_anecdoteText;
}

void ExerciseSessionController::refreshAnecdote()
{
    const std::optional<domain::Anecdote> anecdote = m_anecdoteBook.random( m_anecdoteRandomEngine );

    m_anecdoteText = anecdote.has_value() ? QString::fromStdString( anecdote->text ) : QString{};

    emit anecdoteChanged();
}

void ExerciseSessionController::announceSessionEndIfNeeded()
{
    if( m_sessionEndAnnounced || !isFinished() )
    {
        return;
    }

    m_sessionEndAnnounced = true;

    // On entre dans une session sur une anecdote et on en sort sur une autre : le joueur a appris quelque chose,
    // meme quand la partie s'arrete la. Un seul tirage, garanti par le drapeau.
    refreshAnecdote();
}

void ExerciseSessionController::stopPlayback()
{
    // La boucle de rythme est un son comme un autre : quitter l'ecran doit la faire taire, sinon elle continue de
    // battre derriere le banc d'essai.
    stopRhythmLoop();

    m_notePlayer.stopAll();
}

QString ExerciseSessionController::playerName() const
{
    return ( m_levelStore != nullptr ) ? QString::fromStdString( m_levelStore->playerName() ) : QString{};
}

void ExerciseSessionController::setPlayerName( const QString & p_name )
{
    if( m_levelStore == nullptr )
    {
        return;
    }

    m_levelStore->storePlayerName( p_name.toStdString() );

    emit playerNameChanged();
}

int ExerciseSessionController::totalExperience() const
{
    return ( m_levelStore != nullptr ) ? static_cast<int>( m_levelStore->totalExperience() ) : 0;
}

void ExerciseSessionController::persistSessionOutcome()
{
    if( ( m_levelStore == nullptr ) || ( m_session == nullptr ) )
    {
        return;
    }

    // The session is over: its experience, its count and its star become part of the profile, once. Calling this
    // twice would count the same session twice, so it happens only from the transition into "finished".
    m_levelStore->storeTotalExperience( m_levelStore->totalExperience() + m_session->score().experience() );
    m_levelStore->storeSessionCount( m_levelStore->sessionCount() + 1 );

    if( m_session->hasEarnedStar() )
    {
        m_levelStore->storeStarCount( m_levelStore->starCount() + 1 );
    }

    emit totalExperienceChanged();
}

int ExerciseSessionController::sessionCount() const
{
    return ( m_levelStore != nullptr ) ? static_cast<int>( m_levelStore->sessionCount() ) : 0;
}

int ExerciseSessionController::starCount() const
{
    return ( m_levelStore != nullptr ) ? static_cast<int>( m_levelStore->starCount() ) : 0;
}

bool ExerciseSessionController::dailyReminderEnabled() const
{
    return ( m_levelStore != nullptr ) && m_levelStore->dailyReminderEnabled();
}

void ExerciseSessionController::setDailyReminderEnabled( bool p_enabled )
{
    if( m_levelStore == nullptr )
    {
        return;
    }

    m_levelStore->storeDailyReminderEnabled( p_enabled );

    emit dailyReminderChanged();
}

int ExerciseSessionController::temperament() const
{
    return ( m_levelStore != nullptr ) ? static_cast<int>( m_levelStore->storedTemperament() ) : 0;
}

void ExerciseSessionController::setTemperament( int p_index )
{
    if( m_levelStore == nullptr )
    {
        return;
    }

    if( p_index < 0 )
    {
        return;
    }

    if( static_cast<std::size_t>( p_index ) >= domain::TEMPERAMENT_NAMES.size() )
    {
        return;
    }

    m_levelStore->storeTemperament( static_cast<domain::Temperament>( p_index ) );

    emit temperamentChanged();
}

// A Q_PROPERTY READ must be a member function, even when it reads nothing from the object.
QVariantList ExerciseSessionController::temperaments() const    // NOLINT(readability-convert-member-functions-to-static)
{
    QVariantList names;

    for( const std::string_view name : domain::TEMPERAMENT_NAMES )
    {
        names.append( QString::fromStdString( std::string{ name } ) );
    }

    return names;
}

int ExerciseSessionController::tuningRoot() const
{
    return ( m_levelStore != nullptr ) ? m_levelStore->storedTuningRoot().pitchClassIndex() : 0;
}

void ExerciseSessionController::setTuningRoot( int p_index )
{
    if( m_levelStore == nullptr )
    {
        return;
    }

    if( p_index < 0 || p_index >= domain::SEMITONES_PER_OCTAVE )
    {
        return;
    }

    // A root is a PITCH CLASS: which octave it sits in does not change an interval, so it is stored in octave 4.
    m_levelStore->storeTuningRoot( domain::Note{ 60 + p_index } );

    emit tuningRootChanged();
}

QVariantList ExerciseSessionController::tuningRoots() const    // NOLINT(readability-convert-member-functions-to-static)
{
    QVariantList names;

    for( std::int32_t index = 0; index < domain::SEMITONES_PER_OCTAVE; ++index )
    {
        names.append( QString::fromStdString( domain::Note{ 60 + index }.name() ) );
    }

    return names;
}

double ExerciseSessionController::referencePitch() const
{
    return ( m_levelStore != nullptr ) ? m_levelStore->storedReferencePitch() : 440.0;
}

void ExerciseSessionController::setReferencePitch( double p_hertz )
{
    if( m_levelStore == nullptr )
    {
        return;
    }

    // A diapason outside this band is a typo, not a tuning: it is refused rather than stored.
    if( p_hertz < 400.0 || p_hertz > 480.0 )
    {
        return;
    }

    m_levelStore->storeReferencePitch( p_hertz );

    emit referencePitchChanged();
}

int ExerciseSessionController::singQuestionShare() const
{
    return ( m_levelStore != nullptr ) ? m_levelStore->storedSingQuestionShare() : 20;
}

void ExerciseSessionController::setSingQuestionShare( int p_share )
{
    if( m_levelStore == nullptr )
    {
        return;
    }

    // A share is a percentage: outside the range it is a typo, not a setting.
    if( p_share < 0 || p_share > 100 )
    {
        return;
    }

    m_levelStore->storeSingQuestionShare( p_share );

    // The NEXT session takes the new share; a session already running keeps its own rules.
    m_settings.singQuestionShare = p_share;

    emit singQuestionShareChanged();
}

int ExerciseSessionController::rhythmQuestionShare() const
{
    // ZERO quand il n'y a pas de profil : le defaut de l'application, celui du domaine et celui du fichier de
    // reglages disent tous les trois la meme chose, sinon l'un des trois finirait par mentir a l'ecran.
    return ( m_levelStore != nullptr ) ? m_levelStore->storedRhythmQuestionShare() : 0;
}

void ExerciseSessionController::setRhythmQuestionShare( int p_share )
{
    if( m_levelStore == nullptr )
    {
        return;
    }

    if( p_share < 0 || p_share > 100 )
    {
        return;
    }

    m_levelStore->storeRhythmQuestionShare( p_share );

    // La session SUIVANTE prend la nouvelle part ; une session en cours garde ses regles. Meme contrat que le chant,
    // et c'est ce qui rend le reglage sur : rien ne change sous les pieds du joueur au milieu d'une partie.
    m_settings.rhythmQuestionShare = p_share;

    emit rhythmQuestionShareChanged();
}

int ExerciseSessionController::chordQuestionShare() const
{
    return ( m_levelStore != nullptr ) ? m_levelStore->storedChordQuestionShare() : 20;
}

void ExerciseSessionController::setChordQuestionShare( int p_share )
{
    if( m_levelStore == nullptr )
    {
        return;
    }

    if( p_share < 0 || p_share > 100 )
    {
        return;
    }

    m_levelStore->storeChordQuestionShare( p_share );

    m_settings.chordQuestionShare = p_share;

    emit chordQuestionShareChanged();
}

void ExerciseSessionController::listenToTarget()
{
    if( m_session == nullptr )
    {
        return;
    }

    // For a non-beginner, hearing the target first is help: it counts as a replay, and a replay reduces the
    // experience earned. A beginner hears the target automatically, without penalty.
    if( !isBeginner() )
    {
        m_session->registerReplay();
    }

    if( m_microphone != nullptr )
    {
        m_microphone->playSingingTarget();
    }
}

void ExerciseSessionController::resetProfile()
{
    if( m_levelStore == nullptr )
    {
        return;
    }

    // The score is wiped: experience, sessions and stars back to zero. The name and the level stay - they are
    // choices, not a score.
    m_levelStore->storeTotalExperience( 0 );
    m_levelStore->storeSessionCount( 0 );
    m_levelStore->storeStarCount( 0 );

    // ET LES STATISTIQUES AUSSI. Roger : « n'oublie pas que "Remise a zero" met a zero les statistiques aussi. » Un
    // score remis a zero qui garderait son journal serait un demi-mensonge : la page de statistiques continuerait de
    // raconter une histoire que le joueur vient effacer.
    if( m_questionLog != nullptr )
    {
        m_questionLog->clear();
    }

    // Et un bilan en cours n'a plus de plan a suivre : la remise a zero le referme.
    m_isReviewRunning = false;
    m_reviewEasyQuestionCount = 0;

    emit totalExperienceChanged();
    emit sessionChanged();
    emit statisticsChanged();
}

bool ExerciseSessionController::isBeginner() const noexcept
{
    return !m_playerLevel.has_value() || ( *m_playerLevel == domain::PlayerLevel::Beginner );
}

int ExerciseSessionController::reminderHour() const noexcept
{
    // Le profil porte l'heure ; sans profil - un test - on rend la meme valeur par defaut que le domaine, sinon l'un des
    // deux finirait par mentir a l'ecran.
    return ( m_levelStore != nullptr ) ? m_levelStore->storedReminderMoment().hour : 19;
}

int ExerciseSessionController::reminderMinute() const noexcept
{
    return ( m_levelStore != nullptr ) ? m_levelStore->storedReminderMoment().minute : 0;
}

void ExerciseSessionController::setReminderHour( int p_hour )
{
    if( m_levelStore == nullptr )
    {
        return;
    }

    // On RAMENE l'heure dans une journee au lieu de la refuser, et le bornage vit dans le DOMAINE : c'est une regle, pas
    // une precaution d'ecran. Un ecran qui se tromperait ne doit pas priver le joueur de son rappel.
    const std::int32_t minute = m_levelStore->storedReminderMoment().minute;

    m_levelStore->storeReminderMoment( domain::clampedReminderMoment( domain::ReminderMoment{ p_hour, minute } ) );

    // Le signal est ce qui fait REPROGRAMMER l'application : une heure changee qui n'atteindrait pas les alarmes serait
    // un reglage qui ne fait rien.
    emit dailyReminderChanged();
}

void ExerciseSessionController::setReminderMinute( int p_minute )
{
    if( m_levelStore == nullptr )
    {
        return;
    }

    const std::int32_t hour = m_levelStore->storedReminderMoment().hour;

    m_levelStore->storeReminderMoment( domain::clampedReminderMoment( domain::ReminderMoment{ hour, p_minute } ) );

    emit dailyReminderChanged();
}

void ExerciseSessionController::testReminder()
{
    emit testReminderRequested();
}

void ExerciseSessionController::refreshChoices()
{
    if( m_session == nullptr )
    {
        return;
    }

    // Pour une question CHANTEE, la cible du micro doit etre celle de la session : on la lui confie a chaque
    // nouvelle question. Sans cette synchronisation, l'accordeur jugerait contre sa propre cible.
    if( ( m_microphone != nullptr ) && ( m_session->currentQuestion().kind == domain::QuestionKind::Sing ) )
    {
        m_microphone->setSingingTarget( m_session->currentQuestion().target.semitones() );
    }

    QVariantList choices;

    for( const domain::Interval & choice : m_session->currentQuestion().choices )
    {
        choices.append( describeInterval( choice ) );
    }

    m_choices = std::move( choices );

    emit questionChanged();
}

// ---------------------------------------------------------------------------------------------------------------------
// La boucle de rythme
//
// Deux mesures, et elles ne font pas la meme chose : la premiere fait ECOUTER la cellule, la seconde la fait
// REPRODUIRE. Puis on recommence, jusqu'a ce que la cellule soit juste - c'est tout l'exercice, et il ne demande au
// joueur aucun geste en dehors de ses frappes.
// ---------------------------------------------------------------------------------------------------------------------

void ExerciseSessionController::startRhythmQuestion()
{
    if( !isRhythmQuestion() )
    {
        return;
    }

    // Rien n'a encore ete frappe sur cette question : -1, et non 0, qui est deja un Miss.
    m_rhythmLastQuality = NO_RHYTHM_TAP;
    m_rhythmCoveredOnsets = 0;

    beginRhythmListening();

    const double beatMs = domain::beatDurationMs( static_cast<double>( m_session->currentQuestion().bpm ) );

    if( beatMs <= 0.0 )
    {
        // Un tempo nul est un reglage fautif, pas une question a jouer : la question reste posee, la boucle ne bat
        // simplement pas. Le domaine refuse les frappes pour la meme raison, donc les deux disent la meme chose.
        emit rhythmStateChanged();

        return;
    }

    m_rhythmTimer.start( static_cast<int>( std::lround( beatMs ) ) );

    emit rhythmStateChanged();
}

void ExerciseSessionController::beginRhythmListening()
{
    m_rhythmIsPlaying = false;
    m_rhythmBeatInBar = 0;

    // L'horloge repart ici : c'est le premier temps de la mesure d'ecoute. La reproduction repartira la sienne, parce
    // que la position d'une frappe se compte depuis le premier temps de la CELLULE, pas depuis le debut de la question.
    m_rhythmClock.restart();

    // Le premier temps sonne tout de suite : une mesure qui attendrait un battement de timer pour commencer decalerait
    // tout ce qui suit.
    playRhythmHitsForBeat( 0 );
}

void ExerciseSessionController::beginRhythmPlaying()
{
    m_rhythmIsPlaying = true;
    m_rhythmBeatInBar = 0;
    m_rhythmClock.restart();

    // La tentative commence : l'ardoise affichee repart de zero, et la derniere frappe avec elle.
    //
    // ICI plutot qu'a l'ecoute, et c'est ce qui laisse au joueur le temps de lire son "trois sur quatre" pendant que
    // la cellule se rejoue apres un rate - le temps de comprendre ce qui a manque.
    m_rhythmCoveredOnsets = 0;
    m_rhythmLastQuality = NO_RHYTHM_TAP;

    // Le clic du premier temps, tout de suite : c'est le repere que le joueur attend, et le retarder d'un battement
    // decalerait toute la mesure.
    m_notePlayer.playMetronomeClick( true );
}

void ExerciseSessionController::finishRhythmLoop()
{
    if( m_session == nullptr )
    {
        return;
    }

    // Le detail de la tentative est lu AVANT le verdict : le domaine remet son ardoise a zero en jugeant, et l'ecran a
    // besoin de savoir combien de frappes sur combien ont ete touchees pour pouvoir le dire.
    m_rhythmCoveredOnsets = coveredOnsetCount();

    const bool isCorrect = m_session->endRhythmLoop();

    if( isCorrect )
    {
        // La question est close : la cellule est rejouee UNE fois en confirmation, et la boucle s'arrete. La suite
        // appartient a la pause de l'ecran, qui a la duree de la cellule pour laisser le son se finir.
        stopRhythmLoop();

        playRhythmModelOnce();
    }
    else
    {
        // La question reste posee, et l'ecoute qui recommence EST le feedback : le joueur reentend ce qu'il n'a pas su
        // reproduire, puis il retente. Aucun bouton a presser entre deux tentatives - c'est ce qui fait qu'un exercice
        // de rythme se JOUE au lieu de se lire.
        beginRhythmListening();
    }

    // Le score, les vies et le verdict passent par le chemin commun a toutes les questions.
    processAnswer( isCorrect );

    emit rhythmStateChanged();
}

void ExerciseSessionController::stopRhythmLoop()
{
    m_rhythmTimer.stop();

    m_rhythmIsPlaying = false;
    m_rhythmBeatInBar = 0;
}

void ExerciseSessionController::onRhythmBeat()
{
    if( !isRhythmQuestion() || ( m_session->state() != domain::SessionState::Asking ) )
    {
        // La question a change, ou elle a sa reponse : la boucle s'arrete d'elle-meme plutot que de continuer a battre
        // sous une autre question. C'est le filet de securite de toute la mecanique.
        stopRhythmLoop();

        emit rhythmStateChanged();

        return;
    }

    const int beatsPerBar = rhythmBeatsPerBar();

    if( beatsPerBar <= 0 )
    {
        stopRhythmLoop();

        emit rhythmStateChanged();

        return;
    }

    // Ce qui sonne sur ce temps : la cellule pendant l'ecoute, la pulsation pendant la reproduction.
    //
    // Le clic est MUET pendant l'ecoute, et c'est un choix : le modele doit s'entendre seul, sinon une syncope se noie
    // sous le metronome. Pendant la reproduction, au contraire, le clic est la seule reference - la cellule ne s'y
    // rejoue pas, sinon le joueur ne ferait que la suivre et il n'y aurait plus rien a reproduire.
    if( m_rhythmIsPlaying )
    {
        m_notePlayer.playMetronomeClick( m_rhythmBeatInBar == 0 );
    }
    else
    {
        playRhythmHitsForBeat( m_rhythmBeatInBar );
    }

    ++m_rhythmBeatInBar;

    if( m_rhythmBeatInBar < beatsPerBar )
    {
        emit rhythmStateChanged();

        return;
    }

    // La mesure est finie.
    m_rhythmBeatInBar = 0;

    if( !m_rhythmIsPlaying )
    {
        // L'ecoute est finie : a toi.
        beginRhythmPlaying();

        emit rhythmStateChanged();

        return;
    }

    finishRhythmLoop();
}

void ExerciseSessionController::playRhythmHitsForBeat( int p_beatInBar )
{
    if( m_session == nullptr )
    {
        return;
    }

    const domain::Question & question = m_session->currentQuestion();

    const double beatMs = domain::beatDurationMs( static_cast<double>( question.bpm ) );

    if( beatMs <= 0.0 )
    {
        return;
    }

    const auto barBeat = static_cast<double>( p_beatInBar );

    for( const domain::RhythmHit & hit : patternOf( question ).hits() )
    {
        if( ( hit.beat < barBeat ) || ( hit.beat >= barBeat + 1.0 ) )
        {
            continue;
        }

        const int delayMs = static_cast<int>( std::lround( ( hit.beat - barBeat ) * beatMs ) );

        if( delayMs <= 0 )
        {
            m_notePlayer.playDrum( hit.drum );

            continue;
        }

        // Une frappe decalee part en differe : c'est ce qu'est une syncope - une frappe ENTRE deux temps. Le
        // controleur est le contexte du tir, donc il vit assez longtemps pour l'entendre.
        QTimer::singleShot( delayMs, this, [this, drum = hit.drum]() { m_notePlayer.playDrum( drum ); } );
    }
}

void ExerciseSessionController::playRhythmModelOnce()
{
    if( m_session == nullptr )
    {
        return;
    }

    // Toute la cellule d'un coup : les frappes du premier temps partent maintenant, les autres en differe. C'est
    // exactement le chemin de la mesure d'ecoute, sans le timer de la boucle.
    for( int beat = 0; beat < rhythmBeatsPerBar(); ++beat )
    {
        playRhythmHitsForBeat( beat );
    }
}

double ExerciseSessionController::rhythmPositionInBeats() const noexcept
{
    if( m_session == nullptr )
    {
        return 0.0;
    }

    const double beatMs = domain::beatDurationMs( static_cast<double>( m_session->currentQuestion().bpm ) );

    if( beatMs <= 0.0 )
    {
        return 0.0;
    }

    return static_cast<double>( m_rhythmClock.elapsed() ) / beatMs;
}

int ExerciseSessionController::coveredOnsetCount() const noexcept
{
    if( m_session == nullptr )
    {
        return 0;
    }

    const std::vector<bool> & covered = m_session->currentQuestion().coveredOnsets;

    return static_cast<int>( std::ranges::count( covered, true ) );
}

void ExerciseSessionController::tapRhythm()
{
    if( ( m_session == nullptr ) || !isRhythmQuestion() )
    {
        return;
    }

    // Le doigt s'entend TOUJOURS, meme quand la frappe ne vaut rien : sans ce son, taper donnerait l'impression que
    // l'ecran n'a pas recu le geste, et le joueur recommencerait a taper pour rien.
    m_notePlayer.playDrum( domain::Drum::Snare );

    if( !m_rhythmIsPlaying )
    {
        // Pendant l'ecoute, une frappe sonne et n'est pas jugee : celui qui accompagne la cellule ne perd pas une vie
        // pour l'avoir suivie.
        return;
    }

    // La mesure est prise ICI, et le jugement appartient au domaine : c'est la seule repartition possible entre une
    // horloge et des regles pures.
    m_rhythmLastQuality = screenQualityOf( m_session->registerRhythmTap( rhythmPositionInBeats() ) );

    // Et l'ardoise se relit tout de suite, pour que l'ecran montre la tentative se construire frappe apres frappe
    // plutot que d'attendre le verdict pour en dire un mot.
    m_rhythmCoveredOnsets = coveredOnsetCount();

    emit rhythmStateChanged();
}

void ExerciseSessionController::playCurrentQuestion()
{
    if( m_session == nullptr )
    {
        return;
    }

    const domain::Question & question = m_session->currentQuestion();

    // Une question de rythme ne se joue pas, elle se BOUCLE : sa cellule s'ecoute puis se reproduit. Le domaine n'a
    // donc rien a faire entendre ici, et c'est la boucle qui s'en charge.
    if( question.kind == domain::QuestionKind::Rhythm )
    {
        startRhythmQuestion();

        return;
    }

    // Un accord ne se joue pas comme un intervalle : ses notes sont PLAQUEES, et la question est de reconnaitre leur
    // couleur, pas de suivre une melodie.
    if( question.kind == domain::QuestionKind::Chord )
    {
        playChordNotes( question.chord );

        return;
    }

    // Toute autre question met fin a la boucle de rythme, et c'est ce qui garantit qu'une cellule ne continue pas a
    // battre sous une question d'intervalle.
    stopRhythmLoop();

    // Une question chantee n'est pas jouee d'office, sauf pour un debutant : les autres ont le bouton "Ecouter",
    // et s'en servir coute de l'experience (voir listenToTarget).
    if( ( question.kind == domain::QuestionKind::Sing ) && !isBeginner() )
    {
        return;
    }

    const domain::Note rootNote{ question.rootMidiNumber };
    const domain::Note upperNote = rootNote.transposedBy( question.target.semitones() );

    if( question.direction == domain::IntervalDirection::Harmonic )
    {
        const std::array<domain::Note, 2> notes{ rootNote, upperNote };

        m_notePlayer.playChord( notes );

        return;
    }

    if( question.direction == domain::IntervalDirection::Descending )
    {
        // A falling interval is the SAME interval: only the order of the two notes changes. Playing
        // them in the wrong order would change what is heard, and the interval the player is asked to
        // name would no longer be the one the domain named.
        const std::array<domain::Note, 2> descendingNotes{ upperNote, rootNote };

        m_notePlayer.playMelody( descendingNotes, m_session->settings().melodicGap );

        return;
    }

    const std::array<domain::Note, 2> ascendingNotes{ rootNote, upperNote };

    m_notePlayer.playMelody( ascendingNotes, m_session->settings().melodicGap );
}

void ExerciseSessionController::playChordNotes( const domain::Chord & p_chord )
{
    // Les notes viennent du domaine, l'accord entier d'un coup : c'est la COULEUR qu'on fait entendre, pas une suite
    // de notes. Un span se construit sur le vecteur, donc rien n'est copie de plus.
    const std::vector<domain::Note> notes = p_chord.notes();

    m_notePlayer.playChord( notes );
}

void ExerciseSessionController::recordCurrentQuestion( bool p_wasCorrect, bool p_wasRevealed )
{
    if( ( m_questionLog == nullptr ) || ( m_session == nullptr ) )
    {
        // Aucun journal : l'application tourne sans statistiques, et c'est un mode valide. Un test, un appareil dont le
        // disque est plein - dans tous les cas, le jeu continue.
        return;
    }

    const domain::Question & question = m_session->currentQuestion();

    domain::QuestionRecord record;

    // L'horloge est lue ICI, dans la couche qui a le droit de la lire. Le domaine recoit une date, il ne la demande
    // jamais : c'est ce qui garde le domaine pur, et le journal testable sans attendre une seule seconde.
    record.askedAt = std::chrono::system_clock::now();

    record.kind = question.kind;

    // Ce que la question DEMANDAIT, dans l'unite de son genre : le genre dit comment lire ce nombre. Une seule colonne
    // pour trois unites, parce que c'est le genre - et non le nombre - qui porte le sens.
    switch( question.kind )
    {
        case domain::QuestionKind::Rhythm:
            record.target = static_cast<std::int32_t>( question.patternIndex );
            break;

        case domain::QuestionKind::Chord:
            record.target = static_cast<std::int32_t>( question.chord.quality );
            break;

        case domain::QuestionKind::NamedInterval:
        case domain::QuestionKind::Direction:
        case domain::QuestionKind::Sing:
            record.target = question.target.semitones();
            break;
    }

    record.direction = question.direction;

    // Le nombre d'essais : la bonne reponse donnee a l'essai numero un, c'est UN essai. Le domaine compte les erreurs,
    // donc l'essai gagnant s'y ajoute - et c'est cette addition qui fait la difference entre « il sait » et « il a fini
    // par trouver ».
    record.attemptCount = question.wrongAttemptCount + 1;

    record.replayCount = question.replayCount;

    record.outcome = domain::outcomeOf( p_wasCorrect, p_wasRevealed, record.attemptCount );

    m_questionLog->append( record );
}

void ExerciseSessionController::startReviewSession()
{
    domain::SessionSettings settings = m_settings;

    const std::vector<domain::QuestionTarget> plan = reviewPlan();

    if( plan.empty() )
    {
        // Rien a reviser : pas de journal, ou trop peu de matiere pour construire un « facile puis difficile ». Le bilan
        // devient alors une partie ordinaire, ce qui vaut mieux qu'un bouton qui refuse de s'ouvrir.
        m_isReviewRunning = false;
        m_reviewEasyQuestionCount = 0;

        beginSession( settings );

        return;
    }

    settings.plannedQuestions = plan;
    settings.questionCount = plan.size();

    // Un bilan ne se PERD pas : il se termine. Le joueur vient voir ou il en est, et rater trois questions de suite le
    // renverrait chez lui avant la fin - exactement l'inverse d'un bilan.
    settings.lives = std::nullopt;

    // Et les aides restent : un bilan est un examen de passage, jamais un couperet.
    settings.aidsAllowed = true;

    // L'echauffement, c'est la premiere tranche du plan, bornee : au-dela, la difficulte commence - et c'est ce qui
    // permet a l'encouragement d'arriver au bon moment.
    m_reviewEasyQuestionCount = std::max<std::size_t>( 1, std::min( REVIEW_EASY_QUESTION_COUNT, plan.size() / 2 ) );
    m_isReviewRunning = true;

    beginSession( settings );
}

std::vector<domain::QuestionTarget> ExerciseSessionController::reviewPlan() const
{
    std::vector<domain::QuestionTarget> plan;

    if( m_questionLog == nullptr )
    {
        return plan;
    }

    const auto since = std::chrono::system_clock::now() - ( std::chrono::hours{ 24 } * REVIEW_PERIOD_DAYS );

    domain::StatisticsFilter filter;
    filter.since = since;

    const std::vector<domain::TargetStatistics> byTarget =
      domain::statisticsByTarget( m_questionLog->since( since ), filter );

    if( byTarget.size() < 4 )
    {
        // Moins de quatre cibles travaillees : il n'y a pas de « facile » et de « difficile » a opposer, seulement
        // quelques exercices. Un bilan de deux questions n'apprendrait rien au joueur sur lui-meme.
        return plan;
    }

    const auto toPlannedQuestion = []( const domain::TargetStatistics & p_target ) {
        return domain::QuestionTarget{ p_target.kind, p_target.target, p_target.direction };
    };

    // D'ABORD ce qui va bien, du meilleur au moins bon : un bilan qui commencerait par un echec serait decourageant, et
    // c'est l'autre sens que Roger a demande. Les cibles sont triees du PLUS FAIBLE au meilleur, donc on remonte la liste
    // par la fin.
    const std::size_t easyCount = std::min( REVIEW_EASY_QUESTION_COUNT, byTarget.size() / 2 );

    for( std::size_t index = 0; index < easyCount; ++index )
    {
        plan.push_back( toPlannedQuestion( byTarget.at( byTarget.size() - 1 - index ) ) );
    }

    // PUIS ce qui resiste, du plus faible au moins faible - et sans reprendre ce qui a servi d'echauffement.
    for( std::size_t index = easyCount; index + easyCount < byTarget.size(); ++index )
    {
        plan.push_back( toPlannedQuestion( byTarget.at( index ) ) );
    }

    return plan;
}

bool ExerciseSessionController::isCurrentQuestionAHardPart() const noexcept
{
    if( !m_isReviewRunning || ( m_session == nullptr ) )
    {
        return false;
    }

    // Le rang dans le plan : au-dela de l'echauffement, c'est ce qui resiste. Le controleeur a construit le plan dans cet
    // ordre, donc il le sait - sans recroiser les statistiques a chaque question.
    return m_session->questionNumber() > m_reviewEasyQuestionCount;
}

QString ExerciseSessionController::encouragementText() const
{
    if( !m_isReviewRunning || ( m_session == nullptr ) )
    {
        // Hors bilan, il n'y a rien a dire : un ecran qui parle pour ne rien dire devient un ecran qu'on n'ecoute plus, et
        // le silence est ce qui donne du poids aux mots qui restent.
        return QString{};
    }

    // Une reussite sur ce qui resistait : le mot juste, et il arrive au bon moment.
    if( isFeedbackVisible() && m_session->wasLastAnswerCorrect() && isCurrentQuestionAHardPart() )
    {
        return tr( "Bravo ! C'est exactement ce qui te résistait." );
    }

    // Une serie : on le dit, parce qu'une serie se sent et se dit.
    if( m_session->score().streak() >= 3 )
    {
        return tr( "Bien joué, ne lâche pas." );
    }

    // Et AVANT d'attaquer une difficulte connue, la prevenance : c'est le seul moment ou elle aide vraiment.
    if( isAsking() && isCurrentQuestionAHardPart() )
    {
        return tr( "Là, tu attaques un exercice qui t'a résisté cette semaine. Prends ton temps." );
    }

    return QString{};
}

bool ExerciseSessionController::isWeekEnd() const
{
    // QDate::dayOfWeek() suit exactement la convention du domaine : 1 = lundi ... 7 = dimanche. Cette correspondance est
    // verifiee par un test du domaine, et non par ce commentaire.
    return domain::isWeekEnd( QDate::currentDate().dayOfWeek() );
}

int ExerciseSessionController::chordQualityCount() const noexcept
{
    return static_cast<int>( domain::CHORD_QUALITY_COUNT );
}

bool ExerciseSessionController::isChordHintAvailable() const noexcept
{
    return ( m_session != nullptr ) && m_session->canRemoveOneWrongChordChoice();
}

bool ExerciseSessionController::isChordArpeggioAvailable() const noexcept
{
    return ( m_session != nullptr ) && m_session->canHearChordAsArpeggio();
}

void ExerciseSessionController::useChordHint()
{
    if( ( m_session == nullptr ) || !m_session->removeOneWrongChordChoice() )
    {
        return;
    }

    // L'ecran doit RELIRE la grille : une reponse retiree est une reponse qui disparait. La grille est reconstruite a
    // chaque lecture depuis le domaine (voir chordChoices), donc le signal suffit - l'ecran ne garde aucune copie.
    emit questionChanged();
    emit sessionChanged();
}

void ExerciseSessionController::playCurrentChordAsArpeggio()
{
    if( ( m_session == nullptr ) || !isChordQuestion() )
    {
        return;
    }

    // Une note apres l'autre, et LENTEMENT : c'est tout l'interet de l'arpege - trop rapide, il redevient un accord, et
    // l'indice ne montrerait plus rien.
    m_notePlayer.playMelody( m_session->currentQuestion().chord.notes(), ARPEGGIO_NOTE_GAP );
}

void ExerciseSessionController::playCurrentQuestionAsChord()
{
    if( m_session == nullptr )
    {
        return;
    }

    const domain::Question & question = m_session->currentQuestion();

    // Sur une question d'accord, la confirmation est l'accord lui-meme : il etait deja plaque, et le rejouer est
    // exactement ce que le joueur doit garder en tete.
    if( question.kind == domain::QuestionKind::Chord )
    {
        playChordNotes( question.chord );

        return;
    }

    const domain::Note rootNote{ question.rootMidiNumber };
    const domain::Note upperNote = rootNote.transposedBy( question.target.semitones() );

    // The two notes TOGETHER, whatever the direction of the question was: on a correct answer the
    // player already knows which way it went, and what is left to hear is the colour.
    const std::array<domain::Note, 2> notes{ rootNote, upperNote };

    m_notePlayer.playChord( notes );
}

}    // namespace musichien::ui