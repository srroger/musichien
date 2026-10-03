#include "domain/music/Mode.h"

#include "domain/music/Chord.h"
#include "domain/music/Scale.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <set>
#include <span>
#include <vector>

namespace musichien::domain
{

namespace
{

// Les classes de hauteur d'une suite de notes : c'est ce qui permet de comparer deux gammes SANS se soucier de
// l'octave, donc de dire « ce sont bien les memes notes ». Un mode se reconnait a ses classes, pas a ses hauteurs.
[[nodiscard]] std::set<std::int32_t> pitchClassesOf( std::span<const Note> p_notes )
{
    std::set<std::int32_t> classes;

    for( const Note & note : p_notes )
    {
        classes.insert( note.pitchClassIndex() );
    }

    return classes;
}

}    // namespace

// Une gamme de mode SE REFERME : les sept degres, puis la tonique a l'octave.
TEST( ModeTest, a_scale_closes_on_its_tonic )
{
    // Roger : « est-ce que ce n'est pas mieux de boucler en entier et de revenir sur le 1er ? » Oui - et le domaine le
    // disait deja pour la gamme montee-descendue : « une gamme qui monte seulement laisse la phrase en l'air ». Cette
    // fonction applique la meme regle a la gamme qui ne monte pas.
    const std::vector<Note> scale = notesOfModeClosingOnTonic( Note{ 60 }, Mode::Dorian );

    ASSERT_EQ( DEGREE_COUNT + 1, scale.size() );

    // LE PREMIER ET LE DERNIER : la tonique, une octave plus haut. C'est cet ecart qui REFERME la phrase.
    EXPECT_EQ( 60, scale.front().midiNumber() );
    EXPECT_EQ( 72, scale.back().midiNumber() );

    // Et les sept premiers degres sont EXACTEMENT ceux de la gamme : la note ajoutee ne remplace rien. Un mode qui
    // perdrait un degre en se refermant serait un autre mode.
    const std::vector<Note> open = notesOfMode( Note{ 60 }, Mode::Dorian );

    EXPECT_TRUE( std::equal( open.begin(), open.end(), scale.begin() ) );
}

TEST( ModeTest, every_mode_has_seven_rising_degrees_inside_one_octave )
{
    for( std::size_t index = 0; index < MODE_COUNT; ++index )
    {
        const auto mode = static_cast<Mode>( index );

        const std::array<std::int32_t, DEGREE_COUNT> offsets = modeDegreeOffsets( mode );

        EXPECT_EQ( 0, offsets.front() ) << modeIdentifier( mode );

        for( std::size_t degree = 1; degree < DEGREE_COUNT; ++degree )
        {
            EXPECT_LT( offsets.at( degree - 1 ), offsets.at( degree ) ) << modeIdentifier( mode );
            EXPECT_LT( offsets.at( degree ), SEMITONES_PER_OCTAVE ) << modeIdentifier( mode );
        }
    }
}

TEST( ModeTest, the_ionian_is_the_major_scale_and_the_aeolian_the_natural_minor_one )
{
    EXPECT_EQ( MAJOR_SCALE_DEGREE_OFFSETS, modeDegreeOffsets( Mode::Ionian ) );

    const std::array<std::int32_t, DEGREE_COUNT> aeolian{ 0, 2, 3, 5, 7, 8, 10 };

    EXPECT_EQ( aeolian, modeDegreeOffsets( Mode::Aeolian ) );
}

TEST( ModeTest, a_mode_is_the_major_scale_seen_from_another_of_its_degrees )
{
    // Le point du §3.2 de la note 27 : les sept modes de la gamme de do sont la MEME gamme, vue depuis chacune de ses
    // sept notes. Les sept toniques sont donc les sept notes de do majeur, et chacune doit rendre le meme jeu de
    // classes de hauteur - sinon un mode ne serait plus une rotation de la gamme majeure.
    const std::array<std::int32_t, MODE_COUNT> tonicMidiNumbers{ 65, 60, 67, 62, 69, 64, 71 };    // F C G D A E B

    const std::set<std::int32_t> reference = pitchClassesOf( notesOfMode( Note{ 60 }, Mode::Ionian ) );

    for( std::size_t index = 0; index < MODE_COUNT; ++index )
    {
        const auto mode = static_cast<Mode>( index );

        const std::vector<Note> notes = notesOfMode( Note{ tonicMidiNumbers.at( index ) }, mode );

        EXPECT_EQ( reference, pitchClassesOf( notes ) ) << modeIdentifier( mode );
    }
}

TEST( ModeTest, d_dorian_holds_exactly_the_notes_of_c_major )
{
    // Le piege du §4 de la note 27, verifie ici comme une EGALITE : do dorien, si bemol majeur et sol mineur
    // partagent les memes notes. Ce qui les separe n'est pas dans ce test - c'est le CENTRE, donc le bourdon, et cela
    // ne se demontre pas avec des ensembles de notes.
    const std::set<std::int32_t> dorianOfD = pitchClassesOf( notesOfMode( Note{ 62 }, Mode::Dorian ) );

    EXPECT_EQ( pitchClassesOf( notesOfMode( Note{ 60 }, Mode::Ionian ) ), dorianOfD );

    // Et la meme chose vue depuis do : do dorien ne contient PAS les notes de do majeur - il en perd une et en gagne
    // une autre. C'est un mode different, pas une autre facon d'ecrire le meme.
    EXPECT_NE( pitchClassesOf( notesOfMode( Note{ 60 }, Mode::Dorian ) ),
               pitchClassesOf( notesOfMode( Note{ 60 }, Mode::Ionian ) ) );
}

TEST( ModeTest, two_neighbouring_modes_in_the_colour_order_differ_by_a_single_note )
{
    // C'est la propriete qui justifie l'ordre de l'enumeration, donc celle de tout l'exercice du degrade : descendre la
    // liste eteint une note et en rallume une autre, et l'ecart s'entend en UN SEUL geste. Si cette propriete tombe,
    // l'ordre de couleur n'est plus qu'une preference.
    for( std::size_t index = 0; index + 1 < MODE_COUNT; ++index )
    {
        const std::set<std::int32_t> lower = pitchClassesOf( notesOfMode( Note{ 60 }, static_cast<Mode>( index ) ) );

        const std::set<std::int32_t> higher = pitchClassesOf( notesOfMode( Note{ 60 }, static_cast<Mode>( index + 1 ) ) );

        std::vector<std::int32_t> onlyInLower;
        std::vector<std::int32_t> onlyInHigher;

        std::ranges::set_difference( lower, higher, std::back_inserter( onlyInLower ) );
        std::ranges::set_difference( higher, lower, std::back_inserter( onlyInHigher ) );

        EXPECT_EQ( 1, onlyInLower.size() ) << modeIdentifier( static_cast<Mode>( index ) );
        EXPECT_EQ( 1, onlyInHigher.size() ) << modeIdentifier( static_cast<Mode>( index ) );
    }
}

TEST( ModeTest, an_identifier_round_trips_and_an_unknown_one_designs_nothing )
{
    for( std::size_t index = 0; index < MODE_COUNT; ++index )
    {
        const auto mode = static_cast<Mode>( index );

        const std::optional<Mode> read = modeFromIdentifier( modeIdentifier( mode ) );

        ASSERT_TRUE( read.has_value() ) << modeIdentifier( mode );
        EXPECT_EQ( mode, read.value() );
    }

    // Un fichier de contenu est un fichier qu'un humain peut ecrire : un identifiant inconnu doit couter la phrase
    // concernee, jamais l'application.
    EXPECT_FALSE( modeFromIdentifier( "doriann" ).has_value() );
    EXPECT_FALSE( modeFromIdentifier( "" ).has_value() );
}

TEST( ModeTest, the_characteristic_note_is_where_a_mode_is_told_apart )
{
    // Les notes qui colorent, verifiees sur les degres : la quarte augmentee du lydien, la septieme mineure du
    // mixolydien, la sixte majeure du dorien, la seconde mineure du phrygien, la quinte diminuee du locrien.
    EXPECT_EQ( 6, degreeOffset( Mode::Lydian, 4 ) );
    EXPECT_EQ( 10, degreeOffset( Mode::Mixolydian, 7 ) );
    EXPECT_EQ( 9, degreeOffset( Mode::Dorian, 6 ) );
    EXPECT_EQ( 1, degreeOffset( Mode::Phrygian, 2 ) );
    EXPECT_EQ( 6, degreeOffset( Mode::Locrian, 5 ) );

    // Et les deux modes de reference n'ont rien a demontrer : c'est justement pour cela qu'ils servent de reference.
    EXPECT_EQ( 4, degreeOffset( Mode::Ionian, 3 ) );
    EXPECT_EQ( 3, degreeOffset( Mode::Aeolian, 3 ) );
}

TEST( ModeTest, moving_the_mode_towards_the_bright_and_the_tonic_down_keeps_the_same_notes )
{
    // La regle sur laquelle un VAMP est bati : monter d'un cran vers le clair, c'est poser la MEME gamme un demi-ton
    // plus bas. Do dorien et si bemol majeur ont exactement les memes sept notes.
    EXPECT_EQ( pitchClassesOf( notesOfMode( Note{ 60 }, Mode::Dorian ) ),
               pitchClassesOf( notesOfMode( Note{ 58 }, Mode::Ionian ) ) );

    // Et l'autre sens, evidemment : descendre vers le sombre, c'est monter la tonique.
    EXPECT_EQ( pitchClassesOf( notesOfMode( Note{ 58 }, Mode::Ionian ) ),
               pitchClassesOf( notesOfMode( Note{ 60 }, Mode::Dorian ) ) );

    // Un pas de DEUX, et pas seulement d'un demi-ton : re mixolydien et do lydien ont les memes notes.
    EXPECT_EQ( pitchClassesOf( notesOfMode( Note{ 62 }, Mode::Mixolydian ) ),
               pitchClassesOf( notesOfMode( Note{ 60 }, Mode::Lydian ) ) );
}

TEST( ModeTest, two_neighbouring_modes_differ_by_exactly_one_note )
{
    // La propriete du §2.2 de la note 27, verifiee ici parce que le domaine s'en sert maintenant pour DIRE un verdict :
    // « la tierce a monte d'un demi-ton » n'a de sens que si une seule note a bouge.
    for( std::size_t index = 0; index + 1 < MODE_COUNT; ++index )
    {
        const ModeDifference difference = modeDifference( static_cast<Mode>( index ), static_cast<Mode>( index + 1 ) );

        EXPECT_NE( 0, difference.degree ) << modeIdentifier( static_cast<Mode>( index ) );
        EXPECT_EQ( 1, std::abs( difference.semitones ) ) << modeIdentifier( static_cast<Mode>( index ) );
    }

    // Et deux fois le meme mode ne different de rien du tout.
    EXPECT_EQ( 0, modeDifference( Mode::Dorian, Mode::Dorian ).degree );
}

TEST( ModeTest, the_learning_order_holds_every_mode_exactly_once_and_starts_with_the_known_ones )
{
    const std::span<const Mode> order = modeLearningOrder();

    ASSERT_EQ( MODE_COUNT, order.size() );

    std::set<std::size_t> seen;

    for( const Mode mode : order )
    {
        seen.insert( modeIndex( mode ) );
    }

    // L'invariant : chaque mode y figure EXACTEMENT UNE FOIS. Une liste ecrite a la main derive, et un mode absent
    // serait un mode qu'on ne pourrait jamais apprendre - sans que rien d'autre ne le signale.
    EXPECT_EQ( MODE_COUNT, seen.size() );

    // Le majeur et le mineur d'abord, et ENSEMBLE : c'est le seul ecart que l'oreille connait deja.
    EXPECT_EQ( Mode::Ionian, order.front() );
    EXPECT_EQ( Mode::Aeolian, order.at( 1 ) );

    // Et le locrien en dernier : c'est le seul mode ou rien ne repose.
    EXPECT_EQ( Mode::Locrian, order.back() );
}

TEST( ModeTest, a_beginner_palette_never_holds_fewer_than_two_modes )
{
    // Une question de couleur compare DEUX modes : une palette d'un seul ne pourrait pas la poser. Le domaine refuse
    // donc cet etat tout de suite, au lieu de laisser la session le decouvrir en tirant une question.
    EXPECT_EQ( 2, beginnerModePalette( 0 ).size() );
    EXPECT_EQ( 2, beginnerModePalette( 1 ).size() );
    EXPECT_EQ( 3, beginnerModePalette( 3 ).size() );

    // Et demander plus que l'ordre n'en contient rend l'ordre entier, sans jamais lever.
    EXPECT_EQ( MODE_COUNT, beginnerModePalette( 99 ).size() );
}

TEST( ModeTest, brightness_follows_the_colour_order )
{
    // Le lydien est le plus clair, le locrien le plus sombre, et la comparaison n'est qu'un rang : c'est ce qui
    // garantit qu'un exercice demandant « plus clair ou plus sombre ? » ne peut pas se tromper de sens.
    EXPECT_TRUE( isBrighterThan( Mode::Lydian, Mode::Locrian ) );
    EXPECT_TRUE( isBrighterThan( Mode::Ionian, Mode::Aeolian ) );
    EXPECT_FALSE( isBrighterThan( Mode::Aeolian, Mode::Ionian ) );

    // Un mode n'est pas plus clair que lui-meme : la comparaison est STRICTE.
    EXPECT_FALSE( isBrighterThan( Mode::Dorian, Mode::Dorian ) );
}

TEST( ModeTest, the_circle_of_fifths_holds_the_seven_notes_of_a_mode_in_one_arc )
{
    // La propriete qui rend le dessin utile : sur le cercle des quintes, les sept notes d'un mode sont VOISINES. L'arc
    // des cases allumees est donc toujours CONTIGU - c'est exactement ce qu'est une armure - et il ne reste plus qu'a
    // lire ou se trouve la tonique dedans.
    constexpr std::array<std::int32_t, 6> TONICS{ 0, 2, 4, 5, 7, 9 };

    for( std::size_t modeIndex = 0; modeIndex < MODE_COUNT; ++modeIndex )
    {
        for( const std::int32_t tonic : TONICS )
        {
            const std::array<CircleNote, SEMITONES_PER_OCTAVE> circle =
              modeCircleNotes( static_cast<Mode>( modeIndex ), tonic );

            // La tonique ouvre le cercle, et elle en fait partie : un mode sans sa tonique ne serait pas un mode.
            EXPECT_TRUE( circle.front().isTonic );
            EXPECT_TRUE( circle.front().belongsToMode );
            EXPECT_EQ( ( tonic % SEMITONES_PER_OCTAVE ), circle.front().pitchClassIndex );

            std::size_t litCount = 0;
            std::size_t arcStart = 0;
            bool sawArcStart = false;

            for( std::size_t index = 0; index < circle.size(); ++index )
            {
                if( circle.at( index ).belongsToMode )
                {
                    ++litCount;
                }

                // Le debut de l'arc est la case allumee dont la PRECEDENTE est eteinte, en tournant : un arc peut
                // enjamber la case 0 - c'est le cas des que la tonique n'est pas la premiere note de son armure - et
                // une recherche qui ne tourne pas prendrait la tonique pour le debut de l'arc.
                const std::size_t previous = ( index + circle.size() - 1 ) % circle.size();

                if( !sawArcStart && circle.at( index ).belongsToMode && !circle.at( previous ).belongsToMode )
                {
                    arcStart = index;
                    sawArcStart = true;
                }
            }

            EXPECT_EQ( 7U, litCount );
            ASSERT_TRUE( sawArcStart );

            // SEPT cases allumees d'affilee depuis le debut de l'arc, en tournant : c'est la definition d'une armure.
            for( std::size_t step = 0; step < 7; ++step )
            {
                const std::size_t position = ( arcStart + step ) % circle.size();

                EXPECT_TRUE( circle.at( position ).belongsToMode )
                  << "l'arc se casse apres " << step << " cases";
            }

            // Et la case qui SUIT l'arc est eteinte : sans quoi l'arc ne serait pas un arc, mais le cercle entier.
            EXPECT_FALSE( circle.at( ( arcStart + 7 ) % circle.size() ).belongsToMode );
        }
    }
}

TEST( ModeTest, the_rank_of_the_tonic_in_its_own_arc_is_what_tells_the_modes_apart )
{
    // La lecture qu'un joueur fera du dessin, et la seule qui compte.
    //
    // Do ionien et re dorien ont exactement les MEMES sept notes : meme armure, meme arc. Ce qui change est la place de
    // la tonique DANS cet arc - deuxieme rang pour l'un, quatrieme pour l'autre. Le mode cesse alors d'etre une liste de
    // notes pour devenir une POSITION, ce qui est la facon dont une oreille le reconnait.
    const auto rankOfTheTonicInItsArc = []( const std::array<CircleNote, SEMITONES_PER_OCTAVE> & p_circle ) {
        // On remonte depuis la derniere case : chacune qui appartient au mode est une note de l'arc qui precede la
        // tonique. La premiere qui n'y appartient pas marque le debut de l'arc.
        std::size_t rank = 1;

        for( std::size_t index = p_circle.size() - 1; p_circle.at( index ).belongsToMode; --index )
        {
            ++rank;

            if( index == 0 )
            {
                break;
            }
        }

        return rank;
    };

    EXPECT_EQ( 2U, rankOfTheTonicInItsArc( modeCircleNotes( Mode::Ionian, 0 ) ) );
    EXPECT_EQ( 3U, rankOfTheTonicInItsArc( modeCircleNotes( Mode::Mixolydian, 7 ) ) );
    EXPECT_EQ( 4U, rankOfTheTonicInItsArc( modeCircleNotes( Mode::Dorian, 2 ) ) );
    EXPECT_EQ( 5U, rankOfTheTonicInItsArc( modeCircleNotes( Mode::Aeolian, 9 ) ) );
    EXPECT_EQ( 6U, rankOfTheTonicInItsArc( modeCircleNotes( Mode::Phrygian, 4 ) ) );

    // Et l'ordre de ces rangs EST l'ordre de couleur des modes : c'est ce qui fait du dessin une image de l'axe qu'on
    // demande a l'oreille de suivre.
    EXPECT_TRUE( rankOfTheTonicInItsArc( modeCircleNotes( Mode::Lydian, 5 ) )
                 < rankOfTheTonicInItsArc( modeCircleNotes( Mode::Ionian, 0 ) ) );
}

TEST( ModeTest, a_scale_goes_up_and_comes_back_down_to_its_tonic )
{
    // Le geste commun au banc d'essai des modes et a l'apercu d'un instrument : une couleur va et vient, et c'est la
    // DESCENTE qui dit ou se trouve le centre - un mode se dit en revenant sur sa tonique.
    const std::vector<Note> played = modeScaleUpAndDown( Note{ 62 }, Mode::Phrygian );

    // Les sept degres montes, puis les six qui redescendent : le sommet n'est joue QU'UNE fois, sans quoi le demi-tour
    // serait une hesitation.
    ASSERT_EQ( 13U, played.size() );

    EXPECT_EQ( 62, played.front().midiNumber() );
    EXPECT_EQ( 62, played.back().midiNumber() );

    for( std::size_t index = 1; index < 7; ++index )
    {
        EXPECT_LT( played.at( index - 1 ).midiNumber(), played.at( index ).midiNumber() );
    }

    for( std::size_t index = 8; index < played.size(); ++index )
    {
        EXPECT_GT( played.at( index - 1 ).midiNumber(), played.at( index ).midiNumber() );
    }

    // Et le phrygien est bien LA : sa seconde est a un demi-ton de la tonique, jamais un ton.
    EXPECT_EQ( 63, played.at( 1 ).midiNumber() );
}

TEST( ModeTest, the_phrygian_signature_chord_is_a_major_a_semitone_above_the_tonic )
{
    // Ce qui dit le phrygien quand la gamme seule ne le dit plus : le majeur pose sur son deuxieme degre. Six des sept
    // modes partagent la meme fenetre de notes, mais ce majeur-la n'appartient qu'au phrygien - c'est la couleur
    // andalouse, et c'est la chose la plus courte a faire entendre pour le reconnaitre.
    const std::vector<Note> chord = phrygianSignatureChord( Note{ 62 } );

    ASSERT_EQ( 3U, chord.size() );

    EXPECT_EQ( 63, chord.at( 0 ).midiNumber() );

    // Et ce sont les intervalles d'un MAJEUR, demandes a la regle de l'accord : une tierce majeure, puis une quinte
    // juste. Rien n'est recompte ici, ce qui est exactement le point - la couleur d'un accord vit a un seul endroit.
    for( std::size_t index = 0; index < chord.size(); ++index )
    {
        EXPECT_EQ( 63 + chordIntervals( ChordQuality::Major ).at( index ), chord.at( index ).midiNumber() );
    }
}

TEST( ScaleTest, the_six_scales_have_the_notes_they_are_supposed_to )
{
    // UNE GAMME FAUSSE S'ENTEND MAIS NE SE VOIT PAS. C'est pour cela que ce test existe : personne, en lisant un tableau
    // de degres, ne remarque qu'une septieme est majeure la ou elle devrait etre mineure. Le tableau est donc verifie
    // contre ce qui definit chaque gamme, une propriete a la fois.

    // LA PENTATONIQUE : cinq notes, et AUCUN demi-ton. C'est ce qui la rend facile - aucune note voisine a rater - et
    // c'est pour cela qu'elle sert des la premiere jam. Si un demi-ton y apparaissait, ce ne serait plus elle.
    const ScaleDegrees pentatonic = scaleDegreeOffsets( Scale::PentatonicMinor );

    EXPECT_EQ( 5U, pentatonic.count );

    for( std::size_t index = 0; index < pentatonic.count; ++index )
    {
        const std::int32_t gap = ( pentatonic.offsets.at( ( index + 1 ) % pentatonic.count )
                                   - pentatonic.offsets.at( index ) + SEMITONES_PER_OCTAVE )
                                 % SEMITONES_PER_OCTAVE;

        EXPECT_GE( gap, 2 ) << "un demi-ton au degre " << index;
    }

    // LE BLUES, C'EST LA PENTATONIQUE MINEURE PLUS UNE NOTE : la quinte diminuee, la « note bleue ». Meme depart, une
    // note de plus, et c'est toute la couleur du genre.
    const ScaleDegrees blues = scaleDegreeOffsets( Scale::Blues );

    EXPECT_EQ( 6U, blues.count );

    for( std::size_t index = 0; index < pentatonic.count; ++index )
    {
        const bool isInBlues = std::ranges::find( blues.offsets.begin(),
                                                  blues.offsets.begin() + static_cast<std::ptrdiff_t>( blues.count ),
                                                  pentatonic.offsets.at( index ) )
                               != blues.offsets.begin() + static_cast<std::ptrdiff_t>( blues.count );

        EXPECT_TRUE( isInBlues ) << "le blues a perdu une note de la pentatonique : " << pentatonic.offsets.at( index );
    }

    EXPECT_EQ( 1, std::ranges::count( blues.offsets.begin(), blues.offsets.begin() + 6, 6 ) ) << "la note bleue a disparu";

    // LE MINEUR HARMONIQUE : le majeur, avec une SIXTE MINEURE et une SEPTIEME MAJEURE. C'est le demi-ton entre les deux
    // qui fait tout son caractere - le majeur, lui, a un ton entier a cet endroit.
    const ScaleDegrees harmonic = scaleDegreeOffsets( Scale::HarmonicMinor );

    ASSERT_EQ( 7U, harmonic.count );

    EXPECT_EQ( 3, harmonic.offsets.at( 2 ) ) << "la tierce doit etre mineure";
    EXPECT_EQ( 8, harmonic.offsets.at( 5 ) ) << "la sixte doit etre mineure";
    EXPECT_EQ( 11, harmonic.offsets.at( 6 ) ) << "la septieme doit etre majeure";

    // LE PHRYGIEN DOMINANT est le CINQUIEME mode du mineur harmonique. Le test le verifie comme une propriete, et non en
    // recopiant un tableau : on remonte la gamme depuis son cinquieme degre, on ramene tout a la tonique, et on doit
    // retrouver exactement ses degres. C'est la seule facon de garantir que les deux gammes disent la meme chose.
    std::array<std::int32_t, MAX_SCALE_DEGREE_COUNT> rotation{};

    for( std::size_t degree = 0; degree < harmonic.count; ++degree )
    {
        const std::int32_t fromFifth = ( harmonic.offsets.at( ( degree + 4 ) % harmonic.count ) - harmonic.offsets.at( 4 )
                                         + SEMITONES_PER_OCTAVE )
                                       % SEMITONES_PER_OCTAVE;

        rotation.at( degree ) = fromFifth;
    }

    std::ranges::sort( rotation.begin(), rotation.begin() + 7 );

    const ScaleDegrees phrygian = scaleDegreeOffsets( Scale::PhrygianDominant );

    ASSERT_EQ( 7U, phrygian.count );

    for( std::size_t degree = 0; degree < phrygian.count; ++degree )
    {
        EXPECT_EQ( phrygian.offsets.at( degree ), rotation.at( degree ) );
    }

    // ET LE MINEUR MELODIQUE, MONTE : un mineur naturel avec une SIXTE MAJEURE (9), septieme majeure (11).
    const ScaleDegrees melodic = scaleDegreeOffsets( Scale::MelodicMinor );

    ASSERT_EQ( 7U, melodic.count );

    EXPECT_EQ( 3, melodic.offsets.at( 2 ) ) << "la tierce doit etre mineure";
    EXPECT_EQ( 9, melodic.offsets.at( 5 ) ) << "la sixte doit etre MAJEURE - c'est tout ce qui le separe du naturel";
    EXPECT_EQ( 11, melodic.offsets.at( 6 ) );
}

TEST( ScaleTest, every_scale_starts_on_its_tonic_and_never_above_the_octave )
{
    // DEUX EVIDENCES QU'UN TABLEAU ECRIT A LA MAIN CASSE TOUJOURS UN JOUR : le premier degre est la tonique, et aucun
    // degre ne depasse l'octave. La seconde se voit mal a l'oeil - un 12 au lieu d'un 0 se lit comme une note de plus,
    // alors que c'est la meme, et le cercle aurait alors deux fois la tonique.
    for( std::size_t index = 0; index < SCALE_COUNT; ++index )
    {
        const Scale scale = scaleFromIndex( index );
        const ScaleDegrees degrees = scaleDegreeOffsets( scale );

        EXPECT_EQ( 0, degrees.offsets.at( 0 ) ) << scaleIdentifier( scale );
        EXPECT_GE( degrees.count, 5U ) << scaleIdentifier( scale );
        EXPECT_LE( degrees.count, MAX_SCALE_DEGREE_COUNT ) << scaleIdentifier( scale );

        for( std::size_t degree = 1; degree < degrees.count; ++degree )
        {
            const std::int32_t offset = degrees.offsets.at( degree );

            EXPECT_GT( offset, degrees.offsets.at( degree - 1 ) ) << scaleIdentifier( scale ) << " degre " << degree;
            EXPECT_LT( offset, SEMITONES_PER_OCTAVE ) << scaleIdentifier( scale ) << " degre " << degree;
        }
    }

    // Et un rang absurde rend la premiere gamme plutot qu'une valeur inventee : un reglage abime ne doit jamais couter
    // plus cher qu'un reglage.
    EXPECT_EQ( Scale::PentatonicMinor, scaleFromIndex( SCALE_COUNT ) );
    EXPECT_EQ( Scale::PentatonicMinor, scaleFromIndex( 99 ) );
}

}    // namespace musichien::domain
