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

    // LA ROUE A BESOIN de deux nombres separes, et pas d'une duree totale : le silence d'entree et le pas d'une note. C'est
    // ce qui l'empeche de partir avec le bourdon et de finir dans le silence. Voir ModeCircle.startPlayback.
    EXPECT_GT( controller.playbackNoteStepMs(), 0 );

    // Et le silence d'entree ANNONCE est celui qui a VRAIMENT ete joue : c'est l'encadrement recu par le port. Deux nombres
    // qui doivent coincider finissent toujours par diverger, donc le test les compare.
    EXPECT_EQ( static_cast<int>( played.framing.leadIn.count() ), controller.playbackLeadInMs() );
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

TEST( ModePreviewControllerTest, playing_a_phrase_keeps_the_mode_and_the_durations_of_the_content )
{
    domain::NotePlayerFake notePlayer;
    ModePreviewController controller{ notePlayer };

    // Le tempo vient du REGLAGE, et non plus du contenu : c'est ce que Roger a demande - « d'en choisir un central et de
    // varier autour ». Soixante ici, sans variation, pour que les durees se lisent sans calcul.
    domain::PlayerPreferencesFake preferences;
    preferences.storePhraseTempoBpm( 60 );
    preferences.storePhraseTempoVariation( 0 );

    controller.setPreferences( preferences );

    domain::PhraseBook phraseBook;

    // Deux phrases, dont une seule nous interesse : le livre doit rendre celle du mode DEMANDE, et jamais celle d'a cote.
    //
    // Et la premiere porte un tempo de CONTENU tout autre (cent vingt) : il ne doit pas decider, sinon le reglage ne
    // servirait a rien.
    domain::Phrase slowDorian;
    slowDorian.mode = domain::Mode::Dorian;
    slowDorian.tonic = domain::Note{ 50 };
    slowDorian.bpm = 120;
    slowDorian.steps = { domain::PhraseStep{ .degree = 1, .beats = 1 },
                         domain::PhraseStep{ .degree = 3, .beats = 2 },
                         domain::PhraseStep{ .degree = 1, .beats = 1 } };

    domain::Phrase lydian;
    lydian.mode = domain::Mode::Lydian;
    lydian.tonic = domain::Note{ 50 };
    lydian.steps = { domain::PhraseStep{ .degree = 1, .beats = 4 },
                     domain::PhraseStep{ .degree = 4, .beats = 1 },
                     domain::PhraseStep{ .degree = 1, .beats = 1 } };

    phraseBook.add( slowDorian );
    phraseBook.add( lydian );

    controller.setPhraseBook( phraseBook );

    EXPECT_EQ( 1, controller.phraseCountForMode( static_cast<int>( domain::Mode::Dorian ) ) );
    EXPECT_EQ( 1, controller.phraseCountForMode( static_cast<int>( domain::Mode::Lydian ) ) );
    EXPECT_EQ( 0, controller.phraseCountForMode( static_cast<int>( domain::Mode::Locrian ) ) );

    controller.playPhraseOfMode( static_cast<int>( domain::Mode::Dorian ) );

    const std::vector<domain::NotePlayerFake::PlayedPhraseOverDrone> & played = notePlayer.phrasesOverDrones();

    ASSERT_EQ( 1U, played.size() );

    // La phrase a ete jouee SUR un bourdon, comme une gamme : c'est ce qui en fait un mode, et non une suite de notes.
    ASSERT_EQ( 2U, played.front().drone.size() );

    // Et chaque pas a garde SA duree, dans l'ordre, au tempo du REGLAGE. C'est tout ce qu'une phrase a de plus qu'une
    // gamme : la perdre reviendrait a faire ecouter autre chose que ce que l'oreille avait choisi a l'atelier.
    ASSERT_EQ( 3U, played.front().durations.size() );
    EXPECT_EQ( 1000, played.front().durations.at( 0 ).count() );
    EXPECT_EQ( 2000, played.front().durations.at( 1 ).count() );
    EXPECT_EQ( 1000, played.front().durations.at( 2 ).count() );

    // La mauvaise phrase n'est pas celle qui a sonne : 4 temps d'entree, c'etait la lydienne.
    EXPECT_NE( 4000, played.front().durations.at( 0 ).count() );

    // Et ce que l'ecran affichera, dans l'ecriture de l'atelier : ce que l'oreille a juge peut se relire ici.
    EXPECT_EQ( "1 3(2) 1", controller.lastPlayedPhrase().value( "degrees" ).toString() );
    EXPECT_EQ( "dorian", controller.lastPlayedMode().value( "identifier" ).toString() );
}

TEST( ModePreviewControllerTest, a_mode_without_a_phrase_plays_nothing_at_all )
{
    // Un contenu qui n'a pas encore servi un mode n'est pas une panne : rien ne sonne, et l'ecran n'offre pas le bouton.
    domain::NotePlayerFake notePlayer;
    ModePreviewController controller{ notePlayer };

    domain::PhraseBook phraseBook;

    domain::Phrase dorian;
    dorian.mode = domain::Mode::Dorian;
    dorian.steps = { domain::PhraseStep{ .degree = 1, .beats = 1 },
                     domain::PhraseStep{ .degree = 3, .beats = 1 },
                     domain::PhraseStep{ .degree = 1, .beats = 1 } };

    phraseBook.add( dorian );

    controller.setPhraseBook( phraseBook );
    controller.playPhraseOfMode( static_cast<int>( domain::Mode::Locrian ) );

    EXPECT_TRUE( notePlayer.phrasesOverDrones().empty() );
    EXPECT_FALSE( controller.lastPlayedPhrase().contains( "degrees" ) );
}

TEST( ModePreviewControllerTest, an_index_that_designs_no_mode_is_ignored )
{
    domain::NotePlayerFake notePlayer;
    ModePreviewController controller{ notePlayer };

    const domain::PhraseBook emptyBook;
    controller.setPhraseBook( emptyBook );

    controller.playPhraseOfMode( -1 );
    controller.playPhraseOfMode( 99 );

    EXPECT_TRUE( notePlayer.phrasesOverDrones().empty() );
    EXPECT_EQ( 0, controller.phraseCountForMode( -1 ) );
    EXPECT_EQ( 0, controller.phraseCountForMode( 99 ) );
}

}    // namespace musichien::ui
