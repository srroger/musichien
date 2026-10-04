// =====================================================================================================================
// Musichien - CourseScreen
// L'ECOLE DES CHIOTS : le catalogue des cours, et la page qu'on lit.
// MEME PATRON QUE ScaleScreen : la page peint son fond, porte son titre et sa sortie, et DEMANDE a sortir
// (`closeRequested`) plutot que de connaitre le dialogue qui la porte.
// LE MARKDOWN EST RENDU PAR QT, PAS PAR NOUS : `Text.MarkdownText` rend gras, italique, titres, listes et citations.
// Reecrire un moteur de Markdown serait du travail perdu, avec des bugs en prime - et il ne suivrait pas les cours.
// LES CARTES NE SONT PAS DES BOUTONS DU STYLE, mais des Rectangles avec un MouseArea : le style Material garde sa
// feuille blanche et ses marges, et cette page a deja eu affaire a lui. Les couleurs sont celles de ScaleScreen et de
// KeyCircleScreen, en dur, parce qu'un ecran separe n'herite pas des proprietes de fenetre de Main.qml.

// L'IMPORT DU MODULE DE L'APPLICATION EST OBLIGATOIRE : `CourseController` est un singleton ENREGISTRE, donc il n'existe
// pour une page que si elle importe `Musichien`. Sans cet import, la page s'ouvre BLANCHE, sans bouton et sans erreur de
// build - c'est le piege que ScaleScreen documente, apres l'avoir paye.
// =====================================================================================================================
import Musichien
import QtQuick
import QtQuick.Layouts

Item {
    // LE PASSAGE DE PAGE. Roger : « si possible rajouter une petite animation quand on va a la page suivante, de page
    // qui se tourne. »
    // L'IDEE VIENT D'UN LIVRE : le contenu ENTRE par le cote ou l'on va. Vers l'avant il vient de la droite, vers
    // l'arriere de la gauche - c'est ce qui dit au doigt qu'il a AVANCE plutot que recule, et sans ajouter une fleche.

    id: courseScreen

    // On n'anime QUE le decalage. Une fonte demanderait de faire vivre deux pages a la fois, et un clignotement est pire
    // que pas d'animation du tout.
    property real pageTurnOffset: 0
    property int pageTurnAnchor: -1

    // La page demande a sortir, et celui qui la porte decide comment.
    signal closeRequested()
    // LA PAGE DEMANDE A CHANTER UN INTERVALLE. Elle dit l'intention, et celui qui la porte decide comment - c'est ce qui
    // lui permet d'ignorer ou vit l'outil de chant, et meme s'il y en a un.
    signal singRequested(int p_semitones)
    // OUVRIR UNE ANNEXE : la carte « :: annexe » cite un nom, et c'est ICI qu'on sait ou vit le lecteur de cours. La
    // page dit l'INTENTION, celui qui la porte decide - la meme regle que pour la carte de chant.
    signal annexeRequested(string p_annexeName)
    // OUVRIR LE CERCLE : la page de reference du jeu. La page dit l'intention, celui qui la porte decide - la meme regle
    // que pour l'annexe et pour le chant.
    signal circleRequested()

    // L'IMAGE DEVIENT UNE ADRESSE ICI, ET NULLE PART AILLEURS. Un cours ecrit un NOM ; c'est l'ecran qui sait ou vivent
    // les images. Et un nom qui porte deja son extension est respecte tel quel - sinon le .jpg que Roger a depose pour
    // la cornemuse ne s'afficherait jamais, et le telephone ne le dirait pas plus qu'une image distante.
    function imageSourceFor(fileName) {
        if (fileName.indexOf(".") >= 0)
            return "qrc:/assets/content/images/" + fileName;

        return "qrc:/assets/content/images/" + fileName + ".png";
    }

    anchors.fill: parent

    NumberAnimation {
        id: pageTurn

        target: courseScreen
        property: "pageTurnOffset"
        to: 0
        duration: 220
        easing.type: Easing.OutCubic
    }

    Connections {
        function onCourseChanged() {
            // « courseChanged » part AUSSI a l'ouverture d'un cours, au retour a la note complete et a la fermeture : on
            // n'anime donc que si la PAGE a vraiment change, sinon l'ecran clignoterait pour rien.
            if (CourseController.sectionIndex === courseScreen.pageTurnAnchor)
                return ;

            const versLavant = CourseController.sectionIndex > courseScreen.pageTurnAnchor;
            courseScreen.pageTurnAnchor = CourseController.sectionIndex;
            courseScreen.pageTurnOffset = versLavant ? 70 : -70;
            pageTurn.restart();
        }

        target: CourseController
    }

    Rectangle {
        anchors.fill: parent
        color: "#1d1033"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        // La barre de navigation d'Android passe par-dessus la page : sans cette marge, la derniere carte se retrouve
        // DERRIERE les boutons du systeme.
        anchors.bottomMargin: 48
        spacing: 10

        // L'EN-TETE. Le titre de l'ecole reste, et la ligne du dessous dit ou l'on en est : le catalogue, ou le cours.
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                Text {
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: "#ffd479"
                    font.pixelSize: 20
                    font.bold: true
                    text: qsTr("L'École des Chiots")
                }

                Text {
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: "#8a77ad"
                    font.pixelSize: 12
                    text: CourseController.reading ? CourseController.subtitle : qsTr("Choisis une leçon.")
                }

            }

            Rectangle {
                Layout.preferredWidth: 44
                Layout.preferredHeight: 36
                radius: 8
                color: "#2a1a4a"

                Text {
                    anchors.centerIn: parent
                    color: "#cbbde8"
                    font.pixelSize: 22
                    // MULTIPLICATION SIGN, pas une croix de symbole : U+00D7 est en Latin-1 et vit dans TOUTES les polices,
                    // alors que le '✕' que j'avais mis (U+2715) n'est pas dans Quicksand - et Quicksand est impose comme
                    // police de l'interface. Le resultat etait le rectangle que Roger a vu : « une icone manquante ».
                    text: qsTr("×")
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        ExerciseController.playTapCue();
                        courseScreen.closeRequested();
                    }
                }

            }

        }

        // LE CATALOGUE, quand aucun cours n'est ouvert. Un cours a la fois : la page est un livre, pas une liste.
        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !CourseController.reading
            clip: true
            contentHeight: libraryColumn.height

            ColumnLayout {
                id: libraryColumn

                width: parent.width
                spacing: 10

                Repeater {
                    // LE COURS EST LA LEÇON, l'os a macher est son ANNEXE : c'est donc le COURS qui doit ressortir.

                    model: CourseController.library

                    delegate: Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 66
                        radius: 10
                        // Roger : « tu as mis en avant l'os a macher, alors que c'est plutot le cours qu'il faudrait
                        // mettre en avant ». Il a raison, et j'avais inverse les deux : mon annexe etait plus claire que
                        // mes cours, ce qui mettait en avant exactement ce qui est secondaire.
                        color: modelData.isAnnexe ? "#221a38" : "#2a1a4a"
                        border.color: modelData.isAnnexe ? "#3a2b5c" : "#6a4fa8"
                        border.width: modelData.isAnnexe ? 1 : 2

                        Column {
                            anchors.fill: parent
                            anchors.margins: 10
                            anchors.rightMargin: 34
                            spacing: 2

                            Text {
                                width: parent.width
                                elide: Text.ElideRight
                                color: modelData.isAnnexe ? "#9d8dc0" : "#f2ecff"
                                font.pixelSize: modelData.isAnnexe ? 14 : 16
                                font.bold: !modelData.isAnnexe
                                // L'os dit ce qu'il est, sans qu'on ait a le deviner d'une teinte.
                                text: (modelData.isAnnexe ? qsTr("🦴  ") : qsTr("")) + modelData.title
                            }

                            Text {
                                width: parent.width
                                elide: Text.ElideRight
                                visible: text !== ""
                                color: "#8a77ad"
                                font.pixelSize: 12
                                text: modelData.subtitle
                            }

                        }

                        Text {
                            anchors.right: parent.right
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            color: "#cbbde8"
                            font.pixelSize: 18
                            // Un chevron ASCII, et pas '›' (U+203A) : meme raison que le bouton de sortie, c'est la police
                            // de l'interface qui decide, et elle n'a pas a avoir tous les symboles du monde.
                            text: qsTr(">")
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: CourseController.open(index)
                        }

                    }

                }

            }

        }

        // LE COURS OUVERT. L'ordre des blocs est celui du FICHIER : c'est la donnee, pas une mise en page.
        Flickable {
            // LE SWIPE. Roger : « je me demande si ce serait possible d'aller d'une page a une en swippant de gauche a
            // droite au lieu d'appuyer sur le bouton ? » ... puis, apres l'avoir essaye : « le swipe n'a pas l'air de
            // fonctionner. »
            // LE PREMIER ESSAI ETAIT FAUX, et la raison vaut d'etre gardee : un MouseArea pose DERRIERE le contenu ne
            // voyait jamais le relachement, parce qu'un FLICKABLE REPREND TOUS LES GLISSEMENTS - verticaux comme
            // horizontaux. Un doigt qui bouge est un defilement, un point c'est tout ; le MouseArea n'avait plus la main.

            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: CourseController.reading
            clip: true
            contentHeight: courseColumn.height
            flickableDirection: Flickable.VerticalFlick

            // UN DRAGHANDLER EST UN GESTIONNAIRE DE POINTEUR, et il passe AVANT le Flickable. Et comme il n'ecoute QUE
            // l'axe HORIZONTAL, il ne s'active pas sur un geste vertical - le defilement reste donc au Flickable, intact.
            DragHandler {
                id: pageSwipe

                // LE GLISSEMENT ACCUMULE, en horizontal : on le lit au relachement.
                property real swept: 0

                target: null
                yAxis.enabled: false
                dragThreshold: 24
                enabled: !CourseController.showingWholeNote && CourseController.sectionCount > 1
                onActiveChanged: {
                    // Un geste commence : on repart de zero.
                    if (active) {
                        pageSwipe.swept = 0;
                        return ;
                    }
                    // IL FAUT VRAIMENT GLISSER : 60 px, sinon c'est un frolement.
                    if (Math.abs(pageSwipe.swept) < 60)
                        return ;

                    // Vers la GAUCHE (contenu qui s'en va) : page suivante. Vers la DROITE : precedente.
                    if (pageSwipe.swept < 0)
                        CourseController.nextSection();
                    else
                        CourseController.previousSection();
                }
                xAxis.onActiveValueChanged: (delta) => {
                    pageSwipe.swept += delta;
                }
            }

            ColumnLayout {
                // LA NAVIGATION, AU BOUT DE LA PAGE.

                id: courseColumn

                width: parent.width
                spacing: 14

                // A la fin de la derniere page, on propose LA NOTE COMPLETE - c'est le « et a la fin, on verrait la note
                // complete pour s'y referer » de Roger. Un cours sans section n'affiche rien de tout cela : il a une
                // seule page, et il n'y a rien a tourner.
                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 10
                    spacing: 8
                    visible: CourseController.sectionCount > 1

                    Rectangle {
                        Layout.preferredWidth: 120
                        Layout.preferredHeight: 44
                        radius: 10
                        color: "#2a1a4a"
                        visible: !CourseController.showingWholeNote && CourseController.sectionIndex > 0

                        Text {
                            anchors.centerIn: parent
                            color: "#cbbde8"
                            font.pixelSize: 14
                            text: qsTr("< Precedent")
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                ExerciseController.playTapCue();
                                CourseController.previousSection();
                            }
                        }

                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Rectangle {
                        Layout.preferredWidth: 170
                        Layout.preferredHeight: 44
                        radius: 10
                        color: "#6a4fa8"
                        visible: !CourseController.showingWholeNote && CourseController.sectionIndex + 1 < CourseController.sectionCount

                        Text {
                            anchors.centerIn: parent
                            color: "#ffffff"
                            font.pixelSize: 14
                            text: qsTr("Suivant >")
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                ExerciseController.playTapCue();
                                CourseController.nextSection();
                            }
                        }

                    }

                    Rectangle {
                        Layout.preferredWidth: 190
                        Layout.preferredHeight: 44
                        radius: 10
                        color: "#6a4fa8"
                        visible: !CourseController.showingWholeNote && CourseController.sectionIndex + 1 >= CourseController.sectionCount

                        Text {
                            anchors.centerIn: parent
                            color: "#ffffff"
                            font.pixelSize: 14
                            text: qsTr("Voir la note complete")
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                ExerciseController.playTapCue();
                                CourseController.setShowingWholeNote(true);
                            }
                        }

                    }

                    Rectangle {
                        Layout.preferredWidth: 170
                        Layout.preferredHeight: 44
                        radius: 10
                        color: "#2a1a4a"
                        visible: CourseController.showingWholeNote

                        Text {
                            anchors.centerIn: parent
                            color: "#cbbde8"
                            font.pixelSize: 14
                            text: qsTr("Revenir aux pages")
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                ExerciseController.playTapCue();
                                CourseController.setShowingWholeNote(false);
                            }
                        }

                    }

                }

                Text {
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: "#ffd479"
                    font.pixelSize: 22
                    font.bold: true
                    text: CourseController.title
                }

                // LE TITRE DE LA PAGE. Le cours se lit une SECTION a la fois, et c'est ce titre qui dit ou l'on est - il
                // vient du « ## » ecrit dans le fichier, pas d'une decoupe inventee ici.
                Text {
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
                    Layout.fillWidth: true
                    visible: text !== ""
                    wrapMode: Text.WordWrap
                    color: "#cbbde8"
                    font.pixelSize: 17
                    font.bold: true
                    text: CourseController.sectionTitle

                    // Le titre entre AVEC sa page, pas avant elle.
                    transform: Translate {
                        x: courseScreen.pageTurnOffset
                    }

                }

                Repeater {
                    // LA CARTE "ECOUTE" : la seule du jeu qui SORT de l'application.
                    // LA CARTE "CHANTE-LE" : elle ouvre l'OUTIL DE CHANT sur cet intervalle.
                    // Roger, sur le cours de la quinte juste : « on propose au joueur de chanter la quinte. Autant lui
                    // fournir l'outil pour qu'il verifie lui-meme s'il chante juste. » Un cours qui demande une chose
                    // et ne donne pas le moyen de la verifier laisse le joueur deviner.
                    // La page dit l'INTENTION et celui qui la porte decide : c'est ce qui lui permet d'ignorer ou vit
                    // l'outil de chant, et meme s'il y en a un.
                    // LA CARTE "POUR ALLER PLUS LOIN" : elle ouvre l'ANNEXE.
                    // Une annexe n'est pas une autre sorte de contenu : c'est un cours range a la fin du catalogue,
                    // et celui-ci se lit donc avec le MEME lecteur, la meme pagination et le meme retour.
                    // LA CARTE "IMAGE" : une gravure, et sa legende.
                    // L'ADRESSE SE CONSTRUIT ICI, a partir d'un nom et d'un dossier unique. Un cours n'ecrit jamais
                    // « qrc:/... » : un chemin dans un fichier de contenu est un chemin qui se casse le jour ou
                    // l'image demenage.
                    // LA CARTE "SERIE" : le jeu joue la serie harmonique, et l'ecran la nomme.
                    // LA CARTE "BOURDON" : le jeu TIENT un centre, et l'ecran le nomme.
                    // LA CARTE "CYCLE" : le cercle des quintes se PARCOURT.
                    // LA CARTE "GAMME" : la gamme d'un mode, jouee sur le bourdon.
                    // LE SCHEMA - la seule carte du catalogue qui DESSINE.
                    // Roger, en relisant le cours du centre : « la musique celtique avec cette fameuse cornemuse est
                    // un exemple tres fort du bourdon. Ce serait bien de rajouter une petite page, peut-etre image ou
                    // un dessin ou schema qui parle. Juste pour ne pas avoir que du texte. »
                    // Une image du commerce aurait montre un INSTRUMENT. Ce dessin montre le PRINCIPE - deux notes
                    // tenues du debut a la fin, et une ligne qui part, derive, et REVIENT se poser sur le centre.
                    // C'est exactement le propos du chapitre, et ca se voit avant de le lire.

                    model: CourseController.blocks

                    delegate: ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        // OUVRIR LE CERCLE. Roger : « on est capable de le fabriquer dans le code, on a meme une page
                        // dediee au cercle. » Alors on l'ouvre - et le cours n'a pas besoin de savoir ou elle vit.
                        Rectangle {
                            Layout.fillWidth: true
                            visible: modelData.kind === "cercle"
                            implicitHeight: circleText.implicitHeight + 24
                            radius: 10
                            color: "#241a3d"
                            border.color: "#5a4a8f"
                            border.width: 1

                            Text {
                                id: circleText

                                anchors.fill: parent
                                anchors.margins: 12
                                wrapMode: Text.WordWrap
                                color: "#cbb8e8"
                                font.pixelSize: 13
                                // La legende est FACULTATIVE : le bouton a un nom par defaut, et un cours qui n'a rien de
                                // mieux a dire n'a pas a le repeter.
                                text: modelData.caption !== "" ? qsTr("🎡  %1").arg(modelData.caption) : qsTr("🎡  Ouvrir le cercle des quintes")
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    ExerciseController.playTapCue();
                                    courseScreen.circleRequested();
                                }
                            }

                        }

                        // UN PARAGRAPHE DE MARKDOWN : Qt le rend, nous ne le composons pas.
                        Text {
                            Layout.preferredWidth: 0
                            Layout.minimumWidth: 0
                            Layout.fillWidth: true
                            visible: modelData.kind === "text"
                            textFormat: Text.MarkdownText
                            wrapMode: Text.WordWrap
                            color: "#e7dff7"
                            font.pixelSize: 15
                            text: modelData.markdown
                        }

                        // LA CARTE "LE JEU LA JOUE" : aucun reseau, aucune attente, le bon temperament.
                        Rectangle {
                            Layout.fillWidth: true
                            visible: modelData.kind === "play"
                            implicitHeight: playText.implicitHeight + 24
                            radius: 10
                            color: "#21351f"
                            border.color: "#3f6a3a"
                            border.width: 1

                            Text {
                                id: playText

                                anchors.fill: parent
                                anchors.margins: 12
                                wrapMode: Text.WordWrap
                                color: "#cdeec6"
                                font.pixelSize: 14
                                text: qsTr("🔊  %1").arg(modelData.caption)
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    ExerciseController.playTapCue();
                                    CourseController.playInterval(modelData.semitones, modelData.direction);
                                }
                            }

                        }

                        // LA CARTE "L'ACCORD" : le jeu joue trois notes, et la tierce dit la couleur. C'est la carte du
                        // chapitre de la couleur - le majeur et le mineur, entendus avant d'etre nommes.
                        Rectangle {
                            Layout.fillWidth: true
                            visible: modelData.kind === "accord"
                            implicitHeight: chordText.implicitHeight + 24
                            radius: 10
                            color: "#21351f"
                            border.color: "#3f6a3a"
                            border.width: 1

                            Text {
                                id: chordText

                                anchors.fill: parent
                                anchors.margins: 12
                                wrapMode: Text.WordWrap
                                color: "#cdeec6"
                                font.pixelSize: 14
                                text: qsTr("🎹  %1").arg(modelData.caption)
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    ExerciseController.playTapCue();
                                    CourseController.playChord(modelData.chordQuality, modelData.chordInversion);
                                }
                            }

                        }

                        // La regle d'or veut que le jeu joue ce qu'il sait jouer ; cette carte sert a ce qu'il ne saura
                        // jamais produire - un orchestre, une interpretation. Et c'est le SYSTEME qui ouvre le lien, pas
                        // nous : un lecteur integre demanderait Qt WebEngine, 50 a 80 Mo, et annulerait tout le travail
                        // d'allegement de l'APK.
                        Rectangle {
                            Layout.fillWidth: true
                            visible: modelData.kind === "listen"
                            implicitHeight: listenColumn.implicitHeight + 24
                            radius: 10
                            color: "#2a1a4a"
                            border.color: "#6a4fa8"
                            border.width: 1

                            Column {
                                id: listenColumn

                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 6

                                Text {
                                    width: parent.width
                                    wrapMode: Text.WordWrap
                                    color: "#f2ecff"
                                    font.pixelSize: 14
                                    font.bold: true
                                    text: modelData.cardTitle
                                }

                                // CE QU'IL FAUT Y ENTENDRE : le contrat en fait une obligation, et c'est le principe de
                                // signalisation de Mayer. Un lien sans consigne d'ecoute est un lien qu'on n'ecoute pas.
                                Text {
                                    width: parent.width
                                    wrapMode: Text.WordWrap
                                    color: "#b9a8dd"
                                    font.pixelSize: 13
                                    text: modelData.listenFor
                                }

                                Rectangle {
                                    width: parent.width
                                    height: 40
                                    radius: 8
                                    color: "#6a4fa8"

                                    Text {
                                        anchors.centerIn: parent
                                        color: "#ffffff"
                                        font.pixelSize: 14
                                        text: modelData.source === "spotify" ? qsTr("Écouter sur Spotify") : qsTr("Écouter sur YouTube")
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: {
                                            ExerciseController.playTapCue();
                                            Qt.openUrlExternally(modelData.url);
                                        }
                                    }

                                }

                            }

                        }

                        // LA CARTE "ESSAIE" : elle quitte la page ET ouvre l'exercice de la famille des intervalles.
                        Rectangle {
                            Layout.fillWidth: true
                            visible: modelData.kind === "try"
                            implicitHeight: tryText.implicitHeight + 24
                            radius: 10
                            color: "#2b2350"
                            border.color: "#5a4a8f"
                            border.width: 1

                            Text {
                                id: tryText

                                anchors.fill: parent
                                anchors.margins: 12
                                wrapMode: Text.WordWrap
                                color: "#d8cdf4"
                                font.pixelSize: 14
                                text: modelData.exerciseFamily === 1 ? qsTr("Essaie-le : l'exercice des accords") : modelData.exerciseFamily === 2 ? qsTr("Essaie-le : l'exercice des modes") : qsTr("Essaie-le : l'exercice des intervalles")
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    // ON NE FERME PAS L'ECOLE : l'ecran d'exercice est un calque declare PLUS BAS, donc il
                                    // passe par-dessus la page. Quitter la partie fait alors RETOMBER sur la lecon qu'on
                                    // etait en train de lire - ce que Roger attend, et non la page de garde.
                                    // L'EXERCICE DE CE COURS, et non l'entrainement en general. Le mot de la carte dit la
                                    // FAMILLE : intervalle, accord ou mode ; la LISTE dit ce qu'on ouvre dedans (les modes
                                    // 'tous', par exemple). Pour les intervalles, la palette est EXACTEMENT les concepts
                                    // du cours.
                                    ExerciseController.playTapCue();
                                    ExerciseController.startTrainingSessionFromLesson(CourseController.currentConcepts, modelData.exerciseFamily, modelData.exerciseTargets);
                                }
                            }

                        }

                        // ⚠️ CETTE CARTE A ETE LIVREE MORTE. Elle a tenu sur UNE seule ligne, commentaires et code
                        // melanges : tout ce qui suit le premier « // » appartient au commentaire, donc le Rectangle
                        // n'existait pas. Roger lisait « chante le sol » sans avoir un seul bouton pour le faire. Une
                        // ligne avalee par un commentaire ne fait echouer aucun outil : ni le compilateur, ni qmllint,
                        // qui ne voit qu'un commentaire. C'est POURQUOI ce fichier se relit a l'oeil apres formatage.
                        Rectangle {
                            Layout.fillWidth: true
                            visible: modelData.kind === "sing"
                            implicitHeight: singText.implicitHeight + 24
                            radius: 10
                            color: "#2b2350"
                            border.color: "#7a5cc0"
                            border.width: 1

                            Text {
                                id: singText

                                anchors.fill: parent
                                anchors.margins: 12
                                wrapMode: Text.WordWrap
                                color: "#d8cdf4"
                                font.pixelSize: 14
                                text: modelData.caption !== "" ? modelData.caption : qsTr("Chante-le, et verifie d'un coup d'oeil")
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    ExerciseController.playTapCue();
                                    courseScreen.singRequested(modelData.semitones);
                                }
                            }

                        }

                        // Elle etait INERTE, et elle le disait - « les Os a macher arrivent bientot » - alors que le
                        // fichier existait deja : personne ne pouvait le lire. Un contenu ecrit et inaccessible est pire
                        // qu'un contenu absent, parce qu'il donne l'impression d'un jeu casse.
                        Rectangle {
                            Layout.fillWidth: true
                            visible: modelData.kind === "annexe"
                            implicitHeight: annexeText.implicitHeight + 24
                            radius: 10
                            color: "#241a3d"
                            border.color: "#5a4a8f"
                            border.width: 1

                            Text {
                                id: annexeText

                                anchors.fill: parent
                                anchors.margins: 12
                                wrapMode: Text.WordWrap
                                color: "#cbb8e8"
                                font.pixelSize: 13
                                // Le titre vient DU COURS, tel que son auteur l'a ecrit : la carte n'invente rien, et ne
                                // peut donc pas annoncer autre chose que ce qu'elle ouvre.
                                text: qsTr("🦴  Pour aller plus loin — %1").arg(modelData.annexeName)
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    ExerciseController.playTapCue();
                                    courseScreen.annexeRequested(modelData.annexeName);
                                }
                            }

                        }

                        // Et l'image est EMBARQUEE, jamais distante : l'application n'a pas la permission d'acces au
                        // reseau - c'est la charte du projet - donc une image venant d'une adresse ne s'afficherait pas,
                        // et le telephone ne le dirait pas.
                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: modelData.kind === "image"
                            spacing: 6

                            Image {
                                Layout.fillWidth: true
                                // Une hauteur MAXIMALE : sans elle, une image haute mangerait trois pages de defilement.
                                // PreserveAspectFit garde les proportions, donc rien n'est deforme.
                                Layout.preferredHeight: 380
                                source: courseScreen.imageSourceFor(modelData.imageName)
                                fillMode: Image.PreserveAspectFit
                                asynchronous: true
                                smooth: true
                            }

                            Text {
                                Layout.preferredWidth: 0
                                Layout.minimumWidth: 0
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                color: "#8a77ad"
                                font.pixelSize: 12
                                text: modelData.caption
                            }

                        }

                        // C'est la carte qui rend le chapitre audible. « La quinte est DANS la note » se lit en trois
                        // secondes et se croit sur parole ; l'entendre, c'est autre chose.
                        Rectangle {
                            Layout.fillWidth: true
                            visible: modelData.kind === "serie"
                            implicitHeight: seriesText.implicitHeight + 24
                            radius: 10
                            color: "#21351f"
                            border.color: "#3f6a3a"
                            border.width: 1

                            Text {
                                id: seriesText

                                anchors.fill: parent
                                anchors.margins: 12
                                wrapMode: Text.WordWrap
                                color: "#cdeec6"
                                font.pixelSize: 14
                                text: qsTr("🎼  %1").arg(modelData.caption)
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    ExerciseController.playTapCue();
                                    CourseController.playHarmonicSeries();
                                }
                            }

                        }

                        // C'est la formule exacte des questions de couleur du jeu : la tonique et sa quinte, sans tierce.
                        // Un centre qui ne colore rien lui-meme - donc qui laisse entendre la couleur de ce qu'on pose
                        // dessus. Avec une tierce, il dirait deja majeur ou mineur.
                        Rectangle {
                            Layout.fillWidth: true
                            visible: modelData.kind === "bourdon"
                            implicitHeight: droneText.implicitHeight + 24
                            radius: 10
                            color: "#21351f"
                            border.color: "#3f6a3a"
                            border.width: 1

                            Text {
                                id: droneText

                                anchors.fill: parent
                                anchors.margins: 12
                                wrapMode: Text.WordWrap
                                color: "#cdeec6"
                                font.pixelSize: 14
                                text: qsTr("🎵  %1").arg(modelData.caption)
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    ExerciseController.playTapCue();
                                    CourseController.playDrone();
                                }
                            }

                        }

                        // PEINT, ET NON CHARGE : net a toutes les tailles d'ecran, anime, et il ne coute pas un octet de
                        // plus dans l'APK. Le point qui avance fait la difference entre un dessin et un schema - sans
                        // lui, on voit une courbe ; avec lui, on voit une melodie qui RENTRE chez elle.
                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: modelData.kind === "schema"
                            spacing: 6

                            CourseSchema {
                                Layout.fillWidth: true
                                schemaName: modelData.schemaName
                            }

                            Text {
                                Layout.preferredWidth: 0
                                Layout.minimumWidth: 0
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                color: "#8a77ad"
                                font.pixelSize: 12
                                text: modelData.caption
                            }

                        }

                        // Rien de dessine, et c'est volontaire : ce qui compte ici, c'est de l'ENTENDRE revenir sur ses
                        // pas. Une roue a l'ecran attirerait l'oeil sur autre chose que le son.
                        Rectangle {
                            Layout.fillWidth: true
                            visible: modelData.kind === "cycle"
                            implicitHeight: cycleText.implicitHeight + 24
                            radius: 10
                            color: "#2b2350"
                            border.color: "#5a4a8f"
                            border.width: 1

                            Text {
                                id: cycleText

                                anchors.fill: parent
                                anchors.margins: 12
                                wrapMode: Text.WordWrap
                                color: "#d8cdf4"
                                font.pixelSize: 14
                                text: qsTr("🎹  %1").arg(modelData.caption)
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    ExerciseController.playTapCue();
                                    CourseController.playFifthCycle(modelData.fifthCount);
                                }
                            }

                        }

                        // C'est la carte qui rend les couleurs audibles. Sans bourdon, sept notes sont sept notes ;
                        // avec lui, elles sont une COULEUR - et c'est la meme formule que le banc d'essai des modes.
                        Rectangle {
                            Layout.fillWidth: true
                            visible: modelData.kind === "gamme"
                            implicitHeight: scaleText.implicitHeight + 24
                            radius: 10
                            color: "#21351f"
                            border.color: "#3f6a3a"
                            border.width: 1

                            Text {
                                id: scaleText

                                anchors.fill: parent
                                anchors.margins: 12
                                wrapMode: Text.WordWrap
                                color: "#cdeec6"
                                font.pixelSize: 14
                                text: qsTr("🎼  %1").arg(modelData.caption)
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    ExerciseController.playTapCue();
                                    CourseController.playModeScale(modelData.modeIndex);
                                }
                            }

                        }

                        // LA PAGE ENTIERE ENTRE D'UN BLOC : chaque carte suit le meme decalage que le titre, donc la
                        // page se deplace d'un seul geste plutot que ligne par ligne.
                        transform: Translate {
                            x: courseScreen.pageTurnOffset
                        }

                    }

                }

            }

        }

    }

}
