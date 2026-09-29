#include "domain/music/StaffPosition.h"

#include <array>
#include <cmath>
#include <cstddef>

namespace musichien::domain
{

namespace
{

// Le degre de chaque classe de hauteur depuis le do. Les cinq notes alterees tombent a un demi-degre, ce qui les pose
// entre deux positions au lieu de les empiler sur une seule.
constexpr std::array<double, 12> STEP_FROM_C{ 0.0, 0.5, 1.0, 1.5, 2.0, 3.0, 3.5, 4.0, 4.5, 5.0, 5.5, 6.0 };

// Une octave de degres diatoniques : do, re, mi, fa, sol, la, si. C'est ce sur quoi la boule se replie - sur les
// DEGRES, et non sur les douze demi-tons, sans quoi une alteration sortirait de la portee.
constexpr double SCALE_STEPS_PER_OCTAVE = 7.0;

// Le mi est quatre classes de hauteur au-dessus du do : c'est ce decalage qui met la ligne inferieure sur le mi.
constexpr std::int32_t E_PITCH_CLASS = 4;

[[nodiscard]] std::int32_t pitchClassOf( std::int32_t p_midiNumber ) noexcept
{
    return ( ( p_midiNumber % 12 ) + 12 ) % 12;
}

}    // namespace

std::int32_t StaffPosition::drawnMidiNumber( std::int32_t p_midiNumber ) noexcept
{
    // La meme classe de hauteur, ramenee dans l'octave de la portee : du mi 4 (ligne inferieure) au re diese 5, juste
    // sous la ligne superieure. C'est le meme choix que celui de fraction(), et il doit le rester - les deux disent
    // ou la note se dessine.
    const std::int32_t stepsAboveBottomLine = ( pitchClassOf( p_midiNumber ) - E_PITCH_CLASS + 12 ) % 12;

    return BOTTOM_LINE_MIDI_NUMBER + stepsAboveBottomLine;
}

double StaffPosition::fraction( std::int32_t p_midiNumber ) noexcept
{
    double stepFromBottomLine = STEP_FROM_C.at( static_cast<std::size_t>( pitchClassOf( p_midiNumber ) ) ) - 2.0;

    // Le repli : la note redescend d'une octave au lieu de sortir par le haut de la portee.
    stepFromBottomLine -= std::floor( stepFromBottomLine / SCALE_STEPS_PER_OCTAVE ) * SCALE_STEPS_PER_OCTAVE;

    // Zero sur la ligne inferieure, un sur la ligne superieure. La portee dessinee occupe HUIT degres - du mi grave
    // au fa aigu - et non sept : c'est a l'ecran de decider combien de place cela prend, pas au domaine.
    constexpr double SCALE_STEPS_ON_THE_STAFF = 8.0;

    return stepFromBottomLine / SCALE_STEPS_ON_THE_STAFF;
}

int StaffPosition::octaveShift( std::int32_t p_midiNumber ) noexcept
{
    // Deux notes de meme classe de hauteur different d'un nombre ENTIER d'octaves : la division tombe juste, et c'est
    // ce qui permet d'afficher un signe plutot qu'un nombre de demi-tons.
    return ( p_midiNumber - drawnMidiNumber( p_midiNumber ) ) / 12;
}

}    // namespace musichien::domain
