#include "infrastructure/notifications/AndroidNotificationScheduler.h"

#include <QJniObject>
#include <QString>
#include <QtCore/qcoreapplication_platform.h>

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

void AndroidNotificationScheduler::scheduleDailyReminder( int p_hour, int p_minute, std::string_view p_content )
{
    const QJniObject content = QJniObject::fromString( QString::fromStdString( std::string( p_content ) ) );

    QJniObject::callStaticMethod<void>( "io/github/srroger/musichien/ReminderScheduler",
                                        "scheduleDaily",
                                        "(Landroid/content/Context;IILjava/lang/String;)V",
                                        applicationContext().object(),
                                        static_cast<jint>( p_hour ),
                                        static_cast<jint>( p_minute ),
                                        content.object() );
}

void AndroidNotificationScheduler::cancelReminder()
{
    QJniObject::callStaticMethod<void>( "io/github/srroger/musichien/ReminderScheduler",
                                        "cancel",
                                        "(Landroid/content/Context;)V",
                                        applicationContext().object() );
}

void AndroidNotificationScheduler::showReminderNow()
{
    QJniObject::callStaticMethod<void>( "io/github/srroger/musichien/ReminderReceiver",
                                        "showReminder",
                                        "(Landroid/content/Context;)V",
                                        applicationContext().object() );
}

}    // namespace musichien::infrastructure
