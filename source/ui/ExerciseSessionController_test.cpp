#include "ui/ExerciseSessionController.h"

#include "domain/audio/NotePlayerFake.h"
#include "domain/music/Temperament.h"

#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

namespace musichien::ui
{

// ---------------------------------------------------------------------------------------------------------------------
// The view model of the exercise screen
//
// It runs with the fake note player, so nothing here needs a sound card or a phone. What is checked is
// what this class is responsible for: what the screen would HEAR, and what it would DISPLAY.
//
// The seed of a session is drawn from the entropy of the machine, so a test can never know WHICH
// interval will be asked. It does not need to: every property below holds for any interval, and asking
// the session what was heard is exactly what the screen does.
// ---------------------------------------------------------------------------------------------------------------------

namespace
{

constexpr std::size_t SESSION_QUESTION_COUNT = 10;

// A session that ALWAYS goes up.
//
// The direction is drawn at random in a real session, so a test asserting "the question was heard as a
// melody" would fail roughly one time in five - and a test that fails at random teaches nothing except
// to be ignored. Pinning the direction is what makes these tests precise instead of tolerant.
[[nodiscard]] domain::SessionSettings ascendingOnlySettings()
{
    domain::SessionSettings settings;

    settings.ascendingShare = 100;
    settings.descendingShare = 0;
    settings.harmonicShare = 0;

    return settings;
}

// The interval the session just asked, as the screen reads it.
[[nodiscard]] std::int32_t heardDistance( const ExerciseSessionController & p_controller )
{
    return p_controller.heardInterval().value( "semitones" ).toInt();
}

// A book that has a hint for EVERY interval going up.
//
// The session draws its interval at random, so a test cannot know which one will be asked. Covering them
// all is what keeps the assertion precise, instead of sorting through special cases.
[[nodiscard]] domain::HintBook hintBookForEveryAscendingInterval()
{
    domain::HintBook hintBook;

    for( std::int32_t semitones = 0; semitones <= domain::MAXIMUM_INTERVAL_SEMITONES; ++semitones )
    {
        hintBook.add( domain::Interval{ semitones },
                      domain::IntervalDirection::Ascending,
                      domain::IntervalHint{ "indice " + std::to_string( semitones ), {} } );
    }

    return hintBook;
}

void answerCorrectly( ExerciseSessionController & p_controller )
{
    p_controller.answer( heardDistance( p_controller ) );
}

}    // namespace

TEST( ExerciseSessionControllerTest, starting_a_session_asks_the_first_question )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, ascendingOnlySettings() };

    EXPECT_FALSE( controller.running() );

    controller.startSession();

    EXPECT_TRUE( controller.running() );
    EXPECT_TRUE( controller.isAsking() );
    EXPECT_FALSE( controller.isFinished() );
    EXPECT_EQ( 1, controller.questionNumber() );
    EXPECT_EQ( SESSION_QUESTION_COUNT, controller.questionCount() );

    // The question is HEARD, not only displayed: two notes, one after the other.
    ASSERT_EQ( 1, notePlayer.playedMelodies().size() );
    EXPECT_EQ( 2, notePlayer.playedMelodies().front().notes.size() );
}

TEST( ExerciseSessionControllerTest, every_choice_is_ready_to_display )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, ascendingOnlySettings() };

    controller.startSession();

    const QVariantList choices = controller.choices();

    // A question with a single choice is not a question, and the right answer is always one of them.
    ASSERT_GE( choices.size(), 2 );

    const QVariantMap firstChoice = choices.first().toMap();

    // The screen reads names and identifiers and composes neither, so both have to be filled.
    EXPECT_FALSE( firstChoice.value( "identifier" ).toString().isEmpty() );
    EXPECT_FALSE( firstChoice.value( "name" ).toString().isEmpty() );
    EXPECT_TRUE( firstChoice.contains( "intervalClass" ) );
}

TEST( ExerciseSessionControllerTest, the_right_answer_scores_and_the_verdict_is_heard )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, ascendingOnlySettings() };

    controller.startSession();
    answerCorrectly( controller );

    EXPECT_TRUE( controller.wasLastAnswerCorrect() );
    EXPECT_TRUE( controller.isFeedbackVisible() );
    EXPECT_FALSE( controller.isAsking() );
    EXPECT_EQ( domain::SessionScore::BASE_XP_PER_SUCCESS, controller.experience() );

    // The verdict is played AGAIN - as a chord, which the next test checks in detail. The point here
    // is that the screen never only says "right": it always says it again out loud.
    EXPECT_EQ( 1, notePlayer.playedMelodies().size() );
    EXPECT_EQ( 1, notePlayer.playedChords().size() );
}

TEST( ExerciseSessionControllerTest, a_wrong_answer_is_played_again_and_announced )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, ascendingOnlySettings() };

    controller.startSession();

    int wrongAnswerCount = 0;

    // No QtTest here: the signal is counted with a plain connection, which is all this needs.
    QObject::connect( &controller, &ExerciseSessionController::wrongAnswerGiven, [&wrongAnswerCount] { ++wrongAnswerCount; } );

    controller.answer( heardDistance( controller ) + 1 );

    EXPECT_FALSE( controller.wasLastAnswerCorrect() );
    EXPECT_TRUE( controller.isAsking() );
    EXPECT_FALSE( controller.isFeedbackVisible() );

    // Nothing earned, and one life gone: a mistake costs an attempt, never experience.
    EXPECT_EQ( 0, controller.experience() );
    EXPECT_EQ( 4, controller.lives() );

    // The wrong answer is remembered, so that the verdict can be read against it.
    EXPECT_EQ( heardDistance( controller ) + 1,
               controller.answeredInterval().value( "semitones" ).toInt() );

    // The interval is heard AGAIN immediately, and as a melody: there is something to catch up on, and
    // it is the melody that gives the second note its meaning.
    EXPECT_EQ( 2, notePlayer.playedMelodies().size() );
    EXPECT_TRUE( notePlayer.playedChords().empty() );

    // And the screen is told, so that it can answer with its body - the shake, and one day a vibration.
    EXPECT_EQ( 1, wrongAnswerCount );
}

TEST( ExerciseSessionControllerTest, a_correct_answer_is_heard_again_as_a_chord )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, ascendingOnlySettings() };

    controller.startSession();
    answerCorrectly( controller );

    // One melody for the question, and ONE CHORD for the verdict: the two notes together are twice as
    // short as the melody, and a genuinely different listen of the same interval.
    EXPECT_EQ( 1, notePlayer.playedMelodies().size() );
    ASSERT_EQ( 1, notePlayer.playedChords().size() );

    EXPECT_EQ( 2, notePlayer.playedChords().front().notes.size() );

    // A success is not a mistake: nothing is announced to the screen.
    EXPECT_EQ( 0, notePlayer.stopCount() );
}

TEST( ExerciseSessionControllerTest, the_answer_is_offered_after_three_wrong_ones )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, ascendingOnlySettings() };

    controller.startSession();

    const std::int32_t wrongDistance = heardDistance( controller ) + 1;

    controller.answer( wrongDistance );
    controller.answer( wrongDistance );

    EXPECT_FALSE( controller.isHelpAvailable() );

    controller.answer( wrongDistance );

    EXPECT_TRUE( controller.isHelpAvailable() );

    controller.revealAnswer();

    EXPECT_TRUE( controller.isFeedbackVisible() );

    // Help teaches, and it pays nothing: making it profitable would turn it into a way to farm.
    EXPECT_EQ( 0, controller.experience() );
}

TEST( ExerciseSessionControllerTest, a_perfect_session_earns_its_star )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, ascendingOnlySettings() };

    controller.startSession();

    for( std::size_t question = 0; question < SESSION_QUESTION_COUNT; ++question )
    {
        answerCorrectly( controller );
        controller.continueToNextQuestion();
    }

    EXPECT_TRUE( controller.isFinished() );

    // The question number stops at the last question rather than going one past it.
    EXPECT_EQ( SESSION_QUESTION_COUNT, controller.questionNumber() );

    EXPECT_TRUE( controller.starEarned() );
}

TEST( ExerciseSessionControllerTest, replaying_counts_and_costs_experience )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, ascendingOnlySettings() };

    controller.startSession();

    controller.replay();

    EXPECT_EQ( 2, notePlayer.playedMelodies().size() );

    answerCorrectly( controller );

    // Listening again is allowed, and not free: a correct answer after a replay is worth less than one
    // heard right away.
    EXPECT_LT( controller.experience(), domain::SessionScore::BASE_XP_PER_SUCCESS );
}

TEST( ExerciseSessionControllerTest, leaving_the_loop_releases_the_sound_and_the_session )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, ascendingOnlySettings() };

    controller.startSession();

    controller.stopSession();

    EXPECT_FALSE( controller.running() );
    EXPECT_TRUE( controller.choices().isEmpty() );
    EXPECT_GE( notePlayer.stopCount(), 1 );

    // And the bench is back: nothing of the session is left behind.
    EXPECT_FALSE( controller.isFinished() );
    EXPECT_EQ( 0, controller.experience() );
}

// ---------------------------------------------------------------------------------------------------------------------
// A mistake, as a sound and as a feeling
//
// Two channels that are not the interval being taught: one is heard, one is felt. Both are asserted
// separately from the melody that is played again, because telling them apart is the whole point of
// having a cue that is not a note.
// ---------------------------------------------------------------------------------------------------------------------

TEST( ExerciseSessionControllerTest, a_wrong_answer_is_heard_as_a_cue_and_felt )
{
    domain::NotePlayerFake notePlayer;

    int vibrationCount = 0;

    ExerciseSessionController controller{ notePlayer,
                                          ascendingOnlySettings(),
                                          {},
                                          {},
                                          [&vibrationCount] { ++vibrationCount; } };

    controller.startSession();
    controller.answer( heardDistance( controller ) + 1 );

    EXPECT_EQ( 1, notePlayer.mistakeCueCount() );
    EXPECT_EQ( 1, vibrationCount );
}

TEST( ExerciseSessionControllerTest, a_correct_answer_neither_sounds_the_cue_nor_vibrates )
{
    domain::NotePlayerFake notePlayer;

    int vibrationCount = 0;

    ExerciseSessionController controller{ notePlayer,
                                          ascendingOnlySettings(),
                                          {},
                                          {},
                                          [&vibrationCount] { ++vibrationCount; } };

    controller.startSession();
    answerCorrectly( controller );

    EXPECT_EQ( 0, notePlayer.mistakeCueCount() );
    EXPECT_EQ( 0, vibrationCount );
}

TEST( ExerciseSessionControllerTest, a_device_that_cannot_vibrate_still_hears_the_cue )
{
    // No callback at all, which is what a development machine honestly is. Nothing must depend on it.
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, ascendingOnlySettings() };

    controller.startSession();
    controller.answer( heardDistance( controller ) + 1 );

    EXPECT_EQ( 1, notePlayer.mistakeCueCount() );
}

// ---------------------------------------------------------------------------------------------------------------------
// The memory hint
//
// A hint is a nudge, and its timing is the whole design: too early it is the answer in disguise, too late
// it arrives after the player has given up.
// ---------------------------------------------------------------------------------------------------------------------

TEST( ExerciseSessionControllerTest, the_hint_waits_for_a_mistake )
{
    domain::NotePlayerFake notePlayer;

    ExerciseSessionController controller{ notePlayer,
                                          ascendingOnlySettings(),
                                          hintBookForEveryAscendingInterval() };

    controller.startSession();

    // Nothing yet: an interval offered before the player has tried would just be a second question.
    EXPECT_TRUE( controller.hintText().isEmpty() );

    const std::int32_t askedDistance = heardDistance( controller );

    controller.answer( askedDistance + 1 );

    // And the hint is the one of the interval that was ASKED, never of the one that was answered.
    EXPECT_EQ( QString::fromStdString( "indice " + std::to_string( askedDistance ) ),
               controller.hintText() );
}

TEST( ExerciseSessionControllerTest, the_hint_stays_for_the_verdict_and_leaves_with_the_question )
{
    domain::NotePlayerFake notePlayer;

    ExerciseSessionController controller{ notePlayer,
                                          ascendingOnlySettings(),
                                          hintBookForEveryAscendingInterval() };

    controller.startSession();
    controller.answer( heardDistance( controller ) + 1 );

    ASSERT_FALSE( controller.hintText().isEmpty() );

    answerCorrectly( controller );

    // The verdict is being read, so the hint is still worth reading: that is where it turns "you were
    // wrong" into "that is how you could have remembered it".
    EXPECT_FALSE( controller.hintText().isEmpty() );

    controller.continueToNextQuestion();

    // And the next question starts clean: a hint belongs to a question, not to a session.
    EXPECT_TRUE( controller.hintText().isEmpty() );
}

TEST( ExerciseSessionControllerTest, an_interval_without_a_hint_shows_nothing )
{
    // An empty book is what a gap in the content file looks like. Nothing is displayed, and nothing else
    // changes.
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, ascendingOnlySettings() };

    controller.startSession();
    controller.answer( heardDistance( controller ) + 1 );

    EXPECT_TRUE( controller.hintText().isEmpty() );
}

// ---------------------------------------------------------------------------------------------------------------------
// The level of the player
//
// Asked once, remembered, and used to decide where the sessions start. Three things, and the third is the one
// that would be easy to lose: a level must never make the game unplayable.
// ---------------------------------------------------------------------------------------------------------------------

TEST( ExerciseSessionControllerTest, a_level_decides_where_the_sessions_start )
{
    domain::NotePlayerFake notePlayer;
    domain::PlayerPreferencesFake levelStore;

    ExerciseSessionController controller{ notePlayer, {}, {}, {}, {}, &levelStore };

    // Nothing chosen yet, and that is a question to ask - not a default to assume.
    EXPECT_FALSE( controller.hasChosenLevel() );

    controller.choosePlayerLevel( static_cast<int>( domain::PlayerLevel::Advanced ) );

    EXPECT_TRUE( controller.hasChosenLevel() );
    EXPECT_EQ( static_cast<int>( domain::PlayerLevel::Advanced ), controller.playerLevel() );

    // And it is REMEMBERED, which is the whole difference between a level and a setting.
    EXPECT_TRUE( levelStore.storedLevel().has_value() );

    controller.startSession();

    // A player who says he knows the intervals is not asked to tell two of them apart: the grid he is offered
    // is wide, where a beginner gets two choices.
    EXPECT_GT( controller.choices().size(), 2 );
}

TEST( ExerciseSessionControllerTest, a_remembered_level_is_there_at_start_up )
{
    domain::NotePlayerFake notePlayer;
    domain::PlayerPreferencesFake levelStore;

    levelStore.storeLevel( domain::PlayerLevel::Advanced );

    // No choosePlayerLevel call at all: the application opens on what it remembers.
    ExerciseSessionController controller{ notePlayer, {}, {}, {}, {}, &levelStore };

    EXPECT_TRUE( controller.hasChosenLevel() );
    EXPECT_EQ( static_cast<int>( domain::PlayerLevel::Advanced ), controller.playerLevel() );
}

TEST( ExerciseSessionControllerTest, a_level_changes_the_palette_and_nothing_else )
{
    domain::NotePlayerFake notePlayer;
    domain::PlayerPreferencesFake levelStore;

    ExerciseSessionController controller{ notePlayer, {}, {}, {}, {}, &levelStore };

    controller.choosePlayerLevel( static_cast<int>( domain::PlayerLevel::Beginner ) );
    controller.startSession();

    // The same ten questions, the same lives, the same scoring as always: a level says WHERE to start, never
    // HOW the game is played.
    EXPECT_EQ( SESSION_QUESTION_COUNT, controller.questionCount() );
    EXPECT_EQ( 5, controller.lives() );

    // A beginner gets the two intervals of the learning order, and not one more.
    EXPECT_EQ( 2, controller.choices().size() );
}

TEST( ExerciseSessionControllerTest, an_application_with_nowhere_to_remember_still_runs )
{
    // No store at all, which is what a test or a machine without a writable settings file looks like. Nothing
    // must depend on it.
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, ascendingOnlySettings() };

    EXPECT_FALSE( controller.hasChosenLevel() );
    EXPECT_EQ( -1, controller.playerLevel() );

    controller.startSession();

    EXPECT_TRUE( controller.running() );
}

TEST( ExerciseSessionControllerTest, the_levels_to_offer_are_ready_to_display )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, ascendingOnlySettings() };

    const QVariantList levels = controller.playerLevels();

    ASSERT_EQ( domain::PLAYER_LEVEL_COUNT, static_cast<std::size_t>( levels.size() ) );

    for( int index = 0; index < levels.size(); ++index )
    {
        const QVariantMap level = levels.at( index ).toMap();

        // Every entry an index and a name: the screen displays them and never composes one.
        EXPECT_EQ( index, level.value( "index" ).toInt() );
        EXPECT_FALSE( level.value( "name" ).toString().isEmpty() );
    }
}

TEST( ExerciseSessionControllerTest, every_choice_is_on_the_circle_at_the_place_of_its_class )
{
    // Le bug que ce test surveille a coute une soiree de test a Roger : la carte ignorait silencieusement un
    // intervalle quand un AUTRE de la meme classe occupait deja sa place. La question devenait alors impossible
    // a repondre - le bouton affichait l'octave quand l'unisson etait demande - et rien n'echouait.
    //
    // La regle est donc verifiee pour CHAQUE intervalle offert, sur chaque question d'une session entiere, et
    // non seulement pour la cible : tout ce que la session propose doit se retrouver sur la carte.
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, ascendingOnlySettings() };

    controller.startSession();

    for( int question = 0; question < static_cast<int>( SESSION_QUESTION_COUNT ); ++question )
    {
        const QVariantList positions = controller.gridPositions();

        ASSERT_EQ( domain::CIRCLE_OF_FIFTHS_SLOT_COUNT, static_cast<std::size_t>( positions.size() ) );

        // Une place pleine porte un identifiant a montrer, et une place vide n'en porte pas. C'est la seule chose
        // que l'ecran lira pour ecrire dans le bouton : si elle manque, l'ecran affiche des boutons vides - et
        // rien ne le dit.
        for( const QVariant & position : positions )
        {
            const QVariantMap map = position.toMap();

            if( map.value( "isEmpty" ).toBool() )
            {
                continue;
            }

            EXPECT_FALSE( map.value( "identifier" ).toString().isEmpty() )
              << "la place " << map.value( "slot" ).toInt() << " n'a pas d'identifiant a montrer. Cles : "
              << QStringList{ map.keys() }.join( ", " ).toStdString();
        }

        // Est-ce que cet intervalle se trouve bien sur la place de sa classe ?
        const auto isPlaced = [&positions]( std::int32_t p_semitones ) {
            const auto slot = static_cast<int>( domain::circleOfFifthsSlot( domain::Interval{ p_semitones } ) );

            for( const QVariant & position : positions )
            {
                const QVariantMap map = position.toMap();

                if( ( map.value( "slot" ).toInt() == slot )
                    && ( map.value( "semitones" ).toInt() == p_semitones ) )
                {
                    return true;
                }
            }

            return false;
        };

        for( const QVariant & choice : controller.choices() )
        {
            const std::int32_t semitones = choice.toMap().value( "semitones" ).toInt();

            EXPECT_TRUE( isPlaced( semitones ) ) << "l'intervalle de " << semitones << " demi-tons manque a sa place";
        }

        // La question est jouee jusqu'au verdict, pour que la suivante soit tiree a son tour.
        answerCorrectly( controller );
        controller.continueToNextQuestion();
    }
}

TEST( ExerciseSessionControllerTest, a_place_holds_every_octave_of_its_class )
{
    // Le cas precis rapporte par Roger, et il est traitre parce qu'il n'arrive qu'avec les intervalles composes :
    // quand l'unisson ET l'octave sont sur la table, ils visent la MEME place du cercle. La carte les ecrasait
    // l'un par l'autre, et le joueur n'avait plus rien de juste a cliquer.
    //
    // Ce test CONSTRUIT la situation au lieu de l'attendre au hasard : la carte entiere, donc les trois octaves
    // de la meme note sur la meme place.
    domain::NotePlayerFake notePlayer;

    domain::SessionSettings settings = ascendingOnlySettings();

    settings.startingPaletteSize = domain::SUPPORTED_INTERVAL_COUNT;
    settings.choiceCount = domain::SUPPORTED_INTERVAL_COUNT;

    ExerciseSessionController controller{ notePlayer, settings };

    controller.startSession();

    // L'unisson, l'octave et la quinzieme sont la meme note a une ou deux octaves pres : une seule place.
    const std::size_t unisonSlot = domain::circleOfFifthsSlot( domain::Interval{ 0 } );

    ASSERT_EQ( unisonSlot, domain::circleOfFifthsSlot( domain::Interval{ 12 } ) );
    ASSERT_EQ( unisonSlot, domain::circleOfFifthsSlot( domain::Interval{ 24 } ) );

    // Les trois boutons de cette place, dans l'ordre ou l'ecran les empile.
    std::vector<std::int32_t> stacked;

    for( const QVariant & position : controller.gridPositions() )
    {
        const QVariantMap map = position.toMap();

        if( map.value( "slot" ).toInt() == static_cast<int>( unisonSlot ) )
        {
            stacked.push_back( map.value( "semitones" ).toInt() );

            // Et chacun sait combien ils sont et ou il se place : c'est ce qui permet a l'ecran de les repartir
            // sans rien savoir de la musique.
            EXPECT_EQ( 3, map.value( "stackSize" ).toInt() );
        }
    }

    ASSERT_EQ( 3U, stacked.size() );
    EXPECT_EQ( 0, stacked.at( 0 ) );
    EXPECT_EQ( 12, stacked.at( 1 ) );
    EXPECT_EQ( 24, stacked.at( 2 ) );
}

TEST( ExerciseSessionControllerTest, a_guided_question_reports_its_kind_and_takes_a_direction )
{
    domain::NotePlayerFake notePlayer;

    domain::SessionSettings settings = ascendingOnlySettings();
    settings.directionQuestionShare = 100;

    ExerciseSessionController controller{ notePlayer, settings };

    controller.startSession();

    // Le mode guide est annonce a l'ecran...
    EXPECT_EQ( 1, controller.questionKind() );

    // ...et une reponse par direction est acceptee sans erreur, quelle qu'en soit la justesse.
    controller.answerDirection( 0 );
}

TEST( ExerciseSessionControllerTest, the_session_experience_joins_the_profile_total_at_the_end )
{
    domain::NotePlayerFake notePlayer;
    domain::PlayerPreferencesFake store;

    domain::SessionSettings settings = ascendingOnlySettings();

    settings.questionCount = 2;

    ExerciseSessionController controller{ notePlayer, settings, {}, {}, {}, &store };

    controller.startSession();

    // Deux questions jouees jusqu'au bout, toutes les deux justes.
    while( !controller.isFinished() )
    {
        answerCorrectly( controller );
        controller.continueToNextQuestion();
    }

    // Le total du profil a recu l'experience de la session, une fois et pas deux.
    EXPECT_GT( store.totalExperience(), 0 );

    // Et la session a ete comptee, une fois.
    EXPECT_EQ( 1, store.sessionCount() );
}

TEST( ExerciseSessionControllerTest, the_infinite_mode_has_no_lives )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, ascendingOnlySettings() };

    controller.startInfiniteSession();

    // Le mode infini, c'est exactement cela : aucune vie, donc aucune fin - juste enchainer.
    EXPECT_TRUE( controller.hasUnlimitedLives() );
}

TEST( ExerciseSessionControllerTest, the_survival_mode_keeps_its_lives )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, ascendingOnlySettings() };

    controller.startSurvivalSession();

    // Le survival, c'est l'arcade avec des vies : on garde celles du niveau, et la partie finit quand elles
    // tombent a zero.
    EXPECT_FALSE( controller.hasUnlimitedLives() );
}

// ---------------------------------------------------------------------------------------------------------------------
// The tuning
//
// Only the SETTING is checked here: which temperament is heard is the audio layer's business, and it is not wired
// yet. What matters at this level is that the choice survives, and that a value from QML that makes no sense is
// refused instead of stored.
// ---------------------------------------------------------------------------------------------------------------------

TEST( ExerciseSessionControllerTest, the_tuning_is_remembered_and_stays_in_range )
{
    domain::NotePlayerFake notePlayer;
    domain::PlayerPreferencesFake levelStore;

    ExerciseSessionController controller{ notePlayer, {}, {}, {}, {}, &levelStore };

    // Equal temperament by default: the reference, and what an ear has to learn first.
    EXPECT_EQ( static_cast<int>( domain::Temperament::Equal ), controller.temperament() );

    controller.setTemperament( static_cast<int>( domain::Temperament::Pythagorean ) );

    EXPECT_EQ( static_cast<int>( domain::Temperament::Pythagorean ), controller.temperament() );
    EXPECT_EQ( domain::Temperament::Pythagorean, levelStore.storedTemperament() );

    // A value that makes no sense is refused, not cast into a temperament that does not exist.
    controller.setTemperament( 99 );

    EXPECT_EQ( static_cast<int>( domain::Temperament::Pythagorean ), controller.temperament() );

    // And the names a screen shows come from the domain, one per temperament.
    EXPECT_EQ( static_cast<int>( domain::TEMPERAMENT_NAMES.size() ), controller.temperaments().size() );
}

}    // namespace musichien::ui