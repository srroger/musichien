// =====================================================================================================================
// ChordTreeView - l'arbre des accords, dessine UNE fois pour deux ecrans
// Il sert a deux choses, et c'est la meme : la CARTE qu'on consulte, et la GRILLE de reponse d'une question d'accord.
// Repondre dans un arbre, c'est deja comprendre l'accord qu'on cherche : c'est mieux qu'une grille de noms.
// Deux ecrans, un seul dessin : le composant est ecrit ici, et le dupliquer dans Main.qml et ExerciseScreen.qml serait
// la meilleure facon de les faire diverger.
// ---------------------------------------------------------------------------------------------------------------------
// La disposition

// La PROFONDEUR d'un noeud donne sa colonne ; sa LIGNE, elle, se calcule a partir de son PARENT, et c'est ce qui rend
// les branches visibles. Un enfant se pose sur la ligne de son parent quand la place est libre dans sa colonne : le C9
// vient donc a droite du C7, et la chaine C - Cm - Cdim - Cm7b5 se lit d'un seul regard sur la premiere ligne.
// Une seule regle suffit : on s'aligne sur son parent, et l'on s'ecarte d'une ligne quand - et seulement quand - une
// grande branche se referme pour en ouvrir une autre. Une colonne empilee toute seule serait plus courte, mais elle ne
// montrerait plus aucune branche.
// =====================================================================================================================
import Musichien
import QtQuick
import QtQuick.Layouts

Item {
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
    // Les lignes de tous les noeuds, calculees une seule fois : voir computeRows(), plus bas.
    readonly property var nodeRows: computeRows()

    signal qualityChosen(int p_quality)

    // Chaque noeud s'aligne sur SON PARENT, et n'est Ecarte que lorsque sa ligne est deja prise dans sa colonne. Deux
    // regles d'ecart, et elles suffisent a rendre les branches visibles :
    //   * un frere qui n'a pas de descendance est suivi une ligne plus bas ;
    //   * un frere qui EN a une laisse une ligne vide, et la branche suivante ne commence jamais avant que la
    //     precedente ait fini de descendre (d'ou le maximum des deux).
    // C'est ce qui fait tenir l'arbre en DIX lignes au lieu de quinze, tout en montrant l'escalier des branches - la
    // version « chaque colonne empilee toute seule » en faisait huit, mais ne montrait plus aucune branche du tout.
    function computeRows() {
        var nodes = ExerciseController.chordTree;
        var state = {
            "rows": [],
            "ends": [],
            "children": []
        };
        for (var index = 0; index < nodes.length; ++index) {
            // -1 veut dire « pas encore pose », et c'est ce que lit rowIsFree.
            state.rows.push(-1);
            state.ends.push(-1);
            state.children.push([]);
        }
        for (var index = 0; index < nodes.length; ++index) {
            if (!nodes[index].isRoot)
                state.children[nodes[index].parentIndex].push(index);

        }
        placeSubtree(state, 0, 0);
        return state.rows;
    }

    // La fonction pose un noeud puis toute sa descendance, et rend la derniere ligne utilisee par ce sous-arbre : c'est
    // elle qui dit a la branche suivante ou commencer.
    function placeSubtree(p_state, p_index, p_minimumRow) {
        var nodes = ExerciseController.chordTree;
        var node = nodes[p_index];
        var row = node.isRoot ? 0 : Math.max(p_minimumRow, p_state.rows[node.parentIndex]);
        while (!rowIsFree(p_state, node.depth, row))++row
        p_state.rows[p_index] = row;
        var end = row;
        for (var rank = 0; rank < p_state.children[p_index].length; ++rank) {
            var child = p_state.children[p_index][rank];
            // Le premier enfant s'aligne sur son parent ; les suivants se rangent sous les precedents.
            var next = row;
            if (rank > 0) {
                var brother = p_state.children[p_index][rank - 1];
                if (p_state.children[brother].length > 0)
                    next = Math.max(p_state.rows[brother] + 2, p_state.ends[brother]);
                else
                    next = p_state.rows[brother] + 1;
            }
            end = Math.max(end, placeSubtree(p_state, child, next));
        }
        p_state.ends[p_index] = end;
        return end;
    }

    // Cette ligne est-elle libre dans cette colonne ?
    function rowIsFree(p_state, p_depth, p_row) {
        var nodes = ExerciseController.chordTree;
        for (var index = 0; index < nodes.length; ++index) {
            if (p_state.rows[index] === p_row && nodes[index].depth === p_depth)
                return false;

        }
        return true;
    }

    // La position d'un noeud : sa PROFONDEUR est sa colonne, sa ligne vient du calcul ci-dessus.
    function nodeX(p_index) {
        return ExerciseController.chordTree[p_index].depth * (nodeWidth + columnGap);
    }

    function nodeY(p_index) {
        return nodeRows[p_index] * (nodeHeight + rowGap);
    }

    // La hauteur de tout l'arbre : la ligne la plus basse, et un peu d'air en bas.
    function treeHeight() {
        var lastRow = 0;
        for (var index = 0; index < nodeRows.length; ++index) {
            lastRow = Math.max(lastRow, nodeRows[index]);
        }
        return ((lastRow + 1) * (nodeHeight + rowGap)) + 12;
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
