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
    id: courseScreen

    // La page demande a sortir, et celui qui la porte decide comment.
    signal closeRequested()

    anchors.fill: parent

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
                    onClicked: courseScreen.closeRequested()
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
                    model: CourseController.library

                    delegate: Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 66
                        radius: 10
                        // UN OS A MACHER N'EST PAS UN COURS. La difference est deja dans les fichiers - un cours est
                        // numerote, une annexe ne l'est pas - et l'ecran ne fait que la montrer. Roger : « je mettrais
                        // bien les os a macher d'une autre couleur que les cours ».
                        color: modelData.isAnnexe ? "#33244f" : "#2a1a4a"
                        border.color: modelData.isAnnexe ? "#7a5cc0" : "#4a3570"
                        border.width: 1

                        Column {
                            anchors.fill: parent
                            anchors.margins: 10
                            anchors.rightMargin: 34
                            spacing: 2

                            Text {
                                width: parent.width
                                elide: Text.ElideRight
                                color: modelData.isAnnexe ? "#cbbde8" : "#f2ecff"
                                font.pixelSize: 15
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
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: CourseController.reading
            clip: true
            contentHeight: courseColumn.height

            ColumnLayout {
                id: courseColumn

                width: parent.width
                spacing: 14

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

                Repeater {
                    // LA CARTE "ECOUTE" : la seule du jeu qui SORT de l'application.

                    model: CourseController.blocks

                    delegate: ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6

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
                                onClicked: CourseController.playInterval(modelData.semitones, modelData.direction)
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
                                        onClicked: Qt.openUrlExternally(modelData.url)
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
                                text: qsTr("Essaie-le : l'exercice des intervalles")
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    // ON NE FERME PAS L'ECOLE : l'ecran d'exercice est un calque declare PLUS BAS, donc il
                                    // passe par-dessus la page. Quitter la partie fait alors RETOMBER sur la lecon qu'on
                                    // etait en train de lire - ce que Roger attend, et non la page de garde.
                                    ExerciseController.startTrainingSession(0);
                                }
                            }

                        }

                        // LA CARTE "POUR ALLER PLUS LOIN" : inerte pour l'instant, et elle le dit.
                        Rectangle {
                            Layout.fillWidth: true
                            visible: modelData.kind === "annexe"
                            implicitHeight: annexeText.implicitHeight + 24
                            radius: 10
                            color: "#241a3d"
                            border.color: "#3a2b5c"
                            border.width: 1

                            Text {
                                id: annexeText

                                anchors.fill: parent
                                anchors.margins: 12
                                wrapMode: Text.WordWrap
                                color: "#9d8dc0"
                                font.pixelSize: 13
                                text: qsTr("🦴  Pour aller plus loin — les Os à mâcher arrivent bientôt")
                            }

                        }

                    }

                }

            }

        }

    }

}
