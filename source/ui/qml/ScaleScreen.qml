// =====================================================================================================================
// Musichien - ScaleScreen
// LA PAGE DES GAMMES : une gamme dans le cercle, un bourdon, un nom a retrouver.
// C'est une page de PRATIQUE, et elle le dit par ce qu'elle n'a pas : pas de vies, pas de fin, pas de score a sauver. On
// ecoute autant de fois qu'on veut, on repond, on recommence. Dediee a ceux qui connaissent deja les gammes et veulent
// les entendre - c'est le seul endroit du jeu ou l'on suppose la connaissance.

// LE MODULE DE L'APPLICATION, ET C'EST LUI QUI DONNE ACCES AUX CONTROLEURS : `ScaleController` est un singleton
// ENREGISTRE, il n'existe donc pour une page que si elle importe le module qui le declare. Un composant du meme dossier
// (ModeCircle) se resout tout seul ; un singleton, non - et l'erreur a l'execution est un simple « is not defined »,
// qui laisse la page BLANCHE et sans boutons plutot que de la faire planter. C'est exactement ce qui s'est passe.
import Musichien
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

// LE CERCLE EST LE MEME COMPOSANT QUE CELUI DES MODES, et il ne sait rien de ce qu'il dessine : il recoit des pastilles et
// les peint. Une gamme de cinq notes s'y dessine aussi bien qu'un mode de sept.
// =====================================================================================================================
Item {
    // LE FOND APPARTIENT A L'ECRAN, PAS AU DIALOGUE.
    // Les dialogues de cette application n'ont pas de fond a eux : ils laissent voir la feuille BLANCHE du style, et c'est
    // chaque page qui peint la sienne - KeyCircleScreen et l'accordeur font exactement cela. Ma page ne le faisait pas, et
    // Roger a vu « le fond tout blanc » : ses textes clairs sur une feuille blanche, donc illisibles.

    id: scaleScreen

    anchors.fill: parent

    // La couleur est celle de KeyCircleScreen, et ce n'est pas une coincidence : deux pages de reference doivent se
    // ressembler, sinon le joueur croit avoir change d'application.
    Rectangle {
        anchors.fill: parent
        color: "#1d1033"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 10

        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            color: "#e8dcff"
            font.pixelSize: 15
            text: qsTr("Écoute la gamme, puis retrouve son nom. Tu peux la réécouter autant de fois que tu veux : ici, on ne devine pas, on écoute.")
        }

        ModeCircle {
            id: scaleCircle

            Layout.alignment: Qt.AlignHCenter
            span: 262
            dotSize: 42
            notes: ScaleController.circle
            // AUCUNE PASTILLE N'EST CLIQUABLE ICI : le joueur repond avec des NOMS, pas avec des notes. La roue est un
            // dessin - et c'est justement ce qu'on veut lui apprendre a lire.
            selectable: false
        }

        Button {
            Layout.fillWidth: true
            text: qsTr("♪ Réécouter")
            onClicked: ScaleController.playAgain()
        }

        Repeater {
            model: ScaleController.choices

            delegate: Button {
                required property var modelData
                required property int index

                Layout.fillWidth: true
                // LA BONNE REPONSE SE MONTRE APRES COUP, et les autres s'effacent : le joueur voit ce qu'il a rate sans
                // qu'on lui fasse relire quatre lignes pour le trouver.
                highlighted: ScaleController.hasAnswered && (index === ScaleController.correctIndex)
                enabled: !ScaleController.hasAnswered
                opacity: ScaleController.hasAnswered && (index !== ScaleController.correctIndex) ? 0.45 : 1
                text: modelData
                onClicked: ScaleController.answer(index)
            }

        }

        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            color: ScaleController.wasLastAnswerCorrect ? "#8ef2b0" : "#ff8fb0"
            font.pixelSize: 16
            visible: ScaleController.hasAnswered
            text: ScaleController.verdict
        }

        Button {
            Layout.fillWidth: true
            visible: ScaleController.hasAnswered
            highlighted: true
            text: qsTr("La suivante")
            onClicked: ScaleController.next()
        }

        Item {
            Layout.fillHeight: true
        }

        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            color: "#8a77ad"
            font.pixelSize: 12
            text: qsTr("%1 justes sur %2 depuis l'ouverture").arg(ScaleController.correctCount).arg(ScaleController.askedCount)
        }

    }

}
