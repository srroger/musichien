package io.github.srroger.musichien;

import android.Manifest;
import android.app.Activity;
import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;

import java.util.Calendar;

// Planifie les notifications quotidiennes. Des methodes STATIQUES, appelees depuis le C++ par JNI : tout ce qui touche
// a AlarmManager vit ici, et le reste de l'application n'a pas a le savoir.
//
// PLUSIEURS notifications par jour, une par SLOT : le slot est un numero de creneau, et il sert de requestCode - c'est
// ce qui permet a quatre notifications quotidiennes de coexister sans s'ecraser l'une l'autre. Un PendingIntent est
// identifie par son requestCode et son action : deux notifications qui partageraient le meme slot seraient la meme
// alarme, et la seconde remplacerait la premiere.
public class ReminderScheduler {

    // Les creneaux disponibles : trois anecdotes et le rappel du joueur, avec de la marge.
    public static final int MAX_SLOTS = 8;

    private static final int BASE_REQUEST_CODE = 100;

    // Le code de la demande d'autorisation. Peu importe sa valeur : Android le rend tel quel a l'activite, qui n'en
    // fait rien - mais il doit exister, et rester stable.
    private static final int NOTIFICATION_PERMISSION_REQUEST_CODE = 1;

    public static void scheduleDaily(Context context, int hour, int minute, String content, int slot) {
        AlarmManager alarm = (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);
        if (alarm == null || slot < 0 || slot >= MAX_SLOTS) {
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

        // Inexact plutot qu'exact : la notification arrive dans l'heure, et le systeme peut regrouper les reveils pour
        // menager la batterie. C'est le bon compromis pour un message qui ne doit pas etre a la seconde pres - et
        // c'est aussi ce qui evite de demander la permission des alarmes exactes sur les Android recents.
        alarm.setInexactRepeating(
            AlarmManager.RTC_WAKEUP,
            triggerAt,
            AlarmManager.INTERVAL_DAY,
            pendingIntent(context, content, slot)
        );
    }

    // Annule TOUS les creneaux, qu'ils soient occupes ou non. Une alarme qui ne se rappelle plus de rien est une
    // alarme qui ne se declenchera jamais : annuler large ne coute rien, alors qu'oublier un creneau ferait sonner une
    // notification que le joueur a eteinte.
    public static void cancelAll(Context context) {
        AlarmManager alarm = (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);
        if (alarm == null) {
            return;
        }

        for (int slot = 0; slot < MAX_SLOTS; ++slot) {
            // Les extras ne comptent pas dans le matching d'un PendingIntent : un intent sans texte annule bien celui
            // qui en portait un.
            alarm.cancel(pendingIntent(context, null, slot));
        }
    }

    // Demande l'autorisation d'AFFICHER des notifications, si elle manque.
    //
    // Depuis Android 13 (API 33), declarer POST_NOTIFICATIONS dans le manifeste ne suffit plus : il faut la demander a
    // l'utilisateur, et une notification postee sans elle disparait SANS ERREUR. C'est la pire des pannes, parce
    // qu'elle est silencieuse : les alarmes se declenchaient bien, et rien n'apparaissait nulle part.
    //
    // L'appel vient du DEMARRAGE de l'application, et surtout pas de l'instant ou la notification doit s'afficher : a
    // cet instant-la l'application est en arriere-plan ou eteinte, et Android n'affiche une demande de permission que
    // devant une ACTIVITE. Demander a une alarme qui sonne ne montre rien a personne - et c'est exactement ce que
    // faisait le recepteur avant que cette methode n'existe.
    //
    // Avant Android 13, l'autorisation est donnee a l'installation : il n'y a rien a demander, et rien a refuser.
    public static void requestNotificationPermission(Context context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            return;
        }

        if (context == null
                || context.checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS)
                   == PackageManager.PERMISSION_GRANTED) {
            return;
        }

        // Devant une activite seulement : ailleurs, Android ignore la demande en silence, ce qui donnerait
        // l'illusion d'avoir demande quelque chose.
        if (context instanceof Activity) {
            ((Activity) context).requestPermissions(
                new String[] { Manifest.permission.POST_NOTIFICATIONS },
                NOTIFICATION_PERMISSION_REQUEST_CODE
            );
        }
    }

    private static PendingIntent pendingIntent(Context context, String content, int slot) {
        Intent intent = new Intent(context, ReminderReceiver.class);
        if (content != null) {
            intent.putExtra("content", content);
        }
        return PendingIntent.getBroadcast(
            context,
            BASE_REQUEST_CODE + slot,
            intent,
            PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE
        );
    }
}

