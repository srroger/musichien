#pragma once

// =====================================================================================================================
// Musichien - Trophy
//
// What the Bilan is worth beyond experience: TITLES, and TROPHIES.
//
// ---------------------------------------------------------------------------------------------------------------------
// Why the Bilan, and only the Bilan
//
// The arcade pays experience, and that is the ladder. The Bilan pays something else: it is the weekly EXAM, and a reward
// that has to be earned at a fixed moment is a reason to come back to that moment. Roger decided it - « note qu'on mettra
// surement des trophées et/ou des certificats, accessibles seulement via le Bilan ».
//
// A title says where the player IS (one, always, the highest he has reached). A trophy says what he HAS DONE (many, kept
// for good). The two are read in the profile.
//
// ---------------------------------------------------------------------------------------------------------------------
// The titles are orchestral, and the puns are the point
//
// Roger: « des titres inspirés de l'orchestre [...] mais en les détournant en jeu de mot rapport au chien ». The ladder
// runs from the mass of the ensemble to its leader: Toutou, Chien de Tête, Chef de Meute, Soliste, Chien de Concert,
// Ouafstro.
// =====================================================================================================================

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace musichien::domain
{

// Ce qu'un joueur a FAIT de ses bilans. Les quatre nombres viennent des preferences, et aucun n'est recalcule.
struct BilanRecord
{
    std::int64_t bilanCount{ 0 };

    // Les bilans ou aucune reponse n'a ete revelee ET ou tout a ete trouve du premier coup.
    std::int64_t perfectBilanCount{ 0 };

    // Les bilans REUSSIS de suite, en cours, et le plus long jamais atteint. C'est le second qui donne le titre : le
    // premier retombe des qu'on rate, et un joueur reparti de zero n'a pas perdu ce qu'il a fait.
    std::int64_t successStreak{ 0 };
    std::int64_t longestSuccessStreak{ 0 };
};

// Un titre, du plus modeste au plus haut. L'ORDRE compte : le titre du joueur est le plus haut qu'il ait merite.
enum class Title : std::size_t
{
    Toutou = 0,
    ChienDeTete = 1,
    ChefDeMeute = 2,
    Soliste = 3,
    ChienDeConcert = 4,
    Ouafstro = 5
};

inline constexpr std::size_t TITLE_COUNT = 6;

// La plus longue suite de bilans reussis qu'il faut pour meriter chaque titre. Zero pour le premier : tout le monde
// commence quelque part, et un jeu qui n'accorde rien au debut n'a pas de premier jour.
[[nodiscard]] std::int64_t successStreakRequiredFor( Title p_title ) noexcept;

// Le titre merite par ce qu'un joueur a fait, jamais moins que le premier.
[[nodiscard]] Title titleEarnedBy( const BilanRecord & p_record ) noexcept;

[[nodiscard]] std::string_view titleName( Title p_title ) noexcept;
[[nodiscard]] std::string_view titleMotto( Title p_title ) noexcept;

// Un trophee : ce qu'il demande, et s'il est acquis.
struct Trophy
{
    std::string_view identifier;
    std::string_view name;
    std::string_view description;

    // Le seuil a atteindre, et sur QUOI il porte - c'est ce qui permet a un test de verifier chaque trophee sans
    // reecrire sa condition.
    std::int64_t requiredCount{ 0 };

    bool earned{ false };
};

// Les trophees du jeu, avec leur etat pour ce joueur. L'ORDRE est celui de l'affichage.
[[nodiscard]] std::vector<Trophy> trophiesFor( const BilanRecord & p_record );

}    // namespace musichien::domain
