#include "ui/RhythmController.h"

#include "domain/audio/NotePlayerFake.h"

#include <gtest/gtest.h>

namespace musichien::ui
{

TEST( RhythmControllerTest, the_pattern_list_offers_the_bare_metronome_first )
{
    domain::NotePlayerFake notePlayer;
    RhythmController controller{ notePlayer };

    const QVariantList patterns = controller.patterns();

    // One entry for the metronome alone, then one per cell of the domain - so the screen can show the list as it is
    // and hand back the chosen index without any offset to remember.
    EXPECT_EQ( 1 + static_cast<int>( domain::allRhythmPatterns().size() ), patterns.size() );

    EXPECT_EQ( 0, controller.currentPattern() );
}

TEST( RhythmControllerTest, a_pattern_outside_the_list_is_refused )
{
    domain::NotePlayerFake notePlayer;
    RhythmController controller{ notePlayer };

    controller.setCurrentPattern( 2 );
    EXPECT_EQ( 2, controller.currentPattern() );

    controller.setCurrentPattern( 999 );
    EXPECT_EQ( 2, controller.currentPattern() );

    controller.setCurrentPattern( -1 );
    EXPECT_EQ( 2, controller.currentPattern() );
}

TEST( RhythmControllerTest, the_tempo_stays_inside_its_range_and_goes_down_to_one )
{
    domain::NotePlayerFake notePlayer;
    RhythmController controller{ notePlayer };

    // Roger asked for the floor to be 1 bpm: a very slow exercise is a legitimate thing to want.
    controller.setBpm( 1 );
    EXPECT_EQ( 1, controller.bpm() );

    controller.setBpm( 0 );
    EXPECT_EQ( 1, controller.bpm() );

    controller.setBpm( 5000 );
    EXPECT_EQ( 300, controller.bpm() );
}

TEST( RhythmControllerTest, tapping_while_stopped_only_reads_a_tempo )
{
    domain::NotePlayerFake notePlayer;
    RhythmController controller{ notePlayer };

    // Le metronome est arrete : taper ne juge rien, ca propose un tempo. Deux frappes, et l'intervalle donne le tempo.
    controller.tap();
    controller.tap();

    EXPECT_EQ( 0, controller.score() );
    EXPECT_EQ( 0, controller.combo() );
}

TEST( RhythmControllerTest, starting_the_metronome_resets_the_score_and_beats_the_first_time )
{
    domain::NotePlayerFake notePlayer;
    RhythmController controller{ notePlayer };

    controller.start();

    EXPECT_TRUE( controller.isRunning() );
    EXPECT_EQ( 0, controller.score() );
    EXPECT_EQ( 0, controller.combo() );

    // Le premier temps sonne tout de suite : l'oreille part sur un repere franc.
    EXPECT_GT( notePlayer.metronomeClickCount(), 0 );
    EXPECT_EQ( 1, notePlayer.accentedClickCount() );

    controller.stop();

    EXPECT_FALSE( controller.isRunning() );
}

TEST( RhythmControllerTest, the_metronome_and_the_battery_are_two_different_calls )
{
    domain::NotePlayerFake notePlayer;
    RhythmController controller{ notePlayer };

    controller.start();

    const std::size_t clicksAfterStart = notePlayer.metronomeClickCount();

    controller.playDrum( 0 );

    // Le clic du metronome a bien ete demande, et la batterie aussi : deux sons distincts, qui se MELANGENT.
    EXPECT_GT( clicksAfterStart, 0U );
    EXPECT_EQ( 1, notePlayer.drumCount() );
    EXPECT_EQ( 0, notePlayer.drumCount( domain::Drum::Snare ) );

    controller.playDrum( 1 );
    EXPECT_EQ( 1, notePlayer.drumCount( domain::Drum::Snare ) );

    // Et un index hors liste ne fait rien, plutot que de sortir du tableau.
    controller.playDrum( 99 );
    EXPECT_EQ( 1, notePlayer.drumCount( domain::Drum::Snare ) );

    controller.stop();
}

}    // namespace musichien::ui
