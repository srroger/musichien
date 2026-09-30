#include "ui/ModePreviewController.h"

#include "domain/audio/NotePlayerFake.h"

#include <QVariantList>
#include <QVariantMap>

#include <gtest/gtest.h>

namespace musichien::ui
{

// ---------------------------------------------------------------------------------------------------------------------
// Le banc d'essai des modes
//
// Ces tests tournent avec le faux lecteur de notes : ni carte son, ni telephone. Ils verifient exactement ce dont ce
// controleur est responsable - ce que l'ecran AFFICHERAIT, et ce que l'oreille ENTENDRAIT. Deux endroits ou un
// controleur se trompe, et les deux se verifient ici en quelques microsecondes.
// ---------------------------------------------------------------------------------------------------------------------

TEST( ModePreviewControllerTest, the_seven_modes_arrive_ready_to_display_and_ordered_by_clarity )
{
    domain::NotePlayerFake notePlayer;
    ModePreviewController controller{ notePlayer };

    const QVariantList modes = controller.modes();

    ASSERT_EQ( 7, modes.size() );

    // Le premier est le plus CLAIR, le dernier le plus sombre : la liste EST l'ordre de couleur, et l'ecran n'a donc
    // rien a trier - il affiche ce qu'il recoit.
    const QVariantMap brightest = modes.first().toMap();
    const QVariantMap darkest = modes.last().toMap();

    EXPECT_EQ( "lydian", brightest.value( "identifier" ).toString() );
    EXPECT_EQ( "locrian", darkest.value( "identifier" ).toString() );
    EXPECT_DOUBLE_EQ( 1.0, brightest.value( "brightness" ).toDouble() );
    EXPECT_DOUBLE_EQ( 0.0, darkest.value( "brightness" ).toDouble() );

    // Chaque entree porte un nom, la note qui colore le mode, et une clarte : c'est tout ce dont un bouton a besoin, et
    // aucune entree n'a le droit d'en oublier une.
    for( const QVariant & entry : modes )
    {
        const QVariantMap description = entry.toMap();

        EXPECT_FALSE( description.value( "name" ).toString().isEmpty() );
        EXPECT_FALSE( description.value( "characteristic" ).toString().isEmpty() );
        EXPECT_TRUE( description.contains( "index" ) );
    }
}

TEST( ModePreviewControllerTest, playing_a_mode_asks_the_port_for_a_melody_over_a_drone )
{
    domain::NotePlayerFake notePlayer;
    ModePreviewController controller{ notePlayer };

    controller.playMode( 3 );    // le dorien

    // UN SEUL appel au port, et c'est tout le point : la melodie ET le bourdon partent ensemble. Deux appels successifs
    // feraient entendre une gamme puis un accord - deux choses - au lieu d'une couleur posee sur un centre.
    ASSERT_EQ( 1, notePlayer.melodiesOverDrones().size() );

    const domain::NotePlayerFake::PlayedOverDrone & played = notePlayer.melodiesOverDrones().front();

    // Le bourdon est une QUINTE : la tonique, et sa quinte juste au-dessus. Roger a tranche deux fois de suite.
    ASSERT_EQ( 2, played.drone.size() );
    EXPECT_EQ( 7, played.drone.at( 1 ).midiNumber() - played.drone.at( 0 ).midiNumber() );

    // La gamme du dorien, montee PUIS descendue : treize notes, et la meme note aux deux bouts - c'est le retour sur la
    // tonique qui fait entendre ou se trouve le centre.
    EXPECT_EQ( 13, played.melody.size() );
    EXPECT_EQ( played.melody.front().midiNumber(), played.melody.back().midiNumber() );

    // Et le mode entendu est decrit pour l'ecran, sans que personne ait a le redemander au domaine.
    EXPECT_EQ( "dorian", controller.lastPlayedMode().value( "identifier" ).toString() );
}

TEST( ModePreviewControllerTest, an_index_outside_the_known_modes_plays_nothing )
{
    domain::NotePlayerFake notePlayer;
    ModePreviewController controller{ notePlayer };

    // Un index peut venir d'un ecran, donc d'une donnee : un index hors bornes doit ne rien faire, et surtout pas
    // demander au domaine un mode qui n'existe pas.
    controller.playMode( -1 );
    controller.playMode( 7 );

    EXPECT_TRUE( notePlayer.melodiesOverDrones().empty() );
}

}    // namespace musichien::ui
