#pragma once

// =====================================================================================================================
// Musichien - MicrophoneController
//
// The view model of the microphone settings screen: it lists the inputs, lets the player pick one, and runs a LIVE
// test so that the pitch can be seen before it is trusted. The actual detector is a port injected from outside - this
// controller never builds an implementation, exactly like the exercise screen never builds a note player.
//
// It does not decide anything musical: it only displays the pitch the detector reports. Judging an interval against
// a target is another view model's job.
// =====================================================================================================================

#include "domain/audio/SungIntervalDetector.h"

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <random>

namespace musichien::domain
{
class NotePlayer;
class PitchDetector;
class PlayerPreferences;
}    // namespace musichien::domain

namespace musichien::ui
{

class MicrophoneController final : public QObject
{
    Q_OBJECT

    Q_PROPERTY( QStringList inputDeviceNames READ inputDeviceNames CONSTANT )
    Q_PROPERTY( int currentDeviceIndex READ currentDeviceIndex NOTIFY currentDeviceIndexChanged )
    Q_PROPERTY( bool isListening READ isListening NOTIFY isListeningChanged )
    Q_PROPERTY( double detectedFrequencyHz READ detectedFrequencyHz NOTIFY detectedFrequencyHzChanged )
    Q_PROPERTY( double detectedPitchRatio READ detectedPitchRatio NOTIFY detectedPitchRatioChanged )
    Q_PROPERTY( double detectedMidi READ detectedMidi NOTIFY detectedMidiChanged )
    Q_PROPERTY( double detectedStaffFraction READ detectedStaffFraction NOTIFY detectedStaffFractionChanged )

    // De combien d'octaves la note REELLE est au-dessus (positif) ou en dessous (negatif) de la place ou la boule se
    // pose. Zero quand la boule dit la verite entiere. L'ecran en fait un signe, et c'est ce qui rend l'accordeur
    // utilisable pour une hauteur absolue : la boule reste sur les cinq lignes, la note, elle, ne ment pas.
    Q_PROPERTY( int detectedOctaveShift READ detectedOctaveShift NOTIFY detectedOctaveShiftChanged )
    Q_PROPERTY( QString detectedNoteLabel READ detectedNoteLabel NOTIFY detectedNoteLabelChanged )

    // How far the voice is from the nearest note, in cents, and how good that is. This is what turns the microphone
    // page into a usable tuner: five cents is green, twenty is the edge of "recognisable but off".
    Q_PROPERTY( double detectedCents READ detectedCents NOTIFY detectedCentsChanged )
    Q_PROPERTY( int detectedTuningState READ detectedTuningState NOTIFY detectedTuningStateChanged )

    // --- La question chantee -----------------------------------------------------------------------------------------
    //
    // Deux niveaux :
    //   * la cible est JOUEe (debutant) : on entend l'intervalle, puis on le chante ;
    //   * la cible est seulement NOMMEE (avance) : "chante une quinte", et l'oreille se debrouille.
    // La detection, elle, est la meme dans les deux cas : deux notes tenues, et l'ecart entre elles.
    Q_PROPERTY( int singingTargetSemitones READ singingTargetSemitones NOTIFY singingTargetChanged )
    Q_PROPERTY( QString singingTargetLabel READ singingTargetLabel NOTIFY singingTargetChanged )

    // OU TOMBE LA NOTE A CHANTER SUR LA PORTEE, pour y poser la boule fantome.
    //
    // LA VALEUR ENREGISTREE, ET RIEN D'AUTRE. Roger a fini de mettre le doigt dessus : « il faut vraiment se baser sur la
    // note jouee la premiere fois, afficher le fantome a partir de la valeur enregistree et c'est tout. Et penser a bien
    // rafraichir derriere. »
    //
    // Deux erreurs successives m'ont mene ici, et elles disent tout ce qu'il faut retenir :
    //   * la premiere version lisait bien la note enregistree, mais ecoutait singingTargetChanged - un signal qui ne part
    //     qu'une fois par question. La position se calculait donc AVANT la premiere note et restait figee ;
    //   * la seconde suivait la hauteur entendue a l'instant, ce que Roger a immediatement ressenti : « le fantome suit
    //     tout le temps la voix. Trop de refresh. »
    //
    // Elle ecoute donc sungIntervalChanged - l'ecran se rafraichit quand le detecteur parle, et la valeur lue, elle, est
    // celle de la PREMIERE note de l'essai en cours. Le detecteur remet son reading a zero a chaque reponse (voir
    // ExerciseSessionController::answerSung), donc la fantome repart de la bonne note a chaque tentative.
    Q_PROPERTY( double singingTargetStaffFraction READ singingTargetStaffFraction NOTIFY sungIntervalChanged )
    Q_PROPERTY( bool isSingingCaptureActive READ isSingingCaptureActive NOTIFY singingCaptureStateChanged )
    Q_PROPERTY( bool hasSungInterval READ hasSungInterval NOTIFY sungIntervalChanged )
    Q_PROPERTY( int sungSemitones READ sungSemitones NOTIFY sungIntervalChanged )

    // 0 tant que rien n'a ete chante, 1 quand l'intervalle est juste, 2 quand il ne l'est pas.
    Q_PROPERTY( int sungVerdict READ sungVerdict NOTIFY sungIntervalChanged )

    // LE MIROIR, PAR OPPOSITION A LA SESSION : l'intervalle est DONNE et ne change pas, il n'y a ni compteur de
    // questions ni tirage au hasard. C'est ce qu'un cours demande quand il ecrit « :: chante ».
    Q_PROPERTY( bool isSingingMirror READ isSingingMirror NOTIFY singingMirrorChanged )

    // L'ecart, en CENTS, entre l'intervalle chante et l'intervalle PARFAIT du temperament courant : zero quand il est
    // exactement celui du jeu, positif quand il a ete chante trop large, negatif quand il a ete chante trop etroit.
    //
    // Un verdict dit « juste » ou « rate » ; ce chiffre, lui, dit DE COMBIEN - et c'est ce qui permet de progresser.
    // Le calcul part du temperament choisi, pas d'un tempere egal suppose : l'ecart affiche est celui du jeu.
    Q_PROPERTY( int sungCentsOffset READ sungCentsOffset NOTIFY sungIntervalChanged )

    // Le feedback de tenue : la premiere note a-t-elle ete validee, et ou en est la barre de stabilite (0..1). C'est
    // ce qui dit au chanteur si sa note TIENT ou si elle glisse.
    Q_PROPERTY( bool hasFirstNote READ hasFirstNote NOTIFY sungIntervalChanged )
    Q_PROPERTY( double sungStability READ sungStability NOTIFY sungIntervalChanged )

    // La session : une petite serie de questions chantees, avec un score. La fin de session est affichee quand
    // singingSessionOver devient vrai.
    Q_PROPERTY( int singingQuestionIndex READ singingQuestionIndex NOTIFY singingQuestionChanged )
    Q_PROPERTY( int singingCorrectCount READ singingCorrectCount NOTIFY singingQuestionChanged )
    Q_PROPERTY( int singingTotalQuestions READ singingTotalQuestions CONSTANT )
    Q_PROPERTY( bool singingSessionOver READ singingSessionOver NOTIFY singingQuestionChanged )

public:
    // Builds a detector for the input at p_deviceIndex. The index matches inputDeviceNames, and the factory is the
    // application's way of handing over a QAudioPitchDetector without this view model ever seeing Qt Multimedia.
    using DetectorFactory = std::function<std::unique_ptr<musichien::domain::PitchDetector>( int p_deviceIndex )>;

    MicrophoneController( QStringList p_deviceNames,
                          DetectorFactory p_factory,
                          musichien::domain::PlayerPreferences * p_preferences = nullptr,
                          musichien::domain::NotePlayer * p_notePlayer = nullptr,
                          QObject * p_parent = nullptr );

    // Out-of-line, because the detector is only forward-declared here: the unique_ptr cannot destroy it in the
    // header where the type is still incomplete.
    ~MicrophoneController() override;

    MicrophoneController( const MicrophoneController & ) = delete;
    MicrophoneController & operator=( const MicrophoneController & ) = delete;
    MicrophoneController( MicrophoneController && ) = delete;
    MicrophoneController & operator=( MicrophoneController && ) = delete;

    [[nodiscard]] QStringList inputDeviceNames() const { return m_deviceNames; }
    [[nodiscard]] int currentDeviceIndex() const { return m_currentDeviceIndex; }
    [[nodiscard]] bool isListening() const { return m_isListening; }
    [[nodiscard]] double detectedFrequencyHz() const { return m_detectedFrequencyHz; }
    [[nodiscard]] double detectedPitchRatio() const { return m_detectedPitchRatio; }
    [[nodiscard]] double detectedMidi() const { return m_detectedMidi; }
    [[nodiscard]] double detectedStaffFraction() const { return m_detectedStaffFraction; }
    [[nodiscard]] int detectedOctaveShift() const { return m_detectedOctaveShift; }
    [[nodiscard]] QString detectedNoteLabel() const { return m_detectedNoteLabel; }
    [[nodiscard]] double detectedCents() const { return m_detectedCents; }
    [[nodiscard]] int detectedTuningState() const { return m_detectedTuningState; }

    Q_INVOKABLE void selectDevice( int p_deviceIndex );
    Q_INVOKABLE void startTest();
    Q_INVOKABLE void stopTest();

    // Ouvre le micro s'il ne l'est pas deja. Le jeu est bati sur le micro : la page Accordeur n'a donc plus de bouton
    // « tester le micro », elle demande simplement a ecouter en arrivant - et redemander a un micro deja ouvert
    // relancerait pour rien le peripherique, ce qui s'entendrait sous la forme d'un clic.
    Q_INVOKABLE void ensureListening();

    // --- Le cycle de vie de l'application ---------------------------------------------------------------------------

    // L'application part en arriere-plan. Android REPREND le microphone a cet instant : le garder ouvert ne le garde pas
    // vivant, et le flux qui subsiste ecrirait dans un peripherique deja detruit. On ferme donc proprement - mais on
    // RETIENT qu'on voulait ecouter.
    //
    // C'est ce « retenir » qui manquait, et c'est lui qui privait le jeu de son micro : m_isListening restait a VRAI sur
    // un peripherique mort, donc ensureListening() ne faisait plus rien au retour, et il fallait quitter l'ecran puis y
    // revenir pour retrouver l'ecoute - par accident. Roger : « ca empeche de jouer aujourd'hui ».
    void handleApplicationSuspended();

    // L'application revient au premier plan : on rouvre le micro si l'ecran le demandait avant de partir.
    //
    // Le test se fait sur ce qui a ete ferme ICI, et non sur l'etat de la plateforme : sur un ordinateur de bureau rien
    // ne se ferme, et rouvrir le peripherique au moindre regain de focus ferait cliquer l'accordeur pour rien.
    void handleApplicationResumed();

    // --- La question chantee -----------------------------------------------------------------------------------------

    // Draws a new interval to sing. Called when the page opens, and after every answer.
    Q_INVOKABLE void newSingingQuestion();

    // Sets the interval to sing from OUTSIDE: the exercise session knows its own target, and hands it over here so
    // that the capture and the verdict judge the right interval.
    Q_INVOKABLE void setSingingTarget( int p_semitones );

    // Starts a fresh session: the score returns to zero, and the first question is drawn.
    Q_INVOKABLE void startSingingSession();

    // OUVRIR LE MIROIR SUR UN INTERVALLE DONNE : c'est ce que demande une carte « :: chante » d'un cours.
    //
    // Roger, sur le cours de la quinte juste : « on propose au joueur de chanter la quinte. Autant lui fournir l'outil
    // pour qu'il verifie lui-meme s'il chante juste. » Et le cours le dit lui-meme : « aucun score : c'est un miroir,
    // pas un juge ».
    //
    // Or la carte ouvrait la SESSION : son compteur de questions, et son bouton « Suivant » qui tire un intervalle AU
    // HASARD - donc quitte celui que le cours venait de faire entendre, sous les yeux du joueur. Un miroir ne tire rien :
    // il renvoie ce qu'on lui donne.
    Q_INVOKABLE void openSingingMirror( int p_semitones );

    [[nodiscard]] bool isSingingMirror() const { return m_isSingingMirror; }

    [[nodiscard]] int singingQuestionIndex() const { return m_singingQuestionIndex; }
    [[nodiscard]] int singingCorrectCount() const { return m_singingCorrectCount; }
    [[nodiscard]] int singingTotalQuestions() const { return m_singingTotalQuestions; }
    [[nodiscard]] bool singingSessionOver() const { return m_singingQuestionIndex >= m_singingTotalQuestions; }

    // Plays the interval to sing, for the beginner level. The advanced level simply does not call it.
    Q_INVOKABLE void playSingingTarget();

    // LA PREMIERE NOTE EST CAPTEE : on l'ANNONCE. Un court « ding », puis ON REJOUE LA NOTE qui vient d'etre chantee -
    // c'est l'idee de Roger : « il faudrait carrement faire un bruitage (ou rejouer la frequence qu'il vient de
    // chanter) ». Le chanteur s'ANCRE ainsi avant la seconde note, au lieu de deviner qu'il en reste une - c'etait le
    // vrai probleme : « les gens n'ont pas compris qu'il fallait faire une 2eme note ».
    Q_INVOKABLE void announceFirstNote();

    // Un tic du COMPTE A REBOURS qui precede la seconde note. Accentue pour le dernier, comme une mesure qui commence.
    Q_INVOKABLE void playCountdownTick( bool p_accented );

    // LE « GLING » DE VALIDATION : le son qui dit que la premiere note est enregistree. Roger l'a voulu PLUS IMPORTANT
    // que le metronome du compte a rebours, qui n'est qu'un guide - d'ou le gain reduit de playCountdownTick.
    Q_INVOKABLE void playValidationChime();

    // LA TRANSITION ENTRE LES DEUX NOTES : le jeu JOUE des sons (la note rejouee, le compte a rebours), et le micro les
    // ENTENDRAIT - le detecteur croirait alors a un nouveau chant. Roger : « les sons font interferences avec le micro,
    // du coup le micro croit que c'est un nouveau chant ». On met donc le detecteur en PAUSE le temps des sons : la
    // premiere note est gardee, et rien de ce qui est joue ne compte comme chante.
    Q_INVOKABLE void beginSingingTransition();
    Q_INVOKABLE void endSingingTransition();

    // Opens the microphone and listens for two held notes.
    Q_INVOKABLE void startSingingCapture();
    Q_INVOKABLE void stopSingingCapture();

    [[nodiscard]] int singingTargetSemitones() const { return m_singingTargetSemitones; }
    [[nodiscard]] QString singingTargetLabel() const;

    [[nodiscard]] double singingTargetStaffFraction() const;
    [[nodiscard]] bool isSingingCaptureActive() const { return m_isSingingCaptureActive; }
    [[nodiscard]] bool hasSungInterval() const { return m_sungIntervalDetector.reading().hasInterval(); }
    [[nodiscard]] int sungSemitones() const { return m_sungIntervalDetector.reading().semitones(); }
    [[nodiscard]] int sungVerdict() const;
    [[nodiscard]] int sungCentsOffset() const;
    [[nodiscard]] bool hasFirstNote() const { return m_sungIntervalDetector.reading().firstMidiNumber != 0; }
    [[nodiscard]] double sungStability() const { return m_sungIntervalDetector.stabilityFraction(); }

signals:
    void currentDeviceIndexChanged();
    void isListeningChanged();
    void detectedFrequencyHzChanged();
    void detectedPitchRatioChanged();
    void detectedMidiChanged();
    void detectedStaffFractionChanged();
    void detectedOctaveShiftChanged();
    void detectedNoteLabelChanged();
    void detectedCentsChanged();
    void detectedTuningStateChanged();

    void singingTargetChanged();
    void singingMirrorChanged();
    void singingCaptureStateChanged();
    void sungIntervalChanged();
    void singingQuestionChanged();

private:
    void onPitch( float p_frequencyHz );
    void ensureDetector();

    // La tonique sur laquelle la CIBLE JOUEE est construite : le reglage du joueur, et le do central a defaut. Une seule
    // definition, pour que la note entendue et l'intervalle qu'elle annonce ne puissent pas se desaccorder.
    [[nodiscard]] std::int32_t singingRootMidiNumber() const;

    // Ouvre le peripherique, permission comprise. Extrait de startTest() pour que le RETOUR de l'application emprunte
    // exactement le meme chemin : un chemin de reprise ecrit a part finirait par diverger de celui du premier usage.
    void openDetector();

    // Vrai quand le peripherique a ete ferme par un passage en arriere-plan, et qu'il reste donc a rouvrir au retour.
    // C'est le drapeau qui distingue « le micro a ete rendu » de « le micro n'a jamais ete demande ».
    bool m_reopenAfterSuspend{ false };

    QStringList m_deviceNames;
    DetectorFactory m_factory;
    musichien::domain::PlayerPreferences * m_preferences{ nullptr };
    int m_currentDeviceIndex{ 0 };
    bool m_isListening{ false };
    double m_detectedFrequencyHz{ 0.0 };
    double m_detectedPitchRatio{ 0.0 };
    double m_detectedMidi{ 0.0 };
    double m_detectedStaffFraction{ 0.5 };

    // ⚠️ MESURE TEMPORAIRE : la derniere valeur de fantome ECRITE dans le journal, pour n'y ecrire que quand elle
    // change. Sans ce garde-fou, une valeur relue a chaque image noierait la sortie - et le journal d'Android est deja
    // bavard. Elle part avec la ligne affichee, des que la cause est comprise.
    mutable double m_lastLoggedGhostFraction{ -1.0 };
    int m_detectedOctaveShift{ 0 };
    double m_detectedCents{ 0.0 };
    int m_detectedTuningState{ 0 };
    QString m_detectedNoteLabel;

    // La question chantee : l'intervalle tire, la detection, et l'horloge qui mesure le temps entre deux lectures -
    // le detecteur ne possede pas d'horloge, c'est une regle du jeu.
    musichien::domain::SungIntervalDetector m_sungIntervalDetector;
    std::mt19937 m_singingRandomEngine{ std::random_device{}() };
    int m_singingTargetSemitones{ 7 };
    bool m_isSingingCaptureActive{ false };

    // Vrai pendant la transition entre les deux notes : le detecteur n'est alors PAS alimente, pour que les sons joues
    // par le jeu (la note rejouee, le compte a rebours) ne soient pas pris pour un chant. Voir beginSingingTransition.
    bool m_singingTransition{ false };

    // Vrai quand l'ecran de chant a ete ouvert par une carte de cours : l'intervalle est donne, rien n'est compte, et
    // rien n'est tire au hasard. Faux des l'ouverture d'une session de jeu.
    bool m_isSingingMirror{ false };
    QElapsedTimer m_pitchClock;
    musichien::domain::NotePlayer * m_notePlayer{ nullptr };

    // La session : la question en cours, le nombre de bonnes reponses, et la taille de la serie.
    int m_singingQuestionIndex{ 0 };
    int m_singingCorrectCount{ 0 };
    int m_singingTotalQuestions{ 5 };
    std::unique_ptr<musichien::domain::PitchDetector> m_detector;
};

}    // namespace musichien::ui
