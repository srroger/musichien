// =====================================================================================================================
// Musichien - main screen
// This is the landing screen of the project. It exists to prove the whole chain works: domain,
// application, QML, resources, and eventually the deployment on the device.
// Conventions applied here (see docs/CODE_CONVENTIONS.md):
//   * file name in CamelCase, starting with an upper case letter
//   * JavaScript function parameters prefixed with p_
//   * properties of an item are NOT prefixed
//   * every displayed string goes through qsTr() so that it can be translated
//   * no game rule lives in this file
// =====================================================================================================================

// The view models of the application, registered as singletons from main().
import Musichien
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    // Plays a sound and reveals the explanatory text.

    id: mainWindow

    // Local UI state: it belongs to the screen, not to the domain.
    property bool feedbackVisible: false

    // QML function parameters follow the p_ rule, exactly like in C++. Note that a function is passed
    // as a parameter here, which is what lets the three buttons share this code without a
    // string based dispatch that no compiler would ever check.
    function playAndShowFeedback(p_playAction) {
        p_playAction();
        feedbackVisible = true;
        feedbackTimer.restart();
    }

    width: 420
    height: 820
    minimumWidth: 320
    minimumHeight: 480
    visible: true
    title: qsTr("Musichien")

    Rectangle {
        anchors.fill: parent

        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(parent.width * 0.82, 360)
            spacing: 18

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "\uD83D\uDC36" // dog face
                font.pixelSize: 96
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Musichien")
                color: "#ffffff"
                font.pixelSize: 40
                font.bold: true
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Ton oreille musicale, un peu chaque jour.")
                color: "#cbb8e8"
                font.pixelSize: 17
                wrapMode: Text.WordWrap
            }

            Item {
                Layout.preferredHeight: 8
            }

            Button {
                id: singleNoteButton

                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                height: 52
                text: qsTr("Écouter une note")
                onClicked: mainWindow.playAndShowFeedback(function() {
                    IntervalController.playSingleNote();
                })
            }

            Button {
                id: melodicFifthButton

                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                height: 52
                text: qsTr("Quinte, deux notes")
                onClicked: mainWindow.playAndShowFeedback(function() {
                    IntervalController.playMelodicFifth();
                })
            }

            Button {
                id: chordFifthButton

                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                height: 52
                text: qsTr("Quinte, en accord")
                onClicked: mainWindow.playAndShowFeedback(function() {
                    IntervalController.playPerfectFifth();
                })
            }

            // The name of the interval is produced by the DOMAIN and merely displayed here: the
            // interface never names an interval itself.
            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                color: "#cbb8e8"
                font.pixelSize: 16
                wrapMode: Text.WordWrap
                visible: mainWindow.feedbackVisible
                text: IntervalController.lastPlayedIntervalName !== "" ? qsTr("Entendu : %1").arg(IntervalController.lastPlayedIntervalName) : qsTr("Écoute bien, puis nomme ce que tu entends.")
            }

        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 20
            color: "#6f5b93"
            font.pixelSize: 13
            text: qsTr("version %1").arg(Qt.application.version)
        }

        gradient: Gradient {
            GradientStop {
                position: 0
                color: "#1b1035"
            }

            GradientStop {
                position: 1
                color: "#3a1f5c"
            }

        }

    }

    // Hides the explanatory text after a few seconds.
    Timer {
        id: feedbackTimer

        interval: 4000
        onTriggered: mainWindow.feedbackVisible = false
    }

}
