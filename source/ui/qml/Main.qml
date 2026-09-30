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
// ---------------------------------------------------------------------------------------------------------------------
// Pourquoi tant de « Layout.preferredWidth: 0 » et « Layout.minimumWidth: 0 » sur les textes replies
// Parce qu'un Text replie rapporte la largeur de sa PLUS LONGUE LIGNE comme largeur minimale, et que QtQuick.Layouts la
// respecte. Une phrase un peu longue elargissait donc le dialogue qui la contient - jusqu'a 489 points pour une vue de
// 338 sur un telephone - et faisait apparaitre un DEFILEMENT HORIZONTAL dans une page qui n'avait rien a montrer
// de plus a droite. Deux lignes suffisent a l'empecher : la largeur preferee ET la largeur minimale mises a zero, pour
// que le texte se replie au lieu de pousser le mur.
// Les deux vont toujours ENSEMBLE, et c'est le minimum qui compte : sans lui, la largeur preferee seule ne change rien.
// =====================================================================================================================

// The view models of the application, registered as singletons from main().
import Musichien
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    // Une part de question : un titre, une phrase qui dit ce que le reglage fait, et le nombre.
    // =================================================================================================================
    // L'ARBRE DES ACCORDS
    // Une carte ou chaque accord s'obtient en modifiant un intervalle : la forme rappelle les arbres de competences
    // d'un jeu de role, et ca donne un ORDRE aux accords - quinze couleurs, quatorze gestes, une seule racine, au lieu
    // de quinze noms a retenir. C'est la meilleure page pedagogique du jeu.
    // Une anecdote par ouverture, comme les ecrans de chargement d'autrefois : un petit texte qui change et qui
    // donne a lire. Tiree au hasard dans le fichier de contenu, jamais ecrite en dur ici.
    // =================================================================================================================
    // LA PAGE ACCORDEUR
    // L'accordeur vivait dans les reglages, ce qui est la meilleure facon de ne jamais le trouver : c'est un OUTIL, et un
    // outil se prend quand on en a besoin. La page porte les trois choses qui vont ensemble - le micro, les reglages
    // d'accordage, et de quoi comprendre ce qu'on regle.
    // Hides the explanatory text after a few seconds: enough time to read a name and a number, short
    // enough that the screen does not stay cluttered.
    // -------------------------------------------------------------------------------------------------
    // Les instruments joues
    // Un instrument au timbre particulier que le tirage joue au hasard, surtout la nuit, peut etre desagreable a
    // entendre. Un instrument qu'on ne veut pas doit donc pouvoir etre ecarte, et le choix doit SURVIVRE au lancement
    // suivant - un reglage qui s'oublie n'est pas un reglage.
    // Un ComboBox aux couleurs du jeu.
    // Pourquoi un composant : le style Material peint la CASE sur fond clair ET la LISTE ouverte sur fond blanc.
    // Regler seulement `background` ne suffit donc pas - un texte clair sur une liste blanche reste illisible. Il
    // faut aussi remplacer `popup`, ce que Qt Quick Controls 2 attend explicitement.
    // Un champ numerique aux couleurs du jeu.
    // Les couleurs des parts du camembert : un vert pour l'oreille, un bleu pour le sens, un dore pour le chant, un rose
    // pour le rythme, un violet pour les accords, un turquoise pour la couleur des modes et un orange pour leur nom.
    // =================================================================================================================
    // LE CERCLE DES QUINTES
    // =================================================================================================================
    // LE MUSICHIEN QUI S'INVITE
    // Il vit ICI, au niveau de la fenetre et non d'une page : c'est ce qui lui permet de se poser par-dessus n'importe
    // quoi - l'accueil, les reglages, le cercle - sans que chaque page ait a le connaitre. Roger l'a demande comme ca :
    // « ce serait le Musichien qui s'invite pour raconter des trucs ».

    id: mainWindow

    // Le NOMBRE compte : une part prend sa couleur par son RANG, donc une palette plus courte que la liste des genres
    // ferait reapparaitre les premieres couleurs sur les derniers genres - et deux parts de la meme teinte dans un
    // camembert ne disent plus rien.
    readonly property var kindColours: ["#8ef2b0", "#7bb0ff", "#ffd479", "#ff8fb0", "#c9a0ff", "#6fe3d2", "#ffb066"]
    // Width shared by the standalone controls, so that they line up without each repeating the rule.
    readonly property real buttonWidth: Math.min(width * 0.82, 340)
    // What the domain said about the interval heard last, and whether there is anything to say at
    // all. An empty map is what a single note produces, because a single note is not an interval.
    readonly property var heardInterval: IntervalController.lastPlayedInterval
    readonly property bool hasHeardInterval: heardInterval.identifier !== undefined && heardInterval.identifier !== ""
    // Local UI state: it belongs to the screen, not to the domain.
    property bool feedbackVisible: false

    function kindColour(index) {
        return kindColours[index % kindColours.length];
    }

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

    // La demande d'autorisation part un peu APRES le premier affichage : le joueur voit d'abord la page, et la boite
    // d'Android arrive ensuite, sur quelque chose qui existe. Posee pendant la construction de l'ecran, elle
    // apparaitrait sur une fenetre encore vide, ce qui ressemble a un plantage plutot qu'a une question.
    // Le MICRO, lui, n'est PAS ouvert ici : il s'ouvre quand un ecran s'en sert (l'accordeur, les reglages, une
    // question chantee) et se referme quand cet ecran s'en va. Un microphone ouvert pour rien vide la batterie.
    Timer {
        interval: 800
        running: true
        onTriggered: ExerciseController.requestNotificationPermission()
    }

    Rectangle {
        // The loop itself, and the only thing the player ever sees of it: the bench below is a tool for
        // building the project, this is the game.

        anchors.fill: parent

        ScrollView {
            id: scrollView

            anchors.fill: parent
            // PAS de defilement horizontal, jamais : une page qui se decale de cote parce qu'un enfant est trop large est
            // desagreable au doigt, et sur un telephone elle n'a rien a montrer de plus a droite. Le contenu est donc
            // tenu de rentrer dans la largeur - s'il ne rentre pas, c'est le contenu qu'il faut retoucher, pas la vue.
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
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
                // -------------------------------------------------------------------------------------------------
                // Les sept modes, du plus clair au plus sombre
                // La PHRASE du mode : une vraie phrase du contenu, avec les durees que l'oreille de Roger a retenues.
                // Elle vient APRES avoir entendu le mode, et sur le mode allume : c'est le meme geste que la gamme, un
                // cran plus loin. Un bouton par mode en ferait quatorze a l'ecran d'un telephone, pour un choix qui n'a
                // aucune raison d'etre simultane.
                // La ROUE du mode entendu : ses sept notes allumées sur le cercle des quintes, la tonique marquée.

                width: scrollView.availableWidth
                spacing: 12

                Item {
                    Layout.preferredHeight: 28
                }

                // LE CHIEN, l'illustration et non plus un emoji : un emoji ne dit rien de qui parle - c'est le meme
                // chien sur tous les telephones du monde. Celui-ci est le notre, il porte sa guitare et la nuit
                // violette, et c'est exactement l'icone du lanceur - l'application se reconnait d'un ecran a
                // l'autre. Il vient des ressources Qt, comme tout le contenu : un seul chemin, sur le PC et sur le
                // telephone.
                Image {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 164
                    Layout.preferredHeight: 164
                    source: "qrc:/assets/images/shiba-guitar.png"
                    fillMode: Image.PreserveAspectFit
                    smooth: true

                    // Le chien de l'accueil REPOND : appuyer sur lui ouvre la popup avec une AUTRE anecdote. Roger l'a
                    // demande, et c'est ce qui fait du chien un personnage plutot qu'une illustration - il a toujours
                    // quelque chose a raconter, et c'est lui qu'on vient voir.
                    MouseArea {
                        anchors.fill: parent
                        onClicked: ExerciseController.tellAnotherAnecdote()
                    }

                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Musichien")
                    color: "#ffffff"
                    font.pixelSize: 36
                    font.bold: true
                }

                Text {
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
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
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
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

                // Ou en est le joueur, en UNE ligne : c'est la premiere chose qu'un ecran d'accueil doit dire. Ces trois
                // chiffres existaient deja, mais il fallait descendre dans les reglages pour les voir - et un accueil qui
                // ne dit pas ou on en est n'est pas un accueil.
                Text {
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: ExerciseController.totalExperience > 0 ? "#cbb8e8" : "#8a77ad"
                    font.pixelSize: 13
                    text: ExerciseController.totalExperience > 0 ? qsTr("%1 XP · %2 sessions · %3 ⭐").arg(ExerciseController.totalExperience).arg(ExerciseController.sessionCount).arg(ExerciseController.starCount) : qsTr("Ta première partie t'attend.")
                }

                // The way into the loop. It sits above the bench on purpose: the bench is a tool for
                // building the project, and playing is what the application is FOR.
                Button {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: mainWindow.buttonWidth
                    height: 58
                    highlighted: true
                    text: qsTr("▶ Jouer")
                    onClicked: ExerciseController.startSession()
                }

                // Le BILAN : une session dont les questions sont DECIDEES, du plus facile au plus difficile. Il reste
                // disponible a tout moment ; l'ecran le met en avant le week-end, parce que c'est le moment ou l'on a
                // le temps de le prendre.
                Text {
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    visible: ExerciseController.isWeekEnd
                    color: "#ffd479"
                    font.pixelSize: 13
                    text: qsTr("C'est le week-end : l'heure du bilan.")
                }

                Button {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: mainWindow.buttonWidth
                    height: 46
                    // Dore le week-end : la couleur suffit a dire « c'est le moment », et le bouton garde sa place
                    // sous « Jouer » : c'est jouer qui doit rester la porte d'entree.
                    highlighted: ExerciseController.isWeekEnd
                    text: ExerciseController.isWeekEnd ? qsTr("★ Bilan de la semaine") : qsTr("★ Bilan")
                    onClicked: ExerciseController.startReviewSession()
                }

                // Ce que le bilan est, en une phrase : un bouton dont on ne sait pas ce qu'il fait ne se clique pas.
                Text {
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: "#8a77ad"
                    font.pixelSize: 12
                    text: qsTr("Un parcours qui commence par ce que tu réussis, et finit par ce qui te résiste.")
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

                // Les OUTILS et les portes vers des PAGES, sous les modes de jeu. Deux rangees de deux plutot qu'une
                // rangee de quatre : « Metronome » et « Accordeur » ne tiennent pas cote a cote sur la largeur d'un
                // telephone, et un libelle coupe en deux n'est plus un libelle.
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: mainWindow.buttonWidth
                    spacing: 8

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 46
                        text: qsTr("Métronome")
                        onClicked: rhythmDialog.open()
                    }

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 46
                        text: qsTr("Accordeur")
                        onClicked: tunerDialog.open()
                    }

                    // Le cercle des quintes : la CARTE du pilier harmonie, et la seule page qui ne demande rien au
                    // joueur - on y lit, on n'y repond pas.
                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 46
                        text: qsTr("Cercle")
                        onClicked: keyCircleDialog.open()
                    }

                }

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

                // L'ARBRE DES ACCORDS, sur sa propre ligne : c'est une CARTE qu'on consulte, pas un reglage, et elle se
                // trouve sans chercher. Elle merite mieux qu'une quatrieme case dans une rangee de trois.
                Button {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: mainWindow.buttonWidth
                    Layout.preferredHeight: 46
                    text: qsTr("L'arbre des accords")
                    onClicked: chordTreeDialog.open()
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
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
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

                // Le bouton qui tient l'accord six secondes : c'est le temps qu'il faut a l'oreille pour compter les
                // battements entre deux frequences, et donc pour ENTENDRE ce qu'un temperament change. Il rejoue
                // l'intervalle entendu en dernier, les deux notes ensemble.
                Button {
                    Layout.alignment: Qt.AlignHCenter
                    enabled: mainWindow.hasHeardInterval
                    text: qsTr("Tenir 6 s (battements)")
                    onClicked: IntervalController.playSustainedInterval(mainWindow.heardInterval.semitones)
                }

                // Les frequences EXACTES jouees, en hertz, telles que le temperament et le diapason les calculent.
                // C'est ce qu'un accordeur externe doit retrouver - et ce qui prouve, chiffre a l'appui, qu'un
                // changement de temperament est bien descendu jusqu'au son.
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    color: "#8ef2b0"
                    font.pixelSize: 16
                    font.bold: true
                    visible: IntervalController.playedFrequencies !== ""
                    text: IntervalController.playedFrequencies
                }

                Text {
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
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
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
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
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
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

                // Un mode ne s'entend PAS tout seul : il lui faut un CENTRE. Chaque bouton fait donc entendre la gamme
                // montee puis descendue SUR UN BOURDON tenu, et c'est le domaine qui decide quelle quinte tenir et
                // combien de temps. C'est le meme chemin que l'exercice du degrade, donc le meme son.
                // -------------------------------------------------------------------------------------------------
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: mainWindow.buttonWidth
                    height: 1
                    color: "#4a3170"
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 8
                    text: qsTr("Les sept modes, du plus clair au plus obscur")
                    color: "#ffffff"
                    font.pixelSize: 17
                    font.bold: true
                }

                Text {
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Touche un mode : la gamme monte et descend sur un bourdon, pour entendre sa couleur. Les plus clairs sont en haut.")
                    color: "#cbb8e8"
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                }

                Flow {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: Math.min(scrollView.availableWidth - 24, 360)
                    Layout.topMargin: 8
                    spacing: 6

                    Repeater {
                        // La liste vient du domaine, exactement comme celle des intervalles : ajouter un mode au
                        // domaine est ce qui fait apparaitre un bouton ici.
                        model: ModeController.modes

                        delegate: Button {
                            required property var modelData
                            // Le bouton allume est celui qui a vraiment ete ENTENDU, et non celui qui a ete touche.
                            readonly property bool wasHeard: ModeController.lastPlayedMode.index !== undefined && modelData.index === ModeController.lastPlayedMode.index
                            // La CLARTE peint le bouton : plus le mode est clair, plus il est lumineux. C'est l'axe du
                            // cercle des quintes, et il prepare l'exercice du degrade - ou les cases s'assombrissent a
                            // mesure qu'on eteint des notes.
                            readonly property bool isBright: modelData.brightness > 0.5

                            width: 104
                            height: 46
                            leftPadding: 6
                            rightPadding: 6
                            text: modelData.name
                            font.pixelSize: 14
                            highlighted: wasHeard
                            Material.background: Qt.rgba(0.18 + (0.62 * modelData.brightness), 0.14 + (0.56 * modelData.brightness), 0.3 + (0.48 * modelData.brightness), 1)
                            Material.foreground: isBright ? "#1d1033" : "#ffffff"
                            onClicked: ModeController.playMode(modelData.index)
                        }

                    }

                }

                // Ce qui vient de sonner, et la note qui colore ce mode : c'est le lien entre ce que l'oreille entend et
                // ce que la theorie en dit, au moment ou elle l'entend.
                Text {
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    horizontalAlignment: Text.AlignHCenter
                    color: "#8ef2b0"
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                    visible: ModeController.lastPlayedMode.index !== undefined
                    text: ModeController.lastPlayedMode.index !== undefined ? qsTr("%1 — %2").arg(ModeController.lastPlayedMode.name).arg(ModeController.lastPlayedMode.characteristic) : ""
                }

                // Le bouton disparait quand le contenu n'a pas de phrase pour ce mode, et c'est voulu : offrir un bouton
                // qui ne sonne pas apprendrait au joueur a ne plus appuyer.
                Button {
                    readonly property int playedModeIndex: ModeController.lastPlayedMode.index !== undefined ? ModeController.lastPlayedMode.index : -1

                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 10
                    visible: playedModeIndex >= 0 && ModeController.phraseCountForMode(playedModeIndex) > 0
                    text: qsTr("♪ Phrase")
                    onClicked: ModeController.playPhraseOfMode(playedModeIndex)
                }

                // Ce qui vient de sonner, en degres : « 1 4(2) 5 1 ». C'est la meme ecriture que celle de l'atelier, ou
                // ces phrases ont ete gardees - donc ce que l'oreille a juge peut se relire ici.
                Text {
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    Layout.topMargin: 6
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    horizontalAlignment: Text.AlignHCenter
                    color: "#cbb8e8"
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                    visible: ModeController.lastPlayedPhrase.degrees !== undefined
                    text: ModeController.lastPlayedPhrase.degrees !== undefined ? qsTr("la phrase : %1 (%2 bpm)").arg(ModeController.lastPlayedPhrase.degrees).arg(ModeController.lastPlayedPhrase.bpm) : ""
                }

                // Ici elle ne cache rien - le banc d'essai fait écouter à loisir - et c'est justement à quoi elle sert :
                // voir la fenêtre de notes pendant qu'on l'entend, jusqu'à ce que l'oreille la reconnaisse seule.
                ModeCircle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 12
                    visible: ModeController.playedModeCircle.length > 0
                    notes: ModeController.playedModeCircle
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
        // PLEIN ECRAN, et ce n'est pas un caprice : la page des reglages a grandi jusqu'a ne plus tenir dans une popup -
        // instruments, rappel, poids des jeux, tempo - et Roger a demande « un ecran pour avoir un peu plus de place ».
        // Un dialogue qui prend tout l'ecran est cet ecran, sans toucher a la navigation.
        width: mainWindow.width
        height: mainWindow.height
        modal: true
        padding: 12

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
            // La colonne ne doit jamais defiler de cote : rien ne depasse en largeur, et un leger mouvement horizontal
            // quand on fait defiler vers le bas est un defaut, pas une liberte.
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            // Et le contenu est BORNE a la largeur disponible, ce qui est la seule facon d'empecher vraiment le
            // deplacement lateral : la barre cachee n'empechait pas le glissement, et Roger l'a senti - « on peut toujours
            // bouger la page de gauche a droite, ce qui est derangeant avec les sliders ». Un curseur qu'on tire ne doit
            // jamais deplacer la page.
            contentWidth: availableWidth

            ColumnLayout {
                // Le TEMPO des phrases de mode, et son amplitude. Roger : « on pourrait choisir de l'augmenter, d'en choisir
                // un central et de varier autour de 20-30 bpm. Histoire de rendre moins monotone. »
                // Et celle-ci est en TETE parce que c'est d'elle que les autres se detachent : on commence par demander
                // moins de « nommer », et les parts qui suivent prennent ce qu'on lui retire. C'est la question que le
                // jeu posait a ses debuts, et la seule qui n'avait pas de part a elle - elle prenait ce qui restait, et
                // un total qui depassait cent la faisait disparaitre en silence.
                // LES POIDS DES JEUX, et dans CET ordre : d'abord les trois questions d'ecoute, puis les modes, puis la
                // note etrangere. Roger l'a demande tel quel, et l'ordre de l'ecran est aussi celui du tirage - une
                // part ne deplace donc que les questions qui la suivent.
                // Le micro, le tempere et le diapason ne sont PAS ici : ils vivent dans la page Accordeur, ou ils servent
                // vraiment. Roger l'a demande - « enlever le micro et la configuration du temperament, le diapason, et le
                // laisser seulement au niveau de l'accordeur » - et il a raison : un reglage se range la ou il s'emploie.
                // Le panneau du micro, lui, est le MEME composant que celui de l'accordeur : une seule definition.
                // LES DEUX BOUTONS DU BAS, dans l'ordre demande : « Par defaut » puis « Fermer ».

                id: settingsColumn

                width: settingsScroll.availableWidth
                spacing: 3

                Text {
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    color: "#e8dcff"
                    font.pixelSize: 16
                    font.bold: true
                    text: qsTr("Sons des instruments")
                }

                Text {
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
                    Layout.fillWidth: true
                    color: "#cbb8e8"
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                    text: qsTr("Le tirage se fait au hasard parmi les instruments cochés.")
                }

                // Les instruments sur DEUX colonnes : une seule colonne obligeait a faire defiler pour voir la liste
                // entiere, et Roger a demande ce regroupement - « les ecrire propre au lieu de mot sans majuscule, et
                // sur plusieurs colonnes plutot que sur une seule comme maintenant ».
                Flow {
                    Layout.fillWidth: true
                    spacing: 4

                    Repeater {
                        model: ExerciseController.instruments

                        delegate: CheckBox {
                            required property var modelData

                            width: (settingsColumn.width / 2) - 6
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

                }

                // L'HEURE du rappel est sur la MEME ligne que la case qui l'active : celui qui vient de l'activer cherche
                // aussitot QUAND il sonnera, et Roger a demande exactement ce regroupement - « et enlever le texte inutile
                // "L'heure du rappel" ».
                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 2
                    spacing: 6

                    CheckBox {
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

                    DarkSpinBox {
                        Layout.preferredWidth: 76
                        from: 0
                        to: 23
                        value: ExerciseController.reminderHour
                        onValueModified: ExerciseController.setReminderHour(value)
                    }

                    Text {
                        color: "#8a77ad"
                        font.pixelSize: 13
                        text: "h"
                    }

                    DarkSpinBox {
                        Layout.preferredWidth: 76
                        from: 0
                        to: 59
                        stepSize: 5
                        value: ExerciseController.reminderMinute
                        onValueModified: ExerciseController.setReminderMinute(value)
                    }

                }

                // Et les trois anecdotes, dites en clair : un joueur doit savoir ce qu'il va recevoir, sinon il coupera
                // les notifications pour ne plus etre derange - exactement l'inverse du but.
                Text {
                    Layout.fillWidth: true
                    // La largeur PREFEREE est mise a zero, et ce n'est pas un caprice : un Text replie rapporte la
                    // largeur de sa PLUS LONGUE LIGNE comme minimum, et QtQuick.Layouts la respecte - la phrase ci-dessous
                    // elargissait donc tout le dialogue a 489 points pour une vue de 338, et faisait apparaitre un
                    // defilement horizontal dans la page des reglages.
                    Layout.preferredWidth: 0
                    // Et le MINIMUM aussi : c'est lui qui bloquait. Par defaut, un item d'un layout a pour largeur
                    // minimale son implicitWidth - donc un texte replie a pour minimum la largeur de sa plus longue
                    // ligne, et il elargit le parent au lieu de se replier. Les deux lignes vont ensemble.
                    Layout.minimumWidth: 0
                    visible: ExerciseController.dailyReminderEnabled
                    color: "#8a77ad"
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    text: qsTr("Et trois anecdotes musicales dans la journée : le matin, le midi et le soir.")
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

                // Les parts sont des POIDS, lus les uns par rapport aux autres : leur somme est l'echelle, et le total
                // n'a donc pas besoin de faire cent. Vingt partout vaut un sixieme pour chacun.
                Text {
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
                    Layout.fillWidth: true
                    Layout.topMargin: 16
                    color: "#e8dcff"
                    font.pixelSize: 16
                    font.bold: true
                    text: qsTr("Poids des jeux")
                }

                Text {
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
                    Layout.fillWidth: true
                    color: "#cbb8e8"
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                    text: qsTr("Chaque curseur est un poids, pas un pourcentage : c'est le rapport entre les poids qui décide. Le pourcentage affiché est la chance réelle de tomber sur cette question.")
                }

                QuestionShareSetting {
                    Layout.fillWidth: true
                    Layout.topMargin: 10
                    title: qsTr("Nommer")
                    hint: qsTr("Poids des questions où l'on nomme l'intervalle entendu. Les parts se lisent entre elles : 20 partout vaut un sixième chacune.")
                    share: ExerciseController.namedIntervalQuestionShare
                    onShareEdited: (p_share) => {
                        return ExerciseController.setNamedIntervalQuestionShare(p_share);
                    }
                }

                // Le rythme et les accords sont arrives apres le chant, et ils se sont fait brancher sans une ligne
                // de mise en page nouvelle : c'est exactement ce que le composant promettait.
                QuestionShareSetting {
                    Layout.fillWidth: true
                    Layout.topMargin: 10
                    title: qsTr("Chant")
                    hint: qsTr("Poids des questions chantées. Un poids, lu par rapport aux autres parts.")
                    share: ExerciseController.singQuestionShare
                    onShareEdited: (p_share) => {
                        return ExerciseController.setSingQuestionShare(p_share);
                    }
                }

                QuestionShareSetting {
                    Layout.fillWidth: true
                    Layout.topMargin: 10
                    title: qsTr("Accords")
                    hint: qsTr("Poids des questions d'accords. Un poids, lu par rapport aux autres parts.")
                    share: ExerciseController.chordQuestionShare
                    onShareEdited: (p_share) => {
                        return ExerciseController.setChordQuestionShare(p_share);
                    }
                }

                // Les deux marches de l'harmonie, et elles sont bien DEUX : comparer deux couleurs est un travail
                // d'oreille, nommer un mode un travail de vocabulaire. Un joueur peut vouloir la premiere sans la
                // seconde, et l'application doit pouvoir dire laquelle resiste.
                QuestionShareSetting {
                    Layout.fillWidth: true
                    Layout.topMargin: 10
                    title: qsTr("Modes : plus clair, plus obscur")
                    hint: qsTr("Part des questions qui font comparer deux modes sur un bourdon. 0 = jamais. C'est la première marche de l'harmonie : de l'oreille, aucun vocabulaire.")
                    share: ExerciseController.modeColourQuestionShare
                    onShareEdited: (p_share) => {
                        return ExerciseController.setModeColourQuestionShare(p_share);
                    }
                }

                QuestionShareSetting {
                    Layout.fillWidth: true
                    Layout.topMargin: 10
                    title: qsTr("Modes : leur nom")
                    hint: qsTr("Part des questions qui font nommer un mode entendu sur un bourdon. 0 = jamais. La même couleur, mais dite avec un mot.")
                    share: ExerciseController.modeNameQuestionShare
                    onShareEdited: (p_share) => {
                        return ExerciseController.setModeNameQuestionShare(p_share);
                    }
                }

                // La plus avancee des trois : la MEME gamme sur deux centres differents. C'est la seule question dont la
                // reponse soit dans le contexte - les notes ne bougent pas, le mode si.
                QuestionShareSetting {
                    Layout.fillWidth: true
                    Layout.topMargin: 10
                    title: qsTr("Modes : deux centres")
                    hint: qsTr("Part des questions qui font entendre la même gamme sur deux centres différents. 0 = jamais. Les notes sont identiques, le mode change.")
                    share: ExerciseController.modeVampQuestionShare
                    onShareEdited: (p_share) => {
                        return ExerciseController.setModeVampQuestionShare(p_share);
                    }
                }

                // La troisieme marche de l'harmonie : la plus fine des trois, et c'est pour cela qu'elle vient en dernier.
                QuestionShareSetting {
                    Layout.fillWidth: true
                    Layout.topMargin: 10
                    title: qsTr("Note étrangère")
                    hint: qsTr("Poids des questions où une note étrangère se cache dans une gamme. 0 = jamais. Il faut tenir toute la gamme dans sa tête pour la repérer.")
                    share: ExerciseController.foreignNoteQuestionShare
                    onShareEdited: (p_share) => {
                        return ExerciseController.setForeignNoteQuestionShare(p_share);
                    }
                }

                // Le premier est le CENTRE, le second l'amplitude du tirage : chaque phrase est jouée à centre ± tirage,
                // donc aucune ne sonne exactement comme la précédente. Le tempo écrit dans le contenu ne décide plus.
                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 16
                    spacing: 6

                    Text {
                        color: "#8a77ad"
                        font.pixelSize: 13
                        text: qsTr("Tempo des phrases :")
                    }

                    DarkSpinBox {
                        Layout.preferredWidth: 86
                        from: 40
                        to: 160
                        stepSize: 2
                        value: ExerciseController.phraseTempoBpm
                        onValueModified: ExerciseController.setPhraseTempoBpm(value)
                    }

                    Text {
                        color: "#8a77ad"
                        font.pixelSize: 13
                        text: qsTr("bpm")
                    }

                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 2
                    spacing: 6

                    Text {
                        color: "#8a77ad"
                        font.pixelSize: 13
                        text: qsTr("Variation :")
                    }

                    DarkSpinBox {
                        Layout.preferredWidth: 86
                        from: 0
                        to: 40
                        stepSize: 5
                        value: ExerciseController.phraseTempoVariation
                        onValueModified: ExerciseController.setPhraseTempoVariation(value)
                    }

                    Text {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 0
                        color: "#8a77ad"
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                        text: qsTr("± bpm, tiré à chaque phrase")
                    }

                }

                // « Par defaut » est a gauche et discret : c'est un geste rare, et il ne touche JAMAIS au score - voir
                // resetPreferences. Un bouton qui efface les etoiles sans le dire serait un piege.
                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 16
                    spacing: 10

                    Button {
                        Layout.fillWidth: true
                        text: qsTr("Par défaut")
                        onClicked: ExerciseController.resetPreferences()
                    }

                    Button {
                        Layout.fillWidth: true
                        highlighted: true
                        text: qsTr("Fermer")
                        onClicked: settingsDialog.close()
                    }

                }

                // Et un peu de VIDE en bas : les derniers boutons tombaient sous la barre de navigation du telephone, et
                // Roger ne pouvait plus les atteindre - « la page de settings ne descend pas assez, les derniers boutons
                // sont caches par les boutons du telephone ».
                Item {
                    Layout.preferredHeight: 128
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
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

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
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
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

                // La barre de stabilite : elle se remplit tant que la note est tenue, puis repart pour la deuxieme.
                // C'est le feedback qui dit au chanteur si sa note TIENT ou si elle glisse.
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
                    text: MicrophoneController.hasSungInterval ? qsTr("Deux notes entendues.") : (MicrophoneController.hasFirstNote ? qsTr("Première note tenue — maintenant la deuxième") : qsTr("Tiens la première note…"))
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
                    // Un texte long doit se REPLIER, pas elargir la page : le verdict porte desormais la mesure en
                    // cents, et sans ces deux lignes il poussait la page au-dela de la largeur d'un telephone.
                    Layout.preferredWidth: 0
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: mainWindow.singingVerdictColor()
                    font.pixelSize: 18
                    font.bold: true
                    visible: MicrophoneController.hasSungInterval
                    // Le verdict, PUIS la mesure : « juste » ne dit pas de combien, et c'est ce chiffre qui permet de
                    // progresser. Un ecart positif veut dire un intervalle chante trop large, negatif trop etroit.
                    text: {
                        var cents = Math.round(MicrophoneController.sungCentsOffset);
                        var offset = (cents > 0 ? "+" : "") + cents + qsTr(" cents");
                        var verdict = MicrophoneController.sungVerdict === 1 ? qsTr("Juste !") : qsTr("Raté — entendu : %1 demi-tons").arg(MicrophoneController.sungSemitones);
                        return verdict + qsTr("  ·  %1").arg(offset);
                    }
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
        width: Math.min(mainWindow.width * 0.96, 560)
        // PLEIN ECRAN, ou presque : la page porte un histogramme, un camembert et une liste de points faibles, et un
        // dialogue de 420 points de large n'a pas la place de les montrer.
        height: mainWindow.height * 0.94
        modal: true
        padding: 12
        // Les statistiques se recalculent a l'OUVERTURE, et seulement la : rien n'est calcule tant que personne ne
        // regarde, et personne ne regarde un profil en jouant.
        onOpened: {
            StatisticsController.refresh();
            kindPie.requestPaint();
        }

        background: Rectangle {
            color: "#241442"
            radius: 14
            border.width: 1
            border.color: "#5c4a80"
        }

        // Le contenu DEFILE : la page est longue - un profil, six chiffres, un histogramme, un camembert et une liste de
        // points faibles - et un ecran de telephone ne les montre pas d'un coup.
        contentItem: ScrollView {
            id: profileScroll

            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                // -----------------------------------------------------------------------------------------------------
                // LES STATISTIQUES
                // Ce que le joueur travaille vraiment, ce qu'il delaisse sans le savoir, et combien de temps il joue.
                // LE CAMEMBERT : la part de chaque genre de question, pour voir d'un coup d'oeil ce qui est travaille.

                width: profileScroll.availableWidth
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

                // La FLAMME : les jours d'affilee. Elle ne s'affiche QUE s'il y en a une - « 0 jour d'affilee » serait une
                // facon de dire au joueur qu'il n'a rien fait, et un profil n'est pas la pour ca.
                Text {
                    Layout.fillWidth: true
                    visible: StatisticsController.playingDayStreak > 0
                    color: "#ffd479"
                    font.pixelSize: 14
                    font.bold: true
                    text: StatisticsController.playingDayStreak > 1 ? qsTr("🔥 %1 jours d'affilée !").arg(StatisticsController.playingDayStreak) : qsTr("🔥 C'est parti pour une série")
                }

                // Tout vient du JOURNAL, et de lui seul : chaque question conclue y laisse une ligne, et cette page est ce
                // que ces lignes racontent quand on les empile.
                // -----------------------------------------------------------------------------------------------------
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    Layout.topMargin: 8
                    color: "#5c4a80"
                }

                Text {
                    Layout.fillWidth: true
                    color: "#e8dcff"
                    font.pixelSize: 16
                    font.bold: true
                    text: qsTr("Statistiques")
                }

                // Sans journal, la page le DIT plutot que d'afficher des zeros : un ecran plein de « 0 % » n'informe pas, il
                // decourage.
                Text {
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
                    Layout.fillWidth: true
                    visible: !StatisticsController.hasHistory
                    color: "#8a77ad"
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                    text: qsTr("Rien à raconter pour l'instant : joue quelques questions et cette page se remplira toute seule.")
                }

                // LES POINTS FAIBLES ouvrent la page, et c'est volontaire : « tu rates les sixtes » est l'information sur
                // laquelle le joueur peut agir ce soir, la ou « 72 % de reussite » se regarde et ne dit rien. C'est aussi ce
                // qui nourrit le Bilan.
                Text {
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    visible: StatisticsController.weakestTargets.length > 0
                    color: "#e8dcff"
                    font.pixelSize: 14
                    font.bold: true
                    text: qsTr("Ce qui te résiste")
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    visible: StatisticsController.weakestTargets.length > 0
                    spacing: 3

                    Repeater {
                        model: StatisticsController.weakestTargets

                        delegate: RowLayout {
                            required property var modelData

                            Layout.fillWidth: true
                            spacing: 6

                            Text {
                                Layout.fillWidth: true
                                color: "#e8dcff"
                                font.pixelSize: 13
                                elide: Text.ElideRight
                                text: modelData.name
                            }

                            Text {
                                color: "#8a77ad"
                                font.pixelSize: 11
                                text: qsTr("%1 fois").arg(modelData.questionCount)
                            }

                            Text {
                                Layout.preferredWidth: 44
                                horizontalAlignment: Text.AlignRight
                                // Rouge sous la moitie, vert au-dessus : deux couleurs, et l'oeil sait ou regarder sans lire
                                // un seul chiffre.
                                color: modelData.successPercent < 50 ? "#ff8fb0" : "#8ef2b0"
                                font.pixelSize: 13
                                font.bold: true
                                text: qsTr("%1 %").arg(modelData.successPercent)
                            }

                        }

                    }

                }

                // LES TROIS GRANDS CHIFFRES, et le deuxieme est le taux de reussite DU PREMIER COUP : il mesure le « su »
                // plutot que le « trouve », et c'est le plus honnete des trois.
                RowLayout {
                    Layout.fillWidth: true
                    visible: StatisticsController.hasHistory
                    spacing: 6

                    BigStat {
                        Layout.fillWidth: true
                        value: qsTr("%1 %").arg(StatisticsController.successPercent)
                        valueColour: "#8ef2b0"
                        label: qsTr("de réussite")
                    }

                    BigStat {
                        Layout.fillWidth: true
                        value: qsTr("%1 %").arg(StatisticsController.firstTryPercent)
                        valueColour: "#ffd479"
                        label: qsTr("du premier coup")
                    }

                    BigStat {
                        Layout.fillWidth: true
                        value: StatisticsController.questionCount
                        label: qsTr("questions")
                    }

                }

                Text {
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
                    Layout.fillWidth: true
                    visible: StatisticsController.hasHistory
                    color: "#cbb8e8"
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                    text: qsTr("Temps de jeu : %1 en tout · %2 cette semaine").arg(StatisticsController.playTimeText).arg(StatisticsController.recentPlayTimeText)
                }

                // L'HISTOGRAMME des quatorze derniers jours. Une barre par jour : sa hauteur dit le nombre de questions, et sa
                // partie verte dit ce qui a ete trouve. Un jour vide garde une barre minuscule - « je n'ai pas joue » est une
                // information, et sans cette barre on ne verrait pas la difference entre un jour vide et un jour absent.
                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 6
                    Layout.preferredHeight: 84
                    visible: StatisticsController.hasHistory
                    spacing: 3

                    Repeater {
                        model: StatisticsController.lastDays

                        delegate: ColumnLayout {
                            required property var modelData

                            Layout.fillWidth: true
                            spacing: 2

                            Item {
                                Layout.fillWidth: true
                                Layout.fillHeight: true

                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    width: parent.width
                                    height: parent.height * Math.max(modelData.heightRatio, 0.04)
                                    radius: 2
                                    color: modelData.isToday ? "#7b5cc4" : "#4a3670"
                                }

                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    width: parent.width
                                    // La part de REUSSITE, gardee d'une division par zero : un jour sans question n'a pas de
                                    // taux, il a une barre vide.
                                    height: modelData.questionCount > 0 ? parent.height * modelData.heightRatio * (modelData.correctCount / modelData.questionCount) : 0
                                    radius: 2
                                    color: "#8ef2b0"
                                }

                            }

                            Text {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                color: modelData.isToday ? "#ffffff" : "#8a77ad"
                                font.pixelSize: 9
                                font.bold: modelData.isToday
                                text: modelData.dayOfMonth
                            }

                        }

                    }

                }

                // La LEGENDE de l'histogramme, sous les barres.
                Text {
                    Layout.fillWidth: true
                    visible: StatisticsController.hasHistory
                    color: "#8a77ad"
                    font.pixelSize: 10
                    text: qsTr("Les 14 derniers jours · vert : trouvé, violet : posé")
                }

                // Dessine au Canvas, qui fait partie de QtQuick depuis le premier jour : aucun module a deployer, aucune
                // dependance a ajouter pour un dessin. Les ANGLES viennent du CONTROLEUR - les calculer ici, une fois pour la
                // forme et une fois pour la legende, serait la meilleure facon de les faire diverger.
                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 10
                    visible: StatisticsController.hasHistory && StatisticsController.kinds.length > 0
                    spacing: 14

                    Canvas {
                        id: kindPie

                        Layout.preferredWidth: 116
                        Layout.preferredHeight: 116
                        onPaint: {
                            var context = getContext("2d");
                            context.reset();
                            var centerX = width / 2;
                            var centerY = height / 2;
                            var radius = Math.min(centerX, centerY) - 2;
                            var parts = StatisticsController.kinds;
                            for (var index = 0; index < parts.length; ++index) {
                                var part = parts[index];
                                var start = part.startAngle * Math.PI / 180;
                                var end = (part.startAngle + part.sweepAngle) * Math.PI / 180;
                                context.beginPath();
                                context.moveTo(centerX, centerY);
                                context.arc(centerX, centerY, radius, start, end);
                                context.closePath();
                                context.fillStyle = mainWindow.kindColour(index);
                                context.fill();
                            }
                            // Le TROU du milieu : c'est ce qui en fait un beignet plutot qu'une tarte, et c'est la que le
                            // nombre de questions se pose.
                            context.beginPath();
                            context.arc(centerX, centerY, radius * 0.55, 0, 2 * Math.PI);
                            context.fillStyle = "#241442";
                            context.fill();
                        }

                        // Un Canvas ne se repaint PAS tout seul : c'est le signal du controleeur qui le lui dit.
                        Connections {
                            function onStatisticsChanged() {
                                kindPie.requestPaint();
                            }

                            target: StatisticsController
                        }

                        Text {
                            anchors.centerIn: parent
                            color: "#ffffff"
                            font.pixelSize: 16
                            font.bold: true
                            text: StatisticsController.questionCount
                        }

                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Repeater {
                            model: StatisticsController.kinds

                            delegate: RowLayout {
                                required property var modelData
                                required property int index

                                Layout.fillWidth: true
                                spacing: 6

                                Rectangle {
                                    Layout.preferredWidth: 10
                                    Layout.preferredHeight: 10
                                    radius: 2
                                    color: mainWindow.kindColour(index)
                                }

                                Text {
                                    Layout.fillWidth: true
                                    color: "#e8dcff"
                                    font.pixelSize: 12
                                    text: modelData.name
                                }

                                Text {
                                    color: "#8a77ad"
                                    font.pixelSize: 12
                                    text: qsTr("%1 %").arg(modelData.sharePercent)
                                }

                            }

                        }

                    }

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
                    text: qsTr("Tester le rappel")
                    onClicked: ExerciseController.testReminder()
                }

                // Remet l'experience, les sessions, les etoiles ET les statistiques a zero : un score efface qui garderait
                // son journal continuerait de raconter une histoire que le joueur vient d'effacer. Le nom et le niveau
                // restent : ce sont des choix, pas un score. Aucune confirmation pour l'instant - l'application est en
                // developpement.
                Button {
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("Remise à zéro complète")
                    onClicked: ExerciseController.resetProfile()
                }

                Button {
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("Fermer")
                    onClicked: profileDialog.close()
                }

            }

        }

    }

    // L'ARBRE VIENT DU DOMAINE (ChordTree), qui dit qui descend de qui et par quel geste. Cet ecran ne fait que le
    // DESSINER : la PROFONDEUR donne la colonne, le RANG dans la liste donne la ligne, et l'ordre de la liste - un
    // parcours en profondeur - garantit qu'un enfant est toujours juste sous son parent.
    // =================================================================================================================
    Dialog {
        id: chordTreeDialog

        anchors.centerIn: parent
        // Plein ecran : la carte est plus haute que large, et c'est une page qu'on lit, pas un message qu'on acquitte.
        width: mainWindow.width
        height: mainWindow.height
        modal: true
        padding: 8

        background: Rectangle {
            color: "#160d2b"
        }

        contentItem: ScrollView {
            id: chordTreeScroll

            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: chordTreeScroll.availableWidth
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    color: "#e8dcff"
                    font.pixelSize: 18
                    font.bold: true
                    text: qsTr("L'arbre des accords")
                }

                // La phrase qui explique la page, en une ligne : un ecran qu'on ne comprend pas ne s'explore pas.
                Text {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
                    color: "#8a77ad"
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    text: qsTr("Chaque couleur s'obtient depuis celle du dessus en UN geste. Lis les gestes, et les quinze accords deviennent une seule histoire.")
                }

                // L'ARBRE, dessine par le COMPOSANT partage avec l'ecran de jeu : une seule definition du layout, donc
                // aucune chance de voir deux arbres differents selon l'ecran qui les montre. Ce detail a son importance -
                // un arbre de competences qui ne se ressemblerait pas d'un ecran a l'autre serait un arbre auquel on ne se
                // fierait plus.
                ChordTreeView {
                    Layout.fillWidth: true
                    showDegrees: true
                }

                // Les GESTES, en clair, sous la carte : l'arbre montre le CHEMIN, cette liste dit ce qu'on fait en le
                // suivant. C'est la partie qui apprend quelque chose, et elle se lit comme une phrase.
                Text {
                    Layout.fillWidth: true
                    Layout.topMargin: 10
                    color: "#e8dcff"
                    font.pixelSize: 15
                    font.bold: true
                    text: qsTr("Les quatorze gestes")
                }

                Repeater {
                    model: ExerciseController.chordTree

                    delegate: RowLayout {
                        required property var modelData

                        Layout.fillWidth: true
                        spacing: 6
                        visible: !modelData.isRoot

                        Text {
                            Layout.preferredWidth: 56
                            color: "#ffffff"
                            font.pixelSize: 13
                            font.bold: true
                            text: modelData.name
                        }

                        Text {
                            color: "#8a77ad"
                            font.pixelSize: 12
                            // Le noeud RACINE n'a pas de parent : il porte l'index -1, qui ne trouve rien, et QML le
                            // signalait par un « Cannot read property 'name' of undefined » a chaque demarrage. La
                            // ligne est bien masquee pour lui, mais un binding s'evalue MEME quand il ne se voit pas -
                            // c'est la meme lecon que les textes replies qui elargissaient leur dialogue.
                            text: modelData.isRoot ? "" : qsTr("depuis %1").arg(ExerciseController.chordTree[modelData.parentIndex].name)
                        }

                        Text {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 0
                            Layout.minimumWidth: 0
                            color: "#ffd479"
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                            text: "· " + modelData.mutation
                        }

                    }

                }

                Button {
                    Layout.alignment: Qt.AlignRight
                    Layout.topMargin: 8
                    text: qsTr("Fermer")
                    onClicked: chordTreeDialog.close()
                }

            }

        }

    }

    // Plein ecran, comme l'accordeur, et pour la meme raison : c'est une page qu'on LIT, pas un message qu'on acquitte.
    // La roue a besoin de la place pour que ses douze cases respirent, chacune portant trois informations.
    // =================================================================================================================
    Dialog {
        id: keyCircleDialog

        anchors.centerIn: parent
        width: mainWindow.width
        height: mainWindow.height
        modal: true
        padding: 0

        KeyCircleScreen {
            anchors.fill: parent
        }

    }

    // Les textes viennent d'un fichier de contenu (assets/content/tuner.json), comme les anecdotes et les indices : ils
    // s'ecrivent et se corrigent sans toucher au code. Un fichier absent laisse la page sans explications, et rien
    // d'autre : les reglages, eux, restent la.
    // =================================================================================================================
    Dialog {
        id: tunerDialog

        anchors.centerIn: parent
        // Plein ecran : c'est une page qu'on lit et qu'on regle, pas un message qu'on acquitte.
        width: mainWindow.width
        height: mainWindow.height
        modal: true
        padding: 12
        // Arriver sur la page ouvre le micro : le jeu entier est bati sur lui, et un accordeur qui reste muet tant
        // qu'on n'a pas cherche un bouton est un accordeur qu'on croit casse. Quitter la page le referme : personne
        // n'accorde plus rien, et un micro ouvert vide la batterie pour rien.
        onOpened: MicrophoneController.ensureListening()
        onClosed: MicrophoneController.stopTest()

        background: Rectangle {
            color: "#160d2b"
        }

        contentItem: ScrollView {
            id: tunerScroll

            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: tunerScroll.availableWidth
                spacing: 10

                Text {
                    Layout.fillWidth: true
                    color: "#e8dcff"
                    font.pixelSize: 20
                    font.bold: true
                    text: qsTr("Accordeur")
                }

                Text {
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
                    Layout.fillWidth: true
                    color: "#cbb8e8"
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                    text: qsTr("Joue une note, ou chante-la : l'accordeur dit laquelle c'est, et de combien elle s'en écarte.")
                }

                // LE MEME PANNEAU que dans les reglages : la portee, la boule, la note la plus proche et l'ecart en
                // cents. Deux copies montreraient tot ou tard deux accordeurs differents.
                MicrophonePanel {
                }

                // --- Comment s'en servir -------------------------------------------------------------------------
                Text {
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    color: "#e8dcff"
                    font.pixelSize: 15
                    font.bold: true
                    text: qsTr("Comment s'en servir")
                }

                Repeater {
                    model: ExerciseController.tunerGuide.modeEmploi

                    delegate: RowLayout {
                        required property var modelData
                        required property int index

                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            Layout.alignment: Qt.AlignTop
                            color: "#ffd479"
                            font.pixelSize: 14
                            font.bold: true
                            text: (index + 1) + "."
                        }

                        Text {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 0
                            Layout.minimumWidth: 0
                            color: "#cbb8e8"
                            font.pixelSize: 13
                            wrapMode: Text.WordWrap
                            text: modelData
                        }

                    }

                }
                // --- Le temperament ------------------------------------------------------------------------------

                Rectangle {
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    Layout.preferredHeight: temperamentColumn.implicitHeight + 24
                    color: "#2a1a46"
                    radius: 8
                    border.width: 1
                    border.color: "#5c4a80"

                    ColumnLayout {
                        id: temperamentColumn

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 12
                        spacing: 8

                        Text {
                            Layout.fillWidth: true
                            color: "#e8dcff"
                            font.pixelSize: 15
                            font.bold: true
                            text: qsTr("Tempérament")
                        }

                        DarkComboBox {
                            model: ExerciseController.temperaments
                            currentIndex: ExerciseController.temperament
                            onActivated: ExerciseController.setTemperament(index)
                        }

                        // L'EXPLICATION du temperament choisi : d'ou il vient, et comment il fonctionne. Elle vient du
                        // fichier de contenu, donc c'est le contenu qui change, jamais le code.
                        Text {
                            Layout.preferredWidth: 0
                            Layout.minimumWidth: 0
                            Layout.fillWidth: true
                            visible: text !== ""
                            color: "#cbb8e8"
                            font.pixelSize: 13
                            wrapMode: Text.WordWrap
                            text: ExerciseController.temperamentExplanation
                        }

                        Text {
                            Layout.preferredWidth: 0
                            Layout.minimumWidth: 0
                            Layout.fillWidth: true
                            color: "#8a77ad"
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                            text: qsTr("Valeur par défaut : tempérament égal.")
                        }

                    }

                }

                // --- La note de reference ------------------------------------------------------------------------
                Rectangle {
                    Layout.fillWidth: true
                    // Elle n'apparait QUE quand elle veut dire quelque chose : le tempere egal l'ignore, et un reglage
                    // qui ne fait rien fait douter de tous les autres.
                    visible: ExerciseController.temperament !== 0
                    Layout.preferredHeight: referenceColumn.implicitHeight + 24
                    color: "#2a1a46"
                    radius: 8
                    border.width: 1
                    border.color: "#5c4a80"

                    ColumnLayout {
                        id: referenceColumn

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 12
                        spacing: 8

                        Text {
                            Layout.fillWidth: true
                            color: "#e8dcff"
                            font.pixelSize: 15
                            font.bold: true
                            text: qsTr("Note de référence")
                        }

                        DarkComboBox {
                            model: ExerciseController.tuningRoots
                            currentIndex: ExerciseController.tuningRoot
                            onActivated: ExerciseController.setTuningRoot(index)
                        }

                        Text {
                            Layout.preferredWidth: 0
                            Layout.minimumWidth: 0
                            Layout.fillWidth: true
                            visible: text !== ""
                            color: "#cbb8e8"
                            font.pixelSize: 13
                            wrapMode: Text.WordWrap
                            text: ExerciseController.tunerGuide.noteDeReference
                        }

                        Text {
                            Layout.preferredWidth: 0
                            Layout.minimumWidth: 0
                            Layout.fillWidth: true
                            color: "#8a77ad"
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                            text: qsTr("Valeur par défaut : do.")
                        }

                    }

                }

                // --- Le diapason ---------------------------------------------------------------------------------
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: diapasonColumn.implicitHeight + 24
                    color: "#2a1a46"
                    radius: 8
                    border.width: 1
                    border.color: "#5c4a80"

                    ColumnLayout {
                        id: diapasonColumn

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 12
                        spacing: 8

                        Text {
                            Layout.fillWidth: true
                            color: "#e8dcff"
                            font.pixelSize: 15
                            font.bold: true
                            text: qsTr("Diapason (le la de référence)")
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            SpinBox {
                                Layout.preferredWidth: 110
                                Layout.preferredHeight: 36
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

                            Text {
                                color: "#8a77ad"
                                font.pixelSize: 14
                                text: qsTr("Hz")
                            }

                        }

                        // TROIS DIAPASONS en un geste : le baroque, la norme, et l'orchestre. Ce sont les trois
                        // qu'on cherche vraiment quand on accorde, et les taper a la main est une perte de temps.
                        // Sur leur PROPRE ligne, et c'est la meme lecon que la banquette des intervalles : poses a
                        // cote du champ et de son unite, il ne restait plus assez de place pour trois chiffres, et
                        // le style Material repliait « 440 » en « 4... ». Le style Material reserve 24 points de
                        // marge de chaque cote d'un bouton ; ces boutons portent un nombre, pas une phrase, donc ils
                        // n'ont pas besoin de la marge, ils ont besoin de la place.
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Button {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 36
                                leftPadding: 6
                                rightPadding: 6
                                text: "415"
                                onClicked: ExerciseController.setReferencePitch(415)
                            }

                            Button {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 36
                                leftPadding: 6
                                rightPadding: 6
                                text: "440"
                                onClicked: ExerciseController.setReferencePitch(440)
                            }

                            Button {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 36
                                leftPadding: 6
                                rightPadding: 6
                                text: "442"
                                onClicked: ExerciseController.setReferencePitch(442)
                            }

                        }

                        Text {
                            Layout.preferredWidth: 0
                            Layout.minimumWidth: 0
                            Layout.fillWidth: true
                            visible: text !== ""
                            color: "#cbb8e8"
                            font.pixelSize: 13
                            wrapMode: Text.WordWrap
                            text: ExerciseController.tunerGuide.diapason
                        }

                        Text {
                            Layout.preferredWidth: 0
                            Layout.minimumWidth: 0
                            Layout.fillWidth: true
                            color: "#8a77ad"
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                            text: qsTr("Valeur par défaut : 440 Hz.")
                        }

                    }

                }

                // --- Les valeurs par defaut ----------------------------------------------------------------------
                Text {
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    color: "#8a77ad"
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                    text: qsTr("Les valeurs par défaut sont celles du jeu : tempérament égal, note de référence do, diapason 440 Hz.")
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Button {
                        Layout.fillWidth: true
                        text: qsTr("Rétablir les valeurs par défaut")
                        onClicked: {
                            ExerciseController.setTemperament(0);
                            ExerciseController.setTuningRoot(0);
                            ExerciseController.setReferencePitch(440);
                        }
                    }

                    Button {
                        Layout.fillWidth: true
                        text: qsTr("Fermer")
                        onClicked: tunerDialog.close()
                    }

                }

            }

        }

    }

    Timer {
        id: feedbackTimer

        interval: 6000
        onTriggered: mainWindow.feedbackVisible = false
    }

    Dialog {
        id: rhythmDialog

        anchors.centerIn: parent
        width: Math.min(mainWindow.width * 0.9, 420)
        height: Math.min(mainWindow.height * 0.9, rhythmColumn.implicitHeight + 32)
        modal: true
        padding: 16
        // QUITTER LA PAGE ARRETE LE METRONOME, et il n'y a pas d'exception : un clic qui continue de battre sous une
        // autre page est un son qu'on ne peut plus arreter, puisqu'on ne voit plus le bouton qui l'a lance.
        onClosed: RhythmController.stop()

        background: Rectangle {
            color: "#241442"
            radius: 14
            border.width: 1
            border.color: "#5c4a80"
        }

        contentItem: ScrollView {
            id: rhythmScroll

            clip: true
            ScrollBar.vertical.policy: ScrollBar.AsNeeded
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                id: rhythmColumn

                width: rhythmScroll.availableWidth
                spacing: 10

                Text {
                    Layout.fillWidth: true
                    color: "#e8dcff"
                    font.pixelSize: 16
                    font.bold: true
                    text: qsTr("Rythme")
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Text {
                            color: "#8a77ad"
                            font.pixelSize: 12
                            text: qsTr("Tempo (bpm)")
                        }

                        SpinBox {
                            Layout.preferredWidth: 130
                            from: 1
                            to: 300
                            editable: true
                            value: RhythmController.bpm
                            onValueModified: RhythmController.setBpm(value)

                            contentItem: TextInput {
                                text: parent.textFromValue(parent.value, parent.locale)
                                color: "#ffffff"
                                horizontalAlignment: Text.AlignHCenter
                                font.pixelSize: 15
                                validator: parent.validator
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

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Text {
                            color: "#8a77ad"
                            font.pixelSize: 12
                            text: qsTr("Temps par mesure")
                        }

                        SpinBox {
                            Layout.preferredWidth: 130
                            from: 1
                            to: 12
                            editable: true
                            value: RhythmController.beatsPerBar
                            onValueModified: RhythmController.setBeatsPerBar(value)

                            contentItem: TextInput {
                                text: parent.textFromValue(parent.value, parent.locale)
                                color: "#ffffff"
                                horizontalAlignment: Text.AlignHCenter
                                font.pixelSize: 15
                                validator: parent.validator
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

                // La cellule rythmique : le metronome seul, ou un cliche a reproduire. Le clic continue de battre la
                // mesure par-dessus, pour que la pulsation reste le repere.
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Text {
                        color: "#8a77ad"
                        font.pixelSize: 12
                        text: qsTr("Rythmique à reproduire")
                    }

                    DarkComboBox {
                        Layout.fillWidth: true
                        model: RhythmController.patterns
                        currentIndex: RhythmController.currentPattern
                        onActivated: RhythmController.setCurrentPattern(index)
                    }

                    Text {
                        Layout.preferredWidth: 0
                        Layout.minimumWidth: 0
                        Layout.fillWidth: true
                        color: "#8a77ad"
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        text: qsTr("Lance le métronome : la rythmique boucle. Tape sur TAPE en même temps que les frappes.")
                    }

                }

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 90
                    highlighted: RhythmController.lastQuality === 2
                    text: RhythmController.isRunning ? qsTr("TAPE · temps %1").arg(RhythmController.beatInBar + 1) : qsTr("TAPE (tap tempo)")
                    onClicked: RhythmController.tap()
                }

                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    color: "#8ef2b0"
                    font.pixelSize: 14
                    text: qsTr("Score %1 · série %2").arg(RhythmController.score).arg(RhythmController.combo)
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Button {
                        Layout.fillWidth: true
                        text: RhythmController.isRunning ? qsTr("■ Arrêter") : qsTr("▶ Démarrer")
                        onClicked: RhythmController.isRunning ? RhythmController.stop() : RhythmController.start()
                    }

                    Button {
                        Layout.fillWidth: true
                        text: qsTr("Fermer")
                        onClicked: rhythmDialog.close()
                    }

                }

                Text {
                    Layout.fillWidth: true
                    Layout.topMargin: 6
                    color: "#e8dcff"
                    font.pixelSize: 14
                    font.bold: true
                    text: qsTr("Batterie")
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 8
                    rowSpacing: 8

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 56
                        text: qsTr("Grosse caisse")
                        onClicked: RhythmController.playDrum(0)
                    }

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 56
                        text: qsTr("Caisse claire")
                        onClicked: RhythmController.playDrum(1)
                    }

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 56
                        text: qsTr("Charleston")
                        onClicked: RhythmController.playDrum(2)
                    }

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 56
                        text: qsTr("Tom")
                        onClicked: RhythmController.playDrum(3)
                    }

                }

            }

        }

    }

    // Il arrive quand on revient d'une partie FINIE (voir stopSession, cote controleur), avec l'anecdote du moment, et il
    // ne part que sur un clic : un texte qu'on n'a pas fini de lire est un texte qu'on n'aurait pas du montrer.
    // =================================================================================================================
    Item {
        // Le chien parle : sa bulle est au-dessus de lui, et elle porte l'anecdote.

        id: chibaPopup

        anchors.fill: parent
        z: 1000
        visible: ExerciseController.isChibaTalking

        // Un voile : il dit que la page est en pause, et il est cliquable EN ENTIER - y compris a cote du chien, parce
        // qu'un bouton qu'il faut viser est un bouton qu'on rate.
        Rectangle {
            anchors.fill: parent
            color: "#990b0620"

            MouseArea {
                anchors.fill: parent
                onClicked: ExerciseController.dismissChiba()
            }

        }

        // La bulle et le chien ont la MEME largeur, et c'est une correction : Roger a vu le contraire - « le chien est
        // legerement mal centre, legerement trop a droite, du coup la bulle de texte depasse un peu a droite ». Le chien
        // n'etait pas decentre (mesure : son contour occupe toute la largeur de l'image) ; c'etait la bulle qui etait plus
        // large que lui.
        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(chibaPopup.width - 96, 300)
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: musichienLabel.implicitHeight + 36
                color: "#fdf8ff"
                radius: 18
                border.width: 2
                border.color: "#3a1f5c"

                Text {
                    id: musichienLabel

                    anchors.centerIn: parent
                    width: parent.width - 32
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: "#2a1548"
                    font.pixelSize: 16
                    text: ExerciseController.anecdoteText
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: ExerciseController.dismissChiba()
                }

            }

            Image {
                Layout.fillWidth: true
                // La hauteur est bornee par l'ecran, pour que le chien ne mange pas la page en paysage ; la largeur, elle,
                // est celle de la bulle. Les deux blocs se superposent donc exactement, et plus rien ne depasse.
                Layout.preferredHeight: Math.min(chibaPopup.height * 0.34, 190)
                fillMode: Image.PreserveAspectFit
                // Une des QUATRE humeurs, tiree au hasard a chaque fois qu'il ouvre la bouche.
                source: ExerciseController.chibaImageSource

                MouseArea {
                    anchors.fill: parent
                    onClicked: ExerciseController.dismissChiba()
                }

            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 8
                color: "#cbb8e8"
                font.pixelSize: 12
                text: qsTr("touche l'écran pour continuer")
            }

        }

    }

    component MicrophonePanel: Rectangle {
        Layout.fillWidth: true
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
                // Le signe de l'octave : ici, oui. L'accordeur sert a lire une hauteur ABSOLUE, et le signe dit que
                // la note est ailleurs que sur la portee - c'est ce qui distingue un accordeur d'un jeu.
                showOctaveShift: true
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

        }

    }

    // Un grand chiffre, avec ce qu'il veut dire. Trois par page suffisent : au-dela, on ne lit plus, on survole.
    component BigStat: ColumnLayout {
        id: bigStat

        property string value: ""
        property string label: ""
        property color valueColour: "#ffffff"

        spacing: 0

        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            color: bigStat.valueColour
            font.pixelSize: 24
            font.bold: true
            text: bigStat.value
        }

        Text {
            Layout.preferredWidth: 0
            Layout.minimumWidth: 0
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            color: "#8a77ad"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            text: bigStat.label
        }

    }

    // Material peint le champ sur fond clair avec un texte noir : illisible sur notre nuit violette, et c'est le meme
    // defaut que celui deja corrige sur les ComboBox et sur le nom du profil. Il est donc corrige UNE fois, ici, et
    // chaque reglage numerique de la page en herite.
    component DarkSpinBox: SpinBox {
        Layout.preferredHeight: 32
        editable: true

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

    // UNE seule definition pour les trois reglages - chant, rythme, accords - parce que trois copies identiques
    // finissent toujours par diverger, et parce que le prochain genre de question en aura une quatrieme a brancher.
    component QuestionShareSetting: ColumnLayout {
        // Le pourcentage EFFECTIF de cette part : la part divisee par la SOMME des parts.

        id: questionShareSetting

        property string title: ""
        property string hint: ""
        property int share: 0
        // C'est ce que le joueur veut savoir - « quelle chance a cette question de tomber ? » - et le calcul ne doit pas
        // etre le sien. Roger l'a demande exactement comme ca : « l'utilisateur ne va pas faire le calcul lui-meme pour
        // obtenir 100% au total ». Il a raison, et c'est meme pire que ca : les parts sont des POIDS, donc leur somme
        // n'est pas cent - un reglage a 20 partout ne donne pas 20 % mais un septieme, et personne ne peut le deviner.
        readonly property int total: ExerciseController.namedIntervalQuestionShare + ExerciseController.singQuestionShare + ExerciseController.chordQuestionShare + ExerciseController.modeColourQuestionShare + ExerciseController.modeNameQuestionShare + ExerciseController.modeVampQuestionShare + ExerciseController.foreignNoteQuestionShare
        readonly property int percent: total > 0 ? Math.round((100 * share) / total) : 0

        signal shareEdited(real p_share)

        spacing: 6

        Text {
            Layout.fillWidth: true
            color: "#e8dcff"
            font.pixelSize: 14
            font.bold: true
            text: questionShareSetting.title
        }

        Text {
            Layout.preferredWidth: 0
            Layout.minimumWidth: 0
            Layout.fillWidth: true
            color: "#8a77ad"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            text: questionShareSetting.hint
        }

        RowLayout {
            spacing: 10

            // Un CURSEUR, et un champ a cote. Roger : « au lieu de + et de -, mettre un slider (avec le textfield au bout
            // pour changer a la main) ». Le curseur pour le geste, le champ pour la valeur exacte - et le pourcentage
            // effectif juste apres, parce que c'est lui que le joueur veut lire.
            Slider {
                Layout.fillWidth: true
                from: 0
                to: 100
                stepSize: 5
                value: questionShareSetting.share
                onMoved: questionShareSetting.shareEdited(value)
            }

            DarkSpinBox {
                Layout.preferredWidth: 92
                Layout.alignment: Qt.AlignRight
                from: 0
                to: 100
                stepSize: 5
                value: questionShareSetting.share
                onValueModified: questionShareSetting.shareEdited(value)
            }

            // Le chiffre qui compte vraiment. Il est en vert parce qu'il BOUGE quand on regle : c'est lui qu'on regarde.
            Text {
                color: questionShareSetting.percent > 0 ? "#8ef2b0" : "#6f5b93"
                font.pixelSize: 15
                font.bold: true
                text: questionShareSetting.percent + " %"
            }

        }

    }

    // Un composant inline plutot qu'un fichier : il n'est utile qu'ici, et il evite de dupliquer trois fois le meme
    // style pour les trois listes de la page.
    component DarkComboBox: ComboBox {
        // Un ComboBox ne doit JAMAIS prendre la largeur de son texte, et ces trois lignes sont la pour ca.

        id: combo

        // QtQuick.Layouts respecte l'implicitWidth d'un item comme un MINIMUM : une seule entree longue - « Aucune entree
        // audio detectee, verifie le profil de ta carte son » - elargissait donc tout le dialogue des reglages, bien plus
        // large que l'ecran, et faisait apparaitre un defilement horizontal dont personne ne voulait. Le texte trop long
        // est coupe, maintenant, au lieu de pousser le mur.
        Layout.minimumWidth: 0
        Layout.preferredWidth: 0
        Layout.fillWidth: true

        contentItem: Text {
            text: combo.displayText
            color: "#ffffff"
            verticalAlignment: Text.AlignVCenter
            leftPadding: 10
            elide: Text.ElideRight
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

}
