#include "ui/IntervalDescription.h"

#include <QCoreApplication>
#include <QString>

namespace musichien::ui
{

namespace
{

// =====================================================================================================================
// LE NOM FRANCAIS, et pourquoi il vit ICI plutot que dans le domaine
//
// L'intervalle du domaine se nomme en ANGLAIS (« Perfect fifth ») : c'est le nom du MODELE, celui qui ne bouge pas et
// qui sert au code. Ce qui s'AFFICHE, lui, est en FRANCAIS. Roger a trace la frontiere lui-meme : « pour ce qui est
// affiche a l'utilisateur il vaut mieux le mettre en francais ; par contre pour le code source derriere, les noms de
// variables, les cles des tableaux... le nom anglais est tres bien ».
//
// La traduction appartient donc a l'ECRAN, pas a la regle du jeu : le domaine nomme, la couche ui traduit. Un domaine
// qui parlerait francais obligerait la regle du jeu a connaitre la langue du joueur, ce qui n'est l'affaire d'aucune
// des deux.
//
// QT_TRANSLATE_NOOP est pose MEME SANS FICHIER DE TRADUCTION. Roger : « il vaut mieux prendre l'habitude de le mettre ».
// C'est exactement ce que la macro fait : elle MARQUE le texte pour l'extraction sans rien traduire aujourd'hui, donc
// le jour ou un fichier de traduction existera, aucun mot n'aura ete oublie - ce qui est la seule erreur qu'on ne peut
// pas rattraper apres coup.
// =====================================================================================================================

// L'ORDRE SUIT IntervalQuality, comme la table anglaise du domaine : une qualite ajoutee ailleurs ferait echouer la
// compilation ici plutot que de glisser un nom faux dans un ecran.
constexpr std::array<const char *, 5> QUALITY_MASCULINE{
  QT_TRANSLATE_NOOP( "IntervalDescription", "juste" ),
  QT_TRANSLATE_NOOP( "IntervalDescription", "majeur" ),
  QT_TRANSLATE_NOOP( "IntervalDescription", "mineur" ),
  QT_TRANSLATE_NOOP( "IntervalDescription", "augmenté" ),
  QT_TRANSLATE_NOOP( "IntervalDescription", "diminué" ),
};

// L'ACCORD DE GENRE : l'unisson est le SEUL masculin - « unisson juste », mais « quinte JUSTE » et « tierce MAJEURE ».
// D'ou deux formes par qualite, et deux seulement : « juste » ne s'accorde pas.
constexpr std::array<const char *, 5> QUALITY_FEMININE{
  QT_TRANSLATE_NOOP( "IntervalDescription", "juste" ),
  QT_TRANSLATE_NOOP( "IntervalDescription", "majeure" ),
  QT_TRANSLATE_NOOP( "IntervalDescription", "mineure" ),
  QT_TRANSLATE_NOOP( "IntervalDescription", "augmentée" ),
  QT_TRANSLATE_NOOP( "IntervalDescription", "diminuée" ),
};

// LES ACCENTS SONT DANS LE TEXTE, et pas seulement dans les commentaires : c'est ce que le joueur LIT. Les commentaires
// du depot evitent les accents par convention ; un libelle d'ecran qui les eviterait serait simplement mal ecrit.
//
// La MAJUSCULE aussi : le nom vient en premier, donc il ouvre le libelle - « Quinte juste ».
constexpr std::array<const char *, 15> NUMBER_WORDS{
  QT_TRANSLATE_NOOP( "IntervalDescription", "Unisson" ),
  QT_TRANSLATE_NOOP( "IntervalDescription", "Seconde" ),
  QT_TRANSLATE_NOOP( "IntervalDescription", "Tierce" ),
  QT_TRANSLATE_NOOP( "IntervalDescription", "Quarte" ),
  QT_TRANSLATE_NOOP( "IntervalDescription", "Quinte" ),
  QT_TRANSLATE_NOOP( "IntervalDescription", "Sixte" ),
  QT_TRANSLATE_NOOP( "IntervalDescription", "Septième" ),
  QT_TRANSLATE_NOOP( "IntervalDescription", "Octave" ),
  QT_TRANSLATE_NOOP( "IntervalDescription", "Neuvième" ),
  QT_TRANSLATE_NOOP( "IntervalDescription", "Dixième" ),
  QT_TRANSLATE_NOOP( "IntervalDescription", "Onzième" ),
  QT_TRANSLATE_NOOP( "IntervalDescription", "Douzième" ),
  QT_TRANSLATE_NOOP( "IntervalDescription", "Treizième" ),
  QT_TRANSLATE_NOOP( "IntervalDescription", "Quatorzième" ),
  QT_TRANSLATE_NOOP( "IntervalDescription", "Quinzième" ),
};

[[nodiscard]] QString frenchName( const domain::Interval & p_interval )
{
    const auto qualitySlot = static_cast<std::size_t>( p_interval.quality() );
    const auto numberSlot = static_cast<std::size_t>( p_interval.number() ) - 1;

    if( ( qualitySlot >= QUALITY_MASCULINE.size() ) || ( numberSlot >= NUMBER_WORDS.size() ) )
    {
        // Un intervalle hors table n'existe pas, mais un ecran doit pouvoir afficher QUELQUE CHOSE : on rend alors le nom
        // du modele. Un mot en anglais vaut mieux qu'une case vide, et cette branche ne doit jamais s'ouvrir.
        return QString::fromStdString( p_interval.name() );
    }

    const char * const qualityWord = ( p_interval.number() == 1 ) ? QUALITY_MASCULINE.at( qualitySlot )
                                                                  : QUALITY_FEMININE.at( qualitySlot );

    // L'ORDRE EST CELUI DU FRANCAIS : le nom d'abord, la qualite ensuite - « quinte juste ». L'anglais fait l'inverse
    // (« Perfect fifth »), et garder son ordre aurait donne « juste quinte » : deux mots francais dans un ordre anglais.
    return QCoreApplication::translate( "IntervalDescription", "%1 %2" )
      .arg( QCoreApplication::translate( "IntervalDescription", NUMBER_WORDS.at( numberSlot ) ),
            QCoreApplication::translate( "IntervalDescription", qualityWord ) );
}

}    // namespace

QVariantMap describeInterval( const domain::Interval & p_interval )
{
    return QVariantMap{
      { QStringLiteral( "identifier" ), QString::fromStdString( p_interval.identifier() ) },
      // LE NOM EST CELUI DU JOUEUR. L'identifiant juste au-dessus, lui, reste anglais et stable : c'est lui qu'un fichier
      // de sauvegarde ou un contenu garderait, et il ne doit jamais dependre de la langue de l'ecran.
      { QStringLiteral( "name" ), frenchName( p_interval ) },
      { QStringLiteral( "semitones" ), p_interval.semitones() },
      { QStringLiteral( "intervalClass" ), p_interval.intervalClass() },
      { QStringLiteral( "octaveSpan" ), p_interval.octaveSpan() },
      { QStringLiteral( "number" ), p_interval.number() },
      { QStringLiteral( "isCompound" ), p_interval.isCompound() },
    };
}

}    // namespace musichien::ui
