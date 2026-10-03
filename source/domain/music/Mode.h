#pragma once

// =====================================================================================================================
// Musichien - Mode
//
// Les sept modes de la musique occidentale, comme FORMULES : la suite des degres, en demi-tons depuis la tonique.
//
// ---------------------------------------------------------------------------------------------------------------------
// Pourquoi une formule, et non sept gammes ecrites a la main
//
// Parce qu'un mode n'est pas une collection de notes : c'est une collection AVEC UN CENTRE (voir la note 27 du vault).
// Les sept modes separent exactement la meme gamme, donc ecrire sept tables serait ecrire sept fois la meme chose - et
// se donner sept occasions de les desynchroniser l'une de l'autre.
//
// La gamme majeure est donc la SEULE table du fichier, et chaque mode en est une ROTATION. Une rotation ne peut pas
// se tromper.
//
// ---------------------------------------------------------------------------------------------------------------------
// L'ordre de l'enumeration est l'ORDRE DE COULEUR, du plus clair au plus sombre
//
// Lydien, Ionien, Mixolydien, Dorien, Eolien, Phrygien, Locrien : c'est l'ordre dans lequel Roger entend les modes,
// et il a une propriete qui n'est pas un hasard - deux modes VOISINS dans cette liste ne different que d'UNE SEULE
// NOTE. Le pas suivant eteint une note, le pas suivant en rallume une autre, et c'est ce qui rend l'ecart audible en
// un seul geste.
//
// C'est exactement la propriete dont l'exercice du degrade a besoin : il parcourt cette liste dans un sens pour
// assombrir, dans l'autre pour eclaircir, et n'a jamais besoin d'une seconde table.
//
// Le meme ordre range aussi les centres sur le cercle des quintes (F C G D A E B) et fait descendre les notes
// eteintes le long de ce cercle. Voir la note 27, §3.
// =====================================================================================================================

#include "domain/music/Note.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace musichien::domain
{

enum class Mode : std::size_t
{
    // La plus claire, et la seule a porter une quarte AUGMENTEE : c'est sa couleur.
    Lydian = 0,

    // La gamme majeure. La reference : c'est d'elle que tout le reste se dit.
    Ionian = 1,

    // La septieme est mineure. Le mixolydien du blues, et le mode de la deuxieme position d'harmonica.
    Mixolydian = 2,

    // La tierce est mineure : la premiere ombre, et le seul pas qui change de FAMILLE (majeur vers mineur).
    Dorian = 3,

    // La gamme mineure naturelle. La reference des ombres.
    Aeolian = 4,

    // La seconde est mineure : la couleur flamenco, une tension des la premiere marche.
    Phrygian = 5,

    // La plus sombre : la quinte elle-meme est diminuee, donc rien n'y repose vraiment.
    Locrian = 6
};

inline constexpr std::size_t MODE_COUNT = 7;

// Nombre de degres d'un mode. Sept, comme la gamme dont il vient.
inline constexpr std::size_t DEGREE_COUNT = 7;

// La gamme majeure, en demi-tons depuis sa tonique : la SEULE table ecrite a la main de ce fichier.
inline constexpr std::array<std::int32_t, DEGREE_COUNT> MAJOR_SCALE_DEGREE_OFFSETS{ 0, 2, 4, 5, 7, 9, 11 };

// Le degre de la gamme majeure dont chaque mode part.
//
// L'ordre de couleur n'est PAS l'ordre de rotation, et cette table est la seule chose qui le dise. Elle se lit dans
// les deux sens : le lydien est la gamme majeure vue depuis sa quarte, le locrien la meme gamme vue depuis sa
// septieme.
inline constexpr std::array<std::int32_t, MODE_COUNT> MODE_ROTATION_INDEX{ 3, 0, 4, 1, 5, 2, 6 };

// Le rang du mode, pour les listes et les boucles : c'est l'ordre de couleur, et il est stable.
//
// Declare AVANT modeDegreeOffsets, qui s'en sert : c'est la seule raison de sa place ici.
[[nodiscard]] constexpr std::size_t modeIndex( Mode p_mode ) noexcept
{
    return static_cast<std::size_t>( p_mode );
}

// Les degres d'un mode, en demi-tons depuis sa tonique : 0 pour le premier, et une valeur dans [0, 11] pour chacun.
//
// Calcule par rotation de la gamme majeure, et jamais ecrit a la main : un mode qui ne serait plus une rotation
// serait un mode faux.
[[nodiscard]] constexpr std::array<std::int32_t, DEGREE_COUNT> modeDegreeOffsets( Mode p_mode ) noexcept
{
    const auto rotation = static_cast<std::size_t>( MODE_ROTATION_INDEX.at( modeIndex( p_mode ) ) );
    const std::int32_t rotationOffset = MAJOR_SCALE_DEGREE_OFFSETS.at( rotation );

    std::array<std::int32_t, DEGREE_COUNT> offsets{};

    for( std::size_t degree = 0; degree < DEGREE_COUNT; ++degree )
    {
        const std::int32_t scaleOffset = MAJOR_SCALE_DEGREE_OFFSETS.at( ( rotation + degree ) % DEGREE_COUNT );

        // Le modulo POSITIF est necessaire : la gamme majeure retombe quand elle repasse par sa tonique, et une
        // difference negative donnerait un degre sous la tonique.
        offsets.at( degree ) = ( ( scaleOffset - rotationOffset ) + SEMITONES_PER_OCTAVE ) % SEMITONES_PER_OCTAVE;
    }

    return offsets;
}

// LE DEGRE QUI COLORE UN MODE : celui qui le distingue de ses voisins dans l'ordre de couleur.
//
// Zero quand le mode n'a rien a demontrer - l'ionien et l'eolien sont les deux references, et c'est justement pour cela
// qu'elles servent de reference.
//
// Il vit ICI parce que c'est une regle SUR LES MODES, et non sur les phrases : le generateur s'en sert pour insister sur
// la bonne note, et l'ecran s'en sert pour la MONTRER. Deux copies finiraient par se contredire, et le joueur verrait une
// note mise en avant que la phrase ne joue pas davantage que les autres.
[[nodiscard]] constexpr std::int32_t modeCharacteristicDegree( Mode p_mode ) noexcept
{
    switch( p_mode )
    {
        case Mode::Lydian:
            return 4;

        case Mode::Mixolydian:
            return 7;

        case Mode::Dorian:
            return 6;

        case Mode::Phrygian:
            return 2;

        case Mode::Locrian:
            return 5;

        case Mode::Ionian:
        case Mode::Aeolian:
            return 0;
    }

    return 0;
}

// L'ecart, en demi-tons, entre un degre et la tonique. Le degre se compte de 1 a 7, comme un musicien le dit.
[[nodiscard]] constexpr std::int32_t degreeOffset( Mode p_mode, std::int32_t p_degree ) noexcept
{
    const auto index = static_cast<std::size_t>( ( p_degree - 1 ) % static_cast<std::int32_t>( DEGREE_COUNT ) );

    return modeDegreeOffsets( p_mode ).at( index );
}

// Les notes d'un mode, sur une tonique donnee, du premier degre au septieme.
//
// Les notes restent dans l'octave de la tonique : c'est une GAMME, pas une melodie. Une melodie monte et descend, et
// c'est une autre affaire.
[[nodiscard]] std::vector<Note> notesOfMode( Note p_tonic, Mode p_mode );

// Les notes d'un mode, du premier degre au septieme, PUIS LA TONIQUE A L'OCTAVE : la gamme qui se REFERME.
//
// Un mode qui s'arrete sur son septieme degre reste SUSPENDU. Le septieme degre n'existe que pour demander la tonique -
// c'est sa fonction, et c'est pour cela qu'on le nomme le degre conducteur. La phrase reclame donc sa fin, et la lui
// refuser laisse une question en l'air.
//
// C'est exactement la regle de modeScaleUpAndDown, ecrite juste en dessous, et elle vaut donc aussi pour une gamme qui
// ne monte pas. Roger l'a demande : « est-ce que ce n'est pas mieux de boucler en entier et de revenir sur le 1er ? »
//
// Et ce n'est pas qu'une affaire de gout : la note qui CLOT la phrase est la TONIQUE, donc c'est en fermant la gamme
// qu'un mode dit ou est son centre. Le bourdon le donne, la derniere note le NOMME.
[[nodiscard]] std::vector<Note> notesOfModeClosingOnTonic( Note p_tonic, Mode p_mode );

// L'identifiant anglais d'un mode ("lydian", "dorian"...).
//
// Il sert aux fichiers de contenu et aux journaux : une phrase modale doit pouvoir dire de quel mode elle est, et le
// dire dans la langue du code. Le nom AFFICHE, lui, se traduit dans l'interface, comme le reste.
[[nodiscard]] std::string_view modeIdentifier( Mode p_mode ) noexcept;

// La gamme d'un mode, MONTEE puis DESCENDUE : les degres dans l'ordre, puis les memes en revenant, sans repeter le
// sommet.
//
// Une couleur s'entend mieux quand elle va et vient, et c'est la DESCENTE qui fait entendre ou se trouve le centre :
// c'est en revenant sur la tonique qu'un mode se dit, et une gamme qui monte seulement laisse la phrase en l'air. Le
// banc d'essai des modes et l'apercu d'un instrument font donc le MEME geste, et il est ecrit ici une seule fois -
// deux versions du meme aller-retour finiraient par designer deux musiques differentes.
[[nodiscard]] std::vector<Note> modeScaleUpAndDown( Note p_tonic, Mode p_mode );

// L'ACCORD qui dit le PHRYGIEN : le majeur pose une seconde MINEURE au-dessus de la tonique, son deuxieme degre.
//
// C'est la signature du mode, et elle n'appartient qu'a lui : le phrygien est le seul mode de la gamme majeure dont le
// deuxieme degre soit a un demi-ton du centre. Une gamme seule se confond avec d'autres - six des sept modes
// partagent la meme fenetre de notes - mais ce majeur-la dit le mode a lui seul, et c'est de la que vient la couleur
// andalouse. C'est donc la chose la plus courte a faire entendre quand on veut reconnaitre un phrygien.
[[nodiscard]] std::vector<Note> phrygianSignatureChord( Note p_tonic );

// L'ecart, en demi-tons, entre la tonique d'un mode et celle d'un autre, quand les deux partagent la MEME gamme.
//
// C'est la difference de leurs degres dans la gamme majeure : le dorien en est le deuxieme, le mixolydien le cinquieme,
// donc poser la meme gamme sur l'un puis sur l'autre deplace la tonique de deux demi-tons, ou de sept. Rien de plus.
//
// C'est cette fonction qui rend un VAMP possible, et c'est aussi la raison pour laquelle un mode ne se transpose PAS
// comme un intervalle : la meme gamme sur un autre degre est un autre mode, a une autre hauteur.
[[nodiscard]] constexpr std::int32_t modeTonicShift( Mode p_from, Mode p_to ) noexcept
{
    const auto degreeOf = []( Mode p_mode ) {
        return MAJOR_SCALE_DEGREE_OFFSETS.at(
          static_cast<std::size_t>( MODE_ROTATION_INDEX.at( modeIndex( p_mode ) ) ) );
    };

    return degreeOf( p_to ) - degreeOf( p_from );
}

// La difference entre deux modes : le degre qui bouge, et de combien.
//
// C'est la propriete du §2.2 de la note 27, et elle vaut mieux qu'un tableau : deux modes VOISINS dans l'ordre de
// couleur ne different que d'UNE SEULE NOTE. C'est ce qui rend le degrade audible en un geste, et c'est aussi ce qu'un
// verdict peut dire - « de dorien a ionien, la tierce a monte » - au lieu d'un nom qu'on lit trop vite pour apprendre.
struct ModeDifference
{
    // De 1 a 7. ZERO quand les deux modes sont identiques : il n'y a alors aucune note a montrer.
    std::int32_t degree{ 0 };

    // L'ecart, en demi-tons, de la note du SECOND mode au-dessus de celle du premier. Un demi-ton sous la note
    // naturelle se lit « bemol », un demi-ton au-dessus se lit « diese ».
    std::int32_t semitones{ 0 };
};

// Le premier degre ou deux modes different, dans l'ordre des degres.
[[nodiscard]] constexpr ModeDifference modeDifference( Mode p_from, Mode p_to ) noexcept
{
    const std::array<std::int32_t, DEGREE_COUNT> fromOffsets = modeDegreeOffsets( p_from );
    const std::array<std::int32_t, DEGREE_COUNT> toOffsets = modeDegreeOffsets( p_to );

    for( std::size_t index = 0; index < DEGREE_COUNT; ++index )
    {
        if( fromOffsets.at( index ) != toOffsets.at( index ) )
        {
            return ModeDifference{ .degree = static_cast<std::int32_t>( index ) + 1,
                                   .semitones = toOffsets.at( index ) - fromOffsets.at( index ) };
        }
    }

    return ModeDifference{};
}

// Une case du cercle des quintes, vue depuis une tonique.
struct CircleNote
{
    // La classe de hauteur : 0 pour do, 1 pour do diese, ... 11 pour si. C'est la seule chose que le domaine ait a dire
    // de la note : la nommer en francais est une affaire d'affichage.
    std::int32_t pitchClassIndex{ 0 };

    // Vrai pour les SEPT notes du mode : elles allument leur case.
    bool belongsToMode{ false };

    // Vrai pour la tonique, qui est aussi la case de depart du cercle.
    bool isTonic{ false };
};

// Les DOUZE cases du cercle des quintes, dans l'ordre des QUINTES, en partant de la tonique.
//
// ---------------------------------------------------------------------------------------------------------------------
// Pourquoi cet ordre, et pourquoi c'est la bonne facon de voir un mode
//
// Sur un cercle des quintes, les sept notes d'un mode sont VOISINES : c'est exactement ce qu'est une armure - sept notes
// qui se suivent de quinte en quinte. L'arc des sept cases allumees est donc toujours CONTIGU, et ce qui change d'un
// mode a l'autre est la place de la TONIQUE dans cet arc.
//
// C'est ce qui rend le dessin utile : do ionien met la tonique au deuxieme rang de son armure, re dorien au quatrieme,
// et ainsi de suite. Le mode cesse d'etre une liste de notes pour devenir une POSITION, ce qui est justement la facon
// dont une oreille le reconnait.
//
// L'index de la case EST son rang de quinte depuis la tonique : la case 0 est la tonique, la case 1 sa quinte, et la
// case 11 sa quarte (la quinte en dessous).
[[nodiscard]] std::array<CircleNote, SEMITONES_PER_OCTAVE> modeCircleNotes( Mode p_mode,
                                                                            std::int32_t p_tonicPitchClass ) noexcept;

// L'ordre d'apprentissage des modes : celui dans lequel ils entrent dans la palette d'un joueur.
//
// C'est une decision MUSICALE, ecrite ici pour pouvoir etre discutee, et elle suit le meme principe que l'ordre des
// intervalles : ON PART DE CE QUI EST DEJA CONNU, puis on s'eloigne.
//
//   1. l'ionien et l'eolien d'abord, et ENSEMBLE : ce sont le majeur et le mineur, c'est-a-dire ce que toute oreille
//      connait deja sans le savoir. Le tout premier ecart entendu est donc le plus grand de tous - et c'est voulu :
//      commencer par deux modes qui se ressemblent ne s'entendrait pas ;
//   2. le mixolydien et le dorien ensuite : un seul degre eteint depuis le majeur et depuis le mineur, donc les deux
//      pas les plus doux a entendre depuis ce qu'on vient d'apprendre ;
//   3. le lydien, plus clair que le majeur, puis le phrygien, plus sombre que le mineur : les extremes se decouvrent ;
//   4. le locrien en dernier : rien n'y repose, et c'est le mode dont la quinte elle-meme est abimee.
//
// Les sept modes y figurent EXACTEMENT UNE FOIS : c'est l'invariant qu'un test verifie, parce qu'une liste ecrite a la
// main est une liste qui derive, et qu'un mode absent serait un mode qu'on ne pourrait jamais apprendre.
[[nodiscard]] std::span<const Mode> modeLearningOrder();

// La palette d'un joueur qui connait les p_modeCount premiers modes de l'ordre.
//
// Le resultat contient toujours au moins un mode, et au moins DEUX des qu'il en demande deux : une question de couleur
// a besoin de deux modes a comparer, et une palette d'un seul mode ne pourrait pas la poser.
[[nodiscard]] std::vector<Mode> beginnerModePalette( std::size_t p_modeCount );

// Vrai quand p_first est PLUS CLAIR que p_second, dans l'ordre de couleur.
//
// L'ordre de l'enumeration EST l'ordre de couleur (voir Mode.h), donc la comparaison est un simple rang - et c'est ce
// qui garantit qu'un exercice qui demande « plus clair ou plus sombre ? » ne puisse jamais se tromper de sens.
[[nodiscard]] constexpr bool isBrighterThan( Mode p_first, Mode p_second ) noexcept
{
    return modeIndex( p_first ) < modeIndex( p_second );
}

// Le mode qu'un identifiant designe, ou rien s'il n'en designe aucun.
//
// Lit un fichier de contenu, donc un fichier qu'un humain peut ecrire : un identifiant inconnu doit couter la phrase
// concernee, jamais l'application.
[[nodiscard]] std::optional<Mode> modeFromIdentifier( std::string_view p_identifier ) noexcept;

}    // namespace musichien::domain
