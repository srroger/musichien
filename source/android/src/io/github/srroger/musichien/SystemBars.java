package io.github.srroger.musichien;

import android.app.Activity;
import android.os.Build;
import android.view.View;
import android.view.Window;
import android.view.WindowInsetsController;

// La barre systeme du haut, forcee depuis le CODE et non plus depuis le theme.
//
// Pourquoi ca existe : depuis Android 15 (API 35), `android:statusBarColor` est deprecie et IGNORE par le systeme.
// La barre est desormais transparente, et deux choses doivent etre dites a la main :
//   * la couleur des ICONES (claires sur un fond nuit) ;
//   * la couleur de la BARRE elle-meme, quand le systeme veut bien l'ecouter encore.
//
// Le XML ne peut plus le faire seul, donc on le fait ici. Aucun etat n'est garde : c'est idempotent, et une
// application qui revient au premier plan doit pouvoir le redire.
public class SystemBars {

    // Le bleu nuit du jeu, identique a la premiere couleur du degrade de l'accueil.
    private static final int NIGHT = 0xFF1B1035;

    public static void applyNightStyle(Activity activity) {
        if (activity == null) {
            return;
        }

        Window window = activity.getWindow();
        if (window == null) {
            return;
        }

        // Ignore sur Android 15 et plus, applique en dessous : ca ne coute rien d'essayer.
        window.setStatusBarColor(NIGHT);
        window.setNavigationBarColor(NIGHT);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            // API 30 et plus : l'API qui compte. On RETIRE les deux apparences "claires", donc les icones
            // redeviennent blanches, ce qu'un fond nuit demande.
            WindowInsetsController controller = window.getDecorView().getWindowInsetsController();
            if (controller != null) {
                controller.setSystemBarsAppearance(
                    0,
                    WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS
                        | WindowInsetsController.APPEARANCE_LIGHT_NAVIGATION_BARS
                );
            }
        } else {
            // API 28 a 29 : l'ancienne API, toujours disponible et suffisante ici.
            window.getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_VISIBLE);
        }
    }
}
