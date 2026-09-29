#include "ui/MicrophoneController.h"

#include "domain/audio/PitchDetector.h"

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

    constexpr std::array<Octave, 4> OCTAVES{ { { 110.0, "A2  110 Hz", -2 },
                                               { 220.0, "A3  220 Hz", -1 },
                                               { 440.0, "A4  440 Hz", 0 },
                                               { 880.0, "A5  880 Hz", +1 } } };

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

}    // namespace musichien::ui
