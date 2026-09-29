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
    // La plage de recherche, volontairement large : une voix aigue, un sifflement ou un instrument depassent le la
    // aigu, et une note qui mord sur la borne haute est une note qu'on lit faux.
    static constexpr double MINIMUM_FREQUENCY_HZ = 60.0;
    static constexpr double MAXIMUM_FREQUENCY_HZ = 1600.0;

    // La fenetre d'analyse : assez longue pour resoudre une note grave, assez courte pour rester vivante.
    static constexpr std::size_t WINDOW_SIZE = 2048;

    // Le seuil de YIN : en dessous, la fenetre se ressemble assez pour qu'un decalage soit une periode.
    static constexpr double THRESHOLD = 0.15;

    // La hauteur en hertz, ou 0.0 quand rien de stable n'a ete trouve.
    [[nodiscard]] static double estimate( std::span<const double> p_window, double p_sampleRateHz );
};

}    // namespace musichien::domain
