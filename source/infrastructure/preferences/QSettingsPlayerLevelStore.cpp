#include "infrastructure/preferences/QSettingsPlayerLevelStore.h"

#include <QSettings>

#include <cstddef>

namespace musichien::infrastructure
{

namespace
{

// The key of the preference, grouped so that the next ones have somewhere obvious to go.
//
// Written as a string and read as a string: a settings file is a text file a curious player can open, and
// "player/level=2" is readable where a binary blob would not be.
constexpr const char * LEVEL_KEY = "player/level";

}    // namespace

std::optional<domain::PlayerLevel> QSettingsPlayerLevelStore::storedLevel() const
{
    const QSettings settings;

    if( !settings.contains( LEVEL_KEY ) )
    {
        // Nothing stored is NOT the same as "beginner": it is a question that has never been asked, and the
        // screen has to know the difference to ask it.
        return std::nullopt;
    }

    return domain::playerLevelFromIndex(
      static_cast<std::size_t>( settings.value( LEVEL_KEY ).toInt() ) );
}

void QSettingsPlayerLevelStore::storeLevel( domain::PlayerLevel p_level )
{
    QSettings settings;

    settings.setValue( LEVEL_KEY, static_cast<int>( p_level ) );
}

}    // namespace musichien::infrastructure
