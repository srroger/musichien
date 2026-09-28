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
    // La pause d'une question de rythme : la duree de la cellule, plus une respiration.

    id: exerciseScreen

    // There is still no "next" button: a loop the player has to carry forward themselves feels slower
    // than it is. A tap anywhere skips the pause instead - free, and always available.
    readonly property int successPause: 1200
    readonly property int mistakePause: 1800
    // Elle vient du CONTROLEUR, donc du domaine et de son tempo : une mesure a 90 bpm dure deux secondes et demie, et
    // aucune constante ecrite ici ne saurait le dire sans mentir le jour ou le tempo change.
    readonly property int rhythmPause: ExerciseController.rhythmCellDurationMs + 600
    // Horizontal offset of the whole screen during the shake. Zero is the resting state, and it is both
    // where the animation starts and where it ends.
    property real shakeOffset: 0
    // The interval the player chose, when there is one.
    readonly property var answeredInterval: ExerciseController.answeredInterval
    // Both checks, and not just the second one: reading a property of something that does not exist yet
    // is an error in QML, and a screen must never be one binding away from throwing.
    readonly property bool hasAnswered: answeredInterval !== undefined && answeredInterval.identifier !== undefined
    // La meme chose pour un accord : ce que le joueur vient de repondre, et s'il y a quelque chose a montrer.
    readonly property var answeredChordData: ExerciseController.answeredChord
    readonly property bool hasAnsweredChord: answeredChordData !== undefined && answeredChordData.name !== undefined

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
        // Une question de rythme ne se raconte pas avec des noms d'intervalles : elle se compte en frappes. Et c'est
        // le NOMBRE qui parle, jamais un jugement - "tu as manque la deuxieme" serait un conseil que rien ne prouve.
        if (ExerciseController.questionKind === 3) {
            if (ExerciseController.wasLastAnswerCorrect)
                return qsTr("%1 — reproduite !").arg(ExerciseController.rhythmPatternName);

            return qsTr("%1 : %2 frappes sur %3").arg(ExerciseController.rhythmPatternName).arg(ExerciseController.rhythmCoveredOnsets).arg(ExerciseController.rhythmOnsetCount);
        }
        // Un accord se nomme par sa couleur, et se montre par son SYMBOLE : "Minor", et "Cm" - la tonique vient du
        // domaine, et le symbole est deja assemble la-bas.
        if (ExerciseController.questionKind === 4) {
            var chord = ExerciseController.heardChord;
            if (chord.name === undefined)
                return "";

            var chordVerdict = qsTr("%1 (%2)").arg(chord.name).arg(chord.symbol);
            if (exerciseScreen.hasAnsweredChord && !ExerciseController.wasLastAnswerCorrect)
                chordVerdict += qsTr(" — tu as répondu %1").arg(ExerciseController.answeredChord.name);

            return chordVerdict;
        }
        var heard = ExerciseController.heardInterval;
        if (heard.identifier === undefined)
            return "";

        var verdict = qsTr("%1 (%2)").arg(heard.name).arg(heard.identifier);
        if (exerciseScreen.hasAnswered && !ExerciseController.wasLastAnswerCorrect)
            verdict += qsTr(" — tu as répondu %1").arg(exerciseScreen.answeredInterval.identifier);

        return verdict;
    }

    Timer {
        // Read when the timer is RESTARTED, which happens the moment the verdict appears: the pause
        // therefore matches the verdict it is giving time to.

        id: nextQuestionTimer

        // Une question de rythme a la sienne, et elle est plus longue : le feedback y est la CELLULE elle-meme, qui
        // dure une mesure entiere. Couper avant la fin couperait le son qui vient d'etre donne en reponse.
        interval: ExerciseController.questionKind === 3 ? exerciseScreen.rhythmPause : (ExerciseController.wasLastAnswerCorrect ? exerciseScreen.successPause : exerciseScreen.mistakePause)
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
            // -------------------------------------------------------------------------------------------------
            // La question de rythme
            // -------------------------------------------------------------------------------------------------
            // La question d'accord
            // Un accord se reconnait a sa COULEUR : les notes sont plaquees, et l'ecran n'offre qu'une poignee de noms.
            // Pas de cercle des quintes ici - une qualite d'accord n'a pas d'angle sur un cercle - et l'ordre des
            // boutons suit l'ordre d'apprentissage, donc il ne bouge jamais d'une question a l'autre.

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
                    text: ExerciseController.questionCount < 0 ? qsTr("%1 / ∞").arg(ExerciseController.questionNumber) : qsTr("%1 / %2").arg(ExerciseController.questionNumber).arg(ExerciseController.questionCount)
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

                // Le rang de la serie, facon Devil May Cry : un grade qui monte avec l'enchainement, affiche en
                // grand et en couleur. Il ne dit rien d'autre que "tu enchaines", et c'est exactement ce qu'il doit
                // dire.
                Text {
                    visible: ExerciseController.streak >= 2
                    text: ExerciseController.rankLabel
                    color: "#ff5e8a"
                    font.pixelSize: 22
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
                    // Le "Écoute bien…" est le prompt des questions d'OREILLE. Sur une question de rythme, c'est la
                    // zone rythmique qui dit ou en est la boucle, et le meme mot y serait faux la moitie du temps.
                    visible: !ExerciseController.isFeedbackVisible && ExerciseController.questionKind !== 3 && ExerciseController.questionKind !== 4
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

            // -------------------------------------------------------------------------------------------------
            // Le mode guide : "ca monte ou ca descend ?" Quand la question le demande, le cercle s'efface et deux
            // boutons prennent sa place. Une question plus petite, mais c'est la premiere qu'un debutant repond.
            // -------------------------------------------------------------------------------------------------
            RowLayout {
                Layout.fillWidth: true
                visible: ExerciseController.questionKind === 1
                spacing: 10

                Button {
                    Layout.fillWidth: true
                    height: 68
                    text: qsTr("↑ Monte")
                    onClicked: ExerciseController.answerDirection(0)
                }

                Button {
                    Layout.fillWidth: true
                    height: 68
                    text: qsTr("↓ Descend")
                    onClicked: ExerciseController.answerDirection(1)
                }

            }

            // Le chant : quand la question le demande, la grille s'efface et il ne reste qu'a chanter. La portee, la
            // boule et la barre de stabilite sont le composant partage avec l'accordeur ; seule la cible change.
            ColumnLayout {
                Layout.fillWidth: true
                visible: ExerciseController.questionKind === 2
                spacing: 10

                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    color: "#ffffff"
                    font.pixelSize: 22
                    font.bold: true
                    wrapMode: Text.WordWrap
                    text: qsTr("Chante : %1").arg(MicrophoneController.singingTargetLabel)
                }

                StaffBall {
                    Layout.preferredHeight: 120
                }

                // La barre de stabilite : elle se remplit tant que la note est tenue, puis repart pour la deuxieme.
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 8
                    radius: 4
                    color: "#1b1035"

                    Rectangle {
                        height: 8
                        radius: 4
                        color: "#8ef2b0"
                        width: parent.width * MicrophoneController.sungStability
                    }

                }

                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    color: "#cbb8e8"
                    font.pixelSize: 13
                    visible: !MicrophoneController.hasSungInterval
                    text: MicrophoneController.hasFirstNote ? qsTr("Première note tenue — maintenant la deuxième") : qsTr("Tiens la première note…")
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Button {
                        Layout.fillWidth: true
                        text: qsTr("Écouter")
                        onClicked: ExerciseController.listenToTarget()
                    }

                    Button {
                        Layout.fillWidth: true
                        highlighted: MicrophoneController.isSingingCaptureActive
                        text: MicrophoneController.isSingingCaptureActive ? qsTr("J'écoute…") : qsTr("Je chante")
                        onClicked: MicrophoneController.isSingingCaptureActive ? MicrophoneController.stopSingingCapture() : MicrophoneController.startSingingCapture()
                    }

                }

                Connections {
                    function onSungIntervalChanged() {
                        if (ExerciseController.questionKind === 2 && MicrophoneController.hasSungInterval)
                            ExerciseController.answerSung(MicrophoneController.sungVerdict === 1);

                    }

                    target: MicrophoneController
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
                visible: ExerciseController.questionKind === 0

                Repeater {
                    // UN SEUL Repeater, et PLAT : une entree par bouton, chacune sachant sa place, son rang dans
                    // Un seul Repeater, PLAT : une entree par bouton. Chaque bouton porte sa place (l'angle), son octave (la
                    // couche), et son identifiant. Le cercle des quintes se dessine comme des couches d'electrons - voir
                    // la geometrie du delegue juste en dessous.
                    // COUCHES CONCENTRIQUES, comme les electrons d'un atome - l'image de Roger, enfin comprise.
                    // Chaque CLASSE d'intervalle a son ANGLE sur le cercle, et chaque OCTAVE a sa COUCHE : un
                    // rayon plus petit. Le simple vit sur la couche externe, la neuvieme sur la couche interne,
                    // au MEME angle que la seconde, juste plus proche du centre ; la quinzieme sur une troisieme
                    // couche, plus proche encore.

                    // Un Repeater DANS un Repeater ne produit rien, en silence : la page se charge, aucun
                    // avertissement, et pas un bouton a l'ecran. C'est une lecon apprise a la dure.
                    model: ExerciseController.gridPositions

                    delegate: Item {
                        id: gridButton

                        required property var modelData
                        required property int index
                        // Ainsi deux intervalles d'une meme classe ne peuvent jamais se chevaucher : ils sont l'un
                        // derriere l'autre, sur le meme rayon. La distance se lit radialement - c'est exactement ce
                        // que les cercles imbriques et les piles de pastilles ne savaient pas faire.
                        readonly property real octaveSpan: (gridButton.modelData.octaveSpan > 0) ? gridButton.modelData.octaveSpan : 0
                        // Le facteur de la couche : le simple est a 1, chaque octave au-dessus est 0,6 fois plus
                        // proche du centre. La TAILLE suit le meme facteur, pour que les cases d'une couche interne
                        // ne se touchent pas entre elles - la largeur d'arc disponible diminue avec le rayon.
                        readonly property real layerFactor: Math.pow(0.55, octaveSpan)
                        readonly property real buttonSize: circleBoard.slotWidth * layerFactor
                        // Moins quatre-vingt-dix degres, c'est midi : la place zero du cercle est le do, et le do se
                        // met en haut. Le sens des aiguilles d'une montre donne ensuite sol, re, la, mi, si -
                        // l'ordre du cercle, tel qu'il s'enseigne. Le MEME angle pour toutes les couches.
                        readonly property real slotAngleRadians: (-90 + (30 * gridButton.modelData.slot)) * Math.PI / 180
                        // Le rayon de la couche ou vit cet intervalle.
                        readonly property real layerRadius: circleBoard.ringRadius * layerFactor

                        x: (circleBoard.width / 2) + (layerRadius * Math.cos(slotAngleRadians)) - (width / 2)
                        y: (circleBoard.height / 2) + (layerRadius * Math.sin(slotAngleRadians)) - (height / 2)
                        width: buttonSize
                        height: buttonSize

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
                            text: gridButton.modelData.isEmpty ? "" : String(gridButton.modelData.identifier)
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
                                // La taille lue est CELLE DU CALCUL, pas celle du bouton : un bouton qui s'ajuste a
                                // son propre contenu et un contenu qui s'ajuste au bouton font une boucle de liaison,
                                // et QML la signale - sainement - a chaque image. Le Math.max protege le cas ou la
                                // case n'est pas encore mesuree : une taille NaN ne se peint pas du tout.
                                font.pixelSize: Math.max(8, Math.round(gridButton.buttonSize * 0.42))
                                font.bold: true
                            }

                        }

                    }

                }

            }

            // La cellule se dessine, s'ecoute, puis se reproduit au doigt. C'est la "petite partition" du design, et
            // elle sert deux fois : elle annonce ce qu'il faut jouer, et elle dit ou en est la boucle.
            // -------------------------------------------------------------------------------------------------
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: ExerciseController.questionKind === 3
                spacing: 10

                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    color: "#ffffff"
                    font.pixelSize: 20
                    font.bold: true
                    text: qsTr("Reproduis : %1").arg(ExerciseController.rhythmPatternName)
                }

                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    color: "#cbb8e8"
                    font.pixelSize: 14
                    text: qsTr("%1 bpm · %2 temps").arg(ExerciseController.rhythmBpm).arg(ExerciseController.rhythmBeatsPerBar)
                }

                // La mesure : un repere par temps, un trait par frappe de la cellule, et un curseur sur le temps qui
                // sonne. Le trait est plus haut et plus clair quand la frappe est accentuee - c'est ce relief que
                // l'oreille doit retrouver.
                Item {
                    id: rhythmBar

                    readonly property int beatsPerBar: Math.max(1, ExerciseController.rhythmBeatsPerBar)

                    Layout.fillWidth: true
                    Layout.preferredHeight: 96
                    Layout.topMargin: 4

                    Repeater {
                        model: rhythmBar.beatsPerBar

                        delegate: Rectangle {
                            required property int index

                            x: (index / rhythmBar.beatsPerBar) * rhythmBar.width
                            y: 0
                            width: 1
                            height: rhythmBar.height
                            color: index === 0 ? "#5c4a80" : "#3f2d63"
                        }

                    }

                    Repeater {
                        model: ExerciseController.rhythmHits

                        delegate: Rectangle {
                            required property var modelData
                            // La place de la frappe dans la mesure vient du DOMAINE : ce fichier ne compte rien, il
                            // place ce qu'on lui donne.
                            readonly property real hitX: (modelData.beat / rhythmBar.beatsPerBar) * rhythmBar.width

                            x: hitX - width / 2
                            y: modelData.accented ? 10 : 26
                            width: modelData.accented ? 12 : 8
                            height: rhythmBar.height - (modelData.accented ? 20 : 40)
                            radius: width / 2
                            color: modelData.accented ? "#ffd479" : "#a58ad0"
                        }

                    }

                    Rectangle {
                        x: (ExerciseController.rhythmBeatInBar / rhythmBar.beatsPerBar) * rhythmBar.width
                        y: 0
                        width: 3
                        height: rhythmBar.height
                        color: ExerciseController.isRhythmPlaying ? "#8ef2b0" : "#6f5c96"
                    }

                }

                // Ou en est la boucle, et ce que la tentative a donne jusqu'ici. Rien n'est invente ici : le tour vient
                // de l'horloge du controleur, le compte du domaine.
                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    color: ExerciseController.isRhythmPlaying ? "#8ef2b0" : "#cbb8e8"
                    font.pixelSize: 15
                    text: ExerciseController.isRhythmPlaying ? qsTr("À toi !") : qsTr("Écoute…")
                }

                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: 18
                    font.bold: true
                    // Vide tant que rien n'a ete frappe : un "Miss" affiche avant la premiere frappe serait un reproche
                    // que le joueur n'a pas merite.
                    text: ExerciseController.rhythmLastQuality === 2 ? qsTr("Perfect") : (ExerciseController.rhythmLastQuality === 1 ? qsTr("Good") : (ExerciseController.rhythmLastQuality === 0 ? qsTr("Miss") : ""))
                    color: ExerciseController.rhythmLastQuality === 2 ? "#8ef2b0" : (ExerciseController.rhythmLastQuality === 1 ? "#ffd479" : "#ff8fa3")
                }

                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    color: "#8a77ad"
                    font.pixelSize: 13
                    text: qsTr("%1 / %2 frappes").arg(ExerciseController.rhythmCoveredOnsets).arg(ExerciseController.rhythmOnsetCount)
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 90

                    Rectangle {
                        anchors.fill: parent
                        radius: 22
                        color: tapArea.pressed ? "#8ef2b0" : (ExerciseController.isRhythmPlaying ? "#2a1a46" : "#1c1230")
                        border.width: 2
                        border.color: ExerciseController.isRhythmPlaying ? "#8ef2b0" : "#3f2d63"

                        Text {
                            anchors.centerIn: parent
                            color: ExerciseController.isRhythmPlaying ? "#ffffff" : "#6f5c96"
                            font.pixelSize: 26
                            font.bold: true
                            text: ExerciseController.isRhythmPlaying ? qsTr("TAPE") : qsTr("Écoute la cellule…")
                        }

                    }

                    // onPressed, et NON onClicked : "clicked" part au relachement du doigt, donc une centaine de
                    // millisecondes plus tard - un dixieme de temps a 90 bpm, et de quoi transformer un Perfect en
                    // Good. Une frappe part au CONTACT, comme une corde.
                    MouseArea {
                        id: tapArea

                        anchors.fill: parent
                        enabled: ExerciseController.isRhythmPlaying
                        onPressed: ExerciseController.tapRhythm()
                    }

                }

            }

            // Le tout TIENT DANS L'ECRAN : une page de jeu qui se fait scroller cache ses propres reponses, et un
            // joueur de haut niveau peut avoir QUINZE couleurs a l'ecran (les six triades, les trois septiemes, la
            // sixte, le demi-diminu, le diminue 7, le mineur-majeur, add9 et la neuvieme). Trois colonnes et des
            // etiquettes courtes, voila comment quinze reponses tiennent sur un telephone.
            // -------------------------------------------------------------------------------------------------
            GridLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: ExerciseController.isChordQuestion
                // TROIS colonnes quand il y a de quoi les remplir, deux sinon : un debutant qui n'a que majeur et
                // mineur aurait deux petits boutons perdus chacun dans un tiers d'ecran.
                columns: ExerciseController.chordChoices.length >= 3 ? 3 : 2
                rowSpacing: 8
                columnSpacing: 8

                Repeater {
                    model: ExerciseController.chordChoices

                    delegate: Button {
                        required property var modelData
                        // La bonne reponse est mise en avant QUAND ELLE EST CONNUE, jamais avant : un bouton qui se
                        // signalerait tout seul donnerait la reponse.
                        readonly property bool isTheAnswer: ExerciseController.heardChord.quality === modelData.quality

                        Layout.fillWidth: true
                        Layout.preferredHeight: 52
                        Layout.maximumHeight: 64
                        highlighted: isTheAnswer && ExerciseController.isFeedbackVisible
                        enabled: ExerciseController.isAsking
                        // L'ETIQUETTE COURTE - "m7b5", "Maj7", "sus4" - et le nombre de notes a cote : c'est ce qui
                        // separe une triade d'une septieme, et c'est la premiere chose que l'oreille attrape. Le nom
                        // complet ("Half-diminished") revient dans le verdict, la ou il y a la place de l'ecrire :
                        // une reponse coupee en deux n'est pas une reponse.
                        text: qsTr("%1 · %2").arg(modelData.name).arg(modelData.noteCount)
                        // L'index de la qualite voyage tel quel : l'ecran affiche une etiquette, et renvoie l'index de
                        // ce qu'il a affiche. Il ne nomme rien lui-meme.
                        onClicked: ExerciseController.answerChord(modelData.quality)
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
                    // Sur une question de rythme, la question ne se "revele" pas : elle se PASSE. Le meme bouton, le
                    // meme domaine (revealAnswer), et un mot qui dit ce que le joueur fait vraiment.
                    text: ExerciseController.questionKind === 3 ? qsTr("Passer") : qsTr("Réponse")
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

            // L'anecdote de sortie : on quitte sur quelque chose a apprendre, comme on est entre. Bornee en largeur
            // (fillWidth + WordWrap), sinon un texte long pousserait les boutons hors de l'ecran.
            Text {
                Layout.fillWidth: true
                Layout.topMargin: 4
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                color: "#8a77ad"
                font.pixelSize: 13
                font.italic: true
                visible: ExerciseController.anecdoteText !== ""
                text: ExerciseController.anecdoteText
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
