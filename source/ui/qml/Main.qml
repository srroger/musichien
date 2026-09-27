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
    // Hides the explanatory text after a few seconds: enough time to read a name and a number, short
    // enough that the screen does not stay cluttered.
    // -------------------------------------------------------------------------------------------------
    // Les instruments joues
    // Roger : "le saxophone a un volume et un timbre vraiment particulier, le jouer de maniere aleatoire
    // surtout la nuit peut etre desagreable". Un instrument qu'on ne veut pas doit donc pouvoir etre ecarte,
    // et le choix doit SURVIVRE au lancement suivant - un reglage qui s'oublie n'est pas un reglage.
    // Un ComboBox aux couleurs du jeu.
    // Pourquoi un composant : le style Material peint la CASE sur fond clair ET la LISTE ouverte sur fond blanc.
    // Regler seulement `background` ne suffit donc pas - un texte clair sur une liste blanche reste illisible. Il
    // faut aussi remplacer `popup`, ce que Qt Quick Controls 2 attend explicitement.

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

    // Combien de temps avant la prochaine notification, en "X h MM". Lu par la page de profil ; l'heure vient du
    // controleur, pas du QML - une heure qui existe deux fois est une heure qui derive.
    function reminderCountdown() {
        var now = new Date();
        var next = new Date(now.getFullYear(), now.getMonth(), now.getDate(), ExerciseController.reminderHour, ExerciseController.reminderMinute, 0);
        if (next <= now)
            next.setDate(next.getDate() + 1);

        var minutes = Math.round((next - now) / 60000);
        var hours = Math.floor(minutes / 60);
        var rest = minutes % 60;
        return hours + " h " + (rest < 10 ? "0" : "") + rest;
    }

    // La couleur de l'accordeur : vert quand c'est juste, jaune quand c'est proche, rouge au-dela. Les SEUILS vivent
    // dans le controleur - c'est un jugement musical - et l'ecran ne fait que les habiller.
    function tuningColor() {
        if (!MicrophoneController.isListening || MicrophoneController.detectedFrequencyHz <= 0)
            return "#8a77ad";

        var state = MicrophoneController.detectedTuningState;
        if (state === 0)
            return "#7ee8a2";

        if (state === 1)
            return "#f2d982";

        return "#f2848e";
    }

    // La couleur du verdict de chant : vert quand l'intervalle entendu est le bon, rouge sinon.
    function singingVerdictColor() {
        var verdict = MicrophoneController.sungVerdict;
        if (verdict === 1)
            return "#7ee8a2";

        if (verdict === 2)
            return "#f2848e";

        return "#8a77ad";
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

    // The colour of the window itself, not of any item inside it. On Android this is what shows through a system bar
    // while the first frame paints, and it must match the top of the gradient rather than flash white.
    color: "#1b1035"
    // Une anecdote par ouverture, comme les ecrans de chargement d'autrefois : un petit texte qui change et qui
    // donne a lire. Tiree au hasard dans le fichier de contenu, jamais ecrite en dur ici.
    Component.onCompleted: ExerciseController.refreshAnecdote()
    width: 420
    height: 820
    minimumWidth: 320
    minimumHeight: 480
    visible: true
    title: qsTr("Musichien")
    // Leaving the screen must never leave an audio stream open: on a phone that is a battery drain,
    // and a bug. The view model was given a method for exactly this call.
    onClosing: IntervalController.stopPlayback()

    // La police musicale, embarquee dans les ressources : les polices Android par defaut n'ont pas la clef de Sol.
    FontLoader {
        id: musicFont

        source: "qrc:/assets/fonts/NotoMusic-Regular.ttf"
    }

    Rectangle {
        // The loop itself, and the only thing the player ever sees of it: the bench below is a tool for
        // building the project, this is the game.

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

                // L'anecdote du chargement, sous le sous-titre : c'est la qu'elle se lit, et fillWidth + WordWrap
                // borne sa largeur a celle de la page - sans cela un texte long pousse les boutons vers la droite.
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    horizontalAlignment: Text.AlignHCenter
                    text: ExerciseController.anecdoteText
                    color: "#8a77ad"
                    font.pixelSize: 13
                    font.italic: true
                    wrapMode: Text.WordWrap
                    visible: ExerciseController.anecdoteText !== ""
                }

                Item {
                    Layout.preferredHeight: 8
                }

                // The list comes from the view model, exactly as the answer grid does, so that a level added
                // to the domain appears here without a line of QML changing.
                // -----------------------------------------------------------------------------------------
                // Le niveau. Un bloc CENTRE, deux par ligne, tous de la meme largeur : quatre options doivent se
                // lire d'un coup d'oeil, et une grille reguliere fait ca mieux qu'un empilement de tailles
                // differentes.
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    horizontalAlignment: Text.AlignHCenter
                    color: "#cbb8e8"
                    font.pixelSize: 15
                    text: ExerciseController.hasChosenLevel ? qsTr("Ton niveau") : qsTr("Pour commencer : tu en es où ?")
                }

                GridLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: mainWindow.buttonWidth
                    columns: 2
                    columnSpacing: 8
                    rowSpacing: 8

                    Repeater {
                        model: ExerciseController.playerLevels

                        delegate: Button {
                            required property var modelData

                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            text: modelData.name
                            font.pixelSize: 14
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

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: mainWindow.buttonWidth
                    spacing: 8

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 46
                        text: qsTr("Mode infini")
                        onClicked: ExerciseController.startInfiniteSession()
                    }

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 46
                        text: qsTr("Chanter")
                        onClicked: {
                            MicrophoneController.startSingingSession();
                            singingDialog.open();
                        }
                    }

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 46
                        text: qsTr("Survie")
                        onClicked: ExerciseController.startSurvivalSession()
                    }

                }

                // Options et Profil sous les modes de jeu : ce sont des portes vers des PAGES, pas des actions de
                // jeu, donc elles se rangent apres, a parts egales.
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: mainWindow.buttonWidth
                    spacing: 8

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 46
                        text: qsTr("Options")
                        onClicked: settingsDialog.open()
                    }

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 46
                        text: qsTr("Profil")
                        onClicked: profileDialog.open()
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

        // The passage is a CROSS FADE rather than a switch, and it is short: Material motion asks for a change that
        // is felt without being watched - 220 ms is the length of a breath, and the eye reads the arrival instead of
        // the cut. The screen is only invisible once the fade is over, so it never eats a tap.
        ExerciseScreen {
            id: exerciseScreen

            anchors.fill: parent
            visible: opacity > 0
            opacity: ExerciseController.running ? 1 : 0

            Behavior on opacity {
                NumberAnimation {
                    duration: 220
                    easing.type: Easing.OutCubic
                }

            }

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

    // Le dernier coche ne peut pas etre decoche : sans instrument, il n'y a plus de jeu.
    // -------------------------------------------------------------------------------------------------
    Dialog {
        id: settingsDialog

        anchors.centerIn: parent
        width: Math.min(mainWindow.width * 0.9, 420)
        // La popup s'adapte a son contenu, et se borne seulement quand il ne tient pas : plus d'espace vide en bas,
        // et le defilement prend le relais quand il y a trop a montrer.
        height: Math.min(mainWindow.height * 0.9, settingsColumn.implicitHeight + 32)
        modal: true
        padding: 16

        // Un fond sombre, et pas la feuille blanche du systeme : cette page fait partie du jeu, et le
        // blanc de l’application systeme jurait au milieu du bleu nuit.
        background: Rectangle {
            color: "#241442"
            radius: 14
            border.width: 1
            border.color: "#5c4a80"
        }

        contentItem: ScrollView {
            id: settingsScroll

            clip: true
            ScrollBar.vertical.policy: ScrollBar.AsNeeded

            ColumnLayout {
                id: settingsColumn

                width: settingsScroll.availableWidth
                spacing: 3

                Text {
                    Layout.fillWidth: true
                    color: "#cbb8e8"
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                    text: qsTr("Le tirage se fait au hasard parmi les instruments coches.")
                }

                Repeater {
                    model: ExerciseController.instruments

                    delegate: CheckBox {
                        required property var modelData

                        Layout.fillWidth: true
                        // Material reserve une cible tactile de 48 dp : dans une LIGNE de liste, c'est deux fois trop.
                        // Un Layout n'obéit qu'a Layout.preferredHeight, jamais a `height`.
                        Layout.preferredHeight: 34
                        text: modelData.name
                        checked: modelData.enabled
                        onClicked: ExerciseController.setInstrumentEnabled(modelData.index, checked)

                        // Meme defaut que le nom du profil : le style Material ecrit noir sur fond sombre. Le leftPadding
                        // remet le texte a droite de la case, sinon il se pose par-dessus l'indicateur.
                        contentItem: Text {
                            text: parent.text
                            color: "#e8dcff"
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: parent.indicator.width + parent.spacing
                            font.pixelSize: 13
                        }

                    }

                }

                CheckBox {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    text: qsTr("Un rappel chaque jour")
                    checked: ExerciseController.dailyReminderEnabled
                    onClicked: ExerciseController.setDailyReminderEnabled(checked)

                    contentItem: Text {
                        text: parent.text
                        color: "#e8dcff"
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: parent.indicator.width + parent.spacing
                        font.pixelSize: 13
                    }

                }

                // Le compte a rebours, juste sous la case qui l'active : impossible de le manquer, et c'est la que le
                // joueur le cherche.
                Text {
                    Layout.fillWidth: true
                    color: "#8ef2b0"
                    font.pixelSize: 13
                    visible: ExerciseController.dailyReminderEnabled
                    text: qsTr("Prochaine oreille : %1").arg(mainWindow.reminderCountdown())
                }

                // L'accordage. Le tempere egal est la reference, et il reste le defaut : c'est ce sur quoi la musique
                // autour de nous est construite. Les anciens ne prennent leur sens qu'autour d'une tonique, et le
                // texte le dit plutot que de laisser croire a un simple bouton de plus.
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 10
                    spacing: 6

                    Text {
                        Layout.fillWidth: true
                        color: "#e8dcff"
                        font.pixelSize: 14
                        font.bold: true
                        text: qsTr("Accordage")
                    }

                    DarkComboBox {
                        model: ExerciseController.temperaments
                        currentIndex: ExerciseController.temperament
                        onActivated: ExerciseController.setTemperament(index)
                    }

                    Text {
                        Layout.fillWidth: true
                        color: "#8a77ad"
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        text: qsTr("Le tempéré est la référence. Les autres sonnent plus juste par endroits, et faux ailleurs.")
                    }

                    // La note de reference : sans elle, un accordage non egal ne veut rien dire. Elle n'apparait
                    // donc que quand l'accordage en a besoin.
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: 4
                        spacing: 6
                        visible: ExerciseController.temperament !== 0

                        Text {
                            Layout.fillWidth: true
                            color: "#e8dcff"
                            font.pixelSize: 13
                            text: qsTr("Note de référence")
                        }

                        DarkComboBox {
                            model: ExerciseController.tuningRoots
                            currentIndex: ExerciseController.tuningRoot
                            onActivated: ExerciseController.setTuningRoot(index)
                        }

                    }

                    // Le diapason. 440 par defaut, mais beaucoup d'instruments a vent sont construits un peu plus
                    // haut pour sonner plus brillant : le regler, c'est accorder l'accordeur sur eux.
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: 4
                        spacing: 6

                        Text {
                            Layout.fillWidth: true
                            color: "#e8dcff"
                            font.pixelSize: 13
                            text: qsTr("La de référence (diapason)")
                        }

                        SpinBox {
                            Layout.preferredWidth: 150
                            Layout.preferredHeight: 32
                            Layout.alignment: Qt.AlignLeft
                            from: 400
                            to: 480
                            stepSize: 1
                            editable: true
                            value: ExerciseController.referencePitch
                            onValueModified: ExerciseController.setReferencePitch(value)

                            contentItem: TextInput {
                                text: parent.textFromValue(parent.value, parent.locale)
                                color: "#ffffff"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                font.pixelSize: 15
                                validator: parent.validator
                                inputMethodHints: Qt.ImhFormattedNumbersOnly
                                readOnly: !parent.editable
                            }

                            background: Rectangle {
                                color: "#1b1035"
                                radius: 4
                                border.width: 1
                                border.color: "#5c4a80"
                            }

                        }

                    }

                }

                // Le micro : choisir le peripherique et le tester en direct. La boule monte et descend sur une portee
                // miniature au rythme de la voix - c'est l'affichage qu'aura la question chantee, expose ici d'abord.
                Rectangle {
                    id: microphonePanel

                    Layout.fillWidth: true
                    Layout.topMargin: 10
                    // Un Rectangle qui ne contient qu'un layout ancre n'a AUCUNE hauteur propre : ses enfants se
                    // posaient les uns sur les autres. La hauteur vient donc du contenu, explicitement.
                    Layout.preferredHeight: microphoneColumn.implicitHeight + 24
                    color: "#2a1a46"
                    radius: 8
                    border.width: 1
                    border.color: "#5c4a80"

                    ColumnLayout {
                        // --- Chanter un intervalle -----------------------------------------------------------

                        id: microphoneColumn

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 12
                        spacing: 10

                        Text {
                            Layout.fillWidth: true
                            color: "#e8dcff"
                            font.pixelSize: 14
                            font.bold: true
                            text: qsTr("Le micro")
                        }

                        DarkComboBox {
                            model: MicrophoneController.inputDeviceNames
                            currentIndex: MicrophoneController.currentDeviceIndex
                            onActivated: MicrophoneController.selectDevice(index)
                        }

                        // La portee miniature : la boule suit la hauteur chantee. Le composant StaffBall porte le style.
                        StaffBall {
                        }

                        // La note la plus proche, l'ecart en cents, et la couleur : c'est un ACCORDEUR, et il sert
                        // aussi bien a verifier une guitare qu'a voir si la voix est juste.
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Text {
                                Layout.fillWidth: true
                                color: mainWindow.tuningColor()
                                font.pixelSize: 16
                                font.bold: true
                                text: MicrophoneController.detectedNoteLabel
                            }

                            Text {
                                Layout.alignment: Qt.AlignRight
                                color: mainWindow.tuningColor()
                                font.pixelSize: 15
                                visible: MicrophoneController.detectedFrequencyHz > 0
                                text: {
                                    var cents = Math.round(MicrophoneController.detectedCents);
                                    if (cents === 0)
                                        return qsTr("juste");

                                    return (cents > 0 ? "+" : "") + cents + qsTr(" cents");
                                }
                            }

                        }

                        Button {
                            Layout.alignment: Qt.AlignRight
                            text: MicrophoneController.isListening ? qsTr("Arrêter") : qsTr("Tester le micro")
                            onClicked: MicrophoneController.isListening ? MicrophoneController.stopTest() : MicrophoneController.startTest()
                        }

                    }

                }

                Button {
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("Fermer")
                    onClicked: settingsDialog.close()
                }

            }

        }

    }

    // L'exercice de chant : une petite serie d'intervalles a chanter, jugee par le detecteur. Le bouton "Ecouter"
    // est le niveau debutant (on entend la cible), ne pas l'ecouter est le niveau avance (il ne reste que le nom).
    Dialog {
        id: singingDialog

        anchors.centerIn: parent
        width: Math.min(mainWindow.width * 0.9, 420)
        height: Math.min(mainWindow.height * 0.9, singingColumn.implicitHeight + 32)
        modal: true
        padding: 16

        background: Rectangle {
            color: "#241442"
            radius: 14
            border.width: 1
            border.color: "#5c4a80"
        }

        contentItem: ScrollView {
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AsNeeded

            ColumnLayout {
                id: singingColumn

                width: parent.width
                spacing: 10

                Text {
                    Layout.fillWidth: true
                    color: "#ffffff"
                    font.pixelSize: 20
                    font.bold: true
                    text: qsTr("Chanter")
                }

                Text {
                    Layout.fillWidth: true
                    color: "#cbb8e8"
                    font.pixelSize: 14
                    text: qsTr("Question %1 / %2 · %3 juste(s)").arg(MicrophoneController.singingQuestionIndex).arg(MicrophoneController.singingTotalQuestions).arg(MicrophoneController.singingCorrectCount)
                }

                Text {
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    horizontalAlignment: Text.AlignHCenter
                    color: "#ffffff"
                    font.pixelSize: 20
                    font.bold: true
                    wrapMode: Text.WordWrap
                    text: MicrophoneController.singingTargetLabel
                }

                // La boule sur la portee, pendant que le joueur chante.
                StaffBall {
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Button {
                        Layout.fillWidth: true
                        text: qsTr("Écouter")
                        onClicked: MicrophoneController.playSingingTarget()
                    }

                    Button {
                        Layout.fillWidth: true
                        highlighted: MicrophoneController.isSingingCaptureActive
                        text: MicrophoneController.isSingingCaptureActive ? qsTr("J'écoute…") : qsTr("Je chante")
                        onClicked: MicrophoneController.isSingingCaptureActive ? MicrophoneController.stopSingingCapture() : MicrophoneController.startSingingCapture()
                    }

                }

                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    color: mainWindow.singingVerdictColor()
                    font.pixelSize: 18
                    font.bold: true
                    visible: MicrophoneController.hasSungInterval
                    text: MicrophoneController.sungVerdict === 1 ? qsTr("Juste !") : qsTr("Raté — entendu : %1 demi-tons").arg(MicrophoneController.sungSemitones)
                }

                Button {
                    Layout.fillWidth: true
                    text: MicrophoneController.singingSessionOver ? qsTr("Recommencer") : qsTr("Suivant")
                    onClicked: MicrophoneController.singingSessionOver ? MicrophoneController.startSingingSession() : MicrophoneController.newSingingQuestion()
                }

                Button {
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("Fermer")
                    onClicked: singingDialog.close()
                }

            }

        }

    }

    // La page du joueur. Le personnage et les statistiques sont UNE SEULE page : un profil n'est pas un reglage, et
    // des statistiques ne sont pas une personne - c'est la meme chose, regardee sous le nom qu'on lui a donne.
    Dialog {
        id: profileDialog

        anchors.centerIn: parent
        width: Math.min(mainWindow.width * 0.9, 420)
        modal: true
        padding: 16

        background: Rectangle {
            color: "#241442"
            radius: 14
            border.width: 1
            border.color: "#5c4a80"
        }

        contentItem: ColumnLayout {
            spacing: 10

            Text {
                Layout.fillWidth: true
                color: "#cbb8e8"
                font.pixelSize: 16
                text: qsTr("Ton profil")
            }

            TextField {
                Layout.fillWidth: true
                placeholderText: qsTr("Ton nom…")
                text: ExerciseController.playerName
                onEditingFinished: ExerciseController.setPlayerName(text)
                // Le style Material ne connait pas le bleu nuit derriere lui : le texte restait noir sur sombre.
                // La couleur est donc dite ici, comme pour les autres boutons de l'application.
                color: "#ffffff"
                placeholderTextColor: "#7a6a9e"

                background: Rectangle {
                    color: "#2a1a46"
                    radius: 8
                    border.width: 1
                    border.color: "#5c4a80"
                }

            }

            Text {
                Layout.fillWidth: true
                color: "#ffffff"
                font.pixelSize: 16
                font.bold: true
                text: qsTr("%1 XP").arg(ExerciseController.totalExperience)
            }

            Text {
                Layout.fillWidth: true
                color: "#cbb8e8"
                font.pixelSize: 14
                text: qsTr("%1 sessions · %2 étoiles").arg(ExerciseController.sessionCount).arg(ExerciseController.starCount)
            }

            Text {
                Layout.fillWidth: true
                color: "#8ef2b0"
                font.pixelSize: 13
                visible: ExerciseController.dailyReminderEnabled
                text: qsTr("Prochaine oreille : %1").arg(mainWindow.reminderCountdown())
            }

            Button {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Tester la notification")
                onClicked: ExerciseController.testReminder()
            }

            Button {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Fermer")
                onClicked: profileDialog.close()
            }

        }

    }

    Timer {
        id: feedbackTimer

        interval: 6000
        onTriggered: mainWindow.feedbackVisible = false
    }

    // Un composant inline plutot qu'un fichier : il n'est utile qu'ici, et il evite de dupliquer trois fois le meme
    // style pour les trois listes de la page.
    component DarkComboBox: ComboBox {
        id: combo

        Layout.fillWidth: true

        contentItem: Text {
            text: combo.displayText
            color: "#ffffff"
            verticalAlignment: Text.AlignVCenter
            leftPadding: 10
        }

        delegate: ItemDelegate {
            width: combo.width

            contentItem: Text {
                text: modelData
                color: "#e8dcff"
                verticalAlignment: Text.AlignVCenter
            }

            // La ligne survolee : sans ca, on ne sait pas ce qu'on est en train de choisir.
            background: Rectangle {
                color: combo.highlightedIndex === index ? "#3a1f5c" : "transparent"
            }

        }

        background: Rectangle {
            color: "#1b1035"
            radius: 8
            border.width: 1
            border.color: "#5c4a80"
        }

        popup: Popup {
            y: combo.height - 1
            width: combo.width
            implicitHeight: Math.min(contentItem.implicitHeight, 240)
            padding: 1

            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                currentIndex: combo.highlightedIndex
                // Le modele n'est lu que quand la liste est ouverte : c'est le motif du style d'origine, et il
                // evite de construire les lignes d'une liste que personne ne regarde.
                model: combo.popup.visible ? combo.delegateModel : null

                ScrollIndicator.vertical: ScrollIndicator {
                }

            }

            background: Rectangle {
                color: "#241442"
                radius: 8
                border.width: 1
                border.color: "#5c4a80"
            }

        }

    }

    // La portee miniature et sa boule : la clef de Sol, cinq lignes, et la boule qui suit la hauteur chantee. Un
    // composant parce qu'elle sert a DEUX endroits - l'accordeur et l'exercice de chant - et un seul style evite
    // qu'ils divergent.
    component StaffBall: Item {
        Layout.fillWidth: true
        Layout.preferredHeight: 100

        Text {
            x: 2
            y: -2
            text: "\uD834\uDD1E"
            color: "#cbb8e8"
            font.pixelSize: 40
            font.family: musicFont.name
        }

        Repeater {
            model: 5

            delegate: Rectangle {
                required property int index

                x: 0
                y: parent.height * (0.15 + index * 0.175)
                width: parent.width
                height: 1
                color: "#5c4a80"
            }

        }

        Rectangle {
            width: 18
            height: 18
            radius: 9
            visible: MicrophoneController.detectedFrequencyHz > 0
            color: mainWindow.tuningColor()
            x: parent.width / 2 - width / 2
            y: parent.height * (1 - MicrophoneController.detectedStaffFraction) - height / 2

            // L'inertie : la boule ne saute pas de note en note, elle GLISSE vers la bonne place.
            Behavior on y {
                NumberAnimation {
                    duration: 250
                    easing.type: Easing.OutQuad
                }

            }

        }

    }

}
