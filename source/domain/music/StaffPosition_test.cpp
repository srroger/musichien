#include "domain/music/StaffPosition.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>

namespace musichien::domain
{

// ---------------------------------------------------------------------------------------------------------------------
// La place d'une note sur la portee, et l'octave qu'elle a fallu replier
//
// Le meme la, dans quatre octaves, tombe QUATRE fois au meme endroit : c'est ce qui garde la boule sur les cinq
// lignes. Ce que la note a de different, c'est son octave - et l'accordeur, lui, en a besoin.
// ---------------------------------------------------------------------------------------------------------------------

TEST( StaffPositionTest, the_five_lines_are_the_five_notes_of_the_staff )
{
    // La ligne inferieure est le mi, et de ligne en ligne on monte d'un QUART : sol, si, re. C'est ce que dit une cle
    // de sol, et rien d'autre. Zero sur la ligne du bas, un sur celle du haut.
    EXPECT_DOUBLE_EQ( 0.0, StaffPosition::fraction( 64 ) );     // Mi 4, ligne du bas
    EXPECT_DOUBLE_EQ( 0.25, StaffPosition::fraction( 67 ) );    // Sol 4, deuxieme ligne
    EXPECT_DOUBLE_EQ( 0.50, StaffPosition::fraction( 71 ) );    // Si 4, ligne du milieu
    EXPECT_DOUBLE_EQ( 0.75, StaffPosition::fraction( 74 ) );    // Re 5, quatrieme ligne

    // Un degre, c'est un ESPACE : le fa est juste au-dessus du mi, pas une ligne plus haut.
    EXPECT_NEAR( 1.0 / 8.0, StaffPosition::fraction( 65 ), 1e-12 );

    // La ligne du HAUT est le fa aigu, et c'est justement la note que le repli ramene en bas : une octave compte sept
    // degres, la portee en dessine huit. Le fa aigu se dessine donc la ou se dessine le fa grave.
    EXPECT_DOUBLE_EQ( StaffPosition::fraction( 65 ), StaffPosition::fraction( 77 ) );
}

TEST( StaffPositionTest, every_octave_of_a_note_lands_on_the_same_place )
{
    // La 2, la 3, la 4, la 5 : une seule et meme hauteur dessinee.
    const double drawn = StaffPosition::fraction( 69 );

    EXPECT_DOUBLE_EQ( drawn, StaffPosition::fraction( 45 ) );
    EXPECT_DOUBLE_EQ( drawn, StaffPosition::fraction( 57 ) );
    EXPECT_DOUBLE_EQ( drawn, StaffPosition::fraction( 69 ) );
    EXPECT_DOUBLE_EQ( drawn, StaffPosition::fraction( 81 ) );

    // Et le la tient bien sur l'espace qu'une cle de sol lui donne : trois degres au-dessus du mi, sur huit.
    EXPECT_NEAR( 3.0 / 8.0, drawn, 1e-12 );
}

TEST( StaffPositionTest, the_octave_shift_says_which_way_the_real_note_is )
{
    // Le signe est ce qui permet a un accordeur de lire une hauteur ABSOLUE sans quitter la portee des yeux.
    EXPECT_EQ( -2, StaffPosition::octaveShift( 45 ) );    // La 2, deux octaves sous sa place
    EXPECT_EQ( -1, StaffPosition::octaveShift( 57 ) );    // La 3
    EXPECT_EQ( 0, StaffPosition::octaveShift( 69 ) );     // La 4, la ou la boule la dessine
    EXPECT_EQ( +1, StaffPosition::octaveShift( 81 ) );    // La 5

    // Une note hors de l'octave dessinee, dans les deux sens.
    EXPECT_EQ( -1, StaffPosition::octaveShift( 60 ) );    // Do 4, dessine en do 5
    EXPECT_EQ( 0, StaffPosition::octaveShift( 72 ) );     // Do 5
    EXPECT_EQ( +1, StaffPosition::octaveShift( 77 ) );    // Fa 5, dessine en fa 4
    EXPECT_EQ( -3, StaffPosition::octaveShift( 36 ) );    // Do 2
}

TEST( StaffPositionTest, the_drawn_note_stays_inside_the_octave_of_the_staff )
{
    // La note dessinee garde la classe de hauteur ET se tient dans l'octave de la portee : du mi 4 au re diese 5.
    for( std::int32_t midi = 21; midi <= 108; ++midi )
    {
        const std::int32_t drawn = StaffPosition::drawnMidiNumber( midi );

        EXPECT_GE( drawn, StaffPosition::BOTTOM_LINE_MIDI_NUMBER ) << "note " << midi;
        EXPECT_LE( drawn, StaffPosition::BOTTOM_LINE_MIDI_NUMBER + 11 ) << "note " << midi;

        // Meme classe de hauteur : c'est elle qui decide de la place sur la portee.
        EXPECT_EQ( midi % 12, drawn % 12 ) << "note " << midi;

        // Et la meme place que la vraie note, ce qui est tout l'objet du repli.
        EXPECT_DOUBLE_EQ( StaffPosition::fraction( midi ), StaffPosition::fraction( drawn ) ) << "note " << midi;
    }
}

TEST( StaffPositionTest, the_shift_is_zero_exactly_when_the_note_sits_where_it_is_drawn )
{
    // Le signe affiche et la place dessinee doivent dire la MEME chose : un signe sans raison, ou une octave repliee
    // sans signe, seraient deux facons de mentir au joueur.
    for( std::int32_t midi = 21; midi <= 108; ++midi )
    {
        const bool isDrawnAsItIs = midi == StaffPosition::drawnMidiNumber( midi );

        EXPECT_EQ( isDrawnAsItIs, StaffPosition::octaveShift( midi ) == 0 ) << "note " << midi;
    }
}

}    // namespace musichien::domain
