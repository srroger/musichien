#pragma once

#include "infrastructure/notifications/NotificationScheduler.h"

namespace musichien::infrastructure
{

// La vraie implementation Android : elle parle a AlarmManager par JNI, a travers les classes Java
// ReminderScheduler et ReminderReceiver qui vivent dans source/android/src. Compilee seulement sur Android.
class AndroidNotificationScheduler final : public NotificationScheduler
{
public:
    void scheduleDailyReminder( int p_hour, int p_minute, std::string_view p_content ) override;

    void cancelReminder() override;

    void showReminderNow() override;
};

}    // namespace musichien::infrastructure
