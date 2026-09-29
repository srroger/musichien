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

    // The floor is 1 bpm: a very slow exercise is a legitimate thing to want.
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

TEST( RhythmControllerTest, starting_the_metronome_hands_the_beat_over_to_the_audio_stream )
{
    domain::NotePlayerFake notePlayer;
    RhythmController controller{ notePlayer };

    controller.start();

    EXPECT_TRUE( controller.isRunning() );
    EXPECT_EQ( 0, controller.score() );
    EXPECT_EQ( 0, controller.combo() );

    // LE METRONOME EST DESORMAIS MENE PAR LE LECTEUR AUDIO, qui compte des echantillons : c'est lui qui bat, et non
    // plus l'interface, ou la justesse dependait du thread qui peint l'ecran.
    EXPECT_TRUE( notePlayer.isMetronomeRunning() );
    EXPECT_EQ( 1, notePlayer.metronomeStartCount() );
    EXPECT_DOUBLE_EQ( 90.0, notePlayer.metronomeBpm() );
    EXPECT_EQ( 4, notePlayer.metronomeBeatsPerBar() );

    controller.stop();

    EXPECT_FALSE( controller.isRunning() );

    // Et l'arret se fait aussi dans le flux : sans cela, un temps deja planifie sonnerait encore apres l'arret.
    EXPECT_FALSE( notePlayer.isMetronomeRunning() );
    EXPECT_EQ( 1, notePlayer.metronomeStopCount() );
}

TEST( RhythmControllerTest, the_page_never_sends_a_click_itself )
{
    domain::NotePlayerFake notePlayer;
    RhythmController controller{ notePlayer };

    controller.start();

    // LA PREUVE DE LA REFONTE, et c'est la ligne qui compte : le controleur demande au lecteur audio de BATTRE, et il
    // n'envoie plus un seul clic de sa propre initiative. Un clic pousse par le thread d'interface tombait la ou ce
    // thread avait bien voulu - c'est cette gigue qui s'entendait.
    EXPECT_EQ( 0, notePlayer.metronomeClickCount() );

    controller.setBpm( 132 );

    // Changer le tempo ne declenche pas davantage de clic : la nouvelle grille repart dans le flux, et le lecteur audio
    // s'en occupe.
    EXPECT_EQ( 0, notePlayer.metronomeClickCount() );
    EXPECT_EQ( 2, notePlayer.metronomeStartCount() );
    EXPECT_DOUBLE_EQ( 132.0, notePlayer.metronomeBpm() );

    controller.stop();
}

TEST( RhythmControllerTest, a_tap_is_judged_against_the_time_the_ear_hears )
{
    domain::NotePlayerFake notePlayer;
    RhythmController controller{ notePlayer };

    controller.start();

    // 90 bpm : un temps toutes les 666,67 millisecondes. Le fake avance le temps du flux comme le ferait une carte son.
    notePlayer.advanceMetronomeTo( 2, 1333.33 );

    controller.tap();

    EXPECT_EQ( 2, controller.lastQuality() ) << "pile sur le temps : parfait";

    // Le meme temps, mais 167 millisecondes plus tard : le domaine appelle ca un « bon » tap, et il le dit.
    notePlayer.advanceMetronomeTo( 2, 1500.0 );

    controller.tap();

    EXPECT_EQ( 1, controller.lastQuality() );

    controller.stop();
}

TEST( RhythmControllerTest, the_metronome_and_the_battery_are_two_different_calls )
{
    domain::NotePlayerFake notePlayer;
    RhythmController controller{ notePlayer };

    controller.start();

    controller.playDrum( 0 );

    // Le metronome bat dans le flux, et la batterie est frappee par l'interface : deux sons distincts, que le mixer
    // fait entendre ENSEMBLE.
    EXPECT_TRUE( notePlayer.isMetronomeRunning() );
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
