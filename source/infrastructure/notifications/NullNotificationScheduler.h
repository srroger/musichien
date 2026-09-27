#pragma once

#include "infrastructure/notifications/NotificationScheduler.h"

namespace musichien::infrastructure
{

// The honest "nothing" for a machine that has no notifications: a desktop, a test. Nothing is scheduled, and
// nothing crashes. The Android implementation is the real one, and it is chosen by the application at start up.
class NullNotificationScheduler final : public NotificationScheduler
{
public:
    void scheduleDailyReminder( int p_hour, int p_minute ) override
    {
    }

    void cancelReminder() override
    {
    }

    void showReminderNow() override
    {
    }
};

}    // namespace musichien::infrastructure
