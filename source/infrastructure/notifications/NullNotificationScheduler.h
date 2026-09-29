#pragma once

#include "infrastructure/notifications/NotificationScheduler.h"

#include <span>

namespace musichien::infrastructure
{

// The honest "nothing" for a machine that has no notifications: a desktop, a test. Nothing is scheduled, and
// nothing crashes. The Android implementation is the real one, and it is chosen by the application at start up.
class NullNotificationScheduler final : public NotificationScheduler
{
public:
    void scheduleDailyNotifications( std::span<const DailyNotification> p_notifications ) override
    {
        (void)p_notifications;
    }

    void cancelNotifications() override
    {
    }

    void showReminderNow( std::string_view p_content ) override
    {
        (void)p_content;
    }

    void requestNotificationPermission() override
    {
    }
};

}    // namespace musichien::infrastructure
