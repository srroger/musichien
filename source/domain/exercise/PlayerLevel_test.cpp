#include "domain/exercise/PlayerLevel.h"

#include "domain/exercise/LearningOrder.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <initializer_list>

namespace musichien::domain
{

// ---------------------------------------------------------------------------------------------------------------------
// The level of the player
//
// A level answers one question - "where does this player start?" - and it must answer it without ever making
// the game unplayable. Those are the two things tested here.
// ---------------------------------------------------------------------------------------------------------------------

namespace
{

constexpr std::initializer_list<PlayerLevel> EVERY_LEVEL{ PlayerLevel::Beginner,
                                                          PlayerLevel::Fluent,
                                                          PlayerLevel::Advanced,
                                                          PlayerLevel::BeyondTheOctave,
                                                          PlayerLevel::Master };

}    // namespace

TEST( PlayerLevelTest, a_level_widens_the_palette_the_player_starts_with )
{
    const std::size_t beginnerPalette = sessionSettingsFor( PlayerLevel::Beginner ).startingPaletteSize;
    const std::size_t fluentPalette = sessionSettingsFor( PlayerLevel::Fluent ).startingPaletteSize;
    const std::size_t advancedPalette = sessionSettingsFor( PlayerLevel::Advanced ).startingPaletteSize;

    // The order is what the level IS: a player who says he is further along must not be handed the beginner's
    // two intervals, or the first session is a formality and the application is closed before the third
    // question.
    EXPECT_LT( beginnerPalette, fluentPalette );
    EXPECT_LT( fluentPalette, advancedPalette );

    // And "I know them up to the octave" is not a figure of speech: it is literally every SIMPLE interval,
    // which is what the learning order holds first.
    EXPECT_EQ( 12U, advancedPalette );
    EXPECT_LE( advancedPalette, learningOrderIntervals().size() );

    // Chaque niveau va plus loin que le precedent, jusqu'au dernier : celui-la ne joue plus une SELECTION,
    // il joue la carte entiere - et c'est sa definition plutot qu'un reglage.
    const std::size_t beyondTheOctavePalette = sessionSettingsFor( PlayerLevel::BeyondTheOctave ).startingPaletteSize;
    const std::size_t masterPalette = sessionSettingsFor( PlayerLevel::Master ).startingPaletteSize;

    EXPECT_LT( advancedPalette, beyondTheOctavePalette );
    EXPECT_LT( beyondTheOctavePalette, masterPalette );
    EXPECT_EQ( learningOrderIntervals().size(), masterPalette );
}

TEST( PlayerLevelTest, the_master_level_hands_nothing_to_the_player )
{
    // "Aucune aide" est UNE decision et non deux : ni l'indice qui souffle au premier essai, ni la reponse qui
    // se donne au troisieme. Couper l'un sans l'autre laisserait l'autre vendre la meme reponse.
    const SessionSettings master = sessionSettingsFor( PlayerLevel::Master );

    EXPECT_FALSE( master.aidsAllowed );

    // Et la grille ne propose plus une selection : elle propose tout ce que l'application connait.
    EXPECT_EQ( learningOrderIntervals().size(), master.choiceCount );
}

TEST( PlayerLevelTest, every_other_level_keeps_its_aids )
{
    // Un mode dur n'est dur que s'il est le SEUL. Si l'aide disparaissait partout, le mode sans filet ne
    // mesurerait plus rien de particulier.
    constexpr std::array<PlayerLevel, 4> ASSISTED_LEVELS{ PlayerLevel::Beginner,
                                                          PlayerLevel::Fluent,
                                                          PlayerLevel::Advanced,
                                                          PlayerLevel::BeyondTheOctave };

    for( const PlayerLevel level : ASSISTED_LEVELS )
    {
        EXPECT_TRUE( sessionSettingsFor( level ).aidsAllowed );
    }
}

TEST( PlayerLevelTest, every_level_produces_a_playable_session )
{
    for( const PlayerLevel level : EVERY_LEVEL )
    {
        const SessionSettings settings = sessionSettingsFor( level );

        // A palette of zero or one could not ask a question at all. That is the one thing a level must never
        // do, whatever it is tuned for.
        EXPECT_GE( settings.startingPaletteSize, 2U );
        EXPECT_LE( settings.startingPaletteSize, learningOrderIntervals().size() );

        EXPECT_GE( settings.successesBeforeWidening, 1U );

        // La meme chose pour les accords : une main vide, ou une seule couleur, ne poserait aucune question non plus.
        EXPECT_GE( settings.startingChordQualityCount, 2U );
        EXPECT_LE( settings.startingChordQualityCount, CHORD_QUALITY_COUNT );

        // And a level changes WHERE a player starts, never HOW the game is played: same ten questions, same
        // lives, same scoring.
        EXPECT_EQ( SessionSettings{}.questionCount, settings.questionCount );
        EXPECT_EQ( SessionSettings{}.lives, settings.lives );
    }
}

TEST( PlayerLevelTest, a_higher_level_opens_the_chords_wider )
{
    // Roger, apres avoir joue : « pour les accords, ca commence avec majeur mineur quelle que soit la difficulte. Il
    // faudrait que les accords disponibles soient directement nombreux si on augmente la difficulte. Majeur mineur
    // c'est pour les debutants. »
    //
    // Le niveau decide donc la main d'accords comme il decide la palette d'intervalles : un joueur qui se declare
    // "a l'aise" n'a pas a gagner les suspendues une par une.
    constexpr std::array<std::size_t, PLAYER_LEVEL_COUNT> EXPECTED_HAND_SIZE{ 2, 4, 6, 9, CHORD_QUALITY_COUNT };

    std::size_t previous = 0;

    for( std::size_t index = 0; index < PLAYER_LEVEL_COUNT; ++index )
    {
        const SessionSettings settings = sessionSettingsFor( playerLevelFromIndex( index ) );

        EXPECT_EQ( EXPECTED_HAND_SIZE.at( index ), settings.startingChordQualityCount ) << "niveau " << index;

        // Et la main ne se referme jamais en montant de niveau : c'est ce qui rend la progression lisible, et ce qui
        // empeche un niveau mal regle de retirer des accords a quelqu'un.
        EXPECT_GT( settings.startingChordQualityCount, previous ) << "niveau " << index;

        previous = settings.startingChordQualityCount;
    }

    // Le debutant a exactement les deux couleurs de base : c'est la que les distinguer EST l'exercice.
    EXPECT_EQ( 2U, sessionSettingsFor( PlayerLevel::Beginner ).startingChordQualityCount );
}

TEST( PlayerLevelTest, an_unknown_level_is_the_beginner_one )
{
    // Reading a preference means reading a file a curious player can edit. A value that is not a level must
    // cost him a setting, never a crash on start up.
    EXPECT_EQ( PlayerLevel::BeyondTheOctave, playerLevelFromIndex( 3 ) );
    EXPECT_EQ( PlayerLevel::Master, playerLevelFromIndex( 4 ) );

    // Un cran trop loin : plus un niveau, donc le premier - jamais une valeur tiree du hasard.
    EXPECT_EQ( PlayerLevel::Beginner, playerLevelFromIndex( 5 ) );
    EXPECT_EQ( PlayerLevel::Beginner, playerLevelFromIndex( 99 ) );

    EXPECT_EQ( PlayerLevel::Advanced, playerLevelFromIndex( 2 ) );
}

TEST( PlayerLevelTest, the_guided_mode_is_not_on_by_default )
{
    // Le mode guide n'est plus tire au hasard : il arrive apres deux erreurs de suite, et seulement la. Le defaut
    // reste donc zero, et c'est ce que ce test fige - un retour en arriere silencieux se verrait ici.
    for( const PlayerLevel level : EVERY_LEVEL )
    {
        EXPECT_EQ( 0, sessionSettingsFor( level ).directionQuestionShare );
    }
}

}    // namespace musichien::domain
