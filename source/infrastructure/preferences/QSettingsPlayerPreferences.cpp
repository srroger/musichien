#include "infrastructure/preferences/QSettingsPlayerPreferences.h"

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

// One flag per instrument, stored as a LIST rather than as a bit pattern: the settings file stays readable, and
// adding an instrument to the end cannot silently shift the others.
constexpr const char * INSTRUMENTS_KEY = "player/instruments";

// The profile: a name and an experience total. Both stored as readable text, like every other preference.
constexpr const char * NAME_KEY = "player/name";

constexpr const char * EXPERIENCE_KEY = "player/experience";

// The statistics page, in two numbers: how many sessions were played, and how many earned their star.
constexpr const char * SESSIONS_KEY = "player/sessions";

constexpr const char * STARS_KEY = "player/stars";

// The daily reminder.
constexpr const char * REMINDER_KEY = "player/reminder";

// The tuning. Stored as the enum's own number, and read back through a range check.
constexpr const char * TEMPERAMENT_KEY = "player/temperament";

// The root note of the tuning, as a MIDI number.
constexpr const char * TUNING_ROOT_KEY = "player/tuning-root";

// The A4 diapason, in hertz.
constexpr const char * REFERENCE_PITCH_KEY = "player/reference-pitch";

}    // namespace

std::optional<domain::PlayerLevel> QSettingsPlayerPreferences::storedLevel() const
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

void QSettingsPlayerPreferences::storeLevel( domain::PlayerLevel p_level )
{
    QSettings settings;

    settings.setValue( LEVEL_KEY, static_cast<int>( p_level ) );
}

std::vector<bool> QSettingsPlayerPreferences::storedEnabledInstruments() const
{
    const QSettings settings;

    const QVariantList storedFlags = settings.value( INSTRUMENTS_KEY ).toList();

    std::vector<bool> enabledInstruments;

    enabledInstruments.reserve( static_cast<std::size_t>( storedFlags.size() ) );

    for( const QVariant & storedFlag : storedFlags )
    {
        enabledInstruments.push_back( storedFlag.toBool() );
    }

    // An empty list is a first run, or a settings file a player has edited away: the caller then treats it as
    // "everything", which is what a fresh installation should sound like.
    return enabledInstruments;
}

void QSettingsPlayerPreferences::storeEnabledInstruments( std::vector<bool> p_enabledInstruments )
{
    QVariantList storedFlags;

    for( const bool isEnabled : p_enabledInstruments )
    {
        storedFlags.append( isEnabled );
    }

    QSettings settings;

    settings.setValue( INSTRUMENTS_KEY, storedFlags );
}

std::string QSettingsPlayerPreferences::playerName() const
{
    return QSettings{}.value( NAME_KEY ).toString().toStdString();
}

void QSettingsPlayerPreferences::storePlayerName( std::string p_name )
{
    QSettings settings;

    settings.setValue( NAME_KEY, QString::fromStdString( p_name ) );
}

std::int64_t QSettingsPlayerPreferences::totalExperience() const
{
    return static_cast<std::int64_t>( QSettings{}.value( EXPERIENCE_KEY, 0 ).toLongLong() );
}

void QSettingsPlayerPreferences::storeTotalExperience( std::int64_t p_total )
{
    QSettings settings;

    settings.setValue( EXPERIENCE_KEY, static_cast<qlonglong>( p_total ) );
}

std::int64_t QSettingsPlayerPreferences::sessionCount() const
{
    return static_cast<std::int64_t>( QSettings{}.value( SESSIONS_KEY, 0 ).toLongLong() );
}

void QSettingsPlayerPreferences::storeSessionCount( std::int64_t p_count )
{
    QSettings settings;

    settings.setValue( SESSIONS_KEY, static_cast<qlonglong>( p_count ) );
}

std::int64_t QSettingsPlayerPreferences::starCount() const
{
    return static_cast<std::int64_t>( QSettings{}.value( STARS_KEY, 0 ).toLongLong() );
}

void QSettingsPlayerPreferences::storeStarCount( std::int64_t p_count )
{
    QSettings settings;

    settings.setValue( STARS_KEY, static_cast<qlonglong>( p_count ) );
}

bool QSettingsPlayerPreferences::dailyReminderEnabled() const
{
    return QSettings{}.value( REMINDER_KEY, false ).toBool();
}

void QSettingsPlayerPreferences::storeDailyReminderEnabled( bool p_enabled )
{
    QSettings settings;

    settings.setValue( REMINDER_KEY, p_enabled );
}

domain::Temperament QSettingsPlayerPreferences::storedTemperament() const
{
    // Read as an UNSIGNED number: a settings file is a text file a player can open, and a negative value he typed
    // there must fall back to the reference, not wrap around into the last temperament of the list.
    const auto stored = QSettings{}.value( TEMPERAMENT_KEY, 0U ).toUInt();

    if( stored >= domain::TEMPERAMENT_NAMES.size() )
    {
        return domain::Temperament::Equal;
    }

    return static_cast<domain::Temperament>( stored );
}

void QSettingsPlayerPreferences::storeTemperament( domain::Temperament p_temperament )
{
    QSettings settings;

    settings.setValue( TEMPERAMENT_KEY, static_cast<int>( p_temperament ) );
}

domain::Note QSettingsPlayerPreferences::storedTuningRoot() const
{
    const int stored = QSettings{}.value( TUNING_ROOT_KEY, 60 ).toInt();

    if( stored < domain::Note::MINIMUM_MIDI_NUMBER || stored > domain::Note::MAXIMUM_MIDI_NUMBER )
    {
        return domain::Note{ 60 };
    }

    return domain::Note{ stored };
}

void QSettingsPlayerPreferences::storeTuningRoot( domain::Note p_root )
{
    QSettings settings;

    settings.setValue( TUNING_ROOT_KEY, p_root.midiNumber() );
}

double QSettingsPlayerPreferences::storedReferencePitch() const
{
    const double stored = QSettings{}.value( REFERENCE_PITCH_KEY, 440.0 ).toDouble();

    // A settings file is a text file a player can open. A wildly out of range value falls back to the reference.
    if( stored < 400.0 || stored > 480.0 )
    {
        return 440.0;
    }

    return stored;
}

void QSettingsPlayerPreferences::storeReferencePitch( double p_hertz )
{
    QSettings settings;

    settings.setValue( REFERENCE_PITCH_KEY, p_hertz );
}

}    // namespace musichien::infrastructure
