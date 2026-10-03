#include "ui/IntervalPlaybackController.h"

#include "domain/audio/NotePlayerFake.h"

#include <QVariantList>
#include <QVariantMap>

#include <gtest/gtest.h>

#include <vector>

namespace musichien::ui
{

// ---------------------------------------------------------------------------------------------------------------------
// The view model of the landing screen
//
// These tests run with the fake note player, so they need neither a sound card nor a phone. What they
// check is exactly what this class is responsible for: what the screen would HEAR, and what it would
// DISPLAY. Both are the places where a view model goes wrong.
//
// GoogleTest test names are written in snake_case of words because they read as sentences in the
// test report. clang-tidy is therefore disabled on test targets only; see
// cmake/musichienFunctionAddTest.cmake.
// ---------------------------------------------------------------------------------------------------------------------

namespace
{

// Middle C, the note every interval of the bench is built above.
constexpr std::int32_t MIDDLE_C_MIDI_NUMBER = 60;

}    // namespace

TEST( IntervalPlaybackControllerTest, every_interval_the_domain_supports_becomes_a_choice )
{
    domain::NotePlayerFake notePlayer;
    IntervalPlaybackController controller{ notePlayer };

    const QVariantList choices = controller.supportedIntervals();

    ASSERT_EQ( 25, choices.size() );

    // The choices arrive ready to display: the screen reads a name and an identifier, it composes
    // neither. That is the whole reason the domain hands them over instead of the screen listing
    // them.
    const QVariantMap firstChoice = choices.first().toMap();
    const QVariantMap lastChoice = choices.last().toMap();

    EXPECT_EQ( "P1", firstChoice.value( "identifier" ).toString() );
    EXPECT_EQ( "P15", lastChoice.value( "identifier" ).toString() );
    EXPECT_EQ( 24, lastChoice.value( "semitones" ).toInt() );
    EXPECT_TRUE( lastChoice.value( "isCompound" ).toBool() );
}

TEST( IntervalPlaybackControllerTest, playing_a_ninth_plays_a_ninth_and_names_it )
{
    domain::NotePlayerFake notePlayer;
    IntervalPlaybackController controller{ notePlayer };

    controller.playInterval( 14 );

    // What is heard: middle C, then D an octave higher, one after the other.
    ASSERT_EQ( 1, notePlayer.playedMelodies().size() );

    const std::vector<domain::Note> playedNotes = notePlayer.playedMelodies().front().notes;

    ASSERT_EQ( 2, playedNotes.size() );
    EXPECT_EQ( MIDDLE_C_MIDI_NUMBER, playedNotes.at( 0 ).midiNumber() );
    EXPECT_EQ( MIDDLE_C_MIDI_NUMBER + 14, playedNotes.at( 1 ).midiNumber() );

    // What is displayed: the answer of the DOMAIN, and the keys the screen reads.
    const QVariantMap heardInterval = controller.lastPlayedInterval();

    EXPECT_EQ( "M9", heardInterval.value( "identifier" ).toString() );

    // LE NOM EST CELUI DU JOUEUR. L'identifiant au-dessus reste anglais et stable - c'est lui qu'un fichier garderait -
    // mais ce qui s'AFFICHE est en francais : « for what is shown to the user, French » (Roger, 02/10/2026).
    EXPECT_EQ( QStringLiteral( "Neuvième majeure" ), heardInterval.value( "name" ).toString() );
    EXPECT_EQ( 14, heardInterval.value( "semitones" ).toInt() );
    EXPECT_EQ( 2, heardInterval.value( "intervalClass" ).toInt() );
    EXPECT_EQ( 1, heardInterval.value( "octaveSpan" ).toInt() );
    EXPECT_EQ( 9, heardInterval.value( "number" ).toInt() );
    EXPECT_TRUE( heardInterval.value( "isCompound" ).toBool() );
}

TEST( IntervalPlaybackControllerTest, the_switch_chooses_two_notes_or_one_chord )
{
    domain::NotePlayerFake notePlayer;
    IntervalPlaybackController controller{ notePlayer };

    // One note after the other by default, because that is how an interval is taught first.
    EXPECT_FALSE( controller.harmonicPlayback() );

    controller.playInterval( 7 );

    EXPECT_EQ( 1, notePlayer.playedMelodies().size() );
    EXPECT_TRUE( notePlayer.playedChords().empty() );

    notePlayer.clear();
    controller.setHarmonicPlayback( true );
    controller.playInterval( 7 );

    EXPECT_EQ( 1, notePlayer.playedChords().size() );
    EXPECT_TRUE( notePlayer.playedMelodies().empty() );

    // Either way it is the same interval: how one listens to it changes nothing about what it is.
    EXPECT_EQ( "P5", controller.lastPlayedInterval().value( "identifier" ).toString() );
}

TEST( IntervalPlaybackControllerTest, a_single_note_has_nothing_to_name )
{
    domain::NotePlayerFake notePlayer;
    IntervalPlaybackController controller{ notePlayer };

    controller.playSingleNote();

    ASSERT_EQ( 1, notePlayer.playedNotes().size() );
    EXPECT_EQ( MIDDLE_C_MIDI_NUMBER, notePlayer.playedNotes().front().midiNumber() );

    // An EMPTY description is what tells the screen there is nothing to name, so that it can fall
    // back to its generic prompt without knowing what a single note is.
    EXPECT_TRUE( controller.lastPlayedInterval().isEmpty() );
}

TEST( IntervalPlaybackControllerTest, replaying_the_same_interval_plays_but_does_not_repeat_itself )
{
    domain::NotePlayerFake notePlayer;
    IntervalPlaybackController controller{ notePlayer };

    controller.playInterval( 7 );

    const QVariantMap firstDescription = controller.lastPlayedInterval();

    controller.playInterval( 7 );

    // The sound is played again, the description is not re emitted: a screen must not blink because a
    // button was tapped twice.
    EXPECT_EQ( 2, notePlayer.playedMelodies().size() );
    EXPECT_TRUE( firstDescription == controller.lastPlayedInterval() );
}

TEST( IntervalPlaybackControllerTest, leaving_the_screen_closes_the_audio )
{
    domain::NotePlayerFake notePlayer;
    IntervalPlaybackController controller{ notePlayer };

    controller.playInterval( 7 );
    controller.stopPlayback();

    EXPECT_EQ( 1, notePlayer.stopCount() );
}

}    // namespace musichien::ui