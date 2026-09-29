package io.github.srroger.musichien;

import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Build;

// Recut le rappel planifie et affiche la notification. Aucun etat ici : tout vient de l'intention qui a
// declenche le recepteur.
public class ReminderReceiver extends BroadcastReceiver {

    private static final String CHANNEL_ID = "musichien_reminder";

    @Override
    public void onReceive(Context context, Intent intent) {
        String content = intent.getStringExtra("content");
        showReminder(context, content);
    }

    // Affiche le rappel. Statique, pour que le planificateur puisse l'appeler aussi, s'il le faut.
    public static void showReminder(Context context) {
        showReminder(context, null);
    }

    // The notification draws one anecdote from the pool frozen into the alarm at scheduling time. One per line, so a
    // DIFFERENT anecdote can land on each daily firing, without the application running. Without a pool, it falls back
    // to the little fixed nudge.
    public static void showReminder(Context context, String content) {
        ensurePermission(context);

        NotificationManager manager = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        if (manager == null) {
            return;
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(
                CHANNEL_ID,
                "Rappel quotidien",
                NotificationManager.IMPORTANCE_DEFAULT
            );
            manager.createNotificationChannel(channel);
        }

        Intent launch = context.getPackageManager().getLaunchIntentForPackage(context.getPackageName());
        PendingIntent pending = PendingIntent.getActivity(
            context,
            0,
            launch,
            PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE
        );

        String text = pickMessage(content);

        android.app.Notification notification = new android.app.Notification.Builder(context, CHANNEL_ID)
            .setSmallIcon(android.R.drawable.ic_dialog_info)
            .setContentTitle("Musichien")
            .setContentText(text)
            .setStyle(new android.app.Notification.BigTextStyle().bigText(text))
            .setContentIntent(pending)
            .setAutoCancel(true)
            .build();

        manager.notify(1, notification);
    }

    // Draws ONE message from the pool. The pool is a set of lines; a single line (or nothing) is used as is.
    private static String pickMessage(String content) {
        if (content == null || content.isEmpty()) {
            return "Une oreille, une minute : l'intervalle du jour t'attend.";
        }

        String[] lines = content.split("\\n");
        if (lines.length <= 1) {
            return content;
        }

        return lines[new java.util.Random().nextInt(lines.length)];
    }

    // Sur Android 13 et plus, une notification ne s'affiche pas sans la permission POST_NOTIFICATIONS. La decision de
    // la demander vit dans ReminderScheduler, avec son pourquoi : ici on la laisse passer, parce que ce chemin est
    // celui du test manuel - et une alarme qui sonne n'a devant elle aucune activite a qui demander quoi que ce soit.
    private static void ensurePermission(Context context) {
        ReminderScheduler.requestNotificationPermission(context);
    }
}
