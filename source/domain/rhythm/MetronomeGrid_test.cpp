#include "domain/rhythm/MetronomeGrid.h"

#include <gtest/gtest.h>

#include <cmath>

namespace musichien::domain
{

namespace
{

constexpr int TEST_SAMPLE_RATE = 48000;

// La duree d'un temps telle qu'un test la calcule, sans passer par la classe : c'est ce qui permet de comparer la
// grille a la verite, et non a elle-meme.
[[nodiscard]] double expectedFramesPerBeat( double p_bpm ) noexcept
{
    return static_cast<double>( TEST_SAMPLE_RATE ) * 60.0 / p_bpm;
}

}    // namespace

TEST( MetronomeGridTest, a_beat_lands_on_the_sample_the_tempo_asks_for )
{
    MetronomeGrid grid{ TEST_SAMPLE_RATE };

    // 120 bpm : un demi-seconde par temps, donc 24000 echantillons a 48 kHz. C'est le cas ou tout tombe juste.
    grid.start( 120.0, 4, 0 );

    EXPECT_TRUE( grid.isRunning() );
    EXPECT_EQ( 0, grid.frameOfBeat( 0 ) );
    EXPECT_EQ( 24000, grid.frameOfBeat( 1 ) );
    EXPECT_EQ( 48000, grid.frameOfBeat( 2 ) );
}

TEST( MetronomeGridTest, the_thousandth_beat_has_not_drifted_by_a_single_sample )
{
    MetronomeGrid grid{ TEST_SAMPLE_RATE };

    // 130 bpm ne tombe PAS juste : un temps vaut 22153,846... echantillons. C'est exactement le cas ou un metronome
    // naif derape, parce qu'il ajoute un temps arrondi au precedent.
    constexpr double BPM = 130.0;

    grid.start( BPM, 4, 0 );

    const double exactFramesPerBeat = expectedFramesPerBeat( BPM );

    // Ce qu'un metronome qui cumule ferait : mille fois le temps ARRONDI. L'ecart est de 154 echantillons, soit plus
    // de trois millisecondes - et il grandirait sans fin.
    const auto accumulated = static_cast<std::int64_t>( std::llround( exactFramesPerBeat ) ) * 1000;

    const std::int64_t exact = static_cast<std::int64_t>( std::llround( exactFramesPerBeat * 1000.0 ) );

    EXPECT_NE( accumulated, exact ) << "le test ne prouve rien si l'arrondi tombe juste";

    // La grille vise la position EXACTE, et non la position accumulee : au millieme temps, elle n'a pas bouge.
    EXPECT_EQ( exact, grid.frameOfBeat( 1000 ) );

    // Et l'erreur reste bornee a un demi-echantillon, pour toujours.
    EXPECT_NEAR( 0.0, static_cast<double>( grid.frameOfBeat( 1000 ) ) - ( exactFramesPerBeat * 1000.0 ), 0.5 );
}

TEST( MetronomeGridTest, a_stopped_grid_does_not_beat )
{
    MetronomeGrid grid{ TEST_SAMPLE_RATE };

    // Par defaut, rien ne bat : une grille ne demarre que si on la demarre.
    EXPECT_FALSE( grid.isRunning() );

    const MetronomeGrid::Beat stoppedBeat = grid.firstBeatAtOrAfter( 50000 );
    EXPECT_EQ( 0, stoppedBeat.index );
    EXPECT_EQ( 0, stoppedBeat.frame );

    grid.start( 120.0, 4, 0 );
    grid.stop();

    EXPECT_FALSE( grid.isRunning() );

    const MetronomeGrid::Beat afterStop = grid.firstBeatAtOrAfter( 50000 );
    EXPECT_EQ( 0, afterStop.index );
    EXPECT_EQ( 0, afterStop.frame );
}

TEST( MetronomeGridTest, a_window_finds_the_beat_that_falls_inside_it )
{
    MetronomeGrid grid{ TEST_SAMPLE_RATE };

    grid.start( 120.0, 4, 0 );

    // Le premier temps est derriere nous : la fenetre qui commence juste apres doit trouver le suivant.
    EXPECT_EQ( 0, grid.firstBeatAtOrAfter( 0 ).index );
    EXPECT_EQ( 1, grid.firstBeatAtOrAfter( 1 ).index );
    EXPECT_EQ( 1, grid.firstBeatAtOrAfter( 24000 ).index );

    // Une position ENTRE deux temps trouve le temps qui vient, et sa position exacte.
    const MetronomeGrid::Beat next = grid.firstBeatAtOrAfter( 24001 );
    EXPECT_EQ( 2, next.index );
    EXPECT_EQ( 48000, next.frame );

    // Et une fenetre qui commence APRES un temps ne le rejoue pas : c'est ce qui evite un clic manque, ou un double
    // clic, quand le moteur audio demande des echantillons par grands blocs.
    const MetronomeGrid::Beat later = grid.firstBeatAtOrAfter( 48001 );
    EXPECT_EQ( 3, later.index );
    EXPECT_EQ( 72000, later.frame );
}

TEST( MetronomeGridTest, a_start_that_does_not_begin_at_zero_is_honoured )
{
    MetronomeGrid grid{ TEST_SAMPLE_RATE };

    // Le flux audio tourne depuis longtemps quand le metronome demarre : le temps 0 tombe a la position courante, et
    // tout se compte depuis la.
    grid.start( 120.0, 4, 100000 );

    EXPECT_EQ( 100000, grid.frameOfBeat( 0 ) );
    EXPECT_EQ( 124000, grid.frameOfBeat( 1 ) );

    EXPECT_EQ( 0, grid.firstBeatAtOrAfter( 100000 ).index );
    EXPECT_EQ( 0, grid.beatIndexAt( 100000 ) );

    // Un echantillon avant le deuxieme temps, c'est encore le premier : c'est ce que la grille doit dire, et c'est ce
    // qui rend l'affichage et le jugement coherents avec ce qui s'entend.
    EXPECT_EQ( 0, grid.beatIndexAt( 123999 ) );
    EXPECT_EQ( 1, grid.beatIndexAt( 124000 ) );
}

TEST( MetronomeGridTest, the_beat_in_progress_is_the_last_one_that_landed )
{
    MetronomeGrid grid{ TEST_SAMPLE_RATE };

    grid.start( 120.0, 4, 0 );

    EXPECT_EQ( 0, grid.beatIndexAt( 0 ) );
    EXPECT_EQ( 0, grid.beatIndexAt( 23999 ) );
    EXPECT_EQ( 1, grid.beatIndexAt( 24000 ) );
    EXPECT_EQ( 1, grid.beatIndexAt( 47999 ) );
    EXPECT_EQ( 2, grid.beatIndexAt( 48000 ) );
}

TEST( MetronomeGridTest, the_accent_comes_back_at_the_start_of_every_bar )
{
    MetronomeGrid grid{ TEST_SAMPLE_RATE };

    grid.start( 90.0, 4, 0 );

    EXPECT_TRUE( grid.isAccented( 0 ) );
    EXPECT_FALSE( grid.isAccented( 1 ) );
    EXPECT_FALSE( grid.isAccented( 3 ) );

    // Et la mesure suivante : le compte est global, donc le premier temps de la deuxieme mesure est aussi accentue.
    EXPECT_TRUE( grid.isAccented( 4 ) );
    EXPECT_TRUE( grid.isAccented( 8 ) );
}

TEST( MetronomeGridTest, elapsed_time_counts_the_samples )
{
    MetronomeGrid grid{ TEST_SAMPLE_RATE };

    grid.start( 120.0, 4, 1000 );

    EXPECT_DOUBLE_EQ( 0.0, grid.elapsedMsAt( 1000, TEST_SAMPLE_RATE ) );

    // Une seconde d'echantillons, une seconde de musique.
    EXPECT_NEAR( 1000.0, grid.elapsedMsAt( 1000 + TEST_SAMPLE_RATE, TEST_SAMPLE_RATE ), 1e-9 );

    // Et avant le demarrage, le temps est negatif : une frappe qui precede le premier temps n'est pas la meme chose
    // qu'une frappe sur le premier temps.
    EXPECT_LT( grid.elapsedMsAt( 0, TEST_SAMPLE_RATE ), 0.0 );
}

TEST( MetronomeGridTest, a_tempo_of_zero_does_not_start_a_grid )
{
    MetronomeGrid grid{ TEST_SAMPLE_RATE };

    // Un tempo nul est un reglage fautif : la grille refuse de battre plutot que de diviser par zero.
    grid.start( 0.0, 4, 0 );

    EXPECT_FALSE( grid.isRunning() );
}

}    // namespace musichien::domain
