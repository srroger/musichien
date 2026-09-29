#pragma once

#include "infrastructure/notifications/NotificationScheduler.h"

#include <span>

namespace musichien::infrastructure
{

// La vraie implementation Android : elle parle a AlarmManager par JNI, a travers les classes Java
// ReminderScheduler et ReminderReceiver qui vivent dans source/android/src. Compilee seulement sur Android.
class AndroidNotificationScheduler final : public NotificationScheduler
{
public:
    void scheduleDailyNotifications( std::span<const DailyNotification> p_notifications ) override;

    void cancelNotifications() override;

    void showReminderNow( std::string_view p_content ) override;

    void requestNotificationPermission() override;
};

}    // namespace musichien::infrastructure
