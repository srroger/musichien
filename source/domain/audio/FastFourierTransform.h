#pragma once

// =====================================================================================================================
// Musichien - FastFourierTransform
//
// La transformee de Fourier discrete, en Cooley-Tukey : celle qui coupe chaque etape en deux moities et ramene le cout
// de N carre a N log N.
//
// Elle sert DEUX fois dans l'accordeur, et c'est ce qui justifie de l'ecrire plutot que de l'appeler ailleurs :
//   * l'autocorrelation, qui donne la periode d'un son, se calcule par transformee (Wiener-Khinchine). C'est ce qui
//     permet de descendre aux notes graves sans payer un calcul quadratique ;
//   * la position d'un pic dans le spectre affine la hauteur trouvee, et c'est ce qui rend les aigus justes : la ou
//     compter des echantillons n'a plus assez de finesse.
//
// Le domaine ne depend d'aucune bibliotheque externe : elle est donc ecrite ici, a la main, et testee comme le reste.
// =====================================================================================================================

#include <cstddef>
#include <vector>

namespace musichien::domain
{

class FastFourierTransform
{
public:
    // Une transformee rapide ne travaille que sur des tailles puissances de deux.
    [[nodiscard]] static bool supportsSize( std::size_t p_size ) noexcept;

    // La plus petite puissance de deux superieure ou egale a la taille demandee.
    [[nodiscard]] static std::size_t nextSize( std::size_t p_size ) noexcept;

    // Transforme EN PLACE. Les parties reelle et imaginaire sont deux vecteurs de meme taille, puissance de deux.
    // La transformee inverse divise par la taille, ce qui rend l'aller-retour sans effet sur le signal.
    static void apply( std::vector<double> & p_real, std::vector<double> & p_imaginary, bool p_inverse );
};

}    // namespace musichien::domain
