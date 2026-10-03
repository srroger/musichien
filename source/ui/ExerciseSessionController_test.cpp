#include "ui/ExerciseSessionController.h"

#include "domain/audio/NotePlayerFake.h"
#include "domain/music/PhraseBook.h"
#include "domain/music/Temperament.h"
#include "ui/IntervalDescription.h"
#include "ui/MicrophoneController.h"

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
// Les parts sont des POIDS, lus les uns par rapport aux autres : une seule oubliee suffit a voler des questions. Le
// meme piege que pour les profils, et la raison pour laquelle tous les helpers de ce fichier commencent par celui-ci.
void pinEveryQuestionShare( domain::SessionSettings & p_settings )
{
    p_settings.namedIntervalQuestionShare = 0;
    p_settings.sameColourQuestionShare = 0;
    p_settings.singQuestionShare = 0;
    p_settings.directionQuestionShare = 0;
    p_settings.chordQuestionShare = 0;
    p_settings.modeColourQuestionShare = 0;
    p_settings.modeNameQuestionShare = 0;
    p_settings.modeVampQuestionShare = 0;

    // ET LA NOTE ETRANGERE, qui manquait ici comme elle manquait a isIntervalQuestion.
    //
    // Elle est passee inapercue tant que sa part valait ZERO par defaut : le helper n'avait rien a eteindre. Depuis que
    // l'harmonie s'ouvre par defaut (voir SessionSettings, 01/10/2026), une session censee ne poser que des intervalles
    // tombait une fois sur plusieurs sur une question de note etrangere - et les tests d'intervalle se mettaient a echouer
    // pour une raison qui n'avait rien a voir avec ce qu'ils verifiaient.
    p_settings.foreignNoteQuestionShare = 0;
}

[[nodiscard]] domain::SessionSettings intervalOnlySettings()
{
    domain::SessionSettings settings;
    pinEveryQuestionShare( settings );

    return settings;
}

// Une session qui ne pose QUE des questions chantees : la seule ou l'ecart du chant a un sens.
[[nodiscard]] domain::SessionSettings singOnlySettings()
{
    domain::SessionSettings settings;
    pinEveryQuestionShare( settings );

    settings.singQuestionShare = 100;

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
    pinEveryQuestionShare( settings );

    return settings;
}

// Une session qui ne pose QUE des questions d'accords.
[[nodiscard]] domain::SessionSettings chordOnlySettings()
{
    domain::SessionSettings settings;
    pinEveryQuestionShare( settings );

    settings.chordQuestionShare = 100;

    return settings;
}

// Un profil qui ne veut QUE des intervalles A NOMMER.
//
// Les parts de question sont epinglees ENSEMBLE : une seule laissee a sa valeur par defaut - vingt, soixante pour
// « nommer » - et le test qui parle de la grille tombe sur une autre question. Le piege a coute cher une fois deja, et
// il grandit a chaque genre de question nouveau : il vient encore de mordre, quand « nommer » a recu la part qui lui
// manquait et que ce helper ne la connaissait pas encore.
void storeIntervalOnlyShares( domain::PlayerPreferencesFake & p_store )
{
    p_store.storeNamedIntervalQuestionShare( 100 );
    p_store.storeSingQuestionShare( 0 );
    p_store.storeChordQuestionShare( 0 );
    p_store.storeModeColourQuestionShare( 0 );
    p_store.storeModeNameQuestionShare( 0 );
    p_store.storeModeVampQuestionShare( 0 );
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

// Un profil qui ne veut QUE des questions de mode, et NOMMEES : la seule forme ou l'ecran connait la reponse sans
// attendre un minuteur.
//
// Toutes les parts de question sont epinglees ENSEMBLE, et les trois parts d'harmonie le sont aussi : une seule laissee
// a sa valeur par defaut, et le test tombe sur un autre genre une fois sur cinq.
// Combien d'intervalles sont coches dans la page du GodMode. C'est la mesure de ce que le joueur a en main, et elle ne
// depend pas du nombre de boutons que la grille affiche.
[[nodiscard]] int checkedIntervalCount( const ExerciseSessionController & p_controller )
{
    int checked = 0;

    for( const QVariant & entry : p_controller.godModeIntervals() )
    {
        if( entry.toMap().value( QStringLiteral( "checked" ) ).toBool() )
        {
            ++checked;
        }
    }

    return checked;
}

void storeNamedModeOnlyShares( domain::PlayerPreferencesFake & p_store )
{
    p_store.storeNamedIntervalQuestionShare( 0 );
    p_store.storeSingQuestionShare( 0 );
    p_store.storeChordQuestionShare( 0 );
    p_store.storeModeColourQuestionShare( 0 );
    p_store.storeModeNameQuestionShare( 100 );
    p_store.storeModeVampQuestionShare( 0 );
}

// Ne garder QUE les intervalles nommes : c'est le seul genre dont la grille de reponse soit faite de la palette, donc le
// seul qui permette de lire ce que la partie joue vraiment.
//
// Sans cela, une part d'accord restee ouverte par defaut ferait poser une question d'accord, dont la grille est vide - et
// un test qui lit cette grille croirait a une palette vide plutot qu'a un autre genre de question.
void storeNamedIntervalOnlyShares( domain::PlayerPreferencesFake & p_store )
{
    p_store.storeNamedIntervalQuestionShare( 100 );
    p_store.storeSingQuestionShare( 0 );
    p_store.storeForeignNoteQuestionShare( 0 );
    p_store.storeChordQuestionShare( 0 );
    p_store.storeModeColourQuestionShare( 0 );
    p_store.storeModeNameQuestionShare( 0 );
    p_store.storeModeVampQuestionShare( 0 );
}

void answerCorrectly( ExerciseSessionController & p_controller )
{
    p_controller.answer( heardDistance( p_controller ) );
}

}    // namespace

// L'ecart du chant est RETENU, et pas relu sur le micro.
//
// Repondre resynchronise la cible du micro au debut du traitement de la reponse (voir refreshChoices), ce qui remet
// son detecteur a zero. Une premiere version relisait l'ecart au moment d'afficher le verdict : la mesure etait donc
// toujours nulle, et le joueur ne voyait jamais de combien il avait ete juste. Le defaut ne se voyait pas dans les
// tests du domaine, qui jugent le detecteur, et pas l'ecran.
TEST( ExerciseSessionControllerTest, the_sung_offset_is_kept_even_though_the_microphone_is_reset )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, singOnlySettings() };
    MicrophoneController microphone{ QStringList{}, {}, nullptr, &notePlayer };

    controller.setMicrophoneController( &microphone );
    controller.startOrdinarySession();

    ASSERT_TRUE( controller.isAsking() );
    ASSERT_EQ( 2, controller.questionKind() );

    controller.answerSung( true, 25 );

    EXPECT_EQ( 25, controller.lastSungCentsOffset() );
}

TEST( ExerciseSessionControllerTest, a_session_without_singing_keeps_no_offset )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, intervalOnlySettings() };

    controller.startOrdinarySession();

    // Aucun chant n'a ete juge : l'ecran doit lire zero, et non une mesure inventee - ou, pire, celle du chant
    // precedent restee en memoire.
    EXPECT_EQ( 0, controller.lastSungCentsOffset() );
}

TEST( ExerciseSessionControllerTest, starting_a_session_asks_the_first_question )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, ascendingOnlySettings() };

    EXPECT_FALSE( controller.running() );

    controller.startOrdinarySession();

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

    controller.startOrdinarySession();

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

    controller.startOrdinarySession();
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

    controller.startOrdinarySession();

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

    controller.startOrdinarySession();
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

    controller.startOrdinarySession();

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

    controller.startOrdinarySession();

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

    controller.startOrdinarySession();

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

    controller.startOrdinarySession();

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

    controller.startOrdinarySession();
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

    controller.startOrdinarySession();
    answerCorrectly( controller );

    EXPECT_EQ( 0, notePlayer.mistakeCueCount() );
    EXPECT_EQ( 0, vibrationCount );
}

TEST( ExerciseSessionControllerTest, a_device_that_cannot_vibrate_still_hears_the_cue )
{
    // No callback at all, which is what a development machine honestly is. Nothing must depend on it.
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, ascendingOnlySettings() };

    controller.startOrdinarySession();
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

    controller.startOrdinarySession();

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

    controller.startOrdinarySession();
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

    controller.startOrdinarySession();
    controller.answer( heardDistance( controller ) + 1 );

    EXPECT_TRUE( controller.hintText().isEmpty() );
}

// Un profil qui ne veut QUE des questions de couleur (le degrade).
void storeColourOnlyShares( domain::PlayerPreferencesFake & p_store )
{
    p_store.storeNamedIntervalQuestionShare( 0 );
    p_store.storeSingQuestionShare( 0 );
    p_store.storeChordQuestionShare( 0 );
    p_store.storeModeColourQuestionShare( 100 );
    p_store.storeModeNameQuestionShare( 0 );
    p_store.storeModeVampQuestionShare( 0 );
}

// Un profil qui ne veut QUE des questions de note etrangere.
void storeForeignNoteOnlyShares( domain::PlayerPreferencesFake & p_store )
{
    p_store.storeNamedIntervalQuestionShare( 0 );
    p_store.storeSingQuestionShare( 0 );
    p_store.storeChordQuestionShare( 0 );
    p_store.storeModeColourQuestionShare( 0 );
    p_store.storeModeNameQuestionShare( 0 );
    p_store.storeModeVampQuestionShare( 0 );
    p_store.storeForeignNoteQuestionShare( 100 );
}

TEST( ExerciseSessionControllerTest, a_share_of_zero_in_the_profile_never_poses_that_kind )
{
    // Le chemin REEL de l'application, celui que Roger emprunte : le profil, puis le controleur, puis la session. Le
    // domaine est deja verifie de son cote, mais c'est ce chemin-la qui decide ce que le joueur voit - et Roger voit des
    // questions de mode alors que les trois parts de mode sont a ZERO : « j'ai beau mettre plus clair et plus sombre a 0,
    // je l'obtiens toujours dans mes parties ».
    domain::NotePlayerFake notePlayer;
    domain::PlayerPreferencesFake levelStore;

    levelStore.storeNamedIntervalQuestionShare( 60 );
    levelStore.storeSingQuestionShare( 20 );
    levelStore.storeChordQuestionShare( 20 );
    levelStore.storeModeColourQuestionShare( 0 );
    levelStore.storeModeNameQuestionShare( 0 );
    levelStore.storeModeVampQuestionShare( 0 );
    levelStore.storeForeignNoteQuestionShare( 0 );

    int modeQuestionCount = 0;

    constexpr std::uint32_t SEED_COUNT = 200;

    for( std::uint32_t seed = 1; seed <= SEED_COUNT; ++seed )
    {
        ExerciseSessionController controller{ notePlayer, {}, {}, {}, {}, &levelStore };

        controller.choosePlayerLevel( static_cast<int>( domain::PlayerLevel::Advanced ) );
        controller.startOrdinarySession();

        if( controller.isModeQuestion() )
        {
            ++modeQuestionCount;
        }
    }

    // ZERO, et c'est tout l'objet du test : une porte fermee dans le profil doit l'etre dans la partie.
    EXPECT_EQ( 0, modeQuestionCount );
}

TEST( ExerciseSessionControllerTest, a_realistic_share_poses_foreign_note_questions )
{
    // La question que Roger se pose : « je n'arrive pas a acceder au nouvel exercice ». Le chemin du REGLAGE vers la
    // QUESTION est donc verifie a une part realiste - trente, celle qu'un joueur pose vraiment - et pas a cent : une part
    // de cent marcherait meme si le partage entre les genres etait casse.
    //
    // Les trois parts de reference restent la, comme chez un joueur qui les a laissees : soixante pour nommer, vingt pour
    // le chant, vingt pour les accords. Trente sur cent trente, c'est un peu moins d'un quart des questions.
    domain::NotePlayerFake notePlayer;
    domain::PlayerPreferencesFake levelStore;

    levelStore.storeNamedIntervalQuestionShare( 60 );
    levelStore.storeSingQuestionShare( 20 );
    levelStore.storeChordQuestionShare( 20 );
    levelStore.storeModeColourQuestionShare( 0 );
    levelStore.storeModeNameQuestionShare( 0 );
    levelStore.storeModeVampQuestionShare( 0 );
    levelStore.storeForeignNoteQuestionShare( 30 );

    int foreignQuestionCount = 0;

    constexpr std::uint32_t SEED_COUNT = 200;

    for( std::uint32_t seed = 1; seed <= SEED_COUNT; ++seed )
    {
        // Une session par graine, et sa PREMIERE question : c'est le tirage qu'on veut voir.
        ExerciseSessionController controller{ notePlayer, {}, {}, {}, {}, &levelStore };

        controller.choosePlayerLevel( static_cast<int>( domain::PlayerLevel::Advanced ) );
        controller.startOrdinarySession();

        if( controller.isForeignNoteQuestion() )
        {
            ++foreignQuestionCount;
        }
    }

    // Autour du quart, et jamais zero : zero voudrait dire que le reglage ne sert a rien, et c'est exactement ce que
    // Roger a cru voir.
    EXPECT_GT( foreignQuestionCount, 20 );
    EXPECT_LT( foreignQuestionCount, 70 );
}

TEST( ExerciseSessionControllerTest, a_foreign_note_question_offers_the_seven_notes_in_the_order_heard )
{
    // L'ecran recoit les sept notes DANS L'ORDRE ENTENDU : c'est celui de l'ecoute, donc celui des boutons. Le joueur
    // designe la place de l'intrus, et le domaine juge ce pas.
    domain::NotePlayerFake notePlayer;
    domain::PlayerPreferencesFake levelStore;

    storeForeignNoteOnlyShares( levelStore );

    ExerciseSessionController controller{ notePlayer, {}, {}, {}, {}, &levelStore };

    controller.choosePlayerLevel( static_cast<int>( domain::PlayerLevel::Advanced ) );
    controller.startOrdinarySession();

    ASSERT_TRUE( controller.isForeignNoteQuestion() );

    const QVariantList choices = controller.foreignNoteChoices();

    ASSERT_EQ( 7, choices.size() );

    int tonicCount = 0;

    for( int index = 0; index < choices.size(); ++index )
    {
        const QVariantMap note = choices.at( index ).toMap();

        EXPECT_EQ( index, note.value( "stepIndex" ).toInt() );
        EXPECT_FALSE( note.value( "name" ).toString().isEmpty() );

        if( note.value( "isTonic" ).toBool() )
        {
            ++tonicCount;
        }
    }

    // Une seule tonique, et c'est la premiere : le repere de la gamme, comme sur la roue.
    EXPECT_EQ( 1, tonicCount );
    EXPECT_TRUE( choices.first().toMap().value( "isTonic" ).toBool() );

    // Le verdict n'existe pas tant que la question est posee : il DIT ou est l'intrus, donc il est la reponse.
    EXPECT_TRUE( controller.foreignNoteVerdict().isEmpty() );

    // Et le son est bien une melodie sur un bourdon, comme pour un mode.
    EXPECT_FALSE( notePlayer.melodiesOverDrones().empty() );

    // Le pas juste n'est pas expose - ce serait donner la reponse - donc on les essaie tous : l'un d'eux conclut.
    for( int step = 0; ( step < 7 ) && !controller.isFeedbackVisible(); ++step )
    {
        controller.answerForeignNote( step );
    }

    ASSERT_TRUE( controller.isFeedbackVisible() );

    const QVariantMap verdict = controller.foreignNoteVerdict();

    ASSERT_TRUE( verdict.contains( "stepNumber" ) );

    // Un pas de 1 a 7, comme un musicien compte, et deux noms de note DIFFERENTS : ce que l'oreille a entendu, et ce que
    // la gamme attendait. Deux fois le meme nom voudrait dire que la question n'a pas de faute - et l'exercice serait
    // insoluble, ce que le test du domaine verifie de son cote.
    EXPECT_GE( verdict.value( "stepNumber" ).toInt(), 1 );
    EXPECT_LE( verdict.value( "stepNumber" ).toInt(), 7 );
    EXPECT_FALSE( verdict.value( "heardName" ).toString().isEmpty() );
    EXPECT_FALSE( verdict.value( "expectedName" ).toString().isEmpty() );
    EXPECT_NE( verdict.value( "heardName" ).toString(), verdict.value( "expectedName" ).toString() );
}

TEST( ExerciseSessionControllerTest, a_two_mode_question_announces_twice_the_sound )
{
    // L'ecran se sert de cette duree pour ne PAS couper la lecture : Roger a vu le minuteur avancer au milieu du son -
    // « pour les modes, ca va beaucoup trop vite, le son se coupe en plein milieu ». Une question de couleur fait
    // entendre DEUX modes, une question de nom un seul, et la duree doit le dire.
    domain::NotePlayerFake notePlayer;

    domain::PlayerPreferencesFake nameStore;
    storeNamedModeOnlyShares( nameStore );

    ExerciseSessionController nameController{ notePlayer, {}, {}, {}, {}, &nameStore };
    nameController.choosePlayerLevel( static_cast<int>( domain::PlayerLevel::Advanced ) );
    nameController.startOrdinarySession();

    const int oneMode = nameController.modeSoundDurationMs();

    ASSERT_GT( oneMode, 0 );

    // LA ROUE A BESOIN DE DEUX NOMBRES SEPARES, et pas seulement du total : le silence d'entree, et le pas d'une note.
    // Calee sur le total, sa tete partait avec le bourdon et finissait dans le silence - Roger l'a entendu : « la boule des
    // lignes dans les modes est un peu lente par rapport au son ».
    EXPECT_GT( nameController.modeSoundLeadInMs(), 0 );
    EXPECT_GT( nameController.modeSoundNoteStepMs(), 0 );

    // La DERNIERE note sonne AVANT la fin : ce qui reste est le bourdon qui traine, et la tete ne doit pas y voyager. Sept
    // notes se rejoignent par six pas, d'ou le 6.
    EXPECT_LT( nameController.modeSoundLeadInMs() + ( 6 * nameController.modeSoundNoteStepMs() ), oneMode );

    domain::PlayerPreferencesFake colourStore;
    storeColourOnlyShares( colourStore );

    ExerciseSessionController colourController{ notePlayer, {}, {}, {}, {}, &colourStore };
    colourController.choosePlayerLevel( static_cast<int>( domain::PlayerLevel::Advanced ) );
    colourController.startOrdinarySession();

    // Plus du double : les deux modes, ET le silence qui les separe.
    EXPECT_GT( colourController.modeSoundDurationMs(), oneMode * 2 );

    // Et une question qui ne parle pas de mode n'annonce aucune duree : la pause de l'ecran est alors celle des
    // intervalles, et une valeur inventee ici la rallongerait sans raison.
    ExerciseSessionController intervalController{ notePlayer, { intervalOnlySettings() } };
    intervalController.startOrdinarySession();

    EXPECT_EQ( 0, intervalController.modeSoundDurationMs() );

    // Et AUCUN des deux nombres de la roue : hors d'une question de mode, la roue n'est pas affichee, et une valeur inventee
    // ici ne servirait a personne.
    EXPECT_EQ( 0, intervalController.modeSoundLeadInMs() );
    EXPECT_EQ( 0, intervalController.modeSoundNoteStepMs() );
}

TEST( ExerciseSessionControllerTest, a_name_question_is_heard_as_a_melody )
{
    // Le jeu fait ecouter une PHRASE la ou il faisait monter une gamme. C'est la question que les trois cents phrases de
    // l'atelier attendaient, et le fake dit laquelle des deux lectures a eu lieu - ce qu'aucun test d'ecran ne saurait
    // verifier.
    domain::NotePlayerFake notePlayer;
    domain::PlayerPreferencesFake levelStore;

    storeNamedModeOnlyShares( levelStore );

    // UN TEMPO QUI DIVISE JUSTE : a 60 bpm, un temps vaut exactement mille millisecondes, donc deux temps valent
    // exactement le double. A 72, chaque pas est arrondi pour lui-meme (833 ms et 1667 ms), et le test comparerait deux
    // arrondis au lieu de comparer une duree. C'est aussi l'occasion de verifier que le tempo du REGLAGE est bien lu.
    levelStore.storePhraseTempoBpm( 60 );

    // Un livre qui porte une phrase par mode : le tirage tombe donc toujours juste, quel que soit le mode demande.
    domain::PhraseBook phraseBook;

    for( std::size_t index = 0; index < domain::MODE_COUNT; ++index )
    {
        domain::Phrase phrase;
        phrase.mode = static_cast<domain::Mode>( index );
        phrase.tonic = domain::Note{ 50 };
        phrase.bpm = 72;
        phrase.steps = { domain::PhraseStep{ .degree = 1, .beats = 1 },
                         domain::PhraseStep{ .degree = 3, .beats = 2 },
                         domain::PhraseStep{ .degree = 1, .beats = 2 } };

        phraseBook.add( std::move( phrase ) );
    }

    ExerciseSessionController controller{ notePlayer, {}, {}, {}, {}, &levelStore };
    controller.setPhraseBook( phraseBook );

    controller.choosePlayerLevel( static_cast<int>( domain::PlayerLevel::Advanced ) );
    controller.startOrdinarySession();

    ASSERT_EQ( static_cast<int>( domain::QuestionKind::ModeName ), controller.questionKind() );

    // UNE phrase a sonne, et une seule : c'est la question elle-meme.
    ASSERT_EQ( 1U, notePlayer.phrasesOverDrones().size() );

    // Et aucune gamme : une phrase jouee comme une gamme reguliere serait la meme question posee deux fois, et le joueur
    // entendrait autre chose que ce que l'atelier a choisi.
    EXPECT_TRUE( notePlayer.melodiesOverDrones().empty() );

    // Les DUREES viennent de la phrase : la troisieme note dure deux fois la premiere, et c'est exactement ce qui
    // distingue une melodie d'une gamme aux notes egales.
    const domain::NotePlayerFake::PlayedPhraseOverDrone & played = notePlayer.phrasesOverDrones().front();

    ASSERT_EQ( 3U, played.durations.size() );
    EXPECT_EQ( played.durations.at( 0 ).count() * 2, played.durations.at( 1 ).count() );

    // Et le bourdon est la, sous la melodie : un mode sans centre ne serait pas un mode.
    EXPECT_EQ( 2U, played.drone.size() );

    // LA DUREE annoncee a l'ecran est celle de la PHRASE, et pas celle d'une gamme de sept notes : cinq temps a 60 bpm,
    // plus un silence par note, plus l'encadrement du bourdon. Le temoin est le meme controleur SANS livre, qui pose la
    // meme question et fait entendre une gamme - et une gamme est plus courte. Sans ce calcul, l'ecran revelerait la
    // reponse pendant que la melodie joue encore.
    ExerciseSessionController scaleController{ notePlayer, {}, {}, {}, {}, &levelStore };
    scaleController.choosePlayerLevel( static_cast<int>( domain::PlayerLevel::Advanced ) );
    scaleController.startOrdinarySession();

    ASSERT_EQ( static_cast<int>( domain::QuestionKind::ModeName ), scaleController.questionKind() );

    // La preuve que le temoin joue bien une GAMME, et non une phrase : c'est la seule difference entre les deux seances.
    ASSERT_FALSE( notePlayer.melodiesOverDrones().empty() );

    // HUIT notes, et non sept : la gamme du JEU se REFERME sur sa tonique, une octave plus haut. Roger : « est-ce que ce
    // n'est pas mieux de boucler en entier et de revenir sur le 1er ? » C'est la derniere note qui NOMME le centre du mode,
    // la ou le bourdon le donne seulement - et une gamme qui s'arrete sur son septieme degre reste suspendue.
    const std::vector<domain::Note> & playedScale = notePlayer.melodiesOverDrones().back().melody;

    ASSERT_EQ( domain::DEGREE_COUNT + 1, playedScale.size() );
    EXPECT_EQ( domain::SEMITONES_PER_OCTAVE, playedScale.back().midiNumber() - playedScale.front().midiNumber() );

    EXPECT_GT( controller.modeSoundDurationMs(), scaleController.modeSoundDurationMs() );
}

TEST( ExerciseSessionControllerTest, putting_the_settings_back_to_default_brings_the_reminder_back )
{
    // Roger l'a vu en jouant : « par defaut dans les settings tu as enleve les notifications. Pourtant c'est une option qui
    // est choisie par defaut a l'installation, il faudrait qu'elle soit aussi activee si on appuie sur par defaut ».
    //
    // Un bouton qui rend un etat DIFFERENT de celui d'une installation neuve n'est pas un bouton « par defaut » : c'est un
    // bouton « autre chose », et c'est le genre de detail qui fait qu'on ne fait plus confiance a un ecran de reglages.
    domain::NotePlayerFake notePlayer;
    domain::PlayerPreferencesFake levelStore;

    ExerciseSessionController controller{ notePlayer, {}, {}, {}, {}, &levelStore };

    controller.setDailyReminderEnabled( false );

    EXPECT_FALSE( controller.dailyReminderEnabled() );

    controller.resetPreferences();

    EXPECT_TRUE( controller.dailyReminderEnabled() );
}

TEST( ExerciseSessionControllerTest, the_game_offers_the_next_step_when_the_experience_earns_it )
{
    // « Apres une partie, on pourra lui dire : bravo ». Le jeu PROPOSE et c'est l'ecran qui annonce - une seule fois, parce
    // qu'une bonne nouvelle repetee devient une machine a sous.
    domain::NotePlayerFake notePlayer;
    domain::PlayerPreferencesFake levelStore;

    ExerciseSessionController controller{ notePlayer, {}, {}, {}, {}, &levelStore };

    // Un joueur qui debute, et pas encore d'experience : rien a proposer.
    controller.choosePlayerLevel( static_cast<int>( domain::PlayerLevel::Beginner ) );

    EXPECT_FALSE( controller.levelInvitationIsAvailable() );

    // Le voila avec assez d'experience pour la suite : la fleche doree apparait.
    // Les seuils ont TRIPLE le 02/10/2026 (voir PlayerLevel) : « Jusqu'a l'octave » demande maintenant six mille points.
    levelStore.storeTotalExperience( 7000 );

    EXPECT_TRUE( controller.levelInvitationIsAvailable() );
    EXPECT_EQ( static_cast<int>( domain::PlayerLevel::Advanced ), controller.invitedLevel() );
    EXPECT_EQ( QStringLiteral( "Jusqu'à l'octave" ), controller.invitedLevelName() );

    // La felicitation de CE palier n'a pas encore ete annoncee : l'ecran de fin la montrera.
    EXPECT_FALSE( controller.levelInvitationAnnounced() );

    controller.markLevelInvitationAnnounced();

    EXPECT_TRUE( controller.levelInvitationAnnounced() );

    // Et si le joueur ACCEPTE, il monte par le MEME chemin que la liste des difficultes - donc rien de plus a tenir a jour.
    controller.acceptLevelInvitation();

    EXPECT_EQ( static_cast<int>( domain::PlayerLevel::Advanced ), controller.playerLevel() );

    // La fleche disparait : il est a son palier.
    EXPECT_FALSE( controller.levelInvitationIsAvailable() );

    // EN GODMODE, le jeu ne propose rien : le joueur a deja decide de choisir lui-meme, et lui offrir un palier serait lui
    // reprendre la main qu'il vient de prendre.
    controller.choosePlayerLevel( 5 );
    levelStore.storeTotalExperience( 9000 );

    EXPECT_TRUE( controller.godModeIsChosen() );
    EXPECT_FALSE( controller.levelInvitationIsAvailable() );
}

TEST( ExerciseSessionControllerTest, the_god_mode_plays_the_perimeter_the_player_saved )
{
    // Ce que Roger a demande : le joueur choisit lui-meme, et le jeu joue ce qu'il a SAUVEGARDE. Le temoin est la grille de
    // reponse - elle contient exactement les intervalles en jeu, donc elle dit ce que la partie pose vraiment.
    domain::NotePlayerFake notePlayer;
    domain::PlayerPreferencesFake levelStore;

    storeNamedIntervalOnlyShares( levelStore );

    ExerciseSessionController controller{ notePlayer, {}, {}, {}, {}, &levelStore };

    // Un joueur ordinaire : la grille vient de son NIVEAU, et elle est large.
    controller.choosePlayerLevel( static_cast<int>( domain::PlayerLevel::Advanced ) );
    controller.startOrdinarySession();

    const int levelChoiceCount = controller.choices().size();

    // Il passe en GodMode - la SIXIEME difficulte du combo - et choisit un perimetre etroit.
    controller.choosePlayerLevel( 5 );

    EXPECT_TRUE( controller.godModeIsChosen() );

    // Aucune palette n'a jamais ete sauvegardee : la page a du travail a montrer, et le drapeau le dit.
    EXPECT_FALSE( controller.godModeIsSaved() );

    controller.setEveryGodModeIntervalChecked( false );
    controller.toggleGodModeInterval( 0 );
    controller.toggleGodModeInterval( 5 );
    controller.toggleGodModeInterval( 7 );

    EXPECT_TRUE( controller.godModeCanStart() );
    EXPECT_TRUE( controller.godModeHasUnsavedChanges() );

    controller.saveGodMode();

    EXPECT_TRUE( controller.godModeIsSaved() );
    EXPECT_FALSE( controller.godModeHasUnsavedChanges() );

    controller.startOrdinarySession();

    std::vector<std::int32_t> played;

    for( const QVariant & choice : controller.choices() )
    {
        played.push_back( choice.toMap().value( QStringLiteral( "semitones" ) ).toInt() );
    }

    std::ranges::sort( played );

    // Les trois intervalles demandes, et RIEN d'autre : c'est tout le GodMode.
    EXPECT_EQ( ( std::vector<std::int32_t>{ 0, 5, 7 } ), played );

    // Et la grille a RETRECI au lieu de suivre le niveau : c'est ce qui prouve que le perimetre choisi a remplace celui du
    // niveau, et non qu'il s'y est ajoute.
    EXPECT_LT( controller.choices().size(), levelChoiceCount );
}

TEST( ExerciseSessionControllerTest, the_god_mode_refuses_a_perimeter_that_could_not_ask )
{
    // « Au moins deux » : avec UN seul choix, la reponse serait toujours la meme, et le joueur repondrait juste sans
    // ecouter. La regle ne sert a rien si elle ne va pas jusqu'au bout - donc on n'ecrit pas non plus une palette qui ne
    // pourrait pas poser de partie : elle serait sauvegardee, puis refusee au moment de jouer, sans que personne ne
    // comprenne ce qui s'est passe.
    domain::NotePlayerFake notePlayer;
    domain::PlayerPreferencesFake levelStore;

    storeNamedIntervalOnlyShares( levelStore );

    ExerciseSessionController controller{ notePlayer, {}, {}, {}, {}, &levelStore };

    controller.choosePlayerLevel( 5 );

    controller.setEveryGodModeIntervalChecked( false );
    controller.toggleGodModeInterval( 7 );

    EXPECT_FALSE( controller.godModeCanStart() );
    EXPECT_FALSE( controller.godModeProblem().isEmpty() );

    controller.saveGodMode();

    EXPECT_FALSE( controller.godModeIsSaved() );

    // Le modele d'un niveau remet tout d'aplomb : c'est le geste que Roger a decrit - « je debute, ca coche les deux
    // premiers intervalles ».
    controller.prefillGodModeFromLevel( static_cast<int>( domain::PlayerLevel::Fluent ) );

    EXPECT_TRUE( controller.godModeCanStart() );

    controller.saveGodMode();

    EXPECT_TRUE( controller.godModeIsSaved() );
}

TEST( ExerciseSessionControllerTest, the_god_mode_falls_back_on_the_level_until_a_palette_is_saved )
{
    // LA MIGRATION, et elle est invisible a dessein : un joueur qui choisit le GodMode sans avoir jamais ouvert sa page doit
    // pouvoir jouer, avec exactement le perimetre de son niveau. Sans elle, il tomberait sur une configuration vide et une
    // partie impossible a lancer - le seul vrai piege de cette fonctionnalite.
    domain::NotePlayerFake notePlayer;
    domain::PlayerPreferencesFake levelStore;

    storeNamedIntervalOnlyShares( levelStore );

    ExerciseSessionController controller{ notePlayer, {}, {}, {}, {}, &levelStore };

    controller.choosePlayerLevel( static_cast<int>( domain::PlayerLevel::Advanced ) );

    // LE NIVEAU PRE-REMPLIT LES CASES : c'est exactement ce que Roger a decrit - « je debute, ca coche les deux premiers
    // intervalles » - et c'est aussi ce qui garantit qu'un joueur qui passe en GodMode sans avoir jamais ouvert la page
    // joue le perimetre de son niveau, et non une configuration vide.
    EXPECT_EQ( 12, checkedIntervalCount( controller ) );

    controller.choosePlayerLevel( 5 );

    EXPECT_TRUE( controller.godModeIsChosen() );

    // La palette n'a pas change en choisissant le GodMode : elle est simplement devenue celle qui joue.
    EXPECT_EQ( 12, checkedIntervalCount( controller ) );

    controller.startOrdinarySession();

    // Et la grille de la partie offre ces DOUZE intervalles, la ou une partie de niveau n'en montrait que huit : le GodMode
    // n'a pas de plafond, il donne tout ce qui a ete coche.
    EXPECT_EQ( 12, controller.choices().size() );
}

TEST( ExerciseSessionControllerTest, a_harmony_question_offers_no_interval_hint )
{
    // Roger : « pour les bourdons quand je fail, je vois l'indice des intervalles apparaitre ».
    //
    // Toute question porte un intervalle - c'est l'ordre des tirages qui veut ca - mais sur un bourdon, personne ne l'a
    // jamais entendu : l'indice tombait donc sur le souvenir de film d'un intervalle jamais joue.
    //
    // La question est NOMMEE plutot que comparee, et ce n'est pas un detail : sur une question de couleur, le second
    // passage est joue par un minuteur, que le test ne fait pas tourner. Ici le mode qui vient de sonner est connu tout
    // de suite, donc on sait se tromper POUR DE BON.
    domain::NotePlayerFake notePlayer;
    domain::PlayerPreferencesFake levelStore;

    storeNamedModeOnlyShares( levelStore );

    ExerciseSessionController controller{ notePlayer, {}, hintBookForEveryAscendingInterval(), {}, {}, &levelStore };

    controller.choosePlayerLevel( static_cast<int>( domain::PlayerLevel::Advanced ) );
    controller.startOrdinarySession();

    ASSERT_EQ( static_cast<int>( domain::QuestionKind::ModeName ), controller.questionKind() );

    const int heardIndex = controller.heardMode().value( "index" ).toInt();

    // Un AUTRE mode de la palette du joueur : l'erreur est certaine, il n'y a aucun hasard a esperer ni a craindre.
    int wrongIndex = heardIndex;

    for( const QVariant & choice : controller.modeChoices() )
    {
        const int index = choice.toMap().value( "index" ).toInt();

        if( index != heardIndex )
        {
            wrongIndex = index;

            break;
        }
    }

    ASSERT_NE( heardIndex, wrongIndex );

    controller.answerModeName( wrongIndex );

    ASSERT_FALSE( controller.wasLastAnswerCorrect() );

    // L'indice existe - le livre en a un pour tous les intervalles - et il doit pourtant rester absent.
    EXPECT_TRUE( controller.hintText().isEmpty() );

    // La roue, elle, est donnee DES LA QUESTION : Roger l'a voulu ainsi - « l'utilisateur pourra ne pas trop la
    // regarder ». Elle aide fortement, et c'est assume : c'est un choix de difficulte, comme celui de l'indice.
    const QVariantList circle = controller.modeCircle();

    ASSERT_EQ( 12, circle.size() );

    int litCount = 0;
    int tonicCount = 0;

    for( const QVariant & entry : circle )
    {
        const QVariantMap note = entry.toMap();

        if( note.value( "inMode" ).toBool() )
        {
            ++litCount;
        }

        if( note.value( "isTonic" ).toBool() )
        {
            ++tonicCount;
        }
    }

    // Sept notes allumees dont UNE tonique : c'est une armure, et c'est tout le dessin.
    EXPECT_EQ( 7, litCount );
    EXPECT_EQ( 1, tonicCount );

    // Et elle reste une fois la reponse donnee : le verdict la garde sous les yeux.
    controller.answerModeName( heardIndex );

    ASSERT_TRUE( controller.wasLastAnswerCorrect() );
    EXPECT_EQ( 12, controller.modeCircle().size() );
}

// ---------------------------------------------------------------------------------------------------------------------
// The level of the player
//
// Asked once, remembered, and used to decide where the sessions start. Three things, and the third is the one
// that would be easy to lose: a level must never make the game unplayable.
// ---------------------------------------------------------------------------------------------------------------------

TEST( ExerciseSessionControllerTest, the_tuner_page_receives_its_texts )
{
    domain::NotePlayerFake notePlayer;
    domain::PlayerPreferencesFake levelStore;

    ExerciseSessionController controller{ notePlayer, intervalOnlySettings(), {}, {}, {}, &levelStore };

    domain::TunerGuide guide;
    guide.setTemperamentText( domain::Temperament::Equal, "l'egal" );
    guide.setTemperamentText( domain::Temperament::Pythagorean, "le pythagoricien" );
    guide.setDiapasonText( "le diapason" );
    guide.setReferenceNoteText( "la tonique" );
    guide.addHowToStep( "premiere etape" );

    controller.setTunerGuide( guide );

    // Le texte suit le TEMPERAMENT CHOISI, et non l'ordre du fichier de contenu : c'est la correspondance que le QML
    // ne pourrait pas faire sans la recopier.
    EXPECT_EQ( QStringLiteral( "l'egal" ), controller.temperamentExplanation() );

    controller.setTemperament( static_cast<int>( domain::Temperament::Pythagorean ) );
    EXPECT_EQ( QStringLiteral( "le pythagoricien" ), controller.temperamentExplanation() );

    const QVariantMap page = controller.tunerGuide();

    EXPECT_EQ( QStringLiteral( "le diapason" ), page.value( QStringLiteral( "diapason" ) ).toString() );
    EXPECT_EQ( QStringLiteral( "la tonique" ), page.value( QStringLiteral( "noteDeReference" ) ).toString() );
    ASSERT_EQ( 1, page.value( QStringLiteral( "modeEmploi" ) ).toList().size() );
    EXPECT_EQ( QStringLiteral( "premiere etape" ), page.value( QStringLiteral( "modeEmploi" ) ).toList().front().toString() );
}

TEST( ExerciseSessionControllerTest, an_empty_tuner_guide_leaves_empty_texts_and_nothing_else )
{
    domain::NotePlayerFake notePlayer;
    domain::PlayerPreferencesFake levelStore;

    ExerciseSessionController controller{ notePlayer, intervalOnlySettings(), {}, {}, {}, &levelStore };

    // Aucun guide injecte : c'est un fichier de contenu manquant, et la page doit se contenter de ses propres phrases.
    // Un texte vide n'est pas un plantage, et l'ecran sait le reconnaitre.
    EXPECT_TRUE( controller.temperamentExplanation().isEmpty() );
    EXPECT_TRUE( controller.tunerGuide().value( QStringLiteral( "diapason" ) ).toString().isEmpty() );

    // Et les REGLAGES, eux, restent la : ce qui manque est le commentaire, jamais la fonction.
    EXPECT_EQ( 440.0, controller.referencePitch() );
}

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

    controller.startOrdinarySession();

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
    controller.startOrdinarySession();

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

    controller.startOrdinarySession();

    EXPECT_TRUE( controller.running() );
}

TEST( ExerciseSessionControllerTest, the_levels_to_offer_are_ready_to_display )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, ascendingOnlySettings() };

    const QVariantList levels = controller.playerLevels();

    // LES CINQ NIVEAUX, ET LE GODMODE : six entrees, parce que le combo de la page de garde offre les deux - un niveau est
    // une marche, le GodMode est la porte qui sort de l'echelle.
    ASSERT_EQ( domain::PLAYER_LEVEL_COUNT + 1, static_cast<std::size_t>( levels.size() ) );

    for( int index = 0; index < levels.size(); ++index )
    {
        const QVariantMap level = levels.at( index ).toMap();

        // Every entry an index and a name: the screen displays them and never composes one.
        EXPECT_EQ( index, level.value( "index" ).toInt() );
        EXPECT_FALSE( level.value( "name" ).toString().isEmpty() );

        // Et le drapeau dit LAQUELLE n'est pas un niveau : c'est ce qui permet a l'ecran de les traiter differemment, et
        // notamment de ne pas proposer le GodMode comme modele de lui-meme.
        EXPECT_EQ( index == static_cast<int>( domain::PLAYER_LEVEL_COUNT ), level.value( "isGodMode" ).toBool() );
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

    controller.startOrdinarySession();

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

    controller.startOrdinarySession();

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
    pinEveryQuestionShare( settings );
    settings.directionQuestionShare = 100;

    ExerciseSessionController controller{ notePlayer, settings };

    controller.startOrdinarySession();

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

    controller.startOrdinarySession();

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

TEST( ExerciseSessionControllerTest, tapping_on_an_interval_question_changes_nothing )
{
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, intervalOnlySettings() };

    controller.startOrdinarySession();

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

    controller.startOrdinarySession();

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

    controller.startOrdinarySession();

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

    controller.startOrdinarySession();

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

    controller.startOrdinarySession();

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

    controller.startOrdinarySession();

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
    EXPECT_EQ( 20, controller.chordQuestionShare() );

    // On l'allume : le reglage existe, et c'est ce qui compte - le rythme se dose, y compris depuis zero.

    controller.setChordQuestionShare( 45 );

    EXPECT_EQ( 45, controller.chordQuestionShare() );
    EXPECT_EQ( 45, levelStore.storedChordQuestionShare() );

    // Et une valeur qui n'a pas de sens est refusee, pas convertie.
    controller.setChordQuestionShare( -3 );

    EXPECT_EQ( 45, controller.chordQuestionShare() );

    // ZERO est une valeur legitime, et c'est meme celle par defaut : elle fait disparaitre le rythme d'une session.
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

    controller.startOrdinarySession();

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

    controller.startOrdinarySession();

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

TEST( ExerciseSessionControllerTest, a_wrong_chord_is_heard_then_the_answer )
{
    // Roger : « quand on clique sur un accord et qu'on se trompe, on re-entend directement le bon accord. Je changerais
    // ca : entendre d'abord l'accord appuye, PUIS l'accord voulu. C'est moins perturbant. »
    //
    // Et la raison est plus profonde que le confort : entendre la reponse avant sa propre erreur efface l'ECART entre les
    // deux, et l'ecart est toute la lecon.
    domain::NotePlayerFake notePlayer;
    domain::QuestionLogFake log;

    domain::SessionSettings settings = chordOnlySettings();
    settings.lives = std::nullopt;    // la question reste posee, donc la paire se joue

    ExerciseSessionController controller{ notePlayer, settings };
    controller.setQuestionLog( &log );

    controller.startOrdinarySession();

    const std::size_t chordsHeardBefore = notePlayer.playedChords().size();

    const int playedQuality = wrongChordChoice( controller );

    controller.answerChord( playedQuality );

    // UNE seule paire, et non deux appels : un nouveau son REMPLACE le precedent, donc deux appels ne feraient entendre
    // que le second - exactement ce qu'on corrige.
    ASSERT_EQ( 1U, notePlayer.playedChordPairs().size() );

    // Et la question n'est PAS rejouee par le chemin commun : c'est la paire qui a parle.
    EXPECT_EQ( chordsHeardBefore, notePlayer.playedChords().size() );

    const domain::NotePlayerFake::PlayedChordPair & pair = notePlayer.playedChordPairs().front();

    // L'ORDRE est le sujet : ce que le joueur a joue d'abord, la reponse ensuite.
    ASSERT_EQ( 3U, pair.first.size() );
    ASSERT_EQ( 3U, pair.second.size() );

    // Les deux accords partent de la MEME tonique : c'est ce qui rend l'ecart audible sur une seule note de depart, et
    // deux toniques differentes ne seraient plus comparables.
    EXPECT_EQ( pair.first.front().midiNumber(), pair.second.front().midiNumber() );

    // ...et ils sont bien DIFFERENTS. Toute la liste, et pas une note isolee : majeur et mineur partagent leur quinte,
    // donc comparer une seule note ne prouverait rien.
    EXPECT_NE( pair.first, pair.second );

    // Et le silence entre les deux existe, sans quoi ils s'entendraient comme un seul accord qui bouge.
    EXPECT_GT( pair.gap.count(), 0 );
}

TEST( ExerciseSessionControllerTest, a_review_list_names_chords_too )
{
    // Roger, apres 1.6.30 : « le Bilan me montre toujours le Dorien ». La cause n'etait pas les modes du tout : les
    // listes etaient bornees par le nombre de CIBLES, puis chacune ecartait les cibles SANS NOM - et un accord n'avait
    // pas de nom sur cette page. Chaque accord rate mangeait donc une place en silence, jusqu'a ne laisser qu'une ligne.
    domain::NotePlayerFake notePlayer;
    domain::QuestionLogFake log;

    const auto now = std::chrono::system_clock::now();

    const auto add = [&log, &now]( domain::QuestionKind p_kind, std::int32_t p_target, bool p_correct ) {
        for( int index = 0; index < 4; ++index )
        {
            domain::QuestionRecord record;

            record.askedAt = now - std::chrono::hours{ 1 };
            record.kind = p_kind;
            record.target = p_target;
            record.direction = domain::IntervalDirection::Ascending;
            record.outcome = p_correct ? domain::QuestionOutcome::CorrectFirstTry : domain::QuestionOutcome::Failed;

            log.append( record );
        }
    };

    // Quatre cibles, dont un ACCORD rate - et un accord qui porte un nom de COULEUR, pas de tonique : c'est la cible telle
    // que le journal l'ecrit.
    add( domain::QuestionKind::NamedInterval, 12, true );
    add( domain::QuestionKind::NamedInterval, 7, true );
    add( domain::QuestionKind::NamedInterval, 5, false );
    add( domain::QuestionKind::Chord, static_cast<std::int32_t>( domain::ChordQuality::Minor ), false );

    // Les DEUX genres sont ouverts : le bilan ecarte les questions que le joueur a fermees, et un test qui n'ouvrirait
    // que l'intervalle verrait l'accord disparaitre avant meme d'arriver a la page.
    domain::SessionSettings settings = intervalOnlySettings();
    settings.namedIntervalQuestionShare = 50;
    settings.chordQuestionShare = 50;

    ExerciseSessionController controller{ notePlayer, settings };
    controller.setQuestionLog( &log );

    controller.startReviewSession();

    ASSERT_TRUE( controller.isReviewOpeningVisible() );

    const QVariantList weak = controller.reviewWeakPoints();

    // DEUX points faibles, et pas un seul : l'accord nomme ne mange plus la place d'un intervalle. C'est exactement le
    // symptome que Roger decrivait - une liste videe par ce qu'elle ne savait pas dire.
    ASSERT_EQ( 2U, weak.size() );

    // Les DEUX noms y sont, et le test ne se prononce pas sur leur ordre : les deux cibles sont ratees, donc a 0 % toutes
    // les deux, et un tri n'a rien a decider entre deux exgaux. Exiger un ordre ici serait tester une coincidence.
    QStringList names;

    for( const QVariant & point : weak )
    {
        names.append( point.toMap().value( "name" ).toString() );
    }

    names.sort();

    // Une liste construite a part : les virgules d'une liste entre accolades coupent la macro de test en deux, et
    // l'erreur parle alors de « trop d'arguments », ce qui n'aide personne.
    //
    // L'ORDRE EST CELUI D'UN TRI DE CHAINES, majuscules d'abord : « Quarte... » avant « mineur ». C'est une consequence
    // de la majuscule du nom, pas une intention - et le tri reste le plus sur, puisqu'il ne depend pas du tri interne des
    // cibles a egalite.
    const QStringList expected{ QStringLiteral( "Quarte juste montante" ), QStringLiteral( "mineur" ) };

    EXPECT_EQ( expected, names );
}

TEST( ExerciseSessionControllerTest, an_interval_is_named_in_french_for_the_player )
{
    // Roger : « effectivement les intervalles sont ecrits en anglais, je n'avais pas fait attention a ca, il faudrait les
    // ecrire en francais partout ou ca s'affiche ». Le modele GARDE son nom anglais - c'est le nom du code, celui qui
    // sert aux fichiers et aux cles - et c'est l'AFFICHAGE qui traduit.
    const QVariantMap fifth = describeInterval( domain::Interval{ 7 } );

    // L'identifiant reste anglais et stable : c'est lui qu'un fichier de sauvegarde ou un contenu garderait.
    EXPECT_EQ( QStringLiteral( "P5" ), fifth.value( "identifier" ).toString() );
    EXPECT_EQ( QStringLiteral( "Quinte juste" ), fifth.value( "name" ).toString() );

    // L'ACCORD DE GENRE, et c'est le piege de la langue : l'unisson est le SEUL masculin.
    EXPECT_EQ( QStringLiteral( "Unisson juste" ), describeInterval( domain::Interval{ 0 } ).value( "name" ).toString() );
    EXPECT_EQ( QStringLiteral( "Tierce majeure" ), describeInterval( domain::Interval{ 4 } ).value( "name" ).toString() );
    EXPECT_EQ( QStringLiteral( "Sixte mineure" ), describeInterval( domain::Interval{ 8 } ).value( "name" ).toString() );

    // Et un intervalle COMPOSE, qui ne doit pas perdre son nom en chemin : la neuvieme majeure, une octave au-dessus de la
    // seconde majeure.
    EXPECT_EQ( QStringLiteral( "Neuvième majeure" ), describeInterval( domain::Interval{ 14 } ).value( "name" ).toString() );
}

TEST( ExerciseSessionControllerTest, answering_twice_writes_one_line )
{
    domain::NotePlayerFake notePlayer;
    domain::QuestionLogFake log;

    ExerciseSessionController controller{ notePlayer, chordOnlySettings() };
    controller.setQuestionLog( &log );

    controller.startOrdinarySession();

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

    controller.startOrdinarySession();
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

    controller.startOrdinarySession();
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

    // Et le profil doit OUVRIR ce qu'il veut voir : un bilan ne pose plus une question dont la part est fermee, ce qui est
    // la correction demandee par Roger. Le journal ne contient que des intervalles, donc le profil ouvre l'intervalle.
    domain::SessionSettings settings = intervalOnlySettings();
    settings.namedIntervalQuestionShare = 100;

    ExerciseSessionController controller{ notePlayer, settings };
    controller.setQuestionLog( &log );

    controller.startReviewSession();

    // LE BILAN S'OUVRE SUR SA PAGE, et non sur une question : rien n'est compte avant que le joueur ait lu ce que
    // l'app sait de lui. Roger a demande cette page comme une PORTE vers le cote academique de l'app.
    EXPECT_FALSE( controller.running() );
    EXPECT_TRUE( controller.isReviewRunning() );
    EXPECT_TRUE( controller.isReviewOpeningVisible() );

    controller.beginReviewQuestions();

    EXPECT_TRUE( controller.running() );
    EXPECT_FALSE( controller.isReviewOpeningVisible() );

    // Le bilan est FINI : ses questions sont decidees, donc son compte est celui du plan - et il ne se perd pas, puisqu'un
    // bilan sans vies ne peut pas s'arreter au milieu.
    EXPECT_GT( controller.questionCount(), 0 );
    EXPECT_LT( controller.questionCount(), 10 );
    EXPECT_TRUE( controller.hasUnlimitedLives() );
}

TEST( ExerciseSessionControllerTest, the_review_opens_on_what_the_app_knows_about_the_player )
{
    // LA PAGE D'OUVERTURE, et ce qu'elle doit contenir : les points FORTS et les points FAIBLES du joueur, tires de son
    // journal - la demande de Roger, mot pour mot : « avec les intervalles et les modes que le joueur reussit le plus,
    // ainsi que ceux qu'il reussit le moins ».
    domain::NotePlayerFake notePlayer;
    domain::QuestionLogFake log;

    fillJournalWithWorkedTargets( log );

    domain::SessionSettings settings = intervalOnlySettings();
    settings.namedIntervalQuestionShare = 100;

    ExerciseSessionController controller{ notePlayer, settings };
    controller.setQuestionLog( &log );

    controller.startReviewSession();

    ASSERT_TRUE( controller.isReviewOpeningVisible() );

    const QVariantList strong = controller.reviewStrongPoints();
    const QVariantList weak = controller.reviewWeakPoints();

    ASSERT_FALSE( strong.isEmpty() );
    ASSERT_FALSE( weak.isEmpty() );

    // Le journal contient six intervalles : trois sus (12, 7 et 4 demi-tons) et trois rates (2, 6 et 11). Les deux
    // listes disent donc deux choses opposees, et elles sont lues dans la MEME source triee - donc le meilleur ne peut
    // pas se retrouver parmi les faibles.
    //
    // Le tri ne garantit pas l'ordre des exgaux : on verifie donc le TAUX et la langue, pas un nom precis.
    EXPECT_EQ( 100, strong.first().toMap().value( "percent" ).toInt() );
    EXPECT_EQ( 0, weak.first().toMap().value( "percent" ).toInt() );

    // Le nom est FRANCAIS et il porte la direction : « Quinte montante ». L'intervalle du domaine, lui, se nomme en
    // anglais - c'est le nom du modele, et la page parle au joueur.
    EXPECT_TRUE( strong.first().toMap().value( "name" ).toString().endsWith( QStringLiteral( " montante" ) ) );
    EXPECT_GE( weak.first().toMap().value( "asked" ).toInt(), 1 );

    // ET RIEN N'EST COMMENCE : c'est tout l'objet de la page. Une question jouee derriere l'explication serait perdue -
    // le joueur l'entendrait sans la regarder.
    EXPECT_FALSE( controller.running() );

    // « Plus tard » : le bilan n'a pas eu lieu, et RIEN n'a ete compte.
    controller.cancelReviewOpening();

    EXPECT_FALSE( controller.isReviewOpeningVisible() );
    EXPECT_FALSE( controller.isReviewRunning() );
    EXPECT_FALSE( controller.running() );
}

TEST( ExerciseSessionControllerTest, the_two_review_lists_never_share_a_point )
{
    // Roger, sur son propre ecran : « ce qui te resiste encore me montre [...] Octave montante 100 % ». Un taux parfait
    // dans la liste des faiblesses n'est pas une nuance, c'est une contradiction.
    //
    // La cause : les deux listes sont tirees de la MEME liste triee, l'une par la fin, l'autre par le debut, et rien ne
    // les empechait de se rejoindre. Avec quatre cibles travaillees, les deux tranches se recouvraient de deux lignes -
    // donc la meilleure cible du joueur pouvait etre presentee comme ce qui lui resiste.
    domain::NotePlayerFake notePlayer;
    domain::QuestionLogFake log;

    const auto now = std::chrono::system_clock::now();

    const auto add = [&log, &now]( std::int32_t p_semitones, bool p_correct ) {
        for( int index = 0; index < 3; ++index )
        {
            domain::QuestionRecord record;

            record.askedAt = now - std::chrono::hours{ 1 };
            record.kind = domain::QuestionKind::NamedInterval;
            record.target = p_semitones;
            record.direction = domain::IntervalDirection::Ascending;
            record.outcome = p_correct ? domain::QuestionOutcome::CorrectFirstTry : domain::QuestionOutcome::Failed;

            log.append( record );
        }
    };

    // Quatre cibles, trois observations chacune : le seuil qui fait un point faible est atteint, et il y a de quoi
    // remplir deux listes - mais pas de quoi les remplir sans qu'elles se recouvrent.
    add( 12, true );
    add( 7, true );
    add( 5, false );
    add( 2, false );

    domain::SessionSettings settings = intervalOnlySettings();
    settings.namedIntervalQuestionShare = 100;

    ExerciseSessionController controller{ notePlayer, settings };
    controller.setQuestionLog( &log );

    controller.startReviewSession();

    ASSERT_TRUE( controller.isReviewOpeningVisible() );

    const QVariantList strong = controller.reviewStrongPoints();
    const QVariantList weak = controller.reviewWeakPoints();

    ASSERT_FALSE( strong.isEmpty() );
    ASSERT_FALSE( weak.isEmpty() );

    // Aucun point faible n'est aussi un point fort : c'est toute la garantie, et elle vaut mieux qu'un raisonnement sur
    // les indices.
    for( const QVariant & weakPoint : weak )
    {
        for( const QVariant & strongPoint : strong )
        {
            EXPECT_NE( weakPoint.toMap().value( "name" ).toString(), strongPoint.toMap().value( "name" ).toString() );
        }

        // Et le symptome exact que Roger a vu : un taux parfait ne peut pas etre dans ce qui resiste.
        EXPECT_LT( weakPoint.toMap().value( "percent" ).toInt(), 100 );
    }
}

TEST( ExerciseSessionControllerTest, a_target_seen_once_is_not_what_resisted_the_player )
{
    // Ce que Roger a entendu, et qui n'etait pas vrai : « des fois tu dis : bravo, c'etait quelque chose qui te
    // resistait alors que pas du tout ».
    //
    // La cause : une cible ratee UNE SEULE fois affiche 0 % de reussite, donc elle arrive en tete du tri par faiblesse,
    // donc en premiere ligne du bilan, avec la phrase qui va avec. Le joueur, lui, ne se souvient pas de l'avoir ratee -
    // il ne l'a pour ainsi dire jamais rencontree, et l'app lui racontait alors une histoire sur lui-meme qu'elle
    // n'avait pas les moyens de connaitre.
    //
    // Le seuil est celui de la page de statistiques : la meme regle vaut donc partout ou l'app designe ce qui resiste.
    domain::NotePlayerFake notePlayer;
    domain::QuestionLogFake log;

    fillJournalWithWorkedTargets( log );

    // UNE fois, et ratee : elle vaut 0 % de reussite, et c'est precisement pour cela qu'elle n'a rien a faire dans un
    // bilan. Une cible vue une fois n'est pas un point faible, c'est un hasard.
    const auto now = std::chrono::system_clock::now();

    domain::QuestionRecord seenOnce;
    seenOnce.askedAt = now - std::chrono::hours{ 1 };
    seenOnce.kind = domain::QuestionKind::NamedInterval;
    seenOnce.target = 9;
    seenOnce.direction = domain::IntervalDirection::Ascending;
    seenOnce.outcome = domain::QuestionOutcome::Failed;

    log.append( seenOnce );

    domain::SessionSettings settings = intervalOnlySettings();
    settings.namedIntervalQuestionShare = 100;

    ExerciseSessionController controller{ notePlayer, settings };
    controller.setQuestionLog( &log );

    controller.startReviewSession();

    // Le bilan s'ouvre sur sa page : le plan est pret, mais les questions attendent d'etre lues.
    ASSERT_TRUE( controller.isReviewOpeningVisible() );

    controller.beginReviewQuestions();

    ASSERT_TRUE( controller.running() );

    // Six cibles retenues - trois en echauffement, trois en difficulte. La septieme, vue une fois, n'existe pas pour le
    // bilan : sans la garde, elle entrerait dans le plan et le compte vaudrait QUATRE.
    EXPECT_EQ( 3, controller.questionCount() );
}

TEST( ExerciseSessionControllerTest, a_review_session_never_poses_a_kind_the_player_closed )
{
    // LE test du bug que Roger a signale, et il est critique : « j'ai beau mettre plus clair et plus sombre a 0, je
    // l'obtiens toujours dans mes parties ».
    //
    // La cause : le bilan ne passe pas par le TIRAGE, il impose son plan - donc les parts ne s'appliquaient pas a lui. Il
    // proposait au joueur de travailler exactement ce qu'il avait refuse.
    domain::NotePlayerFake notePlayer;
    domain::QuestionLogFake log;

    // Un journal qui contient des questions de MODE rattees : c'est ce que le bilan voudra faire travailler.
    const auto now = std::chrono::system_clock::now();

    for( int index = 0; index < 5; ++index )
    {
        domain::QuestionRecord record;

        record.askedAt = now - std::chrono::hours{ 1 };
        record.kind = domain::QuestionKind::ModeColour;
        record.target = static_cast<std::int32_t>( domain::Mode::Dorian );
        record.outcome = domain::QuestionOutcome::Failed;

        log.append( record );
    }

    domain::SessionSettings settings = intervalOnlySettings();
    settings.namedIntervalQuestionShare = 100;

    ExerciseSessionController controller{ notePlayer, settings };
    controller.setQuestionLog( &log );

    controller.startReviewSession();

    ASSERT_TRUE( controller.running() );

    // Toute la session, question apres question : le mode est FERME, donc le bilan n'en pose aucun.
    while( controller.running() && controller.isAsking() )
    {
        EXPECT_FALSE( controller.isModeQuestion() ) << "le bilan a pose une question que le joueur avait fermee";

        controller.revealAnswer();
        controller.continueToNextQuestion();
    }
}

TEST( ExerciseSessionControllerTest, the_dog_barks_when_the_session_ends )
{
    // Roger : « quand le corgi anecdote apparait, ce serait bien qu'il fasse un petit son ». Le controle porte sur les
    // deux moities de la phrase : il aboie QUAND il apparait, et il se tait tout le reste du temps - un chien qui
    // aboierait a chaque question serait pire que pas de chien du tout.
    domain::NotePlayerFake notePlayer;

    domain::SessionSettings settings = ascendingOnlySettings();

    settings.questionCount = 2;

    ExerciseSessionController controller{ notePlayer, settings };

    controller.startOrdinarySession();

    EXPECT_EQ( 0, notePlayer.dogBarkCount() );

    while( !controller.isFinished() )
    {
        answerCorrectly( controller );
        controller.continueToNextQuestion();
    }

    // Tant que le joueur est encore devant l'ecran de fin, le chien se tait : c'est le RETOUR qui le fait parler, et
    // c'est l'ecran qui le declenche.
    EXPECT_EQ( 0, notePlayer.dogBarkCount() );

    controller.stopSession();

    EXPECT_EQ( 1, notePlayer.dogBarkCount() );

    // Et un second retour ne le fait pas aboyer une deuxieme fois : il a deja parle, et il parle une fois par partie.
    controller.stopSession();

    EXPECT_EQ( 1, notePlayer.dogBarkCount() );
}

TEST( ExerciseSessionControllerTest, a_foreign_note_question_walks_the_wheel_along_the_expected_scale )
{
    // LA ROUE S'ANIME AUSSI SUR UNE NOTE ETRANGERE, et c'est Roger qui l'a demande : « j'aimerais bien que pour la note
    // etrangere il y ait aussi ces lignes ».
    //
    // Ce qu'elle dessine est le chemin que la gamme AURAIT DU suivre, et non celui qu'on entend : les sept pas dans
    // l'ordre. C'est la tout l'interet - la ligne est la REFERENCE, et c'est l'ecart entre elle et le son qui fait
    // entendre l'intrus. Sans ce signal, la roue restait muette sur cette question.
    domain::NotePlayerFake notePlayer;

    domain::SessionSettings settings;
    settings.namedIntervalQuestionShare = 0;
    settings.singQuestionShare = 0;
    settings.chordQuestionShare = 0;
    settings.modeColourQuestionShare = 0;
    settings.modeNameQuestionShare = 0;
    settings.modeVampQuestionShare = 0;
    settings.foreignNoteQuestionShare = 100;

    ExerciseSessionController controller{ notePlayer, settings };

    controller.startOrdinarySession();

    ASSERT_TRUE( controller.isForeignNoteQuestion() );

    // Un COMPTEUR branche sur le signal, et non QSignalSpy : le module Qt Test n'est pas lie a ces tests, et une
    // connexion directe dit exactement la meme chose.
    //
    // Il est branche APRES le demarrage, parce que le demarrage joue deja la question : ce qui est compte ici est la
    // relecture, donc l'appel que fait l'ecran quand on appuie sur « ecouter encore ».
    int playbackCount = 0;

    QObject::connect( &controller,
                      &ExerciseSessionController::modePlaybackStarted,
                      &controller,
                      [&playbackCount]() { ++playbackCount; } );

    controller.replay();

    EXPECT_EQ( 1, playbackCount );

    // Et les deux nombres que la roue consomme decrivent bien cette gamme-la : une question de note etrangere compte
    // PARMI les questions de mode. Sans eux, la roue partirait a l'instant zero, et avancerait a une autre vitesse que
    // les sept notes qu'on entend.
    EXPECT_GT( controller.modeSoundLeadInMs(), 0 );
    EXPECT_GT( controller.modeSoundNoteStepMs(), 0 );
    EXPECT_GT( controller.modeSoundDurationMs(), 0 );
}

TEST( ExerciseSessionControllerTest, the_circle_carries_the_step_of_each_note )
{
    // La roue est rangée par QUINTES, la gamme par DEGRÉS : « appuyer sur une pastille pour répondre » n'a donc de sens
    // que si le domaine a réconcilié les deux. Roger l'a demandé - « il suffit d'appuyer sur un de ces boutons non ? » -
    // et c'est exactement ce que ce test verrouille : chaque note allumée porte le pas que answerForeignNote attend.
    domain::NotePlayerFake notePlayer;

    domain::SessionSettings settings;
    settings.namedIntervalQuestionShare = 0;
    settings.singQuestionShare = 0;
    settings.chordQuestionShare = 0;
    settings.modeColourQuestionShare = 0;
    settings.modeNameQuestionShare = 0;
    settings.modeVampQuestionShare = 0;
    settings.foreignNoteQuestionShare = 100;

    ExerciseSessionController controller{ notePlayer, settings };

    controller.startOrdinarySession();

    ASSERT_TRUE( controller.isForeignNoteQuestion() );

    const QVariantList choices = controller.foreignNoteChoices();
    const QVariantList circle = controller.modeCircle();

    int litCount = 0;

    for( const QVariant & entry : circle )
    {
        const QVariantMap note = entry.toMap();

        if( !note.value( QStringLiteral( "inMode" ) ).toBool() )
        {
            // Une pastille éteinte n'est pas un choix : son pas vaut -1, et le clic est désactivé côté écran.
            EXPECT_EQ( -1, note.value( QStringLiteral( "stepIndex" ) ).toInt() );
            continue;
        }

        ++litCount;

        const int step = note.value( QStringLiteral( "stepIndex" ) ).toInt();

        ASSERT_GE( step, 0 );
        ASSERT_LT( step, choices.size() );

        // Le nom porté par la pastille doit être celui du pas correspondant : c'est la même gamme, vue dans deux ordres.
        EXPECT_EQ( choices.at( step ).toMap().value( QStringLiteral( "name" ) ).toString(),
                   note.value( QStringLiteral( "name" ) ).toString() );
    }

    EXPECT_EQ( 7, litCount );
}

TEST( ExerciseSessionControllerTest, the_dog_of_the_home_page_tells_another_anecdote )
{
    // Le chien de l'accueil repond : appuyer sur lui ouvre la popup avec une AUTRE anecdote. Roger l'a demande en bonus,
    // et c'est ce qui fait du chien un personnage plutot qu'une illustration.
    domain::NotePlayerFake notePlayer;

    ExerciseSessionController controller{ notePlayer, intervalOnlySettings() };

    EXPECT_FALSE( controller.isChibaTalking() );

    controller.tellAnotherAnecdote();

    EXPECT_TRUE( controller.isChibaTalking() );

    // Et il se tait comme les autres : sur un clic.
    controller.dismissChiba();

    EXPECT_FALSE( controller.isChibaTalking() );
}

TEST( ExerciseSessionControllerTest, a_foreign_note_question_is_a_harmony_question )
{
    // L'ecran avait recopie « mode » la ou il fallait « harmonie » : le bloc qui porte les sept boutons de l'intrus etait
    // donc cache, et Roger se retrouvait devant un ecran vide apres avoir entendu la gamme - « j'entends bien une phrase,
    // mais rien ensuite, aucun bouton, on est bloque ».
    //
    // Le predicat vit ici, nomme, et il est teste : c'est ce qui empeche l'ecran de le recopier de travers.
    domain::NotePlayerFake notePlayer;

    domain::SessionSettings foreignOnly;
    foreignOnly.namedIntervalQuestionShare = 0;
    foreignOnly.singQuestionShare = 0;
    foreignOnly.chordQuestionShare = 0;
    foreignOnly.modeColourQuestionShare = 0;
    foreignOnly.modeNameQuestionShare = 0;
    foreignOnly.modeVampQuestionShare = 0;
    foreignOnly.foreignNoteQuestionShare = 100;

    ExerciseSessionController harmonyController{ notePlayer, foreignOnly };
    harmonyController.startOrdinarySession();

    EXPECT_TRUE( harmonyController.isForeignNoteQuestion() );
    EXPECT_TRUE( harmonyController.isHarmonyQuestion() );

    // Et une question d'intervalle n'est PAS une question d'harmonie : sinon la zone des modes se montrerait devant elle.
    domain::SessionSettings intervalsOnly = intervalOnlySettings();
    intervalsOnly.namedIntervalQuestionShare = 100;

    ExerciseSessionController intervalController{ notePlayer, intervalsOnly };
    intervalController.startOrdinarySession();

    EXPECT_FALSE( intervalController.isHarmonyQuestion() );
}

TEST( ExerciseSessionControllerTest, a_foreign_note_question_offers_seven_notes_and_is_answerable )
{
    // Roger : « quand je lance, j'entends bien une phrase, mais rien ensuite, aucun bouton, on est bloqué ».
    //
    // Ce test suit le chemin exact de l'ecran, et il echoue sur le premier maillon casse : la question doit etre POSEE
    // (isAsking), offrir SEPT notes a montrer, et accepter une reponse. Un seul de ces trois manque, et le joueur reste
    // devant un ecran vide.
    domain::NotePlayerFake notePlayer;

    domain::SessionSettings settings;
    settings.namedIntervalQuestionShare = 0;
    settings.singQuestionShare = 0;
    settings.chordQuestionShare = 0;
    settings.modeColourQuestionShare = 0;
    settings.modeNameQuestionShare = 0;
    settings.modeVampQuestionShare = 0;
    settings.foreignNoteQuestionShare = 100;

    ExerciseSessionController controller{ notePlayer, settings };

    controller.startOrdinarySession();

    ASSERT_TRUE( controller.isForeignNoteQuestion() );

    // 1. la question est posee, et pas bloquee en lecture.
    EXPECT_TRUE( controller.isAsking() ) << "la question de note etrangere n'est pas posee";

    // 2. sept notes, celles de la gamme jouee.
    EXPECT_EQ( 7, controller.foreignNoteChoices().size() );

    // 3. et une reponse est acceptee : le verdict d'un intrus se donne apres. Une mauvaise reponse laisse REESSAYER -
    // c'est la regle du jeu, pas un blocage - donc on se sert du geste que l'ecran offre au joueur.
    controller.revealAnswer();

    EXPECT_FALSE( controller.isAsking() );
    EXPECT_TRUE( controller.foreignNoteVerdict().contains( QStringLiteral( "stepNumber" ) ) );
}

TEST( ExerciseSessionControllerTest, only_the_vamp_comes_out_when_only_the_vamp_is_open )
{
    // Le diagnostic de Roger, et il etait exact : « je pense que les modes "deux centre" et "plus clair, plus sombre" se
    // confondent ». Ce test verrouille les deux moities de la reponse :
    //
    //   1. le REGLAGE marchait : seul le vamp ouvert ne pose QUE des vamps - il n'y avait pas de melange cache ;
    //   2. mais l'ecran les affichait pareil, et l'ecran doit donc savoir reconnaitre un vamp (voir isModeVampQuestion).
    domain::NotePlayerFake notePlayer;

    domain::SessionSettings settings;
    settings.namedIntervalQuestionShare = 0;
    settings.singQuestionShare = 0;
    settings.chordQuestionShare = 0;
    settings.modeColourQuestionShare = 0;
    settings.modeNameQuestionShare = 0;
    settings.modeVampQuestionShare = 100;

    // ET LA NOTE ETRANGERE, qui n'existait pas encore ici quand ce test a ete ecrit.
    //
    // Elle est passee inapercue tant que sa part valait zero par defaut. Depuis que l'harmonie s'ouvre a l'installation
    // (01/10/2026), une question sur onze n'etait plus un vamp - et le test, qui dit « seul le vamp ouvert ne pose QUE des
    // vamps », avait raison de le signaler.
    settings.foreignNoteQuestionShare = 0;

    ExerciseSessionController controller{ notePlayer, settings };

    controller.startOrdinarySession();

    int questionCount = 0;
    int vampCount = 0;

    // Bornée : une boucle qui dépend de l'état du jeu doit toujours avoir un plafond, sans quoi un test qui échoue se
    // transforme en test qui ne rend jamais la main.
    for( int index = 0; ( index < 200 ) && controller.running(); ++index )
    {
        ++questionCount;

        EXPECT_TRUE( controller.isModeQuestion() );

        // Toute question de mode est ICI un vamp : la comparaison de deux modes n'a aucune part ouverte.
        EXPECT_TRUE( controller.isModeVampQuestion() );

        if( controller.isModeVampQuestion() )
        {
            ++vampCount;
        }

        // « pareil » n'existe pas sur un vamp : on répond un sens, juste ou faux, et les vies font le reste.
        controller.answerModeColour( true );
        controller.continueToNextQuestion();
    }

    EXPECT_GT( questionCount, 0 );
    EXPECT_EQ( questionCount, vampCount );
}

TEST( ExerciseSessionControllerTest, the_dog_only_talks_after_a_session_that_ended )
{
    // Le Musichien qui s'invite : il arrive quand le joueur REVIENT d'une partie finie, avec une anecdote a raconter.
    //
    // Quitter en pleine partie n'a rien a raconter - c'est meme le contraire d'un moment ou l'on veut lire. Et il ne part
    // que sur le clic du joueur : un texte qu'on n'a pas fini de lire est un texte qu'on n'aurait pas du montrer.
    domain::NotePlayerFake notePlayer;

    domain::SessionSettings settings = intervalOnlySettings();
    settings.namedIntervalQuestionShare = 100;

    ExerciseSessionController controller{ notePlayer, settings };

    EXPECT_FALSE( controller.isChibaTalking() );

    // Une partie qu'on quitte en cours : le chien se tait.
    controller.startOrdinarySession();
    controller.stopSession();

    EXPECT_FALSE( controller.isChibaTalking() );

    // Une partie menee jusqu'au bout : il parle, et une seule fois par partie.
    controller.startOrdinarySession();

    for( int index = 0; ( index < 500 ) && !controller.isFinished(); ++index )
    {
        if( controller.isAsking() )
        {
            controller.revealAnswer();
        }

        controller.continueToNextQuestion();
    }

    ASSERT_TRUE( controller.isFinished() );

    controller.stopSession();

    EXPECT_TRUE( controller.isChibaTalking() );
    EXPECT_TRUE( controller.running() == false );

    // Et il se tait des que le joueur a lu.
    controller.dismissChiba();

    EXPECT_FALSE( controller.isChibaTalking() );
}

TEST( ExerciseSessionControllerTest, a_plain_game_after_a_review_is_not_a_review )
{
    // Le drapeau du bilan doit RETOMBER quand on repart pour une partie ordinaire.
    //
    // Roger a decrit le symptome : « j'ai les encouragements que je ne devrais avoir que dans le mode bilan ». Le drapeau
    // n'etait remis a faux qu'a trois endroits - la remise a zero du score, un bilan vide, le debut d'un bilan - donc UN
    // SEUL bilan joue, une fois, suffisait a faire parler TOUTES les parties suivantes comme un bilan.
    domain::NotePlayerFake notePlayer;
    domain::QuestionLogFake log;

    fillJournalWithWorkedTargets( log );

    domain::SessionSettings settings = intervalOnlySettings();
    settings.namedIntervalQuestionShare = 100;

    ExerciseSessionController controller{ notePlayer, settings };
    controller.setQuestionLog( &log );

    controller.startReviewSession();

    ASSERT_TRUE( controller.isReviewRunning() );

    // Le joueur quitte le bilan, puis joue : ce n'est plus un bilan.
    controller.stopSession();
    controller.startOrdinarySession();

    EXPECT_FALSE( controller.isReviewRunning() );

    // Et sur la partie entiere, aucun mot du bilan ne sort : le silence est la regle hors bilan.
    while( controller.running() && controller.isAsking() )
    {
        EXPECT_TRUE( controller.encouragementText().isEmpty() ) << "le bilan a parle pendant une partie ordinaire";

        controller.revealAnswer();
        controller.continueToNextQuestion();
    }
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

    // Le profil ouvre ce que le bilan doit poser : un intervalle a nommer, seul genre du journal de ce test.
    domain::SessionSettings settings = intervalOnlySettings();
    settings.namedIntervalQuestionShare = 100;

    ExerciseSessionController controller{ notePlayer, settings };
    controller.setQuestionLog( &log );

    // Une partie ordinaire ne dit RIEN : l'ecran reste silencieux, et c'est ce qui donne du poids aux mots du bilan.
    controller.startOrdinarySession();

    EXPECT_TRUE( controller.encouragementText().isEmpty() );

    controller.stopSession();
    controller.startReviewSession();

    // La page d'ouverture, puis ses questions : c'est le meme geste pour le joueur, et le test le fait.
    controller.beginReviewQuestions();

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

    // Le profil ne veut QUE des accords : « nommer » redescend donc a zero, sinon les deux parts se partageraient la
    // session et la moitie des questions ne seraient pas des accords.
    levelStore.storeNamedIntervalQuestionShare( 0 );
    levelStore.storeChordQuestionShare( 100 );

    ExerciseSessionController controller{ notePlayer, chordOnlySettings(), {}, {}, {}, &levelStore };
    controller.setQuestionLog( &log );

    controller.startOrdinarySession();
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

    controller.startOrdinarySession();

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

    controller.startOrdinarySession();
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

    // Le NOMBRE vient du DOMAINE, et non d'un nombre ecrit ici : ajouter un timbre au jeu ne doit pas casser un test qui
    // parle d'autre chose. La liste elle-meme est verifiee juste apres, et c'est elle qui compte.
    ASSERT_EQ( static_cast<int>( domain::INSTRUMENT_COUNT ), instruments.size() );

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

TEST( ExerciseSessionControllerTest, hearing_an_instrument_is_what_lets_a_player_choose_it )
{
    // Roger : « pour l'utilisateur, c'est un peu complique de choisir son instrument car c'est complique de l'entendre ».
    // Le bouton d'ecoute joue donc, pour l'instrument demande, la gamme phrygienne montee puis descendue, et l'accord qui
    // signe le mode.
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, intervalOnlySettings(), {}, {}, {}, nullptr };

    controller.previewInstrument( 4 );

    ASSERT_EQ( 1U, notePlayer.playedMelodies().size() );

    const std::vector<domain::Note> & scale = notePlayer.playedMelodies().front().notes;

    // Treize notes : sept qui montent, six qui redescendent.
    ASSERT_EQ( 13U, scale.size() );

    // La gamme part de la tonique ET y revient : c'est la descente qui fait entendre ou se trouve le centre.
    EXPECT_EQ( scale.front().midiNumber(), scale.back().midiNumber() );

    ASSERT_EQ( 1U, notePlayer.playedChords().size() );

    const std::vector<domain::Note> & chord = notePlayer.playedChords().front().notes;

    ASSERT_EQ( 3U, chord.size() );

    // L'accord du bII majeur, la signature du phrygien, sur la meme tonique que la gamme entendue.
    EXPECT_EQ( scale.front().midiNumber() + 1, chord.front().midiNumber() );
}

TEST( ExerciseSessionControllerTest, an_instrument_that_does_not_exist_is_not_played )
{
    // L'index vient de l'ecran, donc il est verifie ici - et deux tests plutot qu'un, parce que comparer un index signe
    // avec un compte non signe est exactement ce qui laisse passer un index negatif.
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, intervalOnlySettings(), {}, {}, {}, nullptr };

    controller.previewInstrument( static_cast<int>( domain::INSTRUMENT_COUNT ) );
    controller.previewInstrument( -1 );

    EXPECT_TRUE( notePlayer.playedMelodies().empty() );
    EXPECT_TRUE( notePlayer.playedChords().empty() );
}

TEST( ExerciseSessionControllerTest, an_instrument_can_be_heard_before_it_is_chosen )
{
    // C'est tout l'interet du bouton : un instrument DECOCHE doit pouvoir s'ecouter, puisque c'est justement celui qu'on
    // hesite a cocher. L'apercu ne regarde donc jamais la liste des timbres acceptes - il joue ce qu'on lui demande.
    domain::NotePlayerFake notePlayer;
    ExerciseSessionController controller{ notePlayer, intervalOnlySettings(), {}, {}, {}, nullptr };

    controller.setInstrumentEnabled( 4, false );

    controller.previewInstrument( 4 );

    EXPECT_EQ( 1U, notePlayer.playedMelodies().size() );
    EXPECT_EQ( 1U, notePlayer.playedChords().size() );
}

// LES PALIERS SE FERMENT tant que l'experience ne les ouvre pas. C'est ce qui donne au GodMode son sens de passe-droit, et
// c'est la regle que Roger a demandee - « on bloque le choix des difficultes superieures tant qu'on n'a pas un certain
// niveau d'experience ».
TEST( ExerciseSessionControllerTest, a_level_stays_locked_until_experience_opens_it )
{
    domain::NotePlayerFake notePlayer;
    domain::PlayerPreferencesFake levelStore;

    ExerciseSessionController controller{ notePlayer, {}, {}, {}, {}, &levelStore };

    // A ZERO experience, seul le premier palier est ouvert : il n'y a rien a meriter pour commencer.
    EXPECT_TRUE( controller.isLevelUnlocked( 0 ) );
    EXPECT_FALSE( controller.isLevelUnlocked( 1 ) );
    EXPECT_FALSE( controller.isLevelUnlocked( 4 ) );

    // Et la liste le DIT, parce que l'ecran la lit telle quelle pour barrer les entrees.
    const QVariantList levels = controller.playerLevels();
    ASSERT_FALSE( levels.isEmpty() );

    const QVariantMap firstLevel = levels.at( 0 ).toMap();
    const QVariantMap lastLevel = levels.at( static_cast<int>( domain::PLAYER_LEVEL_COUNT ) - 1 ).toMap();

    EXPECT_FALSE( firstLevel.value( QStringLiteral( "isLocked" ) ).toBool() );
    EXPECT_TRUE( lastLevel.value( QStringLiteral( "isLocked" ) ).toBool() );
}

// LE CODE DE DEVELOPPEUR : sept choix de GodMode d'affilee ouvrent toutes les difficultes, et choisir une VRAIE difficulte
// les referme. C'est ce que Roger a demande - « il faudrait que ce deverrouillage soit temporaire ».
TEST( ExerciseSessionControllerTest, the_developer_code_opens_everything_and_is_temporary )
{
    domain::NotePlayerFake notePlayer;
    domain::PlayerPreferencesFake levelStore;

    ExerciseSessionController controller{ notePlayer, {}, {}, {}, {}, &levelStore };

    constexpr int GOD_MODE_INDEX = static_cast<int>( domain::PLAYER_LEVEL_COUNT );

    EXPECT_FALSE( controller.areAllLevelsUnlocked() );

    // SIX ne suffisent pas : le code demande SEPT.
    for( int selection = 0; selection < 6; ++selection )
    {
        controller.choosePlayerLevel( GOD_MODE_INDEX );
    }

    EXPECT_FALSE( controller.areAllLevelsUnlocked() );

    // La SEPTIEME l'ouvre, et toutes les difficultes sont alors permises.
    controller.choosePlayerLevel( GOD_MODE_INDEX );

    EXPECT_TRUE( controller.areAllLevelsUnlocked() );
    EXPECT_TRUE( controller.isLevelUnlocked( 1 ) );
    EXPECT_TRUE( controller.isLevelUnlocked( 4 ) );

    // ET C'EST TEMPORAIRE : choisir une vraie difficulte referme le code. L'experience, elle, n'a pas bouge - donc ce qui
    // se referme est bien le raccourci, pas un merite qui aurait disparu.
    controller.choosePlayerLevel( 0 );

    EXPECT_FALSE( controller.areAllLevelsUnlocked() );
    EXPECT_FALSE( controller.isLevelUnlocked( 4 ) );
}

// REJOUER garde le MEME mode : un Entrainement ne doit pas devenir une Arcade en silence - l'ecran de fin appelait l'Arcade
// quoi qu'il arrive, ce qui gagnait de l'experience au moment ou l'on venait de comprendre que non.
TEST( ExerciseSessionControllerTest, replaying_keeps_the_mode )
{
    domain::NotePlayerFake notePlayer;
    domain::PlayerPreferencesFake levelStore;

    ExerciseSessionController controller{ notePlayer, {}, {}, {}, {}, &levelStore };

    constexpr int MODES_FAMILY = 2;

    controller.startTrainingSession( MODES_FAMILY );

    EXPECT_EQ( static_cast<int>( domain::GameMode::Training ), controller.gameMode() );
    EXPECT_FALSE( controller.sessionGrantsExperience() );

    controller.restartSession();

    EXPECT_EQ( static_cast<int>( domain::GameMode::Training ), controller.gameMode() );
    EXPECT_FALSE( controller.sessionGrantsExperience() );

    // Et l'Arcade rejoue l'Arcade, elle qui paie.
    controller.startSession();

    EXPECT_EQ( static_cast<int>( domain::GameMode::Arcade ), controller.gameMode() );
    EXPECT_TRUE( controller.sessionGrantsExperience() );

    controller.restartSession();

    EXPECT_EQ( static_cast<int>( domain::GameMode::Arcade ), controller.gameMode() );
    EXPECT_TRUE( controller.sessionGrantsExperience() );
}

// LES COEURS D'ARCADE sont reglables, et bornes : c'est le raccourci que Roger a demande pour enfin voir le boss.
TEST( ExerciseSessionControllerTest, the_arcade_hearts_are_configurable )
{
    domain::NotePlayerFake notePlayer;
    domain::PlayerPreferencesFake levelStore;

    ExerciseSessionController controller{ notePlayer, {}, {}, {}, {}, &levelStore };

    // Dix par defaut : c'est la valeur d'installation.
    EXPECT_EQ( 10, controller.arcadeLives() );

    controller.setArcadeLives( 25 );
    EXPECT_EQ( 25, controller.arcadeLives() );

    // Et les bornes tiennent : un fichier edite a la main, ou un curseur pousse trop loin, ne casse pas une partie.
    controller.setArcadeLives( 99 );
    EXPECT_EQ( 25, controller.arcadeLives() );

    controller.setArcadeLives( 0 );
    EXPECT_EQ( 1, controller.arcadeLives() );
}

TEST( ExerciseSessionControllerTest, the_reward_announcement_is_cleared_at_every_session_start )
{
    // LE BADGE DE FIN DE PARTIE, et ce qui n'allait pas.
    //
    // Roger : « a la victoire d'arcade, je vois constamment que j'ai gagne le nouveau titre : Toutou. » Deux fautes s'y
    // ajoutaient : l'ecran lisait `titleJustIncreased` SANS parentheses - une methode non appelee est un objet fonction,
    // donc toujours vrai - et aucune propriete n'etait notifiable, donc la liaison restait figee.
    //
    // Ce que ce test tient est la seconde moitie : l'annonce est remise a ZERO au debut de chaque partie, et l'ecran
    // doit l'APPRENDRE. Sans le signal, le badge du bilan precedent resterait affiche par-dessus la partie suivante - et
    // une arcade, qui n'accorde aucun titre, montrerait celui d'hier.
    domain::NotePlayerFake notePlayer;

    ExerciseSessionController controller{ notePlayer, { intervalOnlySettings() } };

    int announcements = 0;

    QObject::connect( &controller,
                      &ExerciseSessionController::playerProgressChanged,
                      &controller,
                      [&announcements]() { ++announcements; } );

    controller.startOrdinarySession();

    EXPECT_FALSE( controller.titleJustIncreased() );
    EXPECT_TRUE( controller.newlyEarnedTrophies().isEmpty() );

    // UNE FOIS, et pas zero : ce n'est pas la valeur qui compte ici, c'est que l'ecran soit PREVENU.
    EXPECT_EQ( 1, announcements );
}

// LES TITRES ET LES TROPHEES se lisent des compteurs du Bilan, et rien d'autre.
TEST( ExerciseSessionControllerTest, the_end_screen_sounds_go_through_to_the_port )
{
    // Le compte qui grimpe et la fanfare de victoire sont du GAME FEEL - Roger les a demandes comme des bruitages
    // « pour rendre le jeu moins austere ». Mais ils passent par le PORT comme tout le reste : c'est ce qui permet a un
    // adaptateur sans son de les ignorer, et a ce test de dire qu'ils ont bien ete demandes.
    //
    // Le PROGRES fait partie de la demande : c'est lui qui fait monter le tic avec le chiffre, et l'oublier rendrait un
    // tic monotone sans que rien ne le signale.
    domain::NotePlayerFake notePlayer;

    ExerciseSessionController controller{ notePlayer, { intervalOnlySettings() } };

    controller.playScoreTick( 40 );
    controller.playVictoryFanfare();

    EXPECT_EQ( 1, notePlayer.scoreTickCount() );
    EXPECT_EQ( 40, notePlayer.lastScoreTickProgress() );
    EXPECT_EQ( 1, notePlayer.victoryFanfareCount() );
}

TEST( ExerciseSessionControllerTest, the_title_and_the_trophies_come_from_the_bilan )
{
    domain::NotePlayerFake notePlayer;
    domain::PlayerPreferencesFake levelStore;

    ExerciseSessionController controller{ notePlayer, {}, {}, {}, {}, &levelStore };

    // Un joueur neuf est un Toutou, et n'a rien fait : aucun trophee.
    EXPECT_EQ( QStringLiteral( "Toutou" ), controller.playerTitle().value( QStringLiteral( "name" ) ).toString() );

    const QVariantList freshTrophies = controller.trophies();
    ASSERT_FALSE( freshTrophies.isEmpty() );

    for( const QVariant & entry : freshTrophies )
    {
        EXPECT_FALSE( entry.toMap().value( QStringLiteral( "earned" ) ).toBool() );
    }

    // Trois bilans reussis d'affilee, et un bilan parfait plus tard : le titre monte, et les trophees tombent.
    levelStore.storeBilanCount( 4 );
    levelStore.storeLongestBilanSuccessStreak( 3 );
    levelStore.storePerfectBilanCount( 1 );

    EXPECT_EQ( QStringLiteral( "Chef de Meute" ), controller.playerTitle().value( QStringLiteral( "name" ) ).toString() );
    EXPECT_FALSE( controller.playerTitle().value( QStringLiteral( "motto" ) ).toString().isEmpty() );

    QVariantList trophies = controller.trophies();

    const auto earnedNamed = [&trophies]( const QString & p_name ) {
        for( const QVariant & entry : trophies )
        {
            if( entry.toMap().value( QStringLiteral( "name" ) ).toString() == p_name )
            {
                return entry.toMap().value( QStringLiteral( "earned" ) ).toBool();
            }
        }

        return false;
    };

    EXPECT_TRUE( earnedNamed( QStringLiteral( "Premier Bilan" ) ) );
    EXPECT_TRUE( earnedNamed( QStringLiteral( "Trois d'affilée" ) ) );
    EXPECT_TRUE( earnedNamed( QStringLiteral( "Bilan parfait" ) ) );
    EXPECT_FALSE( earnedNamed( QStringLiteral( "Cinq d'affilée" ) ) );
    EXPECT_FALSE( earnedNamed( QStringLiteral( "Assidu" ) ) );

    // ET TOUTE L'ECHELLE, que Roger veut voir meme non acquise - « en grise [...] histoire de donner des objectifs ».
    const QVariantList titles = controller.allTitles();

    ASSERT_EQ( static_cast<int>( domain::TITLE_COUNT ), titles.size() );

    // Le titre porte est acquis, ceux d'AU-DESSUS ne le sont pas : c'est ce qui en fait des objectifs.
    EXPECT_TRUE( titles.at( 0 ).toMap().value( QStringLiteral( "earned" ) ).toBool() );     // Toutou
    EXPECT_TRUE( titles.at( 2 ).toMap().value( QStringLiteral( "earned" ) ).toBool() );     // Chef de Meute
    EXPECT_FALSE( titles.at( 3 ).toMap().value( QStringLiteral( "earned" ) ).toBool() );    // Soliste
    EXPECT_FALSE( titles.at( 5 ).toMap().value( QStringLiteral( "earned" ) ).toBool() );    // Ouafstro
}

}    // namespace musichien::ui