#pragma once

// =====================================================================================================================
// Musichien - PlayerLevelStore
//
// Where the level of the player is REMEMBERED between two launches.
//
// A port, exactly like NotePlayer, and for the same reason: the domain and the view model must be able to say
// "remember this" without knowing whether the answer lives in a settings file, in a save file or in a server
// that does not exist. The infrastructure provides the file; a test provides a variable.
//
// The type is deliberately narrow. A general purpose preferences store would be an invitation to put every
// future setting here, and this project has a rule about that: an option is justified only when two
// reasonable people want different things.
// =====================================================================================================================

#include "domain/exercise/PlayerLevel.h"

#include <optional>

namespace musichien::domain
{

class PlayerLevelStore
{
public:
    PlayerLevelStore() = default;

    PlayerLevelStore( const PlayerLevelStore & ) = delete;
    PlayerLevelStore & operator=( const PlayerLevelStore & ) = delete;
    PlayerLevelStore( PlayerLevelStore && ) = delete;
    PlayerLevelStore & operator=( PlayerLevelStore && ) = delete;

    virtual ~PlayerLevelStore() = default;

    // The level the player chose, or nothing at all the first time - which is a question to ask, not a
    // default to assume.
    [[nodiscard]] virtual std::optional<PlayerLevel> storedLevel() const = 0;

    virtual void storeLevel( PlayerLevel p_level ) = 0;
};

// Remembers a level in a variable, for the tests and for a first run on a machine that has no file yet.
class PlayerLevelStoreFake final : public PlayerLevelStore
{
public:
    [[nodiscard]] std::optional<PlayerLevel> storedLevel() const override { return m_level; }

    void storeLevel( PlayerLevel p_level ) override { m_level = p_level; }

private:
    std::optional<PlayerLevel> m_level;
};

}    // namespace musichien::domain
