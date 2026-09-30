#pragma once

// =====================================================================================================================
// Musichien - ExerciseSession
//
// The loop itself: ask a question, let the player answer, say what was heard, move on.
//
// It owns the palette of the player, the score, and the current question, and it decides everything a
// session decides. It does NOT decide three things, on purpose:
//
//   * it never produces a sound: playing belongs to the NotePlayer port, and the session asks for
//     nothing;
//   * it never waits: the domain has no clock, so the pause between the feedback and the next question
//     belongs to the screen, which calls advance() when it is ready;
//   * it never decides how anything looks.
//
// Consequence: a whole session can be played in a unit test, in microseconds, with no sound card, no
// phone and no waiting. That is the only reason the rules below can be changed with any confidence.
//
// See docs/ARCHITECTURE.md, and note 16 of the vault for the design behind the rules.
// =====================================================================================================================

#include "domain/exercise/SessionScore.h"
#include "domain/music/Chord.h"
#include "domain/music/Interval.h"
#include "domain/music/Mode.h"
#include "domain/music/Note.h"
#include "domain/music/PhraseBook.h"
#include "domain/rhythm/Rhythm.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <span>
#include <vector>

namespace musichien::domain
{

// Ce qu'une question demande au joueur.
//
// Le jeu d'origine demande le NOM d'un intervalle. Le mode guide demande la DIRECTION : l'intervalle est joue, et le
// joueur dit seulement s'il monte ou s'il descend.
//
// La definition vit ICI, avant les reglages, parce qu'un PLAN de questions (le Bilan) s'exprime avec elle : un plan se
// pose dans les reglages, et une enumeration utilisee par eux doit venir avant eux.
// Ce qu'un joueur peut repondre a une question de COULEUR : le second passage est-il plus clair, plus sombre, ou le
// meme ?
//
// La troisieme reponse est une demande de Roger - « bug dans le plus clair plus sombre, si les 2 modes sont identiques,
// erreur. rajouter un bouton egal » - et elle a amene une regle : pour que « pareil » soit une BONNE reponse de temps en
// temps, il faut que le jeu pose parfois deux fois la meme couleur. Voir SessionSettings::sameColourQuestionShare.
//
// C'est un exercice qui a du sens, et pas seulement une reponse de plus : entendre qu'il n'y a PAS de difference est une
// competence d'oreille, et c'est celle qu'on perd en cherchant toujours quelque chose a entendre.
enum class ModeColourAnswer : std::int32_t
{
    // Les valeurs sont explicites : l'interface les lit comme un nombre, et une enumeration qui se renumerote toute seule
    // deplacerait les boutons sans que rien ne casse au build.
    Brighter = 0,
    Darker = 1,
    Same = 2
};

enum class QuestionKind
{
    // Les valeurs sont EXPLICITES parce que l'interface les lit comme un nombre : la page compare questionKind a 0, 1,
    // 2, 3 et 4, et une enumeration qui se renumerote toute seule deplacerait les ecrans sans que rien ne casse au build.
    NamedInterval = 0,
    Direction = 1,
    Sing = 2,

    // Reproduire une cellule rythmique : la cellule est ecoutee, puis le joueur la rejoue en tapant.
    //
    // Une question comme les autres, et c'est tout l'interet : elle est posee par la meme session, payee par le meme
    // score, et coutee par les memes vies. Le rythme n'est pas une page a part, c'est une question de plus.
    Rhythm = 3,

    // Reconnaitre un accord : il est joue plaque, et le joueur dit de QUELLE COULEUR il est - majeur, mineur, et
    // ensuite ce que la palette lui a appris.
    //
    // La question la plus simple du jeu a poser, et la plus difficile a repondre : un accord, c'est plusieurs notes,
    // et l'oreille doit entendre leur RAPPORT plutot que les notes elles-memes.
    Chord = 4,

    // Le DEGRADE : deux modes sont joues l'un apres l'autre sur le meme bourdon, et le joueur dit si le second est
    // plus CLAIR ou plus SOMBRE que le premier.
    //
    // C'est la premiere question du pilier harmonie, et elle est volontairement la plus simple des deux : comparer deux
    // couleurs entendues dans la foulee ne demande aucun nom, donc aucun vocabulaire - seulement une oreille. Roger :
    // « l'exercice peut etre simple pour le joueur, mais reste un bon exercice pour s'imprégner des modes et des
    // couleurs ».
    //
    // Le BOURDON est ce qui la rend possible : sans centre, deux gammes ne sont pas deux modes, et la question n'aurait
    // pas de reponse. Voir la note 27 du vault, §4.
    ModeColour = 5,

    // Et le NOM, ensuite : un seul mode est joue sur le bourdon, et le joueur le nomme parmi ceux de sa palette.
    //
    // La meme couleur, mais dite avec un mot. Les deux genres sont SEPARES parce qu'ils ne sont pas difficiles de la
    // meme facon : comparer est une affaire d'oreille, nommer une affaire de vocabulaire - et un joueur qui reussit la
    // premiere en ratant la seconde apprend quelque chose de lui-meme. Il faut, pour le voir, que leurs statistiques
    // soient distinctes.
    // Et le NOM, ensuite : un seul mode est joue sur le bourdon, et le joueur le nomme parmi ceux de sa palette.
    //
    // La meme couleur, mais dite avec un mot. Les deux genres sont SEPARES parce qu'ils ne sont pas difficiles de la
    // meme facon : comparer est une affaire d'oreille, nommer une affaire de vocabulaire - et un joueur qui reussit la
    // premiere en ratant la seconde apprend quelque chose de lui-meme. Il faut, pour le voir, que leurs statistiques
    // soient distinctes.
    ModeName = 6,

    // Le VAMP : la MEME gamme, posee sur deux centres differents.
    //
    // C'est la demonstration la plus directe du §4 de la note 27, et la plus utile : do dorien et si bemol majeur ont
    // exactement les MEMES sept notes, et ce n'est pas la meme musique. Le joueur entend deux fois la meme suite de
    // notes, sur deux bourdons differents, et il doit dire si la couleur a bouge.
    //
    // C'est aussi la seule question ou la reponse est dans le CONTEXTE et non dans les notes : le meme materiau,
    // deux modes. Un joueur qui a compris cela a compris ce qu'un mode est.
    ModeVamp = 7,

    // LA NOTE ETRANGERE : une gamme monte, une note par degre, et l'une d'elles n'appartient pas a la gamme. Le joueur
    // dit LAQUELLE.
    //
    // C'est le troisieme exercice d'harmonie, et il est different des deux premiers : ceux-la demandent de comparer deux
    // couleurs ou d'en nommer une, celui-ci demande de tenir une gamme ENTIERE dans sa tete et d'y reperer l'intrus.
    // C'est le plus fin des trois, et c'est pour cela qu'il vient en dernier.
    //
    // Roger l'a demande des le debut - « quelle note n'est pas dans la gamme ? » - et il a attendu que la roue des
    // quintes existe : sans un appui visuel, l'exercice est une devinette a sept choix.
    ForeignNote = 8
};

// La question porte-t-elle sur un INTERVALLE qui a ETE JOUE ?
//
// La reponse est OUI pour les trois premiers genres, et NON pour tous les autres - et cette fonction existe parce que
// la question se pose a trois endroits : l'ecran, pour savoir s'il a un indice a montrer ; la session, pour savoir si
// une aide doit reculer la palette ; et le contenu, qui n'a de souvenirs que d'intervalles.
//
// Toute question porte un intervalle - c'est l'ordre des tirages qui veut ca - mais sur une cellule rythmique, une
// couleur d'accord ou un mode, PERSONNE ne l'a jamais entendu. Trois listes recopiees sont trois listes qu'on oublie
// d'etendre : c'est exactement ce qui est arrive, et Roger l'a vu avant nous - « pour les bourdons quand je fail, je
// vois l'indice des intervalles apparaitre » : l'intervalle tire au hasard pour une question de mode tombait sur un
// souvenir de film.
[[nodiscard]] constexpr bool isIntervalQuestion( QuestionKind p_kind ) noexcept
{
    switch( p_kind )
    {
        case QuestionKind::NamedInterval:
        case QuestionKind::Direction:
        case QuestionKind::Sing:
            return true;

        case QuestionKind::Rhythm:
        case QuestionKind::Chord:
        case QuestionKind::ModeColour:
        case QuestionKind::ModeName:
        case QuestionKind::ModeVamp:
        case QuestionKind::ForeignNote:
            return false;
    }

    // Inatteignable tant que le commutateur ci-dessus couvre tous les genres, et c'est voulu : un genre AJOUTE sans
    // etre classe ici doit faire echouer le test qui parcourt l'enumeration, pas tomber dans un « oui » silencieux.
    return false;
}

// Une question DECIDEE a l'avance : quel genre, quelle cible, dans quel sens.
//
// C'est ce qui permet a une session de suivre un PLAN plutot qu'un tirage, et c'est la seule chose dont le Bilan du
// week-end a besoin : commencer par ce que le joueur reussit, puis attaquer ce qui lui resiste. Une partie ordinaire
// laisse la liste vide et tire comme avant - le hasard reste le mode par defaut du jeu.
struct QuestionTarget
{
    QuestionKind kind{ QuestionKind::NamedInterval };

    // La cible, dans l'unite de son genre (voir QuestionRecord::target) : des demi-tons pour un intervalle, l'index
    // d'une qualite pour un accord.
    std::int32_t target{ 0 };

    IntervalDirection direction{ IntervalDirection::Ascending };
};

// Everything that can be tuned in a session, in one place.
//
// These are RULES OF THE GAME, not constants of the code: they will end up in a data file, because
// tuning how hard an exercise is must never require a recompilation. Written here for now, with the
// values of the first playable loop.
struct SessionSettings
{
    // How many questions a session asks before it is over.
    std::size_t questionCount{ 10 };

    // How many intervals the player starts with, out of the learning order.
    std::size_t startingPaletteSize{ 2 };

    // The largest grid offered. The grid is min(palette, this value): it grows with the palette until
    // it reaches this size, and stops there. Beyond it the palette keeps growing, so new intervals
    // appear inside a grid that stays readable.
    std::size_t choiceCount{ 6 };

    // Consecutive correct answers needed before a new interval joins the palette.
    std::size_t successesBeforeWidening{ 3 };

    // Wrong answers on the same question before the answer can be revealed.
    std::int32_t wrongAttemptsBeforeHelp{ 3 };

    // Wrong answers on the same question before the MEMORY HINT appears.
    //
    // ONE, and that is the point: a hint is not a last resort, it is what turns a mistake into a
    // connection. Waiting for three mistakes before helping would mean waiting for a player to be
    // discouraged, and the hint is a nudge where the "Réponse" button is a rescue.
    std::int32_t wrongAttemptsBeforeHint{ 1 };

    // Whether the two aids are offered at all: the hint that nudges on the first mistake, and the button
    // that gives the answer away after three.
    //
    // ONE flag for both, because "no aid" is one decision and not two: a mode where the player measures
    // himself against the whole palette cannot afford either. Cutting only one of them would leave the
    // other to give away the same answer.
    //
    // It is a RULE and not a setting of the screen, which is why it lives here with the others: the day
    // the rules move to a data file, "Master offers no help" is a line of that file, not a line of QML.
    bool aidsAllowed{ true };

    // Lives of the session. Empty means no limit: it is what the training mode of the first version
    // will use. The first playable loop keeps it finite, because a rule nobody can feel is a rule
    // nobody can judge.
    std::optional<std::int32_t> lives{ 5 };

    // Range both notes of a question have to stay inside, whatever the direction.
    //
    // Two octaves wide, and wider than the window below on purpose: it is the window that decides how
    // much variety the player hears, and this range is only the safety net around it.
    std::int32_t lowestPlayableMidiNumber{ 40 };

    std::int32_t highestPlayableMidiNumber{ 84 };

    // The window the note a question STARTS ON is drawn from.
    //
    // Two octaves, and the range above narrows it further depending on the size of the interval and its
    // direction. A single octave made two questions in five start on the same note, which turns the exercise
    // into a memory test.
    std::int32_t lowestRootMidiNumber{ 50 };

    std::int32_t highestRootMidiNumber{ 74 };

    // How often each direction is drawn, out of their total.
    //
    // ALL THREE, and this is the point: an interval heard only upwards is half an interval.
    //
    //   * descending is the SAME distance heard the other way, and it is a separate skill: the ear that
    //     recognises a rising fifth does not automatically recognise a falling one;
    //   * the harmonic form drops the melody altogether and leaves only the colour, which is the
    //     hardest of the three - hence the smallest share.
    //
    // The three are also what the statistics will have to separate: "I recognise fifths" means nothing
    // if it does not say in which direction.
    std::int32_t ascendingShare{ 50 };
    std::int32_t descendingShare{ 30 };
    std::int32_t harmonicShare{ 20 };

    // Share of questions, in percent, that ask the DIRECTION instead of the interval: "does it go up or down?".
    //
    // ZERO by default, so that the original game is untouched unless it is asked for. This is the guided mode of
    // the next step, and it is a RULE like the rest - a share the settings can set to fifty, never a flag the
    // screen flips.
    std::int32_t directionQuestionShare{ 0 };

    // La question historique du jeu : NOMMER l'intervalle entendu.
    //
    // Elle a SA part, comme les autres genres, et c'est une correction - pas un champ de plus. Avant, elle etait le
    // seul genre SANS part : elle prenait « ce qui restait », et un reglage dont la somme depassait cent la faisait
    // disparaitre en silence, comme il faisait disparaitre les derniers genres de la liste.
    //
    // Roger a mis le doigt dessus : « on commence a avoir beaucoup de spinbox 0-100 en disant que c'est des parts, mais
    // sur dix questions, c'est plus des probabilites non ? ». C'est exactement cela, et la part qui manquait est
    // celle-ci. Les parts sont maintenant des POIDS, lus les uns par rapport aux autres : leur somme est l'echelle, et
    // le total n'a plus besoin de faire cent.
    //
    // Soixante par defaut, avec le chant et les accords a vingt : la somme fait cent, et le jeu sonne exactement comme
    // avant que cette part existe.
    std::int32_t namedIntervalQuestionShare{ 60 };

    // Share of questions, in percent, that ask the player to SING the interval instead of naming it.
    //
    // Twenty by default: enough that the voice shows up regularly in a session, small enough that the listening
    // core stays the main game. Like the direction share, it is a RULE - the mixed session is one setting, not a
    // separate screen.
    std::int32_t singQuestionShare{ 20 };

    // Le rythme n'est PLUS un exercice de ce jeu, et c'est une decision de Roger, prise en jouant : « je pense qu'on peut
    // enlever les exercices de Rythme de l'app (mais garder le metronome). Ils seront jamais implementes ni utilises. »
    //
    // La part a disparu - c'etait le seul moyen de demander une cellule - et le genre a disparu du TIRAGE avec elle. Ce
    // qui reste est ce que Roger garde : le pilotage, la frappe, le metronome et les echantillons de batterie. C'est
    // aussi pourquoi QuestionKind::Rhythm est encore declaree : la demonter vraiment demanderait de demonter la frappe,
    // et un bouton de moins ne vaut pas un metronome casse.
    //
    // Aucune ligne de code ne pose donc plus de cellule : voir drawKind, dont la table des parts ne contient plus le
    // rythme.

    // Le tempo, en battements par minute, auquel la cellule rythmique est posee.
    //
    // Une regle du jeu, et pas un reglage d'ecran : juger une frappe, c'est convertir une distance en temps, puis
    // en millisecondes. Sans le tempo, le domaine ne peut pas dire ce qu'une frappe vaut - et c'est justement lui
    // qui doit le dire.
    int rhythmBpm{ 90 };

    // Combien de tentatives ratees avant que la question de rythme ne puisse etre passee.
    //
    // UNE seule, la ou une question d'intervalle en demande trois, et la difference se defend : un intervalle
    // s'ecoute autant de fois qu'on veut, alors qu'une cellule ne s'entend que pendant sa boucle d'ecoute. Rater
    // une cellule ne se debloque pas en la rejouant, et un appareil dont la latence n'est pas encore calibree peut
    // rendre l'exercice injuste. Une tentative perdue, et le joueur peut passer - ce qui reste une aide, pas une
    // recompense : passer coute la question.
    std::int32_t wrongAttemptsBeforeRhythmHelp{ 1 };

    // Share of questions, in percent, that ask the player to RECOGNISE a chord.
    //
    // Twenty by default, like the others, and reglable comme les autres : c'est un genre de question, et un genre de
    // question se dose toujours (le rythme nous l'a appris).
    std::int32_t chordQuestionShare{ 20 };

    // Combien de qualites d'accord le joueur a rencontrees au depart.
    //
    // DEUX, et ce sont majeur et mineur : la couleur de reference et son ombre, les deux seules qu'on puisse opposer
    // sans avoir rien appris d'autre. La palette s'elargit ensuite avec les reussites, une qualite a la fois, dans
    // l'ordre de chordLearningOrder() - les triades d'abord, les accords a quatre notes a la fin.
    std::size_t startingChordQualityCount{ 2 };

    // Share of questions, in percent, that ask the player to COMPARE two modes : « le second est-il plus clair ou plus
    // sombre que le premier ? ».
    //
    // ZERO par defaut, comme le rythme et pour la meme raison : une question d'harmonie dans une session d'intervalles
    // doit se DEMANDER, jamais s'imposer. Le pilier harmonie se dose comme les autres.
    // Part des questions de couleur ou les DEUX passages ont la meme couleur, en poids comme les autres parts.
    //
    // C'est ce qui donne une bonne reponse au bouton « pareil » de Roger : sans cela, le bouton serait un piege
    // permanent, et un bouton qui n'est jamais juste n'apprend rien. A zero, le jeu ne compare que des modes differents -
    // le comportement d'avant.
    std::int32_t sameColourQuestionShare{ 20 };

    // Part des questions qui font chercher la NOTE ETRANGERE d'une gamme.
    //
    // ZERO par defaut, comme les autres parts de l'harmonie : le pilier se decouvre en l'allumant, et une question dont on
    // ne sait pas encore quoi faire n'a rien a faire dans une session ordinaire.
    std::int32_t foreignNoteQuestionShare{ 0 };

    std::int32_t modeColourQuestionShare{ 0 };

    // Share of questions, in percent, that ask the player to NAME a mode heard on a drone.
    //
    // SEPAREE de la part de couleur, et c'est tout l'interet : entendre qu'une couleur a change est une chose, savoir
    // la nommer en est une autre. Un joueur qui reussit la premiere en ratant la seconde apprend quelque chose de son
    // oreille - et il faut, pour le voir, que les deux questions soient comptees separement.
    std::int32_t modeNameQuestionShare{ 0 };

    // Share of questions, in percent, that play the SAME scale on TWO different centres.
    //
    // La plus avancee des trois questions d'harmonie, et la seule dont la reponse soit dans le CONTEXTE : les notes ne
    // bougent pas d'un passage a l'autre, et le mode, si. C'est ce qu'un mode veut dire, et il faut l'entendre une fois
    // pour le croire.
    std::int32_t modeVampQuestionShare{ 0 };

    // Combien de modes le joueur a rencontres au depart.
    //
    // DEUX, et ce sont le majeur et le mineur : le seul ecart que toute oreille connait deja, et la premiere question
    // possible. La palette s'elargit ensuite d'un mode tous les trois succes, du connu vers les extremes.
    std::size_t startingModeCount{ 2 };

    // Les questions a poser, dans l'ORDRE, quand la session doit suivre un plan.
    //
    // Vide pour une partie ordinaire : le tirage decide. Rempli pour un Bilan, ou l'ordre EST le sujet - du plus facile
    // au plus difficile, et l'on finit par ce qui resiste.
    std::vector<QuestionTarget> plannedQuestions;

    // Silence left between the two notes of a question, as heard.
    //
    // A musical value rather than a technical one: too short and the two notes sound like one glide,
    // too long and the first note is forgotten before the second one arrives. It lives here, with the
    // other rules, rather than in the screen, so that it can be tuned without touching the interface.
    std::chrono::milliseconds melodicGap{ 300 };
};

// A question, as the screen needs it.
// Le genre est-il OUVERT dans ces reglages - c'est-a-dire sa part est-elle non nulle ?
//
// La question se pose a DEUX endroits : le tirage, qui la lit deja, et le PLAN d'un bilan, qui l'ignorait. C'est
// exactement le defaut que Roger a signale : « j'ai beau mettre plus clair et plus sombre a 0, je l'obtiens toujours dans
// mes parties ». Le bilan ne passe pas par le tirage - il IMPOSE son plan - donc les parts ne s'appliquaient pas a lui.
//
// Ecrite une fois, ici, elle ne peut plus etre oubliee a un troisieme endroit.
[[nodiscard]] bool isKindOpen( const SessionSettings & p_settings, QuestionKind p_kind ) noexcept;

struct Question
{
    // What the question asks. The screen reads it to know whether to show the circle or the two directions.
    QuestionKind kind{ QuestionKind::NamedInterval };

    // The note the interval is played from.
    std::int32_t rootMidiNumber{ 60 };

    // The interval being asked. Everything the feedback says is derived from this, never from what the
    // player answered.
    Interval target;

    // What is offered. The right answer is one of them, always, and it is never the only one.
    std::vector<Interval> choices;

    // How the two notes are sounded. Only ascending is generated for now; the field is here because
    // the guided mode of the next step works on the direction, and a question that cannot express one
    // would have to be rewritten.
    IntervalDirection direction{ IntervalDirection::Ascending };

    // Times the player asked to hear the interval again. The first listening is not one of them: it is
    // how the question is asked.
    std::int32_t replayCount{ 0 };

    // Wrong answers given on this question so far.
    std::int32_t wrongAttemptCount{ 0 };

    // -------------------------------------------------------------------------------------------------------------
    // La cellule rythmique
    //
    // Ces champs ne veulent dire quelque chose que sur une question RYTHMIQUE, et c'est le domaine qui les remplit :
    // l'ecran lit la cellule et le tempo, il ne les invente pas.
    // -------------------------------------------------------------------------------------------------------------

    // Quelle cellule, comme index dans allRhythmPatterns().
    //
    // Un INDEX plutot qu'un pointeur : une question se copie (l'ecran la lit, les tests la gardent), et un pointeur
    // obligerait chaque copie a negocier la duree de vie de ce qu'elle designe. L'ordre de la liste est deja un
    // contrat pour l'interface, il l'est donc aussi ici.
    std::size_t patternIndex{ 0 };

    // Le tempo auquel la cellule est posee, pose par la session au moment de la question.
    int bpm{ 90 };

    // Les frappes de la tentative EN COURS : une case par frappe de la cellule, vraie quand l'onset a ete touche juste.
    //
    // C'est ce qui fait la difference entre "j'ai tape juste" et "j'ai reproduit la cellule" : une tentative est
    // juste quand TOUTES les cases sont vraies, et remises a zero a chaque boucle, pour qu'une frappe oubliee une
    // fois ne le reste pas pour toujours.
    std::vector<bool> coveredOnsets;

    // Les frappes du joueur qui ne tombaient pres d'aucune frappe de la cellule.
    std::int32_t offBeatTapCount{ 0 };

    // -------------------------------------------------------------------------------------------------------------
    // L'accord
    //
    // Ces champs ne veulent dire quelque chose que sur une question d'ACCORD. Le domaine les remplit, l'ecran les lit.
    // -------------------------------------------------------------------------------------------------------------

    // L'accord pose : sa couleur, et la tonique au-dessus de laquelle il est construit.
    Chord chord;

    // Ce que le joueur a le droit de repondre : les qualites de sa palette, dans l'ordre d'apprentissage.
    //
    // PAS de leurres tires au hasard, contrairement aux intervalles, et la difference est de fond : un intervalle a des
    // voisins (une seconde majeure contre une mineure), une qualite d'accord n'en a pas. Cacher une qualite que le
    // joueur connait ne rendrait pas la question plus juste, seulement plus sournoise.
    std::vector<ChordQuality> chordChoices;

    // -------------------------------------------------------------------------------------------------------------
    // Le mode
    //
    // Ces champs ne veulent dire quelque chose que sur une question d'HARMONIE. Le domaine les remplit, l'ecran les lit.
    // -------------------------------------------------------------------------------------------------------------

    // La tonique, celle que le BOURDON tient. C'est elle qui donne un centre aux modes : sans elle, deux modes ne sont
    // que deux gammes, et la question n'a pas de reponse.
    Note modeTonic{ 62 };

    // La tonique du PREMIER passage d'un VAMP. Le jeu en joue deux, et ce sont deux centres differents sous la MEME
    // gamme : c'est le centre qui change, et rien d'autre.
    Note previousModeTonic{ 62 };

    // Le mode pose, et celui qui vient d'etre entendu juste avant - sur une question de COULEUR seulement.
    //
    // Le precedent est un optional, et pas un mode « vide » : sur une question de NOM il n'y a rien a comparer, et un
    // champ qui contiendrait une valeur sans signification finirait par etre lu par quelqu'un.
    Mode mode{ Mode::Ionian };
    std::optional<Mode> previousMode;

    // LA NOTE ETRANGERE : la gamme jouee, et le pas ou l'intrus se trouve.
    //
    // La melodie est gardee ENTIERE, et pas seulement la note fautive : c'est elle qu'on rejoue quand le joueur demande a
    // reecouter, et la reconstruire a chaque lecture serait deux facons de dire la meme chose. Sept notes, une par degre,
    // dans l'ordre de la gamme.
    std::vector<Note> foreignMelody;

    // L'index de l'intrus dans cette melodie : c'est LA reponse. Le joueur a entendu sept notes, il dit laquelle
    // n'appartenait pas a la gamme.
    std::int32_t foreignStepIndex{ 0 };

    // Ce que le joueur a le droit de repondre : les modes de sa palette, dans l'ordre d'apprentissage.
    //
    // Comme pour les accords, et pour la meme raison : un mode que le joueur n'a jamais rencontre ne serait pas un
    // choix, seulement un piege.
    std::vector<Mode> modeChoices;

    // LA MELODIE, quand la question en a une : sur une question de NOM, le mode s'entend dans une PHRASE plutot que
    // dans une gamme qui monte.
    //
    // C'est ce que les trois cents phrases de l'atelier attendaient. Le banc d'essai l'avait ecrit d'avance : « la phrase
    // fait aussi connaitre son MODE : l'ecran n'a pas a deviner lequel a sonne, puisque c'est justement la question que
    // l'exercice posera un jour ».
    //
    // Un optional, et jamais une phrase vide : une phrase sans pas se jouerait comme un silence, alors qu'un mode sans
    // phrase doit simplement s'entendre en gamme. Les deux sont des questions valides ; il n'y en a qu'une qui sonne.
    std::optional<Phrase> modePhrase;
};

enum class SessionState
{
    Asking,      // the player is choosing
    Feedback,    // the answer is known, and is being shown
    Finished     // no more questions
};

class ExerciseSession
{
public:
    // The seed is provided, never drawn here: the domain owns no entropy source, so the same seed
    // always produces the same session, which is what makes every rule above testable.
    explicit ExerciseSession( std::uint32_t p_seed,
                              SessionSettings p_settings = {},
                              const PhraseBook * p_phraseBook = nullptr );

    [[nodiscard]] const Question & currentQuestion() const noexcept { return m_currentQuestion; }
    [[nodiscard]] SessionState state() const noexcept { return m_state; }
    [[nodiscard]] const SessionScore & score() const noexcept { return m_score; }
    [[nodiscard]] const SessionSettings & settings() const noexcept { return m_settings; }

    // Intervals the player is currently up against, from the learning order.
    [[nodiscard]] std::span<const Interval> palette() const noexcept { return m_palette; }

    // Number of the question being asked, starting at one. Clamped to the last question once the
    // session is over, so that a screen can print "10 / 10" without a special case.
    [[nodiscard]] std::size_t questionNumber() const noexcept { return m_questionNumber; }

    [[nodiscard]] bool isFinished() const noexcept { return m_state == SessionState::Finished; }

    // True once the answer may be revealed: the player has tried enough.
    [[nodiscard]] bool isHelpAvailable() const noexcept;

    // True once the memory hint may be shown: the player has tried, and a snatch of music might unblock
    // them.
    //
    // Distinct from isHelpAvailable, and the difference is the whole design: a hint NUDGES ("remember
    // Star Wars?"), help GIVES UP ("it was a fifth"). The first should arrive early, the second late.
    [[nodiscard]] bool isHintAvailable() const noexcept;

    // True while the player is still allowed to hear the interval again.
    [[nodiscard]] bool canReplay() const noexcept { return m_state == SessionState::Asking; }

    // -------------------------------------------------------------------------------------------------------------
    // L'indice d'accord
    //
    // L'indice est DONNE par le domaine, et pas par l'ecran : c'est le domaine qui sait ce qui est juste, et un ecran
    // qui deciderait ce qui est faux aurait la reponse au bout de la main.
    // -------------------------------------------------------------------------------------------------------------

    // Vrai quand retirer une mauvaise reponse a un sens : c'est une question d'ACCORD, les aides sont autorisees, le
    // joueur a deja essaye au moins une fois, et il reste de quoi retirer.
    //
    // L'essai rate fait partie de la regle, et pas de l'ecran : un indice offert avant d'avoir essaye ne serait pas un
    // indice, ce serait un raccourci.
    [[nodiscard]] bool canRemoveOneWrongChordChoice() const noexcept;

    // Vrai quand ecouter l'accord en ARPEGE a un sens : une question d'accord, des aides autorisees, et un essai deja
    // rate.
    //
    // Distinct de canRemoveOneWrongChordChoice, et la difference compte pour un DEBUTANT : sa palette n'offre que deux
    // couleurs, donc il n'y a jamais rien a retirer - mais entendre l'accord note a note lui reste precieux, et c'est
    // meme a ce moment-la l'aide la plus utile.
    [[nodiscard]] bool canHearChordAsArpeggio() const noexcept;

    // Retire UNE mauvaise reponse de la question d'accord en cours, au HASARD parmi les fausses.
    //
    // Au hasard, et c'est important : retirer toujours la premiere de la palette apprendrait au joueur que le premier
    // bouton est un mauvais choix, ce qui est une information fausse - et l'indice qui apprend quelque chose de faux
    // n'aide personne.
    //
    // Rend false quand il n'y a rien a retirer. Ce n'est pas un echec : c'est une question qui n'a plus rien a cacher.
    [[nodiscard]] bool removeOneWrongChordChoice();

    [[nodiscard]] bool wasLastAnswerCorrect() const noexcept { return m_lastAnswerWasCorrect; }

    // What the player answered last, when there is something to show.
    [[nodiscard]] std::optional<Interval> lastAnswer() const noexcept { return m_lastAnswer; }

    // La couleur d'accord que le joueur a nommee en dernier, quand il y en a une : c'est ce qui permet au verdict de
    // dire "c'etait un accord mineur, tu as repondu majeur" plutot qu'un simple "faux".
    [[nodiscard]] std::optional<ChordQuality> lastChordAnswer() const noexcept { return m_lastChordAnswer; }

    // The player asked to hear the interval again. Counted, and nothing else: playing it is the job of
    // the adapter, which calls this so that the count matches what was really heard.
    void registerReplay() noexcept;

    // The player chose an interval, given as a distance in semitones. Returns whether it was right.
    // A wrong answer does not end the question: it is asked again, which is how one gets to try.
    bool answer( std::int32_t p_semitones );

    // The player says which way the interval went, on a guided question. Returns whether it was right.
    //
    // Refused on a question that asked for a name: the two answers are different languages, and accepting a
    // direction where an interval was expected would let a lucky tap score by accident.
    bool answerDirection( IntervalDirection p_direction );

    // The player SANG the interval, on a sung question. Returns whether it was right. Nothing was picked from a
    // grid - the voice is the answer, so there is no interval to record as "chosen".
    bool answerSung( bool p_isCorrect );

    // Le joueur a tape, sur une question rythmique, a cette position dans la cellule (en TEMPS, 0 = le premier temps
    // de la boucle de reproduction). Rend la qualite de la frappe, pour que l'ecran la montre sur-le-champ.
    //
    // Le temps entre dans le domaine par la PORTE, et c'est la seule chose que le rythme lui demande : le domaine ne
    // mesure rien, il juge une position qu'on lui donne - exactement comme judgeTap juge un temps qu'on lui donne. Le
    // controleeur, qui possede l'horloge, appelle ceci a chaque frappe.
    //
    // Une frappe JUSTE marque l'onset le plus proche comme couvert. Une frappe a cote est comptee, et rien d'autre :
    // la question n'est pas close, c'est la fin de la boucle qui la jugera.
    [[nodiscard]] HitQuality registerRhythmTap( double p_positionInBeats );

    // La boucle de reproduction est finie : le joueur a tape ce qu'il avait a taper.
    //
    // La tentative est JUSTE quand chaque frappe de la cellule a ete couverte ET qu'aucune frappe n'est tombee a cote.
    // Les deux, et pas seulement la premiere : sans la seconde, taper n'importe quand entre deux onsets serait gratuit.
    // Rater laisse la question posee - on la retente - et remet l'ardoise a zero, sinon une frappe oubliee une fois le
    // resterait pour toujours.
    bool endRhythmLoop();

    // Le joueur a nomme la couleur de l'accord, sur une question d'accord. Rend si c'etait juste.
    //
    // Une seule reponse possible, comme pour un intervalle : la question est une question, pas un questionnaire. Une
    // mauvaise reponse ne ferme pas la question - on retente, et l'accord est rejoue.
    bool answerChord( ChordQuality p_quality );

    // Repond a une question de DEGRADE : le second mode etait-il plus CLAIR que le premier ?
    //
    // Un booleen, et non une distance : la reponse n'est pas un ecart, c'est un SENS. Le domaine compare les deux modes
    // par leur RANG dans l'ordre de couleur, ce qui garantit qu'il ne peut pas se tromper de direction.
    bool answerModeColour( bool p_secondIsBrighter );

    // La meme question, avec la troisieme reponse : « pareil ».
    //
    // C'est ce que le bouton de Roger appelle, et c'est la seule facon de repondre juste quand les deux passages portent
    // la meme couleur - ce qui arrive quand la part le demande (voir SessionSettings::sameColourQuestionShare).
    bool answerModeColour( ModeColourAnswer p_answer );

    // Repond a une question de NOM : quel mode a ete joue ?
    bool answerModeName( Mode p_mode );

    // La reponse a une question de NOTE ETRANGERE : l'index du pas qui n'appartenait pas a la gamme.
    //
    // L'index, et non une classe de hauteur : les sept notes sont entendues DANS L'ORDRE, et le joueur designe la place
    // ou il a entendu l'intrus. Un nom de note marcherait aussi, mais il demanderait au joueur de nommer ce qu'il vient
    // d'entendre - un autre exercice, et pas celui-ci.
    bool answerForeignNote( std::int32_t p_stepIndex );

    // Les modes que le joueur a rencontres, dans l'ordre d'apprentissage.
    [[nodiscard]] std::span<const Mode> modePalette() const noexcept { return m_modePalette; }

    // Le LIVRE DES PHRASES, donne a la CONSTRUCTION et jamais apres.
    //
    // Il n'y a pas de setter, et ce n'est pas un oubli : la premiere question est construite par le constructeur lui-meme -
    // une session qui existe est une session qui demande quelque chose - donc un livre donne ensuite ne pourrait plus rien
    // pour elle. Un test l'a montre en echouant, et c'est exactement ce qu'un test doit faire.
    //
    // Il n'est pas dans SessionSettings non plus : un pointeur dans des reglages qui se copient serait un piege a
    // proprietaire, alors que sa place est evidemment au cote de ce qui construit les questions.

    // Ce que le joueur a repondu en dernier a une question de mode, pour que le verdict puisse le montrer.
    [[nodiscard]] std::optional<Mode> lastModeAnswer() const noexcept { return m_lastModeAnswer; }

    // Les qualites que le joueur a rencontrees, dans l'ordre d'apprentissage. C'est ce que sa palette d'accords
    // contient, et ce que l'ecran a le droit de proposer.
    [[nodiscard]] std::span<const ChordQuality> chordPalette() const noexcept { return m_chordPalette; }

    // The player gave up on this question and asked to see the answer. Worth nothing, and it costs
    // nothing: help is not a mistake.
    void revealAnswer();

    // Leaves the feedback and starts the next question, or finishes the session. Called by the screen,
    // which owns the pause.
    void advance();

    // The star of the session: only a session that was played to the end, with no revealed answer, and
    // mostly right on the first try.
    [[nodiscard]] bool hasEarnedStar() const noexcept;

private:
    // Builds the next question from the palette, the settings and the engine.
    [[nodiscard]] Question buildQuestion();

    // Whether the next question asks for a name or a direction, drawn from the settings.
    [[nodiscard]] QuestionKind drawKind();

    // Remplit la part rythmique d'une question : quelle cellule, a quel tempo, et l'ardoise des onsets a couvrir.
    void buildRhythmicCell( Question & p_question );

    // Remplit la part "accord" d'une question : quelle couleur, quelle tonique, et ce que le joueur peut repondre.
    // Construit une question d'accord : sa couleur, sa tonique, et les choix de la palette.
    void buildChordQuestion( Question & p_question );

    // Construit une question d'harmonie : la tonique du bourdon, le ou les modes, et les choix de la palette.
    //
    // p_compare dit laquelle des deux : deux modes a comparer, ou un seul a nommer. Une seule fonction, parce que les
    // deux questions partagent tout - la tonique, le bourdon, la palette, la facon de tirer un mode - et que deux
    // constructions separees finiraient par tirer differemment.
    void buildModeQuestion( Question & p_question, bool p_compare );

    [[nodiscard]] Mode drawMode();

    // La tonique du bourdon. Tiree dans la fenetre jouable, et choisie BASSE : c'est le bourdon qui la tient, et un
    // bourdon aigu n'a plus rien d'un bourdon.
    [[nodiscard]] Note drawModeTonic();

    // Construit un VAMP : la MEME gamme, posee sur deux centres differents.
    void buildVampQuestion( Question & p_question );

    // La question de la NOTE ETRANGERE : une gamme montee, une note par degre, et l'une d'elles est etrangere a la gamme.
    void buildForeignNoteQuestion( Question & p_question );

    void widenModePalette();

    // La question decidee pour ce rang, quand la session suit un plan.
    //
    // Rend un optional vide quand il n'y a pas de plan, ou quand le plan est epuise : la session reprend alors son
    // tirage, ce qui lui permet de finir une partie ordinaire sans rien savoir des bilans.
    [[nodiscard]] std::optional<QuestionTarget> plannedQuestionAt( std::size_t p_index ) const noexcept;

    // Une qualite d'accord de la palette, tiree au hasard.
    [[nodiscard]] ChordQuality drawChordQuality();

    // Une tonique qui laisse toute la place a l'accord, au-dessus d'elle.
    [[nodiscard]] std::int32_t drawChordRootMidiNumber( ChordQuality p_quality );

    // What a right or a wrong answer produces, whatever its form: the score moves, the palette widens or the grid
    // closes in, and the question passes to feedback or the session ends.
    bool resolveAnswer( bool p_isCorrect, std::optional<Interval> p_answer );

    // An interval of the palette, drawn evenly.
    [[nodiscard]] Interval drawTarget();

    // How the question is sounded: one way among ascending, descending and both at once.
    [[nodiscard]] IntervalDirection drawDirection();

    // A root note that leaves room for the interval, in the direction the question will be played.
    [[nodiscard]] std::int32_t drawRootMidiNumber( const Interval & p_target,
                                                   IntervalDirection p_direction );

    // Gives a new interval to the player, or takes the newest one back when they struggle.
    void widenPalette();
    void narrowPalette();

    // La meme chose pour les accords : une couleur nouvelle apres assez de reussites, et jamais moins que ce avec quoi
    // le joueur a commence. Un accord ne se RETIRE pas sur une erreur - le retirer laisserait parfois une palette
    // vide, et une question d'accord sans rien a repondre n'est plus une question.
    void widenChordPalette();

    std::mt19937 m_randomEngine;
    SessionSettings m_settings;
    std::vector<Interval> m_palette;

    // Les couleurs d'accord que le joueur a rencontrees. Un PREFIXE de chordLearningOrder(), elargi avec les
    // reussites - exactement comme la palette d'intervalles, et pour la meme raison : une couleur a la fois.
    std::vector<ChordQuality> m_chordPalette;

    // Les modes que le joueur a rencontres : un PREFIXE de modeLearningOrder(), elargi par les MEMES reussites que le
    // reste. Une seule progression a tenir, plutot que trois compteurs dont l'un finirait par mentir.
    std::vector<Mode> m_modePalette;

    // Le livre des phrases modales, s'il a ete donne. C'est lui qui fait entendre un mode en MELODIE plutot qu'en gamme.
    const PhraseBook * m_phraseBook{ nullptr };

    // La derniere reponse de mode, pour que le verdict puisse dire ce qui a ete repondu.
    std::optional<Mode> m_lastModeAnswer;
    SessionScore m_score;

    // Wrong answers in a row. Two of them, and the next question becomes a guided one - a smaller question the
    // player can still answer, which is help that does not announce itself.
    //
    // Declared BEFORE m_currentQuestion, and that order is not decorative: buildQuestion() reads it while it
    // initialises m_currentQuestion, so it must already exist.
    std::size_t m_consecutiveErrors{ 0 };

    // Le rang de la question en cours. Declare AVANT m_currentQuestion, et l'ordre n'est pas decoratif non plus :
    // buildQuestion() lit ce rang pour trouver la question PLANIFIEE, et il est appele pendant l'initialisation de
    // m_currentQuestion. Un membre lu avant d'etre initialise ne vaut pas zero : il vaut n'importe quoi.
    std::size_t m_questionNumber{ 1 };

    Question m_currentQuestion;
    SessionState m_state{ SessionState::Asking };
    bool m_lastAnswerWasCorrect{ false };
    std::optional<Interval> m_lastAnswer;

    // La derniere couleur d'accord nommee. Se remet a zero a chaque nouvelle question, comme la reponse d'intervalle :
    // un verdict qui survivrait a sa question serait un verdict qui parle d'autre chose.
    std::optional<ChordQuality> m_lastChordAnswer;
};

}    // namespace musichien::domain
