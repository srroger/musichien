// =====================================================================================================================
// Musichien - GlossaryScreen
// LE GLOSSAIRE : un dictionnaire, et rien d'autre.
// Roger, 04/10/2026 : « je veux que ca fasse vraiment liste de dico, pas des chapitres ou des icones ou de mise en page
// particuliere. Juste une grosse liste par ordre alphabetique. [...] Normalement on devrait savoir utiliser un
// dictionnaire. Pas de recherche, c'est inutile. »
// Ce qui suit est donc volontairement PAUVRE : une lettre, un mot, une ligne. Aucun champ de recherche, aucune icone par
// entree, aucun pliage - tout cela ajouterait des manieres de faire la ou il n'y en a qu'une : descendre.
// L'ORDRE ET LES LETTRES NE SONT PAS CALCULES ICI. Le tri d'un dictionnaire francais - sans casse, sans accents - vit
// dans GlossaryController, ou il se teste sans interface. Cette page ne fait que dessiner ce qu'on lui donne.
// =====================================================================================================================

import Musichien
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: glossaryScreen

    // La page demande a sortir, et celui qui la porte decide comment.
    signal closeRequested()

    anchors.fill: parent

    Rectangle {
        anchors.fill: parent
        color: "#14231c"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        // La barre de navigation d'Android passe par-dessus la page : sans cette marge, le dernier mot se retrouve
        // DERRIERE les boutons du systeme.
        anchors.bottomMargin: 48
        spacing: 10

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
                    color: "#9dffc4"
                    font.pixelSize: 20
                    font.bold: true
                    text: qsTr("🐟  Le Glossaire")
                }

                Text {
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: "#7fa08f"
                    font.pixelSize: 12
                    text: qsTr("%1 mots, comme dans un dictionnaire.").arg(GlossaryController.termCount)
                }

            }

            Rectangle {
                Layout.preferredWidth: 44
                Layout.preferredHeight: 36
                radius: 8
                color: "#1c3a2c"

                Text {
                    anchors.centerIn: parent
                    color: "#c8ead8"
                    font.pixelSize: 22
                    // MULTIPLICATION SIGN, pas une croix de symbole : U+00D7 est en Latin-1 et vit dans toutes les
                    // polices, alors que '✕' n'est pas dans Quicksand - la police imposee par l'interface.
                    text: qsTr("×")
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: glossaryScreen.closeRequested()
                }

            }

        }

        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: wordColumn.height

            ColumnLayout {
                id: wordColumn

                // La largeur vient du Flickable, qui est deja a la taille de la page. Pas de ScrollView ici : la barre
                // horizontale n'a rien a faire, et son contenu ne prendrait pas la largeur du viewport.
                width: parent.width
                spacing: 0

                Repeater {
                    model: GlossaryController.entries

                    delegate: ColumnLayout {
                        required property var modelData
                        // L'INDEX DOIT ETRE DECLARE, et pas seulement lu : Qt 6 ne le donne plus comme une propriete de
                        // contexte des qu'un « required property » est declare. Sans cette ligne, le premier mot de
                        // chaque section affichait un « index is not defined » dans le journal - et le separateur de
                        // lettre ne se posait plus qu'a moitie.
                        required property int index

                        Layout.fillWidth: true
                        spacing: 1

                        // LA LETTRE DE SECTION : le repere qui fait qu'on descend vite sans se perdre. Elle ne se pose
                        // que sur le PREMIER mot de sa lettre, et le calcul n'est pas refait ici.
                        Text {
                            Layout.fillWidth: true
                            Layout.topMargin: (modelData.startsSection && index > 0) ? 16 : 0
                            visible: modelData.startsSection
                            color: "#9dffc4"
                            font.pixelSize: 13
                            font.bold: true
                            text: qsTr("── %1 ──").arg(modelData.sectionLetter)
                        }

                        // LE MOT, puis sa definition, et on descend. C'est tout ce qu'un dictionnaire fait.
                        Text {
                            Layout.preferredWidth: 0
                            Layout.minimumWidth: 0
                            Layout.fillWidth: true
                            Layout.topMargin: 6
                            wrapMode: Text.WordWrap
                            color: "#e8fff2"
                            font.pixelSize: 15
                            font.bold: true
                            text: modelData.word
                        }

                        Text {
                            Layout.preferredWidth: 0
                            Layout.minimumWidth: 0
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: "#a9c4b6"
                            font.pixelSize: 13
                            text: modelData.definition
                        }

                    }

                }

            }

        }

    }

}
