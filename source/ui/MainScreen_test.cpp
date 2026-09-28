#include "domain/audio/NotePlayerFake.h"
#include "ui/ExerciseSessionController.h"
#include "ui/IntervalPlaybackController.h"
#include "ui/MicrophoneController.h"
#include "ui/RhythmController.h"
#include "ui/StatisticsController.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariant>

#include <gtest/gtest.h>

#include <memory>

namespace musichien::ui
{

// ---------------------------------------------------------------------------------------------------------------------
// L'ecran PRINCIPAL, charge pour de vrai
//
// ExerciseScreen_test instancie l'ecran d'exercice depuis le debut, et il a deja servi : il a trouve une boucle de
// liaison, et il est le seul endroit qui aurait vu une propriete declaree deux fois. Main.qml, lui, n'etait charge par
// AUCUN test - alors que c'est la page sur laquelle l'application s'ouvre, et celle ou vivent tous les reglages.
//
// Un QML casse ne fait echouer aucun test de view model, ne casse aucun build, et fait demarrer l'application sur une
// page noire : c'est le joueur qui le decouvre. Ce fichier existe pour que ce soit le build.
//
// Les quatre singletons sont enregistres ICI, dans la meme forme que main.cpp : c'est le prix d'un ecran qui lit quatre
// view models, et il se paie une fois.
// ---------------------------------------------------------------------------------------------------------------------

namespace
{

[[maybe_unused]] const bool OFFSCREEN_IS_IMPOSED = qputenv( "QT_QPA_PLATFORM", "offscreen" );

// Les messages de QML, retenus plutot que perdus sur la sortie d'erreur du binaire de test.
[[nodiscard]] QStringList & qmlMessages()
{
    static QStringList messages;

    return messages;
}

[[maybe_unused]] const bool MESSAGES_ARE_CAPTURED = [] {
    qInstallMessageHandler( []( QtMsgType, const QMessageLogContext &, const QString & p_message ) {
        qmlMessages().append( p_message );
    } );

    return true;
}();

// Une application Qt pour tout le binaire : QML ne peut pas instancier un Item sans elle, et il n'en faut qu'une.
//
// Sur le TAS et jamais detruite, comme dans ExerciseScreen_test : detruire une application Qt pendant que des objets Qt
// vivent encore termine un processus de test par un segfault, APRES que tous les tests ont passe.
[[nodiscard]] QGuiApplication & application()
{
    static int argumentCount = 1;
    static char argumentName[]{ "test_ui" };
    static char * arguments[]{ argumentName, nullptr };

    static QGuiApplication * instance = new QGuiApplication{ argumentCount, arguments };

    return *instance;
}

// Les quatre view models de la page, exposes au QML comme main.cpp le fait.
//
// Ils vivent sur le TAS pour la meme raison que l'application : qmlRegisterSingletonInstance prend POSSESSION de
// l'instance, et un objet statique se ferait detruire une fois par Qt et une fois par le programme.
void registerViewModels()
{
    application();

    static auto * notePlayer = new domain::NotePlayerFake;

    static auto * exerciseController = new ExerciseSessionController{ *notePlayer };
    static auto * intervalController = new IntervalPlaybackController{ *notePlayer };
    static auto * rhythmController = new RhythmController{ *notePlayer };

    static auto * microphoneController = new MicrophoneController{ QStringList{}, {}, nullptr, notePlayer };

    // Le journal et la page de statistiques : la page de profil lit le premier a travers le second, et sans eux
    // Main.qml ne se charge pas du tout.
    static auto * questionLog = new domain::QuestionLogFake;
    static auto * statisticsController = new StatisticsController{ *questionLog };

    static const bool REGISTERED = [] {
        qmlRegisterSingletonInstance( "Musichien", 1, 0, "ExerciseController", exerciseController );
        qmlRegisterSingletonInstance( "Musichien", 1, 0, "IntervalController", intervalController );
        qmlRegisterSingletonInstance( "Musichien", 1, 0, "RhythmController", rhythmController );
        qmlRegisterSingletonInstance( "Musichien", 1, 0, "MicrophoneController", microphoneController );
        qmlRegisterSingletonInstance( "Musichien", 1, 0, "StatisticsController", statisticsController );

        return true;
    }();

    (void)REGISTERED;
}

// Tous les textes affiches a l'ecran, en profondeur : c'est ce qui permet de demander « le bouton Jouer est-il la ? »
// sans dependre de la moindre position.
void collectTexts( QObject * p_object, QStringList & p_texts )
{
    if( p_object == nullptr )
    {
        return;
    }

    const QVariant text = p_object->property( "text" );

    if( text.isValid() && !text.toString().isEmpty() )
    {
        p_texts.append( text.toString() );
    }

    for( QObject * child : p_object->children() )
    {
        collectTexts( child, p_texts );
    }
}

[[nodiscard]] std::unique_ptr<QObject> loadMainScreen( QQmlEngine & p_engine )
{
    registerViewModels();

    QQmlComponent component{ &p_engine, QUrl{ QStringLiteral( "qrc:/qml/Main.qml" ) } };

    std::unique_ptr<QObject> root{ component.create() };

    if( root == nullptr )
    {
        qmlMessages().append( component.errorString() );
    }

    return root;
}

}    // namespace

TEST( MainScreenTest, the_main_screen_instantiates )
{
    // L'application AVANT le moteur : QQmlEngine refuse d'exister sans instance de QCoreApplication, et il le dit en
    // ABORTANT. C'est exactement le crash que ce fichier a attrape la premiere fois qu'il a tourne - et c'est la raison
    // pour laquelle Main.qml a maintenant un test.
    application();

    QQmlEngine engine;

    const std::unique_ptr<QObject> screen = loadMainScreen( engine );

    ASSERT_NE( nullptr, screen.get() )
      << "l'ecran principal n'a pas pu etre instancie : " << qmlMessages().join( '\n' ).toStdString();
}

TEST( MainScreenTest, the_main_screen_shows_its_main_actions )
{
    application();

    QQmlEngine engine;

    const std::unique_ptr<QObject> screen = loadMainScreen( engine );

    ASSERT_NE( nullptr, screen.get() );

    QStringList texts;

    collectTexts( screen.get(), texts );

    // La porte d'entree de l'application. Le test cherche un MOT, et non un libelle exact : une icone peut s'ajouter
    // devant, et ce fichier ne doit pas se casser a chaque retouche d'orthographe d'un bouton.
    const auto hasTextContaining = [&texts]( const QString & p_word ) {
        return std::ranges::any_of( texts, [&p_word]( const QString & p_text ) {
            return p_text.contains( p_word );
        } );
    };

    EXPECT_TRUE( hasTextContaining( QStringLiteral( "Jouer" ) ) ) << "le bouton Jouer est absent de l'ecran principal";

    // Et la page construit vraiment quelque chose : une vingtaine de textes, c'est le minimum d'une page de reglages
    // qui en compte des dizaines.
    EXPECT_GT( texts.size(), 20 ) << "l'ecran principal ne construit presque rien";
}

}    // namespace musichien::ui
