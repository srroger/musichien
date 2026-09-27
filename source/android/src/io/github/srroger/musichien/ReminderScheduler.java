package io.github.srroger.musichien;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;

import java.util.Calendar;

// Planifie le rappel quotidien. Des methodes STATIQUES, appelees depuis le C++ par JNI : tout ce qui touche a
// AlarmManager vit ici, et le reste de l'application n'a pas a le savoir.
public class ReminderScheduler {

    private static final int REQUEST_CODE = 1;

    public static void scheduleDaily(Context context, int hour, int minute, String content) {
        AlarmManager alarm = (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);
        if (alarm == null) {
            return;
        }

        Calendar calendar = Calendar.getInstance();
        calendar.set(Calendar.HOUR_OF_DAY, hour);
        calendar.set(Calendar.MINUTE, minute);
        calendar.set(Calendar.SECOND, 0);
        calendar.set(Calendar.MILLISECOND, 0);

        long triggerAt = calendar.getTimeInMillis();
        long now = System.currentTimeMillis();
        if (triggerAt <= now) {
            triggerAt += AlarmManager.INTERVAL_DAY;
        }

        // Inexact plutot qu'exact : le rappel arrive dans l'heure, et le systeme peut regrouper les reveils pour
        // menager la batterie. C'est le bon compromis pour un rappel qui ne doit pas etre a la seconde pres.
        alarm.setInexactRepeating(
            AlarmManager.RTC_WAKEUP,
            triggerAt,
            AlarmManager.INTERVAL_DAY,
            pendingIntent(context, content)
        );
    }

    public static void cancel(Context context) {
        AlarmManager alarm = (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);
        if (alarm == null) {
            return;
        }

        // Les extras ne comptent pas dans le matching d'un PendingIntent : un intent sans texte annule bien
        // celui qui en portait un.
        alarm.cancel(pendingIntent(context, null));
    }

    private static PendingIntent pendingIntent(Context context, String content) {
        Intent intent = new Intent(context, ReminderReceiver.class);
        if (content != null) {
            intent.putExtra("content", content);
        }
        return PendingIntent.getBroadcast(
            context,
            REQUEST_CODE,
            intent,
            PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE
        );
    }
}
