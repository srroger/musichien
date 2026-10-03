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
#include "domain/exercise/GameMode.h"
#include "domain/exercise/HintBook.h"
#include "domain/exercise/PlayerPreferences.h"
#include "domain/exercise/QuestionLog.h"
#include "domain/exercise/QuestionStatistics.h"
#include "domain/exercise/Rank.h"
#include "domain/exercise/Trophy.h"
#include "domain/music/TunerGuide.h"

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

    // L'ecart, en cents, du chant qui vient d'etre juge - FIGE au moment de la reponse.
    //
    // Il ne peut pas etre relu sur le micro pendant que le verdict s'affiche : repondre resynchronise la cible du
    // micro (voir refreshChoices), ce qui remet le detecteur a zero. La mesure doit donc etre mise de cote AVANT,
    // exactement comme « la derniere reponse etait juste » l'est deja.
    Q_PROPERTY( int lastSungCentsOffset READ lastSungCentsOffset NOTIFY sessionChanged )
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

    // OU EN EST LE SON, dans la mesure, en TEMPS (et fractionnaire) : 0.0 au premier temps, 1.5 a la moitie du deuxieme.
    //
    // C'est ce que lit le CURSEUR de la mesure, et c'est pour lui que cette propriete existe : un entier qui saute d'un
    // temps a l'autre ne peut pas etre suivi du regard. Voir rhythmPositionInBeats pour la source de la mesure.
    Q_PROPERTY( double rhythmPositionInBar READ rhythmPositionInBar NOTIFY rhythmStateChanged )

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

    // -----------------------------------------------------------------------------------------------------------------
    // Le pilier harmonie : le degrade
    //
    // Une question de mode se joue comme aucune autre : DEUX modes sur le MEME bourdon, l'un apres l'autre. L'ecran a
    // donc besoin de savoir laquelle des deux questions il pose, et quels modes il peut proposer.
    // -----------------------------------------------------------------------------------------------------------------

    // Le mode a comparer ou a nommer, et celui qui vient d'etre entendu avant lui.
    Q_PROPERTY( bool isModeQuestion READ isModeQuestion NOTIFY questionChanged )

    // Une question d'HARMONIE : celles de mode, et la note etrangere - qui pose la meme oreille devant le meme bourdon.
    //
    // Elle existe parce que l'ecran avait recopie « mode » la ou il fallait « harmonie » : le bloc qui porte les sept
    // boutons de l'intrus etait cache sur une question de note etrangere, et Roger s'est retrouve devant un ecran vide -
    // « j'entends bien une phrase, mais rien ensuite, aucun bouton, on est bloque ». Un predicat nomme, teste, ne se
    // recopie pas.
    Q_PROPERTY( bool isHarmonyQuestion READ isHarmonyQuestion NOTIFY questionChanged )

    // Vrai sur la question de COULEUR : deux modes, et une reponse « plus clair / plus sombre ».
    Q_PROPERTY( bool isModeColourQuestion READ isModeColourQuestion NOTIFY questionChanged )
    Q_PROPERTY( bool isModeVampQuestion READ isModeVampQuestion NOTIFY questionChanged )

    // Les modes que le joueur peut repondre : SA palette, dans l'ordre d'apprentissage, du plus clair au plus sombre.
    // Chaque entree porte un index, un identifiant, un nom, la note qui colore, et une CLARTE entre 0 et 1.
    Q_PROPERTY( QVariantList modeChoices READ modeChoices NOTIFY questionChanged )

    // Le mode qui vient d'etre joue, decrit comme une entree de la liste ci-dessus - et vide tant que rien n'a sonne.
    Q_PROPERTY( QVariantMap heardMode READ heardMode NOTIFY sessionChanged )

    // Le mode entendu JUSTE AVANT, sur une question de couleur, et vide sur une question de nom.
    Q_PROPERTY( QVariantMap previousMode READ previousMode NOTIFY questionChanged )

    // La DIFFERENCE entre les deux modes d'une question de comparaison : le degre qui a bouge, et son accidental.
    //
    // C'est ce qui remplace un verdict qui ne disait rien. « De dorien a ionien, la tierce a monte » se lit, s'entend et
    // s'apprend ; deux noms poses cote a cote ne laissent aucun souvenir - Roger : « on voit ecrit 1 mode, on ne sait
    // pas lequel, le temps de lecture est trop court ».
    Q_PROPERTY( QVariantMap modeDifference READ modeDifference NOTIFY questionChanged )

    // Le cercle des quintes du mode de la question : ses sept notes allumees, le reste eteint, la tonique marquee.
    //
    // DONNE DES LA QUESTION, et c'est un choix de Roger, assume : « je mettrais quand meme la roue dans la question,
    // l'utilisateur pourra ne pas trop la regarder ». La fenetre de sept notes plus la tonique donne le mode, donc c'est
    // une aide forte - mais elle est visible et ignorable, comme l'indice des intervalles, et un joueur qui apprend a
    // situer les modes a besoin de la voir PENDANT qu'il ecoute, pas apres.
    //
    // La tonique reste EN HAUT du cercle : c'est ce qui rend l'arc lisible d'un coup d'oeil, et c'est ce qui ne doit
    // jamais bouger d'un mode a l'autre.
    Q_PROPERTY( QVariantList modeCircle READ modeCircle NOTIFY questionChanged )
    // Ce que la roue montre, et DUQUEL il s'agit : Roger, en jouant - « on ne sait pas a qui correspond le cercle ».
    //
    // Sur une question de couleur, DEUX modes ont sonne, et le cercle est celui du SECOND - c'est le seul des deux que la
    // question nomme. Le nom lui-meme n'arrive qu'avec le verdict : avant, ce serait la reponse de la question de nom.
    Q_PROPERTY( QString modeCircleLabel READ modeCircleLabel NOTIFY questionChanged )

    // LA NOTE ETRANGERE : sept notes montees, une seule etrangere a la gamme, et le joueur dit laquelle.
    Q_PROPERTY( bool isForeignNoteQuestion READ isForeignNoteQuestion NOTIFY questionChanged )

    // Les sept notes de la gamme, DANS L'ORDRE ENTENDU : c'est celui de l'ecoute, donc celui des boutons de reponse.
    Q_PROPERTY( QVariantList foreignNoteChoices READ foreignNoteChoices NOTIFY questionChanged )

    // Le verdict de la note etrangere : quel pas etait l'intrus, quelle note il portait, et quelle note la gamme
    // attendait. Vide tant que la question est posee.
    Q_PROPERTY( QVariantMap foreignNoteVerdict READ foreignNoteVerdict NOTIFY questionChanged )

    // Combien de temps DURE le son d'une question de mode, en millisecondes.
    //
    // L'ecran s'en sert pour ne pas couper la lecture : Roger a vu le defaut en jouant - « pour les modes, ca va beaucoup
    // trop vite, le son se coupe en plein milieu ». C'est le minuteur de pause qui avancait, et il ne pouvait pas savoir
    // qu'un mode dure plus longtemps qu'un intervalle.
    //
    // La valeur vient du DOMAINE, par la meme fonction qui decide combien de temps le bourdon tient : deux calculs qui
    // doivent coincider finissent toujours par diverger.
    Q_PROPERTY( int modeSoundDurationMs READ modeSoundDurationMs NOTIFY questionChanged )

    // LES DEUX NOMBRES DONT LA ROUE A BESOIN pour arriver sur chaque pastille a l'instant ou la note sonne : le silence
    // d'entree, et le temps d'un pas.
    //
    // Une duree TOTALE ne suffit pas : elle comprend le bourdon seul du debut et de la fin, donc une tete calee dessus
    // part trop tot et finit dans le silence. Roger : « la boule des lignes dans les modes est un peu lente par rapport au
    // son ». Voir ModeCircle.startPlayback.
    //
    // Zero quand la question n'a pas de mode : la roue n'est alors pas affichee, et une valeur inventee ici ne servirait a
    // personne.
    //
    // La roue ne s'anime QUE sur la gamme. Une phrase ne l'anime pas - voir playPhraseQuestion - donc ces deux nombres
    // decrivent exactement ce que la roue suit, et rien d'autre.
    Q_PROPERTY( int modeSoundLeadInMs READ modeSoundLeadInMs NOTIFY questionChanged )
    Q_PROPERTY( int modeSoundNoteStepMs READ modeSoundNoteStepMs NOTIFY questionChanged )

    // Les deux parts de l'harmonie, en pour cent : comparer deux modes, et nommer un mode. Deux reglages, parce que ce
    // sont deux competences - un joueur peut vouloir la comparaison sans le vocabulaire, et l'inverse.
    Q_PROPERTY( int modeColourQuestionShare READ modeColourQuestionShare WRITE setModeColourQuestionShare NOTIFY
                  modeQuestionShareChanged )
    Q_PROPERTY( int modeNameQuestionShare READ modeNameQuestionShare WRITE setModeNameQuestionShare NOTIFY
                  modeQuestionShareChanged )
    Q_PROPERTY( int modeVampQuestionShare READ modeVampQuestionShare WRITE setModeVampQuestionShare NOTIFY
                  modeQuestionShareChanged )

    // L'accord qui vient d'etre joue, pret a afficher : son nom ("Minor"), son symbole ("Cm") et sa tonique, deja
    // ecrite avec son nom de note - l'ecran n'assemble rien.
    Q_PROPERTY( QVariantMap heardChord READ heardChord NOTIFY sessionChanged )

    // La couleur que le joueur a nommee, quand il y en a une : c'est ce qui permet au verdict de dire ce qui a ete
    // repondu, et pas seulement ce qui a ete entendu.
    Q_PROPERTY( QVariantMap answeredChord READ answeredChord NOTIFY sessionChanged )

    // Les couleurs que le joueur peut repondre, dans l'ordre ou elles ont ete apprises : l'ordre des boutons est donc
    // stable, et une couleur nouvelle s'ajoute a la fin.
    Q_PROPERTY( QVariantList chordChoices READ chordChoices NOTIFY questionChanged )

    // Le bilan : l'ecran affiche son nom et son mot quand il tourne.
    Q_PROPERTY( bool isReviewRunning READ isReviewRunning NOTIFY sessionChanged )
    Q_PROPERTY( QString encouragementText READ encouragementText NOTIFY sessionChanged )

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
    //
    // ET LE GODMODE EST LA DERNIERE ENTREE, sans etre un niveau du domaine : il ne dit pas ce que le joueur sait, il dit
    // qu'il a decide de choisir. Il porte donc un drapeau de plus, et c'est ce drapeau que l'ecran lit.
    // Pas CONSTANT : l'etat « ouvert / ferme » d'un palier change avec l'experience et le code de developpeur, et une liste
    // figee ne le montrerait jamais. C'est ce defaut qui a fait dire a Roger « j'ai essaye, rien ne s'est passe » : le
    // deverrouillage AVait bien eu lieu, mais le cadenas ne disparaissait pas.
    Q_PROPERTY( QVariantList playerLevels READ playerLevels NOTIFY playerLevelChanged )

    // ---------------------------------------------------------------------------------------------------------------
    // LE GODMODE
    //
    // Ce que Roger a demande : une difficulte ou le joueur choisit lui-meme ce qu'il travaille. Il ne REMPLACE pas les
    // cinq niveaux - ceux-ci gardent leur progression, qui compte dans l'avancement - il les prend comme modeles et
    // casse la conduite accompagnee. L'XP, les statistiques et les bilans continuent : « c'est un dieu, il a tous les
    // pouvoirs ».
    // ---------------------------------------------------------------------------------------------------------------

    // La difficulte choisie est-elle le GodMode ?
    Q_PROPERTY( bool godModeIsChosen READ godModeIsChosen NOTIFY godModeChanged )

    // Une palette a-t-elle deja ete sauvegardee ? C'est ce qui distingue « il n'a jamais ouvert cette page » de « il a
    // tout decoche », et les deux ne se reparent pas de la meme facon.
    Q_PROPERTY( bool godModeIsSaved READ godModeIsSaved NOTIFY godModeChanged )

    // Le brouillon a-t-il ete touche depuis la derniere sauvegarde ? C'est ce qui fait apparaitre « non sauvegarde » a
    // cote du nom, et rien d'autre.
    Q_PROPERTY( bool godModeHasUnsavedChanges READ godModeHasUnsavedChanges NOTIFY godModeChanged )

    // La palette en cours peut-elle poser une partie ? Faux, l'ecran doit dire POURQUOI (voir godModeProblem).
    Q_PROPERTY( bool godModeCanStart READ godModeCanStart NOTIFY godModeChanged )

    // La raison, en une phrase, quand la palette n'est pas jouable. Vide quand tout va bien.
    Q_PROPERTY( QString godModeProblem READ godModeProblem NOTIFY godModeChanged )

    // Les trois familles, chacune prete a afficher : l'index a renvoyer, le nom, et si la case est cochee.
    Q_PROPERTY( QVariantList godModeIntervals READ godModeIntervals NOTIFY godModeChanged )
    Q_PROPERTY( QVariantList godModeChords READ godModeChords NOTIFY godModeChanged )
    Q_PROPERTY( QVariantList godModeModes READ godModeModes NOTIFY godModeChanged )

    // ---------------------------------------------------------------------------------------------------------------
    // LA FELICITATION DE PALIER
    //
    // Le jeu PROPOSE, il n'impose pas : un joueur qui a joue assez pour la suite s'entend offrir le palier suivant.
    // Roger : « apres une partie, si il atteint un certain niveau d'experience, on pourra lui dire : bravo, tu passes au
    // niveau “Jusqu'a l'octave”, regarde derriere toi tu as fait enormement de progres ».
    // ---------------------------------------------------------------------------------------------------------------

    // Le joueur a-t-il merite mieux que son palier actuel ? C'est ce qui fait apparaitre la fleche doree dans la liste des
    // difficultes.
    Q_PROPERTY( bool levelInvitationIsAvailable READ levelInvitationIsAvailable NOTIFY levelInvitationChanged )

    // La felicitation de ce palier a-t-elle deja ete annoncee ? L'ecran de fin de partie ne la montre qu'une fois : une
    // bonne nouvelle repetee devient une machine a sous.
    Q_PROPERTY( bool levelInvitationAnnounced READ levelInvitationAnnounced NOTIFY levelInvitationChanged )

    // Le palier propose, et son nom, tout prets pour la popup.
    Q_PROPERTY( int invitedLevel READ invitedLevel NOTIFY levelInvitationChanged )
    Q_PROPERTY( QString invitedLevelName READ invitedLevelName NOTIFY levelInvitationChanged )

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

    // LE MUSICHIEN QUI S'INVITE : il apparait quand le joueur revient de sa partie, avec une anecdote a raconter.
    //
    // Il vit au-dessus de TOUTES les pages, et il ne s'efface que sur un clic : un texte qu'on n'a pas fini de lire est un
    // texte qu'on n'aurait pas du montrer. Le drapeau est arme par la fin d'une partie, jamais par une page.
    Q_PROPERTY( bool isChibaTalking READ isChibaTalking NOTIFY chibaTalkingChanged )

    // L'humeur du chien qui parle : tiree au hasard parmi les siennes a chaque fois qu'il ouvre la bouche.
    Q_PROPERTY( QString chibaImageSource READ chibaImageSource NOTIFY chibaTalkingChanged )

    // Vrai quand la partie est finie et gagnee : c'est ce qui decide quel chien vient a la fin.
    Q_PROPERTY( bool wasSessionWon READ wasSessionWon NOTIFY sessionChanged )

    // L'anecdote de la QUESTION en cours. Elle change a chaque question, donc on apprend quelque chose en jouant au lieu
    // d'attendre entre deux parties.
    Q_PROPERTY( QString questionAnecdoteText READ questionAnecdoteText NOTIFY questionAnecdoteChanged )
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

    // L'heure du rappel : un reglage du joueur, et il est modifiable.
    Q_PROPERTY( int reminderHour READ reminderHour WRITE setReminderHour NOTIFY dailyReminderChanged )
    Q_PROPERTY( int reminderMinute READ reminderMinute WRITE setReminderMinute NOTIFY dailyReminderChanged )

    // L'accordage. Le tempere egal d'abord, et les anciens pour le plaisir d'entendre ce que "juste" veut dire.
    // La liste des noms vient du domaine, donc elle ne peut pas deriver de l'enumeration.
    Q_PROPERTY( int temperament READ temperament NOTIFY temperamentChanged )
    Q_PROPERTY( QVariantList temperaments READ temperaments CONSTANT )
    Q_PROPERTY( int tuningRoot READ tuningRoot NOTIFY tuningRootChanged )
    Q_PROPERTY( QVariantList tuningRoots READ tuningRoots CONSTANT )
    Q_PROPERTY( double referencePitch READ referencePitch NOTIFY referencePitchChanged )

    // Les EXPLICATIONS de la page Accordeur : ce qu'un temperament est, d'ou il vient, comment il fonctionne, et ce
    // que sont le diapason et la note de reference. Elles viennent d'un fichier de contenu, jamais du code - le QML ne
    // fait que les afficher.
    Q_PROPERTY( QString temperamentExplanation READ temperamentExplanation NOTIFY temperamentChanged )
    Q_PROPERTY( QVariantMap tunerGuide READ tunerGuide CONSTANT )

    // How many questions in a hundred ask the player to SING, the rest asking him to name the interval. A rule the
    // player can tune: from zero (no singing) to one hundred (nothing but singing).
    Q_PROPERTY( int singQuestionShare READ singQuestionShare NOTIFY singQuestionShareChanged )

    // La part de la question historique du jeu : NOMMER l'intervalle entendu.
    //
    // Ce n'est pas un reglage de plus, c'est celui qui manquait : les autres genres avaient chacun leur part, et nommer
    // - la question que le jeu posait a ses debuts - n'en avait aucune. Elle prenait « ce qui restait », et depuis que
    // les parts se lisent les unes par rapport aux autres, une part qui se tait n'est plus une part.
    Q_PROPERTY( int namedIntervalQuestionShare READ namedIntervalQuestionShare NOTIFY namedIntervalQuestionShareChanged )

    // Part des questions qui font chercher la NOTE ETRANGERE d'une gamme. Zero par defaut, comme les deux autres marches
    // de l'harmonie : le pilier se decouvre en l'allumant.
    Q_PROPERTY( int foreignNoteQuestionShare READ foreignNoteQuestionShare NOTIFY foreignNoteQuestionShareChanged )

    // Le TEMPO des phrases de mode, et son amplitude de variation. Roger : « on pourrait choisir de l'augmenter, d'en
    // choisir un central et de varier autour de 20-30 bpm. Histoire de rendre moins monotone. »
    //
    // Le premier est le CENTRE, le second l'amplitude : une phrase est jouee a centre ± tirage. Retenus entre deux
    // seances, comme tous les reglages.
    Q_PROPERTY( int phraseTempoBpm READ phraseTempoBpm NOTIFY phraseTempoChanged )
    Q_PROPERTY( int phraseTempoVariation READ phraseTempoVariation NOTIFY phraseTempoChanged )

    // Combien de questions sur cent portent sur les ACCORDS. Meme reglage, memes bornes, meme raison : au-dela d'une
    // part, on ne choisit plus ce qu'on travaille, on le subit.
    Q_PROPERTY( int chordQuestionShare READ chordQuestionShare NOTIFY chordQuestionShareChanged )

    // Only meaningful once the session is over.
    Q_PROPERTY( bool starEarned READ starEarned NOTIFY sessionChanged )

    // L'ECRAN DE FIN D'ARCADE : ce qu'une partie d'Arcade a de plus que les autres, et ce qu'une autre n'a pas.
    //
    // Le CHRONO et la PLUS LONGUE SERIE sont figes au moment ou la partie se conclut : un chrono qui court pendant que le
    // joueur lit son bilan serait un chiffre qui bouge sous ses yeux.
    Q_PROPERTY( int sessionDurationSeconds READ sessionDurationSeconds NOTIFY sessionChanged )
    Q_PROPERTY( int sessionLongestStreak READ sessionLongestStreak NOTIFY sessionChanged )

    // L'experience REELLEMENT gagnee, multiplicateur applique. Zero dans tout mode qui ne paie pas.
    Q_PROPERTY( int arcadeXpEarned READ arcadeXpEarned NOTIFY sessionChanged )

    // Le multiplicateur en pour cent (100, 120, 200), tel que l'ecran l'affiche.
    Q_PROPERTY( int arcadeMultiplierPercent READ arcadeMultiplierPercent NOTIFY sessionChanged )

    // Le mode de la partie en cours paie-t-il l'experience ? C'est ce qui decide si l'ecran de fin montre le bilan d'Arcade
    // ou une simple ligne.
    Q_PROPERTY( bool sessionGrantsExperience READ sessionGrantsExperience NOTIFY sessionChanged )

    // COMBIEN DE COEURS une Arcade accorde : dix par defaut, jusqu'a vingt-cinq. Le raccourci de Roger.
    Q_PROPERTY( int arcadeLives READ arcadeLives NOTIFY arcadeLivesChanged )

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

    // La position dans la mesure, en temps et fractionnaire : voir la propriete.
    [[nodiscard]] double rhythmPositionInBar() const;

    // OU EN EST LE SON, en millisecondes. Le PUITS AUDIO d'abord, l'horloge de l'interface seulement en repli.
    //
    // C'est la seule facon que le curseur soit d'accord avec ce que le joueur ENTEND : celle de l'interface mesure le
    // moment ou l'on a DEMANDE le son, et entre les deux il y a le tampon du systeme.
    [[nodiscard]] std::int64_t rhythmElapsedMilliseconds() const;
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

    [[nodiscard]] bool isModeQuestion() const noexcept;

    // Vrai pour les questions d'harmonie : un mode a comparer ou a nommer, un vamp, OU une note etrangere a trouver.
    // L'ecran s'en sert pour montrer la zone qui va avec - la roue, le verdict, et les boutons.
    [[nodiscard]] bool isHarmonyQuestion() const noexcept;

    [[nodiscard]] bool isModeColourQuestion() const noexcept;

    // Le vamp : la MEME gamme sur deux centres. Il se repond comme une comparaison de couleurs - la reponse est un sens -
    // mais il n'a PAS de reponse « pareil » : ses deux passages portent deux modes, toujours. L'ecran a donc besoin de le
    // reconnaitre pour ne pas offrir une reponse qu'on ne peut pas gagner.
    [[nodiscard]] bool isModeVampQuestion() const noexcept;

    [[nodiscard]] QVariantList modeChoices() const;

    [[nodiscard]] QVariantMap heardMode() const;

    [[nodiscard]] QVariantMap previousMode() const;

    [[nodiscard]] QVariantMap modeDifference() const;

    [[nodiscard]] QVariantList modeCircle() const;

    [[nodiscard]] QString modeCircleLabel() const;

    [[nodiscard]] int modeSoundDurationMs() const;

    // Le silence d'entree de la gamme, en millisecondes : le bourdon seul, avant la premiere note. Voir les Q_PROPERTY.
    [[nodiscard]] int modeSoundLeadInMs() const;

    // Le temps d'un pas, en millisecondes : une note ET le silence qui la suit. La roue en a besoin separement de la
    // duree totale, parce que c'est lui qui dit quand la note suivante sonne.
    [[nodiscard]] int modeSoundNoteStepMs() const;

    [[nodiscard]] bool isForeignNoteQuestion() const noexcept;

    [[nodiscard]] QVariantList foreignNoteChoices() const;

    [[nodiscard]] QVariantMap foreignNoteVerdict() const;

    [[nodiscard]] int modeColourQuestionShare() const;

    [[nodiscard]] int modeNameQuestionShare() const;

    [[nodiscard]] int modeVampQuestionShare() const;

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

    // L'heure du rappel, choisie par le joueur. La valeur est RAMENEE dans une journee plutot que refusee : un ecran qui
    // se tromperait ne doit pas priver le joueur de son rappel.
    Q_INVOKABLE void setReminderHour( int p_hour );

    Q_INVOKABLE void setReminderMinute( int p_minute );

    // The developer button that fires a reminder right now, to check the plumbing.
    Q_INVOKABLE void testReminder();

    // Demande a Android l'autorisation d'afficher des notifications, si elle manque.
    //
    // Appelee au demarrage de l'ecran d'accueil, et quand le joueur rallume le rappel. Pas avant, et pas a chaque
    // instant : Android ne montre la boite qu'une fois de toute facon, et une demande posee pour rien est une demande
    // qui fait douter de tout le reste.
    Q_INVOKABLE void requestNotificationPermission();

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

    // Le texte du temperament CHOISI : une seule phrase longue, tiree du fichier de contenu, qui dit d'ou il vient et
    // comment il fonctionne. Vide si le fichier n'a pas ete lu, et l'ecran garde alors ses propres mots.
    [[nodiscard]] QString temperamentExplanation() const;

    // Le reste du guide : le diapason, la note de reference, et le mode d'emploi. Une seule propriete plutot que
    // quatre, parce que ces textes ne changent jamais et qu'ils se lisent ensemble.
    [[nodiscard]] QVariantMap tunerGuide() const;

    // Le guide des textes de l'accordeur, injecte par l'application comme le micro et le journal : ce controleur
    // expose des textes, il ne sait pas d'ou ils viennent.
    void setTunerGuide( domain::TunerGuide p_guide );

    // How many questions in a hundred ask the player to sing, remembered between launches.
    [[nodiscard]] int singQuestionShare() const;

    Q_INVOKABLE void setSingQuestionShare( int p_share );

    // La part des questions qui demandent de NOMMER l'intervalle. Meme contrat que les autres : bornee a 0-100,
    // memorisee, et prise en compte par la session SUIVANTE.
    [[nodiscard]] int namedIntervalQuestionShare() const;

    Q_INVOKABLE void setNamedIntervalQuestionShare( int p_share );

    // La part de la note etrangere. Meme contrat que les autres : bornee a 0-100, memorisee, et prise en compte par la
    // session SUIVANTE.
    [[nodiscard]] int foreignNoteQuestionShare() const;

    Q_INVOKABLE void setForeignNoteQuestionShare( int p_share );

    // Le tempo des phrases de mode, et son amplitude. Memes contrats que les autres reglages : bornes verifiees, valeur
    // memorisee, et prise en compte a la prochaine phrase jouee.
    [[nodiscard]] int phraseTempoBpm() const;

    Q_INVOKABLE void setPhraseTempoBpm( int p_bpm );
    Q_INVOKABLE void setPhraseTempoVariation( int p_variation );
    [[nodiscard]] int phraseTempoVariation() const;

    // Combien de questions sur cent portent sur les accords. Meme contrat que le chant : borne a 0-100, memorise, et pris
    // en compte par la session SUIVANTE.
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

    [[nodiscard]] QString questionAnecdoteText() const { return m_questionAnecdoteText; }

    Q_INVOKABLE void refreshAnecdote();

    // Tire une nouvelle anecdote de QUESTION. Privee : c'est le deroulement d'une partie qui la declenche, jamais l'ecran.
    void refreshQuestionAnecdote();
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
    [[nodiscard]] bool isFinished() const noexcept { return ( m_session != nullptr ) && m_session->isFinished(); }

    // LA PARTIE EST-ELLE GAGNEE ?
    //
    // Gagnee veut dire ARRIVEE AU BOUT : c'est le score qui dira comment. Une partie qui finit avec une seule vie est une
    // partie gagnee, et une partie dont les vies ont saute est une partie perdue - les deux se ressemblent quand on ne
    // regarde que les questions posees, et c'est ce qui rend la question utile. Elle choisit le dessin de la fin.
    [[nodiscard]] bool wasSessionWon() const noexcept;

    // Le chien qui s'invite : vrai quand il a quelque chose a raconter. Il ne s'efface que sur un clic du joueur, jamais
    // tout seul - un texte qu'on n'a pas fini de lire est un texte qu'on n'aurait pas du montrer.
    [[nodiscard]] bool isChibaTalking() const noexcept { return m_isChibaTalking; }

    // L'humeur du chien : un chemin de ressource, tire au hasard a chaque fois qu'il ouvre la bouche.
    [[nodiscard]] QString chibaImageSource() const { return m_chibaImageSource; }

    // Remet les REGLAGES au defaut, et seulement eux.
    //
    // Roger l'a demande : « je rajouterais bien un "Reset by default" pour remettre tous les parametres par defaut ».
    // Le score n'est pas un parametre : l'experience, les sessions et les etoiles sont le journal du joueur, et les
    // effacer au passage ferait de ce bouton un piege.
    Q_INVOKABLE void resetPreferences();

    // Le joueur a lu : la popup s'efface, et ne revient pas avant la prochaine fin de partie.
    Q_INVOKABLE void dismissChiba();

    // Le joueur appuie sur le chien : il raconte autre chose.
    Q_INVOKABLE void tellAnotherAnecdote();

    // LE BRUITAGE D'UN GAIN QUI S'AFFICHE, et l'ecran seul sait quand il sonne.
    //
    // Roger : « une animation sur les nombres ... et un bruitage de jeux video gling gling gling ... et un bruitage ou
    // melodie ou accord de victoire ». C'est du GAME FEEL, donc cela appartient a l'ecran : LUI seul sait quand le
    // compte commence, ou il en est, et quand il arrive. Le controleur ne fait que porter la demande jusqu'au son.
    //
    // p_progressPercent dit ou en est le compte, de 0 a 100 - c'est ce qui fait monter le tic avec le chiffre.
    Q_INVOKABLE void playScoreTick( int p_progressPercent );

    Q_INVOKABLE void playVictoryFanfare();

    // Tire une des quatre humeurs du chien. Privee : c'est le deroulement - la fin d'une partie, ou le clic du joueur -
    // qui la declenche, jamais l'ecran.
    void pickChibaImage();

    [[nodiscard]] bool isFeedbackVisible() const noexcept;
    [[nodiscard]] bool wasLastAnswerCorrect() const noexcept;
    [[nodiscard]] int lastSungCentsOffset() const noexcept { return m_lastSungCentsOffset; }
    [[nodiscard]] bool isHelpAvailable() const noexcept;
    [[nodiscard]] QVariantMap heardInterval() const;
    [[nodiscard]] QVariantMap answeredInterval() const;
    [[nodiscard]] QString hintText() const;
    [[nodiscard]] bool hasChosenLevel() const noexcept;
    [[nodiscard]] int playerLevel() const noexcept;
    // Les difficultes proposees, avec leur etat : ouvertes ou FERMEES tant que l'experience ne les a pas meritees. Pas
    // `static` : elle lit l'etat du joueur (son experience, le code de developpeur), donc elle a besoin de l'instance.
    [[nodiscard]] QVariantList playerLevels() const;

    [[nodiscard]] bool godModeIsChosen() const noexcept { return m_godModeIsChosen; }

    [[nodiscard]] bool godModeIsSaved() const noexcept { return m_godModeIsSaved; }

    [[nodiscard]] bool godModeHasUnsavedChanges() const;

    [[nodiscard]] bool godModeCanStart() const;

    // La raison, en une phrase, quand la palette ne peut pas poser de partie. Le message vient de l'ecran (tr) et la
    // regle du domaine : c'est la seule facon de dire « il manque des accords » sans que l'ecran ait a recompter.
    [[nodiscard]] QString godModeProblem() const;

    [[nodiscard]] QVariantList godModeIntervals() const;
    [[nodiscard]] QVariantList godModeChords() const;
    [[nodiscard]] QVariantList godModeModes() const;

    [[nodiscard]] bool levelInvitationIsAvailable() const;
    [[nodiscard]] bool levelInvitationAnnounced() const;
    [[nodiscard]] int invitedLevel() const;
    [[nodiscard]] QString invitedLevelName() const;

    // Accepter la felicitation, c'est choisir ce palier - et par le MEME chemin que si le joueur l'avait pris dans la liste,
    // donc sans second mecanisme a tenir a jour.
    Q_INVOKABLE void acceptLevelInvitation();

    // L'ecran a annonce la felicitation : on la marque, pour ne pas la repeter apres chaque partie.
    Q_INVOKABLE void markLevelInvitationAnnounced();

    // Coche ou decoche un element du brouillon. Rien n'est ecrit sur le disque : c'est le bouton Sauvegarder qui ecrit,
    // et c'est ce qui rend « non sauvegarde » vrai.
    Q_INVOKABLE void toggleGodModeInterval( int p_semitones );
    Q_INVOKABLE void toggleGodModeChord( int p_qualityIndex );
    Q_INVOKABLE void toggleGodModeMode( int p_modeIndex );

    // Tout cocher ou tout decocher une famille : c'est le geste qu'on veut pour « je veux repartir de zero », et le faire
    // case par case serait une punition.
    Q_INVOKABLE void setEveryGodModeIntervalChecked( bool p_checked );
    Q_INVOKABLE void setEveryGodModeChordChecked( bool p_checked );
    Q_INVOKABLE void setEveryGodModeModeChecked( bool p_checked );

    // Pré-remplit le brouillon avec le modele d'un niveau. C'est le geste que Roger a decrit : « je debute, ca coche les
    // deux premiers intervalles », et il sert aussi a repartir d'une base connue.
    Q_INVOKABLE void prefillGodModeFromLevel( int p_level );

    // Ecrit le brouillon, et RIEN d'autre : c'est la seule ecriture, donc la seule chose qui puisse faire passer
    // « non sauvegarde » a « sauvegarde ».
    Q_INVOKABLE void saveGodMode();
    [[nodiscard]] QVariantList instruments() const;

    // The flags as the rest of the application needs them: main.cpp filters the loaded instruments with this,
    // which is what keeps the audio adapter from having to know anything about preferences.
    [[nodiscard]] std::vector<bool> enabledInstruments() const { return m_enabledInstruments; }
    [[nodiscard]] int experience() const noexcept;
    [[nodiscard]] int streak() const noexcept;
    [[nodiscard]] int lives() const noexcept;
    [[nodiscard]] bool hasUnlimitedLives() const noexcept;
    [[nodiscard]] bool starEarned() const noexcept;

    // L'ARCADE : la porte principale depuis que le jeu a quatre modes.
    //
    // Vingt-cinq questions au dosage impose - dix intervalles, huit accords, sept modes - et la note etrangere en final.
    // C'est le SEUL mode qui paie de l'experience, et c'est tout l'interet : un joueur ne peut pas cultiver son niveau en
    // ne travaillant que ce qu'il sait deja faire.
    Q_INVOKABLE void startSession();

    // Une partie PILOTEE PAR LES REGLAGES, sans plan ni famille imposee : le MOTEUR du jeu, pour les tests qui verifient
    // une regle et non une porte.
    //
    // Ce n'est PAS un bouton - les quatre facons de jouer sont au-dessus - et c'est deliberate : les tests de regle ont
    // besoin d'un tirage gouverne par les parts, et les modes de l'ecran en imposent chacun une variante (l'Arcade son
    // plan, l'Entrainement sa famille). Elle paie l'experience comme l'Arcade, ce qui preserve le comportement des tests
    // qui la lisaient.
    void startOrdinarySession();

    // L'ENTRAINEMENT : une famille, dix questions, aucune experience.
    //
    // p_family est un QuestionFamily (0 intervalles, 1 accords, 2 modes). La famille ouverte garde ses sous-parts, et
    // compte dans les statistiques - c'est ce qui nourrit le Bilan - mais ne rapporte rien.
    Q_INVOKABLE void startTrainingSession( int p_family );

    // REJOUER : relance le MEME mode. Un Entrainement rejoue son Entrainement, un Bilan son Bilan.
    //
    // Le bouton « Rejouer » de l'ecran de fin appelait l'Arcade quoi qu'il arrive - ce qui transformait silencieusement un
    // entrainement en Arcade, et gagnait de l'experience alors qu'on venait de comprendre que non. Le mode est retenu ici,
    // donc le bouton ne peut plus se tromper.
    Q_INVOKABLE void restartSession();

    // Le mode de la partie en cours, et s'il paie de l'experience. L'ecran de fin s'en sert pour dire ce qu'il doit dire,
    // et pour ne PAS afficher d'experience gagnee la ou il n'y en a pas.
    [[nodiscard]] int gameMode() const noexcept { return static_cast<int>( m_gameMode ); }
    [[nodiscard]] bool sessionGrantsExperience() const noexcept { return domain::grantsExperience( m_gameMode ); }

    // Le multiplicateur d'experience de l'Arcade, en POUR CENT (100, 120, 200), tel que l'ecran de fin l'affiche.
    //
    // Il se lit sur les coeurs PERDUS de la partie qui vient de finir, et il est fige au moment ou la session se conclut :
    // un ecran qui le recalculerait plus tard lirait un score qui a pu changer.
    [[nodiscard]] int arcadeMultiplierPercent() const noexcept { return m_lastArcadeMultiplierPercent; }

    // Le chrono de la partie, en secondes, FIGE a la fin. Zero tant que la session n'est pas finie.
    [[nodiscard]] int sessionDurationSeconds() const noexcept { return m_sessionDurationSeconds; }

    // La plus longue serie de la partie. La derniere serie retombe a chaque erreur et ne raconte rien ; le plus haut que le
    // compteur soit monte, si.
    [[nodiscard]] int sessionLongestStreak() const noexcept;

    // L'experience gagnee, multiplicateur applique. Voir persistSessionOutcome, le seul endroit qui l'ecrit.
    [[nodiscard]] int arcadeXpEarned() const noexcept { return m_arcadeXpEarned; }

    // Les trois familles, avec ce que la partie a demande et reussi : {name, asked, correct, percent}.
    //
    // C'est ce que l'ecran de fin d'Arcade lit pour dire ou le joueur est fort et ou il resiste. Les familles sans question
    // sont EXCLUES : une ligne « 0 sur 0 » n'informe pas, elle remplit.
    [[nodiscard]] Q_INVOKABLE QVariantList familyResults() const;

    // COMBIEN DE COEURS une Arcade accorde. Le reglage de Roger, et le seul qui rende le boss atteignable quand on
    // n'arrive a rien.
    [[nodiscard]] int arcadeLives() const;
    Q_INVOKABLE void setArcadeLives( int p_lives );

    // LE TITRE du joueur, d'apres ses bilans reussis - et sa devise, en une phrase.
    //
    // Q_PROPERTY, ET NON SEULEMENT UNE METHODE, et c'est une CORRECTION. Roger : « a la victoire d'arcade, je vois
    // constamment que j'ai gagne le nouveau titre : Toutou. »
    //
    // L'ecran ecrivait `ExerciseController.titleJustIncreased` SANS parentheses. Une methode non appelee n'est pas un
    // booleen : c'est un OBJET FONCTION, et un objet fonction est TOUJOURS VRAI. Le badge s'affichait donc a chaque fin
    // de partie, avec le titre du moment - « Toutou » - comme s'il venait d'etre gagne.
    //
    // Et une methode ne suffirait pas, meme appelee : QML ne sait pas QUAND sa reponse change, donc une liaison qui
    // l'appelle reste figee sur ce qu'elle valait a sa creation. Une propriete notifiable dit les deux choses : quelle
    // valeur, et quand elle a change. La methode est gardee a cote, pour que les appels explicites restent possibles.
    Q_PROPERTY( QVariantMap playerTitle READ playerTitle NOTIFY playerProgressChanged )
    [[nodiscard]] Q_INVOKABLE QVariantMap playerTitle() const;

    // LES TROPHEES, avec leur etat : {identifier, name, description, earned}. Gagnes au Bilan, gardes pour de bon.
    Q_PROPERTY( QVariantList trophies READ trophies NOTIFY playerProgressChanged )
    [[nodiscard]] Q_INVOKABLE QVariantList trophies() const;

    // TOUS les titres, du plus modeste au plus haut, avec leur etat. Roger veut les VOIR, meme non acquis : « on peut
    // voir la liste dans la page de profil (mais en grise). Histoire de donner des "objectifs" au joueur. » L'echelle
    // entiere est donc un objectif, et pas seulement le titre du moment.
    Q_PROPERTY( QVariantList allTitles READ allTitles NOTIFY playerProgressChanged )
    [[nodiscard]] Q_INVOKABLE QVariantList allTitles() const;

    // CE QUE LE DERNIER BILAN VIENT DE RAPPORTER : les trophees tout neufs, et si le titre a monte.
    //
    // Roger : « a la fin du bilan, si il a gagne un trophee ou une recompense, il faut lui dire (et lui dire qu'ils sont
    // dans "profil") ». Vides et faux pour tout autre mode, et remis a zero au debut de chaque partie : une annonce qui
    // survivrait a sa partie serait une annonce qui ment.
    Q_PROPERTY( QVariantList newlyEarnedTrophies READ newlyEarnedTrophies NOTIFY playerProgressChanged )
    [[nodiscard]] Q_INVOKABLE QVariantList newlyEarnedTrophies() const { return m_newlyEarnedTrophies; }

    Q_PROPERTY( bool titleJustIncreased READ titleJustIncreased NOTIFY playerProgressChanged )
    [[nodiscard]] Q_INVOKABLE bool titleJustIncreased() const noexcept { return m_titleJustIncreased; }

    // Le plus haut palier de difficulte que l'experience du joueur lui ouvre.
    //
    // Les paliers superieurs se FERMENT tant qu'on ne les a pas merites : c'est ce qui donne au GodMode son sens - un
    // passe-droit pour qui veut tester le jeu sans y etre regulier. Voir unlockAllLevels pour le raccourci de developpeur.
    [[nodiscard]] Q_INVOKABLE bool isLevelUnlocked( int p_index ) const;

    // Le code de developpeur : choisir GodMode SEPT fois d'affilee deverrouille toutes les difficultes.
    //
    // Roger : « pour le dev, on va laisser un cheat code aussi pour pouvoir choisir un niveau de difficulte malgre le
    // manque de point d'experience du style, choisir GodMode 7 fois d'affile, boom ca debloque le choix de toute les
    // difficulte ». Le compteur est remis a zero des qu'un autre niveau est choisi.
    Q_INVOKABLE void noteGodModeSelection();

    [[nodiscard]] bool areAllLevelsUnlocked() const noexcept { return m_allLevelsUnlocked; }

    // The player says where he is, once. His answer is remembered, and it decides where his sessions start.
    // Le LIVRE DES PHRASES : donne par la couche de cablage, comme le diapason.
    //
    // C'est lui qui fait entendre un mode en MELODIE, et l'ecran n'a rien a en savoir : le domaine tire la phrase au
    // moment ou il pose la question, et se rabat sur une gamme quand le contenu n'en porte pas pour ce mode.
    void setPhraseBook( const domain::PhraseBook & p_phraseBook ) { m_phraseBook = &p_phraseBook; }

    Q_INVOKABLE void choosePlayerLevel( int p_level );

    // Le clic de menu, a la disposition de l'ecran.
    //
    // Il existe deja sur les boutons de difficulte, et Roger veut l'entendre PARTOUT ou l'on ne fait que naviguer :
    // « ces petits sons de menu, on devrait les etendre, surtout les boutons qui ne produisent pas de musique ou de
    // bruit ». C'est donc l'ecran qui decide - lui seul sait si un bouton va faire sonner quelque chose.
    Q_INVOKABLE void playTapCue();

    // Turns one instrument on or off. The last enabled one cannot be turned off: an instrument list with nothing
    // in it is a game with no sound.
    Q_INVOKABLE void setInstrumentEnabled( int p_index, bool p_isEnabled );

    // Fait ENTENDRE un instrument, pour que le joueur puisse le choisir a l'oreille.
    //
    // Roger : « pour l'utilisateur, c'est un peu complique de choisir son instrument car c'est complique de l'entendre ».
    // Un nom sur une case ne dit rien de ce qu'on entendra : onze instruments sont offerts, et le seul moyen de choisir
    // est de les ecouter. C'est une gamme phrygienne qui les presente - montee, descendue, puis l'accord du bII - parce
    // que c'est le mode qui trahit le mieux un timbre : sa seconde mineure fait entendre tout de suite un son qui
    // grince ou qui bave, la ou une gamme majeure les laisserait tous paraitre agreables.
    //
    // C'est un APERCU, et rien de plus : ni le timbre de la session en cours, ni les reglages ne bougent.
    Q_INVOKABLE void previewInstrument( int p_index );

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
    Q_INVOKABLE void answerSung( bool p_isCorrect, int p_centsOffset );

    // Le joueur a tape, sur la question de rythme en cours.
    //
    // C'est le controleeur qui MESURE - la position du doigt dans la cellule se compte depuis le premier temps de la
    // boucle de reproduction - et le domaine qui JUGE. Une frappe pendant l'ECOUTE sonne et ne vaut rien : celui qui
    // accompagne la cellule ne perd pas une vie pour l'avoir suivie.
    Q_INVOKABLE void tapRhythm();

    // Le joueur a nomme la couleur de l'accord, donnee comme l'index du domaine. L'ecran ne compose ni un nom ni un
    // symbole : il renvoie l'index de ce qu'il a affiche.
    Q_INVOKABLE void answerChord( int p_quality );

    // Repond a une question de COULEUR : le second mode etait-il plus clair que le premier ?
    //
    // Un booleen, comme le domaine le demande : la reponse n'est pas un ecart, c'est un SENS, et c'est le domaine qui
    // calcule lequel des deux modes est le plus clair.
    Q_INVOKABLE void answerModeColour( bool p_secondIsBrighter );

    // Le bouton « pareil » : la troisieme reponse d'une question de couleur, celle qui dit qu'il n'y a rien a entendre.
    //
    // Demande par Roger, et c'est une vraie question d'oreille : ne pas inventer une difference quand il n'y en a pas.
    Q_INVOKABLE void answerSameColour();

    // La reponse a une question de NOTE ETRANGERE : le pas ou l'intrus a ete entendu, de 0 a 6.
    Q_INVOKABLE void answerForeignNote( int p_stepIndex );

    // Repond a une question de NOM : l'index du mode joue, dans la liste de modeChoices().
    Q_INVOKABLE void answerModeName( int p_modeIndex );

    // Les deux parts de l'harmonie, reglables comme celles du chant, du rythme et des accords.
    Q_INVOKABLE void setModeColourQuestionShare( int p_share );

    Q_INVOKABLE void setModeNameQuestionShare( int p_share );

    Q_INVOKABLE void setModeVampQuestionShare( int p_share );

    // Vrai quand il y a un indice a proposer : une question d'accord, des aides, un essai deja rate, et de quoi retirer
    // une reponse fausse.
    Q_PROPERTY( bool isChordHintAvailable READ isChordHintAvailable NOTIFY questionChanged )

    // L'ARBRE DES ACCORDS, pret a etre dessine : une entree par couleur, dans l'ordre de LECTURE (le parent avant ses
    // enfants), avec son nom anglo-saxon, ses degres, sa profondeur et le geste qui la fait naitre.
    //
    // CONSTANT : l'arbre est de la theorie, il ne change pas d'une partie a l'autre.
    Q_PROPERTY( QVariantList chordTree READ chordTree CONSTANT )

    [[nodiscard]] QVariantList chordTree() const;

    // Le nombre de couleurs d'accord que le jeu connait.
    //
    // L'ecran s'en sert pour repartir ses teintes sur tout le cercle chromatique. Une constante ecrite a la main dans le
    // QML finirait par mentir le jour ou une qualite s'ajoute : c'est exactement le genre de nombre qui doit traverser la
    // frontiere une seule fois, et dans ce sens-la.
    Q_PROPERTY( int chordQualityCount READ chordQualityCount CONSTANT )

    [[nodiscard]] int chordQualityCount() const noexcept;

    // LA COULEUR d'une couleur d'accord, prete a peindre.
    //
    // Elle est calculee ICI plutot que dans un fichier QML, et c'est deliberé : deux ecrans qui recalculeraient chacun
    // leur teinte finiraient par en montrer deux differentes, et un code couleur qui diverge n'apprend plus rien. C'est
    // la meme raison qui fait que les noms viennent du domaine.
    Q_INVOKABLE [[nodiscard]] QString chordColourName( int p_quality ) const;

    [[nodiscard]] bool isChordHintAvailable() const noexcept;

    // Vrai quand l'arpege a un sens. Separe du precedent, parce qu'un DEBUTANT n'a que deux couleurs d'accord : il n'a
    // jamais rien a retirer, et l'arpege reste pourtant l'aide qui lui apprend le plus.
    Q_PROPERTY( bool isChordArpeggioAvailable READ isChordArpeggioAvailable NOTIFY questionChanged )

    [[nodiscard]] bool isChordArpeggioAvailable() const noexcept;

    // L'INDICE : retire une mauvaise reponse de la grille d'accords. La regle vit dans le DOMAINE ; le controleur ne
    // fait que la brancher.
    Q_INVOKABLE void useChordHint();

    // Rejoue l'accord en ARPEGE : les notes l'une apres l'autre, au lieu de plaquees.
    //
    // C'est le second indice demande - « puis le jouer en arpege » - et le meilleur des deux : il ne donne pas la
    // reponse, il donne a ENTENDRE ce qui la constitue.
    Q_INVOKABLE void playCurrentChordAsArpeggio();

    // Le week-end : c'est le moment du Bilan, et l'ecran s'en sert pour le mettre en avant.
    //
    // La regle vit dans le DOMAINE (Weekend.h) et elle y est TESTEE : un ecran qui redeciderait ici ce qu'est un week-end
    // finirait par dire autre chose que le reste de l'application.
    Q_PROPERTY( bool isWeekEnd READ isWeekEnd NOTIFY sessionChanged )

    [[nodiscard]] bool isWeekEnd() const;

    // Le BILAN : une session dont les questions sont DECIDEES, du plus facile au plus difficile, et qui finit par ce qui
    // resiste au joueur. Il se lance quand on veut ; l'ecran le met en avant le week-end.
    Q_INVOKABLE void startReviewSession();

    // Vrai pendant un bilan : l'ecran sait alors que la session est differente, et peut le dire.
    [[nodiscard]] bool isReviewRunning() const noexcept { return m_isReviewRunning; }

    // LA PAGE D'OUVERTURE DU BILAN : elle dit au joueur ce que l'app sait de lui - ses points forts, ses points faibles
    // - et a quoi le bilan sert, AVANT de lui poser la premiere question.
    //
    // C'est la piece que Roger a demandee en premier, et il l'a formulee comme une porte plutot qu'un ecran : « le
    // bilan, en montrant les points forts et les points faibles, serait une super porte d'acces vers ce cote plus
    // academique ». Un joueur qui comprend POURQUOI on lui pose ces questions travaille ; un joueur qui les subit
    // repond.
    Q_PROPERTY( bool isReviewOpeningVisible READ isReviewOpeningVisible NOTIFY sessionChanged )

    // Ce que le joueur reussit le mieux, et le moins bien : des INTERVALLES et des MODES, nommes en francais. Chaque
    // entree porte 'name', 'percent' et 'asked'.
    Q_PROPERTY( QVariantList reviewStrongPoints READ reviewStrongPoints NOTIFY sessionChanged )
    Q_PROPERTY( QVariantList reviewWeakPoints READ reviewWeakPoints NOTIFY sessionChanged )

    [[nodiscard]] bool isReviewOpeningVisible() const noexcept;

    // CE QUE LE NIVEAU ATTEND, ET QU'ON N'A PAS TRAVAILLE.
    Q_PROPERTY( QVariantList reviewLeastWorkedPoints READ reviewLeastWorkedPoints NOTIFY sessionChanged )

    [[nodiscard]] QVariantList reviewLeastWorkedPoints() const;

    [[nodiscard]] QVariantList reviewStrongPoints() const;
    [[nodiscard]] QVariantList reviewWeakPoints() const;

    // Le joueur a lu la page : le bilan commence, avec SES questions et pas avant.
    Q_INVOKABLE void beginReviewQuestions();

    // Le joueur referme la page : le bilan n'a pas eu lieu, et RIEN n'a ete compte.
    Q_INVOKABLE void cancelReviewOpening();

    // Le mot du moment : un encouragement AVANT une difficulte connue, et apres une reussite sur ce qui resistait.
    //
    // Vide quand il n'y a rien a dire, et ce n'est pas un detail : un ecran qui parle pour ne rien dire devient un ecran
    // qu'on n'ecoute plus. Le silence est ce qui donne du poids aux mots qui restent.
    [[nodiscard]] QString encouragementText() const;

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

    // CE QUE LE JOUEUR A ACQUIS a change : son titre, ses trophees, ou ce que le dernier bilan vient de rapporter.
    //
    // Sans ce signal, rien de tout cela ne se rafraichit : QML ne sait pas qu'une METHODE a change de reponse, donc une
    // liaison qui l'appelle reste figee sur ce qu'elle valait a sa creation. C'est la seconde moitie du bug du badge.
    void playerProgressChanged();

    // Le nombre de coeurs de l'Arcade a change : la page de reglages se redessine, et la prochaine partie en tiendra compte.
    void arcadeLivesChanged();

    // La gamme d'un mode vient de partir : l'ecran s'en sert pour faire voyager la tete de sa roue, de note en note. Emis
    // au moment ou le son part, jamais avant - un dessin qui demarre avant la musique montre autre chose qu'elle.
    void modePlaybackStarted();
    void scoreChanged();

    // A wrong answer has just been given, and the question is still being asked.
    //
    // The screen answers it with its body - a shake today, a vibration tomorrow - and the controller
    // has no opinion about that: it knows WHAT happened, not how it should feel. Note that this is a
    // wrong ATTEMPT, not the end of a question: a player who is told the answer has not made a mistake.
    void wrongAnswerGiven();

    // The player has just said where he is, or the application has just remembered it.
    void playerLevelChanged();

    void godModeChanged();

    void levelInvitationChanged();

    // The player has just turned an instrument on or off.
    void instrumentsChanged();

    // The player has just written or changed his name.
    void playerNameChanged();

    // The experience total has just grown, after a session ended.
    void totalExperienceChanged();

    // Le journal des questions conclues a ETE EFFACE : une remise a zero. La page de statistiques ecoute ce signal, sans
    // quoi elle montrerait encore l'histoire d'avant.
    //
    // Rien n'est emis apres chaque question, et c'est deliberé : la page se rafraichit a son OUVERTURE, et relire tout le
    // fichier a chaque question couterait cher pour une page que personne ne regarde a ce moment-la.
    void statisticsChanged();

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

    void namedIntervalQuestionShareChanged();

    void foreignNoteQuestionShareChanged();

    void phraseTempoChanged();

    // Le joueur vient de changer la part des accords.
    void chordQuestionShareChanged();

    // Un seul signal pour les deux parts de l'harmonie : elles se reglent ensemble, dans le meme ecran, et un signal
    // par part n'apporterait qu'une occasion d'en oublier un.
    void modeQuestionShareChanged();

    // The player has just pressed the "test the notification" button.
    void testReminderRequested();

    // L'application demande a Android l'autorisation d'AFFICHER des notifications.
    //
    // Un signal plutot qu'un appel direct, et c'est le motif du projet : ce controleur ne connait pas
    // l'infrastructure, il demande ; c'est main() qui sait a qui la demande s'adresse.
    void notificationPermissionRequested();

    // La boucle de rythme a avance : un temps de plus, un passage en reproduction, ou une frappe jugee.
    //
    // Un seul signal pour les trois, parce que c'est la MEME question de l'ecran - "ou en est-on ?" - et qu'un ecran
    // qui les separerait devrait les reunir lui-meme pour se redessiner.
    void rhythmStateChanged();

    // A new anecdote was drawn.
    void anecdoteChanged();

    // Le chien s'invite, ou s'en va.
    void chibaTalkingChanged();

    // L'anecdote de la question a change : une question de plus, donc une anecdote de plus.
    void questionAnecdoteChanged();

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

    // Applique la palette du GodMode aux reglages d'une partie, et SEULEMENT quand c'est la difficulte choisie.
    //
    // Le Bilan n'y passe pas : il a ses propres questions decidees, du plus facile au plus difficile, et elles n'ont rien
    // a voir avec un perimetre choisi a la main. C'est ce que Roger a demande - « le bilan va au plus simple ».
    void applyGodModeIfChosen( domain::SessionSettings & p_settings ) const;

    // Les trois listes du brouillon, chacune rangee dans l'ordre du jeu.
    [[nodiscard]] domain::GodModePalette orderedDraft() const;

    // Ecrit une famille du brouillon d'un coup : tout, ou rien.
    [[nodiscard]] static std::vector<domain::Interval> everyIntervalChecked( bool p_checked );
    [[nodiscard]] static std::vector<domain::ChordQuality> everyChordChecked( bool p_checked );
    [[nodiscard]] static std::vector<domain::Mode> everyModeChecked( bool p_checked );

    // L'etat de bilan ne doit jamais survivre a un bilan : cette fonction le referme, et c'est le seul endroit qui le fait.
    void leaveReviewMode() noexcept;

    // Adds the session's outcome - its experience, its count, its star - to the profile, once, when it ends.
    void persistSessionOutcome();

    // Ce qu'un BILAN laisse derriere lui : les quatre compteurs qui donnent les titres et les trophees. Appelee UNIQUEMENT
    // quand la partie qui se conclut etait un bilan - voir persistSessionOutcome.
    void recordBilanOutcome();

    // Ce qu'un joueur a fait de ses bilans, lu des preferences. Une seule fonction, donc une seule verite.
    [[nodiscard]] domain::BilanRecord bilanRecord() const;

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

    // Le battement suivant de la mesure, vise depuis LE DEBUT DE LA MESURE et non depuis le precedent : la mesure ne
    // peut donc pas deriver, et le premier temps de chaque phase tombe toujours a sa place. La decision elle-meme
    // appartient au domaine (domain::planNextBeat), ou elle est pure - et donc testee.
    void scheduleNextRhythmBeat();

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

    // Ce que le joueur a JOUE, puis la reponse - et VRAI seulement si la paire a ete jouee.
    //
    // Faux quand il n'y a pas d'accord appuye a faire entendre (le joueur a passe) : l'appelant retombe alors sur
    // playCurrentQuestion, qui joue la reponse seule. Un booleen plutot qu'un void, parce que « rien n'a ete joue » doit
    // pouvoir se dire.
    [[nodiscard]] bool playWrongChordThenAnswer();

    // Ecrit une ligne pour la question en cours, qui vient d'etre CONCLUE.
    //
    // L'horloge est lue ICI et nulle part ailleurs : le domaine recoit une date, il ne la demande jamais - c'est ce qui
    // garde le domaine pur et le journal testable sans attendre une seconde.
    void recordCurrentQuestion( bool p_wasCorrect, bool p_wasRevealed );

    // Les questions d'un BILAN, construites a partir des STATISTIQUES : ce que le joueur reussit d'abord, ce qui lui
    // resiste ensuite. Vide quand il n'y a pas de journal, ou pas assez de matiere pour dire « facile puis difficile ».
    [[nodiscard]] std::vector<domain::QuestionTarget> reviewPlan() const;

    // CE QUE LE BILAN SAIT DU JOUEUR : les cibles de son journal, de la plus faible a la mieux reussie, gardees par la
    // meme regle que la page de statistiques - une cible vue une seule fois n'est pas un point faible.
    //
    // Une seule source pour les DEUX pages du bilan. La page d'ouverture les MONTRE, le plan les POSE, et il devient
    // impossible qu'un ecran annonce autre chose que ce qui suit.
    [[nodiscard]] std::vector<domain::TargetStatistics> reviewInsights() const;

    // La meilleure ou la pire tranche de ces cibles, prete a afficher : au plus REVIEW_OPENING_POINT_COUNT entrees.
    [[nodiscard]] QVariantList reviewPointsOf( bool p_strong ) const;

    // Le nom qu'un JOUEUR lit pour une cible : « Quinte montante », « Dorien ». Vide pour ce que cette page ne nomme
    // pas - un accord, une cellule rythmique.
    [[nodiscard]] static QString targetLabel( const domain::TargetStatistics & p_target );

    // La question en cours fait-elle partie de ce qui RESISTE au joueur ?
    //
    // Le plan est construit dans cet ordre, donc le controleeur le sait sans recroiser les statistiques a chaque
    // question - et c'est ce qui rend l'encouragement possible au bon moment.
    [[nodiscard]] bool isCurrentQuestionAHardPart() const noexcept;

    // Joue un accord : ses notes, plaquee. Le seul chemin par lequel un accord s'entend, que ce soit pour poser la
    // question ou pour la confirmer.
    void playChordNotes( const domain::Chord & p_chord );

    // Joue la question d'harmonie en cours.
    //
    // p_secondOnly est ce qui permet les DEUX temps d'une question de couleur : le premier mode est pose tout de suite,
    // et le second par le minuteur, quand le premier a fini de sonner. Sur une question de nom, un seul appel suffit.
    void playModeQuestion( bool p_secondOnly );

    // Fait entendre une PHRASE du contenu, avec le bourdon qui la porte : la question de NOM y gagne une melodie la ou
    // une gamme montante disait la meme chose en moins de musique.
    void playPhraseQuestion( const domain::Phrase & p_phrase );

    // La note etrangere : la gamme montee sur son bourdon, avec l'intrus a sa place.
    void playForeignNoteQuestion();
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
    // L'anecdote de la question en cours, tiree a chaque question.
    QString m_questionAnecdoteText;

    // Empty when the device cannot vibrate.
    VibrationCallback m_vibrate;

    // May be null: a test, or an application that has nowhere to remember anything, must still run.
    domain::PlayerPreferences * m_levelStore{ nullptr };

    // Le livre des phrases modales, s'il a ete donne. C'est le DOMAINE qui y tire la phrase d'une question ; le controleeur
    // ne le garde que pour le passer a la session, et il ne le lit jamais lui-meme.
    const domain::PhraseBook * m_phraseBook{ nullptr };

    // May be null too, et pour la meme raison : un journal absent coute des statistiques, jamais une partie.
    domain::QuestionLog * m_questionLog{ nullptr };

    // Les textes de la page Accordeur, lus par l'application au demarrage. Un guide vide est un cas normal : la page
    // garde alors ses propres phrases, plus courtes, et rien ne casse.
    domain::TunerGuide m_tunerGuide;

    // Le bilan en cours, et le nombre de questions qui l'ont ouvert. Ces deux valeurs suffisent a dire au joueur ou il
    // en est : l'echauffement est passe, ce qui suit est ce qui lui resiste.
    bool m_isReviewRunning{ false };

    // LE MODE DE LA PARTIE EN COURS, et c'est lui qui decide si elle paie : voir persistSessionOutcome.
    //
    // Par defaut l'Arcade, parce que c'est la porte principale - « Jouer » ouvre l'Arcade depuis que le jeu a quatre modes.
    domain::GameMode m_gameMode{ domain::GameMode::Arcade };

    // La famille du dernier Entrainement, pour que « Rejouer » rejoue le MEME.
    int m_lastTrainingFamily{ 0 };

    // Le multiplicateur de la derniere Arcade, en pour cent, FIGE au moment ou elle s'est conclue.
    int m_lastArcadeMultiplierPercent{ 100 };

    // L'experience REELLEMENT gagnee par la derniere partie, multiplicateur applique.
    int m_arcadeXpEarned{ 0 };

    // Le chrono de la partie EN COURS, et sa valeur FIGEE a la fin. Deux membres, parce qu'un seul ne saurait pas dire
    // « arrete » : le timer court tant que la partie vit, et l'ecran de fin lit le nombre, pas le timer.
    QElapsedTimer m_sessionClock;
    int m_sessionDurationSeconds{ 0 };

    // Ce que le dernier BILAN a rapporte de neuf : les trophees tout frais, et si le titre a monte. Voir
    // newlyEarnedTrophies : remis a zero au debut de chaque partie.
    QVariantList m_newlyEarnedTrophies;
    bool m_titleJustIncreased{ false };

    // Le code de developpeur : sept choix de GodMode d'affilee, et toutes les difficultes s'ouvrent.
    std::size_t m_godModeSelectionCount{ 0 };
    bool m_allLevelsUnlocked{ false };

    // Le chien qui s'invite : arme par la fin d'une partie, desarme par le clic du joueur.
    bool m_isChibaTalking{ false };

    // Son humeur du moment : une des quatre planches, tiree a chaque fois qu'il parle.
    QString m_chibaImageSource{ QStringLiteral( "qrc:/assets/images/chibaSpeak.png" ) };

    // L'ecart mesure du dernier chant juge, mis de cote au moment de la reponse : le micro est resynchronise juste
    // apres, et la mesure serait perdue avec lui.
    int m_lastSungCentsOffset{ 0 };
    std::size_t m_reviewEasyQuestionCount{ 0 };

    // LA PAGE D'OUVERTURE : visible apres le clic sur « Bilan », avant la premiere question. Les reglages prepares
    // attendent ici, parce que le bilan ne commence qu'une fois la page lue - voir beginReviewQuestions.
    bool m_reviewOpeningVisible{ false };
    domain::SessionSettings m_pendingReviewSettings{};

    // Read once from the store, then kept here: the screen asks for it on every question, and a settings file
    // has no business being read that often.
    std::optional<domain::PlayerLevel> m_playerLevel;

    // LE BROUILLON DU GODMODE, et l'etat de sa sauvegarde.
    //
    // Le brouillon est la palette telle que la page la montre ; la sauvegarde est ce qui est ecrit. Les deux peuvent
    // differer, et c'est exactement ce que « GodMode (non sauvegarde) » veut dire : tant que le joueur n'a pas appuye,
    // ses clics ne changent RIEN a ce qu'il jouera - et c'est ce qui rend le bouton utile plutot que decoratif.
    domain::GodModePalette m_godModeDraft;
    bool m_godModeIsChosen{ false };
    bool m_godModeIsSaved{ false };

    // LA PALETTE QUI JOUE : celle qui a ete sauvegardee, ou, au premier lancement, celle du niveau courant.
    //
    // Elle est distincte du brouillon, et c'est la decision de Roger : « il peut faire ses changements PUIS appuyer sur
    // sauvegarder POUR POUVOIR JOUER de cette maniere ». Tant qu'il n'a pas appuye, ses clics ne changent rien a ce qu'il
    // jouera - et c'est ce qui donne un sens au mot « sauvegarde ».
    domain::GodModePalette m_godModeSavedPalette;
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

    // Quel passage de la comparaison SONNE : deux modes s'enchainent, et la roue doit dessiner celui qu'on entend - voir
    // modeCircle, ou cette valeur decide du mode affiche.
    bool m_modeSecondPassageIsPlaying{ false };

    QTimer m_rhythmTimer;

    // Le minuteur qui pose le SECOND mode d'une question de couleur, apres le premier.
    //
    // Deux modes ne peuvent pas tenir dans un seul appel au port : chaque appel rend une melodie sur un bourdon, tenu du
    // debut a la fin. Le second mode est donc joue par ce minuteur, et le bourdon est repose a l'identique - la meme
    // tonique, donc l'oreille entend une continuite et non deux questions.
    QTimer m_modeTimer;

    QElapsedTimer m_rhythmClock;

    // Le temps a jouer dans la mesure, de 0 a beatsPerBar. La valeur beatsPerBar n'est pas un temps : c'est le signal
    // que la mesure est finie et que la phase suivante commence. C'est ce qui fait tomber le premier temps de la
    // reproduction PILE a l'instant ou l'ecoute aurait joue le sien, au lieu d'un temps trop tot - un temps perdu a
    // chaque mesure, qui s'entend comme un metronome qui boite.
    int m_rhythmBeatIndex{ 0 };

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
