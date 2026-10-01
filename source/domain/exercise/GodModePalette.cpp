#include "domain/exercise/GodModePalette.h"

#include "domain/exercise/LearningOrder.h"

#include <algorithm>

namespace musichien::domain
{

GodModePalette paletteForLevel( PlayerLevel p_level )
{
    // Les reglages du niveau, et rien d'autre : ce sont EUX qui disent combien d'intervalles, d'accords et de modes un
    // debutant a en main. Les recopier ici aurait ete la deuxieme facon de repondre a la meme question.
    const SessionSettings settings = sessionSettingsFor( p_level );

    GodModePalette palette;
    palette.intervals = beginnerPalette( settings.startingPaletteSize );
    palette.chords = beginnerChordPalette( settings.startingChordQualityCount );
    palette.modes = beginnerModePalette( settings.startingModeCount );

    return palette;
}

GodModePalette orderedPalette( GodModePalette p_palette )
{
    const auto inLearningOrder = []( const auto & p_all, const auto & p_chosen ) {
        using Item = std::ranges::range_value_t<decltype( p_all )>;

        std::vector<Item> ordered;

        for( const Item & item : p_all )
        {
            if( std::ranges::find( p_chosen, item ) != p_chosen.end() )
            {
                ordered.push_back( item );
            }
        }

        return ordered;
    };

    GodModePalette ordered;
    ordered.intervals = inLearningOrder( learningOrderIntervals(), p_palette.intervals );
    ordered.chords = inLearningOrder( chordLearningOrder(), p_palette.chords );
    ordered.modes = inLearningOrder( modeLearningOrder(), p_palette.modes );

    return ordered;
}

bool isPlayable( const GodModePalette & p_palette, const SessionSettings & p_settings )
{
    // Une famille dont la part est nulle ne sera jamais posee : la laisser vide ne coute donc aucune question, et exiger
    // deux elements de plus serait une regle sans objet.
    const bool intervalsAsked = ( p_settings.namedIntervalQuestionShare + p_settings.singQuestionShare
                                  + p_settings.foreignNoteQuestionShare )
                                > 0;
    const bool chordsAsked = p_settings.chordQuestionShare > 0;
    const bool modesAsked = ( p_settings.modeColourQuestionShare + p_settings.modeNameQuestionShare
                              + p_settings.modeVampQuestionShare )
                            > 0;

    const bool intervalsOk = !intervalsAsked || ( p_palette.intervals.size() >= MINIMUM_GOD_MODE_CHOICES );
    const bool chordsOk = !chordsAsked || ( p_palette.chords.size() >= MINIMUM_GOD_MODE_CHOICES );
    const bool modesOk = !modesAsked || ( p_palette.modes.size() >= MINIMUM_GOD_MODE_CHOICES );

    return intervalsOk && chordsOk && modesOk;
}

SessionSettings sessionSettingsFor( const GodModePalette & p_palette, SessionSettings p_base )
{
    const GodModePalette ordered = orderedPalette( p_palette );

    p_base.intervalPalette = ordered.intervals;
    p_base.chordPalette = ordered.chords;
    p_base.modePalette = ordered.modes;

    // Les tailles de depart suivent les listes : c'est ce que la session lit pour savoir ce qu'elle a en main, et une
    // taille qui ne dirait pas la meme chose que la liste serait un mensonge a retardement.
    p_base.startingPaletteSize = ordered.intervals.size();
    p_base.startingChordQualityCount = ordered.chords.size();
    p_base.startingModeCount = ordered.modes.size();

    // La grille offre TOUT ce que le joueur a coche : c'est le principe du GodMode, et une grille qui cacherait un des
    // intervalles choisis lui ferait perdre la question qui porte dessus.
    p_base.choiceCount = ordered.intervals.size();

    // FIGEE : voir SessionSettings::paletteIsFixed. Le jeu n'ajoute plus rien sur une reussite, et ne retire plus rien
    // sur une erreur - c'est exactement ce que ce mode promet.
    p_base.paletteIsFixed = true;

    return p_base;
}

}    // namespace musichien::domain