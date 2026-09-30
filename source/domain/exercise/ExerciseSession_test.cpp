#include "domain/exercise/ExerciseSession.h"

#include "domain/exercise/LearningOrder.h"
#include "domain/music/Chord.h"
#include "domain/rhythm/RhythmPattern.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <span>

namespace musichien::domain
{

// ---------------------------------------------------------------------------------------------------------------------
// The loop
//
// A whole session is played here, in microseconds, with no sound card and no phone. That is the point
// of having put the loop in the domain: everything below - the grid that closes in, the palette that
// grows, the lives, the star - is checked without anyone having to listen to anything.
// ---------------------------------------------------------------------------------------------------------------------

namespace
{

constexpr std::uint32_t TEST_SEED = 20260926;

// AUCUNE part laissee a sa valeur par defaut.
//
// Les parts sont maintenant des POIDS, lus les uns par rapport aux autres : le tirage se fait sur leur SOMME, et non sur
// cent. Une seule part oubliee a vingt suffit donc a voler un cinquieme des questions - et le piege grandit a chaque
// genre nouveau.
//
// Il a mordu une fois : « rythme seulement » laissait les accords a vingt, la somme faisait cent vingt, et sept tests de
// rythme ne tombaient justes que par l'ordre des comparaisons de l'ancien tirage. Ce helper existe pour que cela ne
// puisse plus arriver : tous les reglages d'un genre l'appellent, puis remontent la seule part qui les interesse.
void pinEveryQuestionShare( SessionSettings & p_settings )
{
    p_settings.namedIntervalQuestionShare = 0;
    p_settings.sameColourQuestionShare = 0;
    p_settings.foreignNoteQuestionShare = 0;
    p_settings.singQuestionShare = 0;
    p_settings.directionQuestionShare = 0;
    p_settings.chordQuestionShare = 0;
    p_settings.modeColourQuestionShare = 0;
    p_settings.modeNameQuestionShare = 0;
    p_settings.modeVampQuestionShare = 0;
}

// Des questions d'INTERVALLE A NOMMER, et rien d'autre.
//
// Aucune part n'est remontee, et c'est tout l'interet : une session sans aucune part pose la question par defaut du
// jeu, qui est justement celle que ces tests parlent. C'est la garde de drawKind qui le garantit, et non un reglage -
// donc un genre ajoute plus tard ne pourra pas se glisser ici sans qu'on l'ait voulu.
//
// Une session reelle melange les genres - chant, rythme, accords - et c'est ce qu'on veut en jouant. Un test qui parle
// d'une GRILLE, d'une PALETTE ou des VIES doit donc savoir de quoi il parle : sans cet epeinglage, il tombe une fois
// sur cinq sur une question qui n'offre aucune grille, et il echoue au hasard. Un test qui echoue au hasard n'apprend
// rien a personne, sinon a etre ignore.
[[nodiscard]] SessionSettings intervalOnlySettings()
{
    SessionSettings settings;
    pinEveryQuestionShare( settings );
    return settings;
}

// The settings of most tests: no limit on mistakes, so that a test about the GRID is not disturbed by
// a test about the lives - et aucune question d'un autre genre que l'intervalle, pour la meme raison.
[[nodiscard]] SessionSettings unlimitedLivesSettings()
{
    SessionSettings settings = intervalOnlySettings();
    settings.lives = std::nullopt;
    return settings;
}

[[nodiscard]] std::int32_t targetOf( const ExerciseSession & p_session )
{
    return p_session.currentQuestion().target.semitones();
}

void answerCorrectly( ExerciseSession & p_session )
{
    p_session.answer( targetOf( p_session ) );
}

// Chooses one of the offered intervals that is not the right answer, which is what a player does when
// they are wrong.
void answerWrongly( ExerciseSession & p_session )
{
    const Question & question = p_session.currentQuestion();

    const auto wrongChoice = std::ranges::find_if( question.choices,
                                                   [&question]( const Interval & p_choice ) { return !( p_choice == question.target ); } );

    ASSERT_NE( question.choices.end(), wrongChoice ) << "a grid offered nothing but the right answer";

    p_session.answer( wrongChoice->semitones() );
}

// Plays a whole session, always right, and lets the caller see the result.
void playCorrectly( ExerciseSession & p_session, std::size_t p_questionCount )
{
    for( std::size_t index = 0; index < p_questionCount; ++index )
    {
        answerCorrectly( p_session );
        p_session.advance();
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Le rythme
// ---------------------------------------------------------------------------------------------------------------------

// Une session qui ne pose QUE des questions de rythme.
//
// Le rythme a sa part de tirage, comme le chant : un test qui parle d'une cellule epingle donc les DEUX autres parts,
// sinon la session lui poserait un intervalle une fois sur cinq.
[[nodiscard]] SessionSettings rhythmOnlySettings()
{
    SessionSettings settings;
    pinEveryQuestionShare( settings );
    return settings;
}

// La cellule de la question en cours, telle que le joueur l'entend.
[[nodiscard]] const RhythmPattern & patternOf( const ExerciseSession & p_session )
{
    return allRhythmPatterns().at( p_session.currentQuestion().patternIndex );
}

// Reproduit la cellule : une frappe sur chacune de ses frappes, pile dessus.
void playTheCellCorrectly( ExerciseSession & p_session )
{
    for( const RhythmHit & hit : patternOf( p_session ).hits() )
    {
        p_session.registerRhythmTap( hit.beat );
    }
}

// La place la plus loin possible de TOUTE frappe de la cellule.
//
// Sert a taper a cote, et de facon sure : la cellule est tiree au hasard, donc une position choisie a la main tomberait
// a cote sur une cellule et pile sur une autre. Le balayage est court, et parfaitement deterministe.
[[nodiscard]] double furthestPositionFromAnyHitInBeats( const RhythmPattern & p_pattern )
{
    const double loopLength = static_cast<double>( p_pattern.beatsPerBar() );

    constexpr int STEP_COUNT = 1000;

    double bestPosition = 0.0;
    double bestDistance = -1.0;

    for( int step = 0; step < STEP_COUNT; ++step )
    {
        const double position = ( static_cast<double>( step ) / static_cast<double>( STEP_COUNT ) ) * loopLength;

        const double distance = distanceToNearestOnsetInBeats( p_pattern, position );

        if( distance > bestDistance )
        {
            bestDistance = distance;
            bestPosition = position;
        }
    }

    return bestPosition;
}

// ---------------------------------------------------------------------------------------------------------------------
// Les accords
// ---------------------------------------------------------------------------------------------------------------------

// Une session qui ne pose QUE des questions d'accords.
[[nodiscard]] SessionSettings chordOnlySettings()
{
    SessionSettings settings;
    pinEveryQuestionShare( settings );
    settings.chordQuestionShare = 100;
    return settings;
}

// La couleur de l'accord demande.
[[nodiscard]] ChordQuality askedChordQuality( const ExerciseSession & p_session )
{
    return p_session.currentQuestion().chord.quality;
}

// Une reponse fausse : une AUTRE couleur de ce que le joueur a le droit de repondre.
[[nodiscard]] ChordQuality wrongChordAnswer( const ExerciseSession & p_session )
{
    const std::vector<ChordQuality> & choices = p_session.currentQuestion().chordChoices;

    const auto wrong = std::ranges::find_if( choices, [&p_session]( ChordQuality p_quality ) {
        return p_quality != p_session.currentQuestion().chord.quality;
    } );

    return ( wrong != choices.end() ) ? *wrong : p_session.currentQuestion().chord.quality;
}

// Repond juste, sur une question d'accord.
void answerChordCorrectly( ExerciseSession & p_session )
{
    p_session.answerChord( askedChordQuality( p_session ) );
}

// Une session d'accords avec de quoi RETIRER un leurre : six couleurs, la ou le niveau d'un debutant n'en offre que deux.
//
// Six, et pas quinze : il faut assez de leurres pour que l'indice ait un sens, et assez peu pour qu'un test se lise.
[[nodiscard]] SessionSettings wideChordSettings()
{
    SessionSettings settings = chordOnlySettings();
    settings.startingChordQualityCount = 6;

    return settings;
}

}    // namespace

TEST( ExerciseSessionTest, a_session_asks_its_first_question_immediately )
{
    const ExerciseSession session{ TEST_SEED };

    // No start() call to forget: a session that exists is asking something.
    EXPECT_EQ( SessionState::Asking, session.state() );
    EXPECT_EQ( 1, session.questionNumber() );
    EXPECT_FALSE( session.isFinished() );
}

TEST( ExerciseSessionTest, the_right_answer_is_always_offered )
{
    // Des questions d'intervalle uniquement : ce test parle de la GRILLE.
    ExerciseSession session{ TEST_SEED, intervalOnlySettings() };

    for( std::size_t index = 0; index < 10; ++index )
    {
        const Question & question = session.currentQuestion();

        EXPECT_NE( question.choices.end(), std::ranges::find( question.choices, question.target ) );

        // And never alone: a question with a single choice is not a question.
        EXPECT_GE( question.choices.size(), 2 );

        answerCorrectly( session );
        session.advance();
    }
}

TEST( ExerciseSessionTest, a_wrong_answer_asks_the_same_question_again )
{
    // Une question d'intervalle : ce test repond a cote d'une cible, donc il lui faut une cible d'intervalle.
    ExerciseSession session{ TEST_SEED, intervalOnlySettings() };

    const std::int32_t firstTarget = targetOf( session );
    const std::size_t firstQuestionNumber = session.questionNumber();

    EXPECT_FALSE( session.answer( firstTarget + 1 ) );
    EXPECT_EQ( SessionState::Asking, session.state() );
    EXPECT_EQ( firstTarget, targetOf( session ) );
    EXPECT_EQ( firstQuestionNumber, session.questionNumber() );
}

TEST( ExerciseSessionTest, a_wrong_answer_makes_the_grid_smaller )
{
    SessionSettings settings = unlimitedLivesSettings();
    settings.startingPaletteSize = 5;

    ExerciseSession session{ TEST_SEED, settings };

    const std::size_t firstGridSize = session.currentQuestion().choices.size();

    ASSERT_EQ( 5, firstGridSize );

    answerWrongly( session );

    // "La grille qui s'aide": each mistake removes the least plausible of the wrong answers, so the
    // same question gets easier the longer it is struggled with. The player is helped without having
    // to ask for anything.
    EXPECT_EQ( firstGridSize - 1, session.currentQuestion().choices.size() );

    // And the right answer is still there: help never removes what the player is looking for.
    EXPECT_NE( session.currentQuestion().choices.end(),
               std::ranges::find( session.currentQuestion().choices, session.currentQuestion().target ) );
}

TEST( ExerciseSessionTest, the_grid_never_shrinks_below_two_choices )
{
    SessionSettings settings = unlimitedLivesSettings();
    settings.startingPaletteSize = 5;

    ExerciseSession session{ TEST_SEED, settings };

    for( std::size_t index = 0; index < 10; ++index )
    {
        answerWrongly( session );
    }

    EXPECT_EQ( 2, session.currentQuestion().choices.size() );
}

TEST( ExerciseSessionTest, the_help_is_offered_only_after_three_wrong_answers )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    EXPECT_FALSE( session.isHelpAvailable() );

    answerWrongly( session );
    answerWrongly( session );

    EXPECT_FALSE( session.isHelpAvailable() );

    answerWrongly( session );

    EXPECT_TRUE( session.isHelpAvailable() );

    // And it takes no life to ask: help is not a mistake.
    EXPECT_EQ( 0, session.score().streak() );
}

TEST( ExerciseSessionTest, the_palette_grows_every_three_successes )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    ASSERT_EQ( 2, session.palette().size() );

    answerCorrectly( session );
    session.advance();

    answerCorrectly( session );
    session.advance();

    ASSERT_EQ( 2, session.palette().size() );

    answerCorrectly( session );

    // Three in a row: one more interval to deal with. Note that the question ALREADY on screen keeps
    // the grid it was asked with, which is right: a grid that changed under the finger of the player
    // who has just answered would be a magic trick, not a progression.
    EXPECT_EQ( 3, session.palette().size() );

    session.advance();

    // The next question is the one that offers the new interval, and a fourth button with it.
    EXPECT_EQ( 3, session.currentQuestion().choices.size() );
}

TEST( ExerciseSessionTest, a_helped_question_takes_the_newest_interval_back )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    answerCorrectly( session );
    session.advance();

    answerCorrectly( session );
    session.advance();

    answerCorrectly( session );
    session.advance();

    ASSERT_EQ( 3, session.palette().size() );

    session.revealAnswer();

    // The player asked to be told: they were not ready for the newest interval, so it steps back out.
    // The questions that follow are asked on ground they can stand on.
    EXPECT_EQ( 2, session.palette().size() );
}

TEST( ExerciseSessionTest, the_palette_never_shrinks_below_where_the_player_started )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    ASSERT_EQ( 2, session.settings().startingPaletteSize );

    session.revealAnswer();

    // Someone already at the beginning would be left with nothing to play with.
    EXPECT_EQ( 2, session.palette().size() );
}

TEST( ExerciseSessionTest, a_helped_question_moves_on_to_the_next_question )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    session.revealAnswer();

    EXPECT_EQ( SessionState::Feedback, session.state() );
    EXPECT_FALSE( session.wasLastAnswerCorrect() );

    session.advance();

    EXPECT_EQ( SessionState::Asking, session.state() );
    EXPECT_EQ( 2, session.questionNumber() );
}

TEST( ExerciseSessionTest, ten_questions_end_the_session )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    playCorrectly( session, 10 );

    EXPECT_TRUE( session.isFinished() );
    EXPECT_EQ( SessionState::Finished, session.state() );

    // The last question number, and not eleven: a screen prints "10 / 10" without a special case.
    EXPECT_EQ( 10, session.questionNumber() );
}

TEST( ExerciseSessionTest, five_wrong_answers_end_a_session_that_has_five_lives )
{
    // Des questions d'intervalle : ce test cherche une MAUVAISE reponse dans la grille, donc il lui faut une grille -
    // et les cinq vies par defaut, qui sont le sujet du test.
    ExerciseSession session{ TEST_SEED, intervalOnlySettings() };

    ASSERT_EQ( 5, session.settings().lives.value() );

    for( std::int32_t index = 0; index < 4; ++index )
    {
        answerWrongly( session );
    }

    ASSERT_FALSE( session.isFinished() );
    EXPECT_EQ( 1, session.score().remainingLives().value() );

    answerWrongly( session );

    // La derniere vie s'en va, mais la question merite encore sa REPONSE : savoir ce qu'on a rate est la seule chose
    // qui reste a apprendre quand la partie est perdue.
    //
    // Ce n'etait pas le comportement d'origine, et Roger l'a renverse : « quand on perds, on arrive direct a la page
    // des scores, mais on n'a pas la reponse a la question sur laquelle on a fail ». La session passe donc en Feedback
    // comme n'importe quelle question conclue, et c'est advance() - qui sait deja le faire - qui la termine.
    EXPECT_FALSE( session.isFinished() );
    EXPECT_EQ( SessionState::Feedback, session.state() );

    session.advance();

    EXPECT_TRUE( session.isFinished() );
}

TEST( ExerciseSessionTest, a_perfect_session_earns_its_star )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    playCorrectly( session, 10 );

    EXPECT_TRUE( session.hasEarnedStar() );
}

TEST( ExerciseSessionTest, the_star_is_not_given_before_the_last_question )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    playCorrectly( session, 5 );

    ASSERT_FALSE( session.isFinished() );

    // Five flawless answers are not a session. A star that appeared in the middle of one would say
    // "you are done" while there is still work to do.
    EXPECT_FALSE( session.hasEarnedStar() );
}

TEST( ExerciseSessionTest, nothing_changes_once_the_session_is_over )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    playCorrectly( session, 10 );

    ASSERT_TRUE( session.isFinished() );

    const std::int32_t earned = session.score().experience();

    EXPECT_FALSE( session.answer( 0 ) );

    session.advance();

    EXPECT_EQ( earned, session.score().experience() );
    EXPECT_EQ( 10, session.questionNumber() );
}

TEST( ExerciseSessionTest, a_replay_is_counted_then_costs_experience )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    session.registerReplay();
    session.registerReplay();

    EXPECT_EQ( 2, session.currentQuestion().replayCount );

    answerCorrectly( session );

    // Replaying is allowed, and not free: the reward is reduced, never erased.
    EXPECT_LT( session.score().experience(), SessionScore::BASE_XP_PER_SUCCESS );
    EXPECT_GE( session.score().experience(), SessionScore::MINIMUM_XP_PER_SUCCESS );
}

TEST( ExerciseSessionTest, a_replay_is_ignored_once_the_answer_is_known )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    answerCorrectly( session );

    ASSERT_FALSE( session.canReplay() );

    session.registerReplay();

    // The feedback replays the interval by itself; counting that one would charge the player for
    // something they did not ask for.
    EXPECT_EQ( 0, session.currentQuestion().replayCount );
}

TEST( ExerciseSessionTest, an_answer_arriving_after_the_question_is_over_scores_nothing )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    answerCorrectly( session );

    const std::int32_t earned = session.score().experience();

    // The same answer, tapped twice: it must not be able to score twice.
    EXPECT_FALSE( session.answer( targetOf( session ) ) );

    EXPECT_EQ( earned, session.score().experience() );
}

TEST( ExerciseSessionTest, the_root_note_moves_from_one_question_to_the_next )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    std::set<std::int32_t> roots;

    for( std::size_t index = 0; index < 10; ++index )
    {
        roots.insert( session.currentQuestion().rootMidiNumber );

        answerCorrectly( session );
        session.advance();
    }

    // A session that always started on the same note would teach the sound of that note as much as the
    // interval, and the player would end up recognising the key rather than the distance.
    EXPECT_GT( roots.size(), 1 );
}

TEST( ExerciseSessionTest, both_notes_stay_inside_the_playable_range )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    const SessionSettings & settings = session.settings();

    for( std::size_t index = 0; index < 10; ++index )
    {
        const Question & question = session.currentQuestion();

        const std::int32_t firstNote = question.rootMidiNumber;

        // The second note depends on the DIRECTION, and getting this wrong in the code is silent: the
        // notes would simply be played at the edge of what a phone speaker can do, which sounds like a
        // slightly odd question rather than like a bug.
        const std::int32_t secondNote =
          ( question.direction == IntervalDirection::Descending )
            ? ( firstNote - question.target.semitones() )
            : ( firstNote + question.target.semitones() );

        EXPECT_GE( std::min( firstNote, secondNote ), settings.lowestPlayableMidiNumber );
        EXPECT_LE( std::max( firstNote, secondNote ), settings.highestPlayableMidiNumber );

        answerCorrectly( session );
        session.advance();
    }
}

TEST( ExerciseSessionTest, the_three_directions_all_come_up_in_a_session )
{
    // A long session, so that the draw has every chance to show all three. One interval heard only
    // upwards is half an interval: descending is the same distance heard the other way and a separate
    // skill, and the harmonic form leaves only the colour.
    SessionSettings settings = unlimitedLivesSettings();
    settings.questionCount = 200;

    ExerciseSession session{ TEST_SEED, settings };

    std::size_t ascendingCount = 0;
    std::size_t descendingCount = 0;
    std::size_t harmonicCount = 0;

    for( std::size_t index = 0; index < settings.questionCount; ++index )
    {
        switch( session.currentQuestion().direction )
        {
            case IntervalDirection::Ascending:
                ++ascendingCount;
                break;

            case IntervalDirection::Descending:
                ++descendingCount;
                break;

            case IntervalDirection::Harmonic:
                ++harmonicCount;
                break;
        }

        answerCorrectly( session );
        session.advance();
    }

    EXPECT_GT( ascendingCount, 0 );
    EXPECT_GT( descendingCount, 0 );
    EXPECT_GT( harmonicCount, 0 );
}

// ---------------------------------------------------------------------------------------------------------------------
// The memory hint
//
// The hint is a NUDGE where the "Réponse" button is a rescue, and the whole design is in the timing: too
// early it is the answer in disguise, too late it arrives after the player has given up. The two
// thresholds are therefore asserted against each other, not one at a time.
// ---------------------------------------------------------------------------------------------------------------------

TEST( ExerciseSessionTest, the_hint_waits_for_a_mistake_and_leaves_with_the_question )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    // Nothing before the player has tried.
    EXPECT_FALSE( session.isHintAvailable() );

    answerWrongly( session );

    EXPECT_TRUE( session.isHintAvailable() );

    // The next question starts clean: a hint belongs to a question, not to a session.
    answerCorrectly( session );
    session.advance();

    EXPECT_FALSE( session.isHintAvailable() );
}

TEST( ExerciseSessionTest, the_hint_is_offered_before_the_answer_is )
{
    SessionSettings settings = unlimitedLivesSettings();

    // Enough intervals on the grid that three wrong answers in a row are possible at all: the grid closes
    // in on every mistake, which is the subject of its own test.
    settings.startingPaletteSize = 5;

    ExerciseSession session{ TEST_SEED, settings };

    answerWrongly( session );

    // At the first mistake: the nudge.
    EXPECT_TRUE( session.isHintAvailable() );
    EXPECT_FALSE( session.isHelpAvailable() );

    answerWrongly( session );
    answerWrongly( session );

    // At the third: the rescue.
    EXPECT_TRUE( session.isHelpAvailable() );
}

TEST( ExerciseSessionTest, a_session_without_aids_gives_none_however_hard_the_player_tries )
{
    SessionSettings settings = unlimitedLivesSettings();
    settings.aidsAllowed = false;

    ExerciseSession session{ TEST_SEED, settings };

    answerWrongly( session );
    answerWrongly( session );
    answerWrongly( session );
    answerWrongly( session );

    // Un mode sans filet est un mode sans filet : l'indice ne souffle pas, et la reponse ne se donne pas -
    // meme apres quatre essais, et meme si les seuils de settings disent le contraire. C'est le DOMAINE qui
    // decide, donc aucun ecran ne peut oublier de verifier.
    EXPECT_FALSE( session.isHintAvailable() );
    EXPECT_FALSE( session.isHelpAvailable() );
}

TEST( ExerciseSessionTest, the_whole_palette_stays_whole_however_many_mistakes_are_made )
{
    SessionSettings settings = unlimitedLivesSettings();
    settings.startingPaletteSize = learningOrderIntervals().size();

    ExerciseSession session{ TEST_SEED, settings };

    const std::size_t wholePalette = session.palette().size();

    ASSERT_EQ( learningOrderIntervals().size(), wholePalette );

    // Une erreur retire d'ordinaire le dernier intervalle arrive dans la palette. Dans un mode ou la carte est
    // complete des la premiere question, il n'y a pas de "dernier arrive" : la carte doit rester entiere.
    //
    // C'est narrowPalette qui s'en charge, en ne descendant jamais sous la taille de depart - et c'est
    // exactement le genre de regle qui casse en silence le jour ou quelqu'un la "simplifie".
    answerWrongly( session );
    answerWrongly( session );

    EXPECT_EQ( wholePalette, session.palette().size() );
}

TEST( ExerciseSessionTest, a_guided_question_asks_the_direction_and_takes_a_direction )
{
    SessionSettings settings = unlimitedLivesSettings();

    settings.directionQuestionShare = 100;

    ExerciseSession session{ TEST_SEED, settings };

    // Le mode guide ne peut pas demander un intervalle harmonique : "monte ou descend ?" n'a pas de sens sur un
    // accord.
    EXPECT_EQ( QuestionKind::Direction, session.currentQuestion().kind );
    EXPECT_NE( IntervalDirection::Harmonic, session.currentQuestion().direction );

    // La bonne direction est une bonne reponse.
    EXPECT_TRUE( session.answerDirection( session.currentQuestion().direction ) );

    // Et une direction sur une question qui demandait un nom est un autre langage : elle ne compte pas.
    ExerciseSession namedSession{ TEST_SEED, unlimitedLivesSettings() };

    EXPECT_EQ( QuestionKind::NamedInterval, namedSession.currentQuestion().kind );
    EXPECT_FALSE( namedSession.answerDirection( IntervalDirection::Ascending ) );
}

TEST( ExerciseSessionTest, two_mistakes_turn_the_question_in_progress_guided )
{
    SessionSettings settings = unlimitedLivesSettings();

    // Une direction forcee : le basculement guide n'a pas de sens sur un intervalle harmonique, donc le test pin
    // la question en montant.
    settings.ascendingShare = 100;
    settings.descendingShare = 0;
    settings.harmonicShare = 0;

    ExerciseSession session{ TEST_SEED, settings };

    // Deux erreurs sur la question en cours...
    answerWrongly( session );
    answerWrongly( session );

    // ...et c'est CETTE question, pas la suivante, qui devient guidee - comme un indice.
    EXPECT_EQ( QuestionKind::Direction, session.currentQuestion().kind );
}

TEST( ExerciseSessionTest, a_session_can_ask_to_sing_instead_of_naming )
{
    SessionSettings settings;
    pinEveryQuestionShare( settings );
    settings.singQuestionShare = 100;    // toute la session est chantee

    ExerciseSession session{ 7, settings };

    EXPECT_EQ( QuestionKind::Sing, session.currentQuestion().kind );

    // Une question chantee n'offre rien a choisir : la voix est la reponse, et elle peut etre juste...
    EXPECT_TRUE( session.answerSung( true ) );
    EXPECT_EQ( 1, session.score().streak() );
}

TEST( ExerciseSessionTest, a_sung_question_can_be_passed_at_once )
{
    SessionSettings settings;
    pinEveryQuestionShare( settings );
    settings.singQuestionShare = 100;    // toute la session est chantee

    ExerciseSession session{ 7, settings };

    ASSERT_EQ( QuestionKind::Sing, session.currentQuestion().kind );

    // On peut ne pas etre en mesure de chanter du tout : l'endroit est bruyant, la gorge est prise, le micro ne suit
    // pas. La sortie est donc offerte TOUT DE SUITE - sans attendre une erreur, et sans qu'un mode doive l'autoriser.
    // Demander au joueur de rater une question pour avoir le droit de la passer serait une cruaute gratuite.
    EXPECT_TRUE( session.isHelpAvailable() );

    const std::size_t helpedBefore = session.score().helpedQuestionCount();

    session.revealAnswer();

    // Passer coute quelque chose, et le domaine le dit : la question est comptee comme AIDEE, la serie retombe a zero,
    // et la suite se joue sur un terrain plus sur. C'est ce qui distingue une porte d'une recompense - et c'est pour
    // cela que le bouton peut etre offert tout de suite sans rien casser au jeu.
    EXPECT_EQ( SessionState::Feedback, session.state() );
    EXPECT_EQ( helpedBefore + 1, session.score().helpedQuestionCount() );
    EXPECT_EQ( 0, session.score().streak() );
}

// Le rythme est entre dans la session comme un GENRE de question, au meme titre que le chant : la meme boucle, le meme
// score, les memes vies. Tout ce qui suit se joue donc sans une seule seconde d'attente - consequence directe du fait
// que le domaine ne mesure pas le temps : il RECOIT la position des frappes, et il les juge.
// ---------------------------------------------------------------------------------------------------------------------

TEST( ExerciseSessionTest, every_genre_comes_out_when_every_part_is_equal )
{
    // Le defaut que Roger a trouve, et il etait invisible : six parts a vingt font une somme de CENT VINGT, et l'ancien
    // tirage - borne par cent - faisait disparaitre les deux derniers genres de la liste. On demandait du vamp dans les
    // reglages, et il n'en venait jamais.
    //
    // Six parts egales doivent donc donner SEPT genres. Le test prend une centaine de graines differentes et ne lit que
    // la PREMIERE question de chacune : avancer demanderait de savoir repondre a tous les genres, ce qui n'est pas le
    // sujet ici - le tirage l'est.
    // Six parts egales doivent donc donner SIX genres : le rythme en a ete retire, et c'est le VAMP qui reste en dernier
    // dans l'ordre du tirage - c'est lui qui manquait avant la correction.
    //
    // Le test prend deux cents graines differentes et ne lit que la PREMIERE question de chacune : avancer demanderait de
    // savoir repondre a tous les genres, ce qui n'est pas le sujet ici - le tirage l'est.
    SessionSettings settings;
    pinEveryQuestionShare( settings );

    settings.namedIntervalQuestionShare = 20;
    settings.singQuestionShare = 20;
    settings.chordQuestionShare = 20;
    settings.modeColourQuestionShare = 20;
    settings.modeNameQuestionShare = 20;
    settings.modeVampQuestionShare = 20;

    std::set<QuestionKind> seenKinds;

    constexpr std::uint32_t SEED_COUNT = 200;

    for( std::uint32_t seed = 1; seed <= SEED_COUNT; ++seed )
    {
        const ExerciseSession session{ seed, settings };

        seenKinds.insert( session.currentQuestion().kind );
    }

    // Les six, et pas cinq : c'est le VAMP qui manquait, et c'est lui qui tombe en dernier dans l'ordre du tirage.
    EXPECT_EQ( 6U, seenKinds.size() );
}

// ---------------------------------------------------------------------------------------------------------------------
// Les accords, comme question
//
// Meme boucle, meme score, memes vies : un accord est un troisieme genre de question, et il se juge aussi vite qu'un
// intervalle - parce qu'un accord est une liste de distances, et que cette liste vit dans le domaine.
// ---------------------------------------------------------------------------------------------------------------------

TEST( ExerciseSessionTest, a_chord_question_offers_only_the_colours_the_player_knows )
{
    const ExerciseSession session{ TEST_SEED, chordOnlySettings() };

    const Question & question = session.currentQuestion();

    EXPECT_EQ( QuestionKind::Chord, question.kind );

    // Deux couleurs au depart, et ce sont majeur et mineur : c'est ce que la palette d'un debutant contient.
    ASSERT_EQ( 2U, question.chordChoices.size() );
    EXPECT_EQ( ChordQuality::Major, question.chordChoices.at( 0 ) );
    EXPECT_EQ( ChordQuality::Minor, question.chordChoices.at( 1 ) );

    // La bonne reponse est TOUJOURS l'un des choix : une question dont la reponse n'est pas proposee n'est pas une
    // question, c'est un piege.
    EXPECT_NE( question.chordChoices.end(), std::ranges::find( question.chordChoices, question.chord.quality ) );

    // Et l'accord a des notes : la tonique en bas, le reste au-dessus.
    EXPECT_GE( question.chord.notes().size(), 3U );

    // Un accord n'offre pas de grille d'intervalles : deux listes de choix, deux questions differentes.
    EXPECT_TRUE( question.choices.empty() );
}

TEST( ExerciseSessionTest, naming_the_right_colour_wins_the_question )
{
    ExerciseSession session{ TEST_SEED, chordOnlySettings() };

    answerChordCorrectly( session );

    EXPECT_EQ( SessionState::Feedback, session.state() );
    EXPECT_EQ( 1, session.score().streak() );

    // Et la couleur nommee est retenue, pour que le verdict puisse la montrer.
    ASSERT_TRUE( session.lastChordAnswer().has_value() );
    EXPECT_TRUE( session.lastChordAnswer() == session.currentQuestion().chord.quality );
}

TEST( ExerciseSessionTest, naming_another_colour_keeps_the_question_and_loses_a_life )
{
    SessionSettings settings = chordOnlySettings();

    // DEUX vies, et pas une : avec une seule, perdre la vie termine la session, et le test ne pourrait plus rien dire
    // de la question qui reste posee - ce qui est justement ce qu'il verifie.
    settings.lives = 2;

    ExerciseSession session{ TEST_SEED, settings };

    const ChordQuality wrong = wrongChordAnswer( session );

    // Le prealable du test : il y a bien une autre couleur a repondre.
    ASSERT_NE( askedChordQuality( session ), wrong );

    EXPECT_FALSE( session.answerChord( wrong ) );

    // La question reste posee : un accord rate se retente, et il est rejoue.
    EXPECT_EQ( SessionState::Asking, session.state() );

    // La mauvaise reponse est retenue, elle aussi : c'est elle que le verdict oppose a la bonne.
    ASSERT_TRUE( session.lastChordAnswer().has_value() );
    EXPECT_TRUE( *session.lastChordAnswer() == wrong );

    answerChordCorrectly( session );

    EXPECT_EQ( SessionState::Feedback, session.state() );
}

TEST( ExerciseSessionTest, naming_a_colour_on_a_question_that_is_not_an_accord_judges_nothing )
{
    ExerciseSession session{ TEST_SEED, unlimitedLivesSettings() };

    EXPECT_EQ( QuestionKind::NamedInterval, session.currentQuestion().kind );

    // Trois langues, trois questions : nommer une couleur d'accord la ou un intervalle a ete joue ne repond a rien.
    EXPECT_FALSE( session.answerChord( ChordQuality::Minor ) );
    EXPECT_EQ( SessionState::Asking, session.state() );
}

TEST( ExerciseSessionTest, a_chord_never_turns_into_a_direction_question )
{
    SessionSettings settings = chordOnlySettings();
    settings.lives = std::nullopt;
    settings.ascendingShare = 100;

    ExerciseSession session{ TEST_SEED, settings };

    for( int attempt = 0; attempt < 3; ++attempt )
    {
        // Une autre couleur que la bonne, trois fois de suite.
        EXPECT_FALSE( session.answerChord( wrongChordAnswer( session ) ) );
    }

    // "Ca monte ou ca descend ?" ne veut rien dire d'un accord : la question reste une question d'accord, et elle
    // attend une couleur.
    EXPECT_EQ( QuestionKind::Chord, session.currentQuestion().kind );
}

TEST( ExerciseSessionTest, the_chord_palette_widens_with_successes )
{
    ExerciseSession session{ TEST_SEED, chordOnlySettings() };

    // Un debutant commence par deux couleurs, et c'est la MEME progression que les intervalles : trois succes, et une
    // couleur de plus.
    ASSERT_EQ( 2U, session.chordPalette().size() );

    for( int question = 0; question < 3; ++question )
    {
        answerChordCorrectly( session );
        session.advance();
    }

    ASSERT_EQ( 3U, session.chordPalette().size() );

    // Et la couleur arrivee est la SUIVANTE de l'ordre d'apprentissage, jamais une tiree au hasard : c'est ce qui rend
    // la progression previsible pour le joueur.
    EXPECT_EQ( ChordQuality::Sus4, session.chordPalette().at( 2 ) );

    // Elle est desormais proposee, et elle peut tomber : le tirage peut la demander.
    EXPECT_EQ( 3U, session.currentQuestion().chordChoices.size() );
}

TEST( ExerciseSessionTest, passing_a_chord_question_does_not_take_an_interval_away )
{
    SessionSettings settings = chordOnlySettings();
    settings.lives = std::nullopt;
    settings.startingPaletteSize = 6;

    ExerciseSession session{ TEST_SEED, settings };

    const std::size_t intervalPaletteBefore = session.palette().size();

    session.revealAnswer();

    // Passer un accord n'apprend rien sur les intervalles : la palette d'intervalles ne recule pas. C'est une
    // consequence que le joueur ne pourrait relier a rien, et elle est donc refusee.
    EXPECT_EQ( intervalPaletteBefore, session.palette().size() );
    EXPECT_EQ( SessionState::Feedback, session.state() );
}

TEST( ExerciseSessionTest, the_chord_share_decides_whether_an_accord_is_asked )
{
    // Zero : jamais d'accord, meme sur trente questions.
    SessionSettings withoutChords = unlimitedLivesSettings();
    withoutChords.questionCount = 30;

    ExerciseSession intervalSession{ TEST_SEED, withoutChords };

    for( std::size_t index = 0; index < 30; ++index )
    {
        EXPECT_NE( QuestionKind::Chord, intervalSession.currentQuestion().kind );

        answerCorrectly( intervalSession );
        intervalSession.advance();
    }

    // Cent : toutes les questions en sont.
    SessionSettings onlyChords = chordOnlySettings();
    onlyChords.questionCount = 30;

    ExerciseSession chordSession{ TEST_SEED, onlyChords };

    for( std::size_t index = 0; index < 30; ++index )
    {
        EXPECT_EQ( QuestionKind::Chord, chordSession.currentQuestion().kind );

        answerChordCorrectly( chordSession );

        EXPECT_TRUE( chordSession.wasLastAnswerCorrect() );

        chordSession.advance();
    }

    EXPECT_TRUE( chordSession.isFinished() );
}

// ---------------------------------------------------------------------------------------------------------------------
// Le plan : une session qui suit un ORDRE decide
//
// C'est ce qui rend un Bilan possible : une suite qui commence par ce que le joueur reussit et finit par ce qui lui
// resiste. Le domaine ne sait pas POURQUOI l'ordre est celui-la - il le suit, et c'est tout.
// ---------------------------------------------------------------------------------------------------------------------

TEST( ExerciseSessionTest, a_planned_session_asks_its_questions_in_order )
{
    SessionSettings settings = intervalOnlySettings();

    settings.plannedQuestions = {
      QuestionTarget{ QuestionKind::NamedInterval, 12, IntervalDirection::Ascending },
      QuestionTarget{ QuestionKind::NamedInterval, 3, IntervalDirection::Descending },
      QuestionTarget{ QuestionKind::Chord, static_cast<std::int32_t>( ChordQuality::Diminished ), IntervalDirection::Ascending },
    };

    settings.questionCount = settings.plannedQuestions.size();

    // La palette et la main d'accords doivent contenir ce que le plan demande, sinon la grille ne pourrait pas offrir la
    // bonne reponse - et un plan qui poserait une question sans sa reponse serait un piege.
    settings.startingPaletteSize = SUPPORTED_INTERVAL_COUNT;
    settings.startingChordQualityCount = CHORD_QUALITY_COUNT;

    ExerciseSession session{ TEST_SEED, settings };

    // Question 1 : l'octave, montante.
    EXPECT_EQ( QuestionKind::NamedInterval, session.currentQuestion().kind );
    EXPECT_EQ( 12, session.currentQuestion().target.semitones() );
    EXPECT_EQ( IntervalDirection::Ascending, session.currentQuestion().direction );

    answerCorrectly( session );
    session.advance();

    // Question 2 : la tierce mineure, DESCENDANTE - le sens vient du plan, et non d'un tirage.
    EXPECT_EQ( 3, session.currentQuestion().target.semitones() );
    EXPECT_EQ( IntervalDirection::Descending, session.currentQuestion().direction );

    answerCorrectly( session );
    session.advance();

    // Question 3 : un accord, et c'est bien celui que le plan a decide.
    EXPECT_EQ( QuestionKind::Chord, session.currentQuestion().kind );
    EXPECT_EQ( ChordQuality::Diminished, session.currentQuestion().chord.quality );

    EXPECT_NE( session.currentQuestion().chordChoices.end(),
               std::ranges::find( session.currentQuestion().chordChoices, ChordQuality::Diminished ) );

    EXPECT_TRUE( session.answerChord( ChordQuality::Diminished ) );
    session.advance();

    EXPECT_TRUE( session.isFinished() );
}

TEST( ExerciseSessionTest, an_exhausted_plan_goes_back_to_drawing )
{
    SessionSettings settings = intervalOnlySettings();

    settings.plannedQuestions = {
      QuestionTarget{ QuestionKind::NamedInterval, 12, IntervalDirection::Ascending },
    };

    // PLUS de questions que le plan n'en porte : c'est le cas d'une session ordinaire qui aurait un plan, et celui d'un
    // Bilan auquel on aurait ajoute des questions. La suite doit reprendre son tirage sans rien casser.
    settings.questionCount = 4;

    ExerciseSession session{ TEST_SEED, settings };

    EXPECT_EQ( 12, session.currentQuestion().target.semitones() );

    answerCorrectly( session );
    session.advance();

    // Le plan est epuise : la question vient de la palette, comme dans n'importe quelle partie.
    EXPECT_EQ( QuestionKind::NamedInterval, session.currentQuestion().kind );

    const std::span<const Interval> palette = session.palette();

    EXPECT_NE( palette.end(), std::ranges::find( palette, session.currentQuestion().target ) );
}

// ---------------------------------------------------------------------------------------------------------------------
// L'indice d'accord
//
// L'indice d'accord : une mauvaise reponse retiree de la grille, ou l'accord rejoue en arpege.
// ---------------------------------------------------------------------------------------------------------------------

TEST( ExerciseSessionTest, the_chord_hint_waits_for_a_first_failed_attempt )
{
    ExerciseSession session{ TEST_SEED, wideChordSettings() };

    // Un indice offert AVANT d'avoir essaye ne serait pas un indice, ce serait un raccourci.
    EXPECT_FALSE( session.canRemoveOneWrongChordChoice() );
    EXPECT_FALSE( session.canHearChordAsArpeggio() );

    session.answerChord( wrongChordAnswer( session ) );

    // Une fois rate : les deux aides sont la.
    EXPECT_TRUE( session.canRemoveOneWrongChordChoice() );
    EXPECT_TRUE( session.canHearChordAsArpeggio() );
}

TEST( ExerciseSessionTest, removing_a_wrong_chord_choice_never_removes_the_right_one )
{
    ExerciseSession session{ TEST_SEED, wideChordSettings() };

    session.answerChord( wrongChordAnswer( session ) );

    const std::vector<ChordQuality> before = session.currentQuestion().chordChoices;

    ASSERT_TRUE( session.removeOneWrongChordChoice() );

    const std::vector<ChordQuality> after = session.currentQuestion().chordChoices;

    EXPECT_EQ( before.size() - 1, after.size() );

    // La BONNE reponse est toujours la : un indice qui retirerait la reponse rendrait la question impossible, ce qui est
    // le contraire d'une aide.
    EXPECT_NE( after.end(), std::ranges::find( after, askedChordQuality( session ) ) );

    // Et celle qui est partie etait bien une fausse : tout ce qui reste etait deja dans la liste d'avant.
    for( const ChordQuality quality : after )
    {
        EXPECT_NE( before.end(), std::ranges::find( before, quality ) );
    }
}

TEST( ExerciseSessionTest, the_chord_hint_stops_when_only_a_hint_would_be_left )
{
    SessionSettings settings = chordOnlySettings();

    // Une palette de DEUX couleurs : la bonne reponse et un leurre. Il n'y a rien a retirer, et le domaine doit le dire
    // plutot que d'offrir un bouton qui ne ferait rien.
    settings.startingChordQualityCount = 2;

    ExerciseSession session{ TEST_SEED, settings };

    session.answerChord( wrongChordAnswer( session ) );

    EXPECT_FALSE( session.canRemoveOneWrongChordChoice() );

    // Mais l'ARPEGE reste disponible, et c'est la nuance qui compte : un debutant n'a rien a eliminer, et c'est
    // precisement lui que l'accord note a note aide le plus.
    EXPECT_TRUE( session.canHearChordAsArpeggio() );
}

// ---------------------------------------------------------------------------------------------------------------------
// Le pilier harmonie : le degrade
//
// Ces tests ne verifient pas un son, mais la seule chose qui puisse etre FAUSSE dans une question de mode : le SENS.
// « Plus clair ou plus sombre ? » n'a de reponse que si le domaine sait lui-meme comparer deux couleurs, et si la
// tonique qu'il donne au bourdon tombe dans la fenetre des enregistrements.
// ---------------------------------------------------------------------------------------------------------------------

namespace
{

// Une session qui ne pose QUE des questions de modes, et de couleur : sans cet epeinglage, la session melange les
// genres et les tests echoueraient au hasard - exactement comme pour les accords et le rythme.
[[nodiscard]] SessionSettings modeColourOnlySettings()
{
    SessionSettings settings = intervalOnlySettings();
    settings.modeColourQuestionShare = 100;
    return settings;
}

[[nodiscard]] SessionSettings modeNameOnlySettings()
{
    SessionSettings settings = intervalOnlySettings();
    settings.modeNameQuestionShare = 100;
    return settings;
}

// La reponse juste a une question de couleur, telle que le DOMAINE la calcule.
TEST( ExerciseSessionTest, only_the_three_first_kinds_are_about_an_interval )
{
    // La question que se posent l'ecran (« ai-je un indice a montrer ? ») et la session (« une aide doit-elle reculer la
    // palette ? »), posee une seule fois pour les deux. Elle etait ecrite trois fois, et la troisieme - les modes -
    // manquait : Roger a vu un souvenir de film apparaitre sous une question de bourdon.
    EXPECT_TRUE( isIntervalQuestion( QuestionKind::NamedInterval ) );
    EXPECT_TRUE( isIntervalQuestion( QuestionKind::Direction ) );
    EXPECT_TRUE( isIntervalQuestion( QuestionKind::Sing ) );

    EXPECT_FALSE( isIntervalQuestion( QuestionKind::Rhythm ) );
    EXPECT_FALSE( isIntervalQuestion( QuestionKind::Chord ) );
    EXPECT_FALSE( isIntervalQuestion( QuestionKind::ModeColour ) );
    EXPECT_FALSE( isIntervalQuestion( QuestionKind::ModeName ) );
    EXPECT_FALSE( isIntervalQuestion( QuestionKind::ModeVamp ) );
}

[[nodiscard]] bool expectedColourAnswer( const Question & p_question )
{
    return p_question.previousMode.has_value() && isBrighterThan( p_question.mode, *p_question.previousMode );
}

// Les classes de hauteur d'une suite de notes : c'est ce qui permet de dire « ce sont les MEMES notes » sans se soucier
// des octaves. Un vamp ne se verifie pas autrement.
[[nodiscard]] std::set<std::int32_t> pitchClassesOf( std::span<const Note> p_notes )
{
    std::set<std::int32_t> classes;

    for( const Note & note : p_notes )
    {
        classes.insert( note.pitchClassIndex() );
    }

    return classes;
}

}    // namespace

TEST( ExerciseSessionTest, a_colour_question_poses_two_different_modes_on_one_drone )
{
    ExerciseSession session{ TEST_SEED, modeColourOnlySettings() };

    // La boucle suit la LONGUEUR d'une session, et non un nombre ecrit a la main : repondre au-dela de la derniere
    // question est refuse par le domaine - la session est finie - et un test qui l'ignorerait echouerait sur sa propre
    // borne plutot que sur la regle qu'il verifie.
    for( std::size_t questionIndex = 0; questionIndex < session.settings().questionCount; ++questionIndex )
    {
        const Question & question = session.currentQuestion();

        ASSERT_EQ( QuestionKind::ModeColour, question.kind );

        // Deux modes, et deux modes DIFFERENTS : comparer un mode avec lui-meme n'aurait pas de reponse, donc la
        // question serait impossible plutot que difficile.
        ASSERT_TRUE( question.previousMode.has_value() );
        EXPECT_NE( *question.previousMode, question.mode );

        // UNE seule tonique, tenue par le bourdon sous les deux : c'est elle qui donne un centre, et sans elle deux
        // modes ne seraient que deux gammes.
        EXPECT_TRUE( question.modeTonic.isValid() );

        // Et elle tombe dans la fenetre des enregistrements - 35 a 41. Ce n'est pas un gout : la tonique ET sa quinte
        // doivent rester a moins de trois demi-tons d'un echantillon de bourdon, sinon un enregistrement serait
        // transpose au-dela de ce qui s'entend.
        EXPECT_GE( question.modeTonic.midiNumber(), 35 );
        EXPECT_LE( question.modeTonic.midiNumber(), 41 );

        // Les choix sont ceux de la palette, ni plus ni moins.
        EXPECT_EQ( session.modePalette().size(), question.modeChoices.size() );

        EXPECT_TRUE( session.answerModeColour( expectedColourAnswer( question ) ) );
        EXPECT_EQ( SessionState::Feedback, session.state() );

        session.advance();
    }
}

TEST( ExerciseSessionTest, a_wrong_colour_answer_leaves_the_question_posed )
{
    ExerciseSession session{ TEST_SEED, modeColourOnlySettings() };

    const Question & question = session.currentQuestion();

    ASSERT_EQ( QuestionKind::ModeColour, question.kind );

    const bool wrongAnswer = !expectedColourAnswer( question );

    EXPECT_FALSE( session.answerModeColour( wrongAnswer ) );

    // La question reste posee et le joueur peut repondre encore : exactement le comportement d'une question
    // d'intervalle ratee, donc rien de particulier a faire dans l'ecran.
    EXPECT_EQ( SessionState::Asking, session.state() );
    EXPECT_TRUE( session.answerModeColour( !wrongAnswer ) );
    EXPECT_EQ( SessionState::Feedback, session.state() );
}

TEST( ExerciseSessionTest, a_mode_question_refuses_an_answer_of_another_kind )
{
    // Trois langues, trois questions : dire « plus clair » la ou un nom est demande ne repond a rien, et nommer un mode
    // la ou deux couleurs venaient de sonner non plus. C'est le meme refus que la direction oppose a une question qui
    // demandait un nom.
    ExerciseSession colourSession{ TEST_SEED, modeColourOnlySettings() };
    ExerciseSession nameSession{ TEST_SEED, modeNameOnlySettings() };
    ExerciseSession intervalSession{ TEST_SEED, intervalOnlySettings() };

    EXPECT_FALSE( colourSession.answerModeName( Mode::Ionian ) );
    EXPECT_FALSE( nameSession.answerModeColour( true ) );

    EXPECT_FALSE( intervalSession.answerModeColour( true ) );
    EXPECT_FALSE( intervalSession.answerModeName( Mode::Ionian ) );
}

TEST( ExerciseSessionTest, a_name_question_poses_one_mode_and_remembers_the_answer )
{
    ExerciseSession session{ TEST_SEED, modeNameOnlySettings() };

    const Question & question = session.currentQuestion();

    ASSERT_EQ( QuestionKind::ModeName, question.kind );

    // Rien a comparer sur une question de NOM : c'est pour cela que le mode precedent est un optional, et non un mode
    // « vide » qu'un lecteur finirait par lire.
    EXPECT_FALSE( question.previousMode.has_value() );

    EXPECT_EQ( session.modePalette().size(), question.modeChoices.size() );

    EXPECT_TRUE( session.answerModeName( question.mode ) );

    // Ce que le joueur a repondu est garde, pour que le verdict puisse le montrer : meme role que la derniere reponse
    // d'accord.
    EXPECT_EQ( std::optional<Mode>{ question.mode }, session.lastModeAnswer() );
}

TEST( ExerciseSessionTest, a_same_colour_question_is_answered_by_saying_so )
{
    // Le bouton que Roger a demande - « rajouter un bouton egal » - et la regle qui le rend utile : pour que « pareil »
    // soit une BONNE reponse de temps en temps, le jeu doit parfois poser deux fois la meme couleur.
    SessionSettings settings = modeColourOnlySettings();
    settings.sameColourQuestionShare = 100;

    ExerciseSession session{ TEST_SEED, settings };

    const Question & question = session.currentQuestion();

    ASSERT_TRUE( question.previousMode.has_value() );
    EXPECT_EQ( *question.previousMode, question.mode );

    // Les deux autres reponses sont FAUSSES : un bouton juste tout le temps n'apprendrait rien a personne.
    EXPECT_FALSE( session.answerModeColour( ModeColourAnswer::Brighter ) );
    EXPECT_TRUE( session.answerModeColour( ModeColourAnswer::Same ) );
}

TEST( ExerciseSessionTest, a_question_of_two_different_colours_refuses_sameness )
{
    SessionSettings settings = modeColourOnlySettings();
    settings.sameColourQuestionShare = 0;

    ExerciseSession session{ TEST_SEED, settings };

    ASSERT_TRUE( session.currentQuestion().previousMode.has_value() );
    ASSERT_NE( *session.currentQuestion().previousMode, session.currentQuestion().mode );

    EXPECT_FALSE( session.answerModeColour( ModeColourAnswer::Same ) );
    EXPECT_TRUE( session.answerModeColour( expectedColourAnswer( session.currentQuestion() ) ) );
}

TEST( ExerciseSessionTest, the_same_colour_share_poses_both_kinds_of_question )
{
    // La part par defaut (vingt) doit donner les DEUX cas sur une centaine de questions : sans cela, le bouton « pareil »
    // serait un piege permanent, et un piege ne s'apprend pas.
    SessionSettings settings = modeColourOnlySettings();
    settings.sameColourQuestionShare = 50;

    bool sawSameness = false;
    bool sawDifference = false;

    // Une session par graine, et la premiere question de chacune : c'est le TIRAGE qu'on veut voir, et avancer
    // demanderait de savoir repondre - ce qui n'est pas le sujet ici.
    for( std::uint32_t seed = 1; seed <= 100; ++seed )
    {
        const ExerciseSession session{ seed, settings };

        const Question & question = session.currentQuestion();

        ASSERT_TRUE( question.previousMode.has_value() );

        if( *question.previousMode == question.mode )
        {
            sawSameness = true;
        }
        else
        {
            sawDifference = true;
        }
    }

    EXPECT_TRUE( sawSameness );
    EXPECT_TRUE( sawDifference );
}

TEST( ExerciseSessionTest, the_mode_palette_starts_with_the_known_ones_and_grows_on_successes )
{
    SessionSettings settings = modeColourOnlySettings();
    settings.startingModeCount = 2;

    ExerciseSession session{ TEST_SEED, settings };

    // Le majeur et le mineur : le seul ecart que toute oreille connait deja, et la seule premiere question possible.
    EXPECT_EQ( 2, session.modePalette().size() );
    EXPECT_EQ( Mode::Ionian, session.modePalette().front() );
    EXPECT_EQ( Mode::Aeolian, session.modePalette().at( 1 ) );

    // Trois reussites de suite ouvrent le mode SUIVANT de l'ordre d'apprentissage, et non un mode tire au hasard :
    // c'est ce qui rend la progression previsible, du connu vers les extremes.
    for( int successIndex = 0; successIndex < 3; ++successIndex )
    {
        EXPECT_TRUE( session.answerModeColour( expectedColourAnswer( session.currentQuestion() ) ) );

        session.advance();
    }

    EXPECT_EQ( 3, session.modePalette().size() );
    EXPECT_EQ( Mode::Mixolydian, session.modePalette().at( 2 ) );
}

// Une session qui ne pose QUE des questions de NOTE ETRANGERE.
[[nodiscard]] SessionSettings foreignNoteOnlySettings()
{
    SessionSettings settings;
    pinEveryQuestionShare( settings );

    settings.foreignNoteQuestionShare = 100;

    return settings;
}

TEST( ExerciseSessionTest, a_foreign_note_question_poses_a_scale_with_exactly_one_note_out_of_it )
{
    // La question que Roger a demandee des le debut - « quelle note n'est pas dans la gamme ? » - et la seule chose qui
    // compte : la gamme doit rester RECONNAISSABLE, donc six de ses sept notes y sont, et une seule est etrangere.
    ExerciseSession session{ TEST_SEED, foreignNoteOnlySettings() };

    const Question & question = session.currentQuestion();

    ASSERT_EQ( QuestionKind::ForeignNote, question.kind );

    // Sept notes, une par degre : c'est une gamme montee.
    ASSERT_EQ( 7U, question.foreignMelody.size() );

    const std::vector<Note> scale = notesOfMode( question.modeTonic, question.mode );

    std::set<std::int32_t> scaleClasses;

    for( const Note & note : scale )
    {
        scaleClasses.insert( note.pitchClassIndex() );
    }

    std::size_t foreignCount = 0;
    std::size_t foreignStep = 0;

    for( std::size_t index = 0; index < question.foreignMelody.size(); ++index )
    {
        if( scaleClasses.count( question.foreignMelody.at( index ).pitchClassIndex() ) == 0 )
        {
            ++foreignCount;
            foreignStep = index;
        }
    }

    // UNE seule note etrangere, et c'est celle que le domaine ANNONCE : la question et sa reponse ne peuvent pas
    // diverger, sinon l'exercice serait insoluble.
    EXPECT_EQ( 1U, foreignCount );
    EXPECT_EQ( static_cast<std::int32_t>( foreignStep ), question.foreignStepIndex );

    // A un DEMI-TON de la note de la gamme, jamais plus : la faute doit s'entendre, pas crier.
    EXPECT_EQ( 1,
               std::abs( question.foreignMelody.at( foreignStep ).midiNumber() - scale.at( foreignStep ).midiNumber() ) );

    // Et les six autres pas sont EXACTEMENT la gamme : c'est ce qui permet d'entendre l'intrus.
    for( std::size_t index = 0; index < question.foreignMelody.size(); ++index )
    {
        if( index != foreignStep )
        {
            EXPECT_EQ( scale.at( index ).midiNumber(), question.foreignMelody.at( index ).midiNumber() );
        }
    }
}

TEST( ExerciseSessionTest, answering_the_foreign_note_means_designing_its_step )
{
    ExerciseSession session{ TEST_SEED, foreignNoteOnlySettings() };

    const std::int32_t foreignStep = session.currentQuestion().foreignStepIndex;

    // Un AUTRE pas est faux : l'exercice ne se reussit pas en designant n'importe quoi.
    EXPECT_FALSE( session.answerForeignNote( ( foreignStep + 1 ) % 7 ) );

    // Et le bon pas est juste.
    EXPECT_TRUE( session.answerForeignNote( foreignStep ) );

    // Une question d'intervalle n'accepte pas une reponse de gamme : deux langues differentes ne se repondent pas l'une
    // l'autre, et c'est le meme refus que partout ailleurs dans ce domaine.
    ExerciseSession intervalSession{ TEST_SEED, intervalOnlySettings() };

    EXPECT_FALSE( intervalSession.answerForeignNote( 0 ) );
}

TEST( ExerciseSessionTest, a_vamp_plays_the_same_notes_on_two_different_centres )
{
    SessionSettings settings = intervalOnlySettings();
    settings.modeVampQuestionShare = 100;

    ExerciseSession session{ TEST_SEED, settings };

    for( std::size_t questionIndex = 0; questionIndex < session.settings().questionCount; ++questionIndex )
    {
        const Question & question = session.currentQuestion();

        ASSERT_EQ( QuestionKind::ModeVamp, question.kind );
        ASSERT_TRUE( question.previousMode.has_value() );

        // Deux modes DIFFERENTS...
        EXPECT_NE( *question.previousMode, question.mode );

        // ...et deux passages qui ont EXACTEMENT les memes notes.
        //
        // C'est tout le vamp, et c'est ce qui en fait une lecon sur le CONTEXTE plutot qu'une seconde question de
        // couleur : le joueur entend le meme materiau deux fois, et pourtant deux modes. Do dorien et si bemol majeur,
        // en une phrase.
        const std::set<std::int32_t> first =
          pitchClassesOf( notesOfMode( question.previousModeTonic, *question.previousMode ) );

        const std::set<std::int32_t> second = pitchClassesOf( notesOfMode( question.modeTonic, question.mode ) );

        // Le message d'echec porte les VALEURS : sans elles, un test de tirage dit seulement « ca ne va pas ».
        ASSERT_EQ( first, second ) << "premier mode " << static_cast<int>( *question.previousMode ) << " sur "
                                   << question.previousModeTonic.midiNumber() << ", second mode "
                                   << static_cast<int>( question.mode ) << " sur " << question.modeTonic.midiNumber();

        // Et les DEUX toniques restent a portee des enregistrements de bourdon.
        //
        // La fenetre est PLUS LARGE que celle d'une question simple - 35 a 48 au lieu de 35 a 41 - parce qu'un vamp
        // tient deux centres a la fois, et qu'ils sont a une quinte l'un de l'autre : c'est le prix du meme jeu de notes
        // sur deux hauteurs, et les deux echantillons (re 2 et la 2) le couvrent a trois demi-tons pres.
        EXPECT_GE( question.previousModeTonic.midiNumber(), 35 );
        EXPECT_LE( question.previousModeTonic.midiNumber(), 48 );

        EXPECT_GE( question.modeTonic.midiNumber(), 35 );
        EXPECT_LE( question.modeTonic.midiNumber(), 48 );

        // La reponse se donne comme celle d'une comparaison : c'est la meme question, posee sur un autre materiau.
        EXPECT_TRUE( session.answerModeColour( expectedColourAnswer( question ) ) );
        EXPECT_EQ( SessionState::Feedback, session.state() );

        session.advance();
    }
}

}    // namespace musichien::domain