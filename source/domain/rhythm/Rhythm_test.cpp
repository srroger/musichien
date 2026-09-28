#include "domain/rhythm/Rhythm.h"

#include <gtest/gtest.h>

#include <cmath>

namespace musichien::domain
{

TEST( RhythmTest, a_beat_is_one_minute_divided_by_the_tempo )
{
    EXPECT_NEAR( 1000.0, beatDurationMs( 60.0 ), 1e-9 );    // 60 bpm: one second per beat
    EXPECT_NEAR( 500.0, beatDurationMs( 120.0 ), 1e-9 );    // 120 bpm: half a second
    EXPECT_NEAR( 600.0, beatDurationMs( 100.0 ), 1e-9 );    // 100 bpm: 0.6 second
}

TEST( RhythmTest, a_tap_on_the_beat_is_perfect )
{
    EXPECT_EQ( HitQuality::Perfect, judgeTap( 0.0, 120.0 ) );
    EXPECT_EQ( HitQuality::Perfect, judgeTap( 500.0, 120.0 ) );
    EXPECT_EQ( HitQuality::Perfect, judgeTap( 1500.0, 120.0 ) );
}

TEST( RhythmTest, a_tap_near_the_beat_is_good )
{
    // A hundred milliseconds late: inside the good window, outside the perfect one.
    EXPECT_EQ( HitQuality::Good, judgeTap( 100.0, 120.0 ) );

    // And the same distance early.
    EXPECT_EQ( HitQuality::Good, judgeTap( 500.0 - 100.0, 120.0 ) );
}

TEST( RhythmTest, a_tap_between_two_beats_is_a_miss )
{
    // Halfway between the first and second beat of a 500 ms metronome.
    EXPECT_EQ( HitQuality::Miss, judgeTap( 250.0, 120.0 ) );
}

TEST( RhythmTest, the_windows_are_the_boundaries )
{
    EXPECT_EQ( HitQuality::Perfect, judgeTap( PERFECT_WINDOW_MS, 120.0 ) );
    EXPECT_EQ( HitQuality::Good, judgeTap( PERFECT_WINDOW_MS + 1.0, 120.0 ) );
    EXPECT_EQ( HitQuality::Good, judgeTap( GOOD_WINDOW_MS, 120.0 ) );
    EXPECT_EQ( HitQuality::Miss, judgeTap( GOOD_WINDOW_MS + 1.0, 120.0 ) );
}

TEST( RhythmTest, a_still_metronome_judges_everything_as_a_miss )
{
    EXPECT_EQ( HitQuality::Miss, judgeTap( 0.0, 0.0 ) );
}

// =====================================================================================================================
// Le battement a venir
// Ces quatre tests sont la raison d'etre de planNextBeat : ils disent, sans attendre une seule seconde, ce qui se passe
// quand un metronome joue ses battements en retard. Roger l'a entendu avant qu'il y ait un test pour le dire.
// =====================================================================================================================

TEST( RhythmTest, a_beat_on_time_is_scheduled_one_beat_later )
{
    // A 120 bpm, un temps dure 500 ms. Le deuxieme battement vient d'etre joue a 500 ms : le suivant est a 1000 ms.
    const BeatSchedule schedule = planNextBeat( 120.0, 2, 500.0 );

    EXPECT_EQ( 2U, schedule.beatIndex );
    EXPECT_NEAR( 500.0, schedule.delayMs, 1e-9 );
}

TEST( RhythmTest, a_late_beat_is_caught_up_rather_than_added_to_every_beat_that_follows )
{
    // Le deuxieme battement devait sonner a 500 ms ; la boucle d'evenements l'a joue a 514 ms. Un timer repetitif
    // attendrait 500 ms de plus, et le retard s'ajouterait a chaque battement ; ici, on vise l'echeance d'origine.
    const BeatSchedule schedule = planNextBeat( 120.0, 2, 514.0 );

    EXPECT_EQ( 2U, schedule.beatIndex );
    EXPECT_NEAR( 486.0, schedule.delayMs, 1e-9 );

    // Et la preuve que le retard ne se reporte pas : l'attente est plus COURTE qu'un temps, exactement de ce retard.
    EXPECT_LT( schedule.delayMs, beatDurationMs( 120.0 ) );
}

TEST( RhythmTest, a_beat_that_is_already_due_is_played_at_once )
{
    // Le battement attendu est depasse de 300 ms - moins d'un temps a 60 bpm : il se joue tout de suite, et non dans
    // 700 ms comme le voudrait un timer qui repart de lui-meme.
    const BeatSchedule schedule = planNextBeat( 60.0, 1, 1300.0 );

    EXPECT_EQ( 1U, schedule.beatIndex );
    EXPECT_DOUBLE_EQ( 0.0, schedule.delayMs );
}

TEST( RhythmTest, a_grid_left_far_behind_is_rebased_on_the_present )
{
    // Le telephone a dormi : dix temps se sont ecoules, et le battement 3 est loin derriere. Rejouer les battements
    // manques d'un coup ferait un bruit de mitrailleuse ; le metronome se recale donc sur le battement 11, a 5500 ms.
    const BeatSchedule schedule = planNextBeat( 120.0, 3, 5000.0 );

    EXPECT_EQ( 11U, schedule.beatIndex );
    EXPECT_NEAR( 500.0, schedule.delayMs, 1e-9 );

    // Et le battement recale tombe PILE sur la grille : c'est ce qui rend le metronome juste apres un reveil.
    EXPECT_DOUBLE_EQ( beatTimeMs( 120.0, schedule.beatIndex ) - 5000.0, schedule.delayMs );
}

TEST( RhythmTest, a_metronome_at_a_tempo_of_zero_has_nothing_to_schedule )
{
    const BeatSchedule schedule = planNextBeat( 0.0, 0, 0.0 );

    EXPECT_EQ( 0U, schedule.beatIndex );
    EXPECT_DOUBLE_EQ( 0.0, schedule.delayMs );
}

}    // namespace musichien::domain
