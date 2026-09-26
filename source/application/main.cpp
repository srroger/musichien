// =====================================================================================================================
// Musichien - application entry point
//
// This file only wires things together: it creates the QML engine, exposes the domain to QML and
// hands control over. No musical logic lives here, and no rule of the game either.
// =====================================================================================================================

#include "domain/music/Interval.h"

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QUrl>

#include <iostream>

namespace
{

// Version of the application, injected by the build system so that there is only one place to
// maintain it: the project() call of the top level CMakeLists.txt.
constexpr const char * APPLICATION_NAME = "Musichien";

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

    // A small sanity check of the domain, printed once at start up.
    // It disappears as soon as the first screen consumes the domain for real.
    const musichien::domain::Interval perfectFifth = musichien::domain::intervalBetween(
        musichien::domain::Note{ 60 }, musichien::domain::Note{ 67 } );

    std::cout << APPLICATION_NAME << " " << MUSICHIEN_VERSION << " - domain ready (example: "
              << perfectFifth.name() << ")\n";

    QQmlApplicationEngine qmlEngine;

    // The QML is embedded into the executable through resources.qrc, so the path is identical on the
    // desktop and on the device, and nothing has to be copied next to the binary.
    qmlEngine.load( QUrl{ QStringLiteral( "qrc:/qml/Main.qml" ) } );

    if ( qmlEngine.rootObjects().isEmpty() )
    {
        std::cerr << "Musichien: the QML scene could not be loaded\n";
        return -1;
    }

    // exec() is static as well: the application object only exists to hold the global state.
    return QGuiApplication::exec();


}
