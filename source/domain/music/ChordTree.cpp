#include "domain/music/ChordTree.h"

#include <array>
#include <string>

namespace musichien::domain
{

namespace
{

// Les degres de chaque couleur, du grave vers l'aigu.
//
// Cette table et celle des DEMI-TONS (Chord.cpp) disent la meme chose de deux facons, et le test de ChordTree les compare
// une a une : c'est ce qui garantit qu'une correction d'un cote ne laisse pas l'autre mentir.
constexpr std::array<ChordDegree, 3> MAJOR_DEGREES{ ChordDegree{ 1, 0 }, ChordDegree{ 3, 0 }, ChordDegree{ 5, 0 } };
constexpr std::array<ChordDegree, 3> MINOR_DEGREES{ ChordDegree{ 1, 0 }, ChordDegree{ 3, -1 }, ChordDegree{ 5, 0 } };
constexpr std::array<ChordDegree, 3> SUS4_DEGREES{ ChordDegree{ 1, 0 }, ChordDegree{ 4, 0 }, ChordDegree{ 5, 0 } };
constexpr std::array<ChordDegree, 3> SUS2_DEGREES{ ChordDegree{ 1, 0 }, ChordDegree{ 2, 0 }, ChordDegree{ 5, 0 } };

constexpr std::array<ChordDegree, 3> DIMINISHED_DEGREES{ ChordDegree{ 1, 0 },
                                                         ChordDegree{ 3, -1 },
                                                         ChordDegree{ 5, -1 } };

constexpr std::array<ChordDegree, 3> AUGMENTED_DEGREES{ ChordDegree{ 1, 0 }, ChordDegree{ 3, 0 }, ChordDegree{ 5, 1 } };

constexpr std::array<ChordDegree, 4> DOMINANT_SEVENTH_DEGREES{ ChordDegree{ 1, 0 },
                                                               ChordDegree{ 3, 0 },
                                                               ChordDegree{ 5, 0 },
                                                               ChordDegree{ 7, -1 } };

constexpr std::array<ChordDegree, 4> MAJOR_SEVENTH_DEGREES{ ChordDegree{ 1, 0 },
                                                            ChordDegree{ 3, 0 },
                                                            ChordDegree{ 5, 0 },
                                                            ChordDegree{ 7, 0 } };

constexpr std::array<ChordDegree, 4> MINOR_SEVENTH_DEGREES{ ChordDegree{ 1, 0 },
                                                            ChordDegree{ 3, -1 },
                                                            ChordDegree{ 5, 0 },
                                                            ChordDegree{ 7, -1 } };

constexpr std::array<ChordDegree, 4> SIXTH_DEGREES{ ChordDegree{ 1, 0 },
                                                    ChordDegree{ 3, 0 },
                                                    ChordDegree{ 5, 0 },
                                                    ChordDegree{ 6, 0 } };

constexpr std::array<ChordDegree, 4> HALF_DIMINISHED_DEGREES{ ChordDegree{ 1, 0 },
                                                              ChordDegree{ 3, -1 },
                                                              ChordDegree{ 5, -1 },
                                                              ChordDegree{ 7, -1 } };

// La septieme DIMINUEE, et le double bemol n'est pas une coquetterie : c'est la seule ecriture juste pour dire « une
// septieme, un demi-ton sous la septieme mineure ». L'ecrire « 6 » ferait perdre le fait que c'est une SEPTIEME.
constexpr std::array<ChordDegree, 4> DIMINISHED_SEVENTH_DEGREES{ ChordDegree{ 1, 0 },
                                                                 ChordDegree{ 3, -1 },
                                                                 ChordDegree{ 5, -1 },
                                                                 ChordDegree{ 7, -2 } };

constexpr std::array<ChordDegree, 4> MINOR_MAJOR_SEVENTH_DEGREES{ ChordDegree{ 1, 0 },
                                                                  ChordDegree{ 3, -1 },
                                                                  ChordDegree{ 5, 0 },
                                                                  ChordDegree{ 7, 0 } };

constexpr std::array<ChordDegree, 4> ADD9_DEGREES{ ChordDegree{ 1, 0 },
                                                   ChordDegree{ 3, 0 },
                                                   ChordDegree{ 5, 0 },
                                                   ChordDegree{ 9, 0 } };

constexpr std::array<ChordDegree, 5> NINTH_DEGREES{ ChordDegree{ 1, 0 },
                                                    ChordDegree{ 3, 0 },
                                                    ChordDegree{ 5, 0 },
                                                    ChordDegree{ 7, -1 },
                                                    ChordDegree{ 9, 0 } };

}    // namespace

// L'ARBRE, ecrit dans l'ORDRE DE LECTURE : les couleurs sont rangees par PROFONDEUR, et a l'interieur d'une profondeur
// par ordre d'apprentissage. Un parent vient donc toujours avant ses enfants, et un sous-arbre n'est plus force de
// pousser tout le reste vers le bas.
//
// C'est ce que fait cet ordre : les enfants directs du majeur se suivent, et l'ecran marque la separation par une petite
// MARCHE au lieu de laisser un trou.
//
// Les libelles de mutation sont en minuscules et sans accent : « tierce abaissee », « septieme mineure ajoutee ». C'est
// le GESTE qui compte, et il se lit sous le nom de l'accord comme la legende d'un arbre de competences.
constexpr std::array<ChordNode, 15> CHORD_TREE{ {
  // La racine : tout part de la, et un majeur ne s'obtient de rien.
  ChordNode{ ChordQuality::Major, ChordQuality::Major, "le point de depart", 0 },

  // LES ENFANTS DIRECTS DU MAJEUR : un seul geste, et ils se suivent a l'ecran.
  ChordNode{ ChordQuality::Minor, ChordQuality::Major, "tierce abaissee", 1 },
  ChordNode{ ChordQuality::Sus4, ChordQuality::Major, "tierce remplacee par la quarte", 1 },
  ChordNode{ ChordQuality::Sus2, ChordQuality::Major, "tierce remplacee par la seconde", 1 },
  ChordNode{ ChordQuality::Augmented, ChordQuality::Major, "quinte augmentee", 1 },
  ChordNode{ ChordQuality::Sixth, ChordQuality::Major, "sixte ajoutee", 1 },
  ChordNode{ ChordQuality::MajorSeventh, ChordQuality::Major, "septieme majeure ajoutee", 1 },
  ChordNode{ ChordQuality::Add9, ChordQuality::Major, "neuvieme ajoutee", 1 },
  ChordNode{ ChordQuality::DominantSeventh, ChordQuality::Major, "septieme mineure ajoutee", 1 },

  // PUIS CE QUI DESCEND DU MINEUR : un geste de plus.
  ChordNode{ ChordQuality::Diminished, ChordQuality::Minor, "quinte abaissee", 2 },
  ChordNode{ ChordQuality::MinorSeventh, ChordQuality::Minor, "septieme mineure ajoutee", 2 },
  ChordNode{ ChordQuality::MinorMajorSeventh, ChordQuality::Minor, "septieme majeure ajoutee", 2 },

  // Et ce qui descend de la dominante.
  ChordNode{ ChordQuality::Ninth, ChordQuality::DominantSeventh, "neuvieme ajoutee", 2 },

  // LES SONS LES PLUS TENDUS, au bout de la branche mineure.
  ChordNode{ ChordQuality::HalfDiminished, ChordQuality::Diminished, "septieme mineure ajoutee", 3 },
  ChordNode{ ChordQuality::DiminishedSeventh, ChordQuality::Diminished, "septieme diminuee ajoutee", 3 },
} };

std::int32_t semitonesOfDegree( ChordDegree p_degree ) noexcept
{
    // La distance d'un degre a la tonique, en demi-tons.
    //
    // Ecrit comme le compte un musicien : la tierce vaut QUATRE demi-tons, et son alteration deplace d'un demi-ton - ou
    // de deux, pour un double bemol.
    switch( p_degree.degree )
    {
        case 1:
            return 0 + p_degree.alteration;

        case 2:
            return 2 + p_degree.alteration;

        case 3:
            return 4 + p_degree.alteration;

        case 4:
            return 5 + p_degree.alteration;

        case 5:
            return 7 + p_degree.alteration;

        case 6:
            return 9 + p_degree.alteration;

        case 7:
            return 11 + p_degree.alteration;

        case 9:
            // La neuvieme est une seconde, une octave plus haut : deux demi-tons plus douze.
            return 14 + p_degree.alteration;

        default:
            return p_degree.alteration;
    }
}

std::span<const ChordDegree> chordDegrees( ChordQuality p_quality ) noexcept
{
    switch( p_quality )
    {
        case ChordQuality::Major:
            return MAJOR_DEGREES;

        case ChordQuality::Minor:
            return MINOR_DEGREES;

        case ChordQuality::Sus4:
            return SUS4_DEGREES;

        case ChordQuality::Sus2:
            return SUS2_DEGREES;

        case ChordQuality::Diminished:
            return DIMINISHED_DEGREES;

        case ChordQuality::Augmented:
            return AUGMENTED_DEGREES;

        case ChordQuality::DominantSeventh:
            return DOMINANT_SEVENTH_DEGREES;

        case ChordQuality::MajorSeventh:
            return MAJOR_SEVENTH_DEGREES;

        case ChordQuality::MinorSeventh:
            return MINOR_SEVENTH_DEGREES;

        case ChordQuality::Sixth:
            return SIXTH_DEGREES;

        case ChordQuality::HalfDiminished:
            return HALF_DIMINISHED_DEGREES;

        case ChordQuality::DiminishedSeventh:
            return DIMINISHED_SEVENTH_DEGREES;

        case ChordQuality::MinorMajorSeventh:
            return MINOR_MAJOR_SEVENTH_DEGREES;

        case ChordQuality::Add9:
            return ADD9_DEGREES;

        case ChordQuality::Ninth:
            return NINTH_DEGREES;
    }

    return MAJOR_DEGREES;
}

std::string chordDegreesLabel( ChordQuality p_quality )
{
    std::string label;

    for( const ChordDegree & degree : chordDegrees( p_quality ) )
    {
        if( !label.empty() )
        {
            label += ' ';
        }

        // Un bemol, deux bemols, un diese... et en ASCII, volontairement : le caractere typographique du bemol n'existe
        // pas dans toutes les polices de telephone, et un carre vide sous chaque accord serait pire qu'un « b ».
        if( degree.alteration < 0 )
        {
            label.append( static_cast<std::size_t>( -degree.alteration ), 'b' );
        }
        else if( degree.alteration > 0 )
        {
            label.append( static_cast<std::size_t>( degree.alteration ), '#' );
        }

        label += std::to_string( degree.degree );
    }

    return label;
}

std::span<const ChordNode> chordTree() noexcept
{
    return CHORD_TREE;
}

}    // namespace musichien::domain
