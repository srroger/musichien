// =====================================================================================================================
// ChordTreeView - l'arbre des accords, dessine UNE fois pour deux ecrans
// Il sert a deux choses, et c'est la meme : la CARTE qu'on consulte, et la GRILLE de reponse d'une question d'accord.
// Roger : « ce serait beaucoup mieux que de garder la grille simple un peu moche » - et il a raison, parce que repondre
// dans un arbre, c'est deja comprendre l'accord qu'on cherche.
// Deux ecrans, un seul dessin : le composant est ecrit ici, et le dupliquer dans Main.qml et ExerciseScreen.qml serait
// la meilleure facon de les faire diverger.
// ---------------------------------------------------------------------------------------------------------------------
// La disposition

// La PROFONDEUR d'un noeud donne sa colonne, son RANG dans la liste donne sa ligne. L'arbre vient du domaine range par
// profondeur, donc les enfants directs d'une couleur se suivent, et une petite MARCHE se glisse chaque fois que la
// profondeur change - elle dit « nouvelle branche » sans gaspiller tout un trou, ce que Roger a signale sur la branche
// des suspendus.
// =====================================================================================================================
import Musichien
import QtQuick
import QtQuick.Layouts

Item {
    // Le RANG d'un noeud dans SA COLONNE : c'est ce qui fait tenir l'arbre en hauteur.
    // Roger : « le but etait que l'arbre prenne moins de place. Il faudrait que les Dim, m5, mMaj7 et C9 soient en haut
    // aussi, et pas en bas. En gros il faut que l'arbre prenne le moins de place. »

    id: chordTreeView

    // Vrai quand les couleurs OFFERTES doivent etre cliquables : c'est le cas pendant une question d'accord, ou l'arbre
    // sert a repondre. Faux sur la carte, ou il sert a comprendre.
    property bool interactive: false
    // Les qualites offertes, quand interactive est vrai. Les autres restent VISIBLES mais ETEINTES, et c'est demande :
    // « garder les boutons vides plutot que rien du tout... histoire qu'on voie quand meme le schema des mutations ».
    property var offeredQualities: []
    // Les degres sous le nom : sur la carte ils apprennent quelque chose, pendant une partie ils voleraient la place de
    // la reponse.
    property bool showDegrees: true
    // Les mesures. La largeur suit l'ecran : trois colonnes doivent tenir cote a cote, et la colonne la plus profonde est
    // la troisieme.
    readonly property real columnGap: 14
    readonly property real rowGap: 5
    readonly property real nodeWidth: Math.max(62, (width - (3 * columnGap)) / 4)
    readonly property real nodeHeight: showDegrees ? 38 : 30

    signal qualityChosen(int p_quality)

    // Chaque colonne empile donc SES noeuds, independamment des autres : la troisieme colonne commence a la meme hauteur
    // que la deuxieme, et non apres elle. L'arbre passe de QUINZE lignes a HUIT, sans qu'un seul noeud disparaisse - et
    // c'est toute la difference entre une liste mise en colonnes et un arbre qui tient dans un ecran de telephone.
    function nodeRow(p_index) {
        var depth = ExerciseController.chordTree[p_index].depth;
        var row = 0;
        for (var index = 0; index < p_index; ++index) {
            if (ExerciseController.chordTree[index].depth === depth)
                ++row;

        }
        return row;
    }

    // Combien de noeuds dans une colonne : c'est la HAUTEUR de l'arbre, et non le nombre de couleurs.
    function columnCount(p_depth) {
        var count = 0;
        for (var index = 0; index < ExerciseController.chordTree.length; ++index) {
            if (ExerciseController.chordTree[index].depth === p_depth)
                ++count;

        }
        return count;
    }

    // La position d'un noeud : sa PROFONDEUR est sa colonne, son RANG DANS SA COLONNE est sa ligne.
    function nodeX(p_index) {
        return ExerciseController.chordTree[p_index].depth * (nodeWidth + columnGap);
    }

    function nodeY(p_index) {
        return nodeRow(p_index) * (nodeHeight + rowGap);
    }

    // La hauteur de tout l'arbre : la colonne la plus longue, et un peu d'air en bas.
    function treeHeight() {
        var rows = 0;
        for (var depth = 0; depth < 4; ++depth) {
            rows = Math.max(rows, columnCount(depth));
        }
        return (rows * (nodeHeight + rowGap)) + 12;
    }

    // Cette couleur est-elle une reponse possible ?
    function isOffered(p_quality) {
        for (var index = 0; index < offeredQualities.length; ++index) {
            if (offeredQualities[index].quality === p_quality)
                return true;

        }
        return false;
    }

    implicitHeight: treeHeight()

    // Les LIENS d'abord : ils passent DERRIERE les noeuds, comme les branches d'un arbre passent derriere ses feuilles.
    // Un Canvas plutot qu'une pile de rectangles : trois segments par branche, en quinze branches, cela ferait
    // quarante-cinq rectangles a tenir a jour.
    Canvas {
        id: links

        anchors.fill: parent
        onPaint: {
            var context = getContext("2d");
            context.reset();
            context.strokeStyle = "#4a3670";
            context.lineWidth = 2;
            var nodes = ExerciseController.chordTree;
            for (var index = 0; index < nodes.length; ++index) {
                var node = nodes[index];
                if (node.isRoot)
                    continue;

                var parent = node.parentIndex;
                var startX = chordTreeView.nodeX(parent) + chordTreeView.nodeWidth;
                var startY = chordTreeView.nodeY(parent) + (chordTreeView.nodeHeight / 2);
                var endX = chordTreeView.nodeX(index);
                var endY = chordTreeView.nodeY(index) + (chordTreeView.nodeHeight / 2);
                // Le coude : la branche descend le long d'une colonne invisible, entre les deux.
                var elbow = endX - (chordTreeView.columnGap / 2);
                context.beginPath();
                context.moveTo(startX, startY);
                context.lineTo(elbow, startY);
                context.lineTo(elbow, endY);
                context.lineTo(endX, endY);
                context.stroke();
            }
        }
    }

    // Puis les NOEUDS, poses PAR-DESSUS les branches.
    Repeater {
        model: ExerciseController.chordTree

        delegate: Rectangle {
            id: node

            required property var modelData
            required property int index
            // Une couleur est OFFERTE quand elle fait partie des reponses possibles. Hors partie - sur la carte - toutes
            // le sont, puisque le but est justement de les voir toutes.
            readonly property bool offered: !chordTreeView.interactive || chordTreeView.isOffered(modelData.quality)

            x: chordTreeView.nodeX(index)
            y: chordTreeView.nodeY(index)
            width: chordTreeView.nodeWidth
            height: chordTreeView.nodeHeight
            radius: 8
            // Un noeud offert porte la couleur de sa famille ; un noeud qui ne l'est pas reste SOMBRE, avec un simple
            // liseré : le schema des mutations reste lisible, et l'oeil ne peut pas le confondre avec une reponse.
            color: node.offered ? ExerciseController.chordColourName(modelData.quality) : "#1d1136"
            border.width: node.offered ? (modelData.isRoot ? 3 : 0) : 1
            border.color: node.offered ? "#ffd479" : "#3f2d63"

            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width - 8
                spacing: 0

                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    color: node.offered ? "#2b1b47" : "#6b5a8f"
                    font.pixelSize: chordTreeView.showDegrees ? 15 : 13
                    font.bold: true
                    text: modelData.name
                }

                // LES DEGRES sous le nom : « Cm » et « 1 b3 5 ». C'est ce qui reste quand on a oublie l'accord.
                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    visible: chordTreeView.showDegrees
                    color: "#4a3670"
                    font.pixelSize: 10
                    text: modelData.degrees
                }

            }

            MouseArea {
                anchors.fill: parent
                // On ne repond que sur une couleur offerte : toucher un noeud eteint ne doit rien faire du tout - ni
                // repondre, ni faire semblant.
                enabled: chordTreeView.interactive && node.offered
                onClicked: chordTreeView.qualityChosen(modelData.quality)
            }

        }

    }

}
