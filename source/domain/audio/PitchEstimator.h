#pragma once

// =====================================================================================================================
// Musichien - PitchEstimator
//
// Estime la hauteur d'un son, en hertz, par la methode YIN.
//
// Pourquoi dans le domaine : un estimateur de periode ne connait ni Qt, ni microphone, ni tampon audio - il recoit des
// nombres et rend un nombre. C'est ce qui permet de le TESTER : un sinus de 880 Hz doit etre lu a 880 Hz, et aucune
// oreille ne le prouve mieux qu'un test.
// =====================================================================================================================

#include <cstddef>
#include <span>

namespace musichien::domain
{

class PitchEstimator
{
public:
    // La plage de recherche, aussi large que la methode le permet - et pas plus, car au-dela les chiffres ne veulent
    // plus rien dire :
    //
    //   * en BAS, 50 Hz. Un estimateur de periode compare une fenetre a elle-meme decalee : il lui faut DEUX periodes
    //     dans la fenetre. Pour 20 Hz, cela demanderait 4410 echantillons, soit six fois plus de calcul que les 2048
    //     actuels - pour des infrasons qu'aucun instrument ne joue (le piano s'arrete a 27,5 Hz, une basse a 41).
    //   * en HAUT, 4000 Hz. A 44,1 kHz, deux demi-tons voisins y sont separes par 0,62 echantillon : c'est la
    //     derniere octave ou l'interpolation peut encore les distinguer. A 10 kHz l'ecart tombe a 0,25 echantillon et
    //     a 20 kHz a 0,12 : la valeur rendue serait un nombre, pas une note. 4000 Hz couvre deja tout : le do le plus
    //     haut d'un piano est a 4186 Hz, et rien d'usuel ne monte au-dela.
    static constexpr double MINIMUM_FREQUENCY_HZ = 50.0;
    static constexpr double MAXIMUM_FREQUENCY_HZ = 4000.0;

    // La fenetre d'analyse : assez longue pour resoudre une note grave, assez courte pour rester vivante.
    static constexpr std::size_t WINDOW_SIZE = 2048;

    // Le seuil de YIN : en dessous, la fenetre se ressemble assez pour qu'un decalage soit une periode.
    static constexpr double THRESHOLD = 0.15;

    // La hauteur en hertz, ou 0.0 quand rien de stable n'a ete trouve.
    [[nodiscard]] static double estimate( std::span<const double> p_window, double p_sampleRateHz );
};

}    // namespace musichien::domain
