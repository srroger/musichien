#pragma once

// =====================================================================================================================
// Musichien - QuestionStatistics
//
// Ce que le journal DIT, une fois agrege : combien de questions, combien de reussites, et ou sont les trous.
//
// Rien n'est stocke ici : tout se recalcule a partir des lignes. Un total enregistre serait un total a tenir a jour, a
// migrer, et a corriger le jour ou il se trompe - alors qu'un total recalcule ne peut pas mentir sur ses propres
// donnees.
//
// Le tri par CIBLE est ce qui rend la page utile : « 72 % de reussite » ne dit rien a personne, « tu rates les sixtes »
// dit tout. C'est aussi ce qui nourrira plus tard les exercices personnalises et le Bilan du week-end.
// =====================================================================================================================

#include "domain/exercise/QuestionRecord.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace musichien::domain
{

// Combien de questions, et comment elles se sont terminees.
struct QuestionStatistics
{
    std::size_t questionCount{ 0 };

    std::size_t correctFirstTryCount{ 0 };
    std::size_t correctAfterRetriesCount{ 0 };
    std::size_t failedCount{ 0 };
    std::size_t revealedCount{ 0 };

    // Les essais et les reecoutes, additionnes : ce sont les deux chiffres qui disent si une question est DEVENUE
    // facile, ou si elle est seulement finie par etre trouvee.
    std::int32_t totalAttemptCount{ 0 };
    std::int32_t totalReplayCount{ 0 };

    [[nodiscard]] std::size_t correctCount() const noexcept
    {
        return correctFirstTryCount + correctAfterRetriesCount;
    }

    // Le pourcentage de reussite, de 0 a 100. Sans aucune question, il vaut ZERO : une division par zero n'est pas une
    // statistique, et un ecran qui afficherait NaN ou 100 % pour rien mentirait dans les deux cas.
    [[nodiscard]] int successPercent() const noexcept;

    // La part des reussites trouvees DU PREMIER COUP : c'est la mesure du « su » plutot que du « trouve ».
    [[nodiscard]] int firstTryPercent() const noexcept;

    // Le nombre moyen de reecoutes par question, a un dixieme pres : une moyenne entiere perdrait tout l'interet du
    // chiffre (la difference entre 0,4 et 1,3 est justement celle qui compte).
    [[nodiscard]] double averageReplays() const noexcept;
};

// Les statistiques d'une CIBLE : un intervalle, une couleur d'accord, une cellule.
struct TargetStatistics
{
    QuestionKind kind{ QuestionKind::NamedInterval };

    // Ce que la cible est, dans l'unite de son genre (voir QuestionRecord::target).
    std::int32_t target{ 0 };

    // La direction, quand la cible en a une : une sixte montante et une sixte descendante ne se ratent pas pour la
    // meme raison, et les melanger effacerait precisement l'information.
    IntervalDirection direction{ IntervalDirection::Ascending };

    QuestionStatistics statistics;
};

// Ce qu'on regarde, et sur quoi on le filtre.
struct StatisticsFilter
{
    // Depuis quand. Un SEUL point dans le temps : les filtres de l'ecran (sept jours, trente jours, tout) s'y ramenent
    // tous, et « tout » se dit avec le point le plus ancien possible.
    std::chrono::system_clock::time_point since;

    // Aucun genre par defaut, donc tous : c'est ce qu'un joueur veut voir en ouvrant la page.
    std::optional<QuestionKind> kind;
};

// Le minimum de questions avant qu'une cible puisse etre designee comme un POINT FAIBLE.
//
// TROIS, et pas une : une cible ratee une seule fois n'est pas un point faible, c'est un hasard. Sert a construire un
// exercice personnalise sans lui faire dire n'importe quoi.
inline constexpr std::size_t MINIMUM_OBSERVATIONS_FOR_A_WEAKNESS = 3;

// Ce qu'une question fait perdre de temps, au minimum, quand elle est la seule de son bloc.
//
// Vingt secondes : le temps d'ecouter, de chercher, et de repondre. C'est une ESTIMATION, et elle est honnete de l'etre
// - ce qui compte pour un joueur, c'est « trois heures ce mois-ci », jamais la seconde pres.
inline constexpr std::chrono::seconds ASSUMED_QUESTION_DURATION{ 20 };

// Combien de temps le joueur a-t-il joue ?
//
// Le journal ne stocke PAS de durees : il stocke des INSTANTS, et c'est suffisant. Une session est une suite de questions
// rapprochees, et deux questions separees par une longue pause appartiennent a deux sessions - on additionne donc des
// BLOCS, chaque bloc valant l'ecart entre sa premiere et sa derniere question, plus le temps de la derniere.
//
// p_idleGap est ce qui separe deux sessions : dix minutes par defaut. Le joueur peut poser son telephone au milieu d'une
// partie pour repondre a un message sans que sa session se coupe en deux.
[[nodiscard]] std::chrono::seconds playTimeOf( std::span<const QuestionRecord> p_records,
                                               std::chrono::seconds p_idleGap = std::chrono::minutes{ 10 } );

[[nodiscard]] QuestionStatistics computeStatistics( std::span<const QuestionRecord> p_records,
                                                    const StatisticsFilter & p_filter );

// Les statistiques par cible, de la PLUS FAIBLE a la mieux reussie : les points faibles en tete, ce qui est exactement
// l'ordre dans lequel un joueur veut les lire.
[[nodiscard]] std::vector<TargetStatistics> statisticsByTarget( std::span<const QuestionRecord> p_records,
                                                                const StatisticsFilter & p_filter );

}    // namespace musichien::domain
