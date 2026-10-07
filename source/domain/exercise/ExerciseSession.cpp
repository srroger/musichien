#include "domain/exercise/ExerciseSession.h"

#include "domain/exercise/AnswerGrid.h"
#include "domain/exercise/LearningOrder.h"
#include "domain/rhythm/RhythmPattern.h"

#include <algorithm>
#include <span>

namespace musichien::domain
{

namespace
{

// COMBIEN DE FOIS PLUS SOUVENT UN INTERVALLE ETUDIE EST TIRE.
//
// Quatre, et ce n'est pas un reglage : c'est le seul chiffre du mecanisme, et il se discute ici plutot que dans un
// fichier. Assez haut pour que le joueur REMARQUE que la lecon qu'il vient de lire sert a quelque chose - a la moitie
// des questions, on ne verrait qu'un hasard. Assez bas pour que la session ne devienne pas une redite du cours : le
// reste de la palette garde ses chances, et une entree non etudiee reste toujours atteignable.
constexpr double STUDIED_DRAW_WEIGHT = 4.0;

// La cellule d'une question rythmique.
//
// L'index vient du domaine lui-meme, donc il est valide. Le garde-fou est la pour qu'une liste de cellules qui
// changerait un jour ne transforme pas une question en exception : une cellule plutot qu'aucune, le jeu continue.
[[nodiscard]] const RhythmPattern & patternOf( const Question & p_question ) noexcept
{
    const std::vector<RhythmPattern> & patterns = allRhythmPatterns();

    const std::size_t index = std::min( p_question.patternIndex, patterns.size() - 1 );

    return patterns.at( index );
}

// Un genre de question et sa PART, dans l'ordre ou l'ecran les presente.
//
// Une table plutot qu'une suite de comparaisons : c'est ce qui permet de tirer sur la SOMME des parts (voir drawKind),
// et d'ajouter un genre plus tard sans avoir a recalculer toutes les bornes des genres qui le suivent - le defaut exact
// que la chaine de conditions portait.
struct KindShare
{
    std::int32_t share{ 0 };
    QuestionKind kind{ QuestionKind::NamedInterval };
};

}    // namespace

bool isKindOpen( const SessionSettings & p_settings, QuestionKind p_kind ) noexcept
{
    switch( p_kind )
    {
        case QuestionKind::NamedInterval:
            return p_settings.namedIntervalQuestionShare > 0;

        case QuestionKind::Direction:
            return p_settings.directionQuestionShare > 0;

        case QuestionKind::Sing:
            return p_settings.singQuestionShare > 0;

        case QuestionKind::Chord:
            return p_settings.chordQuestionShare > 0;

        case QuestionKind::ModeColour:
            return p_settings.modeColourQuestionShare > 0;

        case QuestionKind::ModeName:
            return p_settings.modeNameQuestionShare > 0;

        case QuestionKind::ModeVamp:
            return p_settings.modeVampQuestionShare > 0;

        case QuestionKind::ForeignNote:
            return p_settings.foreignNoteQuestionShare > 0;

        case QuestionKind::Rhythm:
            // Le rythme n'est plus un exercice de ce jeu : il n'est JAMAIS ouvert, et le dire ici evite qu'un plan
            // l'impose un jour - c'est le genre de question que plus rien ne pose.
            return false;
    }

    return false;
}

namespace
{

// La palette d'intervalles avec laquelle une seance DEMARRE : celle que le joueur a choisie quand il en a choisi une -
// c'est le GodMode - et sinon le prefixe de l'ordre d'apprentissage, exactement comme avant.
//
// Les trois fonctions qui suivent sont lues par la liste d'initialisation du constructeur, donc elles ne peuvent pas
// dependre de membres deja construits : elles prennent les reglages, et rien d'autre.
[[nodiscard]] std::vector<Interval> initialIntervalPalette( const SessionSettings & p_settings )
{
    if( !p_settings.intervalPalette.empty() )
    {
        return p_settings.intervalPalette;
    }

    return beginnerPalette( p_settings.startingPaletteSize );
}

[[nodiscard]] std::vector<ChordQuality> initialChordPalette( const SessionSettings & p_settings )
{
    if( !p_settings.chordPalette.empty() )
    {
        return p_settings.chordPalette;
    }

    return beginnerChordPalette( p_settings.startingChordQualityCount );
}

[[nodiscard]] std::vector<Mode> initialModePalette( const SessionSettings & p_settings )
{
    if( !p_settings.modePalette.empty() )
    {
        return p_settings.modePalette;
    }

    return beginnerModePalette( p_settings.startingModeCount );
}

}    // namespace

ExerciseSession::ExerciseSession( std::uint32_t p_seed, SessionSettings p_settings, const PhraseBook * p_phraseBook )
  : m_randomEngine{ p_seed }
  , m_settings{ std::move( p_settings ) }
  , m_palette{ initialIntervalPalette( m_settings ) }
  , m_chordPalette{ initialChordPalette( m_settings ) }
  , m_modePalette{ initialModePalette( m_settings ) }
  , m_phraseBook{ p_phraseBook }
  , m_score{ m_settings.lives }
  , m_currentQuestion{ buildQuestion() }
{
    // The first question is built HERE rather than on a start() call: a session that exists is a
    // session that is asking something, and the screen therefore never has to handle a state where
    // there is nothing to play.
    //
    // Et c'est aussi pourquoi le LIVRE DES PHRASES arrive par la liste d'initialisation : la premiere question est deja
    // construite quand ce corps s'execute, donc un livre donne apres coup ne pourrait plus rien pour elle.
}

Question ExerciseSession::buildQuestion()
{
    Question question;

    // Un PLAN decide la question, quand il y en a un : genre, cible et sens. C'est ce qui fait d'un Bilan une suite
    // DECIDEE - du plus facile au plus difficile - la ou une partie ordinaire laisse le tirage choisir. Le plan passe
    // avant tout tirage : ce qui est decide ne se retire pas.
    const std::optional<QuestionTarget> planned = plannedQuestionAt( m_questionNumber - 1 );

    // Le plan a-t-il decide la CIBLE, ou seulement le GENRE ?
    //
    // Un Bilan decide la cible elle-meme (« travaille la sixte »). L'Arcade, elle, decide le DOSAGE et laisse le detail
    // au tirage : elle porte DRAWN_TARGET, et la cible se tire comme dans une partie ordinaire.
    const bool targetIsDecided = planned.has_value() && ( planned->target != DRAWN_TARGET );

    if( planned.has_value() )
    {
        question.kind = planned->kind;
        question.direction = planned->direction;

        if( targetIsDecided )
        {
            question.target = Interval{ planned->target };
        }
    }
    else
    {
        // L'intervalle est tire EN PREMIER, meme quand la question sera rythmique : c'est l'ordre des tirages que les
        // tests existants ont appris a suivre, et une question de rythme qui ne s'en sert pas ne doit pas deplacer les
        // autres questions pour autant.
        question.target = drawTarget();

        question.kind = drawKind();
    }

    // Le plan qui n'a decide que le GENRE laisse la cible au tirage, exactement comme une partie ordinaire. Seules les
    // questions d'INTERVALLE tirent ici : un accord tire sa COULEUR, un mode tire son mode, et chacun le fait dans sa
    // propre construction, plus bas.
    if( planned.has_value() && !targetIsDecided && isIntervalQuestion( question.kind ) )
    {
        question.target = drawTarget();
    }

    if( question.kind == QuestionKind::Rhythm )
    {
        // Une question de rythme n'a ni tonique, ni sens, ni grille : une cellule, un tempo, et des frappes a couvrir.
        // Le retour est anticipe parce que tout ce qui suit ne parle QUE d'intervalles.
        buildRhythmicCell( question );

        return question;
    }

    if( question.kind == QuestionKind::Chord )
    {
        if( targetIsDecided )
        {
            // Le plan a dit la COULEUR ; la tonique reste tiree, parce qu'un bilan ne teste pas la hauteur.
            question.chord.quality = static_cast<ChordQuality>( planned->target );
            question.chord.rootMidiNumber = drawChordRootMidiNumber( question.chord.quality );
            question.chordChoices.assign( m_chordPalette.begin(), m_chordPalette.end() );

            // LA COULEUR DEMANDEE EST TOUJOURS PARMI LES CHOIX, et ce n'est pas une precaution de style.
            //
            // Le PLAN vient du JOURNAL, qui garde trente jours de questions ; la PALETTE, elle, repart du niveau a chaque
            // session. Une couleur travaillee il y a trois semaines peut donc etre demandee alors qu'elle n'est plus dans
            // la liste - et Roger l'a vu exactement comme cela : « c'etait joue le demi-diminue, sauf qu'il n'etait pas
            // disponible dans la liste des choix ».
            //
            // Une question sans sa reponse n'est pas difficile, elle est IMPOSSIBLE : le joueur a beau ecouter, aucune des
            // pastilles ne peut lui donner raison. C'est la seule regle que ce bloc doit garantir, et il la garantit
            // maintenant pour tous les chemins - le bilan d'aujourd'hui comme celui de dans six mois.
            if( std::ranges::find( question.chordChoices, question.chord.quality ) == question.chordChoices.end() )
            {
                question.chordChoices.push_back( question.chord.quality );
            }

            return question;
        }

        // Un accord a une tonique et une couleur, mais ni direction ni grille de choix tires au hasard : il a sa
        // propre palette, et sa propre facon de se repondre.
        buildChordQuestion( question );

        return question;
    }

    if( ( question.kind == QuestionKind::ModeColour ) || ( question.kind == QuestionKind::ModeName ) )
    {
        // Une question d'harmonie ne se decide pas par un plan : le Bilan ne la connait pas encore. Elle se construit
        // donc entierement ici - la tonique du bourdon, le ou les modes, et les choix de la palette du joueur.
        buildModeQuestion( question, question.kind == QuestionKind::ModeColour );

        return question;
    }

    if( question.kind == QuestionKind::ModeVamp )
    {
        // Le vamp a sa propre construction, parce qu'il a sa propre contrainte : les deux passages doivent avoir
        // EXACTEMENT les memes notes, et c'est ce qui demande de deplacer la tonique ET le mode ensemble.
        buildVampQuestion( question );

        return question;
    }

    if( question.kind == QuestionKind::ForeignNote )
    {
        // La note etrangere a la sienne : c'est la seule question dont la melodie soit DECIDEE d'avance - sept notes, une
        // par degre - et dont l'intrus soit choisi avant d'etre joue.
        buildForeignNoteQuestion( question );

        return question;
    }

    if( !targetIsDecided )
    {
        // Le SENS n'est tire que si personne ne l'a decide : un plan le porte deja.
        if( question.kind == QuestionKind::Sing )
        {
            // Une question chantee monte toujours : chanter un intervalle descendant depuis une note inconnue est un
            // autre exercice, et le premier jet chante vers le haut.
            question.direction = IntervalDirection::Ascending;
        }
        else if( question.kind == QuestionKind::Direction )
        {
            // Le mode guide demande "ca monte ou ca descend ?" : un intervalle harmonique n'a pas de sens a ce
            // moment-la, donc il est sorti du tirage.
            question.direction = ( std::uniform_int_distribution<std::int32_t>{ 0, 1 }( m_randomEngine ) == 0 )
                                   ? IntervalDirection::Ascending
                                   : IntervalDirection::Descending;
        }
        else
        {
            // The direction is drawn BEFORE the root, because the root depends on it: the room an interval
            // needs is on one side or the other.
            question.direction = drawDirection();
        }
    }

    question.rootMidiNumber = drawRootMidiNumber( question.target, question.direction );

    // The grid is built from the PALETTE and the target, and it is clamped to the palette by the
    // AnswerGrid itself: a question can never offer an interval the player has not met.
    question.choices = AnswerGrid::build( m_palette, question.target, m_settings.choiceCount, m_randomEngine );

    return question;
}

std::optional<QuestionTarget> ExerciseSession::plannedQuestionAt( std::size_t p_index ) const noexcept
{
    if( p_index >= m_settings.plannedQuestions.size() )
    {
        // Plan epuise, ou pas de plan du tout : la session reprend son tirage. C'est ce qui permet a un Bilan de finir
        // proprement, et a une partie ordinaire de ne rien savoir de tout ceci.
        return std::nullopt;
    }

    return m_settings.plannedQuestions.at( p_index );
}

IntervalDirection ExerciseSession::drawDirection()
{
    // The shares are read as WEIGHTS rather than as percentages: the draw is made inside their total,
    // whatever that total is. Changing them to 5 / 3 / 2 therefore changes nothing but the proportions,
    // which is what anyone editing them would expect.
    const std::int32_t total =
      m_settings.ascendingShare + m_settings.descendingShare + m_settings.harmonicShare;

    if( total <= 0 )
    {
        // Every share at zero is a configuration mistake. Ascending is what the application teaches
        // first, so it is the safe answer rather than a question that could not be sounded at all.
        return IntervalDirection::Ascending;
    }

    std::uniform_int_distribution<std::int32_t> distribution{ 1, total };

    const std::int32_t draw = distribution( m_randomEngine );

    if( draw <= m_settings.ascendingShare )
    {
        return IntervalDirection::Ascending;
    }

    if( draw <= ( m_settings.ascendingShare + m_settings.descendingShare ) )
    {
        return IntervalDirection::Descending;
    }

    return IntervalDirection::Harmonic;
}

Interval ExerciseSession::drawTarget()
{
    // CE QUE LE JOUEUR VIENT D'ETUDIER EST TIRE PLUS SOUVENT - et ce n'est PAS ce que ce fichier refusait autrefois.
    //
    // La version precedente refusait de ponderer vers ce que le joueur RATE, et elle avait raison : « insister sur tes
    // faiblesses » fait sentir une session qui s'acharne, et la palette adaptative fait deja le travail de garder les
    // questions au bon niveau.
    //
    // Le FOCUS D'ETUDE est l'inverse. Il ne dit pas « tu es mauvais ici » - il dit « tu viens de lire une lecon sur
    // ca ». C'est une CONTINUITE avec ce que le joueur vient de faire, pas une punition : un joueur qui ferme une lecon
    // sur la quinte juste et tombe sur la quinte juste ne se dit pas qu'on l'attaque, il se dit que la lecon servait a
    // quelque chose.
    //
    // ET LE FOCUS N'ELARGIT JAMAIS LA PALETTE : un concept absent de la palette est simplement ignore ci-dessous. Le
    // poids change la FREQUENCE, jamais l'ensemble des reponses possibles - donc un joueur ne peut pas tomber sur un
    // intervalle qu'on ne lui a pas enseigne.
    std::vector<double> weights;
    weights.reserve( m_palette.size() );

    for( const Interval & interval : m_palette )
    {
        const bool isStudied = std::ranges::find( m_settings.studyFocus, interval.semitones() )
                               != m_settings.studyFocus.end();

        weights.push_back( isStudied ? STUDIED_DRAW_WEIGHT : 1.0 );
    }

    // discrete_distribution et non uniforme : c'est ce qui permet de peser SANS retirer personne du tirage. Une entree
    // non etudiee garde son poids de 1, donc elle reste parfaitement atteignable - elle est seulement moins frequente.
    std::discrete_distribution<std::size_t> distribution{ weights.begin(), weights.end() };

    return m_palette.at( distribution( m_randomEngine ) );
}

std::int32_t ExerciseSession::drawRootMidiNumber( const Interval & p_target,
                                                  IntervalDirection p_direction )
{
    const std::int32_t intervalSize = p_target.semitones();

    std::int32_t lowestRoot = m_settings.lowestRootMidiNumber;
    std::int32_t highestRoot = m_settings.highestRootMidiNumber;

    // The root is the note the interval is played FROM, so what it needs depends on the direction:
    //
    //   * going up, or sounding the two notes at once, it is the UPPER note that must stay under the
    //     ceiling;
    //   * going down, it is the room BELOW the root that matters.
    //
    // Getting this wrong is silent: the notes would simply be played outside the comfortable range of a
    // phone speaker, which sounds like a slightly odd question rather than like a bug.
    if( ( p_direction == IntervalDirection::Ascending ) || ( p_direction == IntervalDirection::Harmonic ) )
    {
        highestRoot = std::min( highestRoot, m_settings.highestPlayableMidiNumber - intervalSize );
    }

    if( p_direction == IntervalDirection::Descending )
    {
        lowestRoot = std::max( lowestRoot, m_settings.lowestPlayableMidiNumber + intervalSize );
    }

    if( highestRoot < lowestRoot )
    {
        // The settings have been changed to something that cannot hold this interval. Returning the
        // middle of the window keeps the question playable, which is better than a note outside the
        // range, an empty draw, and a question nobody could explain.
        return std::clamp( m_settings.lowestRootMidiNumber,
                           m_settings.lowestPlayableMidiNumber,
                           m_settings.highestPlayableMidiNumber );
    }

    std::uniform_int_distribution<std::int32_t> distribution{ lowestRoot, highestRoot };

    return distribution( m_randomEngine );
}

void ExerciseSession::registerReplay() noexcept
{
    if( !canReplay() )
    {
        // Once the answer is known there is nothing to replay: the feedback plays the interval again
        // by itself, and counting it would cost the player experience they did not spend.
        return;
    }

    ++m_currentQuestion.replayCount;
}

bool ExerciseSession::answer( std::int32_t p_semitones )
{
    if( m_state != SessionState::Asking )
    {
        // An answer arriving after the question is over changes nothing, and returns false. A double
        // tap must not be able to score twice, nor to lose a second life.
        return false;
    }

    return resolveAnswer( p_semitones == m_currentQuestion.target.semitones(),
                          intervalFromSemitones( p_semitones ) );
}

bool ExerciseSession::answerDirection( IntervalDirection p_direction )
{
    if( ( m_state != SessionState::Asking ) || ( m_currentQuestion.kind != QuestionKind::Direction ) )
    {
        // A direction where a name was expected is a different language: it changes nothing.
        return false;
    }

    return resolveAnswer( p_direction == m_currentQuestion.direction, std::nullopt );
}

bool ExerciseSession::answerSung( bool p_isCorrect )
{
    if( ( m_state != SessionState::Asking ) || ( m_currentQuestion.kind != QuestionKind::Sing ) )
    {
        // A sung answer where no singing was asked changes nothing: the voice is the answer, and only on a sung
        // question.
        return false;
    }

    return resolveAnswer( p_isCorrect, std::nullopt );
}

HitQuality ExerciseSession::registerRhythmTap( double p_positionInBeats )
{
    if( ( m_state != SessionState::Asking ) || ( m_currentQuestion.kind != QuestionKind::Rhythm ) )
    {
        // Une frappe hors d'une question de rythme ne juge rien. C'est le meme refus que answerDirection oppose a une
        // question qui demandait un nom : deux langues differentes ne se repondent pas l'une l'autre.
        return HitQuality::Miss;
    }

    const RhythmPattern & pattern = patternOf( m_currentQuestion );

    const double beatMs = beatDurationMs( static_cast<double>( m_currentQuestion.bpm ) );

    if( pattern.hits().empty() || ( beatMs <= 0.0 ) )
    {
        // Une cellule sans frappe rendrait toute frappe parfaite - la distance a un ensemble vide vaut zero - et un
        // tempo nul rendrait toute frappe infiniment loin. Les deux sont des reglages fautifs, pas des questions a
        // juger : ils refusent la frappe plutot que de mentir sur ce qu'elle vaut.
        return HitQuality::Miss;
    }

    const HitQuality quality =
      judgeDistance( distanceToNearestOnsetInBeats( pattern, p_positionInBeats ) * beatMs );

    if( quality == HitQuality::Miss )
    {
        ++m_currentQuestion.offBeatTapCount;

        return quality;
    }

    // L'onset le plus proche est marque COUVERT. Le toucher deux fois - le doigt qui rebondit sur l'ecran - ne coute
    // rien : ce qui est juge, c'est la PLACE des frappes, et une frappe de la cellule est touchee ou elle ne l'est pas.
    const std::size_t onsetIndex = nearestOnsetIndex( pattern, p_positionInBeats );

    if( onsetIndex < m_currentQuestion.coveredOnsets.size() )
    {
        m_currentQuestion.coveredOnsets.at( onsetIndex ) = true;
    }

    return quality;
}

bool ExerciseSession::endRhythmLoop()
{
    if( ( m_state != SessionState::Asking ) || ( m_currentQuestion.kind != QuestionKind::Rhythm ) )
    {
        return false;
    }

    // Les deux conditions, et pas seulement la premiere : couvrir chaque frappe de la cellule ET ne rien taper a cote.
    // Sans la seconde, la question serait juste des qu'on a touche les bons onsets, quel que soit le nombre de frappes
    // ajoutees entre eux - et "reproduire une cellule" deviendrait "taper en continu".
    const bool everyOnsetWasCovered =
      !m_currentQuestion.coveredOnsets.empty()
      && std::ranges::all_of( m_currentQuestion.coveredOnsets, []( bool p_isCovered ) { return p_isCovered; } );

    const bool noTapWasOffBeat = ( m_currentQuestion.offBeatTapCount == 0 );

    // L'ardoise est remise a zero AVANT de rendre le verdict, et pas apres : un rate laisse la question posee, et la
    // boucle suivante doit repartir vierge - sinon une frappe oubliee une fois le resterait pour toujours.
    m_currentQuestion.coveredOnsets.assign( m_currentQuestion.coveredOnsets.size(), false );
    m_currentQuestion.offBeatTapCount = 0;

    return resolveAnswer( everyOnsetWasCovered && noTapWasOffBeat, std::nullopt );
}

bool ExerciseSession::answerChord( ChordQuality p_quality )
{
    if( ( m_state != SessionState::Asking ) || ( m_currentQuestion.kind != QuestionKind::Chord ) )
    {
        // Nommer une couleur la ou aucune n'a ete jouee ne repond a rien. C'est le meme refus que la direction oppose a
        // une question qui demandait un nom, et la frappe a une question d'intervalle : trois langues, trois questions.
        return false;
    }

    // Rien a enregistrer comme "repondu" : il n'y a pas de distance a montrer dans le verdict, seulement une couleur.
    m_lastChordAnswer = p_quality;

    return resolveAnswer( p_quality == m_currentQuestion.chord.quality, std::nullopt );
}

QuestionKind drawQuestionKind( const SessionSettings & p_settings, std::mt19937 & p_engine )
{
    // Les parts, et leur SOMME.
    //
    // Le tirage se fait sur la somme, et jamais sur cent. C'etait le defaut, et Roger a mis le doigt dessus : « on
    // commence a avoir beaucoup de spinbox 0-100 en disant que c'est des parts, mais sur dix questions, c'est plus des
    // probabilites non ? ». Avec six parts a vingt - ce que l'ecran permet de regler - la somme fait 120, et un tirage
    // sur [0, 100) faisait DISPARAITRE les derniers genres sans que rien ne le dise : on demandait du vamp, et il n'en
    // venait jamais.
    //
    // Des parts se lisent les unes PAR RAPPORT AUX AUTRES. C'est donc leur somme qui est l'echelle, et vingt partout
    // vaut un sixieme pour chacun, exactement comme on l'attend.
    //
    // L'ordre est celui de l'ecran : chaque genre prend la tranche qui suit la precedente, donc augmenter une part ne
    // deplace que les questions d'apres.
    const std::array<KindShare, 8> shares{ KindShare{ .share = p_settings.namedIntervalQuestionShare,
                                                      .kind = QuestionKind::NamedInterval },
                                           KindShare{ .share = p_settings.singQuestionShare,
                                                      .kind = QuestionKind::Sing },
                                           KindShare{ .share = p_settings.directionQuestionShare,
                                                      .kind = QuestionKind::Direction },
                                           KindShare{ .share = p_settings.chordQuestionShare,
                                                      .kind = QuestionKind::Chord },
                                           KindShare{ .share = p_settings.modeColourQuestionShare,
                                                      .kind = QuestionKind::ModeColour },
                                           KindShare{ .share = p_settings.modeNameQuestionShare,
                                                      .kind = QuestionKind::ModeName },
                                           KindShare{ .share = p_settings.modeVampQuestionShare,
                                                      .kind = QuestionKind::ModeVamp },
                                           KindShare{ .share = p_settings.foreignNoteQuestionShare,
                                                      .kind = QuestionKind::ForeignNote } };

    std::int32_t total = 0;

    for( const KindShare & entry : shares )
    {
        total += std::max( std::int32_t{ 0 }, entry.share );
    }

    if( total <= 0 )
    {
        // Aucune part : l'intervalle a nommer est la question par defaut du jeu, et c'est ce qu'elle a toujours ete.
        return QuestionKind::NamedInterval;
    }

    std::uniform_int_distribution<std::int32_t> distribution{ 0, total - 1 };

    const std::int32_t draw = distribution( p_engine );

    // Une TABLE, et non une chaine de comparaisons cumulees : les bornes s'additionnent d'elles-memes, chaque genre est
    // une ligne, et un genre AJOUTE plus tard ne peut pas oublier de mettre a jour les sommes des suivants - le defaut
    // exact que la version precedente portait.
    std::int32_t boundary = 0;

    for( const KindShare & entry : shares )
    {
        boundary += std::max( std::int32_t{ 0 }, entry.share );

        if( draw < boundary )
        {
            return entry.kind;
        }
    }

    // Inatteignable tant que la somme ci-dessus est celle qui borne le tirage : y arriver voudrait dire qu'une part a
    // ete oubliee en chemin.
    return QuestionKind::NamedInterval;
}

QuestionKind ExerciseSession::drawKind()
{
    // Le tirage vit dans drawQuestionKind : le Bilan en a besoin pour ses questions « au hasard du niveau », et une
    // session ordinaire ne doit pas avoir sa propre copie qui deriverait.
    return drawQuestionKind( m_settings, m_randomEngine );
}

void ExerciseSession::buildChordQuestion( Question & p_question )
{
    p_question.chord.quality = drawChordQuality();

    p_question.chord.rootMidiNumber = drawChordRootMidiNumber( p_question.chord.quality );

    // Ce que le joueur peut repondre : SA palette, dans l'ordre d'apprentissage, et rien d'autre. Une couleur qu'il
    // n'a jamais rencontree ne lui serait d'aucun secours - elle ne serait pas un choix, seulement un piege.
    p_question.chordChoices.assign( m_chordPalette.begin(), m_chordPalette.end() );
}

bool ExerciseSession::answerModeColour( ModeColourAnswer p_answer )
{
    if( ( m_state != SessionState::Asking )
        || ( ( m_currentQuestion.kind != QuestionKind::ModeColour )
             && ( m_currentQuestion.kind != QuestionKind::ModeVamp ) ) )
    {
        // Comparer deux couleurs la ou il n'y en a pas eu deux ne repond a rien : c'est le meme refus que le nom
        // d'intervalle oppose a une question chantee, et la frappe a une question d'intervalle.
        //
        // Le VAMP se repond de la meme facon, et c'est deliberé : la question qu'il pose - « le second passage est-il
        // plus clair ? » - est la meme. Ce qui change est ce qu'il fait entendre, pas ce qu'il demande.
        return false;
    }

    // Le sens attendu est calcule par le DOMAINE, a partir des deux modes de la question : l'ecran ne peut donc pas se
    // tromper de sens en lisant la question, et il n'a rien a recalculer.
    //
    // Et il se calcule en TROIS valeurs, pas deux : quand les deux passages portent la meme couleur, la seule bonne
    // reponse est « pareil ». L'ancien calcul rendait « plus sombre » (la comparaison d'un mode avec lui-meme est fausse),
    // ce qui aurait donne une bonne reponse a un joueur qui n'avait rien entendu - le bug que Roger a vu de loin.
    const bool sameness = !m_currentQuestion.previousMode.has_value()
                          || ( m_currentQuestion.mode == *m_currentQuestion.previousMode );

    // Le ternaire imbrique est remplace par deux comparaisons nommees : clang-tidy a raison de le refuser, et la
    // lecture y gagne - la couleur attendue se lit maintenant comme une phrase, pas comme une poupee russe.
    ModeColourAnswer expected = ModeColourAnswer::Same;

    if( !sameness )
    {
        expected = isBrighterThan( m_currentQuestion.mode, *m_currentQuestion.previousMode ) ? ModeColourAnswer::Brighter
                                                                                             : ModeColourAnswer::Darker;
    }

    return resolveAnswer( p_answer == expected, std::nullopt );
}

bool ExerciseSession::answerModeColour( bool p_secondIsBrighter )
{
    // L'ancienne forme, a deux reponses : la plupart des questions de couleur ne portent qu'une difference a entendre, et
    // les tests qui parlent du SENS de la comparaison n'ont pas a connaitre la troisieme reponse.
    return answerModeColour( p_secondIsBrighter ? ModeColourAnswer::Brighter : ModeColourAnswer::Darker );
}

bool ExerciseSession::answerModeName( Mode p_mode )
{
    if( ( m_state != SessionState::Asking ) || ( m_currentQuestion.kind != QuestionKind::ModeName ) )
    {
        return false;
    }

    // Comme pour un accord : rien a enregistrer comme « repondu », parce qu'il n'y a pas de distance a montrer dans le
    // verdict - seulement une couleur, et un nom.
    m_lastModeAnswer = p_mode;

    return resolveAnswer( p_mode == m_currentQuestion.mode, std::nullopt );
}

bool ExerciseSession::answerForeignNote( std::int32_t p_stepIndex )
{
    if( ( m_state != SessionState::Asking ) || ( m_currentQuestion.kind != QuestionKind::ForeignNote ) )
    {
        // Designer un pas la ou il n'y a pas de gamme a juger ne repond a rien : c'est le meme refus que le nom
        // d'intervalle oppose a une question chantee.
        return false;
    }

    return resolveAnswer( p_stepIndex == m_currentQuestion.foreignStepIndex, std::nullopt );
}

void ExerciseSession::buildModeQuestion( Question & p_question, bool p_compare )
{
    p_question.modeTonic = drawModeTonic();

    p_question.mode = drawMode();

    // LES DEUX PASSAGES D'UNE COMPARAISON ONT LE MEME CENTRE, et il fallait l'ECRIRE.
    //
    // C'est une CORRECTION, et elle vient de Roger : « je viens de tester le cercle des deux modes et ca ne fonctionne
    // pas. La roue reste exactement identique (meme notes, meme tonique, meme trace, meme depart) ».
    //
    // La cause n'etait pas le dessin : `previousModeTonic` n'etait JAMAIS rempli sur une comparaison. Il gardait donc sa
    // valeur par defaut - un RE, la note 62 de l'en-tete - et le PREMIER passage etait joue sur ce re-la pendant que le
    // second etait joue sur la tonique tiree. Deux centres differents pour une comparaison de COULEUR : la question
    // etait injuste, parce que la difference entendue n'etait pas celle qu'on demandait de juger. Et la roue, elle,
    // prenait un repere fixe au lieu de celui du premier mode - ce qui la faisait paraitre immobile.
    //
    // Sur une comparaison, les deux modes sont joues sur le MEME bourdon : c'est exactement ce qui les rend comparables,
    // et c'est ce que la roue appelle « le repere ». Un VAMP, lui, deplace le centre - voir buildModeVampQuestion : c'est
    // le centre qui y change, et c'est toute la question.
    if( p_compare )
    {
        p_question.previousModeTonic = p_question.modeTonic;
        // Parfois, la MEME couleur DEUX FOIS, et c'est deliberé : c'est la seule facon pour que « pareil » soit une bonne
        // reponse de temps en temps. Roger a demande le bouton ; encore fallait-il lui donner quelque chose a entendre.
        //
        // Entendre qu'il n'y a PAS de difference est une competence d'oreille, et c'est celle qu'on perd en cherchant
        // toujours quelque chose a entendre. La part est reglable, et a zero le jeu ne pose que des differences.
        std::uniform_int_distribution<std::int32_t> shareDraw{ 0, 99 };

        if( shareDraw( m_randomEngine ) < m_settings.sameColourQuestionShare )
        {
            p_question.previousMode = p_question.mode;
        }
        else
        {
            // Sinon, un mode DIFFERENT du premier, et tire dans la meme palette : comparer un mode avec lui-meme n'aurait
            // pas de reponse, donc la question serait impossible plutot que difficile.
            //
            // La boucle se termine, et c'est une garantie du DOMAINE et non un espoir : beginnerModePalette rend toujours
            // deux modes au moins, donc il existe toujours un mode different a tirer.
            Mode previousMode = drawMode();

            while( previousMode == p_question.mode )
            {
                previousMode = drawMode();
            }

            p_question.previousMode = previousMode;
        }
    }

    // Et une MELODIE, quand le contenu en porte une pour ce mode : la question de NOM devient alors « quel est le mode
    // de cette phrase ? », au lieu d'une gamme qui monte.
    //
    // Le tirage a lieu ICI, dans le domaine, et non dans un ecran : c'est la question qui decide de ce qu'elle fait
    // entendre, et une phrase tiree par l'affichage serait une question que le domaine ne connait pas. Sans phrase pour
    // ce mode - un contenu absent, ou trie autrement - la question reste entierement posee, en gamme.
    if( !p_compare && ( m_phraseBook != nullptr ) )
    {
        p_question.modePhrase = m_phraseBook->drawPhraseFor( p_question.mode, m_randomEngine );
    }

    // Ce que le joueur peut repondre : SA palette, dans l'ordre d'apprentissage, et rien d'autre. Un mode qu'il n'a
    // jamais rencontre ne serait pas un choix, seulement un piege.
    p_question.modeChoices.assign( m_modePalette.begin(), m_modePalette.end() );
}

Mode ExerciseSession::drawMode()
{
    // Chaque mode de la palette a la meme chance, y compris le dernier arrive : la meme regle que les intervalles et les
    // accords, et pour la meme raison - ponderer vers ce que le joueur rate ferait un meilleur exercice et un pire jeu.
    if( m_modePalette.empty() )
    {
        // Ne peut pas arriver (voir beginnerModePalette), et rend tout de meme un mode valide : une question posee sur
        // une palette vide serait un plantage, et le domaine n'en merite pas.
        return Mode::Ionian;
    }

    std::uniform_int_distribution<std::size_t> distribution{ 0, m_modePalette.size() - 1 };

    return m_modePalette.at( distribution( m_randomEngine ) );
}

Note ExerciseSession::drawModeTonic()
{
    // La tonique du BOURDON, donc grave, et dans une fenetre ETROITE.
    //
    // Trois demi-tons de chaque cote du re 2, ce qui n'est pas un gout : les echantillons du bourdon sont enregistres en
    // re 2 et en la 2, et un echantillon transpose de plus de trois demi-tons s'entend comme un ralentissement. Une
    // tonique entre 35 et 41, et sa quinte a sept demi-tons au-dessus, tiennent donc TOUJOURS a moins de trois
    // demi-tons de l'un des deux enregistrements.
    constexpr std::int32_t LOWEST_TONIC_MIDI_NUMBER = 35;
    constexpr std::int32_t HIGHEST_TONIC_MIDI_NUMBER = 41;

    std::uniform_int_distribution<std::int32_t> distribution{ LOWEST_TONIC_MIDI_NUMBER, HIGHEST_TONIC_MIDI_NUMBER };

    return Note{ distribution( m_randomEngine ) };
}

void ExerciseSession::buildForeignNoteQuestion( Question & p_question )
{
    p_question.modeTonic = drawModeTonic();

    p_question.mode = drawMode();

    const std::vector<Note> scale = notesOfMode( p_question.modeTonic, p_question.mode );

    // L'INTRUS : un pas au hasard, et sa note remplacee par une VOISINE qui n'appartient pas a la gamme.
    //
    // Un demi-ton d'ecart, et jamais plus : c'est ce qui rend la faute audible SANS etre caricaturale. Une note prise
    // trois tons plus loin s'entendrait comme une rupture, et l'exercice deviendrait une question de bon sens plutot
    // qu'une question d'oreille.
    std::uniform_int_distribution<std::size_t> stepDraw{ 0, scale.size() - 1 };

    const std::size_t stepIndex = stepDraw( m_randomEngine );

    p_question.foreignStepIndex = static_cast<std::int32_t>( stepIndex );

    p_question.foreignMelody = scale;

    // La gamme, ramenee a ses CLASSES de hauteur : c'est ce qui dit si une note en fait partie, et non sa place dans la
    // melodie - la meme note peut y revenir une octave plus haut.
    std::array<bool, SEMITONES_PER_OCTAVE> belongsToScale{};

    for( const Note & note : scale )
    {
        belongsToScale.at( static_cast<std::size_t>( note.pitchClassIndex() ) ) = true;
    }

    // Le demi-ton AU-DESSUS d'abord, puis celui du dessous s'il retombe dans la gamme. L'un des deux en sort toujours :
    // une gamme occupe sept des douze classes de hauteur, donc une note de la gamme ne peut pas avoir ses DEUX voisines
    // dedans.
    const Note original = scale.at( stepIndex );

    for( const std::int32_t shift : { 1, -1 } )
    {
        const Note candidate{ original.midiNumber() + shift };

        if( !candidate.isValid() )
        {
            continue;
        }

        if( !belongsToScale.at( static_cast<std::size_t>( candidate.pitchClassIndex() ) ) )
        {
            p_question.foreignMelody.at( stepIndex ) = candidate;

            return;
        }
    }
}

void ExerciseSession::buildVampQuestion( Question & p_question )
{
    const Mode firstMode = drawMode();

    const auto firstIndex = static_cast<std::int32_t>( modeIndex( firstMode ) );

    // UN cran, et un seul.
    //
    // Ce n'est pas une precaution, c'est la musique : monter d'un cran vers le clair demande de deplacer la tonique
    // d'une QUINTE, et deux crans la deplaceraient de deux quintes - soit sept demi-tons de plus, hors de la fenetre des
    // enregistrements de bourdon. Un cran donne deja l'ecart le plus riche : « re dorien » et « sol mixolydien » sont la
    // MEME gamme.
    constexpr std::int32_t VAMP_OFFSET = 1;

    // Si le mode est deja le plus clair, il n'y a pas de cran a monter : on descend alors d'un cran, et c'est la seule
    // asymetrie de cette question. La palette commence par le majeur et le mineur, donc elle ne s'y trouve pas au
    // debut - mais elle s'elargit, et le locrien finit par y arriver.
    const bool goBrighter = firstIndex >= VAMP_OFFSET;

    const std::int32_t secondIndex = goBrighter ? ( firstIndex - VAMP_OFFSET ) : ( firstIndex + VAMP_OFFSET );

    p_question.previousMode = firstMode;

    // La tonique du PREMIER passage, et sa fenetre depend du SENS.
    //
    // Le second passage est a une quinte du premier, et les deux doivent rester a portee des deux enregistrements de
    // bourdon (re 2 et la 2, soit 35 a 48 a trois demi-tons pres). On tire donc le premier du cote ou il reste de la
    // place : vers le grave si la seconde monte, vers l'aigu si elle descend.
    constexpr std::int32_t LOW_RANGE_MIDI_NUMBER = 35;
    constexpr std::int32_t HIGH_RANGE_MIDI_NUMBER = 48;

    // Le deplacement REEEL de la tonique, calcule par le domaine : deux modes n'ont pas le meme degre dans la gamme, et
    // c'est ce degre qui decide de combien la tonique bouge. Le lydien est le quatrieme degre, le mixolydien le
    // cinquieme : la meme gamme posee sur l'un ou sur l'autre ne se deplace donc pas du meme nombre de demi-tons.
    const std::int32_t tonicShift = modeTonicShift( firstMode, static_cast<Mode>( secondIndex ) );

    const std::int32_t lowest = LOW_RANGE_MIDI_NUMBER + ( ( tonicShift < 0 ) ? -tonicShift : 0 );
    const std::int32_t highest = HIGH_RANGE_MIDI_NUMBER - ( ( tonicShift > 0 ) ? tonicShift : 0 );

    p_question.previousModeTonic = Note{ std::uniform_int_distribution<std::int32_t>{ lowest, highest }( m_randomEngine ) };

    p_question.mode = static_cast<Mode>( secondIndex );

    // La tonique suit le mode, du deplacement que le domaine vient de calculer - et les deux passages ont donc
    // EXACTEMENT les memes sept notes.
    //
    // C'est le §3.3 de la note 27 mis en musique : les centres montent le cercle des quintes, les notes eteintes le
    // descendent. Le joueur entend deux fois le meme materiau, et pourtant deux modes - voila ce que « un mode, c'est un
    // jeu de notes plus un centre » veut dire.
    p_question.modeTonic = p_question.previousModeTonic.transposedBy( tonicShift );

    p_question.modeChoices.assign( m_modePalette.begin(), m_modePalette.end() );
}

void ExerciseSession::widenModePalette()
{
    // Un perimetre choisi ne grandit pas : voir widenPalette.
    if( m_settings.paletteIsFixed )
    {
        return;
    }

    // Le plafond, comme pour les intervalles.
    if( ( m_settings.maximumModeCount > 0 ) && ( m_modePalette.size() >= m_settings.maximumModeCount ) )
    {
        return;
    }

    if( m_modePalette.size() >= modeLearningOrder().size() )
    {
        // Tous les modes du jeu sont deja en place : il n'y a plus rien a elargir.
        return;
    }

    // Toujours le mode SUIVANT de l'ordre d'apprentissage, jamais un tire au hasard : c'est ce qui rend la progression
    // previsible, du majeur et du mineur vers les extremes.
    m_modePalette = beginnerModePalette( m_modePalette.size() + 1 );
}

ChordQuality ExerciseSession::drawChordQuality()
{
    // Chaque couleur de la palette a la meme chance, y compris la derniere arrivee. Meme regle que les intervalles, et
    // pour la meme raison : ponderer vers ce que le joueur rate ferait un meilleur exercice et un pire jeu.
    std::uniform_int_distribution<std::size_t> distribution{ 0, m_chordPalette.size() - 1 };

    return m_chordPalette.at( distribution( m_randomEngine ) );
}

std::int32_t ExerciseSession::drawChordRootMidiNumber( ChordQuality p_quality )
{
    // La tonique est la note la PLUS GRAVE de l'accord : ce qui doit tenir dans la fenetre confortable, c'est la note
    // du haut, donc la tonique plus l'intervalle le plus grand de l'accord.
    //
    // Se tromper ici est silencieux : l'accord serait simplement joue hors de la plage d'un haut-parleur de telephone,
    // ce qui s'entend comme un accord un peu etrange plutot que comme un bug.
    const std::span<const std::int32_t> intervals = chordIntervals( p_quality );

    const std::int32_t chordSpan = intervals.back();

    const std::int32_t lowestRoot = m_settings.lowestRootMidiNumber;

    const std::int32_t highestRoot =
      std::min( m_settings.highestRootMidiNumber, m_settings.highestPlayableMidiNumber - chordSpan );

    if( highestRoot < lowestRoot )
    {
        // Les reglages ont ete pousses dans un coin ou cet accord ne tient pas. Rendre le milieu de la fenetre garde la
        // question jouable, ce qui vaut mieux qu'un accord hors plage et une question que personne ne peut expliquer.
        return std::clamp( m_settings.lowestRootMidiNumber,
                           m_settings.lowestPlayableMidiNumber,
                           m_settings.highestPlayableMidiNumber );
    }

    std::uniform_int_distribution<std::int32_t> distribution{ lowestRoot, highestRoot };

    return distribution( m_randomEngine );
}

void ExerciseSession::buildRhythmicCell( Question & p_question )
{
    const std::vector<RhythmPattern> & patterns = allRhythmPatterns();

    // Une cellule au hasard, tous les coups.
    //
    // Pas de progression ici, et c'est une difference de fond avec les intervalles : le rythme ne s'apprend pas du
    // simple vers le compose, les cinq cellules sont accessibles des la premiere session, et retomber sur la meme est
    // une repetition - exactement ce qu'un exercice de rythme demande.
    std::uniform_int_distribution<std::size_t> distribution{ 0, patterns.size() - 1 };

    p_question.patternIndex = distribution( m_randomEngine );

    p_question.bpm = m_settings.rhythmBpm;

    // L'ardoise de la tentative : une case par frappe de la cellule, toutes a couvrir. Le rythme est le seul endroit
    // du jeu ou le joueur peut ne PAS repondre la ou on l'attend - c'est donc le seul endroit qui a besoin de compter
    // des frappes plutot qu'une reponse.
    const RhythmPattern & pattern = patterns.at( p_question.patternIndex );

    p_question.coveredOnsets.assign( pattern.hits().size(), false );
    p_question.offBeatTapCount = 0;
}

bool ExerciseSession::resolveAnswer( bool p_isCorrect, std::optional<Interval> p_answer )
{
    m_lastAnswer = p_answer;
    m_lastAnswerWasCorrect = p_isCorrect;

    if( p_isCorrect )
    {
        m_score.registerSuccess( m_currentQuestion.replayCount, m_currentQuestion.wrongAttemptCount );

        // Le compte par famille : la question est CONCLUE, et elle l'est bien. C'est ce que l'ecran de fin d'Arcade lit.
        m_familyTally.registerQuestion( familyOf( m_currentQuestion.kind ), true );

        // A new interval joins the palette every so many successes in a row. The modulo, rather than a
        // simple comparison, is what makes this happen at every step: without it the condition would
        // stay true for ever after the third success, and the palette would grow on every single
        // correct answer.
        // UNE PROGRESSION PAR FAMILLE, ET SEULEMENT CELLE QU'ON VIENT DE JOUER.
        //
        // Roger, apres avoir fait tester le jeu a des amis : « arrive aux accords, on n'a pas 2 accords a trouver mais deja
        // 4 ; et aux modes c'est pire, on n'a pas 2 modes mais 6. Pour rappel, le joueur est toujours debutant. »
        //
        // La cause etait ici, et c'etait un CHOIX ecrit noir sur blanc : les trois palettes s'elargissaient sur les MEMES
        // reussites - la serie de la session, toutes familles confondues. Dix questions d'intervalle faisaient donc monter
        // les accords et les modes sans qu'une seule question d'accord ait ete posee. Trois compteurs semblaient plus
        // fragiles qu'un seul ; c'est l'inverse, et c'est le telephone qui a tranche.
        const auto wideningPeriod = static_cast<std::int32_t>( m_settings.successesBeforeWidening );

        const QuestionFamily family = familyOf( m_currentQuestion.kind );

        std::int32_t & familyStreak = m_familyStreaks.at( static_cast<std::size_t>( family ) );

        ++familyStreak;

        if( ( wideningPeriod > 0 ) && ( familyStreak % wideningPeriod == 0 ) )
        {
            // UNE SEULE PALETTE GRANDIT : celle de la famille qu'on vient de reussir. Le commutateur couvre les trois cas
            // sans defaut, pour qu'une famille ajoutee demain fasse echouer la compilation plutot que de rester muette.
            switch( family )
            {
                case QuestionFamily::Interval:
                    widenPalette();
                    break;
                case QuestionFamily::Chord:
                    widenChordPalette();
                    break;
                case QuestionFamily::Mode:
                    widenModePalette();
                    break;
            }
        }

        m_state = SessionState::Feedback;

        return true;
    }

    ++m_currentQuestion.wrongAttemptCount;

    ++m_consecutiveErrors;

    // UNE ERREUR, MEME SI LA QUESTION RESTE OUVERTE.
    //
    // La question n'est pas conclue - le joueur va la reprendre - mais il s'est deja trompe, et l'ecran de fin doit le
    // savoir. C'est la seule chose que ce compteur a de plus que `asked` : voir FamilyTally::missed.
    m_familyTally.registerMiss( familyOf( m_currentQuestion.kind ) );

    // ET L'ERREUR NE REMET A ZERO QUE SA FAMILLE.
    //
    // C'est la meme regle que la montee, vue de l'autre cote : se tromper sur un mode ne doit pas annuler ce qu'on vient
    // de comprendre sur les accords. Sans cela, une seule erreur ferait reculer trois progressions - et le joueur
    // paierait pour une chose qu'il n'a pas ratee.
    m_familyStreaks.at( static_cast<std::size_t>( familyOf( m_currentQuestion.kind ) ) ) = 0;

    if( ( m_consecutiveErrors >= 2 ) && ( m_currentQuestion.kind == QuestionKind::NamedInterval )
        && ( m_currentQuestion.direction != IntervalDirection::Harmonic ) )
    {
        // Deux erreurs de suite : la question EN COURS bascule en mode guide, comme un indice. Le joueur n'a plus
        // qu'a dire si ca monte ou ca descend - une question plus petite, a laquelle il sait encore repondre. Un
        // intervalle harmonique n'a ni monte ni descend, donc il reste tel quel.
        //
        // Et SEULE une question d'intervalle a nommer bascule. Une cellule rythmique n'a ni montee ni descente, un
        // accord ne se repond pas en disant dans quel sens il va, et une question chantee tient toute sa valeur de la
        // voix du joueur : la transformer en "monte ou descend ?" remplacerait une question par une autre.
        m_currentQuestion.kind = QuestionKind::Direction;
    }

    m_score.registerError();

    if( m_score.isOutOfLives() )
    {
        // La derniere vie s'en va, mais la question merite encore sa REPONSE.
        //
        // La session passe donc en Feedback comme n'importe quelle question conclue, et c'est advance() qui la termine.
        // C'etait l'inverse, et le commentaire disait pourquoi : « the session is over the moment the last life goes,
        // without a feedback to read ». Roger l'a vu jouer et l'a renverse : « quand on perds, on arrive direct a la page
        // des scores, mais on n'a pas la reponse a la question sur laquelle on a fail ». Savoir ce qu'on a rate est
        // justement ce qui reste a apprendre quand la partie est perdue.
        m_state = SessionState::Feedback;

        // La question se CONCLUT sur un echec : elle compte comme demandee et ratee dans sa famille.
        m_familyTally.registerQuestion( familyOf( m_currentQuestion.kind ), false );

        return false;
    }

    // The grid closes in. Each wrong answer removes the least plausible of the remaining wrong
    // answers, so that the same question gets easier the longer it is struggled with: help the player
    // never has to ask for. See note 16 of the vault, "la grille qui s'aide".
    if( m_currentQuestion.choices.size() > AnswerGrid::MINIMUM_CHOICE_COUNT )
    {
        m_currentQuestion.choices =
          AnswerGrid::build( m_palette, m_currentQuestion.target, m_currentQuestion.choices.size() - 1, m_randomEngine );
    }

    return false;
}

void ExerciseSession::revealAnswer()
{
    if( m_state != SessionState::Asking )
    {
        return;
    }

    m_score.registerHelpedQuestion();

    // Une question revelee est une question CONCLUE, et manquee : elle compte dans sa famille comme telle.
    m_familyTally.registerQuestion( familyOf( m_currentQuestion.kind ), false );

    m_lastAnswerWasCorrect = false;

    // The player asked to be told: they were not ready. The newest interval leaves the palette, so
    // that the questions that follow are asked on ground they can stand on. Help costs nothing, but it
    // does say something, and this is the only place where saying it does not feel like a punishment.
    //
    // Seulement sur une question qui PARLE d'intervalles, et c'est une precision qui compte : passer une cellule
    // rythmique, un accord ou un mode n'apprend rien sur la palette d'intervalles, et la faire reculer serait une
    // consequence que le joueur ne pourrait relier a rien de ce qu'il vient de faire.
    //
    // Le test est demande au domaine via isIntervalQuestion, et non reecrit ici : une liste recopiee est une liste
    // qu'on oublie d'etendre quand un genre apparait.
    if( isIntervalQuestion( m_currentQuestion.kind ) )
    {
        narrowPalette();
    }

    m_state = SessionState::Feedback;
}

void ExerciseSession::advance()
{
    if( m_state != SessionState::Feedback )
    {
        return;
    }

    const bool everyQuestionWasAsked = ( m_score.completedQuestionCount() >= m_settings.questionCount );

    if( everyQuestionWasAsked || m_score.isOutOfLives() )
    {
        m_state = SessionState::Finished;

        return;
    }

    ++m_questionNumber;

    m_currentQuestion = buildQuestion();

    // Le compteur d'erreurs vaut pour la question qui VENAIT d'etre posee : deux erreurs dessus, et la suivante -
    // celle qui vient d'etre construite - a ete tiree guidee. On repart de zero pour celle-ci.
    m_consecutiveErrors = 0;

    m_lastAnswer.reset();
    m_lastChordAnswer.reset();
    m_lastModeAnswer.reset();
    m_lastAnswerWasCorrect = false;

    m_state = SessionState::Asking;
}

bool ExerciseSession::canHearChordAsArpeggio() const noexcept
{
    return m_settings.aidsAllowed && ( m_state == SessionState::Asking )
           && ( m_currentQuestion.kind == QuestionKind::Chord ) && ( m_currentQuestion.wrongAttemptCount >= 1 );
}

bool ExerciseSession::canRemoveOneWrongChordChoice() const noexcept
{
    // Une question d'ACCORD, et rien d'autre : retirer un intervalle faux serait un autre jeu, et personne ne l'a
    // demande.
    if( m_currentQuestion.kind != QuestionKind::Chord )
    {
        return false;
    }

    // Les aides doivent etre autorisees, le joueur doit avoir ESSAYE une fois, et il doit rester de quoi retirer : sous
    // trois choix il n'y a plus que la bonne reponse et un leurre, et une question a deux boutons n'est plus une
    // question.
    return m_settings.aidsAllowed && ( m_state == SessionState::Asking ) && ( m_currentQuestion.wrongAttemptCount >= 1 )
           && ( m_currentQuestion.chordChoices.size() > 2 );
}

bool ExerciseSession::removeOneWrongChordChoice()
{
    if( !canRemoveOneWrongChordChoice() )
    {
        return false;
    }

    // Les positions des leurres : tout ce qui n'est pas la bonne reponse.
    std::vector<std::size_t> wrongChoices;

    for( std::size_t index = 0; index < m_currentQuestion.chordChoices.size(); ++index )
    {
        if( m_currentQuestion.chordChoices.at( index ) != m_currentQuestion.chord.quality )
        {
            wrongChoices.push_back( index );
        }
    }

    if( wrongChoices.empty() )
    {
        return false;
    }

    std::uniform_int_distribution<std::size_t> distribution{ 0, wrongChoices.size() - 1 };

    const std::size_t chosen = wrongChoices.at( distribution( m_randomEngine ) );

    m_currentQuestion.chordChoices.erase( m_currentQuestion.chordChoices.begin()
                                          + static_cast<std::ptrdiff_t>( chosen ) );

    return true;
}

bool ExerciseSession::isHintAvailable() const noexcept
{
    // No condition on the state, on purpose: the hint is worth showing while the player is still
    // choosing AND after the answer is known, where it becomes "that is how you could have remembered
    // it". It appears on the first mistake and stays for the rest of the question, because a new
    // question brings a new Question with a count back at zero.
    //
    // A mode that offers no aid at all cuts it here, at the source: nothing on the screen has to know
    // that such a mode exists, and no screen can forget to check.
    return m_settings.aidsAllowed
           && ( m_currentQuestion.wrongAttemptCount >= m_settings.wrongAttemptsBeforeHint );
}

bool ExerciseSession::isHelpAvailable() const noexcept
{
    // Une question de rythme s'aide PLUS TOT qu'une question d'intervalle, et la difference est une regle du domaine
    // plutot qu'un caprice de l'ecran : voir wrongAttemptsBeforeRhythmHelp. Un intervalle se reecoute autant de fois
    // qu'on veut ; une cellule ne s'entend que pendant sa boucle d'ecoute.
    //
    // Le CHANT n'attend aucune tentative du tout, et il n'attend pas non plus qu'un mode donne le droit a l'aide :
    // on peut ne pas etre en mesure de chanter - un endroit bruyant, une gorge prise, un micro qui ne suit pas. Faire
    // rater une question pour decouvrir la sortie serait demander au joueur de rater pour avoir le droit de passer.
    // Passer n'est d'ailleurs pas une aide ici, c'est une PORTE : elle coute une question, et le score le sait deja.
    if( m_currentQuestion.kind == QuestionKind::Sing )
    {
        return m_state == SessionState::Asking;
    }

    const std::int32_t attemptsBeforeHelp = ( m_currentQuestion.kind == QuestionKind::Rhythm )
                                              ? m_settings.wrongAttemptsBeforeRhythmHelp
                                              : m_settings.wrongAttemptsBeforeHelp;

    return m_settings.aidsAllowed && ( m_state == SessionState::Asking )
           && ( m_currentQuestion.wrongAttemptCount >= attemptsBeforeHelp );
}

bool ExerciseSession::hasEarnedStar() const noexcept
{
    // The session has to have been played to the end. A star earned by running out of lives would
    // reward exactly what the star is meant to discourage.
    const bool everyQuestionWasAsked = ( m_score.completedQuestionCount() >= m_settings.questionCount );

    return everyQuestionWasAsked && m_score.hasEarnedStar();
}

void ExerciseSession::widenPalette()
{
    // Un perimetre CHOISI ne s'elargit pas : c'est le GodMode, et c'est toute sa promesse - le joueur a decide ce qu'il
    // travaille, le jeu n'y ajoute rien.
    if( m_settings.paletteIsFixed )
    {
        return;
    }

    // LE PLAFOND : une Arcade ou un Entrainement ne depassent pas la difficulte de leur niveau. Voir SessionSettings.
    if( ( m_settings.maximumPaletteSize > 0 ) && ( m_palette.size() >= m_settings.maximumPaletteSize ) )
    {
        return;
    }

    if( m_palette.size() >= learningOrderIntervals().size() )
    {
        // Everything the application knows is already in play.
        return;
    }

    // Always the NEXT interval of the order, never a drawn one: taking the palette as a prefix of the
    // order is what makes the progression predictable, and a new interval chosen at random would take
    // the player from a second to a fourteenth with no reason they could feel.
    m_palette = beginnerPalette( m_palette.size() + 1 );
}

void ExerciseSession::narrowPalette()
{
    if( m_settings.paletteIsFixed )
    {
        // Ni elargissement, ni retrecissement : voir widenPalette. Une erreur ne doit pas retirer au joueur un
        // intervalle qu'il a explicitement demande a travailler.
        return;
    }

    if( m_palette.size() <= m_settings.startingPaletteSize )
    {
        // Never below where the player started: someone already at the beginning would be left with
        // nothing to play with, and the session would have no question to ask.
        return;
    }

    m_palette = beginnerPalette( m_palette.size() - 1 );
}

void ExerciseSession::widenChordPalette()
{
    // Un perimetre choisi ne grandit pas : voir widenPalette.
    if( m_settings.paletteIsFixed )
    {
        return;
    }

    // Le plafond, comme pour les intervalles.
    if( ( m_settings.maximumChordQualityCount > 0 ) && ( m_chordPalette.size() >= m_settings.maximumChordQualityCount ) )
    {
        return;
    }

    if( m_chordPalette.size() >= chordLearningOrder().size() )
    {
        // Toutes les couleurs du jeu sont deja en place : il n'y a plus rien a elargir.
        return;
    }

    // Toujours la couleur SUIVANTE de l'ordre, jamais une tiree au hasard : c'est ce qui rend la progression
    // previsible, et une couleur choisie au hasard ferait sauter le joueur du majeur a la septieme majeure sans
    // aucune raison qu'il puisse sentir.
    m_chordPalette = beginnerChordPalette( m_chordPalette.size() + 1 );
}

QuestionFamily familyOf( QuestionKind p_kind ) noexcept
{
    switch( p_kind )
    {
        case QuestionKind::NamedInterval:
        case QuestionKind::Direction:
        case QuestionKind::Sing:
            return QuestionFamily::Interval;

        case QuestionKind::Chord:
            return QuestionFamily::Chord;

        case QuestionKind::Rhythm:
        case QuestionKind::ModeColour:
        case QuestionKind::ModeName:
        case QuestionKind::ModeVamp:
        case QuestionKind::ForeignNote:
            // Le rythme est classe avec les MODES, et ce n'est pas un hasard : c'est la famille « le reste », celle qui
            // n'est ni un intervalle ni une couleur d'accord. Comme il n'est jamais pose (voir isKindOpen), sa place ici
            // n'a aucune consequence - mais le commutateur doit couvrir tout l'enum pour qu'un genre ajoute demain fasse
            // echouer le test qui le parcourt.
            return QuestionFamily::Mode;
    }

    // Inatteignable tant que le commutateur couvre tous les genres, et c'est voulu.
    return QuestionFamily::Interval;
}

void FamilyTally::registerQuestion( QuestionFamily p_family, bool p_wasCorrect ) noexcept
{
    const auto index = static_cast<std::size_t>( p_family );

    ++asked.at( index );

    if( p_wasCorrect )
    {
        ++correct.at( index );
    }
}

void FamilyTally::registerMiss( QuestionFamily p_family ) noexcept
{
    ++missed.at( static_cast<std::size_t>( p_family ) );
}

std::size_t FamilyTally::askedIn( QuestionFamily p_family ) const noexcept
{
    return asked.at( static_cast<std::size_t>( p_family ) );
}

std::size_t FamilyTally::correctIn( QuestionFamily p_family ) const noexcept
{
    return correct.at( static_cast<std::size_t>( p_family ) );
}

std::size_t FamilyTally::successPercentIn( QuestionFamily p_family ) const noexcept
{
    const std::size_t askedCount = askedIn( p_family );

    if( askedCount == 0 )
    {
        // Une famille a laquelle on n'a pas joue n'a pas de taux. Zero, et l'ecran sait qu'un zero sur zero demande n'est
        // pas « nul » mais « absent » - voir askedIn, qu'il lit avant d'afficher.
        return 0;
    }

    return ( correctIn( p_family ) * 100 ) / askedCount;
}

}    // namespace musichien::domain
