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
#include "domain/exercise/ReminderSchedule.h"
#include "domain/music/Temperament.h"

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

    // A QUELLE HEURE le rappel arrive. L'activer ou non est juste au-dessus ; ici, c'est le moment de la journee, et
    // cela se retient pour la meme raison qu'un instrument : une heure qu'on choisit et qui s'oublie n'est pas un choix.
    [[nodiscard]] virtual ReminderMoment storedReminderMoment() const = 0;

    virtual void storeReminderMoment( ReminderMoment p_moment ) = 0;

    // The TUNING the player wants to hear, and the root it is heard from.
    //
    // Equal temperament is the default and the reference: it is what the music around us is built on, and what an ear
    // must learn first. The older temperaments - a chain of pure fifths, or small whole-number ratios - are an
    // exploration, and they are only MEANINGFUL with a root note: the same interval does not sound the same in C and
    // in F#. A setting is justified when two reasonable people want different things; here, one wants the reference
    // and the other wants to hear what "just" means.
    [[nodiscard]] virtual Temperament storedTemperament() const = 0;

    virtual void storeTemperament( Temperament p_temperament ) = 0;

    // The ROOT note the tuning is heard from. In equal temperament it is irrelevant - that is the whole point of it.
    // In the others it is what gives every interval its meaning: a fifth above C is not built on the same reference
    // as a fifth above F#. Stored as a note, but only its PITCH CLASS matters for tuning; the octave does not change
    // an interval.
    [[nodiscard]] virtual Note storedTuningRoot() const = 0;

    virtual void storeTuningRoot( Note p_root ) = 0;

    // The A4 diapason, in hertz. 440 is the modern reference, but whole families of instruments are built sharper on
    // purpose - wind instruments, to sound brighter - and a tuner that cannot follow them is a tuner that only tunes
    // pianos. Stored as a plain number.
    [[nodiscard]] virtual double storedReferencePitch() const = 0;

    virtual void storeReferencePitch( double p_hertz ) = 0;

    // How many questions in a hundred ask the player to SING the interval, the rest asking him to name it. A rule of
    // the session, remembered so that a player who wants more singing does not ask for it every time.
    [[nodiscard]] virtual std::int32_t storedSingQuestionShare() const = 0;

    virtual void storeSingQuestionShare( std::int32_t p_share ) = 0;

    // Combien de questions sur cent portent sur le RYTHME.
    //
    // Meme regle, meme raison d'etre reglable : une part a zero, et le rythme disparait de la session sans qu'aucune
    // ligne de code ne change.
    [[nodiscard]] virtual std::int32_t storedRhythmQuestionShare() const = 0;

    virtual void storeRhythmQuestionShare( std::int32_t p_share ) = 0;

    // Combien de questions sur cent portent sur les ACCORDS.
    //
    // Troisieme part reglable, et la coincidence n'en est pas une : des qu'un genre de question existe, quelqu'un
    // voudra le doser. Elles vivent donc toutes les trois ici, et le controleeur les traite de la meme facon.
    [[nodiscard]] virtual std::int32_t storedChordQuestionShare() const = 0;

    virtual void storeChordQuestionShare( std::int32_t p_share ) = 0;

    // Part des questions, en pour cent, qui demandent de COMPARER deux modes sur un bourdon.
    //
    // ZERO par defaut, comme le rythme : le pilier harmonie se demande, il ne s'impose pas. Un joueur qui veut entendre
    // des modes met ce reglage a trente, et sa session change de nature sans qu'aucun ecran n'ait a le decider.
    [[nodiscard]] virtual std::int32_t storedModeColourQuestionShare() const = 0;

    virtual void storeModeColourQuestionShare( std::int32_t p_share ) = 0;

    // Part des questions, en pour cent, qui demandent de NOMMER un mode entendu sur un bourdon.
    //
    // Separee de la precedente parce que ce sont deux competences : comparer est une affaire d'oreille, nommer une
    // affaire de vocabulaire - et l'application doit pouvoir dire laquelle des deux resiste.
    [[nodiscard]] virtual std::int32_t storedModeNameQuestionShare() const = 0;

    virtual void storeModeNameQuestionShare( std::int32_t p_share ) = 0;
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

    [[nodiscard]] ReminderMoment storedReminderMoment() const override { return m_reminderMoment; }

    void storeReminderMoment( ReminderMoment p_moment ) override { m_reminderMoment = p_moment; }

    [[nodiscard]] Temperament storedTemperament() const override { return m_temperament; }

    void storeTemperament( Temperament p_temperament ) override { m_temperament = p_temperament; }

    [[nodiscard]] Note storedTuningRoot() const override { return m_tuningRoot; }

    void storeTuningRoot( Note p_root ) override { m_tuningRoot = p_root; }

    [[nodiscard]] double storedReferencePitch() const override { return m_referencePitch; }

    void storeReferencePitch( double p_hertz ) override { m_referencePitch = p_hertz; }

    [[nodiscard]] std::int32_t storedSingQuestionShare() const override { return m_singQuestionShare; }

    void storeSingQuestionShare( std::int32_t p_share ) override { m_singQuestionShare = p_share; }

    [[nodiscard]] std::int32_t storedRhythmQuestionShare() const override { return m_rhythmQuestionShare; }

    void storeRhythmQuestionShare( std::int32_t p_share ) override { m_rhythmQuestionShare = p_share; }

    [[nodiscard]] std::int32_t storedChordQuestionShare() const override { return m_chordQuestionShare; }

    void storeChordQuestionShare( std::int32_t p_share ) override { m_chordQuestionShare = p_share; }

    [[nodiscard]] std::int32_t storedModeColourQuestionShare() const override { return m_modeColourQuestionShare; }

    void storeModeColourQuestionShare( std::int32_t p_share ) override { m_modeColourQuestionShare = p_share; }

    [[nodiscard]] std::int32_t storedModeNameQuestionShare() const override { return m_modeNameQuestionShare; }

    void storeModeNameQuestionShare( std::int32_t p_share ) override { m_modeNameQuestionShare = p_share; }

private:
    std::optional<PlayerLevel> m_level;

    std::vector<bool> m_enabledInstruments;

    std::string m_playerName;

    std::int64_t m_totalExperience{ 0 };

    std::int64_t m_sessionCount{ 0 };

    std::int64_t m_starCount{ 0 };

    bool m_dailyReminderEnabled{ true };

    // 19 h par defaut : apres le diner, quand la journee de travail est finie et qu'une partie de dix questions est
    // encore possible. C'est le meme genre de choix que les trois moments d'anecdotes - une heure ou l'on est
    // disponible, pas une heure ou l'on est occupe.
    ReminderMoment m_reminderMoment{ .hour = 19, .minute = 0 };

    Temperament m_temperament{ Temperament::Equal };

    Note m_tuningRoot{ 60 };

    double m_referencePitch{ 440.0 };

    std::int32_t m_singQuestionShare{ 20 };

    // Vingt pour cent chacune, comme le chant : un premier lancement entend un peu de tout, et c'est au joueur de
    // decider ensuite ce qu'il veut travailler. Les memes valeurs que les reglages par defaut du domaine.
    //
    // SAUF le rythme, a zero, et c'est une valeur VOULUE : un reglage qui vaut zero ne veut pas dire « pas encore
    // fait », il veut dire « disponible, et pas impose ».
    std::int32_t m_rhythmQuestionShare{ 0 };

    std::int32_t m_chordQuestionShare{ 20 };

    // L'harmonie : zero par defaut, donc une session d'intervalles reste une session d'intervalles tant que le joueur
    // n'a pas demande autre chose. C'est un REGLAGE, et un reglage se demande.
    std::int32_t m_modeColourQuestionShare{ 0 };
    std::int32_t m_modeNameQuestionShare{ 0 };
};

}    // namespace musichien::domain
