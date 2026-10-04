#include "ui/ModeDescription.h"

#include <QCoreApplication>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace musichien::ui
{

namespace
{

// Les noms francais, dans l'ORDRE DE L'ENUMERATION, avec la phrase qui dit ce qui colore chaque mode.
//
// Une seule table, lue dans un seul sens : un nom et sa caracteristique ne peuvent pas se desynchroniser. L'ordre est
// celui de la clarte, donc celui de Mode - c'est le meme ordre que les boutons du banc d'essai.
//
// LES DEGRES SONT ECRITS EN MOTS, et c'est une correction, pas un gout. La notation « 5 chapeau » (un chiffre suivi d'un
// accent circonflexe COMBINANT, U+0302) s'affiche correctement dans un editeur de texte et se casse a l'ecran : la
// police de l'application rend l'accent comme un caractere separe, et Roger a lu « 5` diminuée » sur son telephone. Un
// mot ne peut pas se casser.
constexpr std::array<const char *, domain::MODE_COUNT> MODE_DISPLAY_NAMES{
  "Lydien", "Ionien", "Mixolydien", "Dorien", "Éolien", "Phrygien", "Locrien" };

constexpr std::array<const char *, domain::MODE_COUNT> MODE_CHARACTERISTICS{
  "la quarte augmentée",
  "la gamme majeure : la référence",
  "la septième mineure, la septième « de dominante »",
  "la sixte majeure, la sixte qui éclaire le mineur",
  "la gamme mineure naturelle : la référence des ombres",
  "la seconde mineure, une tension dès la première marche",
  "la quinte diminuée : rien n'y repose vraiment" };

}    // namespace

QVariantMap describeMode( domain::Mode p_mode )
{
    const auto index = static_cast<std::size_t>( domain::modeIndex( p_mode ) );

    // La clarte : 1 pour le premier de l'ordre de couleur, 0 pour le dernier.
    //
    // CALCULEE et jamais ecrite a la main : l'ordre de l'enumeration EST l'ordre de couleur (voir Mode.h), donc un mode
    // ajoute ou deplace ne peut pas laisser un ecran mentir sur sa clarte.
    const double brightness =
      1.0 - ( static_cast<double>( index ) / static_cast<double>( domain::MODE_COUNT - 1 ) );

    const std::string_view identifier = domain::modeIdentifier( p_mode );

    QVariantMap description;

    description.insert( QStringLiteral( "index" ), static_cast<int>( index ) );
    description.insert( QStringLiteral( "identifier" ),
                        QString::fromUtf8( identifier.data(), static_cast<int>( identifier.size() ) ) );
    description.insert( QStringLiteral( "name" ), QString::fromUtf8( MODE_DISPLAY_NAMES.at( index ) ) );
    description.insert( QStringLiteral( "characteristic" ), QString::fromUtf8( MODE_CHARACTERISTICS.at( index ) ) );
    description.insert( QStringLiteral( "brightness" ), brightness );

    // LES DEGRES DU MODE, ECRITS COMME UN MUSICIEN LES LIT : « 1 2 3 4 5 6 7b ».
    //
    // Roger, sur le boss : « j'ecrirai dans la ligne juste en dessous, les notes qu'il y a dedans en degre [...] ecrit avec
    // les memes couleurs de degrade que les boutons, et le rouge pour la note caracteristique. Histoire d'avoir un repere
    // pour l'utilisateur. »
    //
    // C'est un RAPPEL DU MODE VRAI, jamais de celui que la question joue modifie : le boss fait entendre une note
    // etrangere, et c'est justement ce qu'on veut que le joueur puisse comparer a ce qu'il sait du mode.
    //
    // L'ALTERATION vient de la comparaison avec la gamme MAJEURE - la reference du projet, la meme partout - et la note
    // caracteristique vient du DOMAINE, jamais d'un calcul refait ici. Deux copies de cette regle finiraient par se
    // contredire, et le joueur verrait une note mise en avant que la phrase ne joue pas davantage que les autres.
    QVariantList degrees;

    const std::array<std::int32_t, domain::DEGREE_COUNT> offsets = domain::modeDegreeOffsets( p_mode );
    const std::int32_t characteristic = domain::modeCharacteristicDegree( p_mode );

    for( std::size_t index = 0; index < domain::DEGREE_COUNT; ++index )
    {
        const std::int32_t offset = offsets.at( index );
        const std::int32_t majorOffset = domain::MAJOR_SCALE_DEGREE_OFFSETS.at( index );

        QString label = QString::number( index + 1 );

        if( offset < majorOffset )
        {
            label += QString::fromUtf8( "♭" );
        }
        else if( offset > majorOffset )
        {
            label += QString::fromUtf8( "♯" );
        }

        QVariantMap degree;
        degree.insert( QStringLiteral( "label" ), label );
        degree.insert( QStringLiteral( "index" ), static_cast<int>( index ) );

        // ZERO veut dire « aucune » - l'ionien et l'eolien sont les deux references, et ne se demontrent pas par une note.
        degree.insert( QStringLiteral( "isCharacteristic" ),
                       ( characteristic != 0 ) && ( characteristic == static_cast<std::int32_t>( index ) + 1 ) );

        degrees.append( degree );
    }

    description.insert( QStringLiteral( "degrees" ), degrees );

    return description;
}

namespace
{

// Les noms FRANCAIS des douze classes de hauteur, dans l'ordre des touches : do, puis les cinq alterees, puis les
// naturelles. Un nom est une AFFAIRE D'ECRAN - le domaine ne connait que des numeros - et c'est pour cela que la table
// vit ici, avec les noms de modes, et non dans Mode.h.
//
// Une seule table, lue par le cercle, par la gamme ET par le verdict de la note etrangere : trois ecritures du meme nom
// finiraient par en montrer trois differentes au meme joueur.
constexpr std::array<const char *, domain::SEMITONES_PER_OCTAVE> PITCH_CLASS_NAMES{
  "do", "do♯", "ré", "ré♯", "mi", "fa", "fa♯", "sol", "sol♯", "la", "la♯", "si" };

}    // namespace

QString describeNoteName( std::int32_t p_pitchClassIndex )
{
    const std::int32_t wrapped =
      ( ( p_pitchClassIndex % domain::SEMITONES_PER_OCTAVE ) + domain::SEMITONES_PER_OCTAVE ) % domain::SEMITONES_PER_OCTAVE;

    return QString::fromUtf8( PITCH_CLASS_NAMES.at( static_cast<std::size_t>( wrapped ) ) );
}

QVariantMap describeModeDifference( domain::Mode p_from, domain::Mode p_to )
{
    const domain::ModeDifference difference = domain::modeDifference( p_from, p_to );

    QVariantMap description;

    description.insert( QStringLiteral( "degree" ), difference.degree );
    description.insert( QStringLiteral( "semitones" ), difference.semitones );

    if( difference.degree == 0 )
    {
        // Deux fois le meme mode : il n'y a aucune note a montrer, et l'ecran recoit des chaines vides plutot qu'un cas
        // particulier a gerer.
        description.insert( QStringLiteral( "label" ), QString{} );
        description.insert( QStringLiteral( "sentence" ), QString{} );

        return description;
    }

    // Le NOM du degre, en francais, et dans l'ordre ou un musicien les dit.
    static const std::array<const char *, domain::DEGREE_COUNT> DEGREE_WORDS{ "la tonique", "la seconde", "la tierce", "la quarte", "la quinte", "la sixte", "la septième" };

    const auto degreeIndex = static_cast<std::size_t>( difference.degree - 1 );

    // Le LABEL court, celui que Roger a demande : « 3♭ », « 3♯ ». Il dit le degre ET le sens en deux caracteres, et il
    // se lit d'un coup d'oeil - ce qu'un nom de mode ne fait pas quand il n'est affiche qu'une seconde.
    const QString accidental = ( difference.semitones < 0 ) ? QStringLiteral( "♭" ) : QStringLiteral( "♯" );
    const QString label = accidental + QString::number( difference.degree );

    // Et la phrase, parce qu'un verdict qui n'apprend rien est un verdict perdu.
    //
    // QObject::tr et non tr : cette fonction est LIBRE, et c'est une classe qui porte la traduction dans ce projet.
    const QString sentence = QStringLiteral( "%1 %2 d'un demi-ton" )
                               .arg( QString::fromUtf8( DEGREE_WORDS.at( degreeIndex ) ),
                                     ( difference.semitones < 0 ) ? QObject::tr( "a baissé" ) : QObject::tr( "a monté" ) );

    description.insert( QStringLiteral( "label" ), label );
    description.insert( QStringLiteral( "sentence" ), sentence );

    return description;
}

QVariantMap describePhrase( const domain::Phrase & p_phrase )
{
    // L'ecriture de l'atelier, a la lettre : un chiffre par pas, et la duree seulement quand elle depasse un temps.
    // Ecrire « 1(1) 4(2) » noierait la phrase sous des chiffres qui ne disent rien a personne.
    QString degrees;

    for( const domain::PhraseStep & step : p_phrase.steps )
    {
        if( !degrees.isEmpty() )
        {
            degrees += QLatin1Char( ' ' );
        }

        degrees += QString::number( step.degree );

        if( step.beats > 1 )
        {
            degrees += QStringLiteral( "(%1)" ).arg( step.beats );
        }
    }

    QVariantMap description;

    description.insert( QStringLiteral( "degrees" ), degrees );
    description.insert( QStringLiteral( "bpm" ), p_phrase.bpm );
    description.insert( QStringLiteral( "stepCount" ), static_cast<int>( p_phrase.steps.size() ) );

    return description;
}

// Une QUINTE vaut sept demi-tons - et sept est son propre inverse modulo douze (7 x 7 = 49 = 1 + 4 x 12). Le rang d'une
// note dans une suite de quintes se calcule donc directement depuis son ecart au repere, sans parcourir la suite.
constexpr std::int32_t FIFTHS_STEP = 7;

namespace
{

// LE NOM FRANCAIS DE CHAQUE GAMME, dans l'ordre de l'enumeration du domaine - comme les modes, et pour la meme raison :
// le domaine nomme en anglais et ne bouge plus, l'ecran parle la langue du joueur.
//
// « Blues », « Pentatonique », « Phrygien dominant » : ce sont les mots qu'un musicien emploie, en francais comme en
// anglais, et les inventer autrement serait un vocabulaire que personne n'utilise.
constexpr std::array<const char *, domain::SCALE_COUNT> SCALE_DISPLAY_NAMES{
  QT_TRANSLATE_NOOP( "ModeDescription", "Pentatonique mineure" ),
  QT_TRANSLATE_NOOP( "ModeDescription", "Pentatonique majeure" ),
  QT_TRANSLATE_NOOP( "ModeDescription", "Blues" ),
  QT_TRANSLATE_NOOP( "ModeDescription", "Mineur harmonique" ),
  QT_TRANSLATE_NOOP( "ModeDescription", "Phrygien dominant" ),
  QT_TRANSLATE_NOOP( "ModeDescription", "Mineur mélodique" ),
};

}    // namespace

QString describeScaleName( domain::Scale p_scale )
{
    const auto slot = static_cast<std::size_t>( p_scale );

    if( slot >= SCALE_DISPLAY_NAMES.size() )
    {
        // Une gamme hors table n'existe pas, mais un ecran doit pouvoir afficher quelque chose : l'identifiant anglais
        // vaut mieux qu'une case vide, et cette branche ne doit jamais s'ouvrir.
        return QString::fromUtf8( domain::scaleIdentifier( p_scale ).data() );
    }

    return QCoreApplication::translate( "ModeDescription", SCALE_DISPLAY_NAMES.at( slot ) );
}

QVariantList describeScaleCircle( domain::Scale p_scale, std::int32_t p_tonicPitchClass, std::int32_t p_framePitchClass )
{
    // MEME CONSTRUCTION QUE LE CERCLE D'UN MODE, et volontairement ECRITE A PART.
    //
    // Factoriser les deux demanderait de toucher describeModeCircle, qui vient d'etre corrige et verifie : on ne remanie
    // pas ce qu'on vient de reparer, surtout quand le prochain chantier s'appuie dessus. Le jour ou une TROISIEME forme
    // arrivera, le point commun sera ecrit une bonne fois - et il sera alors visible dans deux fichiers au lieu d'un.
    const domain::ScaleDegrees degrees = domain::scaleDegreeOffsets( p_scale );

    std::vector<std::pair<std::int32_t, QVariantMap>> placed;

    placed.reserve( domain::SEMITONES_PER_OCTAVE );

    for( std::int32_t pitchClass = 0; pitchClass < static_cast<std::int32_t>( domain::SEMITONES_PER_OCTAVE ); ++pitchClass )
    {
        const std::int32_t fromTonic =
          ( ( ( pitchClass - p_tonicPitchClass ) % domain::SEMITONES_PER_OCTAVE ) + domain::SEMITONES_PER_OCTAVE )
          % domain::SEMITONES_PER_OCTAVE;

        std::int32_t stepIndex = -1;

        for( std::size_t degree = 0; degree < degrees.count; ++degree )
        {
            if( degrees.offsets.at( degree ) == fromTonic )
            {
                stepIndex = static_cast<std::int32_t>( degree );
            }
        }

        QVariantMap description;

        description.insert( QStringLiteral( "name" ),
                            QString::fromUtf8( PITCH_CLASS_NAMES.at( static_cast<std::size_t>( pitchClass ) ) ) );
        description.insert( QStringLiteral( "inMode" ), stepIndex >= 0 );
        description.insert( QStringLiteral( "isTonic" ), stepIndex == 0 );
        description.insert( QStringLiteral( "stepIndex" ), stepIndex );

        // RIEN N'EST MARQUE « CARACTERISTIQUE » ICI, et c'est exact : c'est une notion des MODES - la note qui les
        // distingue de leurs voisins dans l'ordre de couleur. Une gamme n'a pas de voisins de ce genre.
        description.insert( QStringLiteral( "isCharacteristic" ), false );

        // LE NOMBRE DE DEGRES VOYAGE DANS CHAQUE PASTILLE : le degrade de l'ecran se divise par le nombre d'ECARTS, et
        // cinq notes n'en ont pas autant que sept. Sans cela, une pentatonique se dessinerait sur un degrade trop long.
        description.insert( QStringLiteral( "degreeCount" ), static_cast<int>( degrees.count ) );

        const std::int32_t fromFrame =
          ( ( ( pitchClass - p_framePitchClass ) % domain::SEMITONES_PER_OCTAVE ) + domain::SEMITONES_PER_OCTAVE )
          % domain::SEMITONES_PER_OCTAVE;

        placed.emplace_back( ( fromFrame * FIFTHS_STEP ) % domain::SEMITONES_PER_OCTAVE, description );
    }

    std::ranges::sort( placed, std::less{}, &std::pair<std::int32_t, QVariantMap>::first );

    QVariantList circle;

    circle.reserve( static_cast<qsizetype>( placed.size() ) );

    for( const auto & item : placed )
    {
        circle.append( item.second );
    }

    return circle;
}

QVariantList describeModeCircle( domain::Mode p_mode, std::int32_t p_tonicPitchClass, std::int32_t p_framePitchClass )
{
    // LA PLACE DE CHAQUE NOTE DANS LA ROUE, calculee AVANT d'etre rangee - et c'est une CORRECTION, demandee par Roger :
    // « quand on a les 2 modes, on voit exactement le meme cercle, avec exactement les memes notes et exactement le meme
    // motif. Pourtant, les notes devraient etre differentes, non ? »
    //
    // Il a raison. Un mode relatif a les MEMES sept notes que son majeur : do ionien et re dorien, ce sont les memes
    // touches. Ce qui change, ce n'est pas quelles notes s'allument, c'est LAQUELLE EST LA TONIQUE - donc ou le trace
    // commence. Or la roue etait construite depuis la tonique du mode AFFICHE, c'est-a-dire qu'elle TOURNAIT avec lui :
    // deux modes relatifs donnaient litteralement la meme image.
    //
    // LA ROUE NE TOURNE PLUS. Son repere est la tonique du PREMIER mode (voir l'appelant), et le mode demande est dessine
    // DEDANS : sa tonique tombe sur un autre bouton, d'autres notes s'allument, et le trace ne part plus forcement du
    // haut. Quand les deux modes sont identiques, l'image est identique - et c'est acceptable : Roger l'a dit, « c'est un
    // jeu educatif, si le joueur triche avec la forme, ce n'est pas grave ».
    //
    // LE PAS de chaque note, lui, se compte toujours depuis LA TONIQUE DU MODE, et non depuis le repere : un rang de degre
    // parle du mode, pas de l'endroit ou on le regarde.
    const std::array<std::int32_t, domain::DEGREE_COUNT> offsets = domain::modeDegreeOffsets( p_mode );

    // LE DEGRE CARACTERISTIQUE : celui qui distingue le mode de ses voisins, et que l'ecran marque a part. Il vaut zero
    // pour les deux modes de reference - l'ionien et l'eolien n'ont rien a demontrer - donc ramene a un rang il vaut -1,
    // et aucun pas ne le porte. C'est exactement ce qu'on veut : rien a marquer.
    const std::int32_t characteristicStep = domain::modeCharacteristicDegree( p_mode ) - 1;

    std::vector<std::pair<std::int32_t, QVariantMap>> placed;

    placed.reserve( domain::SEMITONES_PER_OCTAVE );

    for( const domain::CircleNote & entry : domain::modeCircleNotes( p_mode, p_tonicPitchClass ) )
    {
        const auto index = static_cast<std::size_t>( entry.pitchClassIndex );

        const std::int32_t fromTonic = ( ( ( entry.pitchClassIndex - p_tonicPitchClass ) % domain::SEMITONES_PER_OCTAVE )
                                         + domain::SEMITONES_PER_OCTAVE )
                                       % domain::SEMITONES_PER_OCTAVE;

        // Le rang du degre, ou -1 : une note hors de la gamme n'est pas un pas, et l'ecran ne doit pas la proposer.
        std::int32_t stepIndex = -1;

        for( std::size_t degree = 0; degree < offsets.size(); ++degree )
        {
            if( offsets.at( degree ) == fromTonic )
            {
                stepIndex = static_cast<std::int32_t>( degree );
            }
        }

        QVariantMap description;

        description.insert( QStringLiteral( "name" ), QString::fromUtf8( PITCH_CLASS_NAMES.at( index ) ) );
        description.insert( QStringLiteral( "inMode" ), entry.belongsToMode );
        description.insert( QStringLiteral( "isTonic" ), entry.isTonic );
        description.insert( QStringLiteral( "stepIndex" ), stepIndex );
        description.insert( QStringLiteral( "isCharacteristic" ), stepIndex == characteristicStep );

        // SEPT DEGRES, dit dans chaque pastille comme pour les gammes : le degrade de l'ecran se divise par ce nombre, et
        // il ne doit pas avoir a savoir ce qu'il dessine. Voir ModeCircle.qml.
        description.insert( QStringLiteral( "degreeCount" ), static_cast<int>( domain::DEGREE_COUNT ) );

        // LA POSITION DANS LA ROUE : le rang de la note dans une suite de QUINTES partie du repere. Une quinte vaut sept
        // demi-tons, et sept est son propre inverse modulo douze (7 x 7 = 49 = 1 + 4 x 12), donc l'ordre des quintes
        // avance de 7 rangs par demi-ton - ce qui rend la position calculable sans rien parcourir.
        const std::int32_t fromFrame = ( ( ( entry.pitchClassIndex - p_framePitchClass ) % domain::SEMITONES_PER_OCTAVE )
                                         + domain::SEMITONES_PER_OCTAVE )
                                       % domain::SEMITONES_PER_OCTAVE;

        placed.emplace_back( ( fromFrame * FIFTHS_STEP ) % domain::SEMITONES_PER_OCTAVE, description );
    }

    std::ranges::sort( placed, std::less{}, &std::pair<std::int32_t, QVariantMap>::first );

    QVariantList circle;

    circle.reserve( static_cast<qsizetype>( placed.size() ) );

    for( const auto & item : placed )
    {
        circle.append( item.second );
    }

    return circle;
}

QVariantList describeScale( domain::Mode p_mode, std::int32_t p_tonicPitchClass )
{
    // Les sept offsets du mode, dans l'ordre des DEGRES : c'est le domaine qui les donne, et l'ecran ne les invente pas.
    // C'est aussi ce qui garantit que l'ordre affiche est celui qui a ete joue.
    const std::array<std::int32_t, domain::DEGREE_COUNT> offsets = domain::modeDegreeOffsets( p_mode );

    QVariantList scale;

    for( std::size_t index = 0; index < offsets.size(); ++index )
    {
        const std::int32_t pitchClass =
          ( ( ( p_tonicPitchClass + offsets.at( index ) ) % domain::SEMITONES_PER_OCTAVE ) + domain::SEMITONES_PER_OCTAVE )
          % domain::SEMITONES_PER_OCTAVE;

        QVariantMap note;

        note.insert( QStringLiteral( "name" ),
                     QString::fromUtf8( PITCH_CLASS_NAMES.at( static_cast<std::size_t>( pitchClass ) ) ) );
        note.insert( QStringLiteral( "stepIndex" ), static_cast<int>( index ) );
        note.insert( QStringLiteral( "isTonic" ), index == 0 );

        scale.append( note );
    }

    return scale;
}

QVariantList describeAllModes()
{
    QVariantList modes;

    for( std::size_t index = 0; index < domain::MODE_COUNT; ++index )
    {
        modes.append( describeMode( static_cast<domain::Mode>( index ) ) );
    }

    return modes;
}

}    // namespace musichien::ui
