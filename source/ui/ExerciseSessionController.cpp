#include "ui/ExerciseSessionController.h"

#include "domain/music/Interval.h"
#include "ui/IntervalDescription.h"

#include <QString>

#include <array>
#include <optional>
#include <random>
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

}    // namespace

ExerciseSessionController::ExerciseSessionController( domain::NotePlayer & p_notePlayer,
                                                      domain::SessionSettings p_settings,
                                                      domain::HintBook p_hintBook,
                                                      VibrationCallback p_vibrate,
                                                      domain::PlayerPreferences * p_levelStore,
                                                      QObject * p_parent )
  : QObject{ p_parent }
  , m_notePlayer{ p_notePlayer }
  , m_settings{ p_settings }
  , m_hintBook{ std::move( p_hintBook ) }
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
        m_settings = domain::sessionSettingsFor( *m_playerLevel );
    }

    // The instruments the player asked for. An empty list is a first run: everything is offered, which is what a
    // fresh installation should sound like.
    if( m_levelStore != nullptr )
    {
        m_enabledInstruments = m_levelStore->storedEnabledInstruments();
    }

    m_enabledInstruments.resize( domain::INSTRUMENT_COUNT, true );
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
    return ( m_session != nullptr ) ? static_cast<int>( m_session->settings().questionCount ) : 0;
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
    const std::array<std::optional<domain::Interval>, domain::CIRCLE_OF_FIFTHS_SLOT_COUNT> layout =
      domain::layoutOnCircle( m_session->currentQuestion().choices );

    for( std::size_t slot = 0; slot < layout.size(); ++slot )
    {
        QVariantMap position;
        position.insert( QStringLiteral( "slot" ), static_cast<int>( slot ) );
        position.insert( QStringLiteral( "isAvailable" ), layout.at( slot ).has_value() );

        if( layout.at( slot ).has_value() )
        {
            const QVariantMap description = describeInterval( *layout.at( slot ) );

            position.insert( QStringLiteral( "semitones" ), description.value( QStringLiteral( "semitones" ) ) );
            position.insert( QStringLiteral( "identifier" ), description.value( QStringLiteral( "identifier" ) ) );
            position.insert( QStringLiteral( "name" ), description.value( QStringLiteral( "name" ) ) );

            // La classe de l'intervalle, ET PAS SEULEMENT SA TAILLE : c'est elle qui porte la COULEUR d'une
            // place. L'oublier donne un cercle tout gris - la carte perd d'un coup la moitie de ce qu'elle dit.
            position.insert( QStringLiteral( "intervalClass" ),
                             description.value( QStringLiteral( "intervalClass" ) ) );
        }

        positions.append( position );
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
    if( m_session == nullptr )
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
    if( ( m_session == nullptr ) || !m_session->isHintAvailable() )
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
    m_settings = domain::sessionSettingsFor( level );

    if( m_levelStore != nullptr )
    {
        m_levelStore->storeLevel( level );
    }

    emit playerLevelChanged();
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
                return ExerciseSessionController::tr( "Les composes" );
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
        instrument.insert( QStringLiteral( "name" ), QString::fromLatin1( domain::INSTRUMENT_NAMES.at( index ) ) );

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
    // Le bouton a repondu : un clic tres court et discret, pour que la main soit entendue.
    m_notePlayer.playTapCue();

    // The seed is drawn HERE, in the interface layer, and never inside the domain.
    //
    // That is what keeps a session reproducible from its seed in a test, and it is also why the rules
    // of the game can be replayed exactly when something goes wrong. The domain owns no entropy
    // source, on purpose.
    std::random_device entropySource;

    m_session = std::make_unique<domain::ExerciseSession>( entropySource(), m_settings );

    emit runningChanged();

    refreshChoices();

    playCurrentQuestion();

    emit scoreChanged();
    emit sessionChanged();
}

void ExerciseSessionController::stopSession()
{
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

    const bool isCorrect = m_session->answer( p_semitones );

    // Always, and not only when the question is over: a wrong answer closes the grid in, and the
    // screen must show the grid that exists rather than the one it had a moment ago.
    refreshChoices();

    if( isCorrect )
    {
        // A correct answer is heard again as a CHORD: the two notes together, one block instead of two,
        // which is twice as short - and a genuinely different listen of the same interval, the colour
        // without the melody. Roger asked for it to stop the success from dragging, and the reason to
        // keep it is musical: hearing the interval both ways is what seals it.
        playCurrentQuestionAsChord();
    }
    else
    {
        // A wrong answer is heard again IMMEDIATELY, and in its original form: there is something to
        // catch up on, and the melody is what gives the second note its meaning.
        playCurrentQuestion();

        // And it is announced, so that the screen can answer with its BODY - the shake, and the
        // vibration on a device that has a motor. The controller knows what happened; how it should feel
        // is not its job.
        emit wrongAnswerGiven();

        // The cue, then the buzz: both are mistakes being made audible and tangible, and neither is the
        // interval the player is being asked to name. See NotePlayer::playMistakeCue for why that
        // distinction matters.
        m_notePlayer.playMistakeCue();

        if( m_vibrate )
        {
            m_vibrate();
        }
    }

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

    refreshChoices();

    playCurrentQuestion();

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

    emit scoreChanged();
    emit sessionChanged();
}

void ExerciseSessionController::stopPlayback()
{
    m_notePlayer.stopAll();
}

void ExerciseSessionController::refreshChoices()
{
    if( m_session == nullptr )
    {
        return;
    }

    QVariantList choices;

    for( const domain::Interval & choice : m_session->currentQuestion().choices )
    {
        choices.append( describeInterval( choice ) );
    }

    m_choices = std::move( choices );

    emit questionChanged();
}

void ExerciseSessionController::playCurrentQuestion()
{
    if( m_session == nullptr )
    {
        return;
    }

    const domain::Question & question = m_session->currentQuestion();

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

void ExerciseSessionController::playCurrentQuestionAsChord()
{
    if( m_session == nullptr )
    {
        return;
    }

    const domain::Question & question = m_session->currentQuestion();

    const domain::Note rootNote{ question.rootMidiNumber };
    const domain::Note upperNote = rootNote.transposedBy( question.target.semitones() );

    // The two notes TOGETHER, whatever the direction of the question was: on a correct answer the
    // player already knows which way it went, and what is left to hear is the colour.
    const std::array<domain::Note, 2> notes{ rootNote, upperNote };

    m_notePlayer.playChord( notes );
}

}    // namespace musichien::ui