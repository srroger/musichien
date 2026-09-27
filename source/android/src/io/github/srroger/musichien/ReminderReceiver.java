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

    // La notification porte l'anecdote tiree au moment de la planification ; sans elle, on retombe sur le petit
    // rappel fixe. L'utilisateur reçoit donc un contenu different a chaque fois qu'il re-coche le rappel.
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

        String text = (content == null || content.isEmpty())
            ? "Une oreille, une minute : l'intervalle du jour t'attend."
            : content;

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

    // Sur Android 13 et plus, une notification ne s'affiche pas sans la permission POST_NOTIFICATIONS. Elle est
    // demandee ici, au moment ou l'on en a besoin - jamais au demarrage, jamais pour rien.
    private static void ensurePermission(Context context) {
        if (Build.VERSION.SDK_INT < 33) {
            return;
        }

        if (context.checkSelfPermission("android.permission.POST_NOTIFICATIONS")
                == android.content.pm.PackageManager.PERMISSION_GRANTED) {
            return;
        }

        if (context instanceof android.app.Activity) {
            ((android.app.Activity) context).requestPermissions(
                new String[] { "android.permission.POST_NOTIFICATIONS" }, 1);
        }
    }
}
