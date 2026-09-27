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

    // A reminder every day at this hour and minute. Scheduling twice replaces the previous one.
    virtual void scheduleDailyReminder( int p_hour, int p_minute ) = 0;

    // No more reminders.
    virtual void cancelReminder() = 0;
};

}    // namespace musichien::infrastructure
