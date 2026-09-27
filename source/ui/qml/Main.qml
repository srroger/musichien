// =====================================================================================================================
// Musichien - main screen
// This is the landing screen of the project. It exists to prove the whole chain works: domain,
// application, QML, resources, and eventually the deployment on the device.
// It also carries a BENCH. The interval model now goes all the way to the fifteenth, and a model
// nobody can hear is a model nobody can trust: so this screen offers one button per interval the
// domain supports, plays it, and prints back what the domain had to say about it.
// Conventions applied here (see docs/CODE_CONVENTIONS.md):
//   * file name in CamelCase, starting with an upper case letter
//   * JavaScript function parameters prefixed with p_
//   * properties of an item are NOT prefixed
//   * every displayed string goes through qsTr() so that it can be translated
//   * no game rule lives in this file
// That last rule is why the buttons below are NOT written out one by one: the list comes from the
// domain, so adding an interval to the domain makes a button appear here without a line of QML
// changing. A screen that hard coded them would drift apart from the model it is supposed to show.
// =====================================================================================================================

// The view models of the application, registered as singletons from main().
import Musichien
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: mainWindow

    // Width shared by the standalone controls, so that they line up without each repeating the rule.
    readonly property real buttonWidth: Math.min(width * 0.82, 340)
    // What the domain said about the interval heard last, and whether there is anything to say at
    // all. An empty map is what a single note produces, because a single note is not an interval.
    readonly property var heardInterval: IntervalController.lastPlayedInterval
    readonly property bool hasHeardInterval: heardInterval.identifier !== undefined && heardInterval.identifier !== ""
    // Local UI state: it belongs to the screen, not to the domain.
    property bool feedbackVisible: false

    // QML function parameters follow the p_ rule, exactly like in C++. Note that a function is passed
    // as a parameter here, which is what lets every button share this code without a string based
    // dispatch that no compiler would ever check.
    function playAndShowFeedback(p_playAction) {
        p_playAction();
        feedbackVisible = true;
        feedbackTimer.restart();
    }

    // Plays the interval at a given distance, then reveals the feedback.
    // The distance is all this screen knows how to say about an interval: it never names one, never
    // decides whether one is simple or compound, and never builds one. It asks, then it displays what
    // came back.
    function playInterval(p_semitones) {
        playAndShowFeedback(function() {
            IntervalController.playInterval(p_semitones);
        });
    }

    // Chooses how the intervals are listened to. The switch is what displays it, and this function is
    // the only path to it: every button goes through here rather than writing the view model behind
    // the switch's back, because two places writing the same state is how a control ends up
    // contradicting what was just played.
    function chooseListeningMode(p_harmonicPlayback) {
        harmonicSwitch.checked = p_harmonicPlayback;
        IntervalController.harmonicPlayback = p_harmonicPlayback;
    }

    // How far above the root the interval sits, in words. A helper rather than a long expression
    // inside a binding, because the same phrasing will be needed on the answer screen.
    function octaveSpanLabel(p_octaveSpan) {
        if (p_octaveSpan === 0)
            return qsTr("dans l'octave");

        if (p_octaveSpan === 1)
            return qsTr("une octave plus haut");

        return qsTr("%1 octaves plus haut").arg(p_octaveSpan);
    }

    width: 420
    height: 820
    minimumWidth: 320
    minimumHeight: 480
    visible: true
    title: qsTr("Musichien")
    // Leaving the screen must never leave an audio stream open: on a phone that is a battery drain,
    // and a bug. The view model was given a method for exactly this call.
    onClosing: IntervalController.stopPlayback()

    Rectangle {
        anchors.fill: parent

        ScrollView {
            id: scrollView

            anchors.fill: parent
            // A session takes over the screen: the bench is not hidden, it is simply not what the
            // application is doing at that moment.
            visible: !ExerciseController.running
            clip: true
            // No horizontal scrolling: the content is laid out to fit the width it is given, and a
            // sideways scroll on a phone is always an accident.
            contentWidth: availableWidth

            ColumnLayout {
                // --------------------------------------------------------------------------------------------
                // The three shortcuts
                // They are presets of the bench below: the quickest way to check that the sound comes
                // out at all, and the only thing this screen could do before there was a model worth
                // testing.
                // --------------------------------------------------------------------------------------------
                // --------------------------------------------------------------------------------------------
                // What the domain had to say about it.
                // The name, the identifier, the class, the number: none of it is computed here, all of it
                // is read back. This is the point of the bench - the screen shows the model, it does not
                // reproduce it.
                // --------------------------------------------------------------------------------------------
                // -----------------------------------------------------------------------------------------
                // Where the player is at
                // Asked right here, and right before "Jouer", because it is the only thing the landing screen
                // needs to know before a session starts - and it decides where the sessions begin. It is NOT a
                // setting: the answer is remembered, and it is the first piece of the profile.

                width: scrollView.availableWidth
                spacing: 12

                Item {
                    Layout.preferredHeight: 28
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "\uD83D\uDC36" // dog face
                    font.pixelSize: 72
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Musichien")
                    color: "#ffffff"
                    font.pixelSize: 36
                    font.bold: true
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Ton oreille musicale, un peu chaque jour.")
                    color: "#cbb8e8"
                    font.pixelSize: 17
                    wrapMode: Text.WordWrap
                }

                Item {
                    Layout.preferredHeight: 8
                }

                // The list comes from the view model, exactly as the answer grid does, so that a level added
                // to the domain appears here without a line of QML changing.
                // -----------------------------------------------------------------------------------------
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    horizontalAlignment: Text.AlignHCenter
                    color: "#cbb8e8"
                    font.pixelSize: 15
                    text: ExerciseController.hasChosenLevel ? qsTr("Ton niveau") : qsTr("Pour commencer : tu en es où ?")
                }

                Flow {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: mainWindow.buttonWidth
                    spacing: 8

                    Repeater {
                        model: ExerciseController.playerLevels

                        delegate: Button {
                            required property var modelData

                            text: modelData.name
                            // The chosen one stays marked, so that the screen never leaves any doubt about the
                            // level the next session will use.
                            highlighted: ExerciseController.playerLevel === modelData.index
                            onClicked: ExerciseController.choosePlayerLevel(modelData.index)
                        }

                    }

                }

                Item {
                    Layout.preferredHeight: 6
                }

                // The way into the loop. It sits above the bench on purpose: the bench is a tool for
                // building the project, and playing is what the application is FOR.
                Button {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: mainWindow.buttonWidth
                    height: 58
                    highlighted: true
                    text: qsTr("Jouer")
                    onClicked: ExerciseController.startSession()
                }

                Item {
                    Layout.preferredHeight: 6
                }

                Button {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: mainWindow.buttonWidth
                    height: 52
                    text: qsTr("Écouter une note")
                    onClicked: mainWindow.playAndShowFeedback(function() {
                        IntervalController.playSingleNote();
                    })
                }

                Button {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: mainWindow.buttonWidth
                    height: 52
                    text: qsTr("Quinte, deux notes")
                    onClicked: mainWindow.playInterval(7)
                }

                Button {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: mainWindow.buttonWidth
                    height: 52
                    text: qsTr("Quinte, en accord")
                    onClicked: {
                        mainWindow.chooseListeningMode(true);
                        mainWindow.playInterval(7);
                    }
                }

                Item {
                    Layout.preferredHeight: 12
                }

                // Everything below this line is the bench itself.
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: mainWindow.buttonWidth
                    height: 1
                    color: "#4a3170"
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 8
                    text: qsTr("Tous les intervalles, jusqu'à la quinzième")
                    color: "#ffffff"
                    font.pixelSize: 17
                    font.bold: true
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Touche un intervalle : le nom qui s'affiche vient du domaine, jamais de cet écran.")
                    color: "#cbb8e8"
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                }

                // A Flow rather than a Grid: the buttons wrap to whatever width the phone gives them,
                // which is precisely what a grid with a fixed number of columns fails to do.
                Flow {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: Math.min(scrollView.availableWidth - 24, 360)
                    Layout.topMargin: 8
                    spacing: 6

                    Repeater {
                        // The model is the domain's own list, handed over as plain data. Adding an
                        // interval to the domain is what makes a button appear here.
                        model: IntervalController.supportedIntervals

                        delegate: Button {
                            required property var modelData
                            // The lit button is the one that was really HEARD, not the one that was
                            // last tapped: should the playable range ever clamp a note, the screen
                            // would follow the sound rather than the finger.
                            readonly property bool wasHeard: mainWindow.hasHeardInterval && modelData.semitones === mainWindow.heardInterval.semitones

                            width: 62
                            height: 46
                            // The Material style pads a button by 24 dp on each side, which leaves
                            // 14 dp for the text of a 62 dp button and elides even 'P1' to an
                            // ellipsis. These buttons hold two or three characters: they do not need
                            // the padding, they need the room.
                            leftPadding: 6
                            rightPadding: 6
                            text: modelData.identifier
                            font.pixelSize: 15
                            highlighted: wasHeard
                            onClicked: mainWindow.playInterval(modelData.semitones)
                        }

                    }

                }

                Item {
                    Layout.preferredHeight: 8
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 0

                    Switch {
                        id: harmonicSwitch

                        // The switch is the single place this screen decides how to listen, and
                        // chooseListeningMode() is the only way in: every button goes through it.
                        // A plain two way binding is silently destroyed by the first tap of the user,
                        // and a Binding with a restore mode was measured on the device without
                        // pushing the change through either. Two explicit lines are worth more here
                        // than a subtlety nobody can verify.
                        checked: IntervalController.harmonicPlayback
                        onToggled: IntervalController.harmonicPlayback = checked
                    }

                    Text {
                        text: qsTr("En accord : les deux notes ensemble")
                        color: "#cbb8e8"
                        font.pixelSize: 14
                    }

                }

                Item {
                    Layout.preferredHeight: 10
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20
                    horizontalAlignment: Text.AlignHCenter
                    color: "#ffffff"
                    font.pixelSize: 22
                    font.bold: true
                    wrapMode: Text.WordWrap
                    visible: mainWindow.feedbackVisible && mainWindow.hasHeardInterval
                    text: mainWindow.hasHeardInterval ? qsTr("%1 (%2)").arg(mainWindow.heardInterval.name).arg(mainWindow.heardInterval.identifier) : ""
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20
                    horizontalAlignment: Text.AlignHCenter
                    color: "#cbb8e8"
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                    visible: mainWindow.feedbackVisible && mainWindow.hasHeardInterval
                    text: mainWindow.hasHeardInterval ? qsTr("classe %1 · %2 · numéro %3 · %4").arg(mainWindow.heardInterval.intervalClass).arg(mainWindow.octaveSpanLabel(mainWindow.heardInterval.octaveSpan)).arg(mainWindow.heardInterval.number).arg(mainWindow.heardInterval.isCompound ? qsTr("composé") : qsTr("simple")) : ""
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20
                    horizontalAlignment: Text.AlignHCenter
                    color: "#cbb8e8"
                    font.pixelSize: 15
                    wrapMode: Text.WordWrap
                    visible: mainWindow.feedbackVisible && !mainWindow.hasHeardInterval
                    text: qsTr("Écoute bien, puis nomme ce que tu entends.")
                }

                Item {
                    Layout.preferredHeight: 16
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    color: "#6f5b93"
                    font.pixelSize: 13
                    text: qsTr("version %1").arg(Qt.application.version)
                }

                Item {
                    Layout.preferredHeight: 24
                }

            }

        }

        // The loop itself, and the only thing the player ever sees of it: the bench below is a tool for
        // building the project, this is the game.
        ExerciseScreen {
            id: exerciseScreen

            anchors.fill: parent
            visible: ExerciseController.running
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

    // Hides the explanatory text after a few seconds: enough time to read a name and a number, short
    // enough that the screen does not stay cluttered.
    Timer {
        id: feedbackTimer

        interval: 6000
        onTriggered: mainWindow.feedbackVisible = false
    }

}
