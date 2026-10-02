#include "domain/exercise/PlayerLevel.h"

#include <array>
#include <cstdint>

namespace musichien::domain
{

namespace
{

// L'EXPERIENCE CUMULEE QU'UN NIVEAU DEMANDE, et c'est une decision de GAME DESIGN autant que de musique.
//
// LE CALIBRAGE, chiffre en main : une ARCADE parfaite - vingt-cinq reussites, aucun coeur perdu - rapporte environ NEUF
// CENTS points (470 de score, doubles par le merite des coeurs). C'est la seule partie qui paie, donc c'est la seule
// unite qui compte, et les paliers se lisent en Arcades parfaites :
//
//   * « A l'aise » apres DEUX Arcades parfaites. Le joueur a montre qu'il tient une partie entiere ;
//   * « Jusqu'a l'octave » apres sept : c'est le palier ou l'on s'installe, et celui que beaucoup garderont ;
//   * « Les composes » apres vingt : le palier charniere, celui de Roger - « ce qui separe le joueur intermediaire du
//     joueur avance » ;
//   * « Je maitrise » apres cinquante. Loin, et volontairement : c'est le niveau ou l'application n'aide plus.
//
// POURQUOI CES CHIFFRES ONT TRIPLE (02/10/2026) : les premiers etaient calibres sur l'ancienne partie de dix questions,
// qui payait moins. Roger a joue les Arcades, en a perdu beaucoup, et a quand meme passe un palier : « ca m'a fait
// "Grinder" assez d'experience pour passer au niveau suivant ». Deux corrections ensemble - l'experience d'une partie
// PERDUE est desormais raboteuse (voir arcadeExperience), et les paliers sont BEAUCOUP plus hauts.
constexpr std::array<std::int64_t, PLAYER_LEVEL_COUNT> EXPERIENCE_THRESHOLDS{ 0, 1500, 6000, 18000, 45000 };

}    // namespace

std::int64_t experienceRequiredFor( PlayerLevel p_level ) noexcept
{
    return EXPERIENCE_THRESHOLDS.at( static_cast<std::size_t>( p_level ) );
}

PlayerLevel levelEarnedBy( std::int64_t p_experience ) noexcept
{
    // On descend depuis le plus haut : le premier seuil atteint est celui qui merite d'etre propose. L'ecriture « depuis
    // le haut » est celle qui n'oublie pas un palier le jour ou l'on en ajoute un.
    for( std::size_t index = PLAYER_LEVEL_COUNT; index > 0; --index )
    {
        const auto levelIndex = index - 1;

        if( p_experience >= EXPERIENCE_THRESHOLDS.at( levelIndex ) )
        {
            return playerLevelFromIndex( levelIndex );
        }
    }

    // Une experience negative - un fichier edite a la main - ne merite rien de plus que le premier palier.
    return PlayerLevel::Beginner;
}

namespace
{

// The settings of a session, before the level has had its say.
//
// Written once and copied, so that the three levels below read as the three differences they are: a session
// has the same length, the same lives and the same scoring whoever is playing.
[[nodiscard]] SessionSettings defaultSessionSettings()
{
    // Le mode guide n'est PAS active par defaut : il arrive la ou il compte, apres deux erreurs de suite. Voir
    // ExerciseSession::drawKind - la question suivante devient alors "ca monte ou ca descend ?", et c'est une aide,
    // pas un reglage. Le champ reste la pour le jour ou un joueur voudra plus de questions guidees.
    return SessionSettings{};
}

}    // namespace

SessionSettings sessionSettingsFor( PlayerLevel p_level )
{
    SessionSettings settings = defaultSessionSettings();

    switch( p_level )
    {
        case PlayerLevel::Beginner:
            // Two intervals, and a new one every three successes: the first session must be a success.
            settings.startingPaletteSize = 2;
            settings.successesBeforeWidening = 3;

            // Two chords as well: major and minor, the reference colour and its shadow. This is the level where
            // telling one from the other IS the exercise.
            settings.startingChordQualityCount = 2;

            // Et DEUX MODES : le majeur et son ombre, l'ionien et l'eolien. Meme raison que pour les accords - deux, et
            // les deux seuls qui ne demandent aucun vocabulaire.
            //
            // Ecrit EXPLICITEMENT alors que c'est deja le defaut des reglages : c'est ce qui rend la progression LISIBLE
            // dans un seul fichier. Chaque niveau dit ce qu'il donne, au lieu de le laisser deviner par un defaut pose
            // ailleurs.
            settings.startingModeCount = 2;
            break;

        case PlayerLevel::Fluent:
            // The obvious colours, and the palette widens twice as fast.
            settings.startingPaletteSize = 5;
            settings.successesBeforeWidening = 2;

            // LES QUATRE ACCORDS QU'ON RENCONTRE EN PREMIER, et ce ne sont plus les memes depuis le 01/10/2026 : le majeur,
            // le mineur, la SEPTIEME DE DOMINANTE et la SEPTIEME MAJEURE.
            //
            // C'est la correction que Roger a demandee : « c'est la ou je mets un bemol, Csus2 et Csus4 sont des accords plus
            // simples sur le papier, mais dans la pratique on les voit bien plus tard. Je mettrais plutot C7, beaucoup plus
            // utilise, et le Cmaj7 ». Il a raison, et la raison est ecrite dans Chord.cpp.
            settings.startingChordQualityCount = 4;

            // Et QUATRE MODES : l'ionien et l'eolien, plus le mixolydien et le PHRYGIEN.
            //
            // Le mixolydien est partout - blues, rock, jazz. Le phrygien est la demande de Roger, « car il est
            // caracteristique et beaucoup utilise, avec sa note orientale et espagnole », et il vient ICI pour la raison
            // qui gouverne deja l'ordre des intervalles : c'est le plus CONTRASTE de ceux qui restent.
            settings.startingModeCount = 4;
            break;

        case PlayerLevel::Advanced:
            // Every SIMPLE interval - up to the octave, which is exactly what "I know them up to the octave"
            // means. The compound intervals are not part of it: they are the same colours one octave higher,
            // and the palette reaches them on its own as the player succeeds.
            settings.startingPaletteSize = 12;
            settings.successesBeforeWidening = 2;

            // A wider grid, so that a player who knows every interval is not handed the answer by a grid of
            // six. Clamped by the palette, which holds twelve.
            settings.choiceCount = 8;

            // SEPT ACCORDS : les quatre du niveau precedent, plus le MINEUR SEPT, le DEMI-DIMINUE et le DIMINUE - la
            // famille FONCTIONNELLE, celle qui fait tourner les cadences (le ii-V-i, le vii° qui monte a la tonique).
            //
            // Roger : « ce sont aussi des accords tres utilises, le demi-diminue et le diminue : on les croise beaucoup plus
            // souvent que le Csus2 et le Csus4 ». Le sus, lui, part aux composes.
            settings.startingChordQualityCount = 7;

            // Et SIX MODES : il manque le locrien, le plus instable et le plus rare des sept. Ce que Roger resume ainsi -
            // « et la, on a tous les modes » - est vrai au niveau suivant.
            settings.startingModeCount = 6;
            break;

        case PlayerLevel::BeyondTheOctave:
            // Les douze intervalles simples, PUIS les premiers composes : c'est ce que demande un joueur qui
            // entend deja l'octave et veut savoir ce qu'il y a au-dessus.
            //
            // Les composes ne prennent pas de place nouvelle sur le cercle : ils se posent SUR la place de leur
            // classe, en petit, ce qui est exactement ce qui rend les traits interessants.
            settings.startingPaletteSize = 18;
            settings.successesBeforeWidening = 2;
            settings.choiceCount = 8;

            // ONZE ACCORDS ICI, et ce sont des COULEURS plutot que des fonctions : l'augmente, la sixte, et les deux sus.
            //
            // La SIXTE est la plus interessante du lot, et il faut dire pourquoi : C6 a EXACTEMENT les notes de Am7 - do, mi,
            // sol, la. Ce qui les separe, c'est l'ORDRE, et le projet joue toujours la tonique EN PREMIER (voir
            // CHORD_ROOT_SEMITONES), donc la basse. L'ambiguite est donc levee par la convention du jeu, et l'exercice porte
            // sur ce que la basse affirme. Roger : « la tonique reste la basse, on ne devrait pas avoir trop d'ambiguite ».
            settings.startingChordQualityCount = 11;

            // Et LES SEPT MODES : le locrien entre ici, et la palette des modes est complete.
            settings.startingModeCount = MODE_COUNT;
            break;

        case PlayerLevel::Master:
            // TOUTE la carte des le premier coup : les vingt-cinq intervalles que l'application connait, du
            // simple a la quinzieme - deux octaves pleines.
            //
            // La palette n'a plus rien a elargir, et c'est le domaine lui-meme qui s'en charge : widenPalette
            // s'arrete quand tout est en jeu. Elle n'a surtout rien a RETRECIR, ce qui est le vrai piege de ce
            // niveau : narrowPalette ne descend jamais sous la taille de depart, et une grille qui se vide sur
            // une erreur ne serait plus la carte entiere que ce mode promet.
            settings.startingPaletteSize = SUPPORTED_INTERVAL_COUNT;

            // Les vingt-cinq, donc : plus de leurres choisis dans le voisinage, plus rien a eliminer. Le joueur
            // entend un intervalle et il le NOMME, sur une carte complete. C'est tout l'interet, et c'est aussi
            // ce qui rend le mode mesurable : aucune question ne peut etre reussie par deducation.
            settings.choiceCount = SUPPORTED_INTERVAL_COUNT;

            // Aucune aide : ni l'indice qui souffle, ni le bouton qui donne la reponse.
            settings.aidsAllowed = false;

            // Et TOUTES les couleurs d'accord, comme les intervalles : le mode qui mesure ne cache rien. Le joueur
            // entend un accord et il le nomme sur un clavier complet, sans qu'aucune couleur ne soit arrivee apres lui.
            settings.startingChordQualityCount = CHORD_QUALITY_COUNT;

            // Les SEPT modes aussi, et cette ligne manquait.
            //
            // Le commentaire de ce niveau dit « toute la carte des le premier coup » et « le mode qui mesure ne cache
            // rien » : les intervalles et les accords suivaient, les modes non - un joueur qui maitrise commencait donc
            // avec deux modes, et attendait trois reussites pour entendre le troisieme. C'est un test du GodMode qui l'a
            // montre, en comparant le modele du niveau a ce que le niveau donne vraiment.
            settings.startingModeCount = MODE_COUNT;
            break;
    }

    return settings;
}

PlayerLevel playerLevelFromIndex( std::size_t p_index ) noexcept
{
    if( p_index >= PLAYER_LEVEL_COUNT )
    {
        return PlayerLevel::Beginner;
    }

    return static_cast<PlayerLevel>( p_index );
}

}    // namespace musichien::domain
