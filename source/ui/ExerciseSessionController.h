#pragma once

// =====================================================================================================================
// Musichien - ExerciseSessionController
//
// The view model of the exercise screen, and the ONLY bridge between the loop and the interface.
//
// Thin by construction, exactly like IntervalPlaybackController:
//   * it never decides what a question is: it asks the session;
//   * it never decides what an answer is worth: it asks the score;
//   * it never names an interval: it asks the domain;
//   * it never invents a sound: it asks the NotePlayer port.
//
// What it DOES own is the two things the domain refuses to do, on purpose:
//
//   * the RANDOMNESS - a session is seeded here, because the domain owns no entropy source of its own,
//     which is what keeps a session reproducible in a test;
//   * the PLAYING of notes, because making a sound is not a rule of the game.
//
// See docs/ARCHITECTURE.md and docs/CODE_CONVENTIONS.md.
// =====================================================================================================================

#include "domain/audio/NotePlayer.h"
#include "domain/audio/SampledInstrument.h"
#include "domain/exercise/AnecdoteBook.h"
#include "domain/exercise/AnswerGrid.h"
#include "domain/exercise/ExerciseSession.h"
#include "domain/exercise/HintBook.h"
#include "domain/exercise/PlayerPreferences.h"
#include "domain/exercise/QuestionLog.h"
#include "domain/exercise/Rank.h"

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <cstdint>
#include <functional>
#include <memory>

namespace musichien::ui
{

class MicrophoneController;

// Called when a wrong answer should be FELT, not only seen.
//
// An empty function means "this device cannot vibrate", which is the honest description of a development
// machine. Injecting it rather than calling a platform API from here is what keeps this view model
// testable, and what keeps the knowledge of Android out of the interface layer.
using VibrationCallback = std::function<void()>;

class ExerciseSessionController final : public QObject
{
    Q_OBJECT

    // True while a session exists: the screen shows it instead of the bench.
    Q_PROPERTY( bool running READ running NOTIFY runningChanged )

    Q_PROPERTY( int questionNumber READ questionNumber NOTIFY sessionChanged )
    Q_PROPERTY( int questionCount READ questionCount NOTIFY sessionChanged )

    // What the player can choose, described for the interface. Rebuilt whenever the question changes,
    // and only then: a grid that moved during the feedback would be unreadable.
    Q_PROPERTY( QVariantList choices READ choices NOTIFY questionChanged )

    // Les memes choix, mais PLACES sur le cercle des quintes : douze cases fixes, dont les vides. C'est ce que
    // l'ecran affiche, et c'est ce qui donne une geographie stable a la grille - une carte, pas une liste.
    Q_PROPERTY( QVariantList gridPositions READ gridPositions NOTIFY questionChanged )

    Q_PROPERTY( bool isAsking READ isAsking NOTIFY sessionChanged )
    Q_PROPERTY( bool isFinished READ isFinished NOTIFY sessionChanged )
    Q_PROPERTY( bool isFeedbackVisible READ isFeedbackVisible NOTIFY sessionChanged )
    Q_PROPERTY( bool wasLastAnswerCorrect READ wasLastAnswerCorrect NOTIFY sessionChanged )
    Q_PROPERTY( bool isHelpAvailable READ isHelpAvailable NOTIFY sessionChanged )

    // Ce que la question en cours demande : 0 pour nommer un intervalle, 1 pour dire dans quel sens il a ete joue.
    // C'est ce que l'ecran lit pour savoir s'il montre le cercle ou les deux boutons monte/descend.
    Q_PROPERTY( int questionKind READ questionKind NOTIFY questionChanged )

    // ---------------------------------------------------------------------------------------------------------------
    // La question de rythme
    //
    // Ce que l'ecran affiche pour une cellule : de quoi la DESSINER (ses frappes), de quoi la SITUER (le tempo, la
    // mesure), et ou en est la boucle. Aucune de ces valeurs n'est inventee ici : elles viennent du domaine, ou de
    // l'horloge qui est le seul bien propre de ce controleeur.
    // ---------------------------------------------------------------------------------------------------------------
    Q_PROPERTY( bool isRhythmQuestion READ isRhythmQuestion NOTIFY questionChanged )

    // Le nom de la cellule - "Binaire", "Valse"... Il vient du domaine, et l'ecran le montre tel quel.
    Q_PROPERTY( QString rhythmPatternName READ rhythmPatternName NOTIFY questionChanged )

    Q_PROPERTY( int rhythmBpm READ rhythmBpm NOTIFY questionChanged )
    Q_PROPERTY( int rhythmBeatsPerBar READ rhythmBeatsPerBar NOTIFY questionChanged )

    // Les frappes de la cellule, pretes a dessiner : 'beat' (en temps, depuis le debut de la boucle), 'accented'
    // (frappee plus fort) et 'drum' (quel element est frappe).
    Q_PROPERTY( QVariantList rhythmHits READ rhythmHits NOTIFY questionChanged )

    // Le temps qui sonne, 0 = le premier. L'ecran le montre plus un, pour compter comme un musicien.
    Q_PROPERTY( int rhythmBeatInBar READ rhythmBeatInBar NOTIFY rhythmStateChanged )

    // Vrai pendant la REPRODUCTION : c'est le moment ou le doigt est juge. Faux pendant l'ecoute, ou une frappe sonne
    // sans rien valoir - celui qui accompagne la cellule pendant qu'elle s'ecoute ne perd pas de vie.
    Q_PROPERTY( bool isRhythmPlaying READ isRhythmPlaying NOTIFY rhythmStateChanged )

    // La qualite de la derniere frappe : 0 = Miss, 1 = Good, 2 = Perfect, -1 = rien depuis le debut de la question.
    // Meme convention que la page Rythme, pour que les deux ecrans parlent la meme langue.
    Q_PROPERTY( int rhythmLastQuality READ rhythmLastQuality NOTIFY rhythmStateChanged )

    // La derniere tentative, en frappes : combien de frappes de la cellule ont ete touchees, sur combien il y en
    // avait. Lu AVANT que le domaine ne rende son verdict - c'est ce qui permet de dire "trois sur quatre".
    Q_PROPERTY( int rhythmCoveredOnsets READ rhythmCoveredOnsets NOTIFY rhythmStateChanged )
    Q_PROPERTY( int rhythmOnsetCount READ rhythmOnsetCount NOTIFY questionChanged )

    // Combien de temps dure une mesure de la cellule, en millisecondes. L'ecran s'en sert pour laisser au feedback le
    // temps de se faire entendre : une pause plus courte que la cellule couperait la cellule elle-meme.
    Q_PROPERTY( int rhythmCellDurationMs READ rhythmCellDurationMs NOTIFY questionChanged )

    // ---------------------------------------------------------------------------------------------------------------
    // La question d'accord
    //
    // L'accord entendu, et ce que le joueur a le droit de repondre. Deux listes, deux questions : une qualite d'accord
    // n'est pas un intervalle, et le cercle des quintes n'a rien a y faire.
    // ---------------------------------------------------------------------------------------------------------------
    Q_PROPERTY( bool isChordQuestion READ isChordQuestion NOTIFY questionChanged )

    // L'accord qui vient d'etre joue, pret a afficher : son nom ("Minor"), son symbole ("Cm") et sa tonique, deja
    // ecrite avec son nom de note - l'ecran n'assemble rien.
    Q_PROPERTY( QVariantMap heardChord READ heardChord NOTIFY sessionChanged )

    // La couleur que le joueur a nommee, quand il y en a une : c'est ce qui permet au verdict de dire ce qui a ete
    // repondu, et pas seulement ce qui a ete entendu.
    Q_PROPERTY( QVariantMap answeredChord READ answeredChord NOTIFY sessionChanged )

    // Les couleurs que le joueur peut repondre, dans l'ordre ou elles ont ete apprises : l'ordre des boutons est donc
    // stable, et une couleur nouvelle s'ajoute a la fin.
    Q_PROPERTY( QVariantList chordChoices READ chordChoices NOTIFY questionChanged )

    // What the domain says about the interval that was asked, and about the one the player chose. The
    // screen reads names and identifiers, it composes neither.
    Q_PROPERTY( QVariantMap heardInterval READ heardInterval NOTIFY sessionChanged )
    Q_PROPERTY( QVariantMap answeredInterval READ answeredInterval NOTIFY sessionChanged )

    // A snatch of music to remember the interval by, once the player has made a mistake.
    //
    // EMPTY means "nothing to show", for any of three reasons that the screen does not need to tell
    // apart: no mistake yet, the answer already known, or a gap in the content file.
    Q_PROPERTY( QString hintText READ hintText NOTIFY sessionChanged )

    // ---------------------------------------------------------------------------------------------------------------
    // The player
    //
    // Asked ONCE, and remembered. Not a setting: the first piece of the profile. See PlayerLevel.
    // ---------------------------------------------------------------------------------------------------------------
    Q_PROPERTY( bool hasChosenLevel READ hasChosenLevel NOTIFY playerLevelChanged )

    Q_PROPERTY( int playerLevel READ playerLevel NOTIFY playerLevelChanged )

    // The levels to offer, each ready to display: an index and a name. Built here rather than written in the
    // QML, for the same reason the answer grid is: a list that exists twice drifts.
    //
    // Static because it reads nothing of this object: the list of levels is a fact of the domain, and saying
    // so in the signature is cheaper than a comment. Qt hands it to the screen all the same.
    Q_PROPERTY( QVariantList playerLevels READ playerLevels CONSTANT )

    // The instruments the player wants to hear, each with its index, its name and whether it is enabled.
    //
    // See PlayerPreferences: a saxophone at the same level as a piano is aggressive, and a timbre that grates
    // gets an application closed. What the player turns off stays off.
    Q_PROPERTY( QVariantList instruments READ instruments NOTIFY instrumentsChanged )

    Q_PROPERTY( int experience READ experience NOTIFY scoreChanged )
    Q_PROPERTY( int streak READ streak NOTIFY scoreChanged )

    // The rank of the streak, from D up to SSS - a game feel of its own, displayed and never decided by the screen.
    Q_PROPERTY( int rank READ rank NOTIFY scoreChanged )
    Q_PROPERTY( QString rankLabel READ rankLabel NOTIFY scoreChanged )

    // A loading-screen anecdote, refreshed on demand. Empty when the content file has nothing to say.
    Q_PROPERTY( QString anecdoteText READ anecdoteText NOTIFY anecdoteChanged )
    Q_PROPERTY( int lives READ lives NOTIFY scoreChanged )
    Q_PROPERTY( bool hasUnlimitedLives READ hasUnlimitedLives NOTIFY scoreChanged )

    // The profile: a name and an experience total, remembered between launches. Read from the store on demand - a
    // profile is asked once, not read from disk on every frame.
    Q_PROPERTY( QString playerName READ playerName WRITE setPlayerName NOTIFY playerNameChanged )
    Q_PROPERTY( int totalExperience READ totalExperience NOTIFY totalExperienceChanged )
    Q_PROPERTY( int sessionCount READ sessionCount NOTIFY sessionChanged )
    Q_PROPERTY( int starCount READ starCount NOTIFY sessionChanged )

    // The daily reminder: a setting the player turns on, so that the application nudges him back. The scheduling
    // itself is not this object's job - the application wires the signal to the platform scheduler.
    Q_PROPERTY( bool dailyReminderEnabled READ dailyReminderEnabled WRITE setDailyReminderEnabled NOTIFY dailyReminderChanged )

    // L'heure du rappel : une constante pour l'instant, lue par le compte a rebours de l'ecran.
    Q_PROPERTY( int reminderHour READ reminderHour CONSTANT )
    Q_PROPERTY( int reminderMinute READ reminderMinute CONSTANT )

    // L'accordage. Le tempere egal d'abord, et les anciens pour le plaisir d'entendre ce que "juste" veut dire.
    // La liste des noms vient du domaine, donc elle ne peut pas deriver de l'enumeration.
    Q_PROPERTY( int temperament READ temperament NOTIFY temperamentChanged )
    Q_PROPERTY( QVariantList temperaments READ temperaments CONSTANT )
    Q_PROPERTY( int tuningRoot READ tuningRoot NOTIFY tuningRootChanged )
    Q_PROPERTY( QVariantList tuningRoots READ tuningRoots CONSTANT )
    Q_PROPERTY( double referencePitch READ referencePitch NOTIFY referencePitchChanged )

    // How many questions in a hundred ask the player to SING, the rest asking him to name the interval. A rule the
    // player can tune: from zero (no singing) to one hundred (nothing but singing).
    Q_PROPERTY( int singQuestionShare READ singQuestionShare NOTIFY singQuestionShareChanged )

    // Combien de questions sur cent portent sur le RYTHME, et combien sur les ACCORDS. Memes reglages, memes bornes,
    // memes raisons : au-dela d'une part, on ne choisit plus ce qu'on travaille, on le subit.
    Q_PROPERTY( int rhythmQuestionShare READ rhythmQuestionShare NOTIFY rhythmQuestionShareChanged )
    Q_PROPERTY( int chordQuestionShare READ chordQuestionShare NOTIFY chordQuestionShareChanged )

    // Only meaningful once the session is over.
    Q_PROPERTY( bool starEarned READ starEarned NOTIFY sessionChanged )

public:
    // Ce que la question en cours demande : nommer un intervalle, ou dire dans quel sens il a ete joue. La valeur
    // est celle du domaine, transposee en entier pour le QML.
    [[nodiscard]] int questionKind() const noexcept;

    // ---------------------------------------------------------------------------------------------------------------
    // La question de rythme
    //
    // Tout ce qui suit ne veut rien dire sur une question d'intervalle, et le dit : les valeurs sont vides ou nulles,
    // jamais fausses. Un ecran qui les lit par erreur n'affiche rien plutot qu'un mensonge.
    // ---------------------------------------------------------------------------------------------------------------
    [[nodiscard]] bool isRhythmQuestion() const noexcept;

    [[nodiscard]] QString rhythmPatternName() const;
    [[nodiscard]] int rhythmBpm() const noexcept;
    [[nodiscard]] int rhythmBeatsPerBar() const noexcept;
    [[nodiscard]] QVariantList rhythmHits() const;

    [[nodiscard]] int rhythmBeatInBar() const noexcept;
    [[nodiscard]] bool isRhythmPlaying() const noexcept;
    [[nodiscard]] int rhythmLastQuality() const noexcept;
    [[nodiscard]] int rhythmCoveredOnsets() const noexcept;
    [[nodiscard]] int rhythmOnsetCount() const noexcept;
    [[nodiscard]] int rhythmCellDurationMs() const noexcept;

    // ---------------------------------------------------------------------------------------------------------------
    // La question d'accord
    //
    // Tout ce qui suit ne dit rien sur une question d'intervalle, et le dit : des valeurs vides plutot que des valeurs
    // fausses. Un ecran qui les lirait par erreur n'afficherait rien.
    // ---------------------------------------------------------------------------------------------------------------
    [[nodiscard]] bool isChordQuestion() const noexcept;

    [[nodiscard]] QVariantMap heardChord() const;
    [[nodiscard]] QVariantMap answeredChord() const;
    [[nodiscard]] QVariantList chordChoices() const;

    // The name the player gave himself, empty before the first time he writes one.
    [[nodiscard]] QString playerName() const;

    Q_INVOKABLE void setPlayerName( const QString & p_name );

    // Experience earned across every session, remembered between launches.
    [[nodiscard]] int totalExperience() const;

    // How many sessions were played to the end, and how many earned their star.
    [[nodiscard]] int sessionCount() const;

    [[nodiscard]] int starCount() const;

    // Whether the daily reminder is on, remembered between launches.
    [[nodiscard]] bool dailyReminderEnabled() const;

    Q_INVOKABLE void setDailyReminderEnabled( bool p_enabled );

    [[nodiscard]] int reminderHour() const noexcept;

    [[nodiscard]] int reminderMinute() const noexcept;

    // The developer button that fires a reminder right now, to check the plumbing.
    Q_INVOKABLE void testReminder();

    // The tuning, as the index of the domain's enumeration, and the names a screen can show.
    [[nodiscard]] int temperament() const;

    Q_INVOKABLE void setTemperament( int p_index );

    [[nodiscard]] QVariantList temperaments() const;

    // The root note the tuning is heard from, as its pitch-class index (0 = C), and the twelve names a screen can
    // show. Equal temperament ignores it; the others do not.
    [[nodiscard]] int tuningRoot() const;

    Q_INVOKABLE void setTuningRoot( int p_index );

    [[nodiscard]] QVariantList tuningRoots() const;

    // The A4 diapason, in hertz. 440 by default; some wind instruments want 442 or 444.
    [[nodiscard]] double referencePitch() const;

    Q_INVOKABLE void setReferencePitch( double p_hertz );

    // How many questions in a hundred ask the player to sing, remembered between launches.
    [[nodiscard]] int singQuestionShare() const;

    Q_INVOKABLE void setSingQuestionShare( int p_share );

    // Combien de questions sur cent portent sur le rythme, et combien sur les accords. Meme contrat que le chant :
    // borne a 0-100, memorise, et pris en compte par la session SUIVANTE.
    [[nodiscard]] int rhythmQuestionShare() const;

    Q_INVOKABLE void setRhythmQuestionShare( int p_share );

    [[nodiscard]] int chordQuestionShare() const;

    Q_INVOKABLE void setChordQuestionShare( int p_share );

    // The player pressed "Écouter" on a sung question: play the target, and count it as a replay when the player is
    // not a beginner - hearing the answer first is the easy way, and it costs experience.
    Q_INVOKABLE void listenToTarget();

    // Wipes the experience and the statistics of the profile: points, sessions and stars back to zero. The name and
    // the level stay - they are choices, not a score.
    Q_INVOKABLE void resetProfile();

    // The rank of the current streak, as an index and as its display label.
    [[nodiscard]] int rank() const noexcept;

    [[nodiscard]] QString rankLabel() const;

    // The anecdote currently shown, and a way to draw a new one.
    [[nodiscard]] QString anecdoteText() const;

    Q_INVOKABLE void refreshAnecdote();
    // The settings of the session to come, provided by the caller rather than written here: they are
    // data of the game, they will come from the profile of the player, and a test needs to be able to
    // pin them down - a session whose direction is drawn at random cannot be asserted precisely.
    //
    // The hint book arrives the same way, from the content file the application read at start up, and is
    // held BY VALUE: a reference would be a reference to an object whose lifetime this class does not
    // control, which is the shortest path to a crash nobody can reproduce.
    explicit ExerciseSessionController( domain::NotePlayer & p_notePlayer,
                                        domain::SessionSettings p_settings = {},
                                        domain::HintBook p_hintBook = {},
                                        domain::AnecdoteBook p_anecdoteBook = {},
                                        VibrationCallback p_vibrate = {},
                                        domain::PlayerPreferences * p_levelStore = nullptr,
                                        QObject * p_parent = nullptr );

    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] int questionNumber() const noexcept;
    [[nodiscard]] int questionCount() const noexcept;
    [[nodiscard]] QVariantList choices() const;
    [[nodiscard]] QVariantList gridPositions() const;
    [[nodiscard]] bool isAsking() const noexcept;
    [[nodiscard]] bool isFinished() const noexcept;
    [[nodiscard]] bool isFeedbackVisible() const noexcept;
    [[nodiscard]] bool wasLastAnswerCorrect() const noexcept;
    [[nodiscard]] bool isHelpAvailable() const noexcept;
    [[nodiscard]] QVariantMap heardInterval() const;
    [[nodiscard]] QVariantMap answeredInterval() const;
    [[nodiscard]] QString hintText() const;
    [[nodiscard]] bool hasChosenLevel() const noexcept;
    [[nodiscard]] int playerLevel() const noexcept;
    [[nodiscard]] static QVariantList playerLevels();
    [[nodiscard]] QVariantList instruments() const;

    // The flags as the rest of the application needs them: main.cpp filters the loaded instruments with this,
    // which is what keeps the audio adapter from having to know anything about preferences.
    [[nodiscard]] std::vector<bool> enabledInstruments() const { return m_enabledInstruments; }
    [[nodiscard]] int experience() const noexcept;
    [[nodiscard]] int streak() const noexcept;
    [[nodiscard]] int lives() const noexcept;
    [[nodiscard]] bool hasUnlimitedLives() const noexcept;
    [[nodiscard]] bool starEarned() const noexcept;

    // Starts a new session and plays its first question. Called by the "Jouer" button.
    Q_INVOKABLE void startSession();

    // The player says where he is, once. His answer is remembered, and it decides where his sessions start.
    Q_INVOKABLE void choosePlayerLevel( int p_level );

    // Turns one instrument on or off. The last enabled one cannot be turned off: an instrument list with nothing
    // in it is a game with no sound.
    Q_INVOKABLE void setInstrumentEnabled( int p_index, bool p_isEnabled );

    // Leaves the loop and goes back to the bench. Stops the sound first: a stream left open on a phone
    // is a battery drain.
    Q_INVOKABLE void stopSession();

    // Starts the endless arcade mode: no lives, no end, just one question after another. A mistake costs rhythm,
    // not the game.
    Q_INVOKABLE void startInfiniteSession();

    // Starts the survival mode: endless, but with lives - the game ends when they run out.
    Q_INVOKABLE void startSurvivalSession();

    // The player asks to hear the interval again. Counted by the session, played here.
    Q_INVOKABLE void replay();

    // The player picks an interval, given as a distance in semitones: the screen never sends a name.
    Q_INVOKABLE void answer( int p_semitones );

    // The player says which way the interval went, on a guided question: 0 up, 1 down.
    Q_INVOKABLE void answerDirection( int p_direction );

    // The player sang, on a sung question. Whether it was right comes from the microphone controller, which has
    // listened and compared the sung interval to the target.
    Q_INVOKABLE void answerSung( bool p_isCorrect );

    // Le joueur a tape, sur la question de rythme en cours.
    //
    // C'est le controleeur qui MESURE - la position du doigt dans la cellule se compte depuis le premier temps de la
    // boucle de reproduction - et le domaine qui JUGE. Une frappe pendant l'ECOUTE sonne et ne vaut rien : celui qui
    // accompagne la cellule ne perd pas une vie pour l'avoir suivie.
    Q_INVOKABLE void tapRhythm();

    // Le joueur a nomme la couleur de l'accord, donnee comme l'index du domaine. L'ecran ne compose ni un nom ni un
    // symbole : il renvoie l'index de ce qu'il a affiche.
    Q_INVOKABLE void answerChord( int p_quality );

    // The microphone controller, injected so that a sung question can reach the voice. Null in the tests: a sung
    // question then cannot be answered, but nothing breaks.
    void setMicrophoneController( MicrophoneController * p_microphone );

    // Le journal des questions conclues, injecte par l'application comme le micro : ce controleur dit « enregistre
    // ceci » sans savoir ou cela va. Null quand il n'y a nulle part ou ecrire - un test, ou un appareil ou l'ecriture
    // echoue - et tout continue de fonctionner : des statistiques, pas une regle du jeu.
    void setQuestionLog( domain::QuestionLog * p_questionLog );

    // The player asks for the answer, after the session said it may be revealed.
    Q_INVOKABLE void revealAnswer();

    // Leaves the feedback and asks the next question. Called by the screen, which owns the pause: the
    // domain has no clock, and giving it one would make every rule above untestable.
    Q_INVOKABLE void continueToNextQuestion();

    // Stops every sound. Called when the screen is left.
    Q_INVOKABLE void stopPlayback();

signals:
    void runningChanged();
    void questionChanged();
    void sessionChanged();
    void scoreChanged();

    // A wrong answer has just been given, and the question is still being asked.
    //
    // The screen answers it with its body - a shake today, a vibration tomorrow - and the controller
    // has no opinion about that: it knows WHAT happened, not how it should feel. Note that this is a
    // wrong ATTEMPT, not the end of a question: a player who is told the answer has not made a mistake.
    void wrongAnswerGiven();

    // The player has just said where he is, or the application has just remembered it.
    void playerLevelChanged();

    // The player has just turned an instrument on or off.
    void instrumentsChanged();

    // The player has just written or changed his name.
    void playerNameChanged();

    // The experience total has just grown, after a session ended.
    void totalExperienceChanged();

    // The player has just turned the daily reminder on or off.
    void dailyReminderChanged();

    // The player has just changed the tuning.
    void temperamentChanged();

    // The player has just changed the root the tuning is heard from.
    void tuningRootChanged();

    // The player has just changed the A4 diapason.
    void referencePitchChanged();

    // The player has just changed how often questions ask him to sing.
    void singQuestionShareChanged();

    // Le joueur vient de changer la part du rythme, ou celle des accords.
    void rhythmQuestionShareChanged();
    void chordQuestionShareChanged();

    // The player has just pressed the "test the notification" button.
    void testReminderRequested();

    // La boucle de rythme a avance : un temps de plus, un passage en reproduction, ou une frappe jugee.
    //
    // Un seul signal pour les trois, parce que c'est la MEME question de l'ecran - "ou en est-on ?" - et qu'un ecran
    // qui les separerait devrait les reunir lui-meme pour se redessiner.
    void rhythmStateChanged();

    // A new anecdote was drawn.
    void anecdoteChanged();

private:
    // Rebuilds the list of choices from the question being asked, and only then notifies. Called
    // whenever the question changes AND whenever the grid closes in after a mistake.
    void refreshChoices();

    // What follows a right or a wrong answer, whatever its form: replay the question one way or the other, shake
    // on a mistake, and tell the screen. Both answer() and answerDirection() end here.
    void processAnswer( bool p_isCorrect );

    // Draws a fresh anecdote the FIRST time the session is seen finished, so that a session opens and closes on
    // something to learn. Guarded, because the last question and the last life can both be the end.
    void announceSessionEndIfNeeded();

    // Starts a session with these settings, drawing the seed in the interface layer where entropy belongs.
    void beginSession( domain::SessionSettings p_settings );

    // Les reglages d'une session pour un niveau, en gardant ce qui n'appartient PAS au niveau - les parts de question,
    // qui sont des reglages du joueur. Voir la definition, et le piege que cette methode ferme.
    [[nodiscard]] domain::SessionSettings sessionSettingsForLevel( domain::PlayerLevel p_level ) const;

    // Ce que le PROFIL decide des parts de question : combien de questions de chaque genre sur cent.
    //
    // Une seule fonction pour les trois, parce que c'est un seul reglage repete trois fois. Quand il n'y a pas de
    // profil - un test, ou une application qui n'a nulle part ou ecrire - les reglages passes au constructeur restent
    // en place, et rien n'est ecrase.
    void applyStoredQuestionShares( domain::SessionSettings & p_settings ) const;

    // Adds the session's outcome - its experience, its count, its star - to the profile, once, when it ends.
    void persistSessionOutcome();

    // Plays the interval of the question being asked, from its own root note.
    void playCurrentQuestion();

    // ---------------------------------------------------------------------------------------------------------------
    // La boucle de rythme
    //
    // Une question de rythme se joue en DEUX mesures : une ou la cellule s'ecoute, une ou elle se reproduit. La boucle
    // tourne ensuite toute seule, jusqu'a ce que la question soit juste, passee, ou finie - c'est ce qui evite au
    // joueur d'avoir a relancer quoi que ce soit entre deux tentatives.
    //
    // Elle vit ici plutot que dans le domaine, et pour la meme raison que le reste : le domaine ne mesure pas le
    // temps. C'est aussi pourquoi RhythmController ne s'en occupe pas : la page Rythme est un outil libre, cette
    // boucle-ci appartient a une question.
    // ---------------------------------------------------------------------------------------------------------------

    // Demarre une question de rythme : l'ecoute commence, et la boucle tourne.
    void startRhythmQuestion();

    // La mesure d'ecoute : la cellule se joue, le doigt n'est pas juge.
    void beginRhythmListening();

    // La mesure de reproduction : le clic bat la pulsation, et chaque frappe est jugee.
    void beginRhythmPlaying();

    // La fin d'une mesure de reproduction : le domaine rend son verdict, et la boucle repart.
    void finishRhythmLoop();

    // Arrete la boucle et remet ses compteurs a zero.
    void stopRhythmLoop();

    // Un temps de la boucle : ce qui sonne sur ce temps, puis l'avancement.
    void onRhythmBeat();

    // Les frappes de la cellule qui tombent dans le temps p_beatInBar. Les frappes decalees - les syncopes - partent
    // en differe, parce que c'est ce qu'est une syncope : une frappe ENTRE deux temps.
    void playRhythmHitsForBeat( int p_beatInBar );

    // La cellule en entier, une fois : c'est le feedback, et la confirmation d'une reponse juste.
    void playRhythmModelOnce();

    // Ou en est la cellule, en temps, depuis le premier temps de la reproduction.
    [[nodiscard]] double rhythmPositionInBeats() const noexcept;

    // Combien de frappes de la cellule ont ete touchees, dans la question en cours.
    [[nodiscard]] int coveredOnsetCount() const noexcept;

    // Plays the same two notes TOGETHER, whatever direction the question was asked in.
    void playCurrentQuestionAsChord();

    // Ecrit une ligne pour la question en cours, qui vient d'etre CONCLUE.
    //
    // L'horloge est lue ICI et nulle part ailleurs : le domaine recoit une date, il ne la demande jamais - c'est ce qui
    // garde le domaine pur et le journal testable sans attendre une seconde.
    void recordCurrentQuestion( bool p_wasCorrect, bool p_wasRevealed );

    // Joue un accord : ses notes, plaquee. Le seul chemin par lequel un accord s'entend, que ce soit pour poser la
    // question ou pour la confirmer.
    void playChordNotes( const domain::Chord & p_chord );

    // Whether the player still gets the answer played for him: a beginner hears the interval first, everyone else
    // has to ask for it (and pays for the asking).
    [[nodiscard]] bool isBeginner() const noexcept;

    domain::NotePlayer & m_notePlayer;

    // Kept so that every session this controller starts uses the same rules.
    domain::SessionSettings m_settings;

    // The memory hooks, owned here rather than referenced: see the constructor.
    domain::HintBook m_hintBook;

    // The loading-screen anecdotes, content of the same kind, and the engine that draws them.
    domain::AnecdoteBook m_anecdoteBook;

    std::mt19937 m_anecdoteRandomEngine{ std::random_device{}() };

    QString m_anecdoteText;

    // Empty when the device cannot vibrate.
    VibrationCallback m_vibrate;

    // May be null: a test, or an application that has nowhere to remember anything, must still run.
    domain::PlayerPreferences * m_levelStore{ nullptr };

    // May be null too, et pour la meme raison : un journal absent coute des statistiques, jamais une partie.
    domain::QuestionLog * m_questionLog{ nullptr };

    // Read once from the store, then kept here: the screen asks for it on every question, and a settings file
    // has no business being read that often.
    std::optional<domain::PlayerLevel> m_playerLevel;

    // One flag per instrument, in the order of domain::INSTRUMENT_NAMES. Empty means "everything", which is
    // what a first run has and what the screen must show as all enabled.
    std::vector<bool> m_enabledInstruments;

    // Empty until a session starts: the bench is what the application shows before that.
    std::unique_ptr<domain::ExerciseSession> m_session;

    // ---------------------------------------------------------------------------------------------------------------
    // La boucle de rythme
    // ---------------------------------------------------------------------------------------------------------------

    // Ce que vaut rhythmLastQuality quand rien n'a encore ete frappe. Distinct de 0, qui est un Miss : "je n'ai pas
    // encore tape" et "j'ai tape a cote" sont deux choses differentes, et l'ecran les montre differemment.
    static constexpr int NO_RHYTHM_TAP = -1;

    QTimer m_rhythmTimer;
    QElapsedTimer m_rhythmClock;

    // Le temps qui sonne, 0 = le premier de la mesure.
    int m_rhythmBeatInBar{ 0 };

    // Vrai pendant la reproduction, faux pendant l'ecoute. C'est la seule chose que ce booleen decide, et c'est
    // beaucoup : ce qui sonne, et ce qui est juge.
    bool m_rhythmIsPlaying{ false };

    // La qualite de la derniere frappe, dans la convention de l'ecran (0 = Miss, 1 = Good, 2 = Perfect). -1 tant que
    // rien n'a ete frappe sur cette question, ce qui n'est pas la meme chose qu'un Miss.
    int m_rhythmLastQuality{ NO_RHYTHM_TAP };

    // Le detail de la derniere tentative. Il est lu AVANT que le domaine ne rende son verdict, parce que le domaine
    // remet son ardoise a zero en jugeant - et l'ecran, lui, doit pouvoir dire "trois frappes sur quatre".
    int m_rhythmCoveredOnsets{ 0 };

    // The microphone, for the sung questions. Null until the application wires it in; the tests leave it null.
    MicrophoneController * m_microphone{ nullptr };

    // False until the end of the running session has been announced once. Reset when a session begins.
    bool m_sessionEndAnnounced{ false };

    QVariantList m_choices;
};

}    // namespace musichien::ui
