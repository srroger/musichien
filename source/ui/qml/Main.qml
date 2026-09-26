// =====================================================================================================================
// Musichien - main screen
//
// This is the landing screen of the project. It exists to prove the whole chain works: domain,
// application, QML, resources, and eventually the deployment on the device.
//
// Conventions applied here (see docs/CODE_CONVENTIONS.md):
//   * file name in CamelCase, starting with an upper case letter
//   * JavaScript function parameters prefixed with p_
//   * properties of an item are NOT prefixed
//   * every displayed string goes through qsTr() so that it can be translated
//   * no game rule lives in this file
// =====================================================================================================================

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow
{
    id: mainWindow

    width: 420
    height: 820
    minimumWidth: 320
    minimumHeight: 480
    visible: true
    title: qsTr( "Musichien" )

    // Local UI state: it belongs to the screen, not to the domain.
    property bool comingSoonMessageVisible: false

    // Called when the player taps the main button.
    function startTraining( p_ageInDays )
    {
        comingSoonMessageVisible = true;
        comingSoonMessageTimer.restart();
    }

    Rectangle
    {
        anchors.fill: parent

        gradient: Gradient
        {
            GradientStop { position: 0.0; color: "#1b1035" }
            GradientStop { position: 1.0; color: "#3a1f5c" }
        }

        ColumnLayout
        {
            anchors.centerIn: parent
            width: Math.min( parent.width * 0.82, 360 )
            spacing: 18

            Text
            {
                Layout.alignment: Qt.AlignHCenter
                text: "\uD83D\uDC36"     // dog face
                font.pixelSize: 96
            }

            Text
            {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr( "Musichien" )
                color: "#ffffff"
                font.pixelSize: 40
                font.bold: true
            }

            Text
            {
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: qsTr( "Ton oreille musicale, un peu chaque jour." )
                color: "#cbb8e8"
                font.pixelSize: 17
                wrapMode: Text.WordWrap
            }

            Item { Layout.preferredHeight: 8 }

            Button
            {
                id: startButton

                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                text: qsTr( "Entraîner mon oreille" )
                height: 52

                onClicked: mainWindow.startTraining( 0 )
            }

            Text
            {
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: qsTr( "Bientôt : intervalles, rythme, chant." )
                color: "#9d86c4"
                font.pixelSize: 15
                visible: mainWindow.comingSoonMessageVisible
                wrapMode: Text.WordWrap
            }
        }

        Text
        {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 20
            color: "#6f5b93"
            font.pixelSize: 13
            text: qsTr( "version %1" ).arg( Qt.application.version )
        }
    }

    // Hides the "coming soon" message after a few seconds.
    Timer
    {
        id: comingSoonMessageTimer

        interval: 2600
        onTriggered: mainWindow.comingSoonMessageVisible = false
    }
}
