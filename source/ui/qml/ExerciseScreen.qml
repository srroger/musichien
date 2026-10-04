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
    // ("c'était…, tu as répondu…"). A single pause is tuned for the slowest case, and therefore too long
    // for the fastest - which is the one that happens most.
    // Everything that moves is gathered here, because the whole screen shifts sideways during the shake.
    // La pause d'une question de rythme : la duree de la cellule, plus une respiration.
    // La couleur d'une COULEUR D'ACCORD, comme la couleur d'un intervalle : un code qui se lit avant le nom.
    // La couleur d'une couleur d'accord vient du CONTROLEUR (chordColourName) : elle est la meme sur tous les ecrans, et
    // elle le restera. Deux fichiers QML qui recalculeraient chacun leur teinte finiraient par en montrer deux differentes.
    // LA FAMILLE OU LE JOUEUR EST LE PLUS FORT, et celle ou il resiste - pendant CETTE partie.
    // =================================================================================================================
    // LE BOSS
    // La derniere question d'une Arcade est la NOTE ETRANGERE, et Roger l'a voulue comme un combat de fin : « le chien
    // levite lentement en plein milieu de la page, et tout le fond devient en fondu doux rouge feu ».
    // LE COMPTE QUI GRIMPE, ET SON BRUITAGE.
    // Roger : « une animation sur les nombres en mode nombre qui s'incremente tres vite jusqu'au nombre atteint », puis
    // « un bruitage de jeux video gling gling gling, ou de machine a sous quand ces chiffres s'incrementent ... et un
    // bruitage ou melodie ou accord de victoire ». Et il l'assume pour ce que c'est : « c'est juste un bruitage pour
    // rendre le jeu moins austere, et faire appel a des biais cognitifs d'addiction, comme dans les machines a sous ».
    // LE TIC : un minuteur plutot qu'un appel par image.
    // Le nombre AFFICHE, a un instant donne du compte : la valeur finale, multipliee par l'avancement. Arrondi, parce
    // qu'un compteur montre des entiers - et c'est l'arrondi qui fait le dechiffrement rapide qu'on vient chercher.
    // LE CONSEIL DE SORTIE, choisi d'apres la famille qui a le plus COUTE.
    // Roger : « a la fin du mode arcade, en fonction de la ou il y a le plus d'erreur, je lui mettrai une phrase
    // supplementaire speciale [...] 2-3 phrases dans le genre, qui tourneraient de maniere aleatoire en fonction de la
    // famille la plus ratee. Et si tout est parfait, un "Wow, tu peux clairement envisager de passer a l'etape
    // suivante !" »

    id: exerciseScreen

    // There is still no "next" button: a loop the player has to carry forward themselves feels slower
    // than it is. A tap anywhere skips the pause instead - free, and always available.
    readonly property int successPause: 1200
    readonly property int mistakePause: 1800
    // Elle vient du CONTROLEUR, donc du domaine et de son tempo : une mesure a 90 bpm dure deux secondes et demie, et
    // aucune constante ecrite ici ne saurait le dire sans mentir le jour ou le tempo change.
    readonly property int rhythmPause: ExerciseController.rhythmCellDurationMs + 600
    // Les modes ont BEAUCOUP plus de son que les autres questions : une gamme entiere, encadree par son bourdon, et deux
    // fois quand deux modes se comparent. La pause doit donc couvrir la LECTURE ENTIERE plus le temps de lire le verdict -
    // sinon elle avance au milieu du son, ce que Roger a vu tout de suite : « pour les modes, ca va beaucoup trop vite,
    // le son se coupe en plein milieu, t'as pas le temps de lire ».
    readonly property int modePause: ExerciseController.modeSoundDurationMs + 3000
    // Horizontal offset of the whole screen during the shake. Zero is the resting state, and it is both
    // where the animation starts and where it ends.
    property real shakeOffset: 0
    // UN SEUL compte pour TOUS les nombres - XP, serie, pourcentages. Un compteur par nombre les ferait arriver les uns
    // apres les autres, alors que le plaisir est justement de les voir grimper ENSEMBLE.
    property real rewardCountUp: 0
    // Vrai des que le compte de CETTE fin de partie a demarre : c'est ce qui empeche `sessionChanged`, qui passe a
    // chaque question, de relancer l'animation pendant qu'elle tourne.
    property bool rewardCountUpStarted: false
    readonly property var answeredInterval: ExerciseController.answeredInterval
    // Both checks, and not just the second one: reading a property of something that does not exist yet
    // is an error in QML, and a screen must never be one binding away from throwing.
    readonly property bool hasAnswered: answeredInterval !== undefined && answeredInterval.identifier !== undefined
    // La meme chose pour un accord : ce que le joueur vient de repondre, et s'il y a quelque chose a montrer.
    readonly property var answeredChordData: ExerciseController.answeredChord
    readonly property bool hasAnsweredChord: answeredChordData !== undefined && answeredChordData.name !== undefined

    // La comparaison porte sur un INDICE et non sur un nom : le jour ou l'ordre des familles change, rien ne se casse en
    // silence. Et le tirage est au hasard PARMI les phrases de la famille concernee : deux parties qui ratent les memes
    // modes ne disent pas exactement la meme chose.
    function closingAdvice() {
        var results = ExerciseController.familyResults();
        if (results.length === 0)
            return "";

        var worst = results[0];
        var totalErrors = 0;
        for (var i = 0; i < results.length; ++i) {
            totalErrors += results[i].errors;
            if (results[i].errors > worst.errors)
                worst = results[i];

        }
        if (totalErrors === 0)
            return qsTr("Wow ! Tu peux clairement envisager de passer à l'étape suivante.");

        var lines;
        if (worst.family === 0)
            lines = [qsTr("Les intervalles t'ont coûté cher : l'École des Chiots t'apprend à les reconnaître."), qsTr("Un tour dans « Intervalles », à l'entraînement, et ils rentreront tout seuls.")];
        else if (worst.family === 1)
            lines = [qsTr("Les accords t'ont résisté : l'arbre des accords, dans ton profil, les montre tous."), qsTr("Essaie la famille « Accords » à l'entraînement : là, se tromper ne coûte rien.")];
        else
            lines = [qsTr("Les modes t'ont résisté : l'École des Chiots dit ce qu'ils sont et comment les entendre."), qsTr("Un entraînement « Modes » t'aidera plus que dix Arcades.")];
        return lines[Math.floor(Math.random() * lines.length)];
    }

    function rewardValue(p_final) {
        return Math.round(p_final * rewardCountUp);
    }

    // LE CHRONO, en minutes:secondes. Une fonction plutot qu'une expression : elle sert une fois aujourd'hui et servira
    // partout ou l'on voudra dire un temps.
    function durationLabel(p_seconds) {
        var minutes = Math.floor(p_seconds / 60);
        var seconds = p_seconds % 60;
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds;
    }

    // Calcule sur les resultats de la session, jamais sur le journal : c'est ce que la partie vient de montrer, et c'est ce
    // qui rend le mot vrai au moment ou on le lit. La famille la plus faible n'est proposee que s'il y en a PLUSIEURS : sur
    // une partie d'un seul genre, la meilleure ET la pire seraient la meme ligne, dite deux fois.
    function strongestFamily() {
        var results = ExerciseController.familyResults();
        var best = null;
        for (var index = 0; index < results.length; ++index) {
            if (best === null || results[index].percent > best.percent)
                best = results[index];

        }
        return best;
    }

    function weakestFamily() {
        var results = ExerciseController.familyResults();
        if (results.length < 2)
            return null;

        var worst = null;
        for (var index = 0; index < results.length; ++index) {
            if (worst === null || results[index].percent < worst.percent)
                worst = results[index];

        }
        return worst;
    }

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

        // Le chant : au-dela du nom de l'intervalle, ce qui compte est de combien il est PROPRE. L'ecart en cents est
        // une mesure, pas un jugement : il dit la meme chose a celui qui progresse et a celui qui plafonne, et il est
        // mesure contre le temperament du jeu.
        if (ExerciseController.questionKind === 2) {
            var sungVerdict = qsTr("%1 (%2)").arg(heard.name).arg(heard.identifier);
            // L'ecart du chant vient du CONTROLEUR D'EXERCICE, et non du micro : celui-ci est deja resynchronise sur
            // la question suivante au moment ou le verdict s'affiche, et sa mesure a ete remise a zero.
            var sungCents = Math.round(ExerciseController.lastSungCentsOffset);
            sungVerdict += qsTr(" — écart %1 cents").arg((sungCents > 0 ? "+" : "") + sungCents);
            return sungVerdict;
        }
        var verdict = qsTr("%1 (%2)").arg(heard.name).arg(heard.identifier);
        if (exerciseScreen.hasAnswered && !ExerciseController.wasLastAnswerCorrect)
            verdict += qsTr(" — tu as répondu %1").arg(exerciseScreen.answeredInterval.identifier);

        return verdict;
    }

    NumberAnimation {
        id: rewardCountUpAnimation

        target: exerciseScreen
        property: "rewardCountUp"
        from: 0
        to: 1
        // Assez long pour qu'on ait le temps de voir le chiffre grimper, assez court pour qu'on ne s'impatiente pas
        // devant un ecran qui a deja tout dit.
        duration: 1300
        // L'essentiel du chemin se fait au debut : le chiffre part vite et se pose. C'est la courbe d'une machine a
        // sous, pas celle d'un ascenseur.
        easing.type: Easing.OutCubic
        onStopped: {
            // LA FANFARE arrive QUAND LE COMPTE ARRIVE, et seulement s'il est alle au bout : une animation interrompue -
            // l'ecran quitte, une nouvelle partie lancee - ne doit pas sonner comme une victoire.
            if (exerciseScreen.rewardCountUp >= 1)
                ExerciseController.playVictoryFanfare();

        }
    }

    // Par image, le tic suivrait le rafraichissement de l'ecran - soixante par seconde sur ce telephone - et le
    // bruitage deviendrait un bourdonnement continu. Trente-huit millisecondes font une vingtaine de crans par seconde :
    // assez pour que ca crepite, pas assez pour que ca se confonde.
    Timer {
        interval: 38
        repeat: true
        running: rewardCountUpAnimation.running
        onTriggered: ExerciseController.playScoreTick(Math.round(exerciseScreen.rewardCountUp * 100))
    }

    Connections {
        function onSessionChanged() {
            // Une nouvelle partie remet le compteur a zero : le gain de la precedente ne doit pas rester affiche.
            if (!ExerciseController.isFinished) {
                exerciseScreen.rewardCountUpStarted = false;
                exerciseScreen.rewardCountUp = 0;
                return ;
            }
            if (exerciseScreen.rewardCountUpStarted)
                return ;

            exerciseScreen.rewardCountUpStarted = true;
            rewardCountUpAnimation.restart();
        }

        target: ExerciseController
    }
    // The interval the player chose, when there is one.

    Timer {
        // Read when the timer is RESTARTED, which happens the moment the verdict appears: the pause
        // therefore matches the verdict it is giving time to.

        id: nextQuestionTimer

        // Une question de rythme a la sienne, et elle est plus longue : le feedback y est la CELLULE elle-meme, qui
        // dure une mesure entiere. Couper avant la fin couperait le son qui vient d'etre donne en reponse.
        interval: ExerciseController.isHarmonyQuestion ? exerciseScreen.modePause : (ExerciseController.questionKind === 3 ? exerciseScreen.rhythmPause : (ExerciseController.wasLastAnswerCorrect ? exerciseScreen.successPause : exerciseScreen.mistakePause))
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

    // IL SE POSE DERRIERE TOUT : c'est un DECOR, pas une couche. Le laisser devant cacherait la question, et le joueur ne
    // pourrait plus repondre a ce qu'on lui demande. Un Item sans gestionnaire de souris ne consomme aucun clic, donc le
    // « touche pour passer » continue de marcher a travers lui.
    // =================================================================================================================
    Rectangle {
        id: bossBackdrop

        anchors.fill: parent
        // Il ne s'allume que pour la question du boss, et jamais une fois la partie finie : le rouge doit s'eteindre.
        visible: opacity > 0
        opacity: (ExerciseController.isForeignNoteQuestion && !ExerciseController.isFinished) ? 1 : 0

        Image {
            id: bossImage

            // Position de repos : au MILIEU de la page, comme Roger l'a imagine.
            readonly property real restingY: (parent.height - height) / 2

            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(parent.width * 0.58, 260)
            height: width
            fillMode: Image.PreserveAspectFit
            source: "qrc:/assets/images/shibaBoss.png"
            opacity: 0.9
            y: bossImage.restingY

            // ET IL LEVITE : un va-et-vient lent, de bas en haut, sans fin. Deux animations enchainees, parce qu'une
            // seule ne saurait pas revenir.
            SequentialAnimation on y {
                loops: Animation.Infinite
                running: bossBackdrop.visible

                NumberAnimation {
                    from: bossImage.restingY - 16
                    to: bossImage.restingY + 16
                    duration: 2800
                    easing.type: Easing.InOutSine
                }

                NumberAnimation {
                    from: bossImage.restingY + 16
                    to: bossImage.restingY - 16
                    duration: 2800
                    easing.type: Easing.InOutSine
                }

            }

        }

        gradient: Gradient {
            GradientStop {
                position: 0
                color: "#3a0c11"
            }

            GradientStop {
                position: 1
                color: "#7c1a1c"
            }

        }

        // LE FONDU, dans les deux sens : « un fondu doux » a l'arrivee, et le retour a la nuit violette quand le boss
        // tombe. Sans ce retour, la page resterait rouge pour la suite du jeu.
        Behavior on opacity {
            NumberAnimation {
                duration: 900
                easing.type: Easing.InOutQuad
            }

        }

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
            // -------------------------------------------------------------------------------------------------
            // L'INDICE d'accord
            // Il dit ce qui a ete ENTENDU, jamais ce qui a ete touche : les deux peuvent differer, et quand c'est le
            // cas, c'est au joueur de le decouvrir. C'est la raison pour laquelle la reponse affichee est un fait du
            // domaine, et non ce que l'ecran croit savoir.
            // -------------------------------------------------------------------------------------------------
            // Le verdict, le mot du bilan, et le prompt qui les remplace avant la reponse.
            // Sa hauteur est FIXE, et c'est ce qui compte ici : un item qui prend l'espace libre pousse les boutons de
            // reponse hors de l'ecran. C'est la ZONE DE REPONSE qui prend la place, et elle se pose au milieu.
            // -------------------------------------------------------------------------------------------------
            // LES ACTIONS, en bas de l'ecran, et rien d'autre
            // Les reponses se posent au CENTRE, les actions restent en bas : cette rangee est donc la SEULE chose sous
            // la zone de reponse, et tout ce qui n'est pas une reponse y est - ecouter, l'indice, l'arpege, et voir la
            // reponse.
            // Les libelles sont COURTS : trois ou quatre boutons sur la largeur d'un telephone ne tiennent pas avec des
            // phrases, et un bouton coupe en deux n'est pas un bouton.
            // Le tout TIENT DANS L'ECRAN : une page de jeu qui se fait scroller cache ses propres reponses, et un
            // joueur de haut niveau peut avoir QUINZE couleurs a l'ecran (les six triades, les trois septiemes, la
            // sixte, le demi-diminu, le diminue 7, le mineur-majeur, add9 et la neuvieme). Trois colonnes et des
            // etiquettes courtes, voila comment quinze reponses tiennent sur un telephone.
            // -------------------------------------------------------------------------------------------------
            // L'ARBRE DES ACCORDS, pour REPONDRE.
            // Les couleurs de la palette sont ALLUMEES et cliquables, les autres restent visibles en sombre : le schema
            // des mutations reste lisible, et l'oeil ne peut pas confondre une reponse possible avec un bouton eteint.
            // Repondre dans l'arbre, c'est deja comprendre l'accord qu'on cherche - c'est mieux qu'une grille de noms.
            // L'INDICE et l'ARPEGE apparaissent apres un PREMIER essai rate, et seulement quand le domaine dit qu'ils ont
            // un sens. L'arpege est le meilleur des deux : il ne donne pas la reponse, il donne a entendre ce qui la
            // constitue.
            // -------------------------------------------------------------------------------------------------
            // L'ANECDOTE DE LA QUESTION, juste au-dessus des actions : assez haute pour se lire entre deux questions,
            // assez basse pour ne pas voler la place de la reponse - qui reste au centre, et c'est elle qui compte.
            // -------------------------------------------------------------------------------------------------
            // LA QUESTION D'HARMONIE
            // Deux formes, et deux seulement, parce que ce sont DEUX questions :
            //   * la COULEUR : deux boutons, « plus clair » et « plus sombre ». Rien a nommer, rien a savoir d'avance :
            //     comparer deux choses entendues dans la foulee est un travail d'oreille, et c'est la premiere marche ;
            //   * le NOM : un bouton par mode de la palette, peints par leur CLARTE - l'ecran montre alors exactement ce
            //     que l'oreille vient d'entendre, et l'oeil apprend l'axe en meme temps que l'oreille.

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
                // LES COEURS. Un par vie, et ils passent a la ligne quand la partie en accorde beaucoup : avec le reglage
                // qui monte jusqu'a vingt-cinq, une seule ligne de coeurs deborderait de l'ecran.
                // Les coeurs sont poses sur la MEME ligne que « Quitter », a droite - et leur largeur est CALCULEE, jamais
                // prise a celle du dessin.

                Layout.fillWidth: true

                Button {
                    // Flat, and READABLE - which it was not.

                    id: quitButton

                    // A flat Material button takes its text colour from the style, and the style knows nothing
                    // about the gradient painted behind it: the default is almost black on a night blue
                    // background. The colour is therefore stated here, exactly as the answer buttons state
                    // theirs, and a discreet outline gives the tap somewhere to land.
                    flat: true
                    text: qsTr("← Quitter")
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

                // C'etait le bug : la largeur venait de `implicitWidth`, qui vaut ZERO tant que le Repeater n'a rien
                // construit. La rangee recevait donc une largeur nulle au premier passage, les coeurs se posaient UN PAR
                // LIGNE - une colonne - et cette colonne poussait la zone de jeu vers le haut. Roger l'a vu tout de suite.
                Flow {
                    id: heartsFlow

                    // La taille d'un coeur, et la largeur d'une ligne : au-dela de dix, ils retrecissent et se replient
                    // sur DEUX lignes.
                    readonly property int heartCount: ExerciseController.hasUnlimitedLives ? 1 : Math.max(1, ExerciseController.lives)
                    readonly property int heartSize: heartCount > 10 ? 13 : 21
                    readonly property real heartsWidth: (heartCount > 10 ? Math.ceil(heartCount / 2) : heartCount) * heartSize

                    Layout.alignment: Qt.AlignRight
                    Layout.preferredWidth: heartsWidth
                    // ET LA LARGEUR EST TENUE : sans un minimum, QtQuick.Layouts peut reduire la rangee a la largeur d'un
                    // seul coeur, et le repli se fait alors une note par ligne.
                    Layout.minimumWidth: heartsWidth
                    Layout.maximumWidth: heartsWidth
                    spacing: 1

                    Text {
                        visible: ExerciseController.hasUnlimitedLives
                        text: "∞"
                        color: "#ff8fa3"
                        font.pixelSize: 20
                    }

                    Repeater {
                        model: ExerciseController.hasUnlimitedLives ? 0 : ExerciseController.lives

                        delegate: Text {
                            text: "♥"
                            color: "#ff8fa3"
                            font.pixelSize: heartsFlow.heartSize
                        }

                    }

                }

            }

            // Les trois textes s'empilent dans une colonne CENTREE plutot que de se superposer : deux d'entre eux peuvent
            // etre visibles en meme temps - le verdict et le mot d'encouragement du bilan - et deux Text ancres au meme
            // centre se seraient ecrits l'un sur l'autre.
            Item {
                id: promptBoard

                Layout.fillWidth: true
                // La hauteur SUIT le contenu : une hauteur fixe laissait le verdict, le mot du bilan et le prompt se
                // CHEVAUCHER avec ce qui suit des que l'un des trois passait a deux lignes. Une zone qui s'adapte ne
                // peut pas deborder.
                Layout.preferredHeight: promptColumn.implicitHeight + 8
                Layout.minimumHeight: 56

                ColumnLayout {
                    // Le BILAN : son mot, sous le verdict. Vide hors bilan, et ce n'est pas un detail - un ecran qui parle
                    // pour ne rien dire devient un ecran qu'on n'ecoute plus, et le silence est ce qui donne du poids aux
                    // mots qui restent (voir encouragementText, cote controleeur).
                    // LES DEGRES DU MODE, SOUS SON NOM.
                    // Roger : « j'ecrirai dans la ligne juste en dessous, les notes qu'il y a dedans en degre [...] ecrit
                    // avec les memes couleurs de degrade que les boutons, et le rouge pour la note caracteristique.
                    // Histoire d'avoir un repere pour l'utilisateur. »

                    id: promptColumn

                    anchors.centerIn: parent
                    width: parent.width
                    spacing: 4

                    // It says what was HEARD, never what was tapped: the two can differ, and when they do, the
                    // player has to find out. That is the whole reason the answer displayed is a fact of the domain.
                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        visible: ExerciseController.isFeedbackVisible
                        text: exerciseScreen.verdictText()
                        color: ExerciseController.wasLastAnswerCorrect ? "#8ef2b0" : "#ffd479"
                        font.pixelSize: 19
                        font.bold: true
                    }

                    // Il est GROS : c'est un mot qu'on doit lire sans le chercher, au moment ou l'on vient de repondre.
                    // Il est aussi le seul texte de l'ecran qui s'adresse directement au joueur.
                    Text {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 0
                        Layout.minimumWidth: 0
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        visible: ExerciseController.encouragementText !== ""
                        text: ExerciseController.encouragementText
                        color: "#ffd479"
                        font.pixelSize: 20
                        font.bold: true
                    }

                    Text {
                        // Le "Écoute bien…" est le prompt des questions d'OREILLE. Sur une question de rythme, c'est la
                        // zone rythmique qui dit ou en est la boucle, et le meme mot y serait faux la moitie du temps.
                        // LE MODE EST ANNONCE, et c'est une correction de Roger : « il faut afficher le mode qui est en
                        // train d'etre joue des le debut, car sans rien c'est beaucoup trop difficile - il doit a la fois
                        // trouver le mode ainsi que la note qui ne va pas ». La gamme jouee EST le mode, donc le nommer
                        // n'enleve rien a l'exercice : il enleve une devinette.

                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        // Sur une question d'HARMONIE, il faut dire QUOI ecouter : deux modes a comparer, ou un seul a
                        // nommer. Un prompt generique laisserait le joueur chercher ce qu'on lui demande, et c'est
                        // exactement ce qu'une consigne ne doit pas faire.
                        visible: !ExerciseController.isFeedbackVisible && ExerciseController.questionKind !== 3 && ExerciseController.questionKind !== 4
                        text: {
                            // Le VAMP : deux fois la meme gamme, sur deux centres differents. La consigne doit le dire,
                            // parce que c'est justement ce que le joueur ne peut pas deviner - il entend deux fois les
                            // memes notes, et c'est pourtant deux modes.
                            if (ExerciseController.isModeVampQuestion)
                                return qsTr("Deux fois la même gamme, sur deux centres différents : le second passage est-il plus clair, ou plus obscur ?");

                            if (ExerciseController.isForeignNoteQuestion)
                                return qsTr("Sept notes montent en %1 sur le bourdon : l'une n'appartient pas à la gamme. Laquelle ?").arg(ExerciseController.heardMode.name);

                            if (ExerciseController.isModeColourQuestion)
                                return qsTr("Écoute les deux modes : le second est-il plus clair, plus obscur, ou pareil ?");

                            if (ExerciseController.isModeQuestion)
                                return qsTr("Écoute ce mode sur son bourdon : lequel est-ce ?");

                            return qsTr("Écoute bien…");
                        }
                        color: "#cbb8e8"
                        font.pixelSize: 17
                    }

                    // C'EST LE MODE VRAI, jamais celui que la question joue modifie : c'est un RAPPEL, pour que le joueur
                    // puisse comparer ce qu'il entend a ce qu'il sait du mode. Et la teinte est prise a la MEME source que
                    // les boutons de modes - la clarte du mode - pour que les deux se repondent d'un coup d'oeil.
                    Row {
                        // La couleur sort de la meme formule que les boutons de modes : une seule teinte a maintenir, et
                        // le mode le plus clair reste le plus clair partout.
                        readonly property color badgeColour: Qt.rgba(0.3 + (0.7 * ExerciseController.heardMode.brightness), 0.2 + (0.62 * ExerciseController.heardMode.brightness), 0.55 + (0.45 * ExerciseController.heardMode.brightness), 1)

                        Layout.alignment: Qt.AlignHCenter
                        spacing: 5
                        visible: ExerciseController.isForeignNoteQuestion

                        Repeater {
                            model: ExerciseController.isForeignNoteQuestion ? ExerciseController.heardMode.degrees : []

                            delegate: Rectangle {
                                required property var modelData

                                width: 32
                                height: 30
                                radius: 6
                                // LE ROUGE DE LA NOTE CARACTERISTIQUE : c'est celle qui donne au mode sa couleur, et
                                // c'est celle qu'un joueur doit apprendre a entendre. Le domaine la designe - elle n'est
                                // JAMAIS recalculee ici, sinon deux copies finiraient par se contredire.
                                color: modelData.isCharacteristic ? "#d43a3a" : parent.badgeColour

                                Text {
                                    anchors.centerIn: parent
                                    color: "#ffffff"
                                    font.pixelSize: 14
                                    font.bold: modelData.isCharacteristic
                                    text: modelData.label
                                }

                            }

                        }

                    }

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
                // CE QUE LE MICRO ENTEND, EN DIRECT : la note, sa frequence, et l'ecart en cents.

                Layout.fillWidth: true
                visible: ExerciseController.questionKind === 2
                spacing: 10
                // Le micro suit l'ecran qui s'en sert : il s'ouvre quand la question se chante, et se referme quand
                // elle se tait. Un exercice passe la plupart de son temps sans chant, et un micro ouvert pour rien
                // vide la batterie.
                onVisibleChanged: visible ? MicrophoneController.ensureListening() : MicrophoneController.stopTest()

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
                    // LA BOULE FANTOME S'ALLUME DES LA PREMIERE ERREUR, et seulement a la DEUXIEME note : c'est le moment ou
                    // le joueur a besoin de savoir ou poser sa voix, et pas avant - une aide qui arrive trop tot chante a
                    // sa place.
                    showGhost: ExerciseController.singingGhostIsVisible && MicrophoneController.hasFirstNote
                }

                // Roger : « est-ce qu'on pourrait aussi afficher la note en train d'etre jouee, exactement comme sur
                // l'accordeur ? Ca me permettrait de deboguer la vraie valeur affichee. » Et il a raison d'ajouter que ce
                // n'est pas une triche : savoir QUELLE note on vient de chanter ne dit pas de combien on s'est trompe.
                // L'ecart en cents, lui, est deja ce que le jeu juge - le montrer ne donne donc aucune reponse, il rend
                // la mesure lisible.
                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    visible: MicrophoneController.detectedFrequencyHz > 0
                    color: "#8a77ad"
                    font.pixelSize: 13
                    text: qsTr("%1  ·  %2 cents").arg(MicrophoneController.detectedNoteLabel).arg(Math.round(MicrophoneController.detectedCents))
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
                        text: qsTr("♪ Écouter")
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
                            ExerciseController.answerSung(MicrophoneController.sungVerdict === 1, MicrophoneController.sungCentsOffset);

                    }

                    target: MicrophoneController
                }

            }

            // Douze places, trente degres chacune, la premiere a midi : do en haut, puis les quintes dans le sens
            // des aiguilles d'une montre. C'est exactement la disposition d'un vrai cercle des quintes.
            // L'ESPACE DE LA REPONSE : c'est LUI qui prend la place libre de l'ecran, et la reponse se pose au MILIEU de
            // cet espace, et non en bas de l'ecran.
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: ExerciseController.questionKind === 0

                Item {
                    id: circleBoard

                    // Un CARRE au centre : le cercle des quintes est rond, et un rond dans un rectangle se pose au
                    // milieu. Son cote suit la plus PETITE des deux dimensions, donc il ne deborde jamais - ce qui etait
                    // exactement le defaut d'un carre dont la hauteur valait la largeur sur un ecran plus large que haut.
                    readonly property real side: Math.min(parent.width, parent.height)
                    readonly property real ringRadius: width * 0.36
                    readonly property real slotWidth: width * 0.19
                    readonly property real slotHeight: width * 0.19

                    width: side
                    height: side
                    anchors.centerIn: parent

                    Repeater {
                        // Un seul Repeater, PLAT : une entree par bouton. Chaque bouton porte sa place (l'angle), son octave
                        // (la couche) et son identifiant.
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
                        // LA POSITION VIENT DU SON, et non d'un minuteur de cet ecran : c'est la seule facon que le
                        // curseur soit d'accord avec ce que le joueur ENTEND (voir rhythmPositionInBar). Un entier qui
                        // saute d'un temps a l'autre ne pouvait pas etre suivi du regard - c'est la remarque de Roger :
                        // « le son n'est pas synchro avec la note jouee ».

                        id: rhythmCursor

                        // La minuterie ne FABRIQUE pas la position, elle la RELIT : l'affichage a besoin d'une image
                        // souvent, le son a besoin d'etre la seule autorite. Vingt millisecondes, c'est le
                        // rafraichissement d'un ecran.
                        y: 0
                        width: 3
                        height: rhythmBar.height
                        color: ExerciseController.isRhythmPlaying ? "#8ef2b0" : "#6f5c96"

                        Timer {
                            interval: 20
                            repeat: true
                            running: ExerciseController.questionKind === 3
                            onTriggered: rhythmCursor.x = (ExerciseController.rhythmPositionInBar / rhythmBar.beatsPerBar) * rhythmBar.width
                        }

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

            // Le mode qui vient de sonner est affiche au-dessus, en vert : c'est le lien entre ce que l'oreille entend et
            // ce que la theorie en dit, au moment ou elle l'entend.
            // -------------------------------------------------------------------------------------------------
            ColumnLayout {
                // La ROUE du mode : ses sept notes allumées sur le cercle des quintes, la tonique en haut.
                // LA NOTE ETRANGERE : les sept notes de la gamme, dans l'ordre entendu, et l'intrus se designe par sa PLACE.
                // Les boutons de la note etrangere ont DISPARU : c'est la roue du dessus qui repond, depuis qu'elle est
                // cliquable. Une rangee de sept boutons plats disait la meme chose en plus petit, et son texte etait elide
                // jusqu'au « ... » - Roger l'a vu jouer : « on voit ... au lieu de la note a l'interieur ».
                // LE BOURDON, DIT AU JOUEUR - c'est une des deux questions qui reviennent le plus, et le jeu ne la posait
                // jamais. Roger : « c'est quoi le bourdon (il faut lui expliquer les degres qu'on utilise) ».

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignVCenter
                visible: ExerciseController.isHarmonyQuestion
                spacing: 10

                Text {
                    // Le VERDICT, et il est en GRAS parce que le temps de lecture est court.
                    // Deux noms poses cote a cote ne disent rien : ce qu'on a entendu, c'est UNE NOTE qui a bouge, et
                    // c'est donc cela qu'il faut ecrire - « de dorien a ionien, la tierce a monte ».

                    id: modeVerdict

                    // Et quand RIEN n'a bouge - deux fois la meme couleur - il faut le dire aussi : c'est la seule
                    // reponse possible, et un verdict muet laisserait croire a un bug.
                    readonly property bool sameness: ExerciseController.previousMode.name !== undefined && ExerciseController.heardMode.name !== undefined && (ExerciseController.previousMode.name === ExerciseController.heardMode.name)

                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: "#ffd479"
                    font.pixelSize: 19
                    font.bold: true
                    visible: !ExerciseController.isAsking && ExerciseController.heardMode.name !== undefined
                    text: {
                        if (!visible)
                            return "";

                        // Un SEUL mode a sonne - une question de nom : il n'y a rien a comparer, et « undefined -> Dorien »
                        // n'aurait aucun sens. C'est ce que Roger voyait a la victoire, sur une question de nom.
                        if (ExerciseController.previousMode.name === undefined)
                            return ExerciseController.heardMode.name + " — " + ExerciseController.heardMode.characteristic;

                        if (sameness)
                            return qsTr("Les deux passages : %1 — pareils").arg(ExerciseController.heardMode.name);

                        return ExerciseController.previousMode.name + " → " + ExerciseController.heardMode.name + "  ·  " + ExerciseController.modeDifference.label;
                    }
                }

                Text {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: "#8ef2b0"
                    font.pixelSize: 14
                    // Trois cas ou il n'y a rien a dire ici : la question est encore posee, les deux passages etaient
                    // pareils, ou un seul mode a sonne - et sa caracteristique est deja ecrite dans le verdict.
                    visible: modeVerdict.visible && !modeVerdict.sameness && ExerciseController.previousMode.name !== undefined
                    text: modeVerdict.visible && !modeVerdict.sameness && ExerciseController.previousMode.name !== undefined ? ExerciseController.modeDifference.sentence : ""
                }

                // Le DEGRE est ecrit noir sur blanc parce que c'est exactement ce qu'il demande : le bourdon n'est pas
                // « une note au hasard sous la gamme », c'est LA TONIQUE - le degre 1 - et c'est ce qui en fait un centre.
                Text {
                    // La roue n'existe que sur une question de mode : c'est donc elle qui dit quand expliquer.

                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
                    Layout.bottomMargin: 2
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: "#8a77ad"
                    font.pixelSize: 11
                    // COURT, et volontairement : Roger a trouve le premier texte trop long - « ca a tendance a descendre
                    // tout le cercle et les boutons d'actions, ce qui est dommage ». Une explication qui pousse les
                    // commandes hors de portee coute plus qu'elle n'apprend. Deux lignes au maximum.
                    visible: ExerciseController.modeCircle.length > 0
                    text: qsTr("Le bourdon : la tonique et sa quinte, tenues sous la gamme. C'est lui qui donne le centre.")
                }

                // Elle est là PENDANT la question, et c'est un choix de Roger : « je mettrais quand même la roue dans la
                // question, l'utilisateur pourra ne pas trop la regarder ». Elle donne le mode à qui sait la lire - et
                // c'est justement ce qu'on veut apprendre.
                // LA ROUE NE TOURNE PLUS : son repère reste la tonique du PREMIER mode quand deux modes s'enchaînent, et
                // le second est dessiné DEDANS. Roger l'a demandé après avoir vu deux modes relatifs donner exactement
                // la même image : « même si la tonique du premier reste en haut, la tonique deviendrait un autre bouton
                // dans le cercle ». Une image qui tourne avec la tonique ne pouvait pas le montrer.
                // La ROUE, et c'est elle qui REPOND sur une question de note etrangere : ses sept notes allumees sont
                // exactement les sept pas de la gamme, donc appuyer sur l'une d'elles designe l'intrus. Roger l'a demande -
                // « il suffit d'appuyer sur un de ces boutons non ? » - et il a raison : les pastilles sont plus grandes que
                // les petits boutons de note qu'elles remplacent, et elles sont deja la pour montrer la gamme.
                ModeCircle {
                    // LA ROUE S'ANIME : sa tete part de la tonique et parcourt la gamme, de note en note, pendant que la
                    // musique joue. Roger l'a voulue ici - « dans les exercices, quand on affiche les modes dans leur
                    // cercle » - parce qu'elle dit QUELLE note sonne, et qu'a la fin le chemin laisse une forme.
                    // LA ROUE S'OXYGENE : Roger l'a vue jouer - « les points sont pas assez espaces pour que ca rende
                    // vraiment bien ». Une case fait trente degres, donc l'ecart entre deux pastilles vaut environ la
                    // moitie du rayon : la seule facon d'ouvrir cet ecart est de faire GRANDIR le cercle ou de reduire les
                    // pastilles. Les deux ont ete poussees d'un cran, et d'un seul : un rayon plus grand demande de la
                    // hauteur, et l'ecran n'en a pas beaucoup.
                    // La gamme de l'exercice MONTE, puis se REFERME sur sa tonique : le chemin fait donc le tour du
                    // cercle et revient a son point de depart, ce qui referme la figure - et c'est la figure qui reste a
                    // l'ecran une fois la musique finie.

                    id: modeCircle

                    Layout.alignment: Qt.AlignHCenter
                    // Un souffle, et pas davantage : la roue est le sujet de la question, et chaque pixel pris au-dessus
                    // d'elle est un pixel retire au dessin et aux boutons.
                    Layout.topMargin: 2
                    // Les pastilles restent grandes : elles sont CLIQUEES sur une question de note etrangere, et une cible
                    // qui retrecit trop fait rater la note qu'on visait.
                    span: 304
                    dotSize: 46
                    visible: ExerciseController.isHarmonyQuestion
                    selectable: ExerciseController.isForeignNoteQuestion
                    notes: ExerciseController.modeCircle
                    // L'INTRUS N'EST MARQUE QU'APRES LA REPONSE, et c'est le verdict qui le dit : `foreignNoteVerdict`
                    // n'existe qu'une fois la reponse donnee, donc la couleur ne peut pas vendre la meche. Le pas est
                    // compte de 1 a 7 par le domaine, et de 0 a 6 dans la roue - d'ou le retrait.
                    foreignStep: ExerciseController.foreignNoteVerdict.stepNumber !== undefined ? ExerciseController.foreignNoteVerdict.stepNumber - 1 : -1
                    onNoteChosen: (p_stepIndex) => {
                        ExerciseController.answerForeignNote(p_stepIndex);
                    }

                    // Le DERNIER degre est 0, et non 7 : le septieme degre du domaine EST le premier degre du cercle,
                    // une octave plus haut. Le cercle ne porte que sept notes, donc la note qui ferme la gamme est
                    // celle par laquelle elle a commence.
                    Connections {
                        // LA ROUE SUIT LE PASSAGE QUI SONNE, et elle le relit ICI.

                        function onModePlaybackStarted() {
                            // `modeCircle` change de valeur quand le second passage d'une comparaison commence - c'est
                            // tout le sujet : le premier mode se dessine pendant qu'il sonne, le second prend sa place
                            // ensuite. La lecture est donc IMPERATIVE : la donnee n'a pas de signal a elle, et une
                            // liaison declarative ne se recalculerait pas. Le signal part au meme moment que le son,
                            // donc le dessin et la musique changent ensemble.
                            modeCircle.notes = ExerciseController.modeCircle;
                            modeCircle.startPlayback(ExerciseController.modeSoundLeadInMs, ExerciseController.modeSoundNoteStepMs, [0, 1, 2, 3, 4, 5, 6, 0]);
                        }

                        // Et la trainee S'EFFACE quand la question change : un chemin qui survivrait a sa musique montrerait
                        // un trajet que personne n'a entendu.
                        function onQuestionChanged() {
                            modeCircle.playing = false;
                        }

                        target: ExerciseController
                    }

                }

                // Le verdict de l'intrus : quel pas, et quelle note il portait au lieu de celle de la gamme.
                Text {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: "#ffd479"
                    font.pixelSize: 18
                    font.bold: true
                    visible: ExerciseController.foreignNoteVerdict.stepNumber !== undefined
                    text: ExerciseController.foreignNoteVerdict.stepNumber !== undefined ? qsTr("L'intrus : le pas %1 — %2 au lieu de %3").arg(ExerciseController.foreignNoteVerdict.stepNumber).arg(ExerciseController.foreignNoteVerdict.heardName).arg(ExerciseController.foreignNoteVerdict.expectedName) : ""
                }

                // Et DE QUOI elle parle : sur une question de couleur, deux modes ont sonne, et la roue est celle du
                // second. Sans cette ligne, Roger l'a dit en jouant : « on ne sait pas a qui correspond le cercle ».
                Text {
                    Layout.fillWidth: true
                    Layout.topMargin: 2
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: "#8a77ad"
                    font.pixelSize: 12
                    visible: ExerciseController.modeCircleLabel !== ""
                    text: ExerciseController.modeCircleLabel
                }

                // Le geste pour passer existe depuis toujours - un tap n'importe ou - mais personne ne peut le deviner,
                // et Roger ne l'a pas vu : « est-ce que c'est complique de rajouter du temps pour la reponse ? skipable
                // quand on appuie dessus ». La reponse est donc oui, et on le dit.
                Text {
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    horizontalAlignment: Text.AlignHCenter
                    color: "#6f5b93"
                    font.pixelSize: 11
                    visible: ExerciseController.isFeedbackVisible
                    text: qsTr("touche l'écran pour passer")
                }

                RowLayout {
                    // Le « neutre » au MILIEU, comme Roger le demandait : plus clair d'un cote, plus obscur de l'autre, et
                    // « pareil » entre les deux. Une reponse qui ne prend pas parti se lit mieux la ou elle est.

                    Layout.fillWidth: true
                    spacing: 10
                    visible: ExerciseController.isModeColourQuestion

                    // SAUF SUR UN VAMP, ou « pareil » n'existe pas : les deux passages y portent deux centres differents,
                    // donc deux modes, donc la reponse est TOUJOURS clair ou obscur. Un bouton qu'on ne peut pas gagner
                    // n'est pas un choix, c'est un piege - et c'est aussi ce qui permet de reconnaitre un vamp au premier
                    // coup d'oeil, ce que Roger ne pouvait pas faire : les deux questions se ressemblaient trop.
                    Button {
                        Layout.fillWidth: true
                        height: 56
                        text: qsTr("Plus clair")
                        enabled: ExerciseController.isAsking
                        onClicked: ExerciseController.answerModeColour(true)
                    }

                    Button {
                        Layout.fillWidth: true
                        height: 56
                        visible: !ExerciseController.isModeVampQuestion
                        text: qsTr("Pareil")
                        enabled: ExerciseController.isAsking
                        onClicked: ExerciseController.answerSameColour()
                    }

                    Button {
                        Layout.fillWidth: true
                        height: 56
                        text: qsTr("Plus obscur")
                        enabled: ExerciseController.isAsking
                        onClicked: ExerciseController.answerModeColour(false)
                    }

                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 6
                    visible: !ExerciseController.isModeColourQuestion

                    Repeater {
                        // LA LARGEUR, LA POLICE ET LES MARGES VONT ENSEMBLE, et c'est le NOM qui commande.
                        // UN DEGRADE PLUS CLAIR, PARCE QUE LE PLUS SOMBRE ETAIT PRESQUE NOIR.

                        model: ExerciseController.modeChoices

                        delegate: Button {
                            required property var modelData
                            readonly property bool isBright: modelData.brightness > 0.5
                            readonly property bool wasHeard: ExerciseController.heardMode.index !== undefined && modelData.index === ExerciseController.heardMode.index

                            // Roger : « le Mixolydien n'est pas ecrit en entier, on a des ... ». Un bouton de 104 pour une
                            // police de 14 laisse environ 72 points au texte - le style garde seize points de marge de
                            // chaque cote - et « Mixolydien » en demande quatre-vingts. Les trois valeurs sont donc
                            // reglees ENSEMBLE, sinon le prochain nom long les fera mentir de nouveau.
                            width: 118
                            height: 48
                            leftPadding: 4
                            rightPadding: 4
                            text: modelData.name
                            font.pixelSize: 12
                            enabled: ExerciseController.isAsking
                            highlighted: wasHeard
                            // Roger : « la couleur des boutons des modes, on m'a dit que c'est pas bien visible avec les
                            // couleurs sombres. Il faudrait peut-etre un degrade plus flachy et plus visible. » L'ancien
                            // degrade partait de (0,18 ; 0,14 ; 0,30) - un violet si profond qu'on ne lisait plus rien.
                            // Le plancher est remonte et l'amplitude elargie : le mode le plus sombre reste un violet
                            // franc, et le plus clair tire vers le rose.
                            Material.background: Qt.rgba(0.3 + (0.7 * modelData.brightness), 0.2 + (0.62 * modelData.brightness), 0.55 + (0.45 * modelData.brightness), 1)
                            Material.foreground: isBright ? "#1d1033" : "#ffffff"
                            onClicked: ExerciseController.answerModeName(modelData.index)
                        }

                    }

                }

            }

            // Le dessin vient de ChordTreeView.qml, partage avec la carte du profil : une seule definition du layout,
            // donc aucun risque de voir deux arbres differents selon l'ecran qui les montre.
            ChordTreeView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignVCenter
                visible: ExerciseController.isChordQuestion
                interactive: true
                showDegrees: false
                offeredQualities: ExerciseController.chordChoices
                onQualityChosen: function(p_quality) {
                    ExerciseController.answerChord(p_quality);
                }
            }

            // Elle change a CHAQUE question (voir refreshQuestionAnecdote), et le texte est vide quand le livre
            // d'anecdotes est vide : l'ecran ne montre alors rien du tout, ce qui vaut mieux qu'une ligne vide.
            Text {
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                Layout.minimumWidth: 0
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                // L'anecdote : de la DECORATION, et elle doit se lire comme telle. L'italique entre guillemets, comme le
                // bout de lore en italique des cartes Magic que Roger a en tete - on sait d'un coup d'oeil que ce n'est
                // pas une consigne, et qu'on peut la sauter sans rien perdre.
                maximumLineCount: 3
                elide: Text.ElideRight
                visible: ExerciseController.questionAnecdoteText !== ""
                text: ExerciseController.questionAnecdoteText !== "" ? qsTr("« %1 »").arg(ExerciseController.questionAnecdoteText) : ""
                color: "#8a77ad"
                font.pixelSize: 12
                font.italic: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Button {
                    Layout.fillWidth: true
                    height: 52
                    text: qsTr("♪ Écouter")
                    enabled: ExerciseController.isAsking
                    onClicked: ExerciseController.replay()
                }

                Button {
                    Layout.fillWidth: true
                    height: 52
                    visible: ExerciseController.isChordHintAvailable
                    text: qsTr("Indice")
                    enabled: ExerciseController.isAsking
                    onClicked: ExerciseController.useChordHint()
                }

                Button {
                    Layout.fillWidth: true
                    height: 52
                    // Visible des le premier essai rate, MEME quand il n'y a rien a retirer : avec deux couleurs, un
                    // debutant n'a aucune reponse a eliminer, et c'est pourtant la que l'arpege lui apprend le plus.
                    visible: ExerciseController.isChordArpeggioAvailable
                    text: qsTr("Arpège")
                    onClicked: ExerciseController.playCurrentChordAsArpeggio()
                }

                Button {
                    // Sur une question de rythme ou de CHANT, la question ne se "revele" pas : elle se PASSE. Le meme
                    // bouton, le meme domaine (revealAnswer), et un mot qui dit ce que le joueur fait vraiment.

                    Layout.fillWidth: true
                    height: 52
                    // Appears only once the player has tried enough. Asking to be told is not a failure, and
                    // it is not offered before it is useful either.
                    // Le bouton apparaît pour le rythme ET pour le chant, et le domaine decide quand :
                    // une question chantée l'offre dès la première seconde, parce qu'on peut ne pas être en
                    // mesure de chanter du tout.
                    visible: ExerciseController.isHelpAvailable
                    highlighted: true
                    // Le chant a besoin de ce bouton autant que le rythme, et pour une raison differente : on peut ne pas
                    // etre en mesure de chanter - un endroit bruyant, une gorge prise, un micro qui ne suit pas. Sans
                    // lui, la seule issue etait de rater la question, ce qui n'est pas la meme chose.
                    text: (ExerciseController.questionKind === 3 || ExerciseController.questionKind === 2) ? qsTr("Passer") : qsTr("Réponse")
                    onClicked: ExerciseController.revealAnswer()
                }

            }

        }

        // -----------------------------------------------------------------------------------------------------
        // The end of the session
        // -----------------------------------------------------------------------------------------------------
        ColumnLayout {
            // CE QUE LE BILAN VIENT DE RAPPORTER : les trophees tout neufs, et le titre s'il a monte.

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
            // Le chien de la fin : content quand la partie est gagnee, triste quand elle est perdue.

            // Il vient APRES l'etoile et AVANT les chiffres : l'etoile est la recompense du domaine, le chien est le mot
            // qu'on y ajoute, et un dessin dit « bravo » ou « pas cette fois » plus vite qu'une ligne de score.
            Image {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredHeight: 150
                fillMode: Image.PreserveAspectFit
                source: ExerciseController.wasSessionWon ? "qrc:/assets/images/chibaWin.png" : "qrc:/assets/images/chibaLose.png"
            }

            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: ExerciseController.sessionGrantsExperience ? qsTr("Arcade terminée") : qsTr("Session terminée")
                color: "#ffffff"
                font.pixelSize: 24
                font.bold: true
            }

            // LE BILAN DE L'ARCADE : ce qu'aucun autre mode ne montre, parce qu'aucun autre mode ne le gagne.
            ColumnLayout {
                // LES TROIS FAMILLES, ET CE QU'ELLES ONT COUTE.

                Layout.fillWidth: true
                visible: ExerciseController.sessionGrantsExperience
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        color: "#cbb8e8"
                        font.pixelSize: 16
                        text: qsTr("⏱ %1").arg(exerciseScreen.durationLabel(exerciseScreen.rewardValue(ExerciseController.sessionDurationSeconds)))
                    }

                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        color: "#cbb8e8"
                        font.pixelSize: 16
                        text: qsTr("🔥 série %1").arg(exerciseScreen.rewardValue(ExerciseController.sessionLongestStreak))
                    }

                }

                // L'EXPERIENCE, et son merite : le multiplicateur ne s'affiche QUE s'il a majore, sinon la ligne dirait
                // « x1 », ce qui n'est pas une recompense mais un constat.
                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    color: "#ffd479"
                    font.pixelSize: 22
                    font.bold: true
                    text: ExerciseController.arcadeMultiplierPercent > 100 ? qsTr("+%1 XP  ·  x%2").arg(exerciseScreen.rewardValue(ExerciseController.arcadeXpEarned)).arg(ExerciseController.arcadeMultiplierPercent / 100) : qsTr("+%1 XP").arg(exerciseScreen.rewardValue(ExerciseController.arcadeXpEarned))
                }

                // Roger : « on ecrit les pourcentages de reussite, et on voit 100 % partout. Mais ca n'a pas trop de sens,
                // car forcement s'il arrive a la fin il aura du 100 % partout. » Il a raison, et l'indicateur etait
                // structurellement muet : le taux ne descend QUE si l'on joue mal, et si l'on joue mal la partie s'arrete
                // avant la fin - donc la ligne ne s'affiche meme pas. Ce qu'on montre desormais, c'est ce que la partie a
                // COUTE : le nombre d'erreurs, qui est aussi le nombre de coeurs perdus.
                Repeater {
                    model: ExerciseController.sessionGrantsExperience ? ExerciseController.familyResults() : []

                    // LE MOT DE LA FIN, sous les trois familles.
                    Text {
                        Layout.preferredWidth: 0
                        Layout.minimumWidth: 0
                        Layout.fillWidth: true
                        Layout.topMargin: 10
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        color: "#ffd479"
                        font.pixelSize: 14
                        text: ExerciseController.sessionGrantsExperience ? exerciseScreen.closingAdvice() : ""
                    }

                    delegate: RowLayout {
                        required property var modelData

                        Layout.fillWidth: true
                        spacing: 6

                        Text {
                            Layout.fillWidth: true
                            color: "#e8dcff"
                            font.pixelSize: 14
                            text: modelData.name
                        }

                        Text {
                            horizontalAlignment: Text.AlignRight
                            color: modelData.errors === 0 ? "#8ef2b0" : "#ff8fb0"
                            font.pixelSize: 14
                            font.bold: true
                            // La forme plurielle de Qt, et pas un « (s) » ecrit a la main : c'est ce qui permet a une
                            // traduction de dire « 0 erreur » et « 1 erreur » comme sa langue le demande.
                            text: modelData.errors === 0 ? qsTr("Parfait !") : qsTr("%n erreur(s)", "", modelData.errors)
                        }

                    }

                }

                // LE MOT, sous les chiffres : un point fort et un point a travailler, tires de la partie qu'on vient de
                // jouer. Le second n'apparait que s'il y a PLUSIEURS familles - sinon la meilleure et la pire seraient la
                // meme ligne, dite deux fois.
                Text {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
                    Layout.topMargin: 2
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: "#8ef2b0"
                    font.pixelSize: 13
                    visible: ExerciseController.isFinished && (exerciseScreen.strongestFamily() !== null)
                    text: (exerciseScreen.strongestFamily() !== null) ? qsTr("Fort en %1.").arg(exerciseScreen.strongestFamily().name) : ""
                }

                Text {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: "#ff8fb0"
                    font.pixelSize: 13
                    visible: ExerciseController.isFinished && (exerciseScreen.weakestFamily() !== null)
                    text: (exerciseScreen.weakestFamily() !== null) ? qsTr("À travailler : %1.").arg(exerciseScreen.weakestFamily().name) : ""
                }

            }

            // LES AUTRES MODES ne paient pas, et l'ecran le DIT plutot que d'afficher un gain qui n'a pas eu lieu.
            Text {
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                Layout.minimumWidth: 0
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                color: "#8a77ad"
                font.pixelSize: 14
                visible: !ExerciseController.sessionGrantsExperience
                text: qsTr("Ici, pas d'expérience : l'Arcade seule en donne. Mais tout compte pour tes statistiques.")
            }

            // Roger : « a la fin du bilan, si il a gagne un trophee ou une recompense, il faut lui dire (et lui dire qu'ils
            // sont dans "profil") - du coup il va y aller et se rendre compte qu'il y en a d'autres a gagner ». C'est
            // exactement le but : le renvoyer voir la liste, ou les suivants attendent.
            ColumnLayout {
                Layout.fillWidth: true
                Layout.topMargin: 8
                // ELLES SONT LUES COMME DES PROPRIETES, et c'est une CORRECTION : ecrire `titleJustIncreased` sans
                // parentheses n'appelait PAS la methode - c'etait un objet fonction, donc toujours vrai, et le badge
                // s'affichait a chaque fin de partie avec « Toutou ». Et une propriete notifiable est ce qui permet a
                // cette liaison de se rafraichir quand ce que le bilan a rapporte change.
                visible: (ExerciseController.newlyEarnedTrophies.length > 0) || ExerciseController.titleJustIncreased
                spacing: 4

                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    color: "#ffd479"
                    font.pixelSize: 17
                    font.bold: true
                    text: ExerciseController.titleJustIncreased ? qsTr("🏆 Nouveau titre : %1").arg(ExerciseController.playerTitle.name) : qsTr("🏆 Récompense !")
                }

                Repeater {
                    model: ExerciseController.newlyEarnedTrophies

                    delegate: Text {
                        required property var modelData

                        Layout.fillWidth: true
                        Layout.preferredWidth: 0
                        Layout.minimumWidth: 0
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        color: "#e8dcff"
                        font.pixelSize: 13
                        text: "★ " + modelData.name + " · " + modelData.description
                    }

                }

                Text {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: "#8a77ad"
                    font.pixelSize: 12
                    text: qsTr("Tout est dans ton profil.")
                }

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
                text: ExerciseController.anecdoteText !== "" ? qsTr("« %1 »").arg(ExerciseController.anecdoteText) : ""
            }

            Item {
                Layout.fillHeight: true
            }

            Button {
                Layout.fillWidth: true
                height: 54
                highlighted: true
                text: qsTr("▶ Rejouer")
                onClicked: ExerciseController.restartSession()
            }

            Button {
                Layout.fillWidth: true
                height: 48
                text: qsTr("← Retour au banc")
                onClicked: ExerciseController.stopSession()
            }

        }

    }

}
