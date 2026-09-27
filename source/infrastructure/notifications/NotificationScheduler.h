#pragma once

// =====================================================================================================================
// Musichien - NotificationScheduler
//
// The daily reminder, the way Duolingo does it: a nudge at the same time every day, so that the player comes back.
//
// This is a PORT, exactly like NotePlayer: the application asks for a reminder, and it does not care whether the
// answer is an Android AlarmManager, a desktop no-op or a push service that does not exist. The infrastructure
// provides the real one; a test provides the silence.
// =====================================================================================================================

namespace musichien::infrastructure
{

class NotificationScheduler
{
public:
    NotificationScheduler() = default;

    NotificationScheduler( const NotificationScheduler & ) = delete;
    NotificationScheduler & operator=( const NotificationScheduler & ) = delete;
    NotificationScheduler( NotificationScheduler && ) = delete;
    NotificationScheduler & operator=( NotificationScheduler && ) = delete;

    virtual ~NotificationScheduler() = default;

    // A reminder every day at this hour and minute, carrying the given content (a drawn anecdote, or a plain
    // nudge). Scheduling twice replaces the previous one. The text is frozen into the alarm, so the receiver can
    // show it without asking back - a notification is the one message that must survive a reboot.
    virtual void scheduleDailyReminder( int p_hour, int p_minute, std::string_view p_content ) = 0;

    // No more reminders.
    virtual void cancelReminder() = 0;

    // Fires a reminder RIGHT NOW: the developer button, to check that the plumbing works. It is honest to expose
    // it here, because testing a reminder is the one thing a reminder feature needs most.
    virtual void showReminderNow() = 0;
};

}    // namespace musichien::infrastructure
