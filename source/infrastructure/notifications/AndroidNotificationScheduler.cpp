#include "infrastructure/notifications/AndroidNotificationScheduler.h"

#include <QJniObject>
#include <QtCore/private/qandroidextras_p.h>

namespace musichien::infrastructure
{

namespace
{

// Le contexte de l'application, que le Java exige pour atteindre AlarmManager.
[[nodiscard]] QJniObject applicationContext()
{
    return QNativeInterface::QAndroidApplication::context();
}

}    // namespace

void AndroidNotificationScheduler::scheduleDailyReminder( int p_hour, int p_minute )
{
    QJniObject::callStaticMethod<void>( "io/github/srroger/musichien/ReminderScheduler",
                                        "scheduleDaily",
                                        "(Landroid/content/Context;II)V",
                                        applicationContext().object(),
                                        static_cast<jint>( p_hour ),
                                        static_cast<jint>( p_minute ) );
}

void AndroidNotificationScheduler::cancelReminder()
{
    QJniObject::callStaticMethod<void>( "io/github/srroger/musichien/ReminderScheduler",
                                        "cancel",
                                        "(Landroid/content/Context;)V",
                                        applicationContext().object() );
}

}    // namespace musichien::infrastructure
