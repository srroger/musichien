#include "domain/exercise/PlayerLevel.h"

namespace musichien::domain
{

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
            break;

        case PlayerLevel::Fluent:
            // The obvious colours, and the palette widens twice as fast.
            settings.startingPaletteSize = 5;
            settings.successesBeforeWidening = 2;

            // The four triads a musician meets first: major, minor, and the two suspended ones, where the third is
            // REPLACED rather than moved. Someone who already hears intervals does not need a two-colour diet.
            settings.startingChordQualityCount = 4;
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

            // Les six triades, tendues comprises : diminue et augmente entrent ici, ou l'oreille sait deja entendre
            // une quinte serree ou elargie.
            settings.startingChordQualityCount = 6;
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

            // Et les TROIS septiemes, la famille la plus entendue de toutes : la septieme de dominante, dont l'oreille
            // attrape la tension sans savoir la nommer, puis la majeure et la mineure, qui n'en different que d'une
            // note. Six triades plus trois septiemes : neuf couleurs, et les accords a QUATRE notes sont arrives.
            settings.startingChordQualityCount = 9;
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
