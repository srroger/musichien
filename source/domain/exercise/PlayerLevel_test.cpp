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
    // Les accords offerts ne peuvent pas rester « majeur, mineur » quelle que soit la difficulte : ils s'ouvrent avec le
    // niveau, parce que deux couleurs, c'est un terrain de debutant.
    //
    // Le niveau decide donc la main d'accords comme il decide la palette d'intervalles : un joueur qui se declare
    // "a l'aise" n'a pas a gagner les septiemes une par une.
    //
    // LES VALEURS ONT CHANGE le 01/10/2026 : 4, 7 et 11 au lieu de 4, 6 et 9. Les quatre premiers accords sont desormais le
    // majeur, le mineur, le 7 de dominante et le maj7 ; le sus est parti aux composes. La progression complete est lue en
    // clair par every_level_says_exactly_what_it_gives, juste en dessous.
    constexpr std::array<std::size_t, PLAYER_LEVEL_COUNT> EXPECTED_HAND_SIZE{ 2, 4, 7, 11, CHORD_QUALITY_COUNT };

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

TEST( PlayerLevelTest, every_level_says_exactly_what_it_gives )
{
    // LA PROGRESSION, ECRITE ICI POUR ETRE LUE.
    //
    // Elle vit dans deux listes ordonnees (Chord.cpp, Mode.cpp) et dans les nombres de PlayerLevel.cpp : personne ne peut la
    // relire sans jouer dix parties. Elle est donc figee ici, en clair, et c'est ce test qui dit si un palier a bouge.
    //
    // Roger l'a mise au point le 01/10/2026, apres avoir vu dans le GodMode que les paliers etaient inegaux - « la
    // progression est assez inegale ». Deux principes, et il faut les deux : la FREQUENCE dans le repertoire et la
    // DIFFICULTE a l'entendre, qui n'est pas la difficulte a l'ecrire.
    const SessionSettings beginner = sessionSettingsFor( PlayerLevel::Beginner );

    EXPECT_EQ( 2U, beginner.startingPaletteSize );
    EXPECT_EQ( 2U, beginner.startingChordQualityCount );
    EXPECT_EQ( 2U, beginner.startingModeCount );

    // A l'aise : les QUATRE accords qu'on rencontre en premier, et les deux modes les plus contrastes des cinq restants.
    const SessionSettings fluent = sessionSettingsFor( PlayerLevel::Fluent );

    EXPECT_EQ( 5U, fluent.startingPaletteSize );
    EXPECT_EQ( 4U, fluent.startingChordQualityCount );
    EXPECT_EQ( 4U, fluent.startingModeCount );

    EXPECT_EQ( ( std::vector<ChordQuality>{ ChordQuality::Major,
                                            ChordQuality::Minor,
                                            ChordQuality::DominantSeventh,
                                            ChordQuality::MajorSeventh } ),
               beginnerChordPalette( fluent.startingChordQualityCount ) );

    EXPECT_EQ( ( std::vector<Mode>{ Mode::Ionian, Mode::Aeolian, Mode::Mixolydian, Mode::Phrygian } ),
               beginnerModePalette( fluent.startingModeCount ) );

    // Jusqu'a l'octave : la famille FONCTIONNELLE des accords, et six modes sur sept.
    const SessionSettings advanced = sessionSettingsFor( PlayerLevel::Advanced );

    EXPECT_EQ( 12U, advanced.startingPaletteSize );
    EXPECT_EQ( 7U, advanced.startingChordQualityCount );
    EXPECT_EQ( 6U, advanced.startingModeCount );

    // Les composes : les COULEURS plutot que les fonctions, et les sept modes - « et la, on a tous les modes ».
    const SessionSettings beyond = sessionSettingsFor( PlayerLevel::BeyondTheOctave );

    EXPECT_EQ( 18U, beyond.startingPaletteSize );
    EXPECT_EQ( 11U, beyond.startingChordQualityCount );
    EXPECT_EQ( MODE_COUNT, beyond.startingModeCount );

    // Et la carte entiere, sans aucune aide : le niveau ou le joueur se mesure.
    const SessionSettings master = sessionSettingsFor( PlayerLevel::Master );

    EXPECT_EQ( SUPPORTED_INTERVAL_COUNT, master.startingPaletteSize );
    EXPECT_EQ( CHORD_QUALITY_COUNT, master.startingChordQualityCount );
    EXPECT_EQ( MODE_COUNT, master.startingModeCount );
    EXPECT_FALSE( master.aidsAllowed );
}

TEST( PlayerLevelTest, the_experience_earned_offers_the_next_step )
{
    // Le jeu PROPOSE, il n'impose pas : ce que l'experience merite, et les seuils qui le disent.
    //
    // Roger les a voulus LARGES - « pour aller haut, il faut faire de longues series » - et une partie de dix questions vaut
    // entre cent et deux cents points : les paliers se comptent donc en dizaines de parties, jamais en une soiree.
    EXPECT_EQ( PlayerLevel::Beginner, levelEarnedBy( 0 ) );
    EXPECT_EQ( PlayerLevel::Beginner, levelEarnedBy( 249 ) );
    EXPECT_EQ( PlayerLevel::Fluent, levelEarnedBy( 250 ) );
    EXPECT_EQ( PlayerLevel::Fluent, levelEarnedBy( 999 ) );
    EXPECT_EQ( PlayerLevel::Advanced, levelEarnedBy( 1000 ) );
    EXPECT_EQ( PlayerLevel::BeyondTheOctave, levelEarnedBy( 2500 ) );
    EXPECT_EQ( PlayerLevel::Master, levelEarnedBy( 6000 ) );

    // Et il n'y a AUCUN plafond : celui qui joue beaucoup continue d'ouvrir le jeu - « sky is the limit ».
    EXPECT_EQ( PlayerLevel::Master, levelEarnedBy( 1'000'000 ) );

    // Une experience negative - un fichier edite a la main - ne merite pas moins que le premier palier : le jeu propose, il
    // ne retire jamais.
    EXPECT_EQ( PlayerLevel::Beginner, levelEarnedBy( -50 ) );

    // Et les deux fonctions se REPONDENT : le seuil d'un niveau est exactement l'experience qui fait qu'on le merite. C'est
    // ce qui garantit qu'un seuil deplace ne laisse pas un palier inatteignable.
    for( const PlayerLevel level : EVERY_LEVEL )
    {
        EXPECT_EQ( level, levelEarnedBy( experienceRequiredFor( level ) ) )
          << "niveau " << static_cast<int>( level );
    }
}

}    // namespace musichien::domain
