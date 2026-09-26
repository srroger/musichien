// =====================================================================================================================
// Musichien - exercise screen
// The loop, on screen: listen, answer, read what was heard, move on. This file is the first playable
// version of the game, written to answer ONE question: does playing this feel like anything?
// Conventions applied here (see docs/CODE_CONVENTIONS.md):
//   * JavaScript function parameters prefixed with p_
//   * every displayed string goes through qsTr()
//   * no rule of the game lives in this file: the session, the score, the grid and the names all come
//     from the domain through the view model
// The interval NAMES shown here come from the domain and are language neutral ("Perfect fifth"). A
// translated naming layer is a translation matter, not a model one: the identifier (P5, m3, M10) is the
// stable name, and it is the one a save file would keep.
// =====================================================================================================================

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// The view model, registered as a singleton by main(). Without this line the whole screen is dead:
// every reference to ExerciseController below would be undefined, and QML would only say so at run
// time.
import Musichien

Item {
    // How long the verdict stays on screen before the next question arrives by itself.
    // The colour of an interval comes from its CLASS - the colour inside the octave the domain already
    // names that way. Two intervals of the same class therefore share a hue, which is the whole point:
    // a minor tenth IS a minor third, heard one octave higher.

    id: exerciseScreen

    // There is no "next" button on purpose: a loop the player has to carry forward themselves feels
    // slower than it is, and a session is supposed to last under two minutes.
    readonly property int feedbackDuration: 1900
    // The interval the player chose, when there is one.
    readonly property var answeredInterval: ExerciseController.answeredInterval

    // Both checks, and not just the second one: reading a property of something that does not exist yet
    // is an error in QML, and a screen must never be one binding away from throwing.
    readonly property bool hasAnswered: answeredInterval !== undefined && answeredInterval.identifier !== undefined

    // Deliberately NOT an indication of the answer: a colour says "this button is a fifth", never "this
    // button is the one you are looking for".
    function colourForInterval(p_interval) {
        if (p_interval === undefined || p_interval.intervalClass === undefined)
            return "#9c8bc0";

        return Qt.hsla(p_interval.intervalClass / 12, 0.36, 0.76, 1);
    }

    // What the verdict says. The NAME comes from the domain, the sentence is built here: the domain
    // names intervals, it does not speak French.
    function verdictText() {
        var heard = ExerciseController.heardInterval;
        if (heard.identifier === undefined)
            return "";

        var verdict = qsTr("%1 (%2)").arg(heard.name).arg(heard.identifier);
        if (exerciseScreen.hasAnswered && !ExerciseController.wasLastAnswerCorrect)
            verdict += qsTr(" — tu as répondu %1").arg(exerciseScreen.answeredInterval.identifier);

        return verdict;
    }

    Timer {
        id: nextQuestionTimer

        interval: exerciseScreen.feedbackDuration
        onTriggered: ExerciseController.continueToNextQuestion()
    }

    // The pause belongs to the screen, and the screen is this timer: the domain has no clock, which is
    // what keeps a whole session testable in microseconds.
    Connections {
        function onSessionChanged() {
            if (ExerciseController.isFeedbackVisible)
                nextQuestionTimer.restart();
            else
                nextQuestionTimer.stop();
        }

        target: ExerciseController
    }

    ColumnLayout {
        // -------------------------------------------------------------------------------------------------
        // The verdict
        // -------------------------------------------------------------------------------------------------
        // The choices

        anchors.fill: parent
        anchors.margins: 16
        visible: !ExerciseController.isFinished
        spacing: 12

        // -------------------------------------------------------------------------------------------------
        // The head-up display: where the player is, what it has earned, what it has left
        // -------------------------------------------------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true

            Text {
                text: qsTr("%1 / %2").arg(ExerciseController.questionNumber).arg(ExerciseController.questionCount)
                color: "#cbb8e8"
                font.pixelSize: 15
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                visible: ExerciseController.streak >= 2
                text: qsTr("série ×%1").arg(ExerciseController.streak)
                color: "#ffd479"
                font.pixelSize: 15
                font.bold: true
            }

            Item {
                Layout.preferredWidth: 10
            }

            Text {
                text: qsTr("%1 XP").arg(ExerciseController.experience)
                color: "#ffffff"
                font.pixelSize: 15
                font.bold: true
            }

        }

        RowLayout {
            Layout.fillWidth: true

            Button {
                flat: true
                text: qsTr("Quitter")
                onClicked: ExerciseController.stopSession()
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                // An empty lives property shows a heart count; an unlimited session shows the symbol
                // rather than a number that would have to lie.
                text: ExerciseController.hasUnlimitedLives ? "∞" : "♥".repeat(ExerciseController.lives)
                color: "#ff8fa3"
                font.pixelSize: 20
            }

        }

        // It says what was HEARD, never what was tapped: the two can differ, and when they do, the
        // player has to find out. That is the whole reason the answer displayed is a fact of the domain.
        // -------------------------------------------------------------------------------------------------
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 56

            Text {
                anchors.centerIn: parent
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                visible: ExerciseController.isFeedbackVisible
                text: exerciseScreen.verdictText()
                color: ExerciseController.wasLastAnswerCorrect ? "#8ef2b0" : "#ffd479"
                font.pixelSize: 19
                font.bold: true
            }

            Text {
                anchors.centerIn: parent
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                visible: !ExerciseController.isFeedbackVisible
                text: qsTr("Écoute bien…")
                color: "#cbb8e8"
                font.pixelSize: 17
            }

        }

        // The grid comes from the session, and it CLOSES IN on every mistake: the wrong answers that
        // were the least plausible step aside, one at a time. The player is helped without asking, and
        // without ever being told that they are being helped.
        // -------------------------------------------------------------------------------------------------
        Flow {
            id: choiceFlow

            Layout.fillWidth: true
            spacing: 10

            Repeater {
                model: ExerciseController.choices

                delegate: Button {
                    id: choiceButton

                    required property var modelData

                    width: (choiceFlow.width - choiceFlow.spacing) / 2
                    height: 60
                    text: choiceButton.modelData.identifier
                    // The chosen button keeps a visible mark, so that the verdict can be read against
                    // what was really tapped.
                    highlighted: exerciseScreen.hasAnswered && (exerciseScreen.answeredInterval.semitones === choiceButton.modelData.semitones)
                    onClicked: ExerciseController.answer(choiceButton.modelData.semitones)
                    // A short scale bump on press: the smallest possible acknowledgement that the
                    // finger was heard, before anything else has time to happen.
                    scale: choiceButton.down ? 0.94 : 1

                    Behavior on scale {
                        NumberAnimation {
                            duration: 90
                        }

                    }

                    background: Rectangle {
                        radius: 14
                        color: exerciseScreen.colourForInterval(choiceButton.modelData)
                        border.width: choiceButton.highlighted ? 3 : 0
                        border.color: "#ffffff"
                        // The grid fades a little once the answer is known: the question is over, and
                        // what matters then is the verdict, not the buttons.
                        opacity: ExerciseController.isAsking ? 1 : 0.72

                        Behavior on opacity {
                            NumberAnimation {
                                duration: 180
                            }

                        }

                    }

                    contentItem: Text {
                        text: choiceButton.text
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        color: "#2b1b47"
                        font.pixelSize: 17
                        font.bold: true
                    }

                }

            }

        }

        // -------------------------------------------------------------------------------------------------
        // Listening again, and giving up
        // -------------------------------------------------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Button {
                Layout.fillWidth: true
                height: 52
                text: qsTr("Écouter encore")
                enabled: ExerciseController.isAsking
                onClicked: ExerciseController.replay()
            }

            Button {
                Layout.fillWidth: true
                height: 52
                // Appears only once the player has tried enough. Asking to be told is not a failure, and
                // it is not offered before it is useful either.
                visible: ExerciseController.isHelpAvailable
                highlighted: true
                text: qsTr("Réponse")
                onClicked: ExerciseController.revealAnswer()
            }

        }

    }

    // -----------------------------------------------------------------------------------------------------
    // The end of the session
    // -----------------------------------------------------------------------------------------------------
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        visible: ExerciseController.isFinished
        spacing: 14

        Item {
            Layout.fillHeight: true
        }

        Text {
            id: starText

            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: ExerciseController.starEarned ? "⭐" : "☆"
            font.pixelSize: 64

            // A single short pop when it appears: a reward has to be felt, not only read.
            NumberAnimation on scale {
                running: starText.visible
                from: 0.4
                to: 1
                duration: 450
                easing.type: Easing.OutBack
                loops: 1
            }

        }

        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Session terminée")
            color: "#ffffff"
            font.pixelSize: 24
            font.bold: true
        }

        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            color: "#cbb8e8"
            font.pixelSize: 16
            text: ExerciseController.starEarned ? qsTr("%1 XP · tout reconnu à l'oreille").arg(ExerciseController.experience) : qsTr("%1 XP · la prochaine fois sera meilleure").arg(ExerciseController.experience)
        }

        Item {
            Layout.fillHeight: true
        }

        Button {
            Layout.fillWidth: true
            height: 54
            highlighted: true
            text: qsTr("Rejouer")
            onClicked: ExerciseController.startSession()
        }

        Button {
            Layout.fillWidth: true
            height: 48
            text: qsTr("Retour au banc")
            onClicked: ExerciseController.stopSession()
        }

    }

}
