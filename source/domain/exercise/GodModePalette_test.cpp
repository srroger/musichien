#include "domain/exercise/GodModePalette.h"

#include "domain/exercise/LearningOrder.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <vector>

namespace musichien::domain
{

namespace
{

// Les reglages d'un joueur qui a TOUT allume : c'est le cas ou la regle « au moins deux » a quelque chose a dire sur les
// trois familles a la fois.
[[nodiscard]] SessionSettings everythingAskedSettings()
{
    SessionSettings settings;

    settings.namedIntervalQuestionShare = 40;
    settings.singQuestionShare = 20;
    settings.chordQuestionShare = 20;
    settings.modeNameQuestionShare = 20;

    return settings;
}

// Une palette qui satisfait la regle, pour servir de temoin.
[[nodiscard]] GodModePalette twoOfEachFamily()
{
    GodModePalette palette;
    palette.intervals = beginnerPalette( 2 );
    palette.chords = beginnerChordPalette( 2 );
    palette.modes = beginnerModePalette( 2 );

    return palette;
}

}    // namespace

TEST( GodModePaletteTest, every_level_becomes_a_model )
{
    // « Je debute » coche les deux premiers intervalles : c'est exactement ce que Roger a decrit, et ce modele doit etre
    // la palette que la session aurait prise elle-meme. Le meilleur moyen de le verifier est de comparer les deux.
    const GodModePalette beginner = paletteForLevel( PlayerLevel::Beginner );

    const SessionSettings beginnerSettings = sessionSettingsFor( PlayerLevel::Beginner );

    EXPECT_EQ( beginner.intervals.size(), beginnerSettings.startingPaletteSize );
    EXPECT_EQ( beginner.chords.size(), beginnerSettings.startingChordQualityCount );
    EXPECT_EQ( beginner.modes.size(), beginnerSettings.startingModeCount );

    // Et le modele d'un joueur qui maitrise est la carte ENTIERE : c'est ce que « je maitrise » veut dire.
    const GodModePalette master = paletteForLevel( PlayerLevel::Master );

    EXPECT_EQ( master.intervals.size(), SUPPORTED_INTERVAL_COUNT );
    EXPECT_EQ( master.chords.size(), CHORD_QUALITY_COUNT );
    EXPECT_EQ( master.modes.size(), MODE_COUNT );
}

TEST( GodModePaletteTest, the_playable_palette_holds_at_least_two_of_every_asked_family )
{
    // Avec UN seul element, la reponse serait toujours la meme : le joueur repondrait juste sans ecouter. La regle porte
    // donc sur les familles dont la part n'est pas nulle.
    const SessionSettings settings = everythingAskedSettings();

    EXPECT_TRUE( isPlayable( twoOfEachFamily(), settings ) );

    GodModePalette oneInterval = twoOfEachFamily();
    oneInterval.intervals = beginnerPalette( 1 );

    EXPECT_FALSE( isPlayable( oneInterval, settings ) );

    // Une famille ABSENTE n'est pas une famille vide : si le joueur n'ecoute jamais d'accords, ne pas en cocher un seul
    // ne lui coute aucune question, et l'exiger serait une regle sans objet.
    GodModePalette noChords = twoOfEachFamily();
    noChords.chords.clear();

    SessionSettings withoutChords = settings;
    withoutChords.chordQuestionShare = 0;

    EXPECT_TRUE( isPlayable( noChords, withoutChords ) );

    // Et la meme palette, avec la part des accords rallumee, ne tient plus : c'est la part du joueur qui decide, et non
    // une regle generale qu'on appliquerait a l'aveugle.
    EXPECT_FALSE( isPlayable( noChords, settings ) );
}

TEST( GodModePaletteTest, the_settings_carry_the_chosen_perimeter_and_freeze_it )
{
    // Le point qui distingue le GodMode d'un niveau : la palette est FIGEE. Sans cela, une reussite y ajouterait un
    // intervalle que le joueur n'a pas demande, et ce serait la conduite accompagnee que ce mode est fait pour casser.
    SessionSettings base = everythingAskedSettings();
    base.lives = 3;
    base.questionCount = 7;

    const GodModePalette palette = twoOfEachFamily();

    const SessionSettings settings = sessionSettingsFor( palette, base );

    EXPECT_TRUE( settings.paletteIsFixed );

    EXPECT_EQ( settings.intervalPalette.size(), 2U );
    EXPECT_EQ( settings.startingPaletteSize, 2U );
    EXPECT_EQ( settings.startingChordQualityCount, 2U );
    EXPECT_EQ( settings.startingModeCount, 2U );

    // La grille offre TOUT ce qui a ete coche : une grille qui en cacherait un ferait perdre la question qui porte
    // dessus.
    EXPECT_EQ( settings.choiceCount, settings.intervalPalette.size() );

    // Et tout le reste vient des reglages du joueur, jamais d'une valeur inventee ici.
    EXPECT_EQ( settings.lives, base.lives );
    EXPECT_EQ( settings.questionCount, base.questionCount );
    EXPECT_EQ( settings.chordQuestionShare, base.chordQuestionShare );
}

TEST( GodModePaletteTest, the_chosen_items_are_shown_in_the_learning_order )
{
    // Le joueur coche un ENSEMBLE : l'ordre d'affichage ne doit pas dependre de l'ordre de ses clics, sinon deux joueurs
    // ayant coche la meme chose verraient deux listes differentes.
    const std::vector<Interval> order{ learningOrderIntervals().begin(), learningOrderIntervals().end() };

    GodModePalette palette;

    // Les rangs 2 et 4 de l'ordre, coches A L'ENVERS.
    palette.intervals = { order.at( 3 ), order.at( 1 ) };

    const GodModePalette ordered = orderedPalette( palette );

    ASSERT_EQ( 2U, ordered.intervals.size() );

    // Le rang 2 passe donc devant le rang 4 : c'est l'ordre du JEU, et non celui des clics.
    EXPECT_EQ( order.at( 1 ).semitones(), ordered.intervals.at( 0 ).semitones() );
    EXPECT_EQ( order.at( 3 ).semitones(), ordered.intervals.at( 1 ).semitones() );
}

}    // namespace musichien::domain