#pragma once

// =====================================================================================================================
// Musichien - StaffPosition
//
// Ou une hauteur se pose sur une portee de cle de sol, et de combien d'octaves la note REELLE est au-dessus ou en
// dessous de cet endroit.
//
// Les deux reponses sont differentes, et c'est tout l'interet de les separer : la BOULE est repliee sur l'octave pour
// ne jamais quitter les cinq lignes - c'est une decision de DESSIN - tandis que la NOTE garde son octave veritable,
// parce qu'un accordeur a besoin d'une hauteur ABSOLUE. Replier la note en meme temps que la boule ferait ecrire un do
// aigu la ou le joueur joue un do grave : l'ecran serait coherent avec lui-meme, et faux.
//
// La portee dessinee va du mi grave (premiere ligne, en bas) au fa aigu (cinquieme ligne, en haut) : une octave de
// degres diatoniques. Une note se place donc en DEGRES depuis la ligne inferieure, un degre valant un huitieme de la
// hauteur de la portee.
//
// Pourquoi dans le domaine : ranger une note, c'est de la theorie musicale, pas du dessin - et c'est testable sans une
// ligne de QML, ce qu'un calcul ecrit dans une interface n'est jamais.
// =====================================================================================================================

#include <cstdint>

namespace musichien::domain
{

class StaffPosition
{
public:
    // La ligne inferieure de la portee en cle de sol : le mi 4.
    static constexpr std::int32_t BOTTOM_LINE_MIDI_NUMBER = 64;

    // Ou la boule se pose, de 0 (ligne inferieure de la portee) a 1 (ligne superieure). La note est RAMENEE dans
    // l'octave dessinee : un do grave et un do aigu tombent exactement au meme endroit.
    [[nodiscard]] static double fraction( std::int32_t p_midiNumber ) noexcept;

    // Le nombre d'octaves entre la note REELLE et celle que la boule dessine. Negatif quand la vraie note est plus
    // grave que sa place, positif quand elle est plus aigue, zero quand la boule dit la verite entiere.
    [[nodiscard]] static int octaveShift( std::int32_t p_midiNumber ) noexcept;

    // La note que la boule dessine : la meme classe de hauteur, ramenee dans l'octave de la portee.
    [[nodiscard]] static std::int32_t drawnMidiNumber( std::int32_t p_midiNumber ) noexcept;
};

}    // namespace musichien::domain
