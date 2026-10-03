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
    // -----------------------------------------------------------------------------------------------------------------
    // LE CHEMIN DE LECTURE : le « train » qui parcourt la gamme, de note en note.
    // Roger : « quand ils sont joues, un truc se fasse de point en point en partant de la tonique. Ca aide l'utilisateur
    // a se reperer dans le cercle et a voir quelle note est en train d'etre jouee. En plus ca va faire de jolies formes
    // geometriques a la fin. »
    // LANCE le parcours : la duree vient du CONTROLEUR, donc du domaine - c'est le temps que la gamme met vraiment a
    // sonner, et une constante ecrite ici mentirait le jour ou le tempo change.
    // LE DESSIN DU PARCOURS : la trainee et la tete, DERRIERE les pastilles - elles doivent rester lisibles pendant que
    // le chemin se dessine.
    // LES DEGRES, dans l'ordre ou ils sonnent : la gamme monte puis descend, comme le domaine la joue. Le chemin en
    // INDICES DU CERCLE s'en deduit, parce que chaque case porte le degre de sa note (voir describeModeCircle).
    // p_degrees est OPTIONNEL : le banc d'essai joue la gamme MONTEE puis DESCENDUE, et l'exercice la joue MONTE seulement
    // (une question doit tenir en quelques secondes). Le chemin suit ce qu'on entend, donc il se règle avec lui.
    // n notes se rejoignent par n-1 intervalles : le dernier pas n'est pas un deplacement.
    // LA TEINTE DIT LE DEGRE : du jaune de la tonique vers le vert du septieme, en suivant l'ORDRE DES DEGRES. C'est la
    // demande de Roger - « changer les couleurs de la note en degrade du jaune vers le vert [...] le degrade suivant
    // l'ordre des degres de la gamme, histoire d'avoir l'aspect reellement visuel du mode dans le cercle ».
    // DU JAUNE (50 degres de teinte) VERS LE VERT (140), en traversant une teinte FRANCHE.
    // La premiere version interpolait betement les deux couleurs de la palette, et elles sont CLAIRES toutes les deux :
    // le milieu du degrade tombait donc sur un vert-jaune delave, et Roger l'a vu tout de suite - « le jaune vert le
    // vert donne un truc bizarre, comme si le degrade etait trop leger ». Ce n'etait pas une impression : deux
    // pastels melanges donnent un pastel, jamais une couleur.

    id: root

    // Les douze entrees, dans l'ordre des quintes en partant de la tonique.
    property var notes: []
    // Le diametre des pastilles. L'exercice les fait un peu plus petites : il a une consigne et un verdict a afficher
    // au-dessus.
    property int dotSize: 40
    // La taille du dessin, parametrable : l'ecran de l'exercice est deja charge - consigne, boutons de reponse, verdict -
    // et une roue de 260 pixels n'y tiendrait pas. La tonique reste EN HAUT quelle que soit la taille.
    property int span: 260
    // Le clic : vrai quand l'ecran ATTEND une note. La roue est alors un clavier, et plus seulement un dessin - c'est la
    // demande de Roger pour la note etrangere : « on a deja le cercle en haut avec toutes les notes de la gamme allumees.
    // Il suffit d'appuyer sur un de ces boutons non ? »
    property bool selectable: false
    // LE PAS DE L'INTRUS, quand il y en a un : -1 le reste du temps. L'ecran seul sait si la reponse est donnee, donc il
    // le dit ; la roue, elle, ne fait que le colorer.
    property int foreignStep: -1
    // OU EST L'INTRUS DANS LA ROUE, et ou est la note caracteristique. Deux indices, calcules une fois, plutot qu'une
    // recherche par pastille : douze pastilles feraient douze parcours pour la meme reponse.
    readonly property int foreignCircleIndex: {
        for (var index = 0; index < notes.length; ++index) {
            if (notes[index].stepIndex === foreignStep)
                return index;

        }
        return -1;
    }
    readonly property int characteristicCircleIndex: {
        for (var index = 0; index < notes.length; ++index) {
            if (notes[index].isCharacteristic)
                return index;

        }
        return -1;
    }
    readonly property real radius: (Math.min(width, height) / 2) - (dotSize / 2) - 4
    // TREIZE DEGRES, et c'est une CORRECTION : la gamme montee puis descendue partage sa note du haut, donc elle compte
    // treize notes et non quatorze (voir modeScaleUpAndDown). Le degre 6 y figurait deux fois de suite, ce qui ajoutait un
    // bond de longueur NULLE - la tete restait donc un pas en arriere jusqu'a la fin, en plus d'attendre sur place au
    // sommet.
    property var playbackDegrees: [0, 1, 2, 3, 4, 5, 6, 5, 4, 3, 2, 1, 0]
    property real playbackHead: 0
    property bool playing: false
    readonly property var playbackPath: {
        var path = [];
        for (var step = 0; step < playbackDegrees.length; ++step) {
            for (var index = 0; index < notes.length; ++index) {
                if (notes[index].stepIndex === playbackDegrees[step]) {
                    path.push(index);
                    break;
                }
            }
        }
        return path;
    }

    // Le pas de la gamme de la note choisie : c'est ce que le domaine a mis dans chaque case (voir describeModeCircle).
    signal noteChosen(int p_stepIndex)

    // Le septieme est le dernier pas de l'echelle des teintes : la gamme se lit alors comme un degrade continu, et la
    // tonique garde le jaune qu'elle avait.
    function degreeColour(p_stepIndex, p_degreeCount) {
        if (p_stepIndex < 0)
            return "#6fd08a";

        // Ici la TEINTE bouge et la saturation ne bouge plus : meme quantite de jaune et de vert, mais une couleur qu'on
        // peut NOMMER - et sept pastilles qui se distinguent l'une de l'autre.
        var count = ((modelData !== undefined) && (modelData.degreeCount !== undefined)) ? modelData.degreeCount : 7;
        var ratio = Math.max(0, Math.min(1, p_stepIndex / Math.max(2, count - 1)));
        return Qt.hsla(0.14 + (0.25 * ratio), 0.68, 0.56, 1);
    }

    // Le centre d'une CASE, en coordonnees locales. Une seule fonction, donc le dessin du chemin et celui de la tete ne
    // peuvent pas diverger.
    function dotCentre(p_circleIndex) {
        var angle = ((-90 + (p_circleIndex * 30)) * Math.PI) / 180;
        return Qt.point((width / 2) + (Math.cos(angle) * radius), (height / 2) + (Math.sin(angle) * radius));
    }

    // DEUX NOMBRES, ET NON UNE DUREE TOTALE. Roger : « la boule des lignes dans les modes est un peu lente par rapport au
    // son ». Le parcours etait cale sur la duree TOTALE de la musique, or celle-ci commence et finit par un BOURDON SEUL :
    // la tete partait donc avec le bourdon, et finissait dans le silence qui le suit - elle arrivait sur la derniere
    // pastille alors que la musique etait finie. Elle est maintenant calee sur ce qu'on ENTEND : le silence d'entree, puis
    // un pas par note. Elle arrive donc sur chaque pastille a l'instant ou la note sonne.
    function startPlayback(p_leadInMs, p_noteStepMs, p_degrees) {
        if (p_degrees !== undefined)
            playbackDegrees = p_degrees;

        // Le silence d'entree : la tete reste SUR la tonique pendant que le bourdon s'installe. C'est vrai - c'est ce
        // qu'on entend - et c'est aussi ce qui se regarde : on voit d'ou l'on part.
        leadInPause.duration = Math.max(0, p_leadInMs);
        // LA DUREE SE POSE SUR L'ANIMATION, et c'est une CORRECTION : `restart()` ne prend AUCUN argument, donc celle que
        // je lui passais etait ignoree - et le parcours se faisait en une fraction de seconde. Roger l'a vu avant moi :
        // « le trait se dessine hyper vite, genre en une fraction de seconde ».
        headAnimation.duration = Math.max(1, (playbackPath.length - 1) * p_noteStepMs);
        playbackHead = 0;
        playing = true;
        startAnimation.restart();
    }

    implicitWidth: span
    implicitHeight: span

    // Le silence d'entree, PUIS le parcours : deux temps, et un seul depart. C'est une SEQUENCE, et non une animation dont
    // la duree engloberait le silence : sinon la tete avancerait pendant que rien ne sonne - exactement le defaut que
    // Roger a entendu.
    SequentialAnimation {
        id: startAnimation

        PauseAnimation {
            id: leadInPause

            duration: 0
        }

        NumberAnimation {
            id: headAnimation

            target: root
            property: "playbackHead"
            from: 0
            to: Math.max(0, root.playbackPath.length - 1)
            // Une duree par DEFAUT, posee par startPlayback avant chaque depart. Une animation a zero dure une fraction
            // de seconde, et c'est exactement ce que Roger a entendu.
            duration: 2000
            easing.type: Easing.Linear
        }

    }

    // Un Canvas, comme le camembert des statistiques : aucun module a deployer pour un trait. Il est repeint a chaque
    // mouvement de la tete, et c'est le seul moyen de faire GRANDIR la trainee : une trainee figee ne montrerait pas le
    // chemin, et c'est le chemin qui apprend quelque chose.
    Canvas {
        id: trailCanvas

        anchors.fill: parent
        visible: root.playing
        onPaint: {
            var context = getContext("2d");
            context.reset();
            if (!root.playing || (root.playbackPath.length < 2))
                return ;

            var head = Math.min(root.playbackHead, root.playbackPath.length - 1);
            var whole = Math.floor(head);
            var fraction = head - whole;
            context.strokeStyle = "#ffffff";
            context.lineWidth = 2.5;
            context.lineCap = "round";
            context.lineJoin = "round";
            context.beginPath();
            var start = root.dotCentre(root.playbackPath[0]);
            context.moveTo(start.x, start.y);
            for (var index = 1; index <= whole; ++index) {
                var point = root.dotCentre(root.playbackPath[index]);
                context.lineTo(point.x, point.y);
            }
            // Le segment EN COURS, interpole : entre deux notes, la tete est presque toujours au milieu d'un trait.
            var nextIndex = Math.min(whole + 1, root.playbackPath.length - 1);
            var from = root.dotCentre(root.playbackPath[whole]);
            var to = root.dotCentre(root.playbackPath[nextIndex]);
            if (fraction > 0)
                context.lineTo(from.x + ((to.x - from.x) * fraction), from.y + ((to.y - from.y) * fraction));

            context.stroke();
            // LA TETE, un rond plein qui se voit de loin : c'est elle qu'on suit des yeux.
            context.beginPath();
            context.arc(from.x + ((to.x - from.x) * fraction), from.y + ((to.y - from.y) * fraction), root.dotSize * 0.22, 0, 2 * Math.PI);
            context.fillStyle = "#ffffff";
            context.fill();
        }

        // Un Canvas ne se repeint pas tout seul : c'est le mouvement de la tete qui le lui dit.
        Connections {
            function onPlaybackHeadChanged() {
                trailCanvas.requestPaint();
            }

            target: root
        }

    }

    Repeater {
        model: root.notes

        delegate: Rectangle {
            required property var modelData
            required property int index
            // Trente degres par case, et la case 0 - la tonique - EN HAUT : la convention de toutes les roues imprimees.
            readonly property real angle: ((-90 + (index * 30)) * Math.PI) / 180
            // La couleur dit l'appartenance au mode, et rien d'autre : la tonique est doree, les six autres notes du mode
            // sont vertes, et tout ce qui n'en fait pas partie reste eteint - c'est ce qui fait voir l'armure.
            // DEUX EXCEPTIONS, ET ELLES SONT ROUGES TOUTES LES DEUX parce qu'elles disent la meme chose : « regarde
            // celle-la ». La note CARACTERISTIQUE - celle qui distingue le mode de ses voisins - et, sur une question de
            // note etrangere, L'INTRUS. Roger a demande la couleur pour l'intrus avant tout le reste : « il y a beaucoup
            // de texte pendant la reponse, beaucoup trop pour etre lu dans le temps imparti - la couleur est plus
            // parlante ». Le texte reste, mais il n'est plus le seul a parler.
            readonly property color dotColour: {
                if (!modelData.inMode)
                    return "#2a1b45";

                if (index === root.foreignCircleIndex)
                    return "#ff3b30";

                if (index === root.characteristicCircleIndex)
                    return "#ff7a8a";

                return root.degreeColour(modelData.stepIndex, modelData.degreeCount);
            }

            x: (root.width / 2) + (Math.cos(angle) * root.radius) - (width / 2)
            y: (root.height / 2) + (Math.sin(angle) * root.radius) - (height / 2)
            width: root.dotSize
            height: root.dotSize
            radius: width / 2
            color: dotColour
            border.width: modelData.isTonic ? 2 : 1
            border.color: modelData.isTonic ? "#fff3c4" : "#4a3170"

            Text {
                anchors.centerIn: parent
                text: modelData.name
                color: modelData.inMode ? "#1d1033" : "#6f5b93"
                // La taille du texte SUIT celle de la pastille : agrandir la roue sans agrandir ses noms ne servirait a rien,
                // et c'est la pastille entiere qui doit etre lisible - elle est un bouton quand l'ecran attend une note.
                font.pixelSize: Math.round(root.dotSize * 0.36)
                font.bold: modelData.isTonic
            }

            // Le clic n'existe que si l'ecran ATTEND une note, et seulement sur une note de la gamme : une pastille eteinte
            // n'est pas un choix, et la rendre cliquable ferait croire le contraire.
            MouseArea {
                anchors.fill: parent
                enabled: root.selectable && (modelData.stepIndex >= 0)
                onClicked: root.noteChosen(modelData.stepIndex)
            }

        }

    }

}
