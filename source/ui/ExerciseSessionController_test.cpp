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

// Des questions d'intervalle, et RIEN d'autre : ni chant, ni rythme, ni mode guide.
//
// Les trois parts sont epinglees ENSEMBLE, et c'est le piege de ce fichier : une part laissee a sa valeur par defaut
// fait echouer un test une fois sur cinq, et un test qui echoue au hasard n'apprend rien a personne. Le meme piege
// existait deja pour le chant - voir l'en-tete.
[[nodiscard]] domain::SessionSettings intervalOnlySettings()
{
    domain::SessionSettings settings;

    settings.singQuestionShare = 0;
    settings.directionQuestionShare = 0;
    settings.rhythmQuestionShare = 0;
    settings.chordQuestionShare = 0;

    return settings;
}

// A session that ALWAYS goes up.
//
// The direction is drawn at random in a real session, so a test asserting "the question was heard as a
// melody" would fail roughly one time in five - and a test that fails at random teaches nothing except
// to be ignored. Pinning the direction is what makes these tests precise instead of tolerant.
[[nodiscard]] domain::SessionSettings ascendingOnlySettings()
{
    domain::SessionSettings settings = intervalOnlySettings();

    settings.ascendingShare = 100;
    settings.descendingShare = 0;
    settings.harmonicShare = 0;

    return settings;
}

// Une session qui ne pose QUE des questions de rythme : la ou le rythme se teste, il n'y a pas de place pour un
// intervalle tire au hasard.
[[nodiscard]] domain::SessionSettings rhythmOnlySettings()
{
    domain::SessionSettings settings;

    settings.singQuestionShare = 0;
    settings.directionQuestionShare = 0;
    settings.rhythmQuestionShare = 100;

    return settings;
}

// Une session qui ne pose QUE des questions d'accords.
[[nodiscard]] domain::SessionSettings chordOnlySettings()
{
    domain::SessionSettings settings;

    settings.singQuestionShare = 0;
    settings.directionQuestionShare = 0;
    settings.rhythmQuestionShare = 0;
    settings.chordQuestionShare = 100;

    return settings;
}

// Un profil qui ne veut QUE des intervalles.
//
// Les quatre parts de question sont epinglees ENSEMBLE : une seule laissee a sa valeur par defaut - vingt - et le test
// qui parle de la grille tombe sur une autre question une fois sur cinq. Le piege a coute cher une fois deja, et il
// grandit a chaque genre de question nouveau.
void storeIntervalOnlyShares( domain::PlayerPreferencesFake & p_store )
{
    p_store.storeSingQuestionShare( 0 );
    p_store.storeRhythmQuestionShare( 0 );
    p_store.storeChordQuestionShare( 0 );
}

// Une couleur d'accord qui n'est PAS la bonne, prise parmi celles que le joueur peut repondre.
[[nodiscard]] int wrongChordChoice( ExerciseSessionController & p_controller )
{
    const int correct = p_controller.heardChord().value( "quality" ).toInt();

    for( const QVariant & choice : p_controller.chordChoices() )
    {
        const int quality = choice.toMap().value( "quality" ).toInt();

        if( quality != correct )
        {
            return quality;
        }
    }

    return correct;
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

    // Ni chant ni rythme ni accords : ce test parle de la GRILLE, et une question d'un autre genre n'offre aucune grille
    // a regarder. Sans cet epeinglage, il echouerait une fois sur cinq - et un test qui echoue au hasard ne dit rien.
    storeIntervalOnlyShares( levelStore );

    ExerciseSessionController controller{ notePlayer, intervalOnlySettings(), {}, {}, {}, &levelStore };

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

    // Meme epeinglage que ci-dessus, et pour la meme raison : le niveau relit les parts de question du profil.
    storeIntervalOnlyShares( levelStore );

    ExerciseSessionController controller{ notePlayer, intervalOnlySettings(), {}, {}, {}, &levelStore };

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
    // Le bug que ce test surveille est silencieux : la carte ignorait un intervalle quand un AUTRE de la meme classe
    // occupait deja sa place. La question devenait alors impossible a repondre - le bouton affichait l'octave quand
    // l'unisson etait demande - et rien n'echouait.
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
    // Le cas est traitre parce qu'il n'arrive qu'avec les intervalles composes : quand l'unisson ET l'octave sont sur
    // la table, ils visent la MEME place du cercle. La carte les ecrasait l'un par l'autre, et le joueur n'avait plus
    // rien de juste a cliquer.
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

    // Ni chant, ni rythme, ni accords : le profil les relit tous, et ce test joue une session d'INTERVALLES jusqu'au
    // bout. Sans cet epeinglage, il tomberait sur une question qui ne se repond pas au doigt, et la session finirait
    // par epuisement des vies plutot que par ses deux questions.
    storeIntervalOnlyShares( store );

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

    // The root the tuning is heard from is remembered too, and it is a pitch class - C is 0, G is 7.
    controller.setTuningRoot( 7 );

    EXPECT_EQ( 7, controller.tuningRoot() );
    EXPECT_EQ( 7, levelStore.storedTuningRoot().pitchClassIndex() );

    controller.setTuningRoot( 99 );

    EXPECT_EQ( 7, controller.tuningRoot() );

    // The diapason is remembered too, and a value that makes no sense is refused.
    controller.setReferencePitch( 442.0 );

    EXPECT_EQ( 442.0, controller.referencePitch() );
    EXPECT_EQ( 442.0, levelStore.storedReferencePitch() );

    controller.setReferencePitch( 999.0 );

    EXPECT_EQ( 442.0, controller.referencePitch() );
}

TEST( ExerciseSessionControllerTest, the_sing_share_is_remembered_and_the_profile_can_be_reset )
{
    domain::NotePlayerFake notePlayer;
    domain::PlayerPreferencesFake levelStore;

    ExerciseSessionController controller{ notePlayer, {}, {}, {}, {}, &levelStore };

    // Twenty per cent by default, and the share can be changed and remembered.
    EXPECT_EQ( 20, controller.singQuestionShare() );

    controller.setSingQuestionShare( 40 );

    EXPECT_EQ( 40, controller.singQuestionShare() );
    EXPECT_EQ( 40, levelStore.storedSingQuestionShare() );

    // A value that makes no sense is refused.
    controller.setSingQuestionShare( 150 );

    EXPECT_EQ( 40, controller.singQuestionShare() );

    // The profile: some history, then wiped clean.
    levelStore.storeTotalExperience( 320 );
    levelStore.storeSessionCount( 5 );
    levelStore.storeStarCount( 2 );

    EXPECT_EQ( 320, controller.totalExperience() );

    controller.resetProfile();

    EXPECT_EQ( 0, controller.totalExperience() );
    EXPECT_EQ( 0, controller.sessionCount() );
    EXPECT_EQ( 0, controller.starCount() );
}

// ---------------------------------------------------------------------------------------------------------------------
// La question de rythme, vue par l'ecran
//
// Le jugement lui-meme appartient au domaine, et il y est teste sans horloge : ici, on verifie ce que l'ECRAN peut lire
// et entendre - qu'une cellule se fait ecouter avant d'etre reproduite, qu'une frappe s'entend, et qu'aucune de ces
// proprietes ne ment sur une question d'intervalle.
// ---------------------------------------------------------------------------------------------------------------------

TEST( ExerciseSessionControllerTest, a_rhythm_question_is_heard_as_a_cell_and_not_as_an_interval )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, rhythmOnlySettings() };

    controller.startSession();

    EXPECT_TRUE( controller.isRhythmQuestion() );
    EXPECT_EQ( 3, controller.questionKind() );

    // L'ecoute commence tout de suite : le premier temps de la cellule est frappe des l'ouverture de la question.
    EXPECT_GT( notePlayer.drumCount(), 0 );

    // Et AUCUN intervalle n'est joue : une question de rythme ne fait pas entendre de notes.
    EXPECT_TRUE( notePlayer.playedMelodies().empty() );
    EXPECT_TRUE( notePlayer.playedChords().empty() );
}

TEST( ExerciseSessionControllerTest, the_rhythm_properties_describe_the_cell_for_the_screen )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, rhythmOnlySettings() };

    controller.startSession();

    // Le nom vient du domaine, et c'est celui d'une cellule du domaine.
    EXPECT_FALSE( controller.rhythmPatternName().isEmpty() );

    EXPECT_EQ( 90, controller.rhythmBpm() );
    EXPECT_GT( controller.rhythmBeatsPerBar(), 0 );

    // Une frappe a dessiner par frappe de la cellule, et rien de plus : l'ecran place ce qu'on lui donne.
    EXPECT_GT( controller.rhythmOnsetCount(), 0 );
    EXPECT_EQ( controller.rhythmOnsetCount(), controller.rhythmHits().size() );

    const QVariantMap firstHit = controller.rhythmHits().first().toMap();

    EXPECT_TRUE( firstHit.contains( "beat" ) );
    EXPECT_TRUE( firstHit.contains( "accented" ) );
    EXPECT_TRUE( firstHit.contains( "drum" ) );

    // Une mesure dure ce que dit le domaine : l'ecran s'en sert pour laisser le feedback s'entendre en entier.
    EXPECT_GT( controller.rhythmCellDurationMs(), 0 );

    // Et l'ecran commence par l'ECOUTE : le doigt n'est pas juge tant que la cellule n'a pas ete entendue.
    EXPECT_FALSE( controller.isRhythmPlaying() );

    // Rien n'a encore ete frappe : -1, et non 0, qui est deja un Miss.
    EXPECT_EQ( -1, controller.rhythmLastQuality() );
}

TEST( ExerciseSessionControllerTest, a_tap_during_the_listening_phase_is_heard_but_not_judged )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, rhythmOnlySettings() };

    controller.startSession();

    const int drumsBefore = notePlayer.drumCount();

    controller.tapRhythm();

    // Le doigt s'entend toujours : sans ce son, taper donnerait l'impression que l'ecran n'a pas recu le geste.
    EXPECT_GT( notePlayer.drumCount(), drumsBefore );

    // Et il ne vaut RIEN tant que la cellule n'a pas ete entendue en entier : celui qui accompagne le modele pendant
    // qu'il s'ecoute ne perd pas une vie pour l'avoir suivi.
    EXPECT_FALSE( controller.isRhythmPlaying() );
    EXPECT_EQ( -1, controller.rhythmLastQuality() );
    EXPECT_EQ( 0, controller.rhythmCoveredOnsets() );
}

TEST( ExerciseSessionControllerTest, tapping_on_an_interval_question_changes_nothing )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, intervalOnlySettings() };

    controller.startSession();

    // La meme mesure de surete que le domaine : une frappe sur une question d'intervalle ne juge rien, et ne fait
    // aucun bruit - il n'y a pas de batterie derriere.
    controller.tapRhythm();

    EXPECT_EQ( 0, notePlayer.drumCount() );
    EXPECT_EQ( -1, controller.rhythmLastQuality() );

    // Et les proprietes de la cellule restent vides plutot que d'inventer un nom ou un tempo.
    EXPECT_FALSE( controller.isRhythmQuestion() );
    EXPECT_TRUE( controller.rhythmPatternName().isEmpty() );
    EXPECT_TRUE( controller.rhythmHits().isEmpty() );
    EXPECT_EQ( 0, controller.rhythmBeatsPerBar() );
    EXPECT_EQ( 0, controller.rhythmCellDurationMs() );
}

TEST( ExerciseSessionControllerTest, a_rhythm_question_shows_no_interval )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, rhythmOnlySettings() };

    controller.startSession();

    // La question porte un intervalle tire au hasard - c'est l'ordre des tirages - mais il n'a jamais ete joue : le
    // montrer ferait croire a un intervalle entendu, et l'ecran afficherait un verdict faux.
    EXPECT_TRUE( controller.heardInterval().isEmpty() );
    EXPECT_TRUE( controller.hintText().isEmpty() );
}

TEST( ExerciseSessionControllerTest, passing_a_rhythm_question_plays_the_cell_once )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, rhythmOnlySettings() };

    controller.startSession();

    const int drumsAfterListening = notePlayer.drumCount();

    controller.revealAnswer();

    // Passer la question laisse entendre ce qu'il fallait reproduire : la cellule est rejouee, et la question est close.
    EXPECT_GT( notePlayer.drumCount(), drumsAfterListening );
    EXPECT_TRUE( controller.isFeedbackVisible() );
}

TEST( ExerciseSessionControllerTest, leaving_a_rhythm_question_silences_the_loop )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, rhythmOnlySettings() };

    controller.startSession();

    controller.stopSession();

    // La boucle est un son comme un autre : quitter l'ecran la fait taire, et la session est bel et bien terminee.
    EXPECT_FALSE( controller.running() );
    EXPECT_GT( notePlayer.stopCount(), 0 );
    EXPECT_FALSE( controller.isRhythmQuestion() );
}

// ---------------------------------------------------------------------------------------------------------------------
// La question d'accord, vue par l'ecran
//
// Le domaine dit ce qu'est un accord ; ici, on verifie ce que l'ECRAN peut jouer, lire et renvoyer : un accord plaque,
// une poignee de couleurs nommees, et un index qui repart vers le domaine tel qu'il a ete affiche.
// ---------------------------------------------------------------------------------------------------------------------

TEST( ExerciseSessionControllerTest, a_chord_question_is_played_as_a_block )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, chordOnlySettings() };

    controller.startSession();

    EXPECT_TRUE( controller.isChordQuestion() );
    EXPECT_EQ( 4, controller.questionKind() );

    // PLAQUE, et pas arpege : un seul appel, plusieurs notes. C'est la couleur qu'on fait entendre, et c'est ce que le
    // joueur doit reconnaitre.
    ASSERT_EQ( 1, notePlayer.playedChords().size() );
    EXPECT_GE( notePlayer.playedChords().front().notes.size(), 3U );

    // Et aucune melodie : une suite de notes serait une autre question.
    EXPECT_TRUE( notePlayer.playedMelodies().empty() );
}

TEST( ExerciseSessionControllerTest, the_chord_choices_and_the_accord_are_ready_to_display )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, chordOnlySettings() };

    controller.startSession();

    const QVariantList choices = controller.chordChoices();

    // Deux couleurs au depart : majeur et mineur. L'ecran affiche la liste telle quelle, et renvoie l'index choisi.
    ASSERT_EQ( 2, choices.size() );

    const QVariantMap firstChoice = choices.first().toMap();

    EXPECT_TRUE( firstChoice.contains( "quality" ) );
    EXPECT_FALSE( firstChoice.value( "name" ).toString().isEmpty() );

    // LE NOM DU BOUTON est la notation anglo-saxonne : la tonique de la question, plus le suffixe de la couleur. « Maj »
    // et « min » ont disparu, et avec eux le numero de notes : un majeur ne s'annonce pas, donc son bouton dit juste la
    // tonique - « C ».
    const QString rootName = controller.heardChord().value( "rootName" ).toString();

    EXPECT_EQ( rootName, firstChoice.value( "name" ).toString() );

    // Et le mineur, juste a cote, ajoute son « m » : « Cm ».
    const QVariantMap secondChoice = choices.at( 1 ).toMap();

    EXPECT_EQ( rootName + QStringLiteral( "m" ), secondChoice.value( "name" ).toString() );

    // L'accord entendu est decrit pour l'ecran, symbole compris : "C" + "m" font "Cm", et l'ecran n'assemble rien.
    const QVariantMap heard = controller.heardChord();

    EXPECT_FALSE( heard.value( "name" ).toString().isEmpty() );
    EXPECT_FALSE( heard.value( "symbol" ).toString().isEmpty() );
    EXPECT_FALSE( heard.value( "rootName" ).toString().isEmpty() );

    // Et rien n'a encore ete repondu : un verdict qui parlerait d'une reponse qui n'existe pas serait un mensonge.
    EXPECT_TRUE( controller.answeredChord().isEmpty() );
}

TEST( ExerciseSessionControllerTest, naming_the_heard_colour_scores_and_the_accord_is_heard_again )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, chordOnlySettings() };

    controller.startSession();

    const int quality = controller.heardChord().value( "quality" ).toInt();

    controller.answerChord( quality );

    EXPECT_TRUE( controller.isFeedbackVisible() );
    EXPECT_TRUE( controller.wasLastAnswerCorrect() );

    // La confirmation est l'accord lui-meme, rejoue : deux appels plaques au total.
    EXPECT_EQ( 2, notePlayer.playedChords().size() );

    // Et la couleur nommee est retenue, pour que le verdict puisse la montrer.
    EXPECT_EQ( quality, controller.answeredChord().value( "quality" ).toInt() );
}

TEST( ExerciseSessionControllerTest, an_index_that_is_not_a_colour_is_refused )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, chordOnlySettings() };

    controller.startSession();

    // Un ecran qui inventerait un index ne doit pas pouvoir repondre : la question reste posee, et rien ne bouge.
    controller.answerChord( 99 );
    controller.answerChord( -1 );

    EXPECT_TRUE( controller.isAsking() );
    EXPECT_TRUE( controller.answeredChord().isEmpty() );
}

TEST( ExerciseSessionControllerTest, an_interval_question_offers_no_chord )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, intervalOnlySettings() };

    controller.startSession();

    // Les proprietes d'accord sont vides plutot que fausses : un ecran qui les lirait par erreur n'afficherait rien.
    EXPECT_FALSE( controller.isChordQuestion() );
    EXPECT_TRUE( controller.chordChoices().isEmpty() );
    EXPECT_TRUE( controller.heardChord().isEmpty() );

    // Et repondre une couleur ne juge rien.
    controller.answerChord( static_cast<int>( domain::ChordQuality::Minor ) );

    EXPECT_TRUE( controller.isAsking() );
}

TEST( ExerciseSessionControllerTest, the_four_question_shares_are_remembered_and_stay_in_range )
{
    domain::NotePlayerFake notePlayer;
    domain::PlayerPreferencesFake levelStore;

    ExerciseSessionController controller{ notePlayer, {}, {}, {}, {}, &levelStore };

    // Vingt pour cent par defaut pour le chant et les accords... et ZERO pour le rythme : la question de rythme est
    // eteinte tant qu'on ne l'allume pas (sa mesure du temps n'est pas encore fiable).
    EXPECT_EQ( 20, controller.singQuestionShare() );
    EXPECT_EQ( 0, controller.rhythmQuestionShare() );
    EXPECT_EQ( 20, controller.chordQuestionShare() );

    // On l'allume : le reglage existe, et c'est ce qui compte - le rythme se dose, y compris depuis zero.
    controller.setRhythmQuestionShare( 25 );

    EXPECT_EQ( 25, controller.rhythmQuestionShare() );
    EXPECT_EQ( 25, levelStore.storedRhythmQuestionShare() );

    controller.setChordQuestionShare( 45 );

    EXPECT_EQ( 45, controller.chordQuestionShare() );
    EXPECT_EQ( 45, levelStore.storedChordQuestionShare() );

    // Et une valeur qui n'a pas de sens est refusee, pas convertie.
    controller.setRhythmQuestionShare( 150 );
    controller.setChordQuestionShare( -3 );

    EXPECT_EQ( 25, controller.rhythmQuestionShare() );
    EXPECT_EQ( 45, controller.chordQuestionShare() );

    // ZERO est une valeur legitime, et c'est meme celle par defaut : elle fait disparaitre le rythme d'une session.
    controller.setRhythmQuestionShare( 0 );

    EXPECT_EQ( 0, controller.rhythmQuestionShare() );
    EXPECT_EQ( 0, levelStore.storedRhythmQuestionShare() );
}

// ---------------------------------------------------------------------------------------------------------------------
// Le journal des questions conclues
//
// C'est la fondation des statistiques : une ligne par question CONCLUE, et rien de plus. Le journal est un port, donc ce
// fichier y branche un journal EN MEMOIRE et regarde ce qui part - exactement ce que l'application fait avec un
// fichier.
// ---------------------------------------------------------------------------------------------------------------------

TEST( ExerciseSessionControllerTest, a_concluded_question_is_written_to_the_journal )
{
    domain::NotePlayerFake notePlayer;
    domain::QuestionLogFake log;

    ExerciseSessionController controller{ notePlayer, chordOnlySettings() };
    controller.setQuestionLog( &log );

    controller.startSession();

    // Rien tant que la question n'est pas conclue : une question posee n'est pas encore une lecon.
    EXPECT_EQ( 0U, log.size() );

    const int correctQuality = controller.heardChord().value( "quality" ).toInt();

    controller.answerChord( correctQuality );

    ASSERT_EQ( 1U, log.size() );

    const domain::QuestionRecord & record = log.records().front();

    EXPECT_EQ( domain::QuestionKind::Chord, record.kind );
    EXPECT_EQ( correctQuality, record.target );
    EXPECT_EQ( domain::QuestionOutcome::CorrectFirstTry, record.outcome );
    EXPECT_EQ( 1, record.attemptCount );
}

TEST( ExerciseSessionControllerTest, a_wrong_answer_that_leaves_the_question_open_writes_nothing )
{
    domain::NotePlayerFake notePlayer;
    domain::QuestionLogFake log;

    domain::SessionSettings settings = chordOnlySettings();
    settings.lives = std::nullopt;    // pour que la question reste posee apres l'erreur

    ExerciseSessionController controller{ notePlayer, settings };
    controller.setQuestionLog( &log );

    controller.startSession();

    controller.answerChord( wrongChordChoice( controller ) );

    // La question est TOUJOURS POSEE : elle se retente, et c'est la tentative finale qui compte. Ecrire ici ferait
    // compter trois fois la meme question pour une seule lecon.
    EXPECT_TRUE( controller.isAsking() );
    EXPECT_EQ( 0U, log.size() );

    controller.answerChord( controller.heardChord().value( "quality" ).toInt() );

    // Conclue au deuxieme essai : une ligne, et elle dit « trouve », pas « su ».
    ASSERT_EQ( 1U, log.size() );
    EXPECT_EQ( domain::QuestionOutcome::CorrectAfterRetries, log.records().front().outcome );
    EXPECT_EQ( 2, log.records().front().attemptCount );
}

TEST( ExerciseSessionControllerTest, answering_twice_writes_one_line )
{
    domain::NotePlayerFake notePlayer;
    domain::QuestionLogFake log;

    ExerciseSessionController controller{ notePlayer, chordOnlySettings() };
    controller.setQuestionLog( &log );

    controller.startSession();

    const int correctQuality = controller.heardChord().value( "quality" ).toInt();

    controller.answerChord( correctQuality );
    controller.answerChord( correctQuality );    // refusee : la question est deja en feedback

    EXPECT_EQ( 1U, log.size() );
}

TEST( ExerciseSessionControllerTest, a_revealed_question_is_recorded_as_help_not_as_a_mistake )
{
    domain::NotePlayerFake notePlayer;
    domain::QuestionLogFake log;

    ExerciseSessionController controller{ notePlayer, chordOnlySettings() };
    controller.setQuestionLog( &log );

    controller.startSession();
    controller.revealAnswer();

    ASSERT_EQ( 1U, log.size() );

    // Vue la reponse : ni une reussite, ni un echec. Un joueur qui demande la reponse n'est pas un joueur qui se
    // trompe, et ses statistiques lui mentiraient s'il y apparaissait comme tel.
    EXPECT_EQ( domain::QuestionOutcome::Revealed, log.records().front().outcome );
}

TEST( ExerciseSessionControllerTest, a_session_without_a_journal_still_plays )
{
    domain::NotePlayerFake notePlayer;

    // AUCUN journal branche : c'est le cas d'un test, et celui d'un appareil dont le disque est plein. Le jeu doit
    // fonctionner exactement pareil - des statistiques, pas une regle du jeu.
    ExerciseSessionController controller{ notePlayer, chordOnlySettings() };

    controller.startSession();
    controller.answerChord( controller.heardChord().value( "quality" ).toInt() );

    EXPECT_TRUE( controller.isFeedbackVisible() );
    EXPECT_TRUE( controller.wasLastAnswerCorrect() );
}

// ---------------------------------------------------------------------------------------------------------------------
// Le Bilan
//
// Une session dont les questions sont DECIDEES, du plus facile au plus difficile. Le plan vient des STATISTIQUES : ce
// fichier remplit donc un journal EN MEMOIRE, puis regarde ce que le bilan en fait.
// ---------------------------------------------------------------------------------------------------------------------

namespace
{

// Un journal qui a de la matiere : des cibles sues, et des cibles qui resistent.
//
// Les cibles sont des INTERVALLES : c'est le triple (genre, cible, direction) qui fait une cible, et six d'entre elles
// suffisent a ce qu'un « facile puis difficile » ait un sens.
void fillJournalWithWorkedTargets( domain::QuestionLogFake & p_log )
{
    const auto now = std::chrono::system_clock::now();

    const auto add = [&p_log, &now]( std::int32_t p_semitones, bool p_correct, int p_count ) {
        for( int index = 0; index < p_count; ++index )
        {
            domain::QuestionRecord record;

            record.askedAt = now - std::chrono::hours{ 1 };
            record.kind = domain::QuestionKind::NamedInterval;
            record.target = p_semitones;
            record.direction = domain::IntervalDirection::Ascending;
            record.outcome = p_correct ? domain::QuestionOutcome::CorrectFirstTry : domain::QuestionOutcome::Failed;

            p_log.append( record );
        }
    };

    add( 12, true, 5 );
    add( 7, true, 5 );
    add( 4, true, 3 );

    add( 2, false, 5 );
    add( 6, false, 5 );
    add( 11, false, 3 );
}

}    // namespace

TEST( ExerciseSessionControllerTest, a_review_session_plans_its_questions )
{
    domain::NotePlayerFake notePlayer;
    domain::QuestionLogFake log;

    fillJournalWithWorkedTargets( log );

    ExerciseSessionController controller{ notePlayer, intervalOnlySettings() };
    controller.setQuestionLog( &log );

    controller.startReviewSession();

    EXPECT_TRUE( controller.running() );
    EXPECT_TRUE( controller.isReviewRunning() );

    // Le bilan est FINI : ses questions sont decidees, donc son compte est celui du plan - et il ne se perd pas, puisqu'un
    // bilan sans vies ne peut pas s'arreter au milieu.
    EXPECT_GT( controller.questionCount(), 0 );
    EXPECT_LT( controller.questionCount(), 10 );
    EXPECT_TRUE( controller.hasUnlimitedLives() );
}

TEST( ExerciseSessionControllerTest, a_review_session_without_a_journal_is_an_ordinary_game )
{
    domain::NotePlayerFake notePlayer;

    // Aucun journal : il n'y a rien a reviser. Le bilan devient une partie ordinaire plutot que de refuser de s'ouvrir -
    // un bouton qui ne fait rien est pire qu'un bouton qui fait autre chose.
    ExerciseSessionController controller{ notePlayer, intervalOnlySettings() };

    controller.startReviewSession();

    EXPECT_TRUE( controller.running() );
    EXPECT_FALSE( controller.isReviewRunning() );
    EXPECT_EQ( SESSION_QUESTION_COUNT, controller.questionCount() );
}

TEST( ExerciseSessionControllerTest, the_encouragement_speaks_only_during_a_review )
{
    domain::NotePlayerFake notePlayer;
    domain::QuestionLogFake log;

    fillJournalWithWorkedTargets( log );

    ExerciseSessionController controller{ notePlayer, intervalOnlySettings() };
    controller.setQuestionLog( &log );

    // Une partie ordinaire ne dit RIEN : l'ecran reste silencieux, et c'est ce qui donne du poids aux mots du bilan.
    controller.startSession();

    EXPECT_TRUE( controller.encouragementText().isEmpty() );

    controller.stopSession();
    controller.startReviewSession();

    // Le bilan, lui, parle - au minimum quand il attaque ce qui resiste.
    bool spokeAtSomePoint = false;

    for( int question = 0; question < controller.questionCount(); ++question )
    {
        if( !controller.encouragementText().isEmpty() )
        {
            spokeAtSomePoint = true;

            break;
        }

        if( controller.isChordQuestion() )
        {
            controller.answerChord( controller.heardChord().value( "quality" ).toInt() );
        }
        else
        {
            controller.answer( heardDistance( controller ) );
        }

        controller.continueToNextQuestion();
    }

    EXPECT_TRUE( spokeAtSomePoint ) << "le bilan n'a jamais encourage le joueur";
}

TEST( ExerciseSessionControllerTest, resetting_the_profile_also_wipes_the_statistics )
{
    domain::NotePlayerFake notePlayer;
    domain::PlayerPreferencesFake levelStore;
    domain::QuestionLogFake log;

    storeIntervalOnlyShares( levelStore );
    levelStore.storeChordQuestionShare( 100 );

    ExerciseSessionController controller{ notePlayer, chordOnlySettings(), {}, {}, {}, &levelStore };
    controller.setQuestionLog( &log );

    controller.startSession();
    controller.answerChord( controller.heardChord().value( "quality" ).toInt() );

    ASSERT_EQ( 1U, log.size() );

    controller.resetProfile();

    // Un score a zero qui garderait son journal serait un demi-mensonge : la page de statistiques continuerait de
    // raconter une histoire que le joueur vient d'effacer.
    EXPECT_EQ( 0U, log.size() );
}

TEST( ExerciseSessionControllerTest, the_chord_hint_removes_a_choice_from_the_screen )
{
    domain::NotePlayerFake notePlayer;

    domain::SessionSettings settings = chordOnlySettings();

    // Six couleurs : il faut de quoi retirer un leurre. Avec deux couleurs - le niveau d'un debutant - la question se
    // resoudrait au premier essai, et l'indice n'aurait rien a enlever.
    settings.startingChordQualityCount = 6;

    ExerciseSessionController controller{ notePlayer, settings };

    controller.startSession();

    // Rien avant d'avoir essaye : l'indice attend un premier essai rate.
    EXPECT_FALSE( controller.isChordHintAvailable() );

    controller.answerChord( wrongChordChoice( controller ) );

    // Une erreur ne change pas la question - elle se retente - et l'indice est desormais propose.
    ASSERT_TRUE( controller.isAsking() );
    EXPECT_TRUE( controller.isChordHintAvailable() );

    const int choiceCountBefore = controller.chordChoices().size();

    controller.useChordHint();

    // L'ECRAN a une reponse en moins : la grille se relit depuis le domaine, donc retirer la reponse retire le bouton.
    EXPECT_EQ( choiceCountBefore - 1, controller.chordChoices().size() );

    // Et la bonne reponse est toujours proposee : un indice qui la retirerait rendrait la question impossible.
    const int correctQuality = controller.heardChord().value( "quality" ).toInt();

    bool correctChoiceIsStillOffered = false;

    for( const QVariant & choice : controller.chordChoices() )
    {
        if( choice.toMap().value( "quality" ).toInt() == correctQuality )
        {
            correctChoiceIsStillOffered = true;
        }
    }

    EXPECT_TRUE( correctChoiceIsStillOffered );

    // Et la reponse reste juste : retirer un leurre ne change pas la question.
    controller.answerChord( correctQuality );

    EXPECT_TRUE( controller.wasLastAnswerCorrect() );
}

TEST( ExerciseSessionControllerTest, the_arpeggio_is_offered_even_when_nothing_can_be_removed )
{
    domain::NotePlayerFake notePlayer;

    // DEUX couleurs, donc rien a retirer : c'est le niveau d'un debutant, et c'est exactement le cas ou l'arpege compte
    // le plus.
    domain::SessionSettings settings = chordOnlySettings();
    settings.startingChordQualityCount = 2;

    ExerciseSessionController controller{ notePlayer, settings };

    controller.startSession();
    controller.answerChord( wrongChordChoice( controller ) );

    EXPECT_FALSE( controller.isChordHintAvailable() );
    EXPECT_TRUE( controller.isChordArpeggioAvailable() );
}

TEST( ExerciseSessionControllerTest, the_chord_tree_is_ready_to_be_drawn )
{
    domain::NotePlayerFake notePlayer;

    ExerciseSessionController controller{ notePlayer, chordOnlySettings() };

    const QVariantList tree = controller.chordTree();

    // Quinze couleurs, comme le domaine : l'arbre est la CARTE de tout ce que le jeu sait jouer, et il ne doit pas en
    // oublier une - ce serait une branche manquante sur un arbre de competences.
    ASSERT_EQ( 15, tree.size() );

    const QVariantMap root = tree.first().toMap();

    EXPECT_TRUE( root.value( QStringLiteral( "isRoot" ) ).toBool() );
    EXPECT_EQ( -1, root.value( QStringLiteral( "parentIndex" ) ).toInt() );
    EXPECT_EQ( 0, root.value( QStringLiteral( "depth" ) ).toInt() );

    // La racine est un do majeur : « C », et ses degres sont 1 3 5.
    EXPECT_EQ( QStringLiteral( "C" ), root.value( QStringLiteral( "name" ) ).toString() );
    EXPECT_EQ( QStringLiteral( "1 3 5" ), root.value( QStringLiteral( "degrees" ) ).toString() );

    // Et le mineur, juste en dessous, s'appelle « Cm » : le nom anglo-saxon, sur la carte comme sur les boutons.
    EXPECT_EQ( QStringLiteral( "Cm" ), tree.at( 1 ).toMap().value( QStringLiteral( "name" ) ).toString() );

    for( int index = 1; index < tree.size(); ++index )
    {
        const QVariantMap node = tree.at( index ).toMap();

        // Chaque noeud sait de QUI il descend, et son parent vient AVANT lui : l'ecran trace un trait entre deux
        // positions, il lui faut les deux - et une branche qui remonterait la liste ne se dessinerait pas.
        EXPECT_GE( node.value( QStringLiteral( "parentIndex" ) ).toInt(), 0 );
        EXPECT_LT( node.value( QStringLiteral( "parentIndex" ) ).toInt(), index );

        // Et chaque noeud porte son geste et ses degres : ce sont eux qui apprennent quelque chose.
        EXPECT_FALSE( node.value( QStringLiteral( "mutation" ) ).toString().isEmpty() );
        EXPECT_FALSE( node.value( QStringLiteral( "degrees" ) ).toString().isEmpty() );
    }

    // La couleur vient du CONTROLEUR, pour que tous les ecrans peignent la meme : deux teintes differentes pour le meme
    // accord, et le code couleur n'apprendrait plus rien.
    EXPECT_FALSE( controller.chordColourName( 0 ).isEmpty() );
    EXPECT_NE( controller.chordColourName( 0 ), controller.chordColourName( 1 ) );
}

TEST( ExerciseSessionControllerTest, a_first_run_offers_a_piano_and_a_guitar )
{
    domain::NotePlayerFake notePlayer;
    domain::PlayerPreferencesFake levelStore;

    // Un profil VIERGE : c'est un premier lancement.
    ExerciseSessionController controller{ notePlayer, intervalOnlySettings(), {}, {}, {}, &levelStore };

    const QVariantList instruments = controller.instruments();

    ASSERT_EQ( 6, instruments.size() );

    // Un premier lancement doit sonner JUSTE : piano et guitare, deux sons neutres, et le reste a portee de reglage.
    EXPECT_TRUE( instruments.at( 0 ).toMap().value( QStringLiteral( "enabled" ) ).toBool() );
    EXPECT_TRUE( instruments.at( 1 ).toMap().value( QStringLiteral( "enabled" ) ).toBool() );

    for( int index = 2; index < instruments.size(); ++index )
    {
        EXPECT_FALSE( instruments.at( index ).toMap().value( QStringLiteral( "enabled" ) ).toBool() )
          << "instrument " << index << " allume au premier lancement";
    }
}
TEST( ExerciseSessionControllerTest, the_daily_reminder_is_active_at_the_first_launch )
{
    domain::NotePlayerFake notePlayer;
    domain::PlayerPreferencesFake levelStore;

    ExerciseSessionController controller{ notePlayer, intervalOnlySettings(), {}, {}, {}, &levelStore };

    // Un premier lancement a le rappel ACTIF, sans rien avoir a cocher. Une application d'oreille musicale qui ne se
    // rappelle a personne est une application qu'on oublie - et c'est exactement ce que le rappel existe pour eviter.
    EXPECT_TRUE( controller.dailyReminderEnabled() );

    // Et celui qui le coupe garde son choix : le reglage s'ecrit, et il se relit.
    controller.setDailyReminderEnabled( false );

    EXPECT_FALSE( controller.dailyReminderEnabled() );
    EXPECT_FALSE( levelStore.dailyReminderEnabled() );
}

}    // namespace musichien::ui