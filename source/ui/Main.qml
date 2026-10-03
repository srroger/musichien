// LE MEME REMEDE QUE PARTOUT DANS CE FICHIER, et c'est le MINIMUM qui compte.

// Un Text replie rapporte la largeur de sa plus longue ligne comme largeur IMPLICITE, et un contenu de dialogue
// se dimensionne sur l'implicite - pas sur la largeur qu'on lui donne. Mettre `width` sur le texte ne suffit donc
// PAS : le dialogue s'elargissait encore a 477 pour une vue de 352, et le test de l'ecran le voyait. Preferred ET
// minimum a zero, dans une disposition qui a une largeur a elle : c'est ce qui marche.
ColumnLayout {
    width: mainWindow.width - 120
    spacing: 0

    Text {
        // Le texte DIT ce qui va se passer. Un bouton « quitter » tout court laisse croire qu'on ferme
        // l'application, ce qui n'est pas la question posee.
        Layout.preferredWidth: 0
        Layout.minimumWidth: 0
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: "#e7dff7"
        font.pixelSize: 14
        text: qsTr("Voulez-vous revenir à la page principale ? La partie en cours sera perdue.")
    }

}
