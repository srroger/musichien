// =====================================================================================================================
// Musichien - CourseSchema
// LES DESSINS DES COURS. Un composant a part, parce qu'un cours a le droit de MONTRER une chose et pas seulement de
// l'ecrire - et parce qu'un dessin se range avec les autres dessins, pas au milieu d'une page.
// ---------------------------------------------------------------------------------------------------------------------
// Pourquoi PEINT et non charge
// Une image du commerce montre un instrument ; un dessin montre le PRINCIPE. Et une image se paie deux fois : en octets
// dans l'APK, et en pixels sur un ecran qu'on ne choisit pas. Ici le trait est net a toutes les tailles, il bouge, et
// il ne coute RIEN.
// ---------------------------------------------------------------------------------------------------------------------
// Un dessin s'ECOUTE
// Chaque schema joue ce qu'il montre, et c'est la meme regle que partout dans ce projet : un cours qui demande de
// comprendre une chose donne le moyen de l'entendre.
//   * le bourdon      -> le bourdon du jeu, tenu
//   * le cercle       -> le TOUR des quintes, la chaine repliee dans l'octave
//   * les deux gammes -> la gamme majeure, sur le bourdon
// =====================================================================================================================

import Musichien
import QtQuick

Item {
    id: courseSchema

    // QUEL DESSIN. Le nom vient DU COURS, et c'est le seul lien entre un fichier de contenu et ce fichier-ci : un cours
    // n'ecrit pas « drawCircle », il ecrit « cercle ».
    property string schemaName: ""
    // CE QUI BOUGE, ET CE QUI NE BOUGE PAS. Un dessin immobile n'a pas besoin d'une animation qui tourne dans le vide,
    // et un telephone n'a pas d'energie a offrir a un point que personne ne regarde.
    readonly property bool animates: schemaName === "bourdon" || schemaName === "cercle"
    // UN NOM QU'ON NE CONNAIT PAS NE DESSINE RIEN. C'est le contrat du contenu : une faute de frappe coute une
    // illustration, jamais la lecon - et surtout, elle n'affiche pas le dessin d'une AUTRE lecon, ce qui serait pire
    // que pas de dessin du tout.
    readonly property bool draws: schemaName === "bourdon" || schemaName === "cercle" || schemaName === "deux-gammes"

    // LA HAUTEUR SUIT LE DESSIN. Un cercle de douze noms a besoin de place pour respirer, deux rangees de sept cases
    // non : sans ca, les trois premiers noms du cercle se chevaucheraient sur un telephone.
    implicitHeight: schemaName === "cercle" ? 300 : 168

    Rectangle {
        id: schemaFrame

        anchors.fill: parent
        visible: courseSchema.draws
        radius: 10
        color: "#1b1533"
        border.color: "#3d3268"
        border.width: 1
        clip: true

        Canvas {
            // LE CERCLE DES QUINTES. Roger : « la premiere page est tres bien, MAIS a aucun moment on montre un cercle
            // O_o. Il faut absolument montrer un cercle de quinte. C'est visuel. »
            // LES DEUX GAMMES, COTE A COTE. Roger : « pour la gamme c'est trop verbeux, et pas assez visuel, quand on
            // compare le fa et fa# par exemple. On le dit avec des mots et pas une image. »

            id: painter

            // OU EN EST LE DESSIN, de 0 a 1. C'est lui qui fait avancer le point : sans lui on voit une courbe, avec lui
            // on voit un chemin qu'on PARCOURT.
            property real progress: 0

            // LA COURBE DU BOURDON, en un seul endroit : le trace ET le point l'utilisent. Deux formules decrivant le
            // meme chemin finiraient par ne plus decrire le meme chemin.
            function pointAt(t, x0, y0, x1, y1, x2, y2, x3, y3) {
                var u = 1 - t;
                return {
                    "x": u * u * u * x0 + 3 * u * u * t * x1 + 3 * u * t * t * x2 + t * t * t * x3,
                    "y": u * u * u * y0 + 3 * u * u * t * y1 + 3 * u * t * t * y2 + t * t * t * y3
                };
            }

            function melodyAt(t) {
                var tonicY = height - 34;
                var top = 26;
                if (t < 0.5)
                    return pointAt(t * 2, 0, tonicY, width * 0.18, top, width * 0.3, top + 8, width * 0.42, height * 0.46);

                return pointAt((t - 0.5) * 2, width * 0.42, height * 0.46, width * 0.56, top, width * 0.74, top + 24, width, tonicY);
            }

            // LE BOURDON : deux notes tenues, et une melodie qui revient s'y poser. Le dessin du chapitre du centre.
            function drawDrone(ctx) {
                var tonicY = height - 34;
                var fifthY = tonicY - 20;
                // Les deux traits TENUS, d'un bord a l'autre. Ils ne commencent pas et ne finissent pas : c'est
                // exactement ce qu'ils ont a dire.
                ctx.strokeStyle = "#7b68b8";
                ctx.lineWidth = 4;
                ctx.beginPath();
                ctx.moveTo(0, tonicY);
                ctx.lineTo(width, tonicY);
                ctx.stroke();
                ctx.strokeStyle = "#57498c";
                ctx.lineWidth = 2;
                ctx.beginPath();
                ctx.moveTo(0, fifthY);
                ctx.lineTo(width, fifthY);
                ctx.stroke();
                // LA MELODIE.
                ctx.strokeStyle = "#cdeec6";
                ctx.lineWidth = 2.5;
                ctx.beginPath();
                for (var step = 0; step <= 64; ++step) {
                    var point = melodyAt(step / 64);
                    if (step === 0)
                        ctx.moveTo(point.x, point.y);
                    else
                        ctx.lineTo(point.x, point.y);
                }
                ctx.stroke();
                // LES NOMS. Roger, apres avoir vu la premiere version : « j'aime bien le schema, meme si on ne le comprend
                // pas du tout xD ». Il avait raison : un dessin sans un mot est une decoration, pas une explication.
                // Trois mots suffisent, et ils disent exactement ce que le texte dit a cote.
                ctx.font = "11px sans-serif";
                ctx.fillStyle = "#9b8ad4";
                ctx.fillText("le bourdon : do + sol", width / 2, tonicY + 17);
                ctx.fillStyle = "#8fbf85";
                ctx.fillText("la melodie", width * 0.52, 12);
                ctx.fillStyle = "#e6e0f7";
                ctx.fillText("elle revient", width - 46, tonicY - 10);
                // LE POINT : ce qui avance. Il se pose sur le centre, et c'est la fin de la phrase.
                var here = melodyAt(progress);
                ctx.fillStyle = "#ffffff";
                ctx.beginPath();
                ctx.arc(here.x, here.y, 5, 0, 2 * Math.PI);
                ctx.fill();
            }

            // Les douze cases sont les douze TONALITES, dans l'ordre ou le cercle les rencontre - et non les douze
            // hauteurs rangees par demi-ton. Monter d'une case ajoute un diese ; la septieme est fa#, et la huitieme
            // s'ecrit reb : la meme touche du clavier, mais une AUTRE tonalite. C'est tout le sujet de l'annexe.
            function drawCircle(ctx) {
                var cx = width / 2;
                var cy = height / 2 + 2;
                var radius = Math.min(width, height) / 2 - 28;
                var names = ["do", "sol", "ré", "la", "mi", "si", "fa#", "réb", "lab", "mib", "sib", "fa"];
                var marks = ["0", "1#", "2#", "3#", "4#", "5#", "6#", "5b", "4b", "3b", "2b", "1b"];
                // L'ANNEAU.
                ctx.strokeStyle = "#3d3268";
                ctx.lineWidth = 1.5;
                ctx.beginPath();
                ctx.arc(cx, cy, radius, 0, 2 * Math.PI);
                ctx.stroke();
                for (var i = 0; i < 12; ++i) {
                    var angle = -Math.PI / 2 + i * Math.PI / 6;
                    var cosine = Math.cos(angle);
                    var sine = Math.sin(angle);
                    // LE COTE QUI MONTE ET LE COTE QUI DESCEND : a droite les dieses, a gauche les bemols. Un cote du
                    // cercle s'eclaire, l'autre s'assombrit - et ca se voit avant de se lire.
                    ctx.fillStyle = (i <= 6) ? "#7b68b8" : "#4a3f78";
                    ctx.beginPath();
                    ctx.arc(cx + radius * cosine, cy + radius * sine, i === 0 ? 6 : 4, 0, 2 * Math.PI);
                    ctx.fill();
                    // LE NOM DE LA TONALITE, dehors, et son ARMURE, dedans. Les deux ensemble : le cercle des quintes est
                    // aussi le tableau des armoires, et c'est pour ca qu'il sert tous les jours.
                    ctx.font = "bold 13px sans-serif";
                    ctx.fillStyle = (i === 0) ? "#ffffff" : "#cbb8e8";
                    ctx.fillText(names[i], cx + (radius + 16) * cosine, cy + (radius + 16) * sine);
                    ctx.font = "9px sans-serif";
                    ctx.fillStyle = "#8a77ad";
                    ctx.fillText(marks[i], cx + (radius - 16) * cosine, cy + (radius - 16) * sine);
                }
                ctx.font = "10px sans-serif";
                ctx.fillStyle = "#8a77ad";
                ctx.fillText("chaque pas = une quinte", cx, cy + 6);
                // LE POINT QUI EN FAIT LE TOUR. Il part du do et il y revient : c'est la promesse du chapitre, et elle se
                // voit mieux qu'elle ne se lit.
                var around = -Math.PI / 2 + progress * 2 * Math.PI;
                ctx.fillStyle = "#ffffff";
                ctx.beginPath();
                ctx.arc(cx + radius * Math.cos(around), cy + radius * Math.sin(around), 5, 0, 2 * Math.PI);
                ctx.fill();
            }

            // Il a raison, et une ligne de mots ne fera jamais voir une difference. Deux rangees ALIGNEES la font voir :
            // sept cases en haut (les sept quintes, rangees du grave a l'aigu), sept en bas (la gamme), et la seule case
            // qui change saute aux yeux sans qu'on ait a la chercher.
            function drawTwoScales(ctx) {
                var fifths = ["do", "ré", "mi", "fa#", "sol", "la", "si"];
                var scale = ["do", "ré", "mi", "fa", "sol", "la", "si"];
                drawScaleRow(ctx, fifths, height * 0.28, "#d98a4a", "les sept quintes, rangées");
                drawScaleRow(ctx, scale, height * 0.72, "#7fbf7a", "la gamme");
                // LE LIEN, entre les deux cases qui different. C'est exactement la comparaison que le texte ne faisait pas
                // voir - et c'est la charniere de toute la lecon.
                var x = width / 2;
                ctx.strokeStyle = "#6b5ea0";
                ctx.lineWidth = 1.5;
                ctx.setLineDash([3, 3]);
                ctx.beginPath();
                ctx.moveTo(x, height * 0.28 + 19);
                ctx.lineTo(x, height * 0.72 - 19);
                ctx.stroke();
                ctx.setLineDash([]);
            }

            // UNE RANGEE DE SEPT CASES. Les deux rangees sortent d'ici, donc elles ne peuvent pas se dessiner de deux
            // facons differentes - ce qui est deja la moitie de la demonstration.
            function drawScaleRow(ctx, notes, centreY, markedColour, label) {
                var cell = (width - 12) / 7;
                var pillHeight = 30;
                var top = centreY - pillHeight / 2;
                for (var i = 0; i < notes.length; ++i) {
                    var x = 6 + i * cell;
                    // LA CASE QUI CHANGE, et elle seule, est coloree. Quatreieme position dans les deux rangees : c'est
                    // la meme place, et ce n'est pas la meme note.
                    ctx.fillStyle = (i === 3) ? markedColour : "#2b2350";
                    ctx.fillRect(x + 1, top, cell - 2, pillHeight);
                    ctx.font = "bold 13px sans-serif";
                    ctx.fillStyle = "#ffffff";
                    ctx.fillText(notes[i], x + cell / 2, centreY);
                }
                ctx.font = "10px sans-serif";
                ctx.fillStyle = "#8a77ad";
                ctx.fillText(label, width / 2, top - 8);
            }

            anchors.fill: parent
            anchors.margins: 10
            onProgressChanged: requestPaint()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                var ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);
                ctx.textAlign = "center";
                ctx.textBaseline = "middle";
                if (courseSchema.schemaName === "cercle")
                    drawCircle(ctx);
                else if (courseSchema.schemaName === "deux-gammes")
                    drawTwoScales(ctx);
                else if (courseSchema.schemaName === "bourdon")
                    drawDrone(ctx);
            }
        }

        // Taper un dessin, c'est l'ENTENDRE : chaque schema joue ce qu'il montre - la meme regle que la carte ':: jeu'.
        MouseArea {
            anchors.fill: parent
            onClicked: {
                ExerciseController.playTapCue();
                if (courseSchema.schemaName === "cercle")
                    CourseController.playFifthCycle(12);
                else if (courseSchema.schemaName === "deux-gammes")
                    CourseController.playModeScale(1);
                else
                    CourseController.playDrone();
            }
        }

    }

    SequentialAnimation {
        // Elle tourne quand elle se VOIT, et pas avant : un dessin sur une autre page n'a personne a qui montrer son
        // mouvement.
        running: courseSchema.visible && courseSchema.animates
        loops: Animation.Infinite

        NumberAnimation {
            target: painter
            property: "progress"
            from: 0
            to: 1
            // Le tour du cercle prend plus long que la courbe du bourdon : douze pas, ce n'est pas deux traits.
            duration: courseSchema.schemaName === "cercle" ? 5000 : 2600
            easing.type: Easing.InOutSine
        }

        // Il se POSE, et on le laisse se poser : une phrase qui repart aussitot ne s'entend pas finir.
        PauseAnimation {
            duration: 900
        }

    }

}
