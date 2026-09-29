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

}    // namespace

ExerciseSession::ExerciseSession( std::uint32_t p_seed, SessionSettings p_settings )
  : m_randomEngine{ p_seed }
  , m_settings{ p_settings }
  , m_palette{ beginnerPalette( m_settings.startingPaletteSize ) }
  , m_chordPalette{ beginnerChordPalette( m_settings.startingChordQualityCount ) }
  , m_score{ m_settings.lives }
  , m_currentQuestion{ buildQuestion() }
{
    // The first question is built HERE rather than on a start() call: a session that exists is a
    // session that is asking something, and the screen therefore never has to handle a state where
    // there is nothing to play.
}

Question ExerciseSession::buildQuestion()
{
    Question question;

    // Un PLAN decide la question, quand il y en a un : genre, cible et sens. C'est ce qui fait d'un Bilan une suite
    // DECIDEE - du plus facile au plus difficile - la ou une partie ordinaire laisse le tirage choisir. Le plan passe
    // avant tout tirage : ce qui est decide ne se retire pas.
    const std::optional<QuestionTarget> planned = plannedQuestionAt( m_questionNumber - 1 );

    if( planned.has_value() )
    {
        question.kind = planned->kind;
        question.target = Interval{ planned->target };
        question.direction = planned->direction;
    }
    else
    {
        // L'intervalle est tire EN PREMIER, meme quand la question sera rythmique : c'est l'ordre des tirages que les
        // tests existants ont appris a suivre, et une question de rythme qui ne s'en sert pas ne doit pas deplacer les
        // autres questions pour autant.
        question.target = drawTarget();

        question.kind = drawKind();
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
        if( planned.has_value() )
        {
            // Le plan a dit la COULEUR ; la tonique reste tiree, parce qu'un bilan ne teste pas la hauteur, et les
            // choix sont ceux de la palette.
            question.chord.quality = static_cast<ChordQuality>( planned->target );
            question.chord.rootMidiNumber = drawChordRootMidiNumber( question.chord.quality );
            question.chordChoices.assign( m_chordPalette.begin(), m_chordPalette.end() );

            return question;
        }

        // Un accord a une tonique et une couleur, mais ni direction ni grille de choix tires au hasard : il a sa
        // propre palette, et sa propre facon de se repondre.
        buildChordQuestion( question );

        return question;
    }

    if( !planned.has_value() )
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
    // Every interval of the palette has the same chance, including the newest one. Weighting the draw
    // towards what the player struggles with would be a better exercise and a worse game: it would
    // make the session feel like it is picking on them, and the adaptive palette already does the work
    // of keeping the questions at the right level.
    std::uniform_int_distribution<std::size_t> distribution{ 0, m_palette.size() - 1 };

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

QuestionKind ExerciseSession::drawKind()
{
    std::uniform_int_distribution<std::int32_t> distribution{ 0, 99 };

    const std::int32_t draw = distribution( m_randomEngine );

    if( draw < m_settings.singQuestionShare )
    {
        return QuestionKind::Sing;
    }

    if( ( m_settings.directionQuestionShare > 0 )
        && ( draw < m_settings.singQuestionShare + m_settings.directionQuestionShare ) )
    {
        return QuestionKind::Direction;
    }

    // Le rythme vient APRES les deux autres, et les parts se lisent comme des BORNES CUMULEES : chacune prend la
    // tranche qui suit la precedente. L'ordre n'est pas une preference, c'est ce qui rend le tirage lisible d'un coup
    // d'oeil - et ce qui fait qu'augmenter une part ne deplace que les questions qui la suivent.
    if( ( m_settings.rhythmQuestionShare > 0 )
        && ( draw < m_settings.singQuestionShare + m_settings.directionQuestionShare
                      + m_settings.rhythmQuestionShare ) )
    {
        return QuestionKind::Rhythm;
    }

    // Et les accords en dernier : c'est la question la plus exigeante des quatre, donc celle qui ferme la marche.
    if( ( m_settings.chordQuestionShare > 0 )
        && ( draw < m_settings.singQuestionShare + m_settings.directionQuestionShare
                      + m_settings.rhythmQuestionShare + m_settings.chordQuestionShare ) )
    {
        return QuestionKind::Chord;
    }

    return QuestionKind::NamedInterval;
}

void ExerciseSession::buildChordQuestion( Question & p_question )
{
    p_question.chord.quality = drawChordQuality();

    p_question.chord.rootMidiNumber = drawChordRootMidiNumber( p_question.chord.quality );

    // Ce que le joueur peut repondre : SA palette, dans l'ordre d'apprentissage, et rien d'autre. Une couleur qu'il
    // n'a jamais rencontree ne lui serait d'aucun secours - elle ne serait pas un choix, seulement un piege.
    p_question.chordChoices.assign( m_chordPalette.begin(), m_chordPalette.end() );
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

        // A new interval joins the palette every so many successes in a row. The modulo, rather than a
        // simple comparison, is what makes this happen at every step: without it the condition would
        // stay true for ever after the third success, and the palette would grow on every single
        // correct answer.
        const auto wideningPeriod = static_cast<std::int32_t>( m_settings.successesBeforeWidening );

        if( ( wideningPeriod > 0 ) && ( m_score.streak() % wideningPeriod == 0 ) )
        {
            widenPalette();

            // Les accords s'elargissent sur les MEMES reussites : une seule progression a tenir, plutot que deux
            // compteurs dont l'un finirait par mentir. Une couleur de plus tous les trois succes, comme un intervalle.
            widenChordPalette();
        }

        m_state = SessionState::Feedback;

        return true;
    }

    ++m_currentQuestion.wrongAttemptCount;

    ++m_consecutiveErrors;

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
        // The session is over the moment the last life goes, without a feedback to read: there is
        // nothing left to answer, and pretending otherwise would only delay the summary.
        m_state = SessionState::Finished;

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

    m_lastAnswerWasCorrect = false;

    // The player asked to be told: they were not ready. The newest interval leaves the palette, so
    // that the questions that follow are asked on ground they can stand on. Help costs nothing, but it
    // does say something, and this is the only place where saying it does not feel like a punishment.
    //
    // Seulement sur une question qui PARLE d'intervalles, et c'est une precision qui compte : passer une cellule
    // rythmique ou un accord n'apprend rien sur la palette d'intervalles, et la faire reculer serait une consequence
    // que le joueur ne pourrait relier a rien de ce qu'il vient de faire.
    if( ( m_currentQuestion.kind == QuestionKind::NamedInterval )
        || ( m_currentQuestion.kind == QuestionKind::Direction )
        || ( m_currentQuestion.kind == QuestionKind::Sing ) )
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

}    // namespace musichien::domain
