#include "domain/music/Mode.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace musichien::domain
{

namespace
{

// La quinte juste : le pas du cercle des quintes, et il ne depend d'aucun mode. Douze quintes font sept octaves, donc
// la marche fait le tour des DOUZE classes de hauteur sans jamais repasser deux fois sur la meme.
constexpr std::int32_t FIFTH_IN_SEMITONES = 7;

// Les identifiants, dans l'ORDRE DE L'ENUMERATION.
//
// Une seule liste, lue dans les deux sens (le nom d'un mode, et le mode d'un nom) : c'est ce qui garantit qu'un nom
// et son mode ne peuvent pas se desynchroniser. La table s'ecrit une fois, et les deux fonctions y puisent.
constexpr std::array<std::string_view, MODE_COUNT> MODE_IDENTIFIERS{ "lydian", "ionian", "mixolydian", "dorian", "aeolian", "phrygian", "locrian" };

}    // namespace

std::vector<Note> notesOfMode( Note p_tonic, Mode p_mode )
{
    std::vector<Note> notes;
    notes.reserve( DEGREE_COUNT );

    const std::int32_t tonicMidiNumber = p_tonic.midiNumber();

    for( const std::int32_t offset : modeDegreeOffsets( p_mode ) )
    {
        notes.emplace_back( tonicMidiNumber + offset );
    }

    return notes;
}

std::string_view modeIdentifier( Mode p_mode ) noexcept
{
    return MODE_IDENTIFIERS.at( modeIndex( p_mode ) );
}

std::span<const Mode> modeLearningOrder()
{
    // L'ordre est ecrit UNE fois, et il est STATIQUE : un span sur une variable locale serait un pointeur vers un
    // cadavre. L'ordre de la liste est explique en detail dans l'en-tete, parce qu'il se discute.
    static const std::array<Mode, MODE_COUNT> ORDER{ Mode::Ionian, Mode::Aeolian, Mode::Mixolydian, Mode::Dorian, Mode::Lydian, Mode::Phrygian, Mode::Locrian };

    return ORDER;
}

std::vector<Mode> beginnerModePalette( std::size_t p_modeCount )
{
    const std::span<const Mode> order = modeLearningOrder();

    // JAMAIS MOINS DE DEUX MODES.
    //
    // Une question de couleur compare deux modes, et une palette d'un seul ne pourrait pas la poser : le domaine
    // refuse donc cet etat tout de suite, plutot que de laisser la session le decouvrir au moment de tirer une
    // question - c'est-a-dire au pire moment.
    const std::size_t count = std::clamp( p_modeCount, std::size_t{ 2 }, order.size() );

    return std::vector<Mode>{ order.begin(), order.begin() + static_cast<std::ptrdiff_t>( count ) };
}

std::array<CircleNote, SEMITONES_PER_OCTAVE> modeCircleNotes( Mode p_mode, std::int32_t p_tonicPitchClass ) noexcept
{
    // Une classe de hauteur est ramenee dans [0, 12) une fois pour toutes : une tonique donnee par un numero MIDI peut
    // etre negative ou depasser l'octave, et la suite du calcul n'a plus a s'en soucier.
    const auto wrapPitchClass = []( std::int32_t p_pitchClass ) noexcept {
        return ( ( p_pitchClass % SEMITONES_PER_OCTAVE ) + SEMITONES_PER_OCTAVE ) % SEMITONES_PER_OCTAVE;
    };

    const std::int32_t tonicPitchClass = wrapPitchClass( p_tonicPitchClass );

    // Les sept notes du mode, ramenees a des classes de hauteur : c'est tout ce dont le cercle a besoin.
    std::array<bool, SEMITONES_PER_OCTAVE> belongsToMode{};

    for( const std::int32_t offset : modeDegreeOffsets( p_mode ) )
    {
        belongsToMode.at( static_cast<std::size_t>( wrapPitchClass( tonicPitchClass + offset ) ) ) = true;
    }

    std::array<CircleNote, SEMITONES_PER_OCTAVE> circle{};

    std::int32_t pitchClass = tonicPitchClass;

    for( std::size_t index = 0; index < circle.size(); ++index )
    {
        const auto position = static_cast<std::size_t>( pitchClass );

        circle.at( index ) = CircleNote{ .pitchClassIndex = pitchClass,
                                         .belongsToMode = belongsToMode.at( position ),
                                         .isTonic = ( index == 0 ) };

        // La quinte juste, et le retour dans [0, 12) : c'est ce qui fait le tour du cercle une fois et une seule.
        pitchClass = wrapPitchClass( pitchClass + FIFTH_IN_SEMITONES );
    }

    return circle;
}

std::optional<Mode> modeFromIdentifier( std::string_view p_identifier ) noexcept
{
    for( std::size_t index = 0; index < MODE_COUNT; ++index )
    {
        if( MODE_IDENTIFIERS.at( index ) == p_identifier )
        {
            return static_cast<Mode>( index );
        }
    }

    return std::nullopt;
}

}    // namespace musichien::domain
