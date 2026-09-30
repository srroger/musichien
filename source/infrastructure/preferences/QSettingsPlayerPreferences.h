#pragma once

// =====================================================================================================================
// Musichien - QSettingsPlayerPreferences
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

#include "domain/exercise/PlayerPreferences.h"

namespace musichien::infrastructure
{

class QSettingsPlayerPreferences final : public domain::PlayerPreferences
{
public:
    QSettingsPlayerPreferences() = default;

    [[nodiscard]] std::optional<domain::PlayerLevel> storedLevel() const override;

    void storeLevel( domain::PlayerLevel p_level ) override;

    [[nodiscard]] std::vector<bool> storedEnabledInstruments() const override;

    void storeEnabledInstruments( std::vector<bool> p_enabledInstruments ) override;

    [[nodiscard]] std::string playerName() const override;

    void storePlayerName( std::string p_name ) override;

    [[nodiscard]] std::int64_t totalExperience() const override;

    void storeTotalExperience( std::int64_t p_total ) override;

    [[nodiscard]] std::int64_t sessionCount() const override;

    void storeSessionCount( std::int64_t p_count ) override;

    [[nodiscard]] std::int64_t starCount() const override;

    void storeStarCount( std::int64_t p_count ) override;

    [[nodiscard]] bool dailyReminderEnabled() const override;

    void storeDailyReminderEnabled( bool p_enabled ) override;

    [[nodiscard]] domain::ReminderMoment storedReminderMoment() const override;

    void storeReminderMoment( domain::ReminderMoment p_moment ) override;

    [[nodiscard]] domain::Temperament storedTemperament() const override;

    void storeTemperament( domain::Temperament p_temperament ) override;

    [[nodiscard]] domain::Note storedTuningRoot() const override;

    void storeTuningRoot( domain::Note p_root ) override;

    [[nodiscard]] double storedReferencePitch() const override;

    void storeReferencePitch( double p_hertz ) override;

    [[nodiscard]] std::int32_t storedPhraseTempoBpm() const override;

    void storePhraseTempoBpm( std::int32_t p_bpm ) override;

    [[nodiscard]] std::int32_t storedPhraseTempoVariation() const override;

    void storePhraseTempoVariation( std::int32_t p_variation ) override;

    [[nodiscard]] std::int32_t storedNamedIntervalQuestionShare() const override;

    void storeNamedIntervalQuestionShare( std::int32_t p_share ) override;

    [[nodiscard]] std::int32_t storedSingQuestionShare() const override;

    void storeSingQuestionShare( std::int32_t p_share ) override;

    [[nodiscard]] std::int32_t storedChordQuestionShare() const override;

    void storeChordQuestionShare( std::int32_t p_share ) override;

    [[nodiscard]] std::int32_t storedModeColourQuestionShare() const override;

    void storeModeColourQuestionShare( std::int32_t p_share ) override;

    [[nodiscard]] std::int32_t storedModeNameQuestionShare() const override;

    void storeModeNameQuestionShare( std::int32_t p_share ) override;

    [[nodiscard]] std::int32_t storedModeVampQuestionShare() const override;

    void storeModeVampQuestionShare( std::int32_t p_share ) override;
};

}    // namespace musichien::infrastructure
