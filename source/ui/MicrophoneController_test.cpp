#include "ui/MicrophoneController.h"

#include "domain/audio/PitchDetector.h"
#include "domain/music/StaffPosition.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QString>
#include <QStringList>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <memory>
#include <utility>

namespace musichien::ui
{

// ---------------------------------------------------------------------------------------------------------------------
// La lecture de l'accordeur
//
// L'accordeur affiche DEUX choses differentes, et les confondre serait une faute :
//   * la BOULE est repliee sur l'octave, pour qu'elle ne quitte jamais les cinq lignes de la portee ;
//   * la NOTE AFFICHEE est la vraie - un do grave est un do grave, jamais le do aigu a cote duquel la boule se pose.
//
// Le repli de la boule est une decision de DESSIN ; replier le nom serait un MENSONGE musical. Rien ne verifiait la
// difference : le jour ou l'un des deux se serait replie par erreur, l'accordeur aurait continue d'afficher une
// reponse d'apparence juste.
// ---------------------------------------------------------------------------------------------------------------------

namespace
{

// Une application Qt pour toute la duree du binaire de test, sur le TAS et jamais detruite : QML ne peut pas
// instancier un Item sans elle, et la detruire pendant que des objets Qt vivent encore termine le processus APRES que
// tous les tests ont passe. Les autres fichiers de test font la meme chose, pour la meme raison.
[[nodiscard]] QGuiApplication & application()
{
    static int argumentCount = 1;
    static char argumentName[]{ "test_ui" };
    static char * arguments[]{ argumentName, nullptr };

    static QGuiApplication * instance = new QGuiApplication{ argumentCount, arguments };

    return *instance;
}

// Un detecteur qui entend ce que le test decide : ni microphone, ni voix, ni temps reel. Le controleur le recoit par
// sa fabrique, exactement comme l'application lui donne un QAudioPitchDetector - ce qui est donc eprouve ici est le
// VRAI chemin de lecture, seules les oreilles sont fausses.
class PitchDetectorFake final : public domain::PitchDetector
{
public:
    void start( PitchCallback p_callback ) override
    {
        m_callback = std::move( p_callback );
        ++m_startCount;
    }

    void stop() override { m_callback = nullptr; }

    // Le geste du test : on fait entendre une frequence, comme le micro le ferait.
    void hear( double p_frequencyHz )
    {
        if( m_callback )
        {
            m_callback( static_cast<float>( p_frequencyHz ) );
        }
    }

    // Combien de fois le micro a ete OUVERT : c'est ce qui permet de verifier qu'une demande repetee ne le rouvre pas.
    [[nodiscard]] int startCount() const { return m_startCount; }

private:
    PitchCallback m_callback;
    int m_startCount{ 0 };
};

// Le controleur, son faux detecteur, et le doigt sur le micro : les trois sont livres ensemble parce que le test a
// besoin du detecteur pour parler.
struct MicrophoneUnderTest
{
    MicrophoneController controller;
    PitchDetectorFake * detector{ nullptr };

    MicrophoneUnderTest()
      : controller{ QStringList{ QStringLiteral( "micro de test" ) },
                    [this]( int ) {
                        auto created = std::make_unique<PitchDetectorFake>();
                        detector = created.get();

                        return created;
                    } }
    {
    }

    // Demarre l'ecoute. La permission du micro passe par la file d'evenements de Qt, meme lorsqu'elle est accordee :
    // on lui laisse donc le tour de boucle dont elle a besoin avant de continuer.
    void start()
    {
        controller.startTest();
        QCoreApplication::processEvents();
    }
};

}    // namespace

// Le cas qui a motive ce fichier : le MEME la, dans quatre octaves. La boule se pose quatre fois au meme endroit -
// c'est ce qui la garde sur la portee - et le nom, lui, doit dire A2, A3, A4, A5. Si un jour le repli de la boule
// venait a se melanger avec la lecture de la note, c'est ce test qui le dirait, et il nommerait l'octave fautive.
//
// Le SIGNE compte autant que le nom : c'est lui qui dit, sans quitter la portee des yeux, que la note est ailleurs.
TEST( MicrophoneControllerTest, every_octave_of_a_note_is_named_while_the_ball_stands_still )
{
    (void)application();

    struct Octave
    {
        double frequencyHz;
        const char * expectedLabel;
        int expectedOctaveShift;
    };

    constexpr std::array<Octave, 4> OCTAVES{ { { 110.0, "A2  110.0 Hz", -2 },
                                               { 220.0, "A3  220.0 Hz", -1 },
                                               { 440.0, "A4  440.0 Hz", 0 },
                                               { 880.0, "A5  880.0 Hz", +1 } } };

    MicrophoneUnderTest test;

    double ballFraction = 0.0;
    bool ballFractionIsKnown = false;

    for( const Octave & octave : OCTAVES )
    {
        // Chaque octave est ecoutee DEPUIS LE SILENCE : le controleur lisse ses lectures pour que la boule glisse au
        // lieu de trembler, et deux notes qui se suivent sans silence se melangeraient.
        test.start();

        ASSERT_TRUE( test.controller.isListening() );
        ASSERT_NE( test.detector, nullptr );

        test.detector->hear( octave.frequencyHz );

        EXPECT_EQ( test.controller.detectedNoteLabel(), QString::fromLatin1( octave.expectedLabel ) );
        EXPECT_NEAR( test.controller.detectedCents(), 0.0, 0.01 );
        EXPECT_EQ( test.controller.detectedOctaveShift(), octave.expectedOctaveShift );

        if( !ballFractionIsKnown )
        {
            ballFraction = test.controller.detectedStaffFraction();
            ballFractionIsKnown = true;
        }
        else
        {
            EXPECT_DOUBLE_EQ( test.controller.detectedStaffFraction(), ballFraction );
        }

        test.controller.stopTest();
    }
}

// La frequence lue s'affiche avec UNE decimale : c'est ce qui permet de voir une corde bouger de 440,0 a 440,4 Hz
// quand on la tourne, ou de comparer deux instruments au dixieme de hertz. Deux decimales mentiraient sur la finesse
// de la mesure, aucune priverait l'accordeur de ce qu'il a de plus utile.
TEST( MicrophoneControllerTest, the_frequency_read_is_shown_with_its_decimal )
{
    (void)application();

    MicrophoneUnderTest test;

    test.start();

    ASSERT_TRUE( test.controller.isListening() );
    ASSERT_NE( test.detector, nullptr );

    // Un peu au-dessus du la : la note reste le la, et la frequence garde sa decimale.
    test.detector->hear( 442.3 );

    EXPECT_EQ( test.controller.detectedNoteLabel(), QStringLiteral( "A4  442.3 Hz" ) );
}

// Sans chant, l'ecart est NUL et non une valeur inventee : un ecran qui afficherait « +0 cents » avant que le joueur
// ait chante quoi que ce soit lui apprendrait a ne plus lire ce chiffre.
TEST( MicrophoneControllerTest, no_sung_interval_means_no_offset_at_all )
{
    (void)application();

    MicrophoneUnderTest test;

    test.start();

    ASSERT_TRUE( test.controller.isListening() );

    EXPECT_FALSE( test.controller.hasSungInterval() );
    EXPECT_EQ( 0, test.controller.sungCentsOffset() );
    EXPECT_EQ( 0, test.controller.sungVerdict() );
}

// L'ecoute demandee deux fois ne rouvre pas le micro : l'ecran d'accueil et la page Accordeur la demandent tous les
// deux, et relancer le peripherique s'entendrait sous la forme d'un clic.
TEST( MicrophoneControllerTest, asking_to_listen_twice_opens_the_microphone_once )
{
    (void)application();

    MicrophoneUnderTest test;

    test.start();

    ASSERT_TRUE( test.controller.isListening() );

    const int startsBefore = test.detector->startCount();

    test.controller.ensureListening();

    EXPECT_EQ( test.detector->startCount(), startsBefore );
    EXPECT_TRUE( test.controller.isListening() );
}

// L'ecart en cents se mesure contre la note REELLE : une note juste reste juste dans toutes les octaves, meme si la
// boule les dessine au meme endroit. Sans cela, l'accordeur aurait dit « juste » d'une note qui ne l'est pas.
TEST( MicrophoneControllerTest, the_cents_are_measured_against_the_real_octave )
{
    (void)application();

    MicrophoneUnderTest test;

    test.start();

    ASSERT_TRUE( test.controller.isListening() );
    ASSERT_NE( test.detector, nullptr );

    test.detector->hear( 880.0 );

    EXPECT_NEAR( test.controller.detectedCents(), 0.0, 0.01 );
    EXPECT_EQ( test.controller.detectedTuningState(), 0 );

    // Et la meme note, onze cents plus bas : elle n'est plus tout a fait juste, et l'accordeur le dit - dans cette
    // octave comme dans les autres.
    test.controller.stopTest();
    test.start();

    ASSERT_TRUE( test.controller.isListening() );

    test.detector->hear( 880.0 * std::pow( 2.0, -11.0 / 1200.0 ) );

    EXPECT_NEAR( test.controller.detectedCents(), -11.0, 1.0 );
    EXPECT_EQ( test.controller.detectedTuningState(), 1 );
}

// Le silence : rien plutot qu'une note inventee. La boule disparait, et l'ecran affiche un tiret.
TEST( MicrophoneControllerTest, silence_is_a_dash_and_not_a_note )
{
    (void)application();

    MicrophoneUnderTest test;

    test.start();

    ASSERT_TRUE( test.controller.isListening() );

    test.controller.stopTest();

    EXPECT_FALSE( test.controller.isListening() );
    EXPECT_EQ( test.controller.detectedNoteLabel(), QStringLiteral( "\u2014" ) );
}

// LE MICRO SE REPREND AU RETOUR DE L'APPLICATION. C'est le bug qui empechait de jouer : Android rend le peripherique des
// que l'application s'efface, et personne ne le redemandait. Quitter l'ecran puis y revenir le rouvrait par ACCIDENT - ce
// qui donnait l'impression d'un jeu capricieux plutot que d'un micro repris par la plateforme.
TEST( MicrophoneControllerTest, the_microphone_is_taken_back_when_the_application_returns )
{
    (void)application();

    MicrophoneUnderTest test;

    test.start();

    ASSERT_TRUE( test.controller.isListening() );
    ASSERT_NE( test.detector, nullptr );

    const int startsBefore = test.detector->startCount();

    // L'application s'efface : Android reprend le micro, et on ferme proprement derriere lui.
    test.controller.handleApplicationSuspended();

    // Le retour : le peripherique doit etre ROUVERT, sans que personne n'ait touche a l'ecran.
    test.controller.handleApplicationResumed();

    QCoreApplication::processEvents();

    EXPECT_TRUE( test.controller.isListening() );
    EXPECT_EQ( test.detector->startCount(), startsBefore + 1 );

    // Et il ENTEND vraiment : un peripherique rouvert mais muet ne vaudrait pas mieux qu'un peripherique ferme.
    test.detector->hear( 440.0 );

    EXPECT_EQ( test.controller.detectedNoteLabel(), QStringLiteral( "A4  440.0 Hz" ) );
}

// RIEN A ROUVRIR QUAND PERSONNE N'ECOUTAIT. Le retour au premier plan n'allume pas le micro tout seul : ouvrir un
// peripherique que l'ecran n'a pas demande vide la batterie, et poserait une boule sur un ecran qui n'en veut pas.
//
// C'est aussi ce qui protege l'ordinateur de bureau, ou l'arriere-plan n'existe pas vraiment : l'etat « actif » y revient
// a chaque regain de FOCUS, et relancer le peripherique a cet instant ferait cliquer l'accordeur pour rien.
TEST( MicrophoneControllerTest, returning_to_the_foreground_opens_nothing_when_nobody_was_listening )
{
    (void)application();

    MicrophoneUnderTest test;

    test.controller.handleApplicationSuspended();
    test.controller.handleApplicationResumed();

    QCoreApplication::processEvents();

    EXPECT_FALSE( test.controller.isListening() );
    EXPECT_EQ( test.detector, nullptr );
}

// UN ALLER-RETOUR QUI N'A RIEN FERME NE ROUVRE RIEN. C'est le cas du bureau, et il merite son propre test : « actif »
// revient a chaque fois que la fenetre reprend le focus, et un micro relance a chaque clic hors de la fenetre
// s'entendrait - un clic dans le casque, a chaque fois.
TEST( MicrophoneControllerTest, a_foreground_return_without_a_suspend_does_not_restart_the_microphone )
{
    (void)application();

    MicrophoneUnderTest test;

    test.start();

    ASSERT_TRUE( test.controller.isListening() );

    const int startsBefore = test.detector->startCount();

    // Pas de handleApplicationSuspended() : l'application n'a jamais quitte le premier plan.
    test.controller.handleApplicationResumed();

    QCoreApplication::processEvents();

    EXPECT_EQ( test.detector->startCount(), startsBefore );
    EXPECT_TRUE( test.controller.isListening() );
}

// LE MIROIR D'UN COURS DONNE L'INTERVALLE, ET NE TIRE RIEN. Le cours de la quinte promet « aucun score : c'est un miroir,
// pas un juge » - et la carte ouvrait pourtant une SESSION, dont le bouton « Suivant » tire un intervalle AU HASARD, donc
// fait quitter celui que la page venait de faire entendre.
TEST( MicrophoneControllerTest, a_singing_mirror_keeps_the_given_interval_and_counts_nothing )
{
    (void)application();

    MicrophoneUnderTest test;

    ASSERT_FALSE( test.controller.isSingingMirror() );

    test.controller.openSingingMirror( 7 );

    QCoreApplication::processEvents();

    // L'intervalle est celui du COURS, et il y reste : c'est la seule chose qui distingue un miroir d'un jeu.
    EXPECT_TRUE( test.controller.isSingingMirror() );
    EXPECT_EQ( 7, test.controller.singingTargetSemitones() );

    // Et le micro s'ouvre : un miroir muet serait le pire des echecs, il ferait porter au joueur la faute d'un
    // peripherique ferme.
    EXPECT_TRUE( test.controller.isListening() );
    ASSERT_NE( test.detector, nullptr );

    test.detector->hear( 440.0 );

    EXPECT_EQ( test.controller.detectedNoteLabel(), QStringLiteral( "A4  440.0 Hz" ) );
}

// OUVRIR UNE SESSION FERME LE MIROIR. Les deux ne cohabitent pas : une session compte ses questions et les tire au
// hasard, un miroir renvoie ce qu'on lui donne. Deux modes ouverts a la fois, et le cours promettrait une chose que
// l'ecran contredirait.
TEST( MicrophoneControllerTest, starting_a_session_leaves_the_mirror_behind )
{
    (void)application();

    MicrophoneUnderTest test;

    test.controller.openSingingMirror( 7 );
    QCoreApplication::processEvents();

    ASSERT_TRUE( test.controller.isSingingMirror() );

    test.controller.startSingingSession();

    EXPECT_FALSE( test.controller.isSingingMirror() );
}

// LE MIROIR MONTRE LA NOTE A ATTEINDRE AVANT QU'ON AIT CHANTE UNE SEULE NOTE.
//
// Roger : « la on ne fait pas de scoring, donc on peut carrement mettre le fantome qu'on vient de developper... on veut
// juste guider l'utilisateur ». Et la cible est connue D'AVANCE - c'est meme ce qui rend la chose possible, et ce qui
// distingue un miroir d'un exercice : l'exercice juge un ECART, donc il lui faut la note du joueur ; le miroir guide
// vers une NOTE, donc il l'a deja.
TEST( MicrophoneControllerTest, a_singing_mirror_shows_the_note_to_reach_before_a_single_note )
{
    (void)application();

    MicrophoneUnderTest test;

    test.controller.openSingingMirror( 7 );

    QCoreApplication::processEvents();

    // RIEN n'a ete chante : la fantome a pourtant une position, et c'est tout l'interet.
    EXPECT_FALSE( test.controller.hasFirstNote() );

    // La note a atteindre : un intervalle au-dessus de la tonique de reference. Sans reglage enregistre, c'est le do
    // central (60) - la meme valeur que celle que playSingingTarget() fait entendre.
    EXPECT_DOUBLE_EQ( test.controller.singingTargetStaffFraction(), domain::StaffPosition::fraction( 60 + 7 ) );
}

}    // namespace musichien::ui
