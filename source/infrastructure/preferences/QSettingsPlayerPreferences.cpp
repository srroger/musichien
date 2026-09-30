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

// How many questions in a hundred ask the player to SING, the rest asking him to name the interval.
constexpr const char * SING_QUESTION_SHARE_KEY = "player/sing-question-share";
// La part de la question historique du jeu : nommer l'intervalle. Soixante par defaut, comme le domaine, donc un
// profil neuf sonne exactement comme le jeu d'avant que cette part existe.
constexpr const char * NAMED_INTERVAL_QUESTION_SHARE_KEY = "player/named-interval-question-share";

// Le tempo des phrases de mode, et son amplitude de variation. Soixante-douze par defaut, comme le contenu de l'atelier.
constexpr const char * PHRASE_TEMPO_BPM_KEY = "player/phrase-tempo-bpm";
constexpr const char * PHRASE_TEMPO_VARIATION_KEY = "player/phrase-tempo-variation";

// Les deux autres parts de question, gardees avec les memes bornes et la meme valeur de repli que celle du chant :
// trois reglages du meme genre se lisent de la meme facon, sinon l'un des trois finira par mentir.
constexpr const char * CHORD_QUESTION_SHARE_KEY = "player/chord-question-share";

// L'harmonie : deux parts, et deux cles distinctes. « Entendre une couleur » et « savoir la nommer » sont deux
// competences, donc deux reglages - un joueur peut vouloir l'une sans l'autre.
constexpr const char * MODE_COLOUR_QUESTION_SHARE_KEY = "player/mode-colour-question-share";
constexpr const char * MODE_NAME_QUESTION_SHARE_KEY = "player/mode-name-question-share";
constexpr const char * MODE_VAMP_QUESTION_SHARE_KEY = "player/mode-vamp-question-share";

// L'heure du rappel, en deux nombres separes : une heure et une minute se lisent dans un fichier de reglages plus
// facilement qu'un instant encode, et un joueur curieux doit pouvoir comprendre ce qu'il lit.
constexpr const char * REMINDER_HOUR_KEY = "player/reminder-hour";
constexpr const char * REMINDER_MINUTE_KEY = "player/reminder-minute";

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
    // Le rappel est ACTIF au premier lancement, et c'est un choix de produit : une application d'oreille musicale qui
    // ne se rappelle a personne est une application qu'on oublie. La cle n'existe pas encore a ce moment-la, donc le
    // defaut est bien celui-ci - et celui qui le coupe garde son choix, puisque sa cle, elle, existe.
    return QSettings{}.value( REMINDER_KEY, true ).toBool();
}

void QSettingsPlayerPreferences::storeDailyReminderEnabled( bool p_enabled )
{
    QSettings settings;

    settings.setValue( REMINDER_KEY, p_enabled );
}

domain::ReminderMoment QSettingsPlayerPreferences::storedReminderMoment() const
{
    const QSettings settings;

    const int hour = settings.value( REMINDER_HOUR_KEY, 19 ).toInt();
    const int minute = settings.value( REMINDER_MINUTE_KEY, 0 ).toInt();

    // Une heure impossible dans un fichier edite a la main est RAMENEE dans la journee plutot que refusee : le joueur
    // garde son rappel, a l'heure la plus proche de ce qu'il a voulu dire.
    return domain::clampedReminderMoment( domain::ReminderMoment{ hour, minute } );
}

void QSettingsPlayerPreferences::storeReminderMoment( domain::ReminderMoment p_moment )
{
    const domain::ReminderMoment moment = domain::clampedReminderMoment( p_moment );

    QSettings settings;

    settings.setValue( REMINDER_HOUR_KEY, moment.hour );
    settings.setValue( REMINDER_MINUTE_KEY, moment.minute );
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

std::int32_t QSettingsPlayerPreferences::storedPhraseTempoBpm() const
{
    // Soixante-douze par defaut, comme l'atelier : un profil neuf entend ce que Roger a valide a l'oreille. Les bornes
    // tiennent la phrase jouable - a quarante, elle traine ; a cent soixante, elle n'est plus une phrase.
    const std::int32_t stored = QSettings{}.value( PHRASE_TEMPO_BPM_KEY, 72 ).toInt();

    if( stored < 40 || stored > 160 )
    {
        return 72;
    }

    return stored;
}

void QSettingsPlayerPreferences::storePhraseTempoBpm( std::int32_t p_bpm )
{
    QSettings settings;

    settings.setValue( PHRASE_TEMPO_BPM_KEY, p_bpm );
}

std::int32_t QSettingsPlayerPreferences::storedPhraseTempoVariation() const
{
    // Vingt par defaut : Roger demandait « varier autour de 20-30 bpm », et vingt suffit a casser la monotonie sans
    // rendre un mode plus dur qu'un autre - toutes les phrases du jeu varient de la meme facon.
    const std::int32_t stored = QSettings{}.value( PHRASE_TEMPO_VARIATION_KEY, 20 ).toInt();

    if( stored < 0 || stored > 40 )
    {
        return 20;
    }

    return stored;
}

void QSettingsPlayerPreferences::storePhraseTempoVariation( std::int32_t p_variation )
{
    QSettings settings;

    settings.setValue( PHRASE_TEMPO_VARIATION_KEY, p_variation );
}

std::int32_t QSettingsPlayerPreferences::storedNamedIntervalQuestionShare() const
{
    // Soixante, comme le domaine : le defaut d'un profil neuf est le jeu tel qu'il etait avant que cette part existe.
    const std::int32_t stored = QSettings{}.value( NAMED_INTERVAL_QUESTION_SHARE_KEY, 60 ).toInt();

    // Une part hors bornes est une faute de frappe dans un fichier qu'un joueur peut ouvrir, et retombe sur le defaut.
    if( stored < 0 || stored > 100 )
    {
        return 60;
    }

    return stored;
}

void QSettingsPlayerPreferences::storeNamedIntervalQuestionShare( std::int32_t p_share )
{
    QSettings settings;

    settings.setValue( NAMED_INTERVAL_QUESTION_SHARE_KEY, p_share );
}

std::int32_t QSettingsPlayerPreferences::storedSingQuestionShare() const
{
    const std::int32_t stored = QSettings{}.value( SING_QUESTION_SHARE_KEY, 20 ).toInt();

    // A share outside the range is a typo in a file a player can open, and falls back to the default.
    if( stored < 0 || stored > 100 )
    {
        return 20;
    }

    return stored;
}

void QSettingsPlayerPreferences::storeSingQuestionShare( std::int32_t p_share )
{
    QSettings settings;

    settings.setValue( SING_QUESTION_SHARE_KEY, p_share );
}

std::int32_t QSettingsPlayerPreferences::storedChordQuestionShare() const
{
    const std::int32_t stored = QSettings{}.value( CHORD_QUESTION_SHARE_KEY, 20 ).toInt();

    if( stored < 0 || stored > 100 )
    {
        return 20;
    }

    return stored;
}

void QSettingsPlayerPreferences::storeChordQuestionShare( std::int32_t p_share )
{
    QSettings settings;

    settings.setValue( CHORD_QUESTION_SHARE_KEY, p_share );
}

std::int32_t QSettingsPlayerPreferences::storedModeColourQuestionShare() const
{
    // ZERO par defaut, comme le rythme : un reglage qui n'a jamais ete touche ne doit pas changer la nature du jeu.
    const std::int32_t stored = QSettings{}.value( MODE_COLOUR_QUESTION_SHARE_KEY, 0 ).toInt();

    if( stored < 0 || stored > 100 )
    {
        // Un fichier de reglages est un fichier qu'un humain peut editer : une valeur absurde doit couter le reglage,
        // jamais le lancement.
        return 0;
    }

    return stored;
}

void QSettingsPlayerPreferences::storeModeColourQuestionShare( std::int32_t p_share )
{
    QSettings settings;

    settings.setValue( MODE_COLOUR_QUESTION_SHARE_KEY, p_share );
}

std::int32_t QSettingsPlayerPreferences::storedModeNameQuestionShare() const
{
    const std::int32_t stored = QSettings{}.value( MODE_NAME_QUESTION_SHARE_KEY, 0 ).toInt();

    if( stored < 0 || stored > 100 )
    {
        return 0;
    }

    return stored;
}

void QSettingsPlayerPreferences::storeModeNameQuestionShare( std::int32_t p_share )
{
    QSettings settings;

    settings.setValue( MODE_NAME_QUESTION_SHARE_KEY, p_share );
}

std::int32_t QSettingsPlayerPreferences::storedModeVampQuestionShare() const
{
    const std::int32_t stored = QSettings{}.value( MODE_VAMP_QUESTION_SHARE_KEY, 0 ).toInt();

    if( stored < 0 || stored > 100 )
    {
        return 0;
    }

    return stored;
}

void QSettingsPlayerPreferences::storeModeVampQuestionShare( std::int32_t p_share )
{
    QSettings settings;

    settings.setValue( MODE_VAMP_QUESTION_SHARE_KEY, p_share );
}

}    // namespace musichien::infrastructure
