// =====================================================================================================================
// Musichien - application entry point
//
// This file only wires things together: it creates the QML engine, exposes the domain to QML and
// hands control over. No musical logic lives here, and no rule of the game either.
// =====================================================================================================================

#include "infrastructure/audio/QAudioNotePlayer.h"
#include "infrastructure/content/JsonHintBook.h"
#include "infrastructure/haptics/DeviceHaptics.h"
#include "ui/ExerciseSessionController.h"
#include "ui/IntervalPlaybackController.h"

#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QUrl>
#include <QtQml>

#include <iostream>
#include <string_view>

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

// Where the content files live once embedded. The path is the one they have on disk, thanks to the alias
// declared in resources.qrc: one path to remember, identical on the desktop and on the phone.
constexpr const char * INTERVAL_HINTS_RESOURCE = ":/assets/content/interval-hints.json";

// Reads the memory hints from the resources.
//
// The file is opened HERE and not inside the reader: understanding JSON is the infrastructure's job,
// reading a Qt resource is an application concern, and this is the layer allowed to know both.
//
// Missing or broken content is deliberately not fatal. A game that refuses to start because a hint is
// malformed would trade a small loss for a total one.
[[nodiscard]] musichien::domain::HintBook loadHintBook()
{
    QFile contentFile{ QString::fromUtf8( INTERVAL_HINTS_RESOURCE ) };

    if( !contentFile.open( QIODevice::ReadOnly ) )
    {
        std::cerr << "Musichien: the interval hints are missing from the resources.\n";

        return {};
    }

    const QByteArray content = contentFile.readAll();

    musichien::domain::HintBook hintBook = musichien::infrastructure::readHintBook(
      std::string_view{ content.constData(), static_cast<std::size_t>( content.size() ) } );

    std::cerr << "Musichien: " << hintBook.hintCount() << " interval hints read\n";

    return hintBook;
}

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

    // The exercise screen has its own view model. It receives the SAME port, and neither controller
    // knows the other exists: the bench and the loop are two independent uses of the same domain.
    //
    // The vibration is injected as a function rather than called from the view model, and the hint book
    // is handed over to be owned: one keeps Android out of the interface, the other keeps a reference to
    // somebody else's object out of it.
    musichien::ui::ExerciseSessionController exerciseController{ notePlayer,
                                                                 {},
                                                                 loadHintBook(),
                                                                 musichien::infrastructure::vibrateForMistake };

    qmlRegisterSingletonInstance( QML_MODULE_NAME,
                                  QML_MODULE_MAJOR_VERSION,
                                  QML_MODULE_MINOR_VERSION,
                                  "ExerciseController",
                                  &exerciseController );

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
