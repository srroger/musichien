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
            break;

        case PlayerLevel::Fluent:
            // The obvious colours, and the palette widens twice as fast.
            settings.startingPaletteSize = 5;
            settings.successesBeforeWidening = 2;
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
