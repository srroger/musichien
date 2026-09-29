// =====================================================================================================================
// Musichien - StaffBall
// The miniature staff and its ball: a treble clef, five lines, and the ball that follows the sung pitch. A COMPONENT
// because it is shown in three places - the tuner, the singing bench and the sung question of an exercise - and one
// single style is what keeps them from drifting apart.
// The colour is decided here, but the THRESHOLDS live in the MicrophoneController (a musical judgement, testable).
// =====================================================================================================================

import Musichien
import QtQuick
import QtQuick.Layouts

Item {
    id: staffBall

    // Le signe de l'octave : un + ou un - a cote de la boule, quand la note REELLE n'est pas dans l'octave que la
    // boule dessine. Faux par defaut, et c'est delibere : sur une portee de jeu, ce signe n'apprendrait rien au
    // joueur et lui ferait croire qu'il chante faux d'une octave. Il est reserve a l'accordeur.
    property bool showOctaveShift: false

    // Green when in tune, yellow when close, red beyond - the same colours the tuner page uses.
    function ballColor() {
        if (!MicrophoneController.isListening || MicrophoneController.detectedFrequencyHz <= 0)
            return "#8a77ad";

        var state = MicrophoneController.detectedTuningState;
        if (state === 0)
            return "#7ee8a2";

        if (state === 1)
            return "#f2d982";

        return "#f2848e";
    }

    // La geometrie de la portee, et rien d'autre. La cle de sol est PLUS HAUTE que la portee - sept interlignes
    // contre quatre - donc c'est elle qui donne sa hauteur au composant : la portee est posee dans la partie basse,
    // et l'espace au-dessus lui appartient. Un composant qui n'aurait reserve que la portee aurait vu sa cle deborder
    // sur ce qui l'entoure.
    readonly property real lineSpacing: height * 0.135

    // La ligne INFERIEURE, celle du mi : le bas de la portee.
    readonly property real bottomLineY: height * 0.76

    // La portee entiere : quatre interlignes, de la ligne du bas a celle du haut.
    readonly property real staffSpan: staffBall.lineSpacing * 4

    // Ou tombe la boule, en pixels, pour une note de position 0 (ligne du bas) a 1 (ligne du haut).
    function ballY(p_position) {
        return staffBall.bottomLineY - (p_position * staffBall.staffSpan);
    }

    // Le signe qui dit de quel cote la vraie note se trouve, et rien quand la boule dit la verite entiere.
    readonly property int octaveShift:
        (staffBall.showOctaveShift && MicrophoneController.detectedFrequencyHz > 0) ? MicrophoneController.detectedOctaveShift : 0

    Layout.fillWidth: true
    Layout.preferredHeight: 100

    // The music font, embedded in the resources: the default Android fonts have no treble clef.
    FontLoader {
        id: musicFont

        source: "qrc:/assets/fonts/NotoMusic-Regular.ttf"
    }

    // La cle de sol. Sa TAILLE suit la portee : dans une police de musique, un cadratin vaut quatre interlignes
    // (la convention SMuFL), donc la cle fait sept interlignes de haut et la portee quatre. Un pixelSize fige donnait
    // une cle trop petite des que la hauteur du composant changeait.
    Text {
        id: clef

        color: "#cbb8e8"
        font.family: musicFont.name
        font.pixelSize: staffBall.lineSpacing * 4
        text: "\uD834\uDD1E"
        x: 2

        // La ligne de base du glyphe se pose sur la ligne INFERIEURE de la portee - c'est la convention, et c'est
        // elle qui met la boucle de la cle autour du sol.
        y: staffBall.bottomLineY - baselineOffset
    }

    Repeater {
        model: 5

        delegate: Rectangle {
            required property int index

            x: 0

            // L'index zero est la ligne du BAS, comme sur une portee : on monte d'un interligne a chaque fois.
            y: staffBall.bottomLineY - (index * staffBall.lineSpacing)
            width: parent.width
            height: 1
            color: "#5c4a80"
        }

    }

    Rectangle {
        width: 18
        height: 18
        radius: 9
        visible: MicrophoneController.detectedFrequencyHz > 0
        color: ballColor()
        x: parent.width / 2 - width / 2
        y: staffBall.ballY(MicrophoneController.detectedStaffFraction) - height / 2

        // The inertia: the ball does not jump from note to note, it GLIDES into place.
        Behavior on y {
            NumberAnimation {
                duration: 250
                easing.type: Easing.OutQuad
            }

        }

    }

    // Le signe, a cote de la boule et de la meme couleur : « + » quand la note chantee est plus HAUTE que la place
    // ou la boule se pose, « - » quand elle est plus basse. Deux octaves d'ecart font le meme signe : ce qui compte
    // est le cote, parce que l'octave exacte est ecrite en toutes lettres a cote.
    Text {
        visible: staffBall.octaveShift !== 0
        color: ballColor()
        font.bold: true
        font.pixelSize: 20
        text: staffBall.octaveShift > 0 ? "+" : "\u2212"
        x: (parent.width / 2) + 14
        y: staffBall.ballY(MicrophoneController.detectedStaffFraction) - height / 2
    }

}
