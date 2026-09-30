// =====================================================================================================================
// Musichien - ModeCircle
// Le cercle des quintes d'un mode : ses sept notes allumees, les autres eteintes, la tonique marquee.
// ---------------------------------------------------------------------------------------------------------------------
// Pourquoi la case 0 est la tonique, et en haut
// Parce que c'est ce qui rend le dessin lisible : dans l'ordre des quintes, les sept notes d'un mode sont VOISINES - une
// armure, c'est exactement cela - donc l'arc allume est toujours d'un seul tenant. Ce qui change d'un mode a l'autre est
// la place de la tonique dans cet arc : do ionien la met au deuxieme rang, re dorien au quatrieme.
// Le cercle « tourne » donc avec la tonique, et le mode devient une POSITION. C'est la facon dont une oreille le
// reconnait, et c'est ce que ce dessin donne a voir.
// ---------------------------------------------------------------------------------------------------------------------
// Ce que l'ecran lui donne, et ce qu'il ne calcule pas

import QtQuick

// 'notes' vient du domaine, par ModeDescription : { name, inMode, isTonic }. Aucune regle musicale n'est ecrite ici - ni
// l'ordre des quintes, ni l'appartenance au mode. Un dessin qui recalculerait ce qu'il montre finirait par mentir sur ce
// qu'il a fait entendre.
// =====================================================================================================================
Item {
    id: root

    // Les douze entrees, dans l'ordre des quintes en partant de la tonique.
    property var notes: []
    // Le diametre des pastilles. L'exercice les fait un peu plus petites : il a une consigne et un verdict a afficher
    // au-dessus.
    property int dotSize: 40
    // Le rayon laisse la place a la pastille : sans cela, la case du haut et celle du bas sortiraient du cadre.
    readonly property real radius: (Math.min(width, height) / 2) - (dotSize / 2) - 4

    implicitWidth: 260
    implicitHeight: 260

    Repeater {
        model: root.notes

        delegate: Rectangle {
            required property var modelData
            required property int index
            // Trente degres par case, et la case 0 - la tonique - EN HAUT : la convention de toutes les roues imprimees.
            readonly property real angle: ((-90 + (index * 30)) * Math.PI) / 180

            x: (root.width / 2) + (Math.cos(angle) * root.radius) - (width / 2)
            y: (root.height / 2) + (Math.sin(angle) * root.radius) - (height / 2)
            width: root.dotSize
            height: root.dotSize
            radius: width / 2
            // La couleur dit l'appartenance au mode, et rien d'autre : la tonique est doree, les six autres notes du mode
            // sont vertes, et tout ce qui n'en fait pas partie reste eteint - c'est ce qui fait voir l'armure.
            color: modelData.inMode ? (modelData.isTonic ? "#ffd479" : "#6fd08a") : "#2a1b45"
            border.width: modelData.isTonic ? 2 : 1
            border.color: modelData.isTonic ? "#fff3c4" : "#4a3170"

            Text {
                anchors.centerIn: parent
                text: modelData.name
                color: modelData.inMode ? "#1d1033" : "#6f5b93"
                font.pixelSize: 13
                font.bold: modelData.isTonic
            }

        }

    }

}
