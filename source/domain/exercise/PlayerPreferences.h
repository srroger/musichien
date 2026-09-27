#pragma once

// =====================================================================================================================
// Musichien - PlayerPreferences
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

#include <cstdint>
#include <optional>
#include <string>

namespace musichien::domain
{

class PlayerPreferences
{
public:
    PlayerPreferences() = default;

    PlayerPreferences( const PlayerPreferences & ) = delete;
    PlayerPreferences & operator=( const PlayerPreferences & ) = delete;
    PlayerPreferences( PlayerPreferences && ) = delete;
    PlayerPreferences & operator=( PlayerPreferences && ) = delete;

    virtual ~PlayerPreferences() = default;

    // The level the player chose, or nothing at all the first time - which is a question to ask, not a
    // default to assume.
    [[nodiscard]] virtual std::optional<PlayerLevel> storedLevel() const = 0;

    virtual void storeLevel( PlayerLevel p_level ) = 0;

    // Which instruments the player WANTS to hear, one flag per instrument, in the order the application loads
    // them.
    //
    // This is not a detail of comfort. A saxophone at the same level as a piano is aggressive - it is rich and
    // odd-harmonic - and a timbre that grates is a timbre that makes the application get closed, especially at
    // night. An instrument the player does not want must therefore be a choice he can make once, and it must
    // SURVIVE the next launch: a preference that resets itself is not a preference.
    //
    // An empty list means "everything", which is also what a first run has.
    [[nodiscard]] virtual std::vector<bool> storedEnabledInstruments() const = 0;

    virtual void storeEnabledInstruments( std::vector<bool> p_enabledInstruments ) = 0;

    // The name the player gave himself, when there is one.
    //
    // Part of the PROFILE, like the level: it is asked once and remembered, not a setting to be toggled. The domain
    // asks for a plain string, and it does not care whether the drawer is a file, a variable or a server.
    [[nodiscard]] virtual std::string playerName() const = 0;

    virtual void storePlayerName( std::string p_name ) = 0;

    // Experience accumulated across sessions.
    //
    // Kept here rather than recomputed: a session ends, its experience is ADDED to this total by the controller, and
    // the total is what the profile shows. A total that grew by the session's own sum is a total nobody has to
    // recount.
    [[nodiscard]] virtual std::int64_t totalExperience() const = 0;

    virtual void storeTotalExperience( std::int64_t p_total ) = 0;

    // How many sessions have been played to the end, and how many earned their star.
    //
    // These two are the spine of the statistics page: a player who sees his sessions grow and his stars appear
    // sees himself improving. They are ADDED, never reset, by the controller when a session ends.
    [[nodiscard]] virtual std::int64_t sessionCount() const = 0;

    virtual void storeSessionCount( std::int64_t p_count ) = 0;

    [[nodiscard]] virtual std::int64_t starCount() const = 0;

    virtual void storeStarCount( std::int64_t p_count ) = 0;

    // Whether the daily reminder is on.
    //
    // A REMINDER is a setting, and it is justified because two reasonable people want different things: the player
    // who comes back on his own, and the player who needs the nudge. Same drawer as the instruments for the same
    // reason - what the player wants, remembered.
    [[nodiscard]] virtual bool dailyReminderEnabled() const = 0;

    virtual void storeDailyReminderEnabled( bool p_enabled ) = 0;
};

// Remembers a level in a variable, for the tests and for a first run on a machine that has no file yet.
class PlayerPreferencesFake final : public PlayerPreferences
{
public:
    [[nodiscard]] std::optional<PlayerLevel> storedLevel() const override { return m_level; }

    void storeLevel( PlayerLevel p_level ) override { m_level = p_level; }

    [[nodiscard]] std::vector<bool> storedEnabledInstruments() const override
    {
        return m_enabledInstruments;
    }

    void storeEnabledInstruments( std::vector<bool> p_enabledInstruments ) override
    {
        m_enabledInstruments = std::move( p_enabledInstruments );
    }

    [[nodiscard]] std::string playerName() const override { return m_playerName; }

    void storePlayerName( std::string p_name ) override { m_playerName = std::move( p_name ); }

    [[nodiscard]] std::int64_t totalExperience() const override { return m_totalExperience; }

    void storeTotalExperience( std::int64_t p_total ) override { m_totalExperience = p_total; }

    [[nodiscard]] std::int64_t sessionCount() const override { return m_sessionCount; }

    void storeSessionCount( std::int64_t p_count ) override { m_sessionCount = p_count; }

    [[nodiscard]] std::int64_t starCount() const override { return m_starCount; }

    void storeStarCount( std::int64_t p_count ) override { m_starCount = p_count; }

    [[nodiscard]] bool dailyReminderEnabled() const override { return m_dailyReminderEnabled; }

    void storeDailyReminderEnabled( bool p_enabled ) override { m_dailyReminderEnabled = p_enabled; }

private:
    std::optional<PlayerLevel> m_level;

    std::vector<bool> m_enabledInstruments;

    std::string m_playerName;

    std::int64_t m_totalExperience{ 0 };

    std::int64_t m_sessionCount{ 0 };

    std::int64_t m_starCount{ 0 };

    bool m_dailyReminderEnabled{ false };
};

}    // namespace musichien::domain
