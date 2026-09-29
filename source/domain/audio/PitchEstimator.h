#pragma once

// =====================================================================================================================
// Musichien - PitchEstimator
//
// Estime la hauteur d'un son, en hertz. Deux etages, parce qu'aucune des deux familles de methodes ne suffit seule :
//
//   1. la PERIODE, par la methode YIN - robuste, insensible aux harmoniques manquantes, excellente dans les graves ;
//   2. le SPECTRE, pour affiner - le comptage d'echantillons perd sa finesse des que l'oscillation raccourcit, alors
//      que la position d'un pic dans le spectre, elle, reste precise.
//
// Pourquoi dans le domaine : un estimateur de hauteur ne connait ni Qt, ni microphone, ni tampon audio - il recoit des
// nombres et rend un nombre. C'est ce qui permet de le TESTER : un la de 440 Hz doit etre lu a 440 Hz.
// =====================================================================================================================

#include <cstddef>
#include <span>

namespace musichien::domain
{

class PitchEstimator
{
public:
    // La plage de recherche, celle d'un accordeur qui veut servir a toutes les circonstances :
    //
    //   * en BAS, 30 Hz : le si0 d'une basse cinq cordes, et le mi1 (41 Hz) d'une basse ou d'une contrebasse
    //     ordinaire. Il faut deux periodes dans la fenetre pour mesurer une oscillation, donc c'est la taille de la
    //     fenetre qui fixe ce plancher - 4096 echantillons tiennent deux periodes de 21 Hz, avec la marge qu'exige
    //     une mesure honnete.
    //   * en HAUT, 4500 Hz : le do le plus haut d'un piano est a 4186 Hz, et rien d'usuel ne monte au-dela. C'est
    //     l'affinage par le spectre qui rend cette zone juste, la ou le comptage d'echantillons ne distingue plus
    //     deux demi-tons.
    static constexpr double MINIMUM_FREQUENCY_HZ = 30.0;
    static constexpr double MAXIMUM_FREQUENCY_HZ = 4500.0;

    // La fenetre d'analyse : 4096 echantillons, soit un peu moins de cent millisecondes a 44,1 kHz. Assez longue pour
    // tenir deux periodes d'une note grave, assez courte pour qu'une note tenue fasse encore bouger l'affichage.
    static constexpr std::size_t WINDOW_SIZE = 4096;

    // Le seuil de YIN : en dessous, la fenetre se ressemble assez pour qu'un decalage soit une periode.
    static constexpr double THRESHOLD = 0.15;

    // La hauteur en hertz, ou 0.0 quand rien de stable n'a ete trouve.
    [[nodiscard]] static double estimate( std::span<const double> p_window, double p_sampleRateHz );
};

}    // namespace musichien::domain

