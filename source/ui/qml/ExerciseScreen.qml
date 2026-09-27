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

// The view model, registered as a singleton by main(). Without this line the whole screen is dead:
// every reference to ExerciseController below would be undefined, and QML would only say so at run
// time.
import Musichien
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Item {
    // The colour of an interval comes from its CLASS - the colour inside the octave the domain already
    // names that way. Two intervals of the same class therefore share a hue, which is the whole point:
    // a minor tenth IS a minor third, heard one octave higher.
    // How long the verdict stays on screen before the next question arrives by itself.
    // TWO values rather than one, because the two verdicts are not the same length: a success is read in
    // a glance - and is HEARD as a chord, twice as short - while a mistake carries a longer sentence
    // ("c'était…, tu as répondu…"). Roger found the single 1,9 s pause too long, and he was right: it
    // was tuned for the slowest case and applied to the fastest.
    // Everything that moves is gathered here, because the whole screen shifts sideways during the shake.

    id: exerciseScreen

    // There is still no "next" button: a loop the player has to carry forward themselves feels slower
    // than it is. A tap anywhere skips the pause instead - free, and always available.
    readonly property int successPause: 1200
    readonly property int mistakePause: 1800
    // Horizontal offset of the whole screen during the shake. Zero is the resting state, and it is both
    // where the animation starts and where it ends.
    property real shakeOffset: 0
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

        // Read when the timer is RESTARTED, which happens the moment the verdict appears: the pause
        // therefore matches the verdict it is giving time to.
        interval: ExerciseController.wasLastAnswerCorrect ? exerciseScreen.successPause : exerciseScreen.mistakePause
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

        // The screen answers a mistake with its BODY, not only with its text. No sound and no vibration
        // here, and that is a decision rather than an oversight: one of them would need a new
        // responsibility on the port of the domain, the other a system permission. See note 16 of the
        // vault, section 12.3.
        function onWrongAnswerGiven() {
            shakeAnimation.restart();
        }

        target: ExerciseController
    }

    SequentialAnimation {
        id: shakeAnimation

        NumberAnimation {
            target: exerciseScreen
            property: "shakeOffset"
            from: 0
            to: -9
            duration: 40
        }

        NumberAnimation {
            target: exerciseScreen
            property: "shakeOffset"
            from: -9
            to: 9
            duration: 70
        }

        NumberAnimation {
            target: exerciseScreen
            property: "shakeOffset"
            from: 9
            to: -6
            duration: 70
        }

        NumberAnimation {
            target: exerciseScreen
            property: "shakeOffset"
            from: -6
            to: 6
            duration: 60
        }

        NumberAnimation {
            target: exerciseScreen
            property: "shakeOffset"
            from: 6
            to: 0
            duration: 50
        }

    }

    // A tap anywhere skips the pause. The buttons are drawn ABOVE this area, so they keep receiving the
    // taps meant for them: what this catches is a tap on the background, which is exactly the "I have
    // finished reading" gesture.
    MouseArea {
        anchors.fill: parent
        enabled: ExerciseController.isFeedbackVisible
        onClicked: ExerciseController.continueToNextQuestion()
    }

    // An explicit width and height rather than anchors: an item cannot both be anchored and have its x
    // set, and being able to set x is the entire point of this one.
    Item {
        id: content

        width: parent.width
        height: parent.height
        x: exerciseScreen.shakeOffset
        y: 0

        ColumnLayout {
            // -------------------------------------------------------------------------------------------------
            // The verdict
            // -------------------------------------------------------------------------------------------------
            // The choices
            // The memory hint, once the player has made a mistake: a snatch of music they already know.
            // Its place is RESERVED whether or not there is a hint, and that is not cosmetic: a screen that
            // grows and shrinks moves the buttons under the finger of the player, which in a game played by
            // tapping is unforgivable.
            // The grid comes from the session, and it CLOSES IN on every mistake: the wrong answers that
            // were the least plausible step aside, one at a time. The player is helped without asking, and
            // without ever being told that they are being helped.
            // -------------------------------------------------------------------------------------------------
            // Les douze places, disposees EN CERCLE - et pas en colonnes.
            // Ce n'est pas une question de joliesse : c'est la CONDITION pour qu'un jour les notes d'un accord
            // puissent se relier par des traits. Un accord se lira alors comme une FIGURE - un triangle pour un
            // majeur, une autre pour un septieme - et la forme dira quelque chose de la musique, sans un mot.

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
                    // Flat, and READABLE - which it was not.

                    id: quitButton

                    // A flat Material button takes its text colour from the style, and the style knows nothing
                    // about the gradient painted behind it: the result was almost black on a night blue
                    // background, and Roger reported it as such. The colour is therefore stated here, exactly
                    // as the answer buttons state theirs, and a discreet outline gives the tap somewhere to
                    // land.
                    flat: true
                    text: qsTr("Quitter")
                    onClicked: ExerciseController.stopSession()

                    background: Rectangle {
                        radius: 12
                        color: "#2a1a46"
                        border.width: 1
                        border.color: quitButton.down ? "#a58ad0" : "#5c4a80"
                    }

                    contentItem: Text {
                        text: quitButton.text
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        color: quitButton.down ? "#ffffff" : "#cbb8e8"
                        font.pixelSize: 15
                        font.bold: true
                    }

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

            // Note where it sits: BELOW the prompt, not in the verdict. A hint is useful while the player is
            // still choosing; arriving with the answer, it would only be a second answer.
            // -------------------------------------------------------------------------------------------------
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 36

                Text {
                    anchors.centerIn: parent
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: "#e8dcff"
                    font.pixelSize: 16
                    // Empty text is what "nothing to show" means, whichever of its reasons applies.
                    text: ExerciseController.hintText
                    visible: ExerciseController.hintText !== ""
                }

            }

            // Douze places, trente degres chacune, la premiere a midi : do en haut, puis les quintes dans le sens
            // des aiguilles d'une montre. C'est exactement la disposition d'un vrai cercle des quintes.
            Item {
                id: circleBoard

                readonly property real ringRadius: width * 0.36
                readonly property real slotWidth: width * 0.19
                readonly property real slotHeight: width * 0.19

                Layout.fillWidth: true
                Layout.preferredHeight: width

                Repeater {
                    // UN SEUL Repeater, et PLAT : une entree par bouton, chacune sachant sa place, son rang dans
                    // la case, et le nombre de ses voisines.

                    // C'est la reponse a deux choses a la fois. D'abord une demande de Roger : les composes
                    // restent COLLES a leur simple, parce qu'une seconde majeure et une neuvieme majeure sont la
                    // MEME couleur - elles partagent leur place sur le cercle, et il faut pouvoir designer chacune.
                    // Ensuite une lecon apprise a la dure : un Repeater DANS un Repeater ne produit rien, en
                    // silence. La page se charge, aucun avertissement, et pas un bouton a l'ecran.
                    model: ExerciseController.gridPositions

                    delegate: Item {
                        id: gridButton

                        required property var modelData
                        required property int index
                        // Deux constantes de la pile, ecrites ici parce qu'elles decrivent cet ecran et rien
                        // d'autre : l'ecart entre deux boutons d'une meme case, et la hauteur d'un bouton.
                        readonly property real stackSpacing: 3
                        readonly property int stackSize: (gridButton.modelData.stackSize > 0) ? gridButton.modelData.stackSize : 1
                        readonly property real buttonHeight: (circleBoard.slotHeight - ((gridButton.stackSize - 1) * stackSpacing)) / gridButton.stackSize
                        // Moins quatre-vingt-dix degres, c'est midi : la place zero du cercle est le do, et le do se
                        // met en haut. Le sens des aiguilles d'une montre donne ensuite sol, re, la, mi, si -
                        // l'ordre du cercle, tel qu'il s'enseigne.
                        readonly property real slotAngleRadians: (-90 + 30 * gridButton.modelData.slot) * Math.PI / 180
                        // La pile est CENTREE sur le point du cercle, comme le serait une colonne : une case a un
                        // intervalle, une case a trois et une case vide se ressemblent alors assez pour qu'on lise
                        // la carte d'un seul coup d'oeil.
                        readonly property real stackOffset: (((gridButton.modelData.stackIndex > 0) ? gridButton.modelData.stackIndex : 0) - ((gridButton.stackSize - 1) / 2)) * (buttonHeight + stackSpacing)

                        x: (circleBoard.width / 2) + (circleBoard.ringRadius * Math.cos(slotAngleRadians)) - (width / 2)
                        y: (circleBoard.height / 2) + (circleBoard.ringRadius * Math.sin(slotAngleRadians)) - (height / 2) + stackOffset
                        width: circleBoard.slotWidth
                        height: buttonHeight

                        // Une case vide reste DANS le cercle : meme place, meme taille, un simple anneau. Le joueur
                        // voit donc ou l'intervalle viendra, et sa progression a une forme.
                        Rectangle {
                            anchors.fill: parent
                            radius: width / 2
                            color: "#00000000"
                            border.width: 1
                            border.color: "#3a2a5c"
                            visible: gridButton.modelData.isEmpty
                        }

                        Button {
                            // La police suit la hauteur du bouton : une quinzaine de pixels pour un rond
                            // plein, neuf pour une pastille partagee en trois. Sans cela, le texte du dernier
                            // niveau deborderait de sa case.

                            id: intervalButton

                            anchors.fill: parent
                            visible: !gridButton.modelData.isEmpty
                            text: gridButton.modelData.identifier === undefined ? "uid=" + gridButton.index : gridButton.modelData.identifier
                            highlighted: exerciseScreen.hasAnswered && !gridButton.modelData.isEmpty && (exerciseScreen.answeredInterval.semitones === gridButton.modelData.semitones)
                            onClicked: ExerciseController.answer(gridButton.modelData.semitones)
                            scale: intervalButton.down ? 0.9 : 1

                            Behavior on scale {
                                NumberAnimation {
                                    duration: 90
                                }

                            }

                            background: Rectangle {
                                radius: height / 2
                                color: gridButton.modelData.isEmpty ? "#00000000" : exerciseScreen.colourForInterval(gridButton.modelData)
                                border.width: intervalButton.highlighted ? 3 : 0
                                border.color: "#ffffff"
                                opacity: ExerciseController.isAsking ? 1 : 0.72

                                Behavior on opacity {
                                    NumberAnimation {
                                        duration: 180
                                    }

                                }

                            }

                            contentItem: Text {
                                text: intervalButton.text
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                color: "#2b1b47"
                                // La hauteur lue est CELLE DU CALCUL, pas celle du bouton : un bouton qui s'ajuste
                                // a son propre contenu et un contenu qui s'ajuste au bouton font une boucle de
                                // liaison, et QML la signale - sainement - a chaque image.
                                font.pixelSize: Math.max(9, Math.min(14, gridButton.buttonHeight * 0.45)) || 12
                                font.bold: true
                            }

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

}
