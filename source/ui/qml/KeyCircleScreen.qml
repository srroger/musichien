// =====================================================================================================================
// L'ECRAN DU CERCLE DES QUINTES
// Une page de REFERENCE, et pas un exercice : on n'y repond a rien, on y lit. C'est la carte du pilier harmonie, et
// c'est la meme que celle que l'ecran d'exercice dessine deja - la difference est qu'ici elle prend toute la place, et
// qu'on peut l'explorer.
// Deux parties, et elles se lisent ensemble :
//   * la ROUE, douze cases a trente degres, ou chaque case porte une tonalite majeure, son armure et sa relative ;
//   * le CENTRE : la tonalite ouverte, son armure, et sa relative mineure.
// ---------------------------------------------------------------------------------------------------------------------
// Pourquoi les cases sont placees par un ANGLE, et non rangees en colonnes
// Parce que la position d'une case veut dire quelque chose : monter d'une case ajoute un diese, descendre en ajoute un
// bemol, et la case voisine d'une tonalite lui est une quinte au-dessus ou en dessous. Une liste rangee ferait perdre
// exactement ce que le cercle existe pour montrer - et c'est deja la raison pour laquelle la grille de reponse des
// intervalles est, elle aussi, disposee en cercle.
// =====================================================================================================================

import Musichien
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

Item {
    id: keyCircleScreen

    // La meme nuit violette que le reste de l'application.
    Rectangle {
        anchors.fill: parent
        color: "#1d1033"
    }

    ColumnLayout {
        // -------------------------------------------------------------------------------------------------------------
        // LA TONALITE OUVERTE

        anchors.fill: parent
        anchors.margins: 16
        // Et de la place EN BAS, parce que le bas de l'ecran n'appartient pas a l'application : sur un telephone, la
        // barre de navigation du systeme y vit. Roger l'a vu en jouant - « les degres en bas sont trop en bas, ils sont
        // caches par le layout des boutons du telephone ».
        anchors.bottomMargin: 72
        spacing: 8

        Text {
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            horizontalAlignment: Text.AlignHCenter
            color: "#ffffff"
            font.pixelSize: 19
            font.bold: true
            text: qsTr("Le cercle des quintes")
        }

        Text {
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            color: "#cbb8e8"
            font.pixelSize: 12
            text: qsTr("Touche une tonalité : en montant on ajoute un dièse, en descendant un bémol. La relative mineure partage la même armure.")
        }

        // -------------------------------------------------------------------------------------------------------------
        // LA ROUE
        // -------------------------------------------------------------------------------------------------------------
        Item {
            id: wheel

            readonly property real slotSize: Math.min(width, height) * 0.235
            readonly property real radius: (Math.min(width, height) / 2) - (slotSize / 2) - 2

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignHCenter

            // Le CENTRE : la tonalite ouverte, ecrite en grand. C'est le point fixe de la roue, et l'endroit ou l'oeil
            // revient apres avoir touche une case.
            Rectangle {
                anchors.centerIn: parent
                width: parent.slotSize * 1.1
                height: width
                radius: width / 2
                color: "#2a1a4a"
                border.width: 1
                border.color: "#4a3170"

                Column {
                    anchors.centerIn: parent
                    spacing: 1

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        color: "#ffffff"
                        font.pixelSize: 20
                        font.bold: true
                        text: KeyCircleController.selectedKey.name !== undefined ? KeyCircleController.selectedKey.name : qsTr("?")
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        color: "#8ef2b0"
                        font.pixelSize: 10
                        visible: KeyCircleController.selectedKey.armure !== undefined
                        text: KeyCircleController.selectedKey.armure !== undefined ? KeyCircleController.selectedKey.armure : ""
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        color: "#8a77ad"
                        font.pixelSize: 10
                        visible: KeyCircleController.selectedKey.relativeMinor !== undefined
                        text: KeyCircleController.selectedKey.relativeMinor !== undefined ? KeyCircleController.selectedKey.relativeMinor + " m" : ""
                    }

                }

            }

            Repeater {
                // Trente degres par case, et **DO EN HAUT**.

                // Un seul Repeater, PLAT : une entree par case. Chaque case porte son angle, et c'est ce qui la place.
                model: KeyCircleController.keys

                delegate: Rectangle {
                    id: slot

                    required property var modelData
                    // C'est la convention de toutes les roues qu'on trouve imprimees, et ce n'est pas une coquetterie :
                    // do au sommet met les dieses d'un cote et les bemols de l'autre, donc « une alteration de plus »
                    // se lit dans un sens ou dans l'autre sans reflechir. Placer les cases par leur RANG dans la liste
                    // mettrait re bemol en haut, ce qui ne veut rien dire pour personne.
                    readonly property real slotAngleRadians: (-90 + (30 * modelData.accidentals)) * Math.PI / 180
                    readonly property bool isSelected: modelData.index === KeyCircleController.selectedKey.index

                    width: wheel.slotSize
                    height: wheel.slotSize
                    radius: width / 2
                    x: (wheel.width / 2) + (wheel.radius * Math.cos(slotAngleRadians)) - (width / 2)
                    y: (wheel.height / 2) + (wheel.radius * Math.sin(slotAngleRadians)) - (height / 2)
                    // Deux moities de la roue, et c'est ce qu'elle dit : les bemols d'un cote, les dieses de l'autre,
                    // et do au milieu. La case ouverte s'allume par-dessus.
                    color: {
                        if (isSelected)
                            return "#ffd479";

                        if (modelData.accidentals < 0)
                            return "#3a2a5e";

                        if (modelData.accidentals > 0)
                            return "#24415e";

                        return "#4a3170";
                    }
                    border.width: isSelected ? 2 : 1
                    border.color: isSelected ? "#ffd479" : "#6f5b93"

                    Column {
                        anchors.centerIn: parent
                        spacing: 0

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: slot.isSelected ? "#1d1033" : "#ffffff"
                            font.pixelSize: 15
                            font.bold: true
                            text: slot.modelData.name
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: slot.isSelected ? "#1d1033" : "#cbb8e8"
                            font.pixelSize: 8
                            text: slot.modelData.armure
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: slot.isSelected ? "#1d1033" : "#8a77ad"
                            font.pixelSize: 8
                            text: slot.modelData.relativeMinor + " m"
                        }

                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: KeyCircleController.selectKey(slot.modelData.index)
                    }

                }

            }

        }

        // Les sept degres, avec leur chiffre romain et la qualite de l'accord qu'ils portent. C'est le point demontre au
        // §1.1 de la note 17 : les accords d'une gamme ne sont pas un caprice, ce sont TOUJOURS les memes qualites dans
        // le meme ordre - I, IV et V majeurs, ii, iii et vi mineurs, vii diminue. Et c'est vrai dans les douze
        // tonalites, ce qui est bien la definition d'une tonalite majeure.
        // -------------------------------------------------------------------------------------------------------------
        Text {
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            horizontalAlignment: Text.AlignHCenter
            color: "#8a77ad"
            font.pixelSize: 12
            visible: KeyCircleController.selectedKey.name === undefined
            text: qsTr("Touche une case pour ouvrir sa gamme.")
        }

        Flow {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            spacing: 4
            visible: KeyCircleController.selectedKey.name !== undefined

            Repeater {
                model: KeyCircleController.degrees

                delegate: Rectangle {
                    required property var modelData

                    width: 60
                    height: 42
                    radius: 8
                    // La couleur dit la QUALITE, parce que c'est elle qu'on vient lire : majeure chaude, mineure
                    // froide, diminuee sombre. L'oeil apprend les formes en meme temps que l'oreille apprend les sons.
                    color: {
                        if (modelData.qualityIndex === 0)
                            return "#8ef2b0";

                        if (modelData.qualityIndex === 1)
                            return "#7bb0ff";

                        return "#ff8fb0";
                    }

                    Column {
                        anchors.centerIn: parent
                        spacing: 0

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: "#1d1033"
                            font.pixelSize: 14
                            font.bold: true
                            text: modelData.numeral
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: "#1d1033"
                            font.pixelSize: 8
                            text: modelData.quality
                        }

                    }

                }

            }

        }

    }

}
