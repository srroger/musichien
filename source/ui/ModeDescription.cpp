#include "ui/ModeDescription.h"

#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

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

QVariantList describeModeCircle( domain::Mode p_mode, std::int32_t p_tonicPitchClass )
{
    QVariantList circle;

    for( const domain::CircleNote & entry : domain::modeCircleNotes( p_mode, p_tonicPitchClass ) )
    {
        const auto index = static_cast<std::size_t>( entry.pitchClassIndex );

        QVariantMap description;

        description.insert( QStringLiteral( "name" ), QString::fromUtf8( PITCH_CLASS_NAMES.at( index ) ) );
        description.insert( QStringLiteral( "inMode" ), entry.belongsToMode );
        description.insert( QStringLiteral( "isTonic" ), entry.isTonic );

        circle.append( description );
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
