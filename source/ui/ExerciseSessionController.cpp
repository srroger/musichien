#include "ui/ExerciseSessionController.h"

#include "domain/exercise/Trophy.h"
#include "domain/exercise/Weekend.h"
#include "domain/music/ChordTree.h"
#include "domain/music/Interval.h"
#include "domain/music/Temperament.h"
#include "domain/rhythm/RhythmPattern.h"
#include "ui/IntervalDescription.h"
#include "ui/MicrophoneController.h"
#include "ui/ModeDescription.h"

#include <QColor>
#include <QCoreApplication>
#include <QDate>
#include <QDebug>
#include <QString>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <utility>

namespace musichien::ui
{

namespace
{

// What lives() returns when the session puts no limit on mistakes.
//
// A sentinel rather than zero, because zero lives IS a state - it means the session is over - and the
// screen has to be able to tell the two apart.
constexpr int NO_LIFE_LIMIT = -1;

// L'ecart entre deux notes d'un arpege. Assez lent pour que l'oreille entende chaque note, assez court pour qu'on
// reconnaisse encore l'accord d'ou elles viennent.
constexpr std::chrono::milliseconds ARPEGGIO_NOTE_GAP{ 450 };

// Le DEGRADE : les durees d'une question d'harmonie.
//
// Plus courtes que celles du banc d'essai, et c'est voulu : dans une session, une question doit tenir en quelques
// secondes - et deux modes, c'est deja le double d'une question ordinaire.
constexpr std::chrono::milliseconds MODE_NOTE_DURATION{ 340 };
constexpr std::chrono::milliseconds MODE_NOTE_GAP{ 40 };

// L'APERCU D'UN INSTRUMENT : la meme tonique que le banc d'essai des modes, pour que ce qu'on ecoute en reglant ses
// instruments soit dans la region ou ils sonneront en exercice.
constexpr std::int32_t PREVIEW_TONIC_MIDI_NUMBER = 62;

// Plus vif qu'une question, et c'est voulu : l'apercu n'est pas une devinette a resoudre, c'est un timbre a entendre.
// Toute la gamme - treize notes, montee et descendue - tient ainsi en moins de quatre secondes.
constexpr std::chrono::milliseconds PREVIEW_NOTE_DURATION{ 240 };
constexpr std::chrono::milliseconds PREVIEW_NOTE_GAP{ 50 };

// L'ecart entre deux pas d'une PHRASE, et le tempo de secours quand aucun reglage n'a ete lu.
//
// Les deux valeurs sont celles du banc d'essai des modes, et ce n'est pas une coincidence : c'est la MEME phrase, et elle
// doit sonner pareil dans les deux pages. Deux valeurs differentes feraient deux musiques differentes pour une seule
// phrase.
constexpr std::chrono::milliseconds PHRASE_NOTE_GAP{ 70 };
constexpr std::int32_t FALLBACK_PHRASE_TEMPO = 72;

// L'INDEX DU GODMODE dans la liste des difficultes : juste apres les cinq niveaux.
//
// Il n'appartient PAS a domain::PlayerLevel, et ce n'est pas un detail de rangement : un GodMode n'est pas une marche de
// plus. Il n'y a rien a « savoir » pour etre en GodMode, il y a un joueur qui decide de choisir lui-meme. L'ecran, lui,
// n'a qu'un combo a remplir, donc un seul nombre a comparer.
constexpr int GOD_MODE_DIFFICULTY_INDEX = static_cast<int>( domain::PLAYER_LEVEL_COUNT );

// LE NOM D'UN NIVEAU, ecrit UNE fois : il sert a la liste des difficultes ET a la felicitation de palier. Deux listes de
// noms finiraient par diverger, et c'est exactement ce que le projet evite partout ailleurs.
[[nodiscard]] QString levelName( domain::PlayerLevel p_level )
{
    switch( p_level )
    {
        case domain::PlayerLevel::Beginner:
            return ExerciseSessionController::tr( "Je débute" );

        case domain::PlayerLevel::Fluent:
            return ExerciseSessionController::tr( "À l'aise" );

        case domain::PlayerLevel::Advanced:
            return ExerciseSessionController::tr( "Jusqu'à l'octave" );

        case domain::PlayerLevel::BeyondTheOctave:
            return ExerciseSessionController::tr( "Les composés" );

        case domain::PlayerLevel::Master:
            return ExerciseSessionController::tr( "Je maîtrise" );
    }

    return {};
}

// Le bourdon sonne SEUL avant la melodie, et apres : c'est ce qui installe le centre avant que la couleur n'arrive.
constexpr std::chrono::milliseconds MODE_LEAD_IN{ 800 };
constexpr std::chrono::milliseconds MODE_TAIL{ 600 };

// Le silence entre les DEUX modes d'une question de couleur : assez long pour que l'oreille entende deux choses, assez
// court pour qu'elle les COMPARE - et comparer est tout l'exercice.
constexpr std::chrono::milliseconds MODE_COMPARISON_GAP{ 250 };

// La quinte du bourdon, en demi-tons : c'est ce qui fait qu'une note tenue devient un CENTRE.
constexpr std::int32_t FIFTH_IN_SEMITONES = 7;

// Deux octaves entre le bourdon et la melodie. Le bourdon tient les graves, et une melodie qui partagerait son octave se
// battrait avec lui au lieu de se poser dessus.
constexpr std::int32_t MODE_MELODY_OCTAVE_OFFSET = 24;

// Combien de questions FACILES ouvrent un bilan : assez pour se mettre en confiance, pas assez pour lasser.
constexpr std::size_t REVIEW_EASY_QUESTION_COUNT = 3;

// La periode qu'un bilan regarde. Trente jours : ce que le joueur a travaille recemment, et non sa vie entiere - un
// exercice rate il y a six mois n'est plus une faiblesse, c'est un souvenir.
constexpr int REVIEW_PERIOD_DAYS = 30;

// Combien de points forts - et de points faibles - la page d'ouverture du bilan MONTRE.
//
// TROIS de chaque cote : c'est ce qu'une page peut dire sans devenir une liste, et c'est assez pour que le joueur
// reconnaisse son profil en la lisant. Au-dela, il ne lit plus, il survole - et une page qu'on survole n'explique rien.
constexpr std::size_t REVIEW_OPENING_POINT_COUNT = 3;

// Le silence entre ce que le joueur a joue et la reponse, sur une question d'accord ratee.
//
// Assez long pour que les deux accords soient DEUX accords, assez court pour qu'ils restent une seule phrase. Un souffle,
// pas une pause : c'est ce qui separe « ce que j'ai cru » de « ce qui etait » sans les eloigner.
constexpr std::chrono::milliseconds WRONG_CHORD_GAP{ 240 };

// La qualite d'une frappe, dans la convention de l'ecran : 0 = Miss, 1 = Good, 2 = Perfect.
//
// La page Rythme parle deja cette langue, et la question de rythme doit parler la MEME : deux ecrans qui
// nommeraient differemment la meme chose obligeraient le QML a connaitre deux traductions pour un seul verdict.
[[nodiscard]] int screenQualityOf( domain::HitQuality p_quality ) noexcept
{
    switch( p_quality )
    {
        case domain::HitQuality::Perfect:
            return 2;

        case domain::HitQuality::Good:
            return 1;

        case domain::HitQuality::Miss:
            return 0;
    }

    return 0;
}

// La cellule d'une question, avec le meme garde-fou que le domaine : un index venu du domaine est valide, et une
// liste qui changerait un jour ne doit pas transformer une question en exception.
[[nodiscard]] const domain::RhythmPattern & patternOf( const domain::Question & p_question ) noexcept
{
    const std::vector<domain::RhythmPattern> & patterns = domain::allRhythmPatterns();

    const std::size_t index = std::min( p_question.patternIndex, patterns.size() - 1 );

    return patterns.at( index );
}

// Un accord, decrit pour l'ecran : sa couleur, son nom, son symbole ("Cm") et le nom de sa tonique.
//
// L'ecran AFFICHE, il n'assemble rien : un symbole compose dans le QML serait compose autrement le jour ou un deuxieme
// ecran le montrerait, et les deux divergeraient en silence.
// Le nom d'une couleur d'accord, EN FRANCAIS, pour la meme raison que les intervalles : le modele nomme, l'ecran traduit
// - et l'ordre suit ChordQuality, comme la table du domaine, pour qu'une couleur ajoutee ailleurs fasse echouer la
// compilation ici plutot que de glisser un nom faux dans un verdict.
//
// « sus4 » et « sus2 » ne sont pas traduits : ce sont les noms qu'un musicien lit sur une grille, en francais comme en
// anglais, et les franciser inventerait un vocabulaire que personne n'utilise.
constexpr std::array<const char *, domain::CHORD_QUALITY_COUNT> CHORD_QUALITY_WORDS{
  // LA MAJUSCULE, parce que ce sont des LIBELLES : un verdict affiche « Mineur », pas « mineur » au milieu d'une phrase.
  // C'est la meme regle que les intervalles, ou le nom ouvre toujours le libelle - « Quinte juste ».
  QT_TRANSLATE_NOOP( "ExerciseSessionController", "Majeur" ),
  QT_TRANSLATE_NOOP( "ExerciseSessionController", "Mineur" ),
  QT_TRANSLATE_NOOP( "ExerciseSessionController", "Sus4" ),
  QT_TRANSLATE_NOOP( "ExerciseSessionController", "Sus2" ),
  QT_TRANSLATE_NOOP( "ExerciseSessionController", "Diminué" ),
  QT_TRANSLATE_NOOP( "ExerciseSessionController", "Augmenté" ),
  QT_TRANSLATE_NOOP( "ExerciseSessionController", "Septième de dominante" ),
  QT_TRANSLATE_NOOP( "ExerciseSessionController", "Septième majeure" ),
  QT_TRANSLATE_NOOP( "ExerciseSessionController", "Septième mineure" ),
  QT_TRANSLATE_NOOP( "ExerciseSessionController", "Sixte" ),
  QT_TRANSLATE_NOOP( "ExerciseSessionController", "Demi-diminué" ),
  QT_TRANSLATE_NOOP( "ExerciseSessionController", "Septième diminuée" ),
  QT_TRANSLATE_NOOP( "ExerciseSessionController", "Mineur septième majeure" ),
  QT_TRANSLATE_NOOP( "ExerciseSessionController", "Ajoutée neuvième" ),
};

[[nodiscard]] QString chordQualityWord( domain::ChordQuality p_quality )
{
    const auto slot = static_cast<std::size_t>( p_quality );

    if( slot >= CHORD_QUALITY_WORDS.size() )
    {
        // Une couleur hors table n'existe pas, et un ecran doit pouvoir afficher quelque chose : le nom du modele vaut
        // mieux qu'une case vide, et cette branche ne doit jamais s'ouvrir.
        return QString::fromUtf8( domain::chordQualityName( p_quality ).data() );
    }

    return QCoreApplication::translate( "ExerciseSessionController", CHORD_QUALITY_WORDS.at( slot ) );
}

[[nodiscard]] QVariantMap describeChord( const domain::Chord & p_chord )
{
    const domain::Note root{ p_chord.rootMidiNumber };

    const std::string rootName = root.pitchClassName();

    const std::string_view suffix = domain::chordQualitySymbolSuffix( p_chord.quality );

    QVariantMap described;

    described.insert( QStringLiteral( "quality" ), static_cast<int>( p_chord.quality ) );

    described.insert( QStringLiteral( "name" ), chordQualityWord( p_chord.quality ) );

    described.insert( QStringLiteral( "rootName" ), QString::fromStdString( rootName ) );

    // La tonique et son suffixe, colles : "C" et "m" font "Cm", et "C" tout seul est deja un do majeur.
    described.insert( QStringLiteral( "symbol" ), QString::fromStdString( rootName + std::string{ suffix } ) );

    described.insert( QStringLiteral( "noteCount" ), static_cast<int>( domain::chordNoteCount( p_chord.quality ) ) );

    return described;
}

}    // namespace

ExerciseSessionController::ExerciseSessionController( domain::NotePlayer & p_notePlayer,
                                                      domain::SessionSettings p_settings,
                                                      domain::HintBook p_hintBook,
                                                      domain::AnecdoteBook p_anecdoteBook,
                                                      VibrationCallback p_vibrate,
                                                      domain::PlayerPreferences * p_levelStore,
                                                      QObject * p_parent )
  : QObject{ p_parent }
  , m_notePlayer{ p_notePlayer }
  , m_settings{ std::move( p_settings ) }
  , m_hintBook{ std::move( p_hintBook ) }
  , m_anecdoteBook{ std::move( p_anecdoteBook ) }
  , m_vibrate{ std::move( p_vibrate ) }
  , m_levelStore{ p_levelStore }
{
    // What the application remembers about the player is read once, here, and it OVERRIDES the settings the
    // caller passed: a level the player has chosen is a decision, and a decision beats a default.
    if( m_levelStore != nullptr )
    {
        m_playerLevel = m_levelStore->storedLevel();
    }

    if( m_playerLevel.has_value() )
    {
        m_settings = sessionSettingsForLevel( *m_playerLevel );
    }

    // Les parts de question, telles que le profil s'en souvient : elles priment sur les reglages passes au
    // constructeur, exactement comme le niveau a prime sur eux juste avant.
    applyStoredQuestionShares( m_settings );

    // The instruments the player asked for. An EMPTY list is a first run, and a first run sounds like a PIANO and a
    // GUITARE - see defaultEnabledInstruments for why those two and not the others.
    if( m_levelStore != nullptr )
    {
        m_enabledInstruments = m_levelStore->storedEnabledInstruments();
    }

    // LE GODMODE : la difficulte choisie, la palette qui JOUE, et le brouillon que la page montrera.
    //
    // La palette qui joue est celle qui a ete SAUVEGARDEE ; au premier lancement il n'y en a pas, et elle est alors celle du
    // NIVEAU courant. C'est la migration, et elle est invisible a dessein : un joueur qui choisit le GodMode sans avoir
    // jamais ouvert sa page doit pouvoir jouer, avec exactement le perimetre de son niveau. Sans cela, il tomberait sur une
    // configuration vide et une partie impossible a lancer.
    if( m_levelStore != nullptr )
    {
        m_godModeIsChosen = m_levelStore->storedGodModeIsChosen();

        const std::optional<domain::GodModePalette> saved = m_levelStore->storedGodModePalette();

        m_godModeIsSaved = saved.has_value();

        m_godModeSavedPalette = saved.has_value()
                                  ? *saved
                                  : domain::paletteForLevel( m_playerLevel.value_or( domain::PlayerLevel::Beginner ) );
    }
    else
    {
        m_godModeSavedPalette = domain::paletteForLevel( domain::PlayerLevel::Beginner );
    }

    // Le brouillon part de ce qui joue : tant que le joueur ne touche a rien, la page montre exactement ce qu'il jouera.
    m_godModeDraft = m_godModeSavedPalette;

    if( m_enabledInstruments.empty() )
    {
        m_enabledInstruments = domain::defaultEnabledInstruments();
    }

    // Une liste plus courte que ce que ce build connait veut dire « un instrument de plus depuis la derniere fois » :
    // les nouveaux arrivent ETEINTS, comme au premier lancement. Imposer un son que le joueur n'a pas demande serait la
    // seule facon de le surprendre desagreablement.
    m_enabledInstruments.resize( domain::INSTRUMENT_COUNT, false );

    // La boucle de rythme. Le timer le plus precis que Qt offre, comme la page Rythme : le clic doit tomber ou
    // l'oreille l'attend, et un timer grossier fait tituber toute une mesure.
    //
    // Et SINGLE SHOT, comme celui de la page Rythme : un timer repetitif repart de l'instant ou il a tire, donc chaque
    // temps joue un peu en retard decale tous les suivants, et la mesure part a la derive. Ici, chaque battement
    // re-arme le suivant depuis le debut de la mesure (voir scheduleNextRhythmBeat).
    m_rhythmTimer.setTimerType( Qt::PreciseTimer );
    m_rhythmTimer.setSingleShot( true );

    QObject::connect( &m_rhythmTimer, &QTimer::timeout, this, &ExerciseSessionController::onRhythmBeat );

    // Le SECOND mode d'une question de couleur : le minuteur le pose quand le premier a fini de sonner. Un minuteur a un
    // seul tir, comme celui de la repetition d'un accord.
    m_modeTimer.setSingleShot( true );

    QObject::connect( &m_modeTimer, &QTimer::timeout, this, [this]() {
        playModeQuestion( true );
    } );
}

bool ExerciseSessionController::running() const noexcept
{
    return m_session != nullptr;
}

int ExerciseSessionController::questionNumber() const noexcept
{
    return ( m_session != nullptr ) ? static_cast<int>( m_session->questionNumber() ) : 0;
}

int ExerciseSessionController::questionCount() const noexcept
{
    if( m_session == nullptr )
    {
        return 0;
    }

    // Le mode infini met le nombre de questions a la limite de size_t : le convertir en int le fait retomber sur
    // -1, que l'ecran afficherait tel quel. On le traduit en une sentinelle propre, que l'ecran lit comme "sans
    // fin".
    const std::size_t total = m_session->settings().questionCount;

    return ( total > static_cast<std::size_t>( std::numeric_limits<int>::max() ) ) ? -1 : static_cast<int>( total );
}

QVariantList ExerciseSessionController::gridPositions() const
{
    QVariantList positions;

    if( m_session == nullptr )
    {
        return positions;
    }

    // Les choix ont deja ete decides par la session : lesquels, et combien. Ici on ne fait que les PLACER, ce
    // qui est la seule chose qui manquait pour que la grille devienne une carte.
    const std::array<std::vector<domain::Interval>, domain::CIRCLE_OF_FIFTHS_SLOT_COUNT> layout =
      domain::layoutOnCircle( m_session->currentQuestion().choices );

    // UNE LISTE PLATE DE BOUTONS, et non douze places contenant chacune leurs intervalles.
    //
    // La difference n'est pas cosmetique : c'est ce qui rend l'ecran a la fois juste et simple. Une place porte
    // plusieurs intervalles - une seconde majeure et sa neuvieme, la meme couleur a une octave pres - et l'ecran
    // doit les dessiner empiles dans la case. Une liste imbriquee demandait deux Repeaters l'un dans l'autre, et
    // un seul Repeater ne produit rien en silence : la page se charge, aucun avertissement, aucun bouton.
    //
    // Aplati, chaque bouton sait tout ce qu'il lui faut pour se placer lui-meme :
    //
    //   * 'slot'       : 0 a 11, la place du cercle - c'est ce qui donne l'ANGLE ;
    //   * 'stackIndex' : son rang dans la case, 0 en haut ;
    //   * 'stackSize'  : combien de boutons la case porte, pour savoir comment les repartir ;
    //   * 'isEmpty'    : une place que la palette n'a pas encore, dessinee en anneau.
    for( std::size_t slot = 0; slot < layout.size(); ++slot )
    {
        const std::vector<domain::Interval> & intervals = layout.at( slot );

        if( intervals.empty() )
        {
            QVariantMap position;
            position.insert( QStringLiteral( "slot" ), static_cast<int>( slot ) );
            position.insert( QStringLiteral( "stackIndex" ), 0 );
            position.insert( QStringLiteral( "stackSize" ), 1 );
            position.insert( QStringLiteral( "isEmpty" ), true );

            positions.append( position );

            continue;
        }

        // Du plus petit au plus grand, donc : le simple en tete de case, ses composes colles juste dessous.
        for( std::size_t rank = 0; rank < intervals.size(); ++rank )
        {
            QVariantMap position = describeInterval( intervals.at( rank ) );

            position.insert( QStringLiteral( "slot" ), static_cast<int>( slot ) );
            position.insert( QStringLiteral( "stackIndex" ), static_cast<int>( rank ) );
            position.insert( QStringLiteral( "stackSize" ), static_cast<int>( intervals.size() ) );
            position.insert( QStringLiteral( "isEmpty" ), false );

            positions.append( position );
        }
    }

    return positions;
}

QVariantList ExerciseSessionController::choices() const
{
    return m_choices;
}

bool ExerciseSessionController::isAsking() const noexcept
{
    return ( m_session != nullptr ) && ( m_session->state() == domain::SessionState::Asking );
}

bool ExerciseSessionController::wasSessionWon() const noexcept
{
    // Gagnee = arrivee au bout, et c'est tout : c'est le score qui dira comment. Une partie finie avec une seule vie est
    // une partie gagnee - et si l'on demandait « sans faute », il n'y aurait presque jamais de chien content.
    return isFinished() && ( hasUnlimitedLives() || ( lives() > 0 ) );
}

void ExerciseSessionController::resetPreferences()
{
    // L'instrument : ceux d'un PREMIER lancement, et pas tous ceux que ce build connait. Roger l'a vu tout de suite -
    // « les valeurs par defaut sont mauvaises, ce ne sont pas les valeurs qu'on a quand on installe l'application ». Un
    // profil neuf sonne du piano et de la guitare ; le saxo et les formes d'onde sont OFFERTS, jamais imposes.
    const std::vector<bool> defaultInstruments = domain::defaultEnabledInstruments();

    for( std::size_t index = 0; index < domain::INSTRUMENT_COUNT; ++index )
    {
        const bool isEnabled = std::cmp_less( index, defaultInstruments.size() ) ? defaultInstruments.at( index ) : true;

        setInstrumentEnabled( static_cast<int>( index ), isEnabled );
    }

    // Les poids : ceux du DOMAINE, et pas une liste recopiee ici. Le jour ou un defaut change, il change a un seul
    // endroit - et c'est celui qui fait foi.
    const domain::SessionSettings defaults;

    setNamedIntervalQuestionShare( defaults.namedIntervalQuestionShare );
    setSingQuestionShare( defaults.singQuestionShare );
    setChordQuestionShare( defaults.chordQuestionShare );
    setModeColourQuestionShare( defaults.modeColourQuestionShare );
    setModeNameQuestionShare( defaults.modeNameQuestionShare );
    setModeVampQuestionShare( defaults.modeVampQuestionShare );
    setForeignNoteQuestionShare( defaults.foreignNoteQuestionShare );

    // Le tempo des phrases, l'accordage, et le rappel.
    setPhraseTempoBpm( 72 );
    setPhraseTempoVariation( 20 );
    setTemperament( 0 );
    setReferencePitch( 440.0 );

    // Les COEURS D'ARCADE reviennent a DIX : c'est leur valeur a l'installation, et « Par defaut » promet exactement cela.
    setArcadeLives( 10 );

    // LE RAPPEL REVIENT ACTIF, et c'est une CORRECTION : c'est son etat au premier lancement - voir
    // QSettingsPlayerPreferences::dailyReminderEnabled, qui lit « vrai » quand la cle n'existe pas encore. Roger l'a vu le
    // jour ou il a appuye sur « Par defaut » : « c'est une option qui est choisie par defaut a l'installation, il faudrait
    // qu'elle soit aussi activee si on appuie sur par defaut ».
    //
    // Un bouton qui rend un etat DIFFERENT de celui d'une installation neuve n'est pas un bouton « par defaut », c'est un
    // bouton « autre chose ».
    setDailyReminderEnabled( true );

    // Et le NIVEAU reste : c'est un choix que le joueur fait sur lui-meme, pas un reglage qu'on remet a zero. Le nom
    // aussi : c'est le sien.
}

void ExerciseSessionController::pickChibaImage()
{
    // QUATRE humeurs, et le tirage se fait ICI : Roger a livre trois planches de plus, et il veut les voir tourner -
    // « en gros il faudrait que les 4 tournent aleatoirement, histoire de varier ». L'ecran ne choisit rien : il affiche
    // la ressource que le controleur lui donne, comme pour tout le reste.
    static const std::array<const char *, 4> CHIBA_IMAGES{ "qrc:/assets/images/chibaSpeak.png",
                                                           "qrc:/assets/images/chibaSpeak2.png",
                                                           "qrc:/assets/images/chibaSpeak3.png",
                                                           "qrc:/assets/images/chibaSpeak4.png" };

    std::random_device entropySource;

    std::uniform_int_distribution<std::size_t> draw{ 0, CHIBA_IMAGES.size() - 1 };

    m_chibaImageSource = QString::fromLatin1( CHIBA_IMAGES.at( draw( entropySource ) ) );
}

void ExerciseSessionController::playScoreTick( int p_progressPercent )
{
    m_notePlayer.playScoreTick( p_progressPercent );
}

void ExerciseSessionController::playVictoryFanfare()
{
    m_notePlayer.playVictoryFanfare();
}

void ExerciseSessionController::tellAnotherAnecdote()
{
    // Le chien de l'accueil : il raconte quand on lui demande. C'est le meme chemin que la fin de partie - une anecdote
    // fraiche, et la popup qui va avec - mais c'est le joueur qui le declenche, en appuyant sur le chien lui-meme.
    //
    // La nouvelle anecdote est tiree AVANT d'ouvrir la popup : sans ca, elle s'ouvrirait sur l'anecdote precedente, et
    // « raconte-moi autre chose » aurait l'air de ne rien faire.
    refreshAnecdote();

    // ET UNE HUMEUR AU HASARD : quatre planches, et le chien change de tete a chaque fois qu'il ouvre la bouche - Roger
    // les a dessinees pour ca, « histoire de varier ».
    pickChibaImage();

    // Et il aboie de nouveau : c'est une nouvelle phrase, donc une nouvelle voix.
    m_notePlayer.playDogBark();

    if( m_isChibaTalking )
    {
        return;
    }

    m_isChibaTalking = true;

    emit chibaTalkingChanged();
}

void ExerciseSessionController::dismissChiba()
{
    if( !m_isChibaTalking )
    {
        return;
    }

    m_isChibaTalking = false;

    emit chibaTalkingChanged();
}

bool ExerciseSessionController::isFeedbackVisible() const noexcept
{
    return ( m_session != nullptr ) && ( m_session->state() == domain::SessionState::Feedback );
}

bool ExerciseSessionController::wasLastAnswerCorrect() const noexcept
{
    return ( m_session != nullptr ) && m_session->wasLastAnswerCorrect();
}

bool ExerciseSessionController::isHelpAvailable() const noexcept
{
    return ( m_session != nullptr ) && m_session->isHelpAvailable();
}

QVariantMap ExerciseSessionController::heardInterval() const
{
    // Sur une question qui ne PARLE pas d'intervalle, il n'y a pas d'intervalle a decrire : la question en porte bien un
    // - c'est l'ordre des tirages qui veut ca - mais il n'a jamais ete joue, et le montrer serait un mensonge.
    if( ( m_session == nullptr ) || !domain::isIntervalQuestion( m_session->currentQuestion().kind ) )
    {
        return QVariantMap{};
    }

    return describeInterval( m_session->currentQuestion().target );
}

QVariantMap ExerciseSessionController::answeredInterval() const
{
    if( m_session == nullptr )
    {
        return QVariantMap{};
    }

    const std::optional<domain::Interval> answer = m_session->lastAnswer();

    // operator* rather than value(): an empty optional is what "nothing to show" means here, and it is
    // already handled.
    return answer.has_value() ? describeInterval( *answer ) : QVariantMap{};
}

QString ExerciseSessionController::hintText() const
{
    // Pas d'indice sur une question qui ne parle pas d'INTERVALLES : les indices sont des souvenirs d'intervalles
    // (« pense a Star Wars »), et le contenu n'en a ni pour une cellule, ni pour une couleur d'accord, ni pour un mode.
    //
    // C'est ICI que Roger a vu le defaut : « pour les bourdons quand je fail, je vois l'indice des intervalles
    // apparaitre ». Le test est demande au domaine, et non reecrit : c'est une liste recopiee qui l'avait oublie.
    if( ( m_session == nullptr ) || !m_session->isHintAvailable()
        || !domain::isIntervalQuestion( m_session->currentQuestion().kind ) )
    {
        return QString{};
    }

    const domain::Question & question = m_session->currentQuestion();

    // Looked up for the interval that was ASKED, in the direction it was played: a hint is a fact about
    // the question, never about the answer, and never about the button the player pressed.
    const std::optional<domain::IntervalHint> hint = m_hintBook.hintFor( question.target,
                                                                         question.direction );

    if( !hint.has_value() )
    {
        // A gap in the content file - the seventh descending has no hint yet. Nothing is reported to
        // the player: a missing hint is simply absent, not broken.
        return QString{};
    }

    return QString::fromStdString( hint->label );
}

bool ExerciseSessionController::hasChosenLevel() const noexcept
{
    return m_playerLevel.has_value();
}

int ExerciseSessionController::playerLevel() const noexcept
{
    // LE GODMODE EST LA SIXIEME ENTREE, et il n'est PAS un niveau : le combo de la page de garde offre six difficultes, et
    // la derniere dit « choisis toi-meme ». C'est ce nombre que l'ecran compare pour savoir quoi afficher, donc il doit
    // dire les deux choses - le niveau, ou le GodMode.
    if( m_godModeIsChosen )
    {
        return GOD_MODE_DIFFICULTY_INDEX;
    }

    return m_playerLevel.has_value() ? static_cast<int>( *m_playerLevel ) : -1;
}

void ExerciseSessionController::playTapCue()
{
    // Un simple relais vers le port : le controleur ne fabrique pas de son, il demande. L'ecran, lui, sait QUEL bouton
    // ne fait rien d'autre que naviguer.
    m_notePlayer.playTapCue();
}

void ExerciseSessionController::choosePlayerLevel( int p_level )
{
    // Le bouton a repondu : un clic tres court et discret, pour que la main soit entendue.
    m_notePlayer.playTapCue();

    // LE GODMODE, quand c'est lui qu'on choisit.
    //
    // Il ne touche PAS au niveau memorise : le joueur reste « jusqu'a l'octave » sous son GodMode, et le jour ou il
    // revient en arriere il retrouve exactement ou il en etait. C'est ce que Roger a demande - « si le joueur veut revenir
    // en arriere » - et cela veut dire que les deux choix vivent cote a cote, chacun dans sa case.
    if( p_level == GOD_MODE_DIFFICULTY_INDEX )
    {
        const bool changed = !m_godModeIsChosen;

        m_godModeIsChosen = true;

        if( m_levelStore != nullptr )
        {
            m_levelStore->storeGodModeIsChosen( true );
        }

        if( changed )
        {
            emit godModeChanged();
        }

        // LE CODE DE DEVELOPPEUR : sept choix de GodMode d'affilee, et toutes les difficultes s'ouvrent. Voir
        // noteGodModeSelection.
        noteGodModeSelection();

        emit playerLevelChanged();

        return;
    }

    // Et revenir a un niveau REMET le drapeau a faux : c'est la seule facon de sortir du GodMode sans effacer son profil,
    // et Roger a voulu que le profil se remette a zero SEPAREMENT, avec une confirmation.
    if( m_godModeIsChosen )
    {
        m_godModeIsChosen = false;

        if( m_levelStore != nullptr )
        {
            m_levelStore->storeGodModeIsChosen( false );
        }

        emit godModeChanged();
    }

    // Choisir un AUTRE niveau casse la suite : le code de developpeur demande SEPT GodMode d'affilee, et rien entre les
    // deux. C'est ce que « d'affilee » veut dire, et sans cette remise a zero un joueur pourrait alterner sans le savoir.
    m_godModeSelectionCount = 0;

    // Et le deverrouillage du code de developpeur est TEMPORAIRE : choisir une vraie difficulte le referme. C'est ce que
    // Roger a demande - « il faudrait que ce déverrouillage soit temporaire ». Rouvrir l'application le referme aussi, pour
    // la meme raison : rien de tout ceci ne s'ecrit sur le disque.
    if( m_allLevelsUnlocked )
    {
        m_allLevelsUnlocked = false;

        emit playerLevelChanged();
    }

    const domain::PlayerLevel level = domain::playerLevelFromIndex( static_cast<std::size_t>( p_level ) );

    m_playerLevel = level;

    // The rules of the session to COME. A session already running is deliberately left alone: changing the
    // difficulty under the feet of a player in the middle of ten questions is not a kindness.
    m_settings = sessionSettingsForLevel( level );

    applyStoredQuestionShares( m_settings );

    if( m_levelStore != nullptr )
    {
        m_levelStore->storeLevel( level );
    }

    // Et CHANGER DE NIVEAU PRE-REMPLIT le GodMode, tant que le joueur n'a jamais sauvegarde le sien.
    //
    // C'est exactement ce que Roger a decrit : « le combo a juste pour effet de lui pre-remplir ces checkbox ». Une palette
    // sauvegardee, en revanche, ne bouge plus : elle appartient au joueur, et non a son niveau - sinon le dieu verrait ses
    // cases se decocher parce qu'il a change de marche.
    if( !m_godModeIsSaved )
    {
        m_godModeSavedPalette = domain::paletteForLevel( level );
        m_godModeDraft = m_godModeSavedPalette;

        emit godModeChanged();
    }

    emit playerLevelChanged();

    // Le palier a change : la fleche doree peut devoir disparaitre, ou apparaitre.
    emit levelInvitationChanged();
}

bool ExerciseSessionController::isLevelUnlocked( int p_index ) const
{
    // LE PREMIER NIVEAU EST TOUJOURS OUVERT : il n'y a rien a meriter pour commencer, et un jeu qui ferme sa porte d'entree
    // n'est plus un jeu.
    if( p_index <= 0 )
    {
        return true;
    }

    // Le code de developpeur, quand il a ete entre.
    if( m_allLevelsUnlocked )
    {
        return true;
    }

    // Sinon : un palier s'ouvre quand l'EXPERIENCE du joueur le merite - la meme regle qui propose la montee, appliquee au
    // choix. Un palier se ferme donc tant qu'on ne l'a pas gagne, et c'est ce qui donne au GodMode son sens de passe-droit.
    const auto level = domain::levelEarnedBy( totalExperience() );

    return p_index <= static_cast<int>( level );
}

void ExerciseSessionController::noteGodModeSelection()
{
    ++m_godModeSelectionCount;

    // SEPT d'affilee : le raccourci de developpeur. Au-dela, le compteur reste haut sans rien changer - ce qui compte est
    // qu'il n'ait jamais ete interrompu par un autre niveau (voir choosePlayerLevel, qui le remet a zero).
    constexpr std::size_t GOD_MODE_SELECTIONS_TO_UNLOCK = 7;

    if( !m_allLevelsUnlocked && ( m_godModeSelectionCount >= GOD_MODE_SELECTIONS_TO_UNLOCK ) )
    {
        m_allLevelsUnlocked = true;

        // La liste des difficultes vient de changer : l'ecran doit redessiner ses entrees.
        emit playerLevelChanged();
    }
}

domain::SessionSettings ExerciseSessionController::sessionSettingsForLevel( domain::PlayerLevel p_level ) const
{
    domain::SessionSettings settings = domain::sessionSettingsFor( p_level );

    // Les parts de question sont des reglages du JOUEUR, pas des consequences de sa difficulte : elles survivent au
    // changement de niveau. C'est une correction plutot qu'un detail - sans elle, quelqu'un qui avait demande des
    // questions de rythme sur mesure les perdait en changeant de niveau, et la session recommencait a en poser a sa
    // place.
    //
    // Ce que le PROFIL en dit, lui, est applique juste apres, par applyStoredQuestionShares : c'est la que ces
    // reglages sont ranges, et le profil fait foi.
    settings.namedIntervalQuestionShare = m_settings.namedIntervalQuestionShare;
    settings.foreignNoteQuestionShare = m_settings.foreignNoteQuestionShare;
    settings.singQuestionShare = m_settings.singQuestionShare;
    settings.chordQuestionShare = m_settings.chordQuestionShare;

    return settings;
}

void ExerciseSessionController::applyStoredQuestionShares( domain::SessionSettings & p_settings ) const
{
    if( m_levelStore == nullptr )
    {
        return;
    }

    p_settings.namedIntervalQuestionShare = m_levelStore->storedNamedIntervalQuestionShare();
    p_settings.foreignNoteQuestionShare = m_levelStore->storedForeignNoteQuestionShare();
    p_settings.singQuestionShare = m_levelStore->storedSingQuestionShare();
    p_settings.chordQuestionShare = m_levelStore->storedChordQuestionShare();

    // L'harmonie se pose avec les autres, et vaut zero tant que le joueur ne l'a pas demandee : une session ordinaire
    // reste donc une session d'intervalles, exactement comme avant que ce pilier existe.
    p_settings.modeColourQuestionShare = m_levelStore->storedModeColourQuestionShare();
    p_settings.modeNameQuestionShare = m_levelStore->storedModeNameQuestionShare();
    p_settings.modeVampQuestionShare = m_levelStore->storedModeVampQuestionShare();
}

QVariantList ExerciseSessionController::playerLevels() const
{
    QVariantList levels;

    for( std::size_t index = 0; index < domain::PLAYER_LEVEL_COUNT; ++index )
    {
        QVariantMap level;
        level.insert( QStringLiteral( "index" ), static_cast<int>( index ) );
        level.insert( QStringLiteral( "name" ), levelName( domain::playerLevelFromIndex( index ) ) );
        level.insert( QStringLiteral( "isGodMode" ), false );

        // FERME tant que l'experience ne l'ouvre pas : voir isLevelUnlocked. L'ecran s'en sert pour barrer l'entree au lieu
        // de la laisser cliquer dans le vide.
        level.insert( QStringLiteral( "isLocked" ), !isLevelUnlocked( static_cast<int>( index ) ) );

        levels.append( level );
    }

    // ET LE GODMODE, EN DERNIER.
    //
    // Il n'est pas une sixieme marche : il est la porte qui sort de l'echelle. Le nom se suffit a lui-meme, et le drapeau
    // permet a l'ecran de le traiter autrement - le montrer en majuscules, ou l'annoncer comme un choix qui casse le jeu.
    QVariantMap godMode;
    godMode.insert( QStringLiteral( "index" ), GOD_MODE_DIFFICULTY_INDEX );
    godMode.insert( QStringLiteral( "name" ), ExerciseSessionController::tr( "GodMode" ) );
    godMode.insert( QStringLiteral( "isGodMode" ), true );

    levels.append( godMode );

    return levels;
}

QVariantList ExerciseSessionController::instruments() const
{
    QVariantList instruments;

    for( std::size_t index = 0; index < domain::INSTRUMENT_COUNT; ++index )
    {
        QVariantMap instrument;
        instrument.insert( QStringLiteral( "index" ), static_cast<int>( index ) );

        // Le NOM, avec sa majuscule : c'est un libelle d'ecran, et Roger l'a demande tel quel - « et les ecrire propre
        // au lieu de mot sans majuscule ». La capitale est posee ICI, dans la couche d'affichage, et jamais dans le
        // domaine : les noms du domaine servent aussi a retrouver les enregistrements, et un identifiant ne se decore pas.
        QString name = QString::fromUtf8( domain::INSTRUMENT_NAMES.at( index ) );

        if( !name.isEmpty() )
        {
            name[0] = name.at( 0 ).toUpper();
        }

        instrument.insert( QStringLiteral( "name" ), name );

        // A list shorter than the instruments this build knows about means "everything": a first run must sound
        // complete, not empty.
        instrument.insert( QStringLiteral( "enabled" ),
                           std::cmp_less( index, m_enabledInstruments.size() ) ? m_enabledInstruments.at( index ) : true );

        instruments.append( instrument );
    }

    return instruments;
}

void ExerciseSessionController::setInstrumentEnabled( int p_index, bool p_isEnabled )
{
    // Le bouton a repondu : un clic tres court et discret, pour que la main soit entendue.
    m_notePlayer.playTapCue();

    // Two tests rather than one: comparing a signed index with an unsigned count in the same
    // expression is exactly the kind of comparison that lets a negative index through.
    if( p_index < 0 )
    {
        return;
    }

    if( std::cmp_greater_equal( p_index, domain::INSTRUMENT_COUNT ) )
    {
        return;
    }

    m_enabledInstruments.resize( domain::INSTRUMENT_COUNT, true );

    m_enabledInstruments.at( static_cast<std::size_t>( p_index ) ) = p_isEnabled;

    // The LAST one cannot be switched off: an instrument list with nothing in it is a game with no sound. The
    // signal is emitted all the same, so that a checkbox which refused to change snaps back where it belongs
    // instead of lying about the state.
    const bool anythingLeft = std::ranges::any_of( m_enabledInstruments, []( bool p_isEnabledFlag ) {
        return p_isEnabledFlag;
    } );

    if( anythingLeft )
    {
        if( m_levelStore != nullptr )
        {
            m_levelStore->storeEnabledInstruments( m_enabledInstruments );
        }
    }
    else
    {
        m_enabledInstruments.at( static_cast<std::size_t>( p_index ) ) = true;
    }

    emit instrumentsChanged();
}

void ExerciseSessionController::previewInstrument( int p_index )
{
    // Le bouton a repondu : le meme clic discret que la case a cocher, pour que la main soit entendue.
    m_notePlayer.playTapCue();

    // Deux tests plutot qu'un, comme pour la case a cocher : comparer un index signe et un compte non signe dans la
    // meme expression est exactement ce qui laisse passer un index negatif.
    if( p_index < 0 )
    {
        return;
    }

    if( std::cmp_greater_equal( p_index, domain::INSTRUMENT_COUNT ) )
    {
        return;
    }

    // LE PHRYGIEN, et l'accord qui le signe sont demandes au DOMAINE : ils sont de la musique, pas de la restitution, et
    // le banc d'essai des modes fait le meme aller-retour pour la meme raison.
    const domain::Note tonic{ PREVIEW_TONIC_MIDI_NUMBER };

    const std::vector<domain::Note> scale = domain::modeScaleUpAndDown( tonic, domain::Mode::Phrygian );

    const std::vector<domain::Note> chord = domain::phrygianSignatureChord( tonic );

    // L'INDEX passe tel quel : c'est celui du domaine, et l'adaptateur choisit avec quel son il le joue - la synthese
    // restant le repli pour un appareil sans echantillons.
    m_notePlayer.playInstrumentPreview( scale,
                                        chord,
                                        static_cast<std::size_t>( p_index ),
                                        PREVIEW_NOTE_DURATION,
                                        PREVIEW_NOTE_GAP );
}

int ExerciseSessionController::experience() const noexcept
{
    return ( m_session != nullptr ) ? static_cast<int>( m_session->score().experience() ) : 0;
}

int ExerciseSessionController::streak() const noexcept
{
    return ( m_session != nullptr ) ? static_cast<int>( m_session->score().streak() ) : 0;
}

int ExerciseSessionController::lives() const noexcept
{
    if( m_session == nullptr )
    {
        return NO_LIFE_LIMIT;
    }

    // operator* and not value(): value() throws when the optional is empty, and a getter of a view
    // model has no business being able to throw.
    const std::optional<std::int32_t> remainingLives = m_session->score().remainingLives();

    return remainingLives.has_value() ? static_cast<int>( *remainingLives ) : NO_LIFE_LIMIT;
}

bool ExerciseSessionController::hasUnlimitedLives() const noexcept
{
    return ( m_session == nullptr ) || !m_session->score().remainingLives().has_value();
}

bool ExerciseSessionController::starEarned() const noexcept
{
    return ( m_session != nullptr ) && m_session->hasEarnedStar();
}

int ExerciseSessionController::sessionLongestStreak() const noexcept
{
    return ( m_session != nullptr ) ? static_cast<int>( m_session->score().longestStreak() ) : 0;
}

QVariantList ExerciseSessionController::familyResults() const
{
    QVariantList results;

    if( m_session == nullptr )
    {
        return results;
    }

    // L'ORDRE des familles est celui du jeu, et il ne bouge pas : intervalles, accords, modes. Un ecran de fin qui
    // rearrangerait les lignes d'une partie a l'autre ne se lirait pas.
    constexpr std::array<domain::QuestionFamily, domain::QUESTION_FAMILY_COUNT> FAMILIES{
      domain::QuestionFamily::Interval, domain::QuestionFamily::Chord, domain::QuestionFamily::Mode };

    constexpr std::array<std::string_view, domain::QUESTION_FAMILY_COUNT> FAMILY_NAMES{ "Intervalles", "Accords", "Modes" };

    const domain::FamilyTally & tally = m_session->familyTally();

    for( std::size_t index = 0; index < domain::QUESTION_FAMILY_COUNT; ++index )
    {
        const domain::QuestionFamily family = FAMILIES.at( index );

        // Une famille a laquelle on n'a PAS joue est exclue : « 0 sur 0 » n'informe pas, et une Arcade qui n'a pas atteint
        // les modes ne doit pas afficher une ligne vide a leur sujet.
        if( tally.askedIn( family ) == 0 )
        {
            continue;
        }

        QVariantMap result;
        result.insert( QStringLiteral( "name" ), QString::fromUtf8( FAMILY_NAMES.at( index ).data() ) );
        result.insert( QStringLiteral( "asked" ), static_cast<int>( tally.askedIn( family ) ) );
        result.insert( QStringLiteral( "correct" ), static_cast<int>( tally.correctIn( family ) ) );
        result.insert( QStringLiteral( "percent" ), static_cast<int>( tally.successPercentIn( family ) ) );

        results.append( result );
    }

    return results;
}

int ExerciseSessionController::arcadeLives() const
{
    return ( m_levelStore != nullptr ) ? static_cast<int>( m_levelStore->storedArcadeLives() ) : 10;
}

void ExerciseSessionController::setArcadeLives( int p_lives )
{
    if( m_levelStore == nullptr )
    {
        return;
    }

    m_levelStore->storeArcadeLives( std::clamp( p_lives, 1, 25 ) );

    // Le bouton a repondu : un clic court, pour que la main soit entendue - comme tous les reglages.
    m_notePlayer.playTapCue();

    emit arcadeLivesChanged();
}

domain::BilanRecord ExerciseSessionController::bilanRecord() const
{
    domain::BilanRecord record;

    if( m_levelStore == nullptr )
    {
        return record;
    }

    record.bilanCount = m_levelStore->storedBilanCount();
    record.perfectBilanCount = m_levelStore->storedPerfectBilanCount();
    record.successStreak = m_levelStore->storedBilanSuccessStreak();
    record.longestSuccessStreak = m_levelStore->storedLongestBilanSuccessStreak();

    return record;
}

void ExerciseSessionController::recordBilanOutcome()
{
    if( ( m_levelStore == nullptr ) || ( m_session == nullptr ) || ( m_gameMode != domain::GameMode::Review ) )
    {
        return;
    }

    // L'ETAT AVANT, garde pour pouvoir dire ce qui vient d'etre GAGNE : sans lui, on saurait seulement ce que le joueur
    // possede, et jamais ce que ce bilan lui a apporte.
    const domain::BilanRecord before = bilanRecord();

    const domain::SessionScore & score = m_session->score();

    // UN BILAN REUSSI : il a merite son etoile, c'est-a-dire qu'il a trouve l'essentiel du premier coup sans se faire
    // souffler une reponse. C'est la meme regle que partout ailleurs, donc rien de nouveau a tenir.
    const bool wasSuccessful = m_session->hasEarnedStar();

    // UN BILAN PARFAIT : aucune reponse revelee, ET tout trouve du premier coup. Le plus dur des deux, et le seul qui
    // merite un trophee a lui.
    const bool wasPerfect = ( score.helpedQuestionCount() == 0 )
                            && ( score.firstTrySuccessCount() == score.completedQuestionCount() )
                            && ( score.completedQuestionCount() > 0 );

    std::int64_t streak = m_levelStore->storedBilanSuccessStreak();

    if( wasSuccessful )
    {
        ++streak;
    }
    else
    {
        // Une suite CASSEE, et pas seulement non augmentee : c'est ce que « d'affilee » veut dire.
        streak = 0;
    }

    m_levelStore->storeBilanSuccessStreak( streak );
    m_levelStore->storeLongestBilanSuccessStreak(
      std::max( m_levelStore->storedLongestBilanSuccessStreak(), streak ) );

    m_levelStore->storeBilanCount( m_levelStore->storedBilanCount() + 1 );

    if( wasPerfect )
    {
        m_levelStore->storePerfectBilanCount( m_levelStore->storedPerfectBilanCount() + 1 );
    }

    // ET CE QUE CE BILAN A RAPPORTE : les trophees qui ne l'etaient pas AVANT, et le titre s'il a monte.
    const domain::BilanRecord after = bilanRecord();

    for( const domain::Trophy & trophy : domain::trophiesFor( after ) )
    {
        if( !trophy.earned )
        {
            continue;
        }

        const auto wasAlreadyEarned = [&before]( std::string_view p_identifier ) {
            for( const domain::Trophy & previous : domain::trophiesFor( before ) )
            {
                if( previous.identifier == p_identifier )
                {
                    return previous.earned;
                }
            }

            return false;
        };

        if( wasAlreadyEarned( trophy.identifier ) )
        {
            continue;
        }

        QVariantMap entry;
        entry.insert( QStringLiteral( "name" ), QString::fromUtf8( trophy.name.data() ) );
        entry.insert( QStringLiteral( "description" ), QString::fromUtf8( trophy.description.data() ) );

        m_newlyEarnedTrophies.append( entry );
    }

    m_titleJustIncreased = domain::titleEarnedBy( after ) > domain::titleEarnedBy( before );

    // Et l'ecran l'apprend ICI : tout ce que ce bilan vient de rapporter est pret, donc le badge peut se montrer - ou
    // rester cache, ce qui est le cas ordinaire.
    emit playerProgressChanged();
}

QVariantMap ExerciseSessionController::playerTitle() const
{
    const domain::Title title = domain::titleEarnedBy( bilanRecord() );

    QVariantMap described;
    described.insert( QStringLiteral( "name" ), QString::fromUtf8( domain::titleName( title ).data() ) );
    described.insert( QStringLiteral( "motto" ), QString::fromUtf8( domain::titleMotto( title ).data() ) );
    described.insert( QStringLiteral( "index" ), static_cast<int>( title ) );

    return described;
}

QVariantList ExerciseSessionController::trophies() const
{
    QVariantList described;

    for( const domain::Trophy & trophy : domain::trophiesFor( bilanRecord() ) )
    {
        QVariantMap entry;
        entry.insert( QStringLiteral( "identifier" ), QString::fromUtf8( trophy.identifier.data() ) );
        entry.insert( QStringLiteral( "name" ), QString::fromUtf8( trophy.name.data() ) );
        entry.insert( QStringLiteral( "description" ), QString::fromUtf8( trophy.description.data() ) );
        entry.insert( QStringLiteral( "earned" ), trophy.earned );

        described.append( entry );
    }

    return described;
}

QVariantList ExerciseSessionController::allTitles() const
{
    // Le titre PORTE est le plus haut merite ; tous ceux d'en dessous sont acquis, tous ceux au-dessus sont un objectif.
    const domain::Title earned = domain::titleEarnedBy( bilanRecord() );

    QVariantList described;

    for( std::size_t index = 0; index < domain::TITLE_COUNT; ++index )
    {
        const auto title = static_cast<domain::Title>( index );

        QVariantMap entry;
        entry.insert( QStringLiteral( "name" ), QString::fromUtf8( domain::titleName( title ).data() ) );
        entry.insert( QStringLiteral( "motto" ), QString::fromUtf8( domain::titleMotto( title ).data() ) );
        entry.insert( QStringLiteral( "earned" ), index <= static_cast<std::size_t>( earned ) );

        described.append( entry );
    }

    return described;
}

void ExerciseSessionController::leaveReviewMode() noexcept
{
    // L'etat de bilan ne doit pas SURVIVRE a un bilan. Il vit dans deux membres, et les oublier est exactement ce qui a
    // fait parler toutes les parties de Roger comme des bilans : une seule fonction, donc plus rien a oublier.
    m_isReviewRunning = false;
    m_reviewEasyQuestionCount = 0;
}

void ExerciseSessionController::startSession()
{
    // L'ARCADE : la porte principale. Une partie ordinaire n'est PAS un bilan, et le drapeau retombe ici.
    leaveReviewMode();

    m_gameMode = domain::GameMode::Arcade;

    // Le plan de l'Arcade a besoin d'une graine, et elle est tiree ICI : le domaine n'a aucune source d'entropie, ce qui
    // rend le plan reproductible dans un test. La graine de la SESSION, elle, reste tiree par beginSession.
    std::random_device entropySource;

    beginSession( domain::arcadeSettingsFor( m_playerLevel.value_or( domain::PlayerLevel::Beginner ),
                                             entropySource(),
                                             arcadeLives() ) );
}

void ExerciseSessionController::startOrdinarySession()
{
    // Le MOTEUR, sans porte : `beginSession(m_settings)` et rien de plus. Voir l'en-tete pour WHY ce n'est pas l'Arcade.
    leaveReviewMode();

    // Elle paie l'experience comme l'Arcade : c'est ce que faisait l'ancien « Jouer », et les tests de regle qui lisent
    // l'experience apres une partie continuent de la lire.
    m_gameMode = domain::GameMode::Arcade;

    beginSession( m_settings );
}

void ExerciseSessionController::startTrainingSession( int p_family )
{
    leaveReviewMode();

    m_gameMode = domain::GameMode::Training;

    // La famille est RETENUE : « Rejouer » doit rejouer le meme Entrainement, pas l'Arcade.
    m_lastTrainingFamily = std::clamp( p_family, 0, 2 );

    // Un entrainement ne paie rien, mais il applique le PERIMETRE DU GODMODE quand celui-ci est choisi : c'est meme l'une
    // des raisons d'etre du GodMode - « pour ceux qui veulent juste tester le jeu et ne pas y etre regulier ».
    const auto family = static_cast<domain::QuestionFamily>( m_lastTrainingFamily );

    beginSession( domain::trainingSettingsFor( m_playerLevel.value_or( domain::PlayerLevel::Beginner ), family ) );
}

void ExerciseSessionController::restartSession()
{
    // Le MEME mode, et rien d'autre : voir l'en-tete pour WHY ce n'est pas startSession.
    switch( m_gameMode )
    {
        case domain::GameMode::Training:
            startTrainingSession( m_lastTrainingFamily );
            break;

        case domain::GameMode::Infinite:
            startInfiniteSession();
            break;

        case domain::GameMode::Survival:
            startSurvivalSession();
            break;

        case domain::GameMode::Review:
            startReviewSession();
            break;

        case domain::GameMode::Arcade:
            startSession();
            break;
    }
}

void ExerciseSessionController::startInfiniteSession()
{
    domain::SessionSettings settings = m_settings;

    // Le mode infini, c'est le mode qui ne s'arrete jamais : pas de vies, pas de fin, juste enchaner. Une erreur
    // coute du rythme - la serie retombe - mais jamais la partie.
    leaveReviewMode();

    // Le jeu LIBRE ne paie pas : les poids des reglages le gouvernent, et il ne fait pas monter de niveau. Roger :
    // « si il veut progresser en experience, il doit imperativement faire le B ».
    m_gameMode = domain::GameMode::Infinite;

    settings.lives = std::nullopt;
    settings.questionCount = std::numeric_limits<std::size_t>::max();

    beginSession( settings );
}

void ExerciseSessionController::startSurvivalSession()
{
    domain::SessionSettings settings = m_settings;

    // Le survival, c'est l'arcade avec des vies : un nombre de questions sans fin, et la partie s'arrete quand les
    // vies tombent a zero. Les vies restent donc celles du niveau, pas un retour en arriere vers "illimite".
    leaveReviewMode();

    m_gameMode = domain::GameMode::Survival;

    settings.questionCount = std::numeric_limits<std::size_t>::max();

    beginSession( settings );
}

void ExerciseSessionController::beginSession( domain::SessionSettings p_settings )
{
    // LE CHRONO DE LA PARTIE commence ici, et une seule fois : c'est le seul endroit par lequel toutes les portes passent.
    m_sessionClock.start();
    m_sessionDurationSeconds = 0;

    // Et l'annonce de la partie PRECEDENTE s'efface : un trophee gagne hier ne doit pas s'afficher a la fin de la partie
    // d'aujourd'hui. Un bilan le remplira de nouveau, s'il y a lieu.
    m_newlyEarnedTrophies.clear();
    m_titleJustIncreased = false;

    // L'annonce de la partie PRECEDENTE s'efface aussi A L'ECRAN : sans ce signal, le badge du bilan resterait affiche
    // pendant la partie qui commence.
    emit playerProgressChanged();

    // Le bouton a repondu : un clic tres court et discret, pour que la main soit entendue.
    m_notePlayer.playTapCue();

    // LE TIMBRE DE LA SESSION, choisi UNE fois : voir NotePlayer::beginTimbreForSession. C'est ce qui fait qu'une seconde
    // mineure et une quinte, dans la meme partie, sont jouees par le meme instrument - Roger a entendu le contraire.
    m_notePlayer.beginTimbreForSession();

    // LE PERIMETRE DU GODMODE, quand c'est lui qui joue - et JAMAIS dans un Bilan.
    //
    // Un Bilan a ses propres questions decidees, du plus facile au plus difficile : elles n'ont rien a voir avec un
    // perimetre choisi a la main, et Roger a tranche - « le bilan, va au plus simple ». Un seul endroit, donc rien a
    // oublier dans les trois facons de commencer une partie.
    if( !m_isReviewRunning )
    {
        applyGodModeIfChosen( p_settings );
    }

    // The seed is drawn HERE, in the interface layer, and never inside the domain.
    //
    // That is what keeps a session reproducible from its seed in a test, and it is also why the rules
    // of the game can be replayed exactly when something goes wrong. The domain owns no entropy
    // source, on purpose.
    std::random_device entropySource;

    // Le LIVRE DES PHRASES est donne a la CONSTRUCTION de chaque seance, et pas apres : la premiere question est construite
    // par le constructeur lui-meme, donc un livre fourni ensuite ne pourrait plus rien pour elle. C'est ce que le jeu et le
    // banc d'essai partagent - le domaine tire la phrase, et se rabat sur une gamme quand le contenu n'en a pas.
    m_session = std::make_unique<domain::ExerciseSession>( entropySource(), p_settings, m_phraseBook );

    // LE DIAGNOSTIC, en une ligne, et il reste : « pourquoi cette question-la ? » se repond avec des chiffres, jamais avec
    // une memoire. Les parts affichees par les reglages et celles qui tirent vraiment la question sont deux choses, et
    // elles peuvent diverger sans que rien ne le dise - c'est precisement ce qu'il fallait pouvoir voir.
    qInfo().nospace() << "Musichien session: bilan=" << ( m_isReviewRunning ? "oui" : "non" )
                      << " nommer=" << p_settings.namedIntervalQuestionShare
                      << " chant=" << p_settings.singQuestionShare << " accords=" << p_settings.chordQuestionShare
                      << " modes(clair/nom/deux)=" << p_settings.modeColourQuestionShare << "/"
                      << p_settings.modeNameQuestionShare << "/" << p_settings.modeVampQuestionShare
                      << " etrangere=" << p_settings.foreignNoteQuestionShare
                      // Le genre de la PREMIERE question, avec les parts qui l'ont produite : sans lui, un joueur bloque
                      // sur la question 1 ne laisse aucune trace, et il faut deviner. C'est exactement ce qui est arrive.
                      << " genre1=" << static_cast<int>( m_session->currentQuestion().kind );

    // The end of the previous session has been announced; this one gets its own turn.
    m_sessionEndAnnounced = false;

    // La PREMIERE question a son anecdote, comme les suivantes : c'est la meme regle du debut a la fin.
    refreshQuestionAnecdote();

    emit runningChanged();

    refreshChoices();

    playCurrentQuestion();

    emit scoreChanged();
    emit sessionChanged();
}

void ExerciseSessionController::stopSession()
{
    // LE CHIEN S'INVITE : le joueur revient de sa partie, et c'est LE moment ou une anecdote se lit - juste apres avoir
    // joue. Il ne parle que d'une partie TERMINEE : quitter en cours de route n'a rien a raconter.
    //
    // Le drapeau est pose AVANT la remise a zero ci-dessous, parce que c'est la session qui porte « finie ou non ».
    if( isFinished() && !m_isChibaTalking )
    {
        m_isChibaTalking = true;

        pickChibaImage();

        // ET IL ABOIE : le chien annonce qu'il a quelque chose a dire, et cette annonce doit s'entendre avant qu'on lise
        // son anecdote - sans quoi il apparaîtrait en silence, comme une image.
        m_notePlayer.playDogBark();

        emit chibaTalkingChanged();
    }

    stopRhythmLoop();

    stopPlayback();

    m_session.reset();

    m_choices.clear();

    // ON REVIENT SUR LA PAGE PRINCIPALE : une nouvelle anecdote y attend le joueur. La mise en page de l'accueil ne
    // change pas, mais son texte n'est jamais deux fois le meme - c'est ce qui donne envie de la relire.
    refreshAnecdote();

    emit runningChanged();
    emit questionChanged();
    emit sessionChanged();
    emit scoreChanged();
}

void ExerciseSessionController::replay()
{
    if( ( m_session == nullptr ) || !m_session->canReplay() )
    {
        return;
    }

    // Counted by the session, played here: the count has to follow what was really heard, and only
    // the screen knows that.
    m_session->registerReplay();

    playCurrentQuestion();

    emit scoreChanged();
    emit sessionChanged();
}

void ExerciseSessionController::answer( int p_semitones )
{
    if( ( m_session == nullptr ) || !isAsking() )
    {
        // An answer arriving twice, or after the question is over, changes nothing. The session says
        // so itself; stopping here keeps the signals from firing for nothing.
        return;
    }

    processAnswer( m_session->answer( p_semitones ) );
}

void ExerciseSessionController::answerDirection( int p_direction )
{
    if( ( m_session == nullptr ) || !isAsking() )
    {
        return;
    }

    const auto direction = static_cast<domain::IntervalDirection>( p_direction );

    processAnswer( m_session->answerDirection( direction ) );
}

void ExerciseSessionController::answerSung( bool p_isCorrect, int p_centsOffset )
{
    if( ( m_session == nullptr ) || !isAsking() )
    {
        return;
    }

    // La mesure est mise de cote AVANT de traiter la reponse : la traiter resynchronise la cible du micro, ce qui
    // remet son detecteur a zero. Sans cette ligne, l'ecart du chant etait efface a l'instant meme ou le verdict
    // s'affichait - et le joueur ne voyait jamais de combien il avait ete juste.
    m_lastSungCentsOffset = p_centsOffset;

    processAnswer( m_session->answerSung( p_isCorrect ) );
}

void ExerciseSessionController::setMicrophoneController( MicrophoneController * p_microphone )
{
    m_microphone = p_microphone;
}

void ExerciseSessionController::setQuestionLog( domain::QuestionLog * p_questionLog )
{
    m_questionLog = p_questionLog;
}

int ExerciseSessionController::questionKind() const noexcept
{
    return ( m_session != nullptr ) ? static_cast<int>( m_session->currentQuestion().kind ) : 0;
}

bool ExerciseSessionController::isRhythmQuestion() const noexcept
{
    return ( m_session != nullptr ) && ( m_session->currentQuestion().kind == domain::QuestionKind::Rhythm );
}

QString ExerciseSessionController::rhythmPatternName() const
{
    if( !isRhythmQuestion() )
    {
        return QString{};
    }

    // Le nom vient du domaine, et l'ecran le montre tel quel : "Binaire", "Valse" sont des noms de MUSIQUE, pas des
    // identifiants techniques, et les renommer est une decision de contenu.
    const std::string_view name = patternOf( m_session->currentQuestion() ).name();

    return QString::fromUtf8( name.data(), static_cast<int>( name.size() ) );
}

int ExerciseSessionController::rhythmBpm() const noexcept
{
    return isRhythmQuestion() ? m_session->currentQuestion().bpm : 0;
}

int ExerciseSessionController::rhythmBeatsPerBar() const noexcept
{
    return isRhythmQuestion() ? patternOf( m_session->currentQuestion() ).beatsPerBar() : 0;
}

QVariantList ExerciseSessionController::rhythmHits() const
{
    QVariantList hits;

    if( !isRhythmQuestion() )
    {
        // Aucune frappe a dessiner, plutot que les frappes d'une autre question : une liste vide ne dessine rien, et
        // rien est exactement ce qu'il faut montrer sur une question d'intervalle.
        return hits;
    }

    for( const domain::RhythmHit & hit : patternOf( m_session->currentQuestion() ).hits() )
    {
        QVariantMap described;

        // Ou la frappe tombe, en TEMPS depuis le debut de la boucle : c'est le domaine qui le dit, et l'ecran se
        // contente de placer le trait sur la mesure.
        described.insert( QStringLiteral( "beat" ), hit.beat );
        described.insert( QStringLiteral( "accented" ), hit.accented );
        described.insert( QStringLiteral( "drum" ), static_cast<int>( hit.drum ) );

        hits.append( described );
    }

    return hits;
}

int ExerciseSessionController::rhythmBeatInBar() const noexcept
{
    const int beatsPerBar = rhythmBeatsPerBar();

    // L'ecran place un curseur sur la mesure : il lui faut un temps REEL, jamais le signal de fin de mesure. Le modulo
    // le lui garantit, meme apres un recalage de la grille qui aurait fait bondir l'index.
    return ( beatsPerBar > 0 ) ? ( m_rhythmBeatIndex % beatsPerBar ) : 0;
}

double ExerciseSessionController::rhythmPositionInBar() const
{
    const int beatsPerBar = rhythmBeatsPerBar();

    if( beatsPerBar <= 0 )
    {
        return 0.0;
    }

    // Le modulo d'un flottant, et non d'un entier : c'est TOUT le sujet. Le curseur doit GLISSER d'un temps au suivant,
    // pas y sauter - et un index entier ne peut que sauter.
    return std::fmod( rhythmPositionInBeats(), static_cast<double>( beatsPerBar ) );
}

std::int64_t ExerciseSessionController::rhythmElapsedMilliseconds() const
{
    // LE PUITS AUDIO D'ABORD - c'est lui qui a raison. L'horloge de l'interface ne mesure que le moment ou l'on a
    // DEMANDE le son ; le puits mesure celui ou l'oreille le recoit, tampon compris. Roger : « le son n'est pas synchro
    // avec la note jouee » - et il ne pouvait pas l'etre, les deux horloges etant differentes.
    const std::chrono::milliseconds audioPosition = m_notePlayer.playedMilliseconds();

    if( audioPosition.count() > 0 )
    {
        return audioPosition.count();
    }

    // LE REPLI, et ce n'est pas un ornement : un adaptateur sans horloge audio (les tests, un appareil qui ne repond pas)
    // repond zero, et un curseur FIGE serait pire que le desaccord qu'on corrige. Zero veut donc dire « je ne sais pas »,
    // jamais « le son vient de commencer ».
    return m_rhythmClock.elapsed();
}

bool ExerciseSessionController::isRhythmPlaying() const noexcept
{
    return m_rhythmIsPlaying;
}

int ExerciseSessionController::rhythmLastQuality() const noexcept
{
    return m_rhythmLastQuality;
}

int ExerciseSessionController::rhythmCoveredOnsets() const noexcept
{
    return m_rhythmCoveredOnsets;
}

int ExerciseSessionController::rhythmOnsetCount() const noexcept
{
    return isRhythmQuestion() ? static_cast<int>( m_session->currentQuestion().coveredOnsets.size() ) : 0;
}

int ExerciseSessionController::rhythmCellDurationMs() const noexcept
{
    if( !isRhythmQuestion() )
    {
        return 0;
    }

    // Une mesure : c'est ce que dure l'ecoute, la reproduction, et la confirmation. L'ecran s'en sert pour laisser au
    // feedback le temps de s'entendre - une pause plus courte couperait la cellule en plein milieu.
    const double beatMs = domain::beatDurationMs( static_cast<double>( m_session->currentQuestion().bpm ) );

    return static_cast<int>( std::lround( beatMs * static_cast<double>( rhythmBeatsPerBar() ) ) );
}

bool ExerciseSessionController::isChordQuestion() const noexcept
{
    return ( m_session != nullptr ) && ( m_session->currentQuestion().kind == domain::QuestionKind::Chord );
}

QVariantMap ExerciseSessionController::heardChord() const
{
    if( !isChordQuestion() )
    {
        return QVariantMap{};
    }

    return describeChord( m_session->currentQuestion().chord );
}

QVariantMap ExerciseSessionController::answeredChord() const
{
    if( ( m_session == nullptr ) || !isChordQuestion() )
    {
        return QVariantMap{};
    }

    const std::optional<domain::ChordQuality> answer = m_session->lastChordAnswer();

    // operator* plutot que value() : un optional vide est ce que "rien a montrer" veut dire ici, et c'est deja gere.
    if( !answer.has_value() )
    {
        return QVariantMap{};
    }

    // Le symbole se construit sur la tonique de la QUESTION : le joueur a repondu une couleur, pas une tonique, et le
    // verdict doit opposer deux couleurs sur la meme note.
    return describeChord( domain::Chord{ .quality = *answer,
                                         .rootMidiNumber = m_session->currentQuestion().chord.rootMidiNumber } );
}

QVariantList ExerciseSessionController::chordChoices() const
{
    QVariantList choices;

    if( ( m_session == nullptr ) || !isChordQuestion() )
    {
        return choices;
    }

    // LE NOM DE LA TONIQUE, une fois pour tous les boutons : « C », « F# », « Bb ».
    //
    // Elle vient du DOMAINE (Note::pitchClassName), comme tous les autres noms de l'application - l'ecran ne nomme rien
    // lui-meme. Et la tonique est celle de la QUESTION : elle ne change pas d'un bouton a l'autre, donc seul le suffixe
    // distingue les couleurs.
    const QString rootName =
      QString::fromStdString( domain::Note{ m_session->currentQuestion().chord.rootMidiNumber }.pitchClassName() );

    for( const domain::ChordQuality quality : m_session->currentQuestion().chordChoices )
    {
        QVariantMap described;

        const std::string_view suffix = domain::chordQualitySymbolSuffix( quality );

        described.insert( QStringLiteral( "quality" ), static_cast<int>( quality ) );

        // LE NOM DE L'ACCORD, en notation anglo-saxonne : « C », « Cm », « Csus4 », « C7 », « Cm7b5 ».
        //
        // C'est la notation des recueils et des vraies partitions : un musicien la lit sans y penser. Le nombre de notes
        // affiche avant a disparu, parce qu'il n'apprenait rien qu'un musicien ne sache deja.
        described.insert( QStringLiteral( "name" ),
                          rootName + QString::fromUtf8( suffix.data(), static_cast<int>( suffix.size() ) ) );

        // Le nom complet (« Half-diminished ») reste disponible : c'est lui que le verdict ecrit, la ou il y a la place.
        described.insert( QStringLiteral( "fullName" ),
                          QString::fromUtf8( domain::chordQualityName( quality ).data(),
                                             static_cast<int>( domain::chordQualityName( quality ).size() ) ) );

        choices.append( described );
    }

    return choices;
}

bool ExerciseSessionController::isModeQuestion() const noexcept
{
    if( m_session == nullptr )
    {
        return false;
    }

    const domain::QuestionKind kind = m_session->currentQuestion().kind;

    return ( kind == domain::QuestionKind::ModeColour ) || ( kind == domain::QuestionKind::ModeName )
           || ( kind == domain::QuestionKind::ModeVamp );
}

bool ExerciseSessionController::isModeColourQuestion() const noexcept
{
    if( m_session == nullptr )
    {
        return false;
    }

    // Vrai pour les DEUX questions qui se repondent par un SENS : la comparaison de deux modes, et le vamp - qui pose la
    // meme question, sous une autre lumiere. L'ecran offre donc les memes reponses, a une pres : voir isModeVampQuestion.
    const domain::QuestionKind kind = m_session->currentQuestion().kind;

    return ( kind == domain::QuestionKind::ModeColour ) || ( kind == domain::QuestionKind::ModeVamp );
}

bool ExerciseSessionController::isModeVampQuestion() const noexcept
{
    return ( m_session != nullptr ) && ( m_session->currentQuestion().kind == domain::QuestionKind::ModeVamp );
}

bool ExerciseSessionController::isHarmonyQuestion() const noexcept
{
    return isModeQuestion() || isForeignNoteQuestion();
}

QVariantList ExerciseSessionController::modeChoices() const
{
    QVariantList choices;

    if( m_session == nullptr )
    {
        return choices;
    }

    // Chaque entree est decrite par la MEME fonction que le banc d'essai des modes : un bouton de choix et un bouton de
    // banc d'essai portent donc exactement le meme nom et la meme couleur. Deux descriptions ecrites a la main
    // finiraient par diverger.
    for( const domain::Mode mode : m_session->currentQuestion().modeChoices )
    {
        choices.append( describeMode( mode ) );
    }

    return choices;
}

QVariantMap ExerciseSessionController::heardMode() const
{
    // Le mode de la QUESTION, et non le dernier qui a sonne.
    //
    // C'etait un cache, mis a jour a chaque lecture, et il mentait : un joueur qui ecoute, reecoute, et repond avant que
    // le second passage ne sonne y lisait encore le PREMIER mode - donc « Dorien -> Dorien » dans le verdict, c'est-a-dire
    // deux modes identiques qui n'ont jamais existe. Roger l'a vu : « bug dans le plus clair plus sombre, si les 2 modes
    // sont identiques, erreur ».
    //
    // La question SAIT quel mode elle a pose : le lire la est plus court que de tenir un cache a jour, et surtout cela ne
    // peut pas se desynchroniser de ce qui a ete demande.
    if( m_session == nullptr )
    {
        return {};
    }

    const domain::Question & question = m_session->currentQuestion();

    // LA NOTE ETRANGERE ANNONCE SON MODE, et c'est une correction demandee par Roger : « il faut afficher le mode qui est en
    // train d'etre joue des le debut. Car sans rien c'est beaucoup trop difficile, il doit a la fois trouver le mode ainsi
    // que la note qui ne va pas ».
    //
    // Il a raison, et la question le dit elle-meme : la gamme jouee EST le mode. Le cacher n'ajoutait pas de difficulte, il
    // ajoutait une devinette - on ne peut pas chercher une note etrangere A UN MODE dont on ne sait rien.
    if( !isModeQuestion() && !isForeignNoteQuestion() )
    {
        // Une question d'intervalle ou d'accord ne parle pas de mode : la question en porte un par construction (l'ordre
        // des tirages), mais personne ne l'a entendu, et le decrire serait un mensonge de plus.
        return {};
    }

    return describeMode( question.mode );
}

QVariantMap ExerciseSessionController::previousMode() const
{
    if( ( m_session == nullptr ) || !m_session->currentQuestion().previousMode.has_value() )
    {
        // Une question de NOM n'a rien a comparer : l'ecran recoit une carte VIDE, et c'est ce qui lui dit de ne rien
        // afficher sous les boutons.
        return {};
    }

    return describeMode( *m_session->currentQuestion().previousMode );
}

QVariantMap ExerciseSessionController::modeDifference() const
{
    if( ( m_session == nullptr ) || !m_session->currentQuestion().previousMode.has_value() )
    {
        // Une question de NOM n'a rien a comparer : l'ecran recoit une carte vide, comme pour le mode precedent.
        return {};
    }

    return describeModeDifference( *m_session->currentQuestion().previousMode, m_session->currentQuestion().mode );
}

QVariantList ExerciseSessionController::modeCircle() const
{
    // Deux raisons de ne rien montrer, et une seule de montrer : la question parle d'un mode - ou d'une gamme dont
    // l'intrus est a trouver, et la roue dit alors laquelle des douze notes n'y est pas.
    if( ( m_session == nullptr ) || !( isModeQuestion() || isForeignNoteQuestion() ) )
    {
        return {};
    }

    const domain::Question & question = m_session->currentQuestion();

    return describeModeCircle( question.mode, question.modeTonic.pitchClassIndex() );
}

QString ExerciseSessionController::modeCircleLabel() const
{
    if( ( m_session == nullptr ) || !( isModeQuestion() || isForeignNoteQuestion() ) )
    {
        return {};
    }

    const domain::Question & question = m_session->currentQuestion();

    // Le NOM du mode n'arrive qu'avec le verdict : avant la reponse, il serait la reponse - de la question de nom comme
    // de la question de couleur, ou il est une des deux choses qu'on demande de comparer.
    const QString modeName = isAsking() ? QString{} : describeMode( question.mode ).value( "name" ).toString();

    // La note etrangere est la SEULE question ou la gamme n'est pas la reponse : c'est l'intrus qu'on cherche, donc on
    // peut nommer la gamme tout de suite.
    if( isForeignNoteQuestion() )
    {
        return modeName.isEmpty() ? tr( "La gamme" ) : tr( "La gamme : %1" ).arg( modeName );
    }

    if( question.kind == domain::QuestionKind::ModeName )
    {
        return modeName.isEmpty() ? tr( "Le mode entendu" ) : modeName;
    }

    // Une question de couleur ou un vamp : DEUX modes ont sonne, et la roue est celle du SECOND.
    return modeName.isEmpty() ? tr( "Le second passage" ) : tr( "Le second passage : %1" ).arg( modeName );
}

int ExerciseSessionController::modeSoundDurationMs() const
{
    // Les questions dont le son est une MELODIE sur un bourdon : les trois de mode, et la note etrangere - qui joue sept
    // notes, soit exactement ce qu'un mode fait entendre.
    if( ( m_session == nullptr ) || !( isModeQuestion() || isForeignNoteQuestion() ) )
    {
        return 0;
    }

    const domain::DroneFraming framing{ MODE_LEAD_IN, MODE_TAIL };

    // Une PHRASE a ses propres durees, et elle dure souvent PLUS longtemps qu'une gamme de sept notes. C'est donc SA
    // duree qu'il faut annoncer : sans elle, l'ecran revelerait la reponse - ou enchainerait sur la question suivante -
    // pendant que la melodie joue encore.
    if( const std::optional<domain::Phrase> & phrase = m_session->currentQuestion().modePhrase; phrase.has_value() )
    {
        const std::int32_t bpm =
          ( m_levelStore != nullptr ) ? m_levelStore->storedPhraseTempoBpm() : FALLBACK_PHRASE_TEMPO;

        const domain::PhrasePlayback playback = domain::playbackOf( *phrase, bpm );

        // La surcharge du domaine pour des durees VARIABLES : elle compte le silence une fois par note, exactement comme
        // la version uniforme. Le calcul reste donc le meme des deux cotes, et c'est ce qui l'empeche de mentir.
        return static_cast<int>( domain::droneDurationFor( playback.durations, PHRASE_NOTE_GAP, framing ).count() );
    }

    // HUIT notes, et non sept : la gamme se REFERME sur sa tonique - voir notesOfModeClosingOnTonic. Cette note fait
    // partie de ce qu'on entend, donc elle fait partie de ce qu'il faut annoncer : la compter pour rien couperait le son
    // a la fin de la phrase, et ferait enchainer la question suivante par-dessus la note qui la conclut.
    const std::chrono::milliseconds oneMode =
      domain::droneDurationFor( domain::DEGREE_COUNT + 1, MODE_NOTE_DURATION, MODE_NOTE_GAP, framing );

    // Le vamp et la couleur font entendre DEUX modes, separes par le meme silence que celui du minuteur qui les enchaine.
    // Une question de nom n'en fait entendre qu'un.
    const bool twoModes = m_session->currentQuestion().previousMode.has_value();

    return static_cast<int>( ( twoModes ? ( ( oneMode * 2 ) + MODE_COMPARISON_GAP ) : oneMode ).count() );
}
int ExerciseSessionController::modeSoundLeadInMs() const
{
    // La MEME condition que la duree : hors d'une question de mode, la roue n'est pas affichee, et une valeur inventee ici
    // ne servirait a personne.
    if( ( m_session == nullptr ) || !( isModeQuestion() || isForeignNoteQuestion() ) )
    {
        return 0;
    }

    return static_cast<int>( MODE_LEAD_IN.count() );
}

int ExerciseSessionController::modeSoundNoteStepMs() const
{
    if( ( m_session == nullptr ) || !( isModeQuestion() || isForeignNoteQuestion() ) )
    {
        return 0;
    }

    // La MEME somme que celle passee au lecteur : deux calculs qui doivent coincider finissent toujours par diverger, donc
    // celui-ci est ecrit une seule fois.
    return static_cast<int>( ( MODE_NOTE_DURATION + MODE_NOTE_GAP ).count() );
}

bool ExerciseSessionController::isForeignNoteQuestion() const noexcept
{
    return ( m_session != nullptr ) && ( m_session->currentQuestion().kind == domain::QuestionKind::ForeignNote );
}

QVariantList ExerciseSessionController::foreignNoteChoices() const
{
    if( !isForeignNoteQuestion() )
    {
        return {};
    }

    const domain::Question & question = m_session->currentQuestion();

    return describeScale( question.mode, question.modeTonic.pitchClassIndex() );
}

QVariantMap ExerciseSessionController::foreignNoteVerdict() const
{
    // Rien tant que la question est posee : le verdict dit OU etait l'intrus, donc il est la reponse.
    if( !isForeignNoteQuestion() || isAsking() )
    {
        return {};
    }

    const domain::Question & question = m_session->currentQuestion();

    const auto step = static_cast<std::size_t>( question.foreignStepIndex );

    const QVariantList scale = describeScale( question.mode, question.modeTonic.pitchClassIndex() );

    QVariantMap verdict;

    // Le pas est dit de 1 a 7, comme un musicien compte - la ou l'index est de 0 a 6, comme un tableau.
    verdict.insert( QStringLiteral( "stepNumber" ), static_cast<int>( step ) + 1 );

    // Ce qui a ete ENTENDU, et ce que la gamme attendait : les deux, parce que c'est leur paire qui apprend quelque
    // chose - « fa♯ au lieu de fa » dit la faute, « fa♯ » seul ne dit rien.
    verdict.insert( QStringLiteral( "heardName" ),
                    describeNoteName( question.foreignMelody.at( step ).pitchClassIndex() ) );
    verdict.insert( QStringLiteral( "expectedName" ),
                    scale.at( static_cast<qsizetype>( step ) ).toMap().value( QStringLiteral( "name" ) ) );

    return verdict;
}

void ExerciseSessionController::answerForeignNote( int p_stepIndex )
{
    if( m_session == nullptr )
    {
        return;
    }

    processAnswer( m_session->answerForeignNote( p_stepIndex ) );
}

void ExerciseSessionController::playForeignNoteQuestion()
{
    if( m_session == nullptr )
    {
        return;
    }

    const domain::Question & question = m_session->currentQuestion();

    // La melodie du DOMAINE, posee deux octaves au-dessus du bourdon comme l'est celle d'un mode : c'est le meme geste,
    // donc le meme son a comparer a celui du banc d'essai.
    std::vector<domain::Note> melody;
    melody.reserve( question.foreignMelody.size() + 1 );

    for( const domain::Note & note : question.foreignMelody )
    {
        melody.push_back( note.transposedBy( MODE_MELODY_OCTAVE_OFFSET ) );
    }

    // LA GAMME SE REFERME ICI AUSSI : les sept pas, puis la tonique une octave plus haut.
    //
    // La note vient du DOMAINE - question.modeTonic - et non de la derniere note entendue : l'intrus peut tomber
    // n'importe ou, y compris sur le premier pas, donc la melodie ne se termine pas forcement sur la tonique.
    //
    // Et elle ne dit RIEN de la reponse : c'est la tonique du mode, toujours la meme, et elle est deja connue puisque
    // c'est sur elle que le bourdon est pose.
    melody.push_back( question.modeTonic.transposedBy( MODE_MELODY_OCTAVE_OFFSET + domain::SEMITONES_PER_OCTAVE ) );

    const std::array<domain::Note, 2> drone{ question.modeTonic,
                                             question.modeTonic.transposedBy( FIFTH_IN_SEMITONES ) };

    m_notePlayer.playMelodyOverDrone( melody,
                                      drone,
                                      MODE_NOTE_DURATION,
                                      MODE_NOTE_GAP,
                                      domain::DroneFraming{ MODE_LEAD_IN, MODE_TAIL } );

    // LA ROUE S'ANIME ICI AUSSI, et c'est Roger qui l'a demande : « j'aimerais bien que pour la note etrangere il y
    // ait aussi ces lignes ».
    //
    // ELLE MONTRE LE CHEMIN QUE LA GAMME AURAIT DU SUIVRE, et non celui qu'on entend : les sept pas dans l'ordre, de la
    // tonique au septieme degre. C'est tout l'interet - la ligne est la REFERENCE, et c'est l'ecart entre elle et le son
    // qui fait entendre l'intrus. Une trainee qui suivrait la note etrangere ferait un bond hors du cercle, et
    // montrerait la reponse.
    //
    // Les deux nombres que la roue consomme decrivent cette gamme-la, et ils sont deja les siens : voir
    // modeSoundLeadInMs et modeSoundNoteStepMs, qui comptent la note etrangere parmi les questions de mode. Le dessin et
    // le son partent donc ensemble, sans un decalage que rien n'expliquerait.
    emit modePlaybackStarted();
}

void ExerciseSessionController::answerModeColour( bool p_secondIsBrighter )
{
    if( m_session == nullptr )
    {
        return;
    }

    processAnswer( m_session->answerModeColour( p_secondIsBrighter ) );
}

void ExerciseSessionController::answerSameColour()
{
    if( m_session == nullptr )
    {
        return;
    }

    processAnswer( m_session->answerModeColour( domain::ModeColourAnswer::Same ) );
}

void ExerciseSessionController::answerModeName( int p_modeIndex )
{
    if( m_session == nullptr )
    {
        return;
    }

    // Le signe d'abord, la borne ensuite : meme regle que dans le banc d'essai des modes, et pour la meme raison.
    if( p_modeIndex < 0 )
    {
        return;
    }

    if( std::cmp_greater_equal( p_modeIndex, domain::MODE_COUNT ) )
    {
        return;
    }

    processAnswer( m_session->answerModeName( static_cast<domain::Mode>( p_modeIndex ) ) );
}

void ExerciseSessionController::answerChord( int p_quality )
{
    if( ( m_session == nullptr ) || !isAsking() )
    {
        // Une reponse arrivant deux fois, ou apres la question, ne change rien : la session le dit elle-meme, et
        // s'arreter ici evite d'emettre des signaux pour rien.
        return;
    }

    if( p_quality < 0 || std::cmp_greater_equal( p_quality, domain::CHORD_QUALITY_COUNT ) )
    {
        // Un index hors liste vient d'un ecran qui invente. Il est refuse plutot que converti en une couleur qui
        // n'existe pas.
        return;
    }

    processAnswer( m_session->answerChord( static_cast<domain::ChordQuality>( p_quality ) ) );
}

void ExerciseSessionController::processAnswer( bool p_isCorrect )
{
    // Always, and not only when the question is over: a wrong answer closes the grid in, and the
    // screen must show the grid that exists rather than the one it had a moment ago.
    refreshChoices();

    // Le rythme a deja repondu tout seul : sa cellule est en train de sonner - l'ecoute qui reprend apres un rate, ou
    // la confirmation apres une reussite. Le chemin commun des notes doit donc se taire, sinon deux sons se marcheraient
    // dessus, et c'est la cellule qu'on n'entendrait plus.
    const bool isRhythm = isRhythmQuestion();

    if( p_isCorrect )
    {
        if( !isRhythm )
        {
            // A correct answer is heard again as a CHORD: the two notes together, one block instead of two,
            // which is twice as short, and a genuinely different listen of the same interval - the colour
            // without the melody. It keeps a success from dragging, and hearing an interval both ways is
            // what seals it.
            playCurrentQuestionAsChord();
        }
    }
    else
    {
        if( !isRhythm )
        {
            // UNE QUESTION D'ACCORD SE REJOUE EN DEUX TEMPS : ce que le joueur a joue, PUIS la reponse.
            //
            // Roger : « quand on clique sur un accord et qu'on se trompe, on re-entend directement le bon accord. Je
            // changerais ca : entendre d'abord l'accord appuye, puis l'accord voulu. C'est moins perturbant. »
            //
            // Il a raison, et la raison est plus profonde que le confort : entendre la REPONSE avant sa propre erreur
            // efface l'ECART entre les deux - et l'ecart est toute la lecon.
            if( !playWrongChordThenAnswer() )
            {
                // A wrong answer is heard again IMMEDIATELY, and in its original form: there is something to
                // catch up on, and the melody is what gives the second note its meaning.
                playCurrentQuestion();
            }
        }

        // And it is announced, so that the screen can answer with its BODY - the shake, and the
        // vibration on a device that has a motor. The controller knows what happened; how it should feel
        // is not its job.
        emit wrongAnswerGiven();

        if( !isRhythm )
        {
            // The cue, then the buzz: both are mistakes being made audible and tangible, and neither is the
            // interval the player is being asked to name. See NotePlayer::playMistakeCue for why that
            // distinction matters.
            //
            // Sur une question de rythme, le cue est TU : il couvrirait la cellule, qui est justement ce qu'il faut
            // reentendre. La secousse et la vibration, elles, ne sonnent pas, et restent.
            m_notePlayer.playMistakeCue();
        }

        if( m_vibrate )
        {
            m_vibrate();
        }
    }

    // La question est CONCLUE quand elle est juste, ou quand la session s'arrete sur une derniere vie. Une mauvaise
    // reponse qui laisse la question posee n'est PAS une question conclue : elle se retente, et seule la tentative
    // finale merite une ligne - sinon le journal compterait trois fois la meme question pour une seule lecon.
    if( p_isCorrect || m_session->isFinished() )
    {
        recordCurrentQuestion( p_isCorrect, false );
    }

    announceSessionEndIfNeeded();

    emit scoreChanged();
    emit sessionChanged();
}

void ExerciseSessionController::revealAnswer()
{
    if( ( m_session == nullptr ) || !isAsking() )
    {
        return;
    }

    m_session->revealAnswer();

    // Vue la reponse, et c'est une AIDE : le journal doit le dire ainsi. Un joueur qui demande la reponse n'est pas un
    // joueur qui se trompe, et ses statistiques lui mentiraient s'il y apparaissait comme tel.
    recordCurrentQuestion( false, true );

    refreshChoices();

    if( isRhythmQuestion() )
    {
        // Passer une question de rythme : la cellule est rejouee UNE fois, pour que le joueur entende ce qu'il aurait
        // du reproduire, et la boucle s'arrete - c'est le meme son que la confirmation, et la meme raison.
        m_rhythmCoveredOnsets = coveredOnsetCount();

        stopRhythmLoop();

        playRhythmModelOnce();
    }
    else
    {
        playCurrentQuestion();
    }

    emit scoreChanged();
    emit sessionChanged();
}

void ExerciseSessionController::continueToNextQuestion()
{
    if( ( m_session == nullptr ) || ( m_session->state() != domain::SessionState::Feedback ) )
    {
        return;
    }

    m_session->advance();

    // Chaque question, et son genre, sur une ligne : c'est ce qui repond a « pourquoi celle-la ? » pendant une vraie
    // partie, sur le telephone, sans instrumenter quoi que ce soit apres coup. Le numero du genre est explicite pour que
    // la ligne reste lisible seule.
    qInfo().nospace() << "Musichien question " << m_session->questionNumber() << ": genre="
                      << static_cast<int>( m_session->currentQuestion().kind )
                      << " (0=nommer 1=direction 2=chanter 3=rythme 4=accords 5=mode-clair 6=mode-nom"
                         " 7=mode-deux-centres 8=etrangere)";

    // Une nouvelle question, donc une nouvelle anecdote : le texte suit chaque question, et jamais la meme. Peu de
    // choses sont gratuites et agreables dans une application : celle-ci est les deux.
    refreshQuestionAnecdote();

    refreshChoices();

    if( !m_session->isFinished() )
    {
        playCurrentQuestion();
    }
    else
    {
        // La session s'arrete : rien ne doit continuer a battre derriere l'ecran de fin.
        stopRhythmLoop();

        persistSessionOutcome();
    }

    announceSessionEndIfNeeded();

    emit scoreChanged();
    emit sessionChanged();
}

int ExerciseSessionController::rank() const noexcept
{
    return ( m_session != nullptr ) ? static_cast<int>( domain::rankForStreak( m_session->score().streak() ) ) : 0;
}

QString ExerciseSessionController::rankLabel() const
{
    const domain::Rank rank = ( m_session != nullptr ) ? domain::rankForStreak( m_session->score().streak() )
                                                       : domain::Rank::D;

    switch( rank )
    {
        case domain::Rank::SSS:
            return QStringLiteral( "SSS" );
        case domain::Rank::SS:
            return QStringLiteral( "SS" );
        case domain::Rank::S:
            return QStringLiteral( "S" );
        case domain::Rank::A:
            return QStringLiteral( "A" );
        case domain::Rank::B:
            return QStringLiteral( "B" );
        case domain::Rank::C:
            return QStringLiteral( "C" );
        case domain::Rank::D:
            return QStringLiteral( "D" );
    }

    return QStringLiteral( "D" );
}

QString ExerciseSessionController::anecdoteText() const
{
    return m_anecdoteText;
}

void ExerciseSessionController::refreshAnecdote()
{
    const std::optional<domain::Anecdote> anecdote = m_anecdoteBook.random( m_anecdoteRandomEngine );

    m_anecdoteText = anecdote.has_value() ? QString::fromStdString( anecdote->text ) : QString{};

    emit anecdoteChanged();
}

void ExerciseSessionController::refreshQuestionAnecdote()
{
    // Une anecdote par QUESTION : c'est ce qui fait qu'une partie apprend quelque chose, et c'est aussi ce qui donne une
    // raison de revenir. Un livre vide rend une chaine vide, et l'ecran n'affiche alors rien du tout.
    const std::optional<domain::Anecdote> anecdote = m_anecdoteBook.random( m_anecdoteRandomEngine );

    m_questionAnecdoteText = anecdote.has_value() ? QString::fromStdString( anecdote->text ) : QString{};

    emit questionAnecdoteChanged();
}

void ExerciseSessionController::announceSessionEndIfNeeded()
{
    if( m_sessionEndAnnounced || !isFinished() )
    {
        return;
    }

    m_sessionEndAnnounced = true;

    // On entre dans une session sur une anecdote et on en sort sur une autre : le joueur a appris quelque chose,
    // meme quand la partie s'arrete la. Un seul tirage, garanti par le drapeau.
    refreshAnecdote();
}

void ExerciseSessionController::stopPlayback()
{
    // La boucle de rythme est un son comme un autre : quitter l'ecran doit la faire taire, sinon elle continue de
    // battre derriere le banc d'essai.
    stopRhythmLoop();

    // Et le second temps d'une question d'harmonie, pour la meme raison : un minuteur arme qui survit a l'ecran ferait
    // sonner un mode tout seul, longtemps apres que le joueur est parti.
    m_modeTimer.stop();

    m_notePlayer.stopAll();
}

QString ExerciseSessionController::playerName() const
{
    return ( m_levelStore != nullptr ) ? QString::fromStdString( m_levelStore->playerName() ) : QString{};
}

void ExerciseSessionController::setPlayerName( const QString & p_name )
{
    if( m_levelStore == nullptr )
    {
        return;
    }

    m_levelStore->storePlayerName( p_name.toStdString() );

    emit playerNameChanged();
}

int ExerciseSessionController::totalExperience() const
{
    return ( m_levelStore != nullptr ) ? static_cast<int>( m_levelStore->totalExperience() ) : 0;
}

void ExerciseSessionController::persistSessionOutcome()
{
    if( ( m_levelStore == nullptr ) || ( m_session == nullptr ) )
    {
        return;
    }

    // L'EXPERIENCE, ET SEULEMENT LA OU ELLE SE GAGNE.
    //
    // C'est ici, et nulle part ailleurs, que le mode de la partie decide. Un Entrainement, une partie libre ou un Bilan
    // rapportent ZERO : leurs reglages gouvernent le jeu, mais ils ne font pas monter de niveau. Seule l'Arcade paie, et
    // elle paie le score multiplie par le merite des coeurs.
    std::int32_t earnedExperience = 0;

    if( domain::grantsExperience( m_gameMode ) )
    {
        // Les coeurs PERDUS de la partie qui vient de finir. Le score porte les coeurs RESTANTS, et le budget de depart est
        // la constante de l'Arcade - dix.
        const std::optional<std::int32_t> remainingLives = m_session->score().remainingLives();
        const std::int32_t livesLost =
          remainingLives.has_value() ? std::max( 0, arcadeLives() - *remainingLives ) : 0;

        earnedExperience = domain::arcadeExperience( m_session->score().experience(),
                                                     livesLost,
                                                     m_session->score().completedQuestionCount(),
                                                     m_session->settings().questionCount );

        // FIGE au moment ou la partie se conclut : un ecran de fin qui le relirait plus tard lirait un score qui a change.
        m_lastArcadeMultiplierPercent =
          static_cast<int>( std::lround( domain::arcadeMultiplier( livesLost ) * 100.0 ) );
    }

    // L'experience GAGNEE, telle que le profil vient de la recevoir : c'est le chiffre que l'ecran de fin affiche, et il
    // vaut ZERO dans tout mode qui ne paie pas - l'ecran le dira, plutot que d'afficher un gain qui n'a pas eu lieu.
    m_arcadeXpEarned = static_cast<int>( earnedExperience );

    // ET LE CHRONO S'ARRETE ICI : la duree de la partie est celle qui vient de finir, pas celle qu'on lit.
    m_sessionDurationSeconds = static_cast<int>( m_sessionClock.elapsed() / 1000 );

    // LE BILAN LAISSE UNE TRACE, et c'est la SEULE chose qui donne des titres et des trophees. Roger l'a voulu reserve au
    // bilan : « note qu'on mettra surement des trophees et/ou des certificats, accessibles seulement via le Bilan ».
    recordBilanOutcome();

    // The session is over: its experience, its count and its star become part of the profile, once. Calling this
    // twice would count the same session twice, so it happens only from the transition into "finished".
    m_levelStore->storeTotalExperience( m_levelStore->totalExperience() + earnedExperience );
    m_levelStore->storeSessionCount( m_levelStore->sessionCount() + 1 );

    if( m_session->hasEarnedStar() )
    {
        m_levelStore->storeStarCount( m_levelStore->starCount() + 1 );
    }

    // ET C'EST ICI QUE LA FELICITATION PEUT NAITRE : l'experience vient d'augmenter, donc le palier merite peut avoir change.
    //
    // Une seule emission, au seul endroit qui augmente le total - c'est ce qui garantit que l'ecran de fin de partie voit
    // l'invitation au bon moment, et pas un tour plus tard.
    emit levelInvitationChanged();

    emit totalExperienceChanged();

    // L'experience a monte : un palier a pu s'OUVRIR, donc la liste des difficultees doit se rafraichir - sinon le cadenas
    // reste affiche sur une entree desormais permise.
    emit playerLevelChanged();
}

int ExerciseSessionController::sessionCount() const
{
    return ( m_levelStore != nullptr ) ? static_cast<int>( m_levelStore->sessionCount() ) : 0;
}

int ExerciseSessionController::starCount() const
{
    return ( m_levelStore != nullptr ) ? static_cast<int>( m_levelStore->starCount() ) : 0;
}

bool ExerciseSessionController::dailyReminderEnabled() const
{
    return ( m_levelStore != nullptr ) && m_levelStore->dailyReminderEnabled();
}

void ExerciseSessionController::setDailyReminderEnabled( bool p_enabled )
{
    if( m_levelStore == nullptr )
    {
        return;
    }

    m_levelStore->storeDailyReminderEnabled( p_enabled );

    // Rallumer le rappel demande l'autorisation SUR LE CHAMP : le joueur vient d'exprimer une intention, et c'est le
    // meilleur moment pour poser la question. Attendre le prochain lancement serait une reponse a cote.
    if( p_enabled )
    {
        emit notificationPermissionRequested();
    }

    emit dailyReminderChanged();
}

int ExerciseSessionController::temperament() const
{
    return ( m_levelStore != nullptr ) ? static_cast<int>( m_levelStore->storedTemperament() ) : 0;
}

void ExerciseSessionController::setTemperament( int p_index )
{
    if( m_levelStore == nullptr )
    {
        return;
    }

    if( p_index < 0 )
    {
        return;
    }

    if( static_cast<std::size_t>( p_index ) >= domain::TEMPERAMENT_NAMES.size() )
    {
        return;
    }

    m_levelStore->storeTemperament( static_cast<domain::Temperament>( p_index ) );

    emit temperamentChanged();
}

// A Q_PROPERTY READ must be a member function, even when it reads nothing from the object.
QVariantList ExerciseSessionController::temperaments() const    // NOLINT(readability-convert-member-functions-to-static)
{
    QVariantList names;

    for( const std::string_view name : domain::TEMPERAMENT_NAMES )
    {
        names.append( QString::fromStdString( std::string{ name } ) );
    }

    return names;
}

int ExerciseSessionController::tuningRoot() const
{
    return ( m_levelStore != nullptr ) ? m_levelStore->storedTuningRoot().pitchClassIndex() : 0;
}

void ExerciseSessionController::setTuningRoot( int p_index )
{
    if( m_levelStore == nullptr )
    {
        return;
    }

    if( p_index < 0 || p_index >= domain::SEMITONES_PER_OCTAVE )
    {
        return;
    }

    // A root is a PITCH CLASS: which octave it sits in does not change an interval, so it is stored in octave 4.
    m_levelStore->storeTuningRoot( domain::Note{ 60 + p_index } );

    emit tuningRootChanged();
}

QVariantList ExerciseSessionController::tuningRoots() const    // NOLINT(readability-convert-member-functions-to-static)
{
    QVariantList names;

    for( std::int32_t index = 0; index < domain::SEMITONES_PER_OCTAVE; ++index )
    {
        names.append( QString::fromStdString( domain::Note{ 60 + index }.name() ) );
    }

    return names;
}

double ExerciseSessionController::referencePitch() const
{
    return ( m_levelStore != nullptr ) ? m_levelStore->storedReferencePitch() : 440.0;
}

void ExerciseSessionController::setReferencePitch( double p_hertz )
{
    if( m_levelStore == nullptr )
    {
        return;
    }

    // A diapason outside this band is a typo, not a tuning: it is refused rather than stored.
    if( p_hertz < 400.0 || p_hertz > 480.0 )
    {
        return;
    }

    m_levelStore->storeReferencePitch( p_hertz );

    emit referencePitchChanged();
}

QString ExerciseSessionController::temperamentExplanation() const
{
    const std::string_view text = m_tunerGuide.temperamentText( static_cast<domain::Temperament>( temperament() ) );

    return QString::fromUtf8( text.data(), static_cast<qsizetype>( text.size() ) );
}

QVariantMap ExerciseSessionController::tunerGuide() const
{
    QVariantMap guide;

    guide.insert( QStringLiteral( "diapason" ), QString::fromStdString( std::string{ m_tunerGuide.diapasonText() } ) );

    guide.insert( QStringLiteral( "noteDeReference" ),
                  QString::fromStdString( std::string{ m_tunerGuide.referenceNoteText() } ) );

    QVariantList howTo;

    for( const std::string & step : m_tunerGuide.howToSteps() )
    {
        howTo.append( QString::fromStdString( step ) );
    }

    guide.insert( QStringLiteral( "modeEmploi" ), howTo );

    return guide;
}

void ExerciseSessionController::setTunerGuide( domain::TunerGuide p_guide )
{
    m_tunerGuide = std::move( p_guide );

    // Le texte affiche depend du temperament CHOISI : arriver apres coup oblige donc a le redire, sans quoi la page
    // resterait vide jusqu'au prochain changement de reglage.
    emit temperamentChanged();
}

int ExerciseSessionController::singQuestionShare() const
{
    return ( m_levelStore != nullptr ) ? m_levelStore->storedSingQuestionShare() : 20;
}

int ExerciseSessionController::namedIntervalQuestionShare() const
{
    // SOIXANTE quand il n'y a pas de profil : le defaut du domaine et celui du fichier de reglages disent tous les deux
    // la meme chose, sinon l'un des trois finirait par mentir a l'ecran.
    return ( m_levelStore != nullptr ) ? m_levelStore->storedNamedIntervalQuestionShare() : 60;
}

void ExerciseSessionController::setNamedIntervalQuestionShare( int p_share )
{
    if( m_levelStore == nullptr )
    {
        return;
    }

    // Une part est un pourcentage : hors bornes, c'est une faute de frappe, pas un reglage.
    if( p_share < 0 || p_share > 100 )
    {
        return;
    }

    m_levelStore->storeNamedIntervalQuestionShare( p_share );

    // La session SUIVANTE prend la nouvelle part ; une session en cours garde ses propres regles.
    m_settings.namedIntervalQuestionShare = p_share;

    emit namedIntervalQuestionShareChanged();
}

int ExerciseSessionController::foreignNoteQuestionShare() const
{
    // ZERO quand il n'y a pas de profil : le defaut du domaine et celui du fichier de reglages disent la meme chose,
    // sinon l'un des trois finirait par mentir a l'ecran.
    return ( m_levelStore != nullptr ) ? m_levelStore->storedForeignNoteQuestionShare() : 0;
}

void ExerciseSessionController::setForeignNoteQuestionShare( int p_share )
{
    if( m_levelStore == nullptr )
    {
        return;
    }

    if( p_share < 0 || p_share > 100 )
    {
        return;
    }

    m_levelStore->storeForeignNoteQuestionShare( p_share );

    // La session SUIVANTE prend la nouvelle part ; une session en cours garde ses regles.
    m_settings.foreignNoteQuestionShare = p_share;

    emit foreignNoteQuestionShareChanged();
}

int ExerciseSessionController::phraseTempoBpm() const
{
    // Soixante-douze quand il n'y a pas de profil : le defaut du domaine, celui du fichier de reglages et celui du
    // contenu de l'atelier disent tous les trois la meme chose, sinon l'un des trois finirait par mentir a l'ecran.
    return ( m_levelStore != nullptr ) ? m_levelStore->storedPhraseTempoBpm() : 72;
}

int ExerciseSessionController::phraseTempoVariation() const
{
    return ( m_levelStore != nullptr ) ? m_levelStore->storedPhraseTempoVariation() : 20;
}

void ExerciseSessionController::setPhraseTempoBpm( int p_bpm )
{
    if( m_levelStore == nullptr )
    {
        return;
    }

    // Les bornes du domaine : sous quarante la phrase traine, au-dessus de cent soixante elle n'en est plus une.
    if( p_bpm < 40 || p_bpm > 160 )
    {
        return;
    }

    m_levelStore->storePhraseTempoBpm( p_bpm );

    emit phraseTempoChanged();
}

void ExerciseSessionController::setPhraseTempoVariation( int p_variation )
{
    if( m_levelStore == nullptr )
    {
        return;
    }

    // Une amplitude negative n'est pas un tirage plus petit, c'est une erreur ; et au-dela de quarante, la phrase
    // change de tempo en cours d'ecoute plutot que de varier d'une fois a l'autre.
    if( p_variation < 0 || p_variation > 40 )
    {
        return;
    }

    m_levelStore->storePhraseTempoVariation( p_variation );

    emit phraseTempoChanged();
}

void ExerciseSessionController::setSingQuestionShare( int p_share )
{
    if( m_levelStore == nullptr )
    {
        return;
    }

    // A share is a percentage: outside the range it is a typo, not a setting.
    if( p_share < 0 || p_share > 100 )
    {
        return;
    }

    m_levelStore->storeSingQuestionShare( p_share );

    // The NEXT session takes the new share; a session already running keeps its own rules.
    m_settings.singQuestionShare = p_share;

    emit singQuestionShareChanged();
}

int ExerciseSessionController::chordQuestionShare() const
{
    return ( m_levelStore != nullptr ) ? m_levelStore->storedChordQuestionShare() : 20;
}

void ExerciseSessionController::setChordQuestionShare( int p_share )
{
    if( m_levelStore == nullptr )
    {
        return;
    }

    if( p_share < 0 || p_share > 100 )
    {
        return;
    }

    m_levelStore->storeChordQuestionShare( p_share );

    m_settings.chordQuestionShare = p_share;

    emit chordQuestionShareChanged();
}

int ExerciseSessionController::modeColourQuestionShare() const
{
    return ( m_levelStore != nullptr ) ? m_levelStore->storedModeColourQuestionShare() : 0;
}

int ExerciseSessionController::modeNameQuestionShare() const
{
    return ( m_levelStore != nullptr ) ? m_levelStore->storedModeNameQuestionShare() : 0;
}

int ExerciseSessionController::modeVampQuestionShare() const
{
    return ( m_levelStore != nullptr ) ? m_levelStore->storedModeVampQuestionShare() : 0;
}

void ExerciseSessionController::setModeVampQuestionShare( int p_share )
{
    if( ( m_levelStore == nullptr ) || ( p_share < 0 ) || ( p_share > 100 ) )
    {
        return;
    }

    m_levelStore->storeModeVampQuestionShare( p_share );

    m_settings.modeVampQuestionShare = p_share;

    emit modeQuestionShareChanged();
}

void ExerciseSessionController::setModeColourQuestionShare( int p_share )
{
    if( ( m_levelStore == nullptr ) || ( p_share < 0 ) || ( p_share > 100 ) )
    {
        return;
    }

    m_levelStore->storeModeColourQuestionShare( p_share );

    m_settings.modeColourQuestionShare = p_share;

    emit modeQuestionShareChanged();
}

void ExerciseSessionController::setModeNameQuestionShare( int p_share )
{
    if( ( m_levelStore == nullptr ) || ( p_share < 0 ) || ( p_share > 100 ) )
    {
        return;
    }

    m_levelStore->storeModeNameQuestionShare( p_share );

    m_settings.modeNameQuestionShare = p_share;

    emit modeQuestionShareChanged();
}

void ExerciseSessionController::listenToTarget()
{
    if( m_session == nullptr )
    {
        return;
    }

    // For a non-beginner, hearing the target first is help: it counts as a replay, and a replay reduces the
    // experience earned. A beginner hears the target automatically, without penalty.
    if( !isBeginner() )
    {
        m_session->registerReplay();
    }

    if( m_microphone != nullptr )
    {
        m_microphone->playSingingTarget();
    }
}

void ExerciseSessionController::resetProfile()
{
    if( m_levelStore == nullptr )
    {
        return;
    }

    // The score is wiped: experience, sessions and stars back to zero. The name and the level stay - they are
    // choices, not a score.
    m_levelStore->storeTotalExperience( 0 );
    m_levelStore->storeSessionCount( 0 );
    m_levelStore->storeStarCount( 0 );

    // ET CE QUE LE BILAN A LAISSE : les titres et les trophees sont un score comme un autre, et « remise a zero » les
    // efface avec le reste. Le joueur qui repart de zero ne garde pas un titre qu'il vient d'effacer.
    m_levelStore->storeBilanCount( 0 );
    m_levelStore->storePerfectBilanCount( 0 );
    m_levelStore->storeBilanSuccessStreak( 0 );
    m_levelStore->storeLongestBilanSuccessStreak( 0 );

    // ET LES STATISTIQUES AUSSI. Un score remis a zero qui garderait son journal serait un demi-mensonge : la page de
    // statistiques continuerait de raconter une histoire que le joueur vient d'effacer.
    if( m_questionLog != nullptr )
    {
        m_questionLog->clear();
    }

    // Et un bilan en cours n'a plus de plan a suivre : la remise a zero le referme.
    m_isReviewRunning = false;
    m_reviewEasyQuestionCount = 0;

    emit totalExperienceChanged();
    emit sessionChanged();
    emit statisticsChanged();
}

bool ExerciseSessionController::isBeginner() const noexcept
{
    return !m_playerLevel.has_value() || ( *m_playerLevel == domain::PlayerLevel::Beginner );
}

int ExerciseSessionController::reminderHour() const noexcept
{
    // Le profil porte l'heure ; sans profil - un test - on rend la meme valeur par defaut que le domaine, sinon l'un des
    // deux finirait par mentir a l'ecran.
    return ( m_levelStore != nullptr ) ? m_levelStore->storedReminderMoment().hour : 19;
}

int ExerciseSessionController::reminderMinute() const noexcept
{
    return ( m_levelStore != nullptr ) ? m_levelStore->storedReminderMoment().minute : 0;
}

void ExerciseSessionController::setReminderHour( int p_hour )
{
    if( m_levelStore == nullptr )
    {
        return;
    }

    // On RAMENE l'heure dans une journee au lieu de la refuser, et le bornage vit dans le DOMAINE : c'est une regle, pas
    // une precaution d'ecran. Un ecran qui se tromperait ne doit pas priver le joueur de son rappel.
    const std::int32_t minute = m_levelStore->storedReminderMoment().minute;

    m_levelStore->storeReminderMoment( domain::clampedReminderMoment( domain::ReminderMoment{ p_hour, minute } ) );

    // Le signal est ce qui fait REPROGRAMMER l'application : une heure changee qui n'atteindrait pas les alarmes serait
    // un reglage qui ne fait rien.
    emit dailyReminderChanged();
}

void ExerciseSessionController::setReminderMinute( int p_minute )
{
    if( m_levelStore == nullptr )
    {
        return;
    }

    const std::int32_t hour = m_levelStore->storedReminderMoment().hour;

    m_levelStore->storeReminderMoment( domain::clampedReminderMoment( domain::ReminderMoment{ hour, p_minute } ) );

    emit dailyReminderChanged();
}

void ExerciseSessionController::testReminder()
{
    emit testReminderRequested();
}

void ExerciseSessionController::requestNotificationPermission()
{
    emit notificationPermissionRequested();
}

void ExerciseSessionController::refreshChoices()
{
    if( m_session == nullptr )
    {
        return;
    }

    // Pour une question CHANTEE, la cible du micro doit etre celle de la session : on la lui confie a chaque
    // nouvelle question. Sans cette synchronisation, l'accordeur jugerait contre sa propre cible.
    if( ( m_microphone != nullptr ) && ( m_session->currentQuestion().kind == domain::QuestionKind::Sing ) )
    {
        m_microphone->setSingingTarget( m_session->currentQuestion().target.semitones() );
    }

    QVariantList choices;

    for( const domain::Interval & choice : m_session->currentQuestion().choices )
    {
        choices.append( describeInterval( choice ) );
    }

    m_choices = std::move( choices );

    emit questionChanged();
}

// ---------------------------------------------------------------------------------------------------------------------
// La boucle de rythme
//
// Deux mesures, et elles ne font pas la meme chose : la premiere fait ECOUTER la cellule, la seconde la fait
// REPRODUIRE. Puis on recommence, jusqu'a ce que la cellule soit juste - c'est tout l'exercice, et il ne demande au
// joueur aucun geste en dehors de ses frappes.
// ---------------------------------------------------------------------------------------------------------------------

void ExerciseSessionController::startRhythmQuestion()
{
    if( !isRhythmQuestion() )
    {
        return;
    }

    // Rien n'a encore ete frappe sur cette question : -1, et non 0, qui est deja un Miss.
    m_rhythmLastQuality = NO_RHYTHM_TAP;
    m_rhythmCoveredOnsets = 0;

    const double beatMs = domain::beatDurationMs( static_cast<double>( m_session->currentQuestion().bpm ) );

    if( beatMs <= 0.0 )
    {
        // Un tempo nul est un reglage fautif, pas une question a jouer : la question reste posee, la boucle ne bat
        // simplement pas. Le domaine refuse les frappes pour la meme raison, donc les deux disent la meme chose.
        m_rhythmIsPlaying = false;
        m_rhythmBeatIndex = 0;
        m_rhythmTimer.stop();

        emit rhythmStateChanged();

        return;
    }

    // L'ecoute commence, et c'est ELLE qui arme le premier battement : le demarrage et la marche passent donc par le
    // meme chemin, et il n'existe pas deux endroits qui planifient le temps.
    beginRhythmListening();

    emit rhythmStateChanged();
}

void ExerciseSessionController::beginRhythmListening()
{
    m_rhythmIsPlaying = false;
    m_rhythmBeatIndex = 0;

    // L'horloge repart ici : c'est le premier temps de la mesure d'ecoute. La reproduction repartira la sienne, parce
    // que la position d'une frappe se compte depuis le premier temps de la CELLULE, pas depuis le debut de la question.
    m_rhythmClock.restart();

    // Le premier temps sonne tout de suite : une mesure qui attendrait un battement de timer pour commencer decalerait
    // tout ce qui suit.
    playRhythmHitsForBeat( 0 );

    scheduleNextRhythmBeat();
}

void ExerciseSessionController::beginRhythmPlaying()
{
    m_rhythmIsPlaying = true;
    m_rhythmBeatIndex = 0;
    m_rhythmClock.restart();

    // La tentative commence : l'ardoise affichee repart de zero, et la derniere frappe avec elle.
    //
    // ICI plutot qu'a l'ecoute, et c'est ce qui laisse au joueur le temps de lire son "trois sur quatre" pendant que
    // la cellule se rejoue apres un rate - le temps de comprendre ce qui a manque.
    m_rhythmCoveredOnsets = 0;
    m_rhythmLastQuality = NO_RHYTHM_TAP;

    // Le clic du premier temps, tout de suite : c'est le repere que le joueur attend, et le retarder d'un battement
    // decalerait toute la mesure.
    m_notePlayer.playMetronomeClick( true );

    scheduleNextRhythmBeat();
}

void ExerciseSessionController::finishRhythmLoop()
{
    if( m_session == nullptr )
    {
        return;
    }

    // Le detail de la tentative est lu AVANT le verdict : le domaine remet son ardoise a zero en jugeant, et l'ecran a
    // besoin de savoir combien de frappes sur combien ont ete touchees pour pouvoir le dire.
    m_rhythmCoveredOnsets = coveredOnsetCount();

    const bool isCorrect = m_session->endRhythmLoop();

    if( isCorrect )
    {
        // La question est close : la cellule est rejouee UNE fois en confirmation, et la boucle s'arrete. La suite
        // appartient a la pause de l'ecran, qui a la duree de la cellule pour laisser le son se finir.
        stopRhythmLoop();

        playRhythmModelOnce();
    }
    else
    {
        // La question reste posee, et l'ecoute qui recommence EST le feedback : le joueur reentend ce qu'il n'a pas su
        // reproduire, puis il retente. Aucun bouton a presser entre deux tentatives - c'est ce qui fait qu'un exercice
        // de rythme se JOUE au lieu de se lire.
        beginRhythmListening();
    }

    // Le score, les vies et le verdict passent par le chemin commun a toutes les questions.
    processAnswer( isCorrect );

    emit rhythmStateChanged();
}

void ExerciseSessionController::stopRhythmLoop()
{
    m_rhythmTimer.stop();

    m_rhythmIsPlaying = false;
    m_rhythmBeatIndex = 0;
}

// Le temps suivant, vise depuis LE DEBUT DE LA MESURE - et non depuis le battement precedent.
//
// Un timer repetitif repart de l'instant ou il a tire, donc chaque temps joue un peu en retard decalait tous les
// suivants, et la mesure partait a la derive sous les doigts du joueur. La
// decision elle-meme appartient au domaine (domain::planNextBeat), qui la rend PURE : un test sait donc dire, sans
// attendre une seconde, qu'un battement en retard ne decale pas ceux qui suivent.
void ExerciseSessionController::scheduleNextRhythmBeat()
{
    const domain::BeatSchedule schedule = domain::planNextBeat( static_cast<double>( rhythmBpm() ), static_cast<std::size_t>( m_rhythmBeatIndex ), static_cast<double>( rhythmElapsedMilliseconds() ) );

    // Le recalage eventuel de la grille - apres un reveil du telephone, par exemple - remonte par l'index : s'il
    // depasse la mesure, le prochain battement est celui d'une NOUVELLE mesure, et onRhythmBeat basculera de phase.
    m_rhythmBeatIndex = static_cast<int>( schedule.beatIndex );

    m_rhythmTimer.start( static_cast<int>( std::lround( schedule.delayMs ) ) );
}

void ExerciseSessionController::onRhythmBeat()
{
    if( !isRhythmQuestion() || ( m_session->state() != domain::SessionState::Asking ) )
    {
        // La question a change, ou elle a sa reponse : la boucle s'arrete d'elle-meme plutot que de continuer a battre
        // sous une autre question. C'est le filet de securite de toute la mecanique.
        stopRhythmLoop();

        emit rhythmStateChanged();

        return;
    }

    const int beatsPerBar = rhythmBeatsPerBar();

    if( beatsPerBar <= 0 )
    {
        stopRhythmLoop();

        emit rhythmStateChanged();

        return;
    }

    // LA MESURE EST FINIE, ET CE BATTEMENT EST CELUI DE LA SUIVANTE.
    //
    // C'est ici que la phase bascule, et pas au dernier temps de la mesure : le premier temps de la reproduction tombe
    // donc a l'instant ou l'ecoute aurait joue le sien. Basculer des le dernier temps joue faisait commencer chaque
    // mesure un temps trop tot - un metronome qui boite, une fois par mesure.
    if( m_rhythmBeatIndex >= beatsPerBar )
    {
        if( m_rhythmIsPlaying )
        {
            finishRhythmLoop();
        }
        else
        {
            // L'ecoute est finie : a toi. La reproduction redemarre sa propre grille, et arme son temps 0.
            beginRhythmPlaying();

            emit rhythmStateChanged();
        }

        return;
    }

    // Ce qui sonne sur ce temps : la cellule pendant l'ecoute, la pulsation pendant la reproduction.
    //
    // Le clic est MUET pendant l'ecoute, et c'est un choix : le modele doit s'entendre seul, sinon une syncope se noie
    // sous le metronome. Pendant la reproduction, au contraire, le clic est la seule reference - la cellule ne s'y
    // rejoue pas, sinon le joueur ne ferait que la suivre et il n'y aurait plus rien a reproduire.
    if( m_rhythmIsPlaying )
    {
        m_notePlayer.playMetronomeClick( m_rhythmBeatIndex == 0 );
    }
    else
    {
        playRhythmHitsForBeat( m_rhythmBeatIndex );
    }

    ++m_rhythmBeatIndex;

    // Le temps suivant est arme ici, et depuis l'origine de la mesure : un battement joue en retard repart donc d'une
    // echeance plus proche, au lieu d'ajouter son retard a toute la suite.
    scheduleNextRhythmBeat();

    emit rhythmStateChanged();
}

void ExerciseSessionController::playRhythmHitsForBeat( int p_beatInBar )
{
    if( m_session == nullptr )
    {
        return;
    }

    const domain::Question & question = m_session->currentQuestion();

    const double beatMs = domain::beatDurationMs( static_cast<double>( question.bpm ) );

    if( beatMs <= 0.0 )
    {
        return;
    }

    const auto barBeat = static_cast<double>( p_beatInBar );

    for( const domain::RhythmHit & hit : patternOf( question ).hits() )
    {
        if( ( hit.beat < barBeat ) || ( hit.beat >= barBeat + 1.0 ) )
        {
            continue;
        }

        const int delayMs = static_cast<int>( std::lround( ( hit.beat - barBeat ) * beatMs ) );

        if( delayMs <= 0 )
        {
            m_notePlayer.playDrum( hit.drum );

            continue;
        }

        // Une frappe decalee part en differe : c'est ce qu'est une syncope - une frappe ENTRE deux temps. Le
        // controleur est le contexte du tir, donc il vit assez longtemps pour l'entendre.
        QTimer::singleShot( delayMs, this, [this, drum = hit.drum]() { m_notePlayer.playDrum( drum ); } );
    }
}

void ExerciseSessionController::playRhythmModelOnce()
{
    if( m_session == nullptr )
    {
        return;
    }

    // Toute la cellule d'un coup : les frappes du premier temps partent maintenant, les autres en differe. C'est
    // exactement le chemin de la mesure d'ecoute, sans le timer de la boucle.
    for( int beat = 0; beat < rhythmBeatsPerBar(); ++beat )
    {
        playRhythmHitsForBeat( beat );
    }
}

double ExerciseSessionController::rhythmPositionInBeats() const noexcept
{
    if( m_session == nullptr )
    {
        return 0.0;
    }

    const double beatMs = domain::beatDurationMs( static_cast<double>( m_session->currentQuestion().bpm ) );

    if( beatMs <= 0.0 )
    {
        return 0.0;
    }

    return static_cast<double>( rhythmElapsedMilliseconds() ) / beatMs;
}

int ExerciseSessionController::coveredOnsetCount() const noexcept
{
    if( m_session == nullptr )
    {
        return 0;
    }

    const std::vector<bool> & covered = m_session->currentQuestion().coveredOnsets;

    return static_cast<int>( std::ranges::count( covered, true ) );
}

void ExerciseSessionController::tapRhythm()
{
    if( ( m_session == nullptr ) || !isRhythmQuestion() )
    {
        return;
    }

    // Le doigt s'entend TOUJOURS, meme quand la frappe ne vaut rien : sans ce son, taper donnerait l'impression que
    // l'ecran n'a pas recu le geste, et le joueur recommencerait a taper pour rien.
    m_notePlayer.playDrum( domain::Drum::Snare );

    if( !m_rhythmIsPlaying )
    {
        // Pendant l'ecoute, une frappe sonne et n'est pas jugee : celui qui accompagne la cellule ne perd pas une vie
        // pour l'avoir suivie.
        return;
    }

    // La mesure est prise ICI, et le jugement appartient au domaine : c'est la seule repartition possible entre une
    // horloge et des regles pures.
    m_rhythmLastQuality = screenQualityOf( m_session->registerRhythmTap( rhythmPositionInBeats() ) );

    // Et l'ardoise se relit tout de suite, pour que l'ecran montre la tentative se construire frappe apres frappe
    // plutot que d'attendre le verdict pour en dire un mot.
    m_rhythmCoveredOnsets = coveredOnsetCount();

    emit rhythmStateChanged();
}

void ExerciseSessionController::playCurrentQuestion()
{
    if( m_session == nullptr )
    {
        return;
    }

    const domain::Question & question = m_session->currentQuestion();

    // Une question de rythme ne se joue pas, elle se BOUCLE : sa cellule s'ecoute puis se reproduit. Le domaine n'a
    // donc rien a faire entendre ici, et c'est la boucle qui s'en charge.
    if( question.kind == domain::QuestionKind::Rhythm )
    {
        startRhythmQuestion();

        return;
    }

    // Un accord ne se joue pas comme un intervalle : ses notes sont PLAQUEES, et la question est de reconnaitre leur
    // couleur, pas de suivre une melodie.
    if( question.kind == domain::QuestionKind::Chord )
    {
        playChordNotes( question.chord );

        return;
    }

    // La NOTE ETRANGERE se joue comme un mode qui monte : les sept notes de la gamme, une par degre, avec l'intrus a la
    // place de l'une d'elles. Meme fonction du port, meme bourdon - parce que c'est la meme oreille qui ecoute.
    if( question.kind == domain::QuestionKind::ForeignNote )
    {
        playForeignNoteQuestion();

        return;
    }

    // Une question d'harmonie, elle, se joue en DEUX temps : le mode entendu AVANT, puis le mode pose. C'est la
    // comparaison qui est la question, donc c'est la SUITE qui compte - et c'est playModeQuestion qui la gere, avec le
    // minuteur qui pose le second.
    if( ( question.kind == domain::QuestionKind::ModeColour ) || ( question.kind == domain::QuestionKind::ModeName )
        || ( question.kind == domain::QuestionKind::ModeVamp ) )
    {
        playModeQuestion( false );

        return;
    }

    // Toute autre question met fin a la boucle de rythme, et c'est ce qui garantit qu'une cellule ne continue pas a
    // battre sous une question d'intervalle.
    stopRhythmLoop();

    // Une question chantee n'est pas jouee d'office, sauf pour un debutant : les autres ont le bouton "Ecouter",
    // et s'en servir coute de l'experience (voir listenToTarget).
    if( ( question.kind == domain::QuestionKind::Sing ) && !isBeginner() )
    {
        return;
    }

    const domain::Note rootNote{ question.rootMidiNumber };
    const domain::Note upperNote = rootNote.transposedBy( question.target.semitones() );

    if( question.direction == domain::IntervalDirection::Harmonic )
    {
        const std::array<domain::Note, 2> notes{ rootNote, upperNote };

        m_notePlayer.playChord( notes );

        return;
    }

    if( question.direction == domain::IntervalDirection::Descending )
    {
        // A falling interval is the SAME interval: only the order of the two notes changes. Playing
        // them in the wrong order would change what is heard, and the interval the player is asked to
        // name would no longer be the one the domain named.
        const std::array<domain::Note, 2> descendingNotes{ upperNote, rootNote };

        m_notePlayer.playMelody( descendingNotes, m_session->settings().melodicGap );

        return;
    }

    const std::array<domain::Note, 2> ascendingNotes{ rootNote, upperNote };

    m_notePlayer.playMelody( ascendingNotes, m_session->settings().melodicGap );
}

void ExerciseSessionController::playPhraseQuestion( const domain::Phrase & p_phrase )
{
    // Le TEMPO vient du reglage du joueur, exactement comme au banc d'essai : c'est le meme reglage, et il porte sur les
    // phrases des modes - Roger l'a demande la-bas, et il n'y a aucune raison qu'une meme phrase change de vitesse selon
    // la page qui la joue.
    //
    // SANS la variation aleatoire, en revanche, et c'est une difference VOULUE : le banc d'essai tire un tempo autour du
    // centre pour casser la monotonie d'une ecoute libre, tandis qu'ici le joueur peut REECOUTER la question autant de
    // fois qu'il veut. Deux tempos pour la meme question feraient douter de ce qu'on vient d'entendre - et la question
    // porte sur la COULEUR du mode, jamais sur la vitesse.
    const std::int32_t bpm = ( m_levelStore != nullptr ) ? m_levelStore->storedPhraseTempoBpm() : FALLBACK_PHRASE_TEMPO;

    // Comment la phrase se joue - sa melodie, la duree de chaque pas, son bourdon - vient du DOMAINE, et du meme endroit
    // que le banc d'essai : c'est ce qui garantit les deux musiques identiques.
    const domain::PhrasePlayback playback = domain::playbackOf( p_phrase, bpm );

    m_notePlayer.playPhraseOverDrone( playback.melody, playback.durations, playback.drone, PHRASE_NOTE_GAP );
}

void ExerciseSessionController::playModeQuestion( bool p_secondOnly )
{
    if( m_session == nullptr )
    {
        return;
    }

    // TOUTE nouvelle lecture ANNULE le second temps programme.
    //
    // Roger : « la reponse rejoue n'importe quoi, soit deux fois la derniere soit une fois la derniere avec de longues
    // pauses ». Le minuteur n'etait jamais arrete : une mauvaise reponse rejoue la question - donc le PREMIER mode, et
    // reprogramme le second - pendant que le minuteur de la question precedente attendait encore son heure, et faisait
    // sonner « la derniere » par-dessus. Arreter ici, en tete, couvre tous les chemins : la reponse, l'ecoute a nouveau,
    // et la question suivante.
    m_modeTimer.stop();

    const domain::Question & question = m_session->currentQuestion();

    // Une MELODIE, quand la question en porte une : sur une question de NOM, le mode s'entend dans une PHRASE du contenu
    // au lieu d'une gamme qui monte. C'est ce que les trois cents phrases de l'atelier attendaient, et c'est le domaine
    // qui a decide de ce qu'elle fait entendre.
    //
    // Le second passage d'une comparaison n'arrive jamais ici : une phrase ne se pose que sur une question de nom.
    if( !p_secondOnly && question.modePhrase.has_value() )
    {
        playPhraseQuestion( *question.modePhrase );

        return;
    }

    // Quel mode sonne : celui de la question, ou celui qui vient d'etre entendu avant lui. Sur une question de NOM il
    // n'y a qu'un mode, et c'est celui de la question.
    const bool hasPrevious = question.previousMode.has_value();

    // Le SECOND passage d'une comparaison demande le MEME TIMBRE que le premier : deux modes ont des notes differentes -
    // et un vamp deplace meme le bourdon - donc l'adaptateur les aurait joues avec deux instruments, et la difference de
    // son se mele a la difference de couleur. Roger : « ca evite le bruit de la difference d'instrument ».
    if( p_secondOnly && hasPrevious )
    {
        m_notePlayer.holdTimbre();
    }

    const domain::Mode mode = ( p_secondOnly || !hasPrevious ) ? question.mode : *question.previousMode;

    // Et la TONIQUE qui va avec, qui n'est pas la meme sur un vamp : les deux passages y ont les memes notes, donc deux
    // centres differents. Prendre toujours celle de la question ferait entendre deux fois le meme accord.
    const domain::Note tonic = ( p_secondOnly || !hasPrevious ) ? question.modeTonic : question.previousModeTonic;

    // La gamme MONTE, et rien de plus.
    //
    // Le banc d'essai la fait monter ET descendre, parce qu'on y ecoute une couleur a loisir. Ici une question doit tenir
    // en quelques secondes, et c'est le BOURDON qui donne le centre - pas le retour sur la tonique. Monter suffit donc a
    // faire entendre la note qui colore le mode.
    //
    // La melodie est posee DEUX OCTAVES au-dessus de la tonique : le bourdon tient les graves, et une melodie qui
    // partagerait son octave se battrait avec lui au lieu de se poser dessus.
    //
    // ET ELLE SE REFERME : le septieme degre appelle la tonique, donc la gamme revient sur elle, une octave plus haut.
    // Sans cette note, la question restait en l'air - Roger : « est-ce que ce n'est pas mieux de boucler en entier et de
    // revenir sur le 1er ? » - et c'est la derniere note qui NOMME le centre du mode, la ou le bourdon le donne.
    const std::vector<domain::Note> melody =
      domain::notesOfModeClosingOnTonic( tonic.transposedBy( MODE_MELODY_OCTAVE_OFFSET ), mode );

    // Le bourdon : la tonique tenue, et sa QUINTE. La MEME pour les deux modes d'une question de couleur, et c'est
    // exactement ce qui les rend comparables - tandis qu'un vamp la DEPLACE, parce que c'est le centre qui y change.
    const std::array<domain::Note, 2> drone{ tonic, tonic.transposedBy( FIFTH_IN_SEMITONES ) };

    const domain::DroneFraming framing{ MODE_LEAD_IN, MODE_TAIL };

    m_notePlayer.playMelodyOverDrone( melody, drone, MODE_NOTE_DURATION, MODE_NOTE_GAP, framing );

    // LA ROUE S'ANIME : l'ecran part de la tonique et parcourt la gamme, de note en note. Le signal est emis ICI, au
    // moment ou le son part - donc le dessin et la musique commencent ensemble, sans un decalage que rien n'expliquerait.
    emit modePlaybackStarted();

    // Sur une question de COULEUR, le second mode est programme pour quand le premier a fini de sonner.
    //
    // La duree vient du DOMAINE, et n'est pas recalculee ici : c'est la seule facon de garantir que le minuteur ne coupe
    // pas le son en deux, ou ne laisse pas un trou avant le second mode.
    if( !p_secondOnly && hasPrevious )
    {
        const auto modeDuration =
          domain::droneDurationFor( domain::DEGREE_COUNT, MODE_NOTE_DURATION, MODE_NOTE_GAP, framing );

        m_modeTimer.start( static_cast<int>( ( modeDuration + MODE_COMPARISON_GAP ).count() ) );
    }
}

void ExerciseSessionController::playChordNotes( const domain::Chord & p_chord )
{
    // Les notes viennent du domaine, l'accord entier d'un coup : c'est la COULEUR qu'on fait entendre, pas une suite
    // de notes. Un span se construit sur le vecteur, donc rien n'est copie de plus.
    const std::vector<domain::Note> notes = p_chord.notes();

    m_notePlayer.playChord( notes );
}

void ExerciseSessionController::recordCurrentQuestion( bool p_wasCorrect, bool p_wasRevealed )
{
    if( ( m_questionLog == nullptr ) || ( m_session == nullptr ) )
    {
        // Aucun journal : l'application tourne sans statistiques, et c'est un mode valide. Un test, un appareil dont le
        // disque est plein - dans tous les cas, le jeu continue.
        return;
    }

    const domain::Question & question = m_session->currentQuestion();

    domain::QuestionRecord record;

    // L'horloge est lue ICI, dans la couche qui a le droit de la lire. Le domaine recoit une date, il ne la demande
    // jamais : c'est ce qui garde le domaine pur, et le journal testable sans attendre une seule seconde.
    record.askedAt = std::chrono::system_clock::now();

    record.kind = question.kind;

    // Ce que la question DEMANDAIT, dans l'unite de son genre : le genre dit comment lire ce nombre. Une seule colonne
    // pour trois unites, parce que c'est le genre - et non le nombre - qui porte le sens.
    switch( question.kind )
    {
        case domain::QuestionKind::Rhythm:
            record.target = static_cast<std::int32_t>( question.patternIndex );
            break;

        case domain::QuestionKind::Chord:
            record.target = static_cast<std::int32_t>( question.chord.quality );
            break;

        case domain::QuestionKind::ModeColour:
        case domain::QuestionKind::ModeName:
        case domain::QuestionKind::ModeVamp:
            // Pour les trois questions d'harmonie, la cible est l'INDEX du mode pose.
            //
            // Le genre dit laquelle des deux, et c'est ce qui permettra aux statistiques de separer « entendre une
            // couleur » de « savoir la nommer » - deux competences distinctes, et deux facons distinctes d'echouer.
            record.target = static_cast<std::int32_t>( domain::modeIndex( question.mode ) );
            break;

        case domain::QuestionKind::NamedInterval:
        case domain::QuestionKind::Direction:
        case domain::QuestionKind::Sing:
            record.target = question.target.semitones();
            break;
    }

    record.direction = question.direction;

    // Le nombre d'essais : la bonne reponse donnee a l'essai numero un, c'est UN essai. Le domaine compte les erreurs,
    // donc l'essai gagnant s'y ajoute - et c'est cette addition qui fait la difference entre « il sait » et « il a fini
    // par trouver ».
    record.attemptCount = question.wrongAttemptCount + 1;

    record.replayCount = question.replayCount;

    record.outcome = domain::outcomeOf( p_wasCorrect, p_wasRevealed, record.attemptCount );

    m_questionLog->append( record );
}

void ExerciseSessionController::startReviewSession()
{
    domain::SessionSettings settings = m_settings;

    // LE BILAN NE PAIE PAS non plus. Roger : « l'expérience ne sera accessible qu'en salle d'arcade ». Sa recompense est
    // ailleurs - des titres, des trophees - et le mettre ici, une fois, couvre les DEUX chemins de cette fonction (le plan
    // normal, et le repli sur une partie ordinaire quand il n'y a rien a reviser).
    m_gameMode = domain::GameMode::Review;

    std::vector<domain::QuestionTarget> plan = reviewPlan();

    // Un genre FERME par le joueur n'entre pas dans un bilan.
    //
    // Ses reglages disent ce qu'il veut travailler, et un bilan qui les ignore lui poserait exactement les questions qu'il
    // a refusees - c'est ce que Roger a vu : « j'ai beau mettre plus clair et plus sombre a 0, je l'obtiens toujours dans
    // mes parties ». Le bilan ne passe pas par le tirage : il IMPOSE son plan, et il oubliait donc les parts.
    std::erase_if( plan, [&settings]( const domain::QuestionTarget & p_target ) {
        return !domain::isKindOpen( settings, p_target.kind );
    } );

    if( plan.empty() )
    {
        // Rien a reviser : pas de journal, ou trop peu de matiere pour construire un « facile puis difficile ». Le bilan
        // devient alors une partie ordinaire, ce qui vaut mieux qu'un bouton qui refuse de s'ouvrir.
        m_isReviewRunning = false;
        m_reviewEasyQuestionCount = 0;

        beginSession( settings );

        return;
    }

    settings.plannedQuestions = plan;
    settings.questionCount = plan.size();

    // Un bilan ne se PERD pas : il se termine. Le joueur vient voir ou il en est, et rater trois questions de suite le
    // renverrait chez lui avant la fin - exactement l'inverse d'un bilan.
    settings.lives = std::nullopt;

    // Et les aides restent : un bilan est un examen de passage, jamais un couperet.
    settings.aidsAllowed = true;

    // L'echauffement, c'est la premiere tranche du plan, bornee : au-dela, la difficulte commence - et c'est ce qui
    // permet a l'encouragement d'arriver au bon moment.
    m_reviewEasyQuestionCount = std::max<std::size_t>( 1, std::min( REVIEW_EASY_QUESTION_COUNT, plan.size() / 2 ) );
    m_isReviewRunning = true;

    // ET LES QUESTIONS ATTENDENT. C'est la page d'ouverture qui ouvre le bilan, pas le bouton du menu.
    //
    // Le joueur lit d'abord CE QUE L'APP SAIT DE LUI - ses points forts, ses points faibles, et pourquoi on va lui poser
    // ces questions-la. Roger l'a demande comme une PORTE plutot qu'un ecran de plus : « le bilan, en montrant les points
    // forts et les points faibles, serait une super porte d'acces vers ce cote plus academique ».
    //
    // Rien ne commence donc avant qu'il ait lu. Une premiere question jouee DERRIERE une page d'explication serait une
    // question perdue : il l'entendrait sans la regarder, et le bilan s'ouvrirait sur un echec qu'il n'a pas joue.
    m_reviewOpeningVisible = true;
    m_pendingReviewSettings = settings;

    emit sessionChanged();
}

bool ExerciseSessionController::isReviewOpeningVisible() const noexcept
{
    return m_reviewOpeningVisible;
}

void ExerciseSessionController::beginReviewQuestions()
{
    if( !m_reviewOpeningVisible )
    {
        return;
    }

    m_reviewOpeningVisible = false;

    beginSession( m_pendingReviewSettings );
}

void ExerciseSessionController::cancelReviewOpening()
{
    if( !m_reviewOpeningVisible )
    {
        return;
    }

    // Refermer la page, c'est renoncer au bilan - et RIEN n'a ete compte : aucune question n'a ete posee, aucun essai
    // n'est entre au journal. Le bouton peut donc etre repris plus tard sans laisser de trace, ce qui est le minimum
    // quand une page propose de commencer quelque chose.
    m_reviewOpeningVisible = false;
    m_isReviewRunning = false;
    m_reviewEasyQuestionCount = 0;
    m_pendingReviewSettings = domain::SessionSettings{};

    emit sessionChanged();
}

QVariantList ExerciseSessionController::reviewStrongPoints() const
{
    return reviewPointsOf( true );
}

QVariantList ExerciseSessionController::reviewWeakPoints() const
{
    return reviewPointsOf( false );
}

QVariantList ExerciseSessionController::reviewPointsOf( bool p_strong ) const
{
    QVariantList points;

    const std::vector<domain::TargetStatistics> insights = reviewInsights();

    // D'ABORD LES CIBLES NOMMABLES, de la plus faible a la mieux reussie : la page ne parle que de celles-la, donc c'est
    // sur celles-la qu'il faut partager la place.
    //
    // C'est une CORRECTION, et c'est Roger qui l'a vue : « le Bilan me montre toujours le Dorien ». Les deux listes
    // etaient bornees par le nombre de CIBLES, puis chacune ecartait les cibles SANS NOM. Avec beaucoup d'accords rates -
    // que cette page ne nommait pas - la moitie des places se perdait en silence, et une liste de trois lignes pouvait
    // n'en montrer qu'une. Le tri vivait sur les cibles, la page vivait sur les noms, et les deux ne parlaient pas du
    // meme ensemble.
    QVariantList named;

    for( const domain::TargetStatistics & target : insights )
    {
        const QString label = targetLabel( target );

        if( label.isEmpty() )
        {
            continue;
        }

        QVariantMap point;
        point.insert( QStringLiteral( "name" ), label );
        point.insert( QStringLiteral( "percent" ), target.statistics.successPercent() );
        point.insert( QStringLiteral( "asked" ), static_cast<int>( target.statistics.questionCount ) );

        named.append( point );
    }

    // LES DEUX LISTES NE SE PARTAGENT AUCUNE CIBLE, et chacune prend au plus la moitie : le recouvrement est impossible
    // par construction. C'est la meme correction que « Octave montante 100 % » dans ce qui resiste - la meilleure cible
    // du joueur ne peut plus y apparaitre.
    // Le compte est SIGNE, comme l'index de QVariantList : melanger un std::size_t non signe et un qsizetype signe dans
    // la meme soustraction est une conversion que la norme laisse libre, et clang-tidy a raison de la refuser.
    const qsizetype listSize = std::min( static_cast<qsizetype>( REVIEW_OPENING_POINT_COUNT ), named.size() / 2 );

    for( qsizetype index = 0; index < listSize; ++index )
    {
        points.append( p_strong ? named.at( named.size() - 1 - index ) : named.at( index ) );
    }

    return points;
}

QString ExerciseSessionController::targetLabel( const domain::TargetStatistics & p_target )
{
    // LE NOM VIENT DE describeInterval, et il n'est PAS refait ici : c'est lui qui porte le francais (« Quinte juste »),
    // et deux tables de noms dans deux fichiers finiraient par se contredire devant le joueur. L'intervalle du domaine
    // se nomme en anglais - c'est le nom du modele - et c'est l'affichage qui traduit.

    switch( p_target.kind )
    {
        case domain::QuestionKind::NamedInterval:
        case domain::QuestionKind::Direction: {
            const QString name = describeInterval( domain::Interval{ p_target.target } )
                                   .value( QStringLiteral( "name" ) )
                                   .toString();

            if( name.isEmpty() )
            {
                return QString{};
            }

            const QString direction = p_target.direction == domain::IntervalDirection::Ascending ? tr( "montante" )
                                                                                                 : tr( "descendante" );

            const QString full = tr( "%1 %2" ).arg( name, direction );

            // LE GENRE EST DIT quand ce n'est pas celui de reference, et ce n'est pas un ornement. Le domaine SEPARE les
            // genres a dessein - « un joueur qui reussit l'un en ratant l'autre apprend quelque chose de lui-meme » - et
            // deux genres sont donc DEUX cibles pour la meme distance.
            //
            // Sans le dire, deux lignes portaient le meme mot avec deux chiffres differents, et Roger l'a vu tout de
            // suite : « Dorien 100 % sur 3 questions » d'un cote, « Dorien 10 questions 90 % » de l'autre. Ce n'etait
            // pas un comptage faux, c'etait un comptage muet.
            return p_target.kind == domain::QuestionKind::Direction ? tr( "%1 · sens" ).arg( full ) : full;
        }

        case domain::QuestionKind::ModeName:
            return describeMode( static_cast<domain::Mode>( p_target.target ) ).value( QStringLiteral( "name" ) ).toString();

            // ModeColour n'a PLUS de cas : la comparaison de deux modes ne fabrique plus de cible (voir
            // statisticsByTarget), donc cette page ne peut pas en recevoir une. Le `default` ci-dessous le dit mieux qu'un
            // cas qui ne s'executerait jamais.

        case domain::QuestionKind::ModeVamp:
            return tr( "%1 · contexte" )
              .arg( describeMode( static_cast<domain::Mode>( p_target.target ) ).value( QStringLiteral( "name" ) ).toString() );

        // LES ACCORDS AUSSI, et pas seulement les intervalles et les modes : Roger avait demande les deux, mais une page
        // qui ECARTE en silence tout un genre n'est pas discrete, elle est MUETTE sur une partie de ce qu'il a joue.
        //
        // C'est cette omission qui vidait ses listes : plus il ratait d'accords, plus les places se perdaient, jusqu'a
        // ne plus montrer qu'une ligne. La cible d'un accord est sa COULEUR - il n'y a pas de tonique dans un taux.
        case domain::QuestionKind::Chord:
            return chordQualityWord( static_cast<domain::ChordQuality>( p_target.target ) );

        default:
            return QString{};
    }
}

std::vector<domain::TargetStatistics> ExerciseSessionController::reviewInsights() const
{
    std::vector<domain::TargetStatistics> insights;

    if( m_questionLog == nullptr )
    {
        return insights;
    }

    const auto since = std::chrono::system_clock::now() - ( std::chrono::hours{ 24 } * REVIEW_PERIOD_DAYS );

    domain::StatisticsFilter filter;
    filter.since = since;

    insights = domain::statisticsByTarget( m_questionLog->since( since ), filter );

    // UNE CIBLE VUE UNE OU DEUX FOIS N'EST PAS UN POINT FAIBLE : c'est un hasard, et c'est exactement ce que Roger a
    // entendu - « des fois tu dis : bravo, c'etait quelque chose qui te resistait alors que pas du tout ».
    //
    // Une cible ratee une seule fois affiche 0 % de reussite, donc elle arrive EN TETE du tri par faiblesse, donc en
    // premiere ligne du bilan, avec la phrase qui va avec. Le joueur, lui, ne se souvient pas de l'avoir ratee : il ne
    // l'a pour ainsi dire jamais rencontree. L'app lui racontait alors une histoire sur lui-meme qu'elle n'avait pas
    // les moyens de connaitre.
    //
    // Le seuil est celui de la page de statistiques. La MEME regle doit valoir partout ou l'app designe ce qui resiste,
    // sinon les deux ecrans se contrediraient devant le joueur - et c'est le genre de contradiction qui apprend a ne
    // plus croire ni l'un ni l'autre.
    std::erase_if( insights, []( const domain::TargetStatistics & p_target ) {
        return p_target.statistics.questionCount < domain::MINIMUM_OBSERVATIONS_FOR_A_WEAKNESS;
    } );

    return insights;
}

std::vector<domain::QuestionTarget> ExerciseSessionController::reviewPlan() const
{
    std::vector<domain::QuestionTarget> plan;

    // Les DEUX pages du bilan lisent la meme chose : la page d'ouverture montre ces cibles au joueur, et le plan les
    // lui pose, dans l'ordre. Une seule source, donc un ecran ne peut pas annoncer autre chose que ce qui suit.
    const std::vector<domain::TargetStatistics> byTarget = reviewInsights();

    if( byTarget.size() < 4 )
    {
        // Moins de quatre cibles travaillees : il n'y a pas de « facile » et de « difficile » a opposer, seulement
        // quelques exercices. Un bilan de deux questions n'apprendrait rien au joueur sur lui-meme.
        return plan;
    }

    const auto toPlannedQuestion = []( const domain::TargetStatistics & p_target ) {
        return domain::QuestionTarget{ p_target.kind, p_target.target, p_target.direction };
    };

    // D'ABORD ce qui va bien, du meilleur au moins bon : un bilan qui commencerait par un echec serait decourageant.
    // Les cibles sont triees du PLUS FAIBLE au meilleur, donc on remonte la liste
    // par la fin.
    const std::size_t easyCount = std::min( REVIEW_EASY_QUESTION_COUNT, byTarget.size() / 2 );

    for( std::size_t index = 0; index < easyCount; ++index )
    {
        plan.push_back( toPlannedQuestion( byTarget.at( byTarget.size() - 1 - index ) ) );
    }

    // PUIS ce qui resiste, du plus faible au moins faible - et sans reprendre ce qui a servi d'echauffement.
    for( std::size_t index = easyCount; index + easyCount < byTarget.size(); ++index )
    {
        plan.push_back( toPlannedQuestion( byTarget.at( index ) ) );
    }

    return plan;
}

bool ExerciseSessionController::isCurrentQuestionAHardPart() const noexcept
{
    if( !m_isReviewRunning || ( m_session == nullptr ) )
    {
        return false;
    }

    // Le rang dans le plan : au-dela de l'echauffement, c'est ce qui resiste. Le controleeur a construit le plan dans cet
    // ordre, donc il le sait - sans recroiser les statistiques a chaque question.
    return m_session->questionNumber() > m_reviewEasyQuestionCount;
}

QString ExerciseSessionController::encouragementText() const
{
    if( !m_isReviewRunning || ( m_session == nullptr ) )
    {
        // Hors bilan, il n'y a rien a dire : un ecran qui parle pour ne rien dire devient un ecran qu'on n'ecoute plus, et
        // le silence est ce qui donne du poids aux mots qui restent.
        return QString{};
    }

    // Une reussite sur ce qui resistait : le mot juste, et il arrive au bon moment.
    if( isFeedbackVisible() && m_session->wasLastAnswerCorrect() && isCurrentQuestionAHardPart() )
    {
        return tr( "Bravo ! C'est exactement ce qui te résistait." );
    }

    // Une serie : on le dit, parce qu'une serie se sent et se dit.
    if( m_session->score().streak() >= 3 )
    {
        return tr( "Bien joué, ne lâche pas." );
    }

    // Et AVANT d'attaquer une difficulte connue, la prevenance : c'est le seul moment ou elle aide vraiment.
    if( isAsking() && isCurrentQuestionAHardPart() )
    {
        return tr( "Là, tu attaques un exercice qui t'a résisté cette semaine. Prends ton temps." );
    }

    return QString{};
}

bool ExerciseSessionController::isWeekEnd() const
{
    // QDate::dayOfWeek() suit exactement la convention du domaine : 1 = lundi ... 7 = dimanche. Cette correspondance est
    // verifiee par un test du domaine, et non par ce commentaire.
    return domain::isWeekEnd( QDate::currentDate().dayOfWeek() );
}

QVariantList ExerciseSessionController::chordTree() const
{
    QVariantList nodes;

    // La tonique de la carte : un do. L'arbre est une CARTE et non une question - il montre la FORME des accords, et la
    // tonique n'est la que pour rendre les noms lisibles. C'est aussi pour cela que les statistiques, elles, n'enregistrent
    // que la couleur : c'est elle qu'on apprend, la tonique n'etant qu'un habillage.
    const QString rootName = QString::fromStdString( domain::Note{ 60 }.pitchClassName() );

    const std::span<const domain::ChordNode> tree = domain::chordTree();

    for( const domain::ChordNode & node : tree )
    {
        QVariantMap described;

        const std::string_view suffix = domain::chordQualitySymbolSuffix( node.quality );

        described.insert( QStringLiteral( "quality" ), static_cast<int>( node.quality ) );

        described.insert( QStringLiteral( "name" ),
                          rootName + QString::fromUtf8( suffix.data(), static_cast<int>( suffix.size() ) ) );

        // Les degres : « 1 3 5 », « 1 b3 5 b7 ». C'est CE QUI APPREND quelque chose.
        described.insert( QStringLiteral( "degrees" ), QString::fromStdString( domain::chordDegreesLabel( node.quality ) ) );

        described.insert( QStringLiteral( "mutation" ),
                          QString::fromUtf8( node.mutation.data(), static_cast<int>( node.mutation.size() ) ) );

        described.insert( QStringLiteral( "depth" ), node.depth );

        described.insert( QStringLiteral( "isRoot" ), node.isRoot() );

        // L'INDEX DU PARENT, et pas seulement sa couleur : l'ecran doit tracer un trait entre deux noeuds, et il lui faut
        // deux positions. Chercher le parent lui-meme serait refaire ici un travail deja fait.
        //
        // La racine n'a pas de parent, et -1 le dit : c'est ce que l'ecran lit pour ne tracer AUCUN trait. Sans ce cas
        // particulier, elle se trouverait elle-meme et se relierait a elle-meme.
        int parentIndex = -1;

        if( !node.isRoot() )
        {
            for( std::size_t index = 0; index < tree.size(); ++index )
            {
                if( tree[index].quality == node.parent )
                {
                    parentIndex = static_cast<int>( index );

                    break;
                }
            }
        }

        described.insert( QStringLiteral( "parentIndex" ), parentIndex );

        nodes.append( described );
    }

    return nodes;
}

int ExerciseSessionController::chordQualityCount() const noexcept
{
    return static_cast<int>( domain::CHORD_QUALITY_COUNT );
}

QString ExerciseSessionController::chordColourName( int p_quality ) const
{
    const int count = chordQualityCount();

    // La teinte vient de la position dans l'ordre d'apprentissage, multipliee par 7 avant le modulo : 15 et 7 sont premiers
    // entre eux, donc les quinze couleurs visitent les quinze teintes, et deux couleurs VOISINES n'en partagent jamais une.
    // C'est ce qui fait qu'un majeur et un mineur ne se ressemblent pas, alors qu'ils se suivent dans la liste.
    const double hue = static_cast<double>( ( p_quality * 7 ) % count ) / static_cast<double>( count );

    // Une saturation moderee et une clarte elevee : le texte pose dessus est sombre, et c'est ce contraste qui rend le
    // bouton lisible d'un coup d'oeil.
    //
    // Les suffixes F et le transtypage ne sont pas cosmetiques : fromHslF prend des qreal, qui sont des FLOAT dans
    // Qt 6, et un double y serait une conversion retrecissante - ce que clang-tidy signale a juste titre
    // (bugprone-narrowing-conversions). Le calcul reste en double, ou il est plus precis, et seul le passage se fait.
    return QColor::fromHslF( static_cast<float>( hue ), 0.45F, 0.70F ).name();
}

bool ExerciseSessionController::isChordHintAvailable() const noexcept
{
    return ( m_session != nullptr ) && m_session->canRemoveOneWrongChordChoice();
}

bool ExerciseSessionController::isChordArpeggioAvailable() const noexcept
{
    return ( m_session != nullptr ) && m_session->canHearChordAsArpeggio();
}

void ExerciseSessionController::useChordHint()
{
    if( ( m_session == nullptr ) || !m_session->removeOneWrongChordChoice() )
    {
        return;
    }

    // L'ecran doit RELIRE la grille : une reponse retiree est une reponse qui disparait. La grille est reconstruite a
    // chaque lecture depuis le domaine (voir chordChoices), donc le signal suffit - l'ecran ne garde aucune copie.
    emit questionChanged();
    emit sessionChanged();
}

void ExerciseSessionController::playCurrentChordAsArpeggio()
{
    if( ( m_session == nullptr ) || !isChordQuestion() )
    {
        return;
    }

    // Une note apres l'autre, et LENTEMENT : c'est tout l'interet de l'arpege - trop rapide, il redevient un accord, et
    // l'indice ne montrerait plus rien.
    m_notePlayer.playMelody( m_session->currentQuestion().chord.notes(), ARPEGGIO_NOTE_GAP );
}

void ExerciseSessionController::playCurrentQuestionAsChord()
{
    if( m_session == nullptr )
    {
        return;
    }

    const domain::Question & question = m_session->currentQuestion();

    // Sur une question d'accord, la confirmation est l'accord lui-meme : il etait deja plaque, et le rejouer est
    // exactement ce que le joueur doit garder en tete.
    if( question.kind == domain::QuestionKind::Chord )
    {
        playChordNotes( question.chord );

        return;
    }

    // Sur une question d'harmonie, la confirmation est LE MODE POSE, rejoue seul, sur son bourdon.
    //
    // C'est la reponse, et l'entendre une seconde fois juste apres l'avoir trouvee est ce qui la fixe. Le mode d'AVANT
    // n'etait que la question : le rejouer melerait la reponse a ce qu'elle repond.
    if( ( question.kind == domain::QuestionKind::ModeColour ) || ( question.kind == domain::QuestionKind::ModeName ) )
    {
        playModeQuestion( true );

        return;
    }

    const domain::Note rootNote{ question.rootMidiNumber };
    const domain::Note upperNote = rootNote.transposedBy( question.target.semitones() );

    // The two notes TOGETHER, whatever the direction of the question was: on a correct answer the
    // player already knows which way it went, and what is left to hear is the colour.
    const std::array<domain::Note, 2> notes{ rootNote, upperNote };

    m_notePlayer.playChord( notes );
}

bool ExerciseSessionController::playWrongChordThenAnswer()
{
    if( ( m_session == nullptr ) || !isChordQuestion() )
    {
        return false;
    }

    const std::optional<domain::ChordQuality> answer = m_session->lastChordAnswer();

    if( !answer.has_value() )
    {
        // Le joueur n'a pas repondu : il a passe. Il n'y a donc pas d'accord « appuye » a lui faire entendre, et la
        // reponse seule suffit - c'est exactement ce que playCurrentQuestion fait deja.
        return false;
    }

    // La MEME tonique que la question, et la couleur que le joueur a CHOISIE : c'est ce qui rend l'ecart audible sur une
    // seule note de depart. Deux toniques differentes feraient entendre deux accords etrangers l'un a l'autre, et il n'y
    // aurait plus rien a comparer.
    const domain::Chord played{ .quality = *answer,
                                .rootMidiNumber = m_session->currentQuestion().chord.rootMidiNumber };

    m_notePlayer.playChordThenChord( played.notes(), m_session->currentQuestion().chord.notes(), WRONG_CHORD_GAP );

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// LE GODMODE
// ---------------------------------------------------------------------------------------------------------------------

std::vector<domain::Interval> ExerciseSessionController::everyIntervalChecked( bool p_checked )
{
    if( !p_checked )
    {
        return {};
    }

    const std::array<domain::Interval, domain::SUPPORTED_INTERVAL_COUNT> & all = domain::allSupportedIntervals();

    return { all.begin(), all.end() };
}

std::vector<domain::ChordQuality> ExerciseSessionController::everyChordChecked( bool p_checked )
{
    if( !p_checked )
    {
        return {};
    }

    const std::span<const domain::ChordQuality> order = domain::chordLearningOrder();

    return { order.begin(), order.end() };
}

std::vector<domain::Mode> ExerciseSessionController::everyModeChecked( bool p_checked )
{
    if( !p_checked )
    {
        return {};
    }

    const std::span<const domain::Mode> order = domain::modeLearningOrder();

    return { order.begin(), order.end() };
}

domain::GodModePalette ExerciseSessionController::orderedDraft() const
{
    return domain::orderedPalette( m_godModeDraft );
}

bool ExerciseSessionController::godModeHasUnsavedChanges() const
{
    // Les deux palettes, rangees dans le meme ordre avant d'etre comparees : sans cela, deux listes qui contiennent
    // exactement la meme chose dans un ordre different passeraient pour deux palettes differentes.
    const domain::GodModePalette draft = orderedDraft();

    return !( ( draft.intervals == m_godModeSavedPalette.intervals )
              && ( draft.chords == m_godModeSavedPalette.chords )
              && ( draft.modes == m_godModeSavedPalette.modes ) );
}

QString ExerciseSessionController::godModeProblem() const
{
    // Les PARTS du joueur, et non une regle generale : on n'exige pas deux accords de quelqu'un qui a mis la part des
    // accords a zero. C'est la meme regle que le domaine, lue avec les reglages du joueur.
    domain::SessionSettings shares;
    applyStoredQuestionShares( shares );

    const domain::GodModePalette draft = orderedDraft();

    const bool intervalsAsked = ( shares.namedIntervalQuestionShare + shares.singQuestionShare
                                  + shares.foreignNoteQuestionShare )
                                > 0;
    const bool chordsAsked = shares.chordQuestionShare > 0;
    const bool modesAsked = ( shares.modeColourQuestionShare + shares.modeNameQuestionShare
                              + shares.modeVampQuestionShare )
                            > 0;

    // Le message NOMME la famille qui manque, et dit POURQUOI : « au moins deux » sans raison ressemble a une lubie. Avec
    // un seul choix, la reponse serait toujours la meme, et le joueur repondrait juste sans ecouter.
    if( intervalsAsked && ( draft.intervals.size() < domain::MINIMUM_GOD_MODE_CHOICES ) )
    {
        return tr( "Coche au moins deux intervalles : avec un seul, la réponse serait toujours la même." );
    }

    if( chordsAsked && ( draft.chords.size() < domain::MINIMUM_GOD_MODE_CHOICES ) )
    {
        return tr( "Coche au moins deux accords : avec un seul, la réponse serait toujours la même." );
    }

    if( modesAsked && ( draft.modes.size() < domain::MINIMUM_GOD_MODE_CHOICES ) )
    {
        return tr( "Coche au moins deux modes : avec un seul, la réponse serait toujours la même." );
    }

    return {};
}

bool ExerciseSessionController::godModeCanStart() const
{
    return godModeProblem().isEmpty();
}

QVariantList ExerciseSessionController::godModeIntervals() const
{
    const domain::GodModePalette draft = orderedDraft();

    QVariantList family;

    // TOUS les intervalles du jeu sont proposes, meme ceux que le niveau du joueur n'a pas encore ouverts : c'est la
    // promesse du GodMode - il choisit, et le jeu ne lui cache rien.
    for( const domain::Interval & interval : domain::allSupportedIntervals() )
    {
        QVariantMap entry = describeInterval( interval );

        entry.insert( QStringLiteral( "semitones" ), interval.semitones() );

        entry.insert( QStringLiteral( "checked" ),
                      std::ranges::find( draft.intervals, interval ) != draft.intervals.end() );

        family.append( entry );
    }

    return family;
}

QVariantList ExerciseSessionController::godModeChords() const
{
    const domain::GodModePalette draft = orderedDraft();

    QVariantList family;

    for( const domain::ChordQuality quality : domain::chordLearningOrder() )
    {
        QVariantMap entry;

        entry.insert( QStringLiteral( "index" ), static_cast<int>( quality ) );

        // LE NOM D'UN ACCORD, en notation anglo-saxonne, comme partout ailleurs : « C », « Cm », « C7 », « Cm7b5 ». On ne
        // nomme pas la TONIQUE ici - ce serait mentir sur un choix qui ne porte que la COULEUR -, donc un « C » neutre sert
        // de repere, exactement comme dans la grille de reponse.
        const std::string_view suffix = domain::chordQualitySymbolSuffix( quality );

        entry.insert( QStringLiteral( "name" ),
                      QStringLiteral( "C" ) + QString::fromUtf8( suffix.data(), static_cast<int>( suffix.size() ) ) );

        entry.insert( QStringLiteral( "checked" ), std::ranges::find( draft.chords, quality ) != draft.chords.end() );

        family.append( entry );
    }

    return family;
}

QVariantList ExerciseSessionController::godModeModes() const
{
    const domain::GodModePalette draft = orderedDraft();

    QVariantList family;

    for( const domain::Mode mode : domain::modeLearningOrder() )
    {
        QVariantMap entry = describeMode( mode );

        entry.insert( QStringLiteral( "checked" ), std::ranges::find( draft.modes, mode ) != draft.modes.end() );

        family.append( entry );
    }

    return family;
}

void ExerciseSessionController::toggleGodModeInterval( int p_semitones )
{
    // Un intervalle s'identifie par ses DEMI-TONS, et jamais par sa place dans une liste : c'est ce qui permet de
    // sauvegarder un choix et de le relire des annees plus tard, meme si l'ordre d'apprentissage change un jour.
    if( ( p_semitones < 0 ) || ( p_semitones > domain::MAXIMUM_INTERVAL_SEMITONES ) )
    {
        return;
    }

    const domain::Interval interval{ p_semitones };

    const auto found = std::ranges::find( m_godModeDraft.intervals, interval );

    if( found == m_godModeDraft.intervals.end() )
    {
        m_godModeDraft.intervals.push_back( interval );
    }
    else
    {
        m_godModeDraft.intervals.erase( found );
    }

    emit godModeChanged();
}

void ExerciseSessionController::toggleGodModeChord( int p_qualityIndex )
{
    if( ( p_qualityIndex < 0 ) || std::cmp_greater_equal( p_qualityIndex, domain::CHORD_QUALITY_COUNT ) )
    {
        return;
    }

    const auto quality = static_cast<domain::ChordQuality>( p_qualityIndex );

    const auto found = std::ranges::find( m_godModeDraft.chords, quality );

    if( found == m_godModeDraft.chords.end() )
    {
        m_godModeDraft.chords.push_back( quality );
    }
    else
    {
        m_godModeDraft.chords.erase( found );
    }

    emit godModeChanged();
}

void ExerciseSessionController::toggleGodModeMode( int p_modeIndex )
{
    if( ( p_modeIndex < 0 ) || std::cmp_greater_equal( p_modeIndex, domain::MODE_COUNT ) )
    {
        return;
    }

    const auto mode = static_cast<domain::Mode>( p_modeIndex );

    const auto found = std::ranges::find( m_godModeDraft.modes, mode );

    if( found == m_godModeDraft.modes.end() )
    {
        m_godModeDraft.modes.push_back( mode );
    }
    else
    {
        m_godModeDraft.modes.erase( found );
    }

    emit godModeChanged();
}

void ExerciseSessionController::setEveryGodModeIntervalChecked( bool p_checked )
{
    m_godModeDraft.intervals = everyIntervalChecked( p_checked );

    emit godModeChanged();
}

void ExerciseSessionController::setEveryGodModeChordChecked( bool p_checked )
{
    m_godModeDraft.chords = everyChordChecked( p_checked );

    emit godModeChanged();
}

void ExerciseSessionController::setEveryGodModeModeChecked( bool p_checked )
{
    m_godModeDraft.modes = everyModeChecked( p_checked );

    emit godModeChanged();
}

void ExerciseSessionController::prefillGodModeFromLevel( int p_level )
{
    // Le MODELE d'un niveau, et non son etat courant : un joueur qui a deja gagne trois intervalles en jouant ne doit pas
    // les retrouver coches ici, sinon deux joueurs du meme niveau n'auraient pas le meme point de depart.
    m_godModeDraft = domain::paletteForLevel( domain::playerLevelFromIndex( static_cast<std::size_t>( p_level ) ) );

    emit godModeChanged();
}

void ExerciseSessionController::saveGodMode()
{
    // On n'ecrit JAMAIS une palette qui ne pourrait pas poser de partie : elle serait sauvegardee, puis refusee au moment
    // de jouer, sans que personne ne comprenne ce qui s'est passe. L'ecran, lui, dit deja pourquoi - voir godModeProblem.
    if( !godModeProblem().isEmpty() )
    {
        return;
    }

    const domain::GodModePalette ordered = orderedDraft();

    if( m_levelStore != nullptr )
    {
        m_levelStore->storeGodModePalette( ordered );
    }

    // Ce qui joue devient ce qui vient d'etre ecrit, et le brouillon est range dans le meme ordre : deux listes qui
    // diraient la meme chose dans deux ordres differents feraient apparaitre « non sauvegarde » juste apres avoir sauve.
    m_godModeSavedPalette = ordered;
    m_godModeDraft = ordered;
    m_godModeIsSaved = true;

    emit godModeChanged();
}

void ExerciseSessionController::applyGodModeIfChosen( domain::SessionSettings & p_settings ) const
{
    if( !m_godModeIsChosen )
    {
        return;
    }

    // La palette SAUVEGARDEE, et non le brouillon : Roger l'a dit exactement comme ca - « il peut faire ses changements
    // puis appuyer sur sauvegarder POUR POUVOIR JOUER de cette maniere ». Tant qu'il n'a pas appuye, ses clics ne changent
    // rien a ce qu'il jouera, et c'est ce qui donne son sens au mot « sauvegarde ».
    p_settings = domain::sessionSettingsFor( m_godModeSavedPalette, p_settings );
}

// ---------------------------------------------------------------------------------------------------------------------
// LA FELICITATION DE PALIER
// ---------------------------------------------------------------------------------------------------------------------

int ExerciseSessionController::invitedLevel() const
{
    return static_cast<int>( domain::levelEarnedBy( totalExperience() ) );
}

QString ExerciseSessionController::invitedLevelName() const
{
    return levelName( domain::levelEarnedBy( totalExperience() ) );
}

bool ExerciseSessionController::levelInvitationIsAvailable() const
{
    // Le GODMODE ne propose RIEN : le joueur a deja decide de choisir lui-meme, et lui offrir un palier serait lui reprendre
    // la main qu'il vient de prendre.
    if( m_godModeIsChosen )
    {
        return false;
    }

    // Le palier merite doit etre STRICTEMENT au-dessus du palier actuel. A egalite il n'y a rien a proposer, et un joueur qui
    // a choisi plus haut que son experience ne doit surtout pas se voir proposer de redescendre.
    return domain::levelEarnedBy( totalExperience() ) > m_playerLevel.value_or( domain::PlayerLevel::Beginner );
}

bool ExerciseSessionController::levelInvitationAnnounced() const
{
    if( m_levelStore == nullptr )
    {
        // Sans memoire, il n'y a pas de « premiere fois » : on n'annonce rien plutot que de repeter.
        return true;
    }

    const std::optional<domain::PlayerLevel> announced = m_levelStore->storedAnnouncedLevel();

    return announced.has_value() && ( *announced == domain::levelEarnedBy( totalExperience() ) );
}

void ExerciseSessionController::acceptLevelInvitation()
{
    // Par le MEME chemin que la liste des difficultes : c'est ce qui evite un second mecanisme a tenir a jour, et ce qui
    // garantit que le joueur obtient exactement ce qu'il aurait eu en cliquant lui-meme.
    choosePlayerLevel( invitedLevel() );
}

void ExerciseSessionController::markLevelInvitationAnnounced()
{
    if( m_levelStore != nullptr )
    {
        m_levelStore->storeAnnouncedLevel( domain::levelEarnedBy( totalExperience() ) );
    }

    emit levelInvitationChanged();
}

}    // namespace musichien::ui