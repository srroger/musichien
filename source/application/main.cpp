// =====================================================================================================================
// Musichien - application entry point
//
// This file only wires things together: it creates the QML engine, exposes the domain to QML and
// hands control over. No musical logic lives here, and no rule of the game either.
// =====================================================================================================================

#include "infrastructure/audio/QAudioNotePlayer.h"
#include "ui/IntervalPlaybackController.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QUrl>
#include <QtQml>

#include <iostream>

namespace
{

// Version of the application, injected by the build system so that there is only one place to
// maintain it: the project() call of the top level CMakeLists.txt.
constexpr const char * APPLICATION_NAME = "Musichien";

// QML module under which the view models are exposed to the interface.
//
// Registering a singleton rather than a context property keeps the QML honest: it imports a module and
// calls a named object, instead of relying on a global name injected from the outside.
constexpr const char * QML_MODULE_NAME = "Musichien";
constexpr int QML_MODULE_MAJOR_VERSION = 1;
constexpr int QML_MODULE_MINOR_VERSION = 0;

}    // namespace

int main( int p_argumentCount, char * p_arguments[] )
{
    QGuiApplication application{ p_argumentCount, p_arguments };

    // These members are static in Qt: calling them on the class makes that explicit, and avoids a
    // "static member accessed through instance" finding.
    QGuiApplication::setApplicationName( APPLICATION_NAME );
    QGuiApplication::setApplicationVersion( MUSICHIEN_VERSION );
    QGuiApplication::setOrganizationName( "Musichien" );

    // Material is the style Qt Quick Controls maps onto the Android look and feel. Using it from the
    // first line guarantees that what is developed on the desktop looks like what runs on the phone.
    QQuickStyle::setStyle( "Material" );

    // Diagnostics go to std::cerr on purpose: stderr is not buffered, so these lines always appear
    // immediately, even when the output is redirected to a file or to a pipe.
    std::cerr << APPLICATION_NAME << " " << MUSICHIEN_VERSION << "\n";

    // -------------------------------------------------------------------------------------------------------------
    // Wiring
    //
    // The domain declares a port (NotePlayer), the infrastructure provides an adapter
    // (QAudioNotePlayer), and this function connects the two. It is the only place in the project that
    // knows about the domain, the interface and the concrete implementations at the same time.
    // -------------------------------------------------------------------------------------------------------------
    musichien::infrastructure::QAudioNotePlayer notePlayer;

    // Opening the output now, rather than at the first note, means a machine without a sound card is
    // reported at start up instead of silently refusing to play in the middle of an exercise.
    notePlayer.prepareAudioOutput();

    std::cerr << "Musichien: audio output is " << notePlayer.audioOutputDescription() << "\n";

    // The view model only receives the PORT, never the adapter: it could be handed the fake player of
    // the unit tests without a single line of it changing.
    musichien::ui::IntervalPlaybackController intervalController{ notePlayer };

    qmlRegisterSingletonInstance( QML_MODULE_NAME,
                                  QML_MODULE_MAJOR_VERSION,
                                  QML_MODULE_MINOR_VERSION,
                                  "IntervalController",
                                  &intervalController );

    QQmlApplicationEngine qmlEngine;

    // The QML is embedded into the executable through resources.qrc, so the path is identical on the
    // desktop and on the device, and nothing has to be copied next to the binary.
    qmlEngine.load( QUrl{ QStringLiteral( "qrc:/qml/Main.qml" ) } );

    if( qmlEngine.rootObjects().isEmpty() )
    {
        std::cerr << "Musichien: the QML scene could not be loaded\n";
        return -1;
    }

    const int exitCode = QGuiApplication::exec();

    // Whatever happens, the audio device is released before leaving. Leaving an output stream open on
    // a phone is a battery drain, and a bug.
    notePlayer.stopAll();

    return exitCode;
}
