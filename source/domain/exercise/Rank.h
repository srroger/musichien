#pragma once

// =====================================================================================================================
// Musichien - Rank
//
// The rank of the streak, in the spirit of Devil May Cry: it climbs from D up to SSS as the player chains correct
// answers. A rank is a RULE of the game, expressed as pure data - the screen displays it, and never decides it.
// =====================================================================================================================

#include <cstddef>

namespace musichien::domain
{

enum class Rank
{
    D,
    C,
    B,
    A,
    S,
    SS,
    SSS
};

// The rank a streak earns. The ladder is steep on purpose: reaching SSS has to feel like something, and a rank that
// everyone holds is a rank nobody looks at.
[[nodiscard]] constexpr Rank rankForStreak( std::size_t p_streak ) noexcept
{
    if( p_streak >= 13 )
    {
        return Rank::SSS;
    }

    if( p_streak >= 10 )
    {
        return Rank::SS;
    }

    if( p_streak >= 8 )
    {
        return Rank::S;
    }

    if( p_streak >= 6 )
    {
        return Rank::A;
    }

    if( p_streak >= 4 )
    {
        return Rank::B;
    }

    if( p_streak >= 2 )
    {
        return Rank::C;
    }

    return Rank::D;
}

}    // namespace musichien::domain
