#pragma once

// =====================================================================================================================
// Musichien - QSettingsPlayerLevelStore
//
// Where the level of the player actually lives between two launches: a settings file, on the machine.
//
// ---------------------------------------------------------------------------------------------------------------------
// Why a file of our own, and not a server
//
// This is the whole promise of the project in one line: what the application remembers about its player stays
// on the device. QSettings writes a small file, the application never sends it anywhere, and the package
// cannot reach the network - so there is nothing to explain and nothing to trust.
//
// The class is thin on purpose: deciding WHICH preference exists belongs to the domain, and this is only the
// drawer it is kept in.
// =====================================================================================================================

#include "domain/exercise/PlayerLevelStore.h"

namespace musichien::infrastructure
{

class QSettingsPlayerLevelStore final : public domain::PlayerLevelStore
{
public:
    QSettingsPlayerLevelStore() = default;

    [[nodiscard]] std::optional<domain::PlayerLevel> storedLevel() const override;

    void storeLevel( domain::PlayerLevel p_level ) override;
};

}    // namespace musichien::infrastructure
