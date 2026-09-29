#include "infrastructure/notifications/AndroidNotificationScheduler.h"

#include <QJniObject>
#include <QString>
#include <QtCore/qcoreapplication_platform.h>

#include <cstddef>
#include <string>

namespace musichien::infrastructure
{

namespace
{

// L'objet que le Java attend pour atteindre AlarmManager et poser une question a l'utilisateur.
//
// C'est l'ACTIVITE, malgre le nom que Qt lui donne : QtNative.getContext() renvoie la reference a l'activite, pas le
// contexte applicatif. Ce detail compte, et pas qu'un peu : requestPermissions n'affiche une boite de dialogue que
// devant une activite, et un nom qui dit « applicationContext » ferait croire le contraire.
[[nodiscard]] QJniObject activityContext()
{
    return QNativeInterface::QAndroidApplication::context();
}

}    // namespace

void AndroidNotificationScheduler::scheduleDailyNotifications( std::span<const DailyNotification> p_notifications )
{
    // ON ANNULE TOUT D'ABORD, puis on reprogramme. Sans cela, une notification qui disparait de la liste - le rappel
    // que le joueur vient d'eteindre - continuerait a sonner : une alarme Android n'a aucune raison de savoir qu'on ne
    // l'a pas remise dans la liste.
    cancelNotifications();

    for( std::size_t slot = 0; slot < p_notifications.size(); ++slot )
    {
        const DailyNotification & notification = p_notifications[slot];

        const QJniObject content = QJniObject::fromString( QString::fromStdString( notification.content ) );

        // Le SLOT est le requestCode du PendingIntent cote Java : c'est lui qui permet a quatre notifications
        // quotidiennes de coexister, la ou un requestCode fixe en aurait fait une seule.
        QJniObject::callStaticMethod<void>( "io/github/srroger/musichien/ReminderScheduler",
                                            "scheduleDaily",
                                            "(Landroid/content/Context;IILjava/lang/String;I)V",
                                            activityContext().object(),
                                            static_cast<jint>( notification.hour ),
                                            static_cast<jint>( notification.minute ),
                                            content.object(),
                                            static_cast<jint>( slot ) );
    }
}

void AndroidNotificationScheduler::cancelNotifications()
{
    QJniObject::callStaticMethod<void>( "io/github/srroger/musichien/ReminderScheduler",
                                        "cancelAll",
                                        "(Landroid/content/Context;)V",
                                        activityContext().object() );
}

void AndroidNotificationScheduler::showReminderNow( std::string_view p_content )
{
    const QJniObject content = QJniObject::fromString( QString::fromStdString( std::string( p_content ) ) );

    QJniObject::callStaticMethod<void>( "io/github/srroger/musichien/ReminderReceiver",
                                        "showReminder",
                                        "(Landroid/content/Context;Ljava/lang/String;)V",
                                        activityContext().object(),
                                        content.object() );
}

void AndroidNotificationScheduler::requestNotificationPermission()
{
    // Le Java decide : il sait quelle version d'Android est en face, si l'autorisation manque vraiment, et si l'objet
    // recu est une activite. Repeter ces trois verifications ici serait la meilleure facon de les faire diverger.
    QJniObject::callStaticMethod<void>( "io/github/srroger/musichien/ReminderScheduler",
                                        "requestNotificationPermission",
                                        "(Landroid/content/Context;)V",
                                        activityContext().object() );
}

}    // namespace musichien::infrastructure
