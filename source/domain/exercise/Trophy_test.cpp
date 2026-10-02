#include "domain/exercise/Trophy.h"

#include <gtest/gtest.h>

#include <algorithm>

namespace musichien::domain
{

namespace
{

// Un trophee par son identifiant, pour les tests qui cherchent un seul d'entre eux.
[[nodiscard]] Trophy trophyNamed( const std::vector<Trophy> & p_trophies, std::string_view p_identifier )
{
    const auto found = std::ranges::find_if( p_trophies, [p_identifier]( const Trophy & p_trophy ) {
        return p_trophy.identifier == p_identifier;
    } );

    EXPECT_NE( p_trophies.end(), found ) << "le trophee est absent de la liste";

    return ( found != p_trophies.end() ) ? *found : Trophy{};
}

}    // namespace

// LE TITRE : le plus haut merite, et jamais moins que le premier.
TEST( TrophyTest, the_title_is_the_highest_earned )
{
    // Un joueur neuf est un Toutou : il n'y a pas de titre vide.
    EXPECT_EQ( Title::Toutou, titleEarnedBy( BilanRecord{} ) );

    // Le titre se lit sur la PLUS LONGUE suite, et non sur celle en cours : un joueur dont la suite est retombee n'a pas
    // perdu ce qu'il a fait.
    BilanRecord record;
    record.successStreak = 0;
    record.longestSuccessStreak = 7;

    EXPECT_EQ( Title::ChienDeConcert, titleEarnedBy( record ) );

    // Et les seuils montent : chaque palier est atteignable, et demande plus que le precedent.
    EXPECT_EQ( 0, successStreakRequiredFor( Title::Toutou ) );

    for( std::size_t index = 1; index < TITLE_COUNT; ++index )
    {
        EXPECT_GT( successStreakRequiredFor( static_cast<Title>( index ) ),
                   successStreakRequiredFor( static_cast<Title>( index - 1 ) ) )
          << "les seuils doivent monter d'un titre a l'autre";
    }
}

// LES TROPHEES : chacun dit ce qu'il demande, et son etat suit le compte du joueur.
TEST( TrophyTest, every_trophy_tells_what_it_asks )
{
    // Un joueur neuf n'en a aucun, et la liste est complete.
    const std::vector<Trophy> fresh = trophiesFor( BilanRecord{} );

    ASSERT_FALSE( fresh.empty() );

    for( const Trophy & trophy : fresh )
    {
        EXPECT_FALSE( trophy.earned ) << "un joueur neuf ne doit avoir aucun trophee";
        EXPECT_GT( trophy.requiredCount, 0 ) << "un trophee sans seuil serait donne a tout le monde";
        EXPECT_FALSE( trophy.name.empty() );
    }

    // Un premier bilan, et le premier trophee tombe.
    BilanRecord afterOne;
    afterOne.bilanCount = 1;

    EXPECT_TRUE( trophyNamed( trophiesFor( afterOne ), "first-bilan" ).earned );
    EXPECT_FALSE( trophyNamed( trophiesFor( afterOne ), "ten-bilans" ).earned );

    // Trois bilans reussis d'affilee.
    BilanRecord afterThree;
    afterThree.longestSuccessStreak = 3;

    EXPECT_TRUE( trophyNamed( trophiesFor( afterThree ), "three-in-a-row" ).earned );
    EXPECT_FALSE( trophyNamed( trophiesFor( afterThree ), "five-in-a-row" ).earned );

    // Un bilan parfait, cinq parfaits, dix bilans.
    BilanRecord decorated;
    decorated.perfectBilanCount = 5;
    decorated.bilanCount = 10;

    const std::vector<Trophy> all = trophiesFor( decorated );

    EXPECT_TRUE( trophyNamed( all, "perfect-bilan" ).earned );
    EXPECT_TRUE( trophyNamed( all, "five-perfect" ).earned );
    EXPECT_TRUE( trophyNamed( all, "ten-bilans" ).earned );
}

}    // namespace musichien::domain
