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

    Layout.fillWidth: true
    Layout.preferredHeight: 100

    // The music font, embedded in the resources: the default Android fonts have no treble clef.
    FontLoader {
        id: musicFont

        source: "qrc:/assets/fonts/NotoMusic-Regular.ttf"
    }

    Text {
        x: 2
        y: -2
        text: "\uD834\uDD1E"
        color: "#cbb8e8"
        font.pixelSize: 40
        font.family: musicFont.name
    }

    Repeater {
        model: 5

        delegate: Rectangle {
            required property int index

            x: 0
            y: parent.height * (0.15 + index * 0.175)
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
        y: parent.height * (1 - MicrophoneController.detectedStaffFraction) - height / 2

        // The inertia: the ball does not jump from note to note, it GLIDES into place.
        Behavior on y {
            NumberAnimation {
                duration: 250
                easing.type: Easing.OutQuad
            }

        }

    }

}
