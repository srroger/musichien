#include "domain/exercise/Trophy.h"

#include <array>

namespace musichien::domain
{

namespace
{

// LE SEUIL DE CHAQUE TITRE, en bilans REUSSIS d'affilee. Des nombres, et jamais une formule : un bareme s'ajuste, et un
// bareme qu'on peut relire se defend.
constexpr std::array<std::int64_t, TITLE_COUNT> TITLE_THRESHOLDS{ 0, 1, 2, 4, 7, 10 };

constexpr std::array<std::string_view, TITLE_COUNT> TITLE_NAMES{ "Toutou", "Chien de Tête", "Chef de Meute", "Soliste", "Chien de Concert", "Ouafstro" };

constexpr std::array<std::string_view, TITLE_COUNT> TITLE_MOTTOS{
  "Le premier pas dans la meute.",
  "Tu mènes déjà ta petite section.",
  "Une section entière suit ton oreille.",
  "Tu brilles seul, et ça s'entend.",
  "Le bras droit de l'orchestre.",
  "Le sommet. L'oreille qui dirige tout.",
};

}    // namespace

std::int64_t successStreakRequiredFor( Title p_title ) noexcept
{
    return TITLE_THRESHOLDS.at( static_cast<std::size_t>( p_title ) );
}

Title titleEarnedBy( const BilanRecord & p_record ) noexcept
{
    // On descend depuis le plus haut : le premier seuil atteint est celui qui merite d'etre porte. L'ecriture « depuis le
    // haut » est celle qui n'oublie pas un palier le jour ou l'on en ajoute un.
    for( std::size_t index = TITLE_COUNT; index > 0; --index )
    {
        const auto titleIndex = index - 1;

        if( p_record.longestSuccessStreak >= TITLE_THRESHOLDS.at( titleIndex ) )
        {
            return static_cast<Title>( titleIndex );
        }
    }

    return Title::Toutou;
}

std::string_view titleName( Title p_title ) noexcept
{
    return TITLE_NAMES.at( static_cast<std::size_t>( p_title ) );
}

std::string_view titleMotto( Title p_title ) noexcept
{
    return TITLE_MOTTOS.at( static_cast<std::size_t>( p_title ) );
}

std::vector<Trophy> trophiesFor( const BilanRecord & p_record )
{
    // Chaque trophee porte le seuil qu'il demande, et la VALEUR du joueur qu'il compare. Les ecrire cote a cote est ce qui
    // rend la table lisible - et ce qui empeche d'en ajouter un en oubliant de le mesurer.
    struct TrophyRule
    {
        std::string_view identifier;
        std::string_view name;
        std::string_view description;

        // Ce que le trophee regarde, et le seuil.
        std::int64_t value;
        std::int64_t required;
    };

    const std::array<TrophyRule, 6> rules{
      TrophyRule{ "first-bilan", "Premier Bilan", "Tu as passé ton premier examen.", p_record.bilanCount, 1 },
      TrophyRule{ "three-in-a-row", "Trois d'affilée", "Trois bilans réussis de suite.", p_record.longestSuccessStreak, 3 },
      TrophyRule{ "five-in-a-row", "Cinq d'affilée", "Cinq bilans réussis de suite.", p_record.longestSuccessStreak, 5 },
      TrophyRule{ "perfect-bilan", "Bilan parfait", "Un bilan sans une seule réponse révélée.", p_record.perfectBilanCount, 1 },
      TrophyRule{ "five-perfect", "Sans faute, cinq fois", "Cinq bilans parfaits.", p_record.perfectBilanCount, 5 },
      TrophyRule{ "ten-bilans", "Assidu", "Dix bilans joués.", p_record.bilanCount, 10 },
    };

    std::vector<Trophy> trophies;
    trophies.reserve( rules.size() );

    for( const TrophyRule & rule : rules )
    {
        Trophy trophy;
        trophy.identifier = rule.identifier;
        trophy.name = rule.name;
        trophy.description = rule.description;
        trophy.requiredCount = rule.required;
        trophy.earned = rule.value >= rule.required;

        trophies.push_back( trophy );
    }

    return trophies;
}

}    // namespace musichien::domain
