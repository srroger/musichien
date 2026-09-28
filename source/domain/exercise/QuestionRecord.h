#pragma once

// =====================================================================================================================
// Musichien - QuestionRecord
//
// Une question CONCLUE, telle qu'elle sera relue des mois plus tard : ce qui a ete demande, dans quel genre, et ce que
// le joueur en a fait.
//
// C'est la brique de base des statistiques, et elle est ecrite comme une DONNEE pure - pas de Qt, pas de fichier, pas
// d'horloge. Le temps entre par la porte (askedAt), exactement comme la position d'une frappe entre dans le jugement du
// rythme : le domaine ne mesure rien, il enregistre ce qu'on lui donne.
//
// ---------------------------------------------------------------------------------------------------------------------
// Pourquoi la LIGNE, et pas un compteur
//
// Un total ne se defait pas. Une ligne, si. Le jour ou le calcul du taux de reussite se trompe, on le refait a partir
// des lignes ; si c'est la ligne qui se trompe, il n'y a plus rien a refaire. C'est la meme raison qui fait qu'un
// journal d'ecriture precede toujours les totaux qui en decoulent.
// =====================================================================================================================

#include "domain/exercise/ExerciseSession.h"

#include <chrono>
#include <cstdint>

namespace musichien::domain
{

// Ce qu'une question est devenue.
enum class QuestionOutcome
{
    // Trouvee du premier coup : c'est ce qui distingue « savoir » de « finir par trouver ».
    CorrectFirstTry,

    // Trouvee, mais en s'y reprenant.
    CorrectAfterRetries,

    // Ratee, et la session s'est arretee la - plus de vies.
    Failed,

    // La reponse a ete donnee par l'application : une aide, pas un echec.
    Revealed
};

struct QuestionRecord
{
    // Quand la question a ete conclue. Fourni par l'appelant : le domaine n'a pas d'horloge.
    std::chrono::system_clock::time_point askedAt;

    QuestionKind kind{ QuestionKind::NamedInterval };

    // Ce que la question demandait, dans l'unite de son genre :
    //   * un intervalle -> des demi-tons (0 a 24) ;
    //   * un accord     -> l'index d'une qualite de chordLearningOrder() ;
    //   * une cellule   -> l'index d'une cellule de allRhythmPatterns().
    //
    // Un seul nombre, et le GENRE dit comment le lire. Deux champs separes auraient dit la meme chose en plus long, et
    // un champ polymorphe aurait oblige chaque lecteur a negocier son type.
    std::int32_t target{ 0 };

    IntervalDirection direction{ IntervalDirection::Ascending };

    QuestionOutcome outcome{ QuestionOutcome::Failed };

    // Combien d'essais ont ete necessaires, aide comprise : 1 veut dire du premier coup.
    std::int32_t attemptCount{ 1 };

    // Combien de fois le joueur a demande a reentendre la question. Ce n'est pas une faute : c'est une information sur
    // ce qui est difficile, et elle est deja payee en experience.
    std::int32_t replayCount{ 0 };

    [[nodiscard]] bool isCorrect() const noexcept
    {
        return ( outcome == QuestionOutcome::CorrectFirstTry ) || ( outcome == QuestionOutcome::CorrectAfterRetries );
    }
};

// Ce qu'une question est devenue, a partir de ce que la session en dit.
//
// UN SEUL endroit decide, et c'est ce qui garantit que le journal et l'ecran racontent la meme histoire. Une reponse
// juste du premier coup, une reponse juste apres s'y etre repris, et une reponse donnee par l'application sont trois
// choses differentes : les confondre ferait disparaitre exactement ce que les statistiques doivent montrer.
[[nodiscard]] QuestionOutcome outcomeOf( bool p_wasCorrect, bool p_wasRevealed, std::int32_t p_attemptCount ) noexcept;

}    // namespace musichien::domain
