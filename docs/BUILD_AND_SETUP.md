# 🔧 Musichien — Installation et build

> Tout ce qu'il faut installer, et comment garantir que le build ne dépend **pas** des versions
> installées sur la machine.

---

## 1. Prérequis système

Machine de référence : **Manjaro Linux (Arch), x86_64**, testée avec 4 cœurs / 15 GiB.

| Paquet | Rôle | Pourquoi cette version |
|---|---|---|
| `clang` | Compilateur de référence | ≥ 17 requis. Testé avec **22.1.8** |
| `clang-tools` | `clang-format`, `clang-tidy`, `clangd` | Impose les règles de nommage et la mise en forme |
| `cmake` | Système de build | ≥ 3.26. Testé avec **4.4.2** |
| `ninja` | Générateur | Rapide, utilisé par tous les presets |
| `git` | Versionnage | — |
| `jdk21-openjdk` | **JDK 21** | La chaîne Android de Qt exige **exactement** JDK 21 |
| `python-pipx` | Installe `aqtinstall` de façon isolée | Évite de polluer le Python système |
| `android-tools` | `adb`, `fastboot` *(Android seulement)* | Utile mais **non indispensable** : le SDK fournit son propre `adb`. Voir la découverte n°3 |
| `android-udev` | `adb` voit le téléphone sans `sudo` *(Android seulement)* | — |

```bash
sudo pacman -S clang clang-tools cmake ninja git jdk21-openjdk python-pipx

# Pour Android, en plus
sudo pacman -S android-tools android-udev
```

> ⚠️ **JDK 26 est à éviter.** Il est installé par défaut sur la machine, mais il est **en avance** sur
> ce que supportent Gradle et AGP. Utiliser JDK 21 pour tout ce qui touche à Android.

---

## 2. Dépendances épinglées

Elles **ne viennent pas** des dépôts système : elles vivent dans `MUSICHIEN_EXTERNAL_DIR`
(`../Roger-externals` par défaut). **C'est ce qui rend le build reproductible.**

```bash
# Installation complète (Qt + outillage + dépendances sources)
scripts/install_dependencies.sh --with-superbuild

# Avec le support Android en plus
scripts/install_dependencies.sh --with-android --with-superbuild
```

### Ce que fait le script, étape par étape

| Étape | Action |
|---|---|
| 1 | Installe les paquets système manquants (`pacman`) |
| 2 | Installe **`aqtinstall`** via `pipx` |
| 3 | **Découvre** la dernière version Qt disponible sur le miroir, puis l'installe **avec les modules du projet** dans `Roger-externals/Qt/` |

| 4 | *(avec `--with-android`)* Installe le **SDK Android** et le **NDK r27c** dans `Roger-externals/android-sdk` |
| 5 | *(avec `--with-superbuild`)* Construit **GoogleTest 1.18.0** et **nlohmann/json 3.12.0** via le superbuild |

### Pourquoi `aqtinstall` et pas l'installeur officiel ?

L'installeur officiel de Qt **exige la création d'un compte Qt**. `aqtinstall` (MIT, non officiel)
télécharge **les mêmes binaires officiels** depuis le miroir Qt, **sans compte et sans étape
interactive**. C'est donc le bon outil pour une installation scriptable et reproductible.

### Pourquoi Qt n'est-il pas compilé depuis les sources ?

C'est un **choix assumé** :

| Construire Qt depuis les sources | Utiliser les binaires officiels |
|---|---|
| Des heures de compilation | Quelques minutes de téléchargement |
| À refaire pour chaque cible (desktop, Android…) | Les deux cibles disponibles d'un coup |
| Risque d'échec et de configuration | Officiels, identiques sur toutes les machines |
| Utile seulement si on modifie Qt | **C'est un outil, pas notre code** |

Le superbuild du projet sert donc uniquement à ce que **nous** compilons : GoogleTest et nlohmann/json.

### ⚠️ Les modules Qt sont **séparés**

`aqt` n'installe par défaut que **qtbase** et **qtdeclarative**. Tout le reste est un **module
distinct**, et un module manquant fait échouer `find_package(Qt6 COMPONENTS ...)` avec un message
qui n'indique **pas** quoi faire :

```
Failed to find required Qt component "Multimedia".
```

| Module | Pourquoi Musichien en a besoin |
|---|---|
| **qtmultimedia** | `QAudioSink` / `QAudioSource` : **toute la sortie audio** du projet |
| **qtshadertools** | Compilation des effets de shader QML à l'exécution |
| **qt5compat** | `Qt5Compat.GraphicalEffects` : flou, halo, ombre portée — le « juice » du jeu |

Le script les installe automatiquement, et la liste est une simple variable (`QT_MODULES`) en tête du
script : **ajouter un module plus tard se fait en une ligne**.

### ⚠️ Pourquoi Qt 6.11 est un **minimum absolu**


**Qt 6.10 et antérieurs ne compilent pas en C++26** avec une bibliothèque standard récente. Le projet
refuse désormais ces versions au moment de la configuration, avec un message explicite.

Le détail technique, parce qu'il vaut la peine d'être connu :

| | Qt 6.10.1 | Qt 6.11.1 |
|---|---|---|
| `q26numeric.h` contient | `using std::saturate_cast;` | `using std::saturating_cast;` |
| Nom du trait | celui du **brouillon** C++26 | celui de la **norme finale** |
| Résultat avec GCC 16 | ❌ erreur de compilation | ✅ compile |

Le nom a été changé **tardivement** dans le processus de normalisation de C++26 : `saturate_cast` est
devenu `saturating_cast`. Qt 6.10 a été écrit avant ce renommage. Comme `__cpp_lib_saturation_arithmetic`
est bien défini par GCC 16, Qt 6.10 prend la branche `using std::saturate_cast;` — qui n'existe plus.

**Le symptôme est trompeur** : l'erreur ne parle pas de Qt mais de `std::saturate_cast`, et elle est
levée depuis un en-tête Qt, très loin de la vraie cause.

> [!tip] Comment Qt 6.11 a corrigé le problème
> ```cpp
> #if defined(__cpp_lib_saturation_arithmetic) && __cpp_lib_saturation_arithmetic >= 202603L
> using std::saturating_cast;
> ```
> Ils vérifient la **version** du macro et utilisent le **nom final**. C'est la bonne façon de faire.


---

## 3. Environnement de développement

```bash
source scripts/setup_env.sh
```

**Obligatoire dans chaque nouveau terminal.** Ce script exporte :

| Variable | Rôle |
|---|---|
| `MUSICHIEN_PROJECT_DIR` | Racine du projet |
| `MUSICHIEN_EXTERNAL_DIR` | Dossier des dépendances épinglées |
| `MUSICHIEN_QT_VERSION` | La version de Qt effectivement utilisée |
| `MUSICHIEN_QT_DIR` | **Le** Qt utilisé (détecté automatiquement) |
| `CLANG_DIR` | Dossier du compilateur, lu par `CMakePresets.json` |
| `PATH` | **CMake et Ninja épinglés**, s'ils existent dans `MUSICHIEN_EXTERNAL_DIR` |

### Comment Qt est détecté

**Deux dispositions sont supportées**, car les deux sont légitimes :

| Disposition | Origine | Chemin |
|---|---|---|
| **aqt** | `scripts/install_dependencies.sh` | `<externals>/Qt/<version>/gcc_64` |
| **à plat** | les autres projets personnels | `<externals>/Qt-<version>` |

Le script retient **la version la plus récente** qui contient réellement un
`lib/cmake/Qt6/Qt6Config.cmake`.

> [!warning] Le repli `/usr` est signalé, et il n'est pas collant
> Si aucun Qt épinglé n'est trouvé, le script affiche un **avertissement** et retombe sur le Qt de la
> machine. Ce repli est **volontairement bruyant** : c'est le signal que la reproductibilité n'est
> plus garantie.
> Il est aussi **non collant** : un nouveau `source scripts/setup_env.sh` **redétecte** le Qt épinglé
> dès qu'il est installé, même si la session précédente avait exporté `/usr`.

---

## 4. Compiler


```bash
source scripts/setup_env.sh

cmake --preset "Clang-Debug Musichien"
cmake --build --preset "Build Clang-Debug Musichien"
ctest --preset "CTest Clang-Debug Musichien"
../Musichien-build/Clang-Debug/bin/musichien
```

### Les presets disponibles

| Preset | Usage |
|---|---|
| `Clang-Debug Musichien` | Développement : débogage, `clang-tidy` actif |
| `Clang-RelWithDebInfo Musichien` | **Le défaut** : rapide, avec les informations de débogage |
| `Clang-Release Musichien` | Pour mesurer les performances réelles |
| `Superbuild Musichien` | Construit **uniquement** les dépendances sources |

Pour itérer plus vite, désactiver temporairement l'analyse statique :

```bash
cmake --preset "Clang-Debug Musichien" -DMUSICHIEN_ENABLE_CLANG_TIDY=OFF
```

---

## 5. Ouvrir l'IDE

```bash
scripts/start_code_oss.sh      # code-oss (VS Code libre), environnement garanti
scripts/start_qtcreator.sh     # Qt Creator, environnement garanti
```

> ⚠️ **Ne jamais ouvrir le projet depuis le menu applications.** L'IDE n'aurait pas les variables
> d'environnement : les presets seraient illisibles, et le build prendrait silencieusement le Qt de la
> machine — exactement ce que ce projet cherche à éviter.

`.vscode/` contient la configuration partagée de code-oss : `settings.json` (presets CMake, formatage
automatique, clangd), `tasks.json` (build, test, formatage, vérification), `launch.json` (débogage
avec CodeLLDB) et `extensions.json` (extensions recommandées).

> [!note] code-oss et les extensions
> code-oss installe ses extensions depuis le registre **Open VSX**, pas depuis le marketplace
> Microsoft. Les extensions recommandées sont toutes publiées sur Open VSX. L'extension C++ de
> Microsoft (`ms-vscode.cpptools`) est explicitement **déconseillée** : elle entrerait en conflit avec
> `clangd`, qui est la référence du projet.

---

## 6. Android

### ⚠️ Découverte n°1 : le dépôt Android de Qt a **déménagé**

C'est le piège le plus coûteux rencontré jusqu'ici. Depuis **Qt 6.8**, les binaires Android de Qt ne
sont **plus** dans `linux_x64/android` :

| Dépôt | Dernière mise à jour | Dernière version Qt |
|---|---|---|
| `linux_x64/android/` (l'ancien) | **24 sept. 2024** — **gelé** | **6.7.3** |
| **`all_os/android/`** (le nouveau) | **24 sept. 2026** — vivant | **6.12.0** |

**Conséquence si on se trompe** : `aqt list-qt linux android` ne montre **rien au-delà de 6.7.3**, et
on peut légitimement — mais faussement — conclure que **Qt a abandonné le support d'Android**.
La commande correcte utilise donc `all_os` :

```bash
scripts/install_dependencies.sh --with-android
```

Le script porte le commentaire explicatif, pour que personne ne « corrige » un jour cette ligne en
repassant à `linux`.

> [!tip] Comment le vérifier sans rien télécharger
> ```bash
> aqt list-qt all_os android --arch 6.12.0     # -> android_armv7 android_x86 android_x86_64 android_arm64_v8a
> aqt install-qt all_os android 6.12.0 android_arm64_v8a -m qtmultimedia --dry-run -O /tmp/test
> ```
> `--dry-run` affiche ce qui serait téléchargé, sans rien écrire sur le disque.

### Mise en place

Tout est automatisé, **y compris le SDK et le NDK**, qui ne sont plus à installer à la main :

```bash
scripts/install_dependencies.sh --with-android --with-superbuild
source scripts/setup_env.sh
```

Le SDK Android atterrit dans `Roger-externals/android-sdk`, **jamais** dans le système, avec exactement
les versions exigées par Qt :

| Composant | Version | Pourquoi précisément celle-là |
|---|---|---|
| **NDK** | **r27c** (`27.2.12479018`) | C'est **la révision avec laquelle Qt 6.12 a été compilé**. Elle se lit dans `android_arm64_v8a/lib/cmake/Qt6/qt.toolchain.cmake` ; elle ne se devine pas. |
| **Plateforme** | `android-36` | Qt prend la plateforme installée la plus récente comme `compileSdk`. **API 37 n'existe pas** en canal stable : `sdkmanager` répond `Failed to find package`. |
| **Build tools** | `36.0.0` | Minimum exigé par AGP 9.2.1, la version qu'embarque le template Gradle de Qt 6.12. |

### Construire et installer

```bash
scripts/build_android.sh               # compile, vérifie, signe, puis installe sur le téléphone
scripts/build_android.sh --no-install  # sans toucher au téléphone
```

Le script enchaîne configuration, compilation (cible `apk` fournie par Qt), récupération du paquet,
**vérification des permissions**, signature, puis installation. La vérification est la raison d'être du
script : voir la découverte n°2.

### Sur le téléphone

```bash
# Paramètres → À propos → Numéro de build (7 tapes)
# Options développeur → Débogage USB
adb devices
```

### 🔴 Découverte n°2 : Qt réclame `INTERNET` à notre place

C'est **le piège central du projet**, et il ne se voit qu'en relisant l'APK terminée.

Le seul fait de lier `Qt6::Core` fait hériter de permissions :

```cmake
# android_arm64_v8a/lib/cmake/Qt6Core/Qt6CoreTargets.cmake
INTERFACE_QT_ANDROID_PERMISSIONS "…WRITE_EXTERNAL_STORAGE;…INTERNET"
```

Et il existe **deux canaux indépendants**, ce qui rend le problème plus retors qu'il n'y paraît :

| Canal | Mécanisme | Fermé par |
|---|---|---|
| **1** | La propriété CMake `QT_ANDROID_PERMISSIONS`, qui finit dans le JSON de déploiement | `cmake/musichienAndroid.cmake` la vide sur chaque module Qt |
| **2** | Les fichiers `<module>-android-dependencies.xml` livrés avec Qt, que **`androiddeployqt` lit de lui-même**, sans rien demander à CMake | `source/android/AndroidManifest.xml`, **sans le marqueur `INSERT_PERMISSIONS`** |

> [!important] Fermer le canal 1 ne suffit pas
> Un premier build pourtant « propre » — JSON de déploiement contenant **zéro permission** — produisait
> quand même un APK demandant `INTERNET`, `CAMERA`, `RECORD_AUDIO`, `BLUETOOTH` et
> `ACCESS_NETWORK_STATE`. `androiddeployqt` les prélevait dans les XML des modules Qt (Qt6Core,
> Qt6Network, Qt6Multimedia) et les insérait à son marqueur `INSERT_PERMISSIONS`. En retirant ce
> marqueur du manifeste, il ne lui reste plus où les écrire.

Résultat relu sur l'APK finale :

```
permissions declared by the package: none at all
```

> [!note] La seule permission restante est inoffensive
> `io.github.srroger.musichien.DYNAMIC_RECEIVER_NOT_EXPORTED_PERMISSION` est **déclarée par
> l'application elle-même** (via `androidx.core`), avec un `protectionLevel` **signature**. Elle ne
> peut être détenue que par une application signée de la même clé, n'accorde **aucune capacité
> système** et n'apparaît pas dans les réglages. Elle est inévitable, et sans rapport avec le réseau.

### Pourquoi c'est une garantie et non une promesse

Sur Android, une application qui ne déclare pas `android.permission.INTERNET` n'est **pas placée dans le
groupe `inet`** : ses appels `socket()` échouent. C'est le **noyau** qui applique la règle, pas
Musichien. Retirer la permission, c'est retirer la capacité — et non cacher un bouton.

### Signature

Un APK **release est non signé**, donc **non installable**. `scripts/build_android.sh` le signe avec une
**clé personnelle**, créée au premier lancement et conservée **hors du dépôt** :

```
~/.android/musichien-release.keystore
~/.android/musichien-release.password     (chmod 600)
```

**Sauvegarder ces deux fichiers.** Android refuse d'installer une mise à jour dont la signature diffère
de celle déjà en place : perdre la clé oblige à désinstaller l'application, donc à perdre la
progression. Sous GPL, chacun reste libre de recompiler Musichien et de le signer de sa propre clé.

### ⚠️ Découverte n°3 : `adb` du système peut être cassé sans rapport avec le projet

`android-tools` (le paquet qui fournit `/usr/bin/adb`) était inutilisable :

```
adb: error while loading shared libraries: libprotobuf.so.36.0.0: cannot open shared object file
```

**Cause** : sur une distribution *rolling*, les dépendances se vérifient **par nom de paquet**, pas par
version de bibliothèque. `protobuf` étant installé (en 35.1), `pacman` a jugé la dépendance satisfaite —
alors que le binaire d'`adb` avait été compilé contre la **36**.

**Deux conséquences pour le projet :**

1. `scripts/setup_env.sh` place **le `platform-tools` du SDK en tête du `PATH`**. L'`adb` du SDK
   n'a aucune dépendance système et fonctionne toujours, quel que soit l'état des paquets.
2. `scripts/install_dependencies.sh` **avertit** désormais quand des dizaines de paquets attendent une
   mise à jour : installer un paquet isolé sur un système en retard est exactement ce qui produit ce
   genre de casse. Le remède est un upgrade complet (`sudo pacman -Syu`), jamais une réinstallation
   ciblée.

### ⚠️ Découverte n°4 : le paquet s'installe, démarre, puis se ferme aussitôt

C'est le symptôme le plus déroutant du projet : **l'installation réussit, l'icône apparaît, et l'écran se
ferme immédiatement** — sans message sur le téléphone, et sans que rien n'ait été signalé à la
compilation.

La cause se lit dans le journal de l'application :

```
QQmlApplicationEngine failed to load component
qrc:/qml/Main.qml:16:1: module "QtQuick.Controls" plugin
"qml_QtQuick_Controls_qtquickcontrols2plugin" not found
```

Qt **embarque** dans le paquet les plugins QML que l'application importe réellement, et il décide
lesquels en lançant `qmlimportscanner` sur les dossiers déclarés dans les settings de déploiement. Ces
dossiers proviennent de la propriété **`QT_QML_ROOT_PATH`**. Quand elle n'est pas définie, Qt retombe sur
le **dossier source de la cible** — ici `source/application`, qui **ne contient aucun QML** : les fichiers
vivent dans le module `ui`. Le scanner ne trouve donc rien, n'embarque aucun plugin, et l'application
meurt au démarrage.

**Rien ne le signale à la compilation, et le desktop n'est pas touché**, puisque les plugins y viennent de
l'installation Qt de la machine. Seul le téléphone révèle le problème.

La correction tient en une ligne, dans `source/application/CMakeLists.txt` : pointer `QT_QML_ROOT_PATH`
sur le module qui possède les QML.

> [!tip] Lire le journal de l'application sur le téléphone
> ```bash
> adb logcat -c
> adb shell am start -n io.github.srroger.musichien/org.qtproject.qt.android.bindings.QtActivity
>
> # Tout le journal est très bavard : se limiter à NOTRE application
> adb logcat -d --pid=$(adb shell pidof io.github.srroger.musichien)
>
> # Ou chercher large
> adb logcat -d | grep -iE "musichien|QML|default :"
> ```
> `--pid` avec `pidof` est de loin le plus lisible. C'est aussi ce qui a permis de constater que le son
> sort bien : après un appui, le journal montre l'ouverture du flux `AAudio` (48 000 Hz, stéréo,
> `format = 0x5` soit **Float 32 bits**).

> [!warning] Les permissions sont une **décision**, pas un détail
> Seules **deux** permissions sont acceptées, et chacune doit se justifier par écrit dans
> `ALLOWED_SYSTEM_PERMISSIONS` (`scripts/build_android.sh`) :
>
> | Permission | Pourquoi elle est acceptée |
> |---|---|
> | `POST_NOTIFICATIONS` | Afficher le rappel quotidien. N'accorde **rien** : ni réseau, ni fichier, ni micro, ni position. Se refuse sans casser l'application |
> | `RECORD_AUDIO` | Le **pilier chant**. Chanter un intervalle est la moitié de l'entraînement de l'oreille |
>
> **Toute autre permission fait échouer le build.** Et `android.permission.INTERNET` est refusée
> **en dur**, séparément de la liste : même en l'y ajoutant par erreur, elle serait rejetée. C'est la
> promesse centrale du projet, et elle est **contrôlée**, pas espérée — deux fois, sur le paquet
> compilé puis sur le paquet **signé**, celui qui sera réellement installé.

Le jour où le rappel ou le micro arrive, il suffit d'écrire la permission dans
`source/android/AndroidManifest.xml` :

```xml
<uses-permission android:name="android.permission.POST_NOTIFICATIONS" />
```

Vérifié le 2026-09-26 en déclarant les deux temporairement : une permission écrite **là** arrive bien
dans l'APK, alors que celles des modules Qt (`CAMERA`, `BLUETOOTH`, `ACCESS_NETWORK_STATE`…)
**n'y arrivent pas**, faute de marqueur `INSERT_PERMISSIONS`. Notre manifeste est donc la seule
porte, et le contrôle du script la garde étroite.


---

## 7. Dépannage

| Symptôme | Cause probable | Solution |
|---|---|---|
| `MUSICHIEN_QT_DIR is not defined` | Script d'environnement non sourcé | `source scripts/setup_env.sh` |
| `Pinned GoogleTest not found` | Superbuild non lancé | `scripts/install_dependencies.sh --with-superbuild` |
| Le QML ne se charge pas | Mauvais préfixe dans `resources.qrc` | Préfixe `/` avec `<file>qml/Main.qml</file>` |
| `could not find Qt6` | Chemin Qt incorrect | Vérifier `${MUSICHIEN_QT_DIR}/lib/cmake/Qt6/Qt6Config.cmake` |
| `aqt` introuvable | `pipx ensurepath` pas encore pris en compte | Ouvrir un nouveau terminal |
| `adb devices` ne voit rien | `android-udev` manquant | `sudo pacman -S android-udev`, puis rebrancher |
| Erreurs Gradle obscures | JDK 26 utilisé | Utiliser **JDK 21** |
| `-Wlogical-op` inconnu | L'option est propre à GCC | Déjà conditionnée par compilateur dans le CMake |
| `Failed to find package 'platforms;android-37'` | API 37 n'existe pas encore en canal stable | Utiliser `android-36` : c'est ce que fait le script |
| `Expected '>', but got '-'` dans `AndroidManifest.xml` | Un commentaire **XML** contient `--`, ce qui est interdit en XML | Retirer toute séquence `--` des commentaires |
| L'APK demande `INTERNET` | `androiddeployqt` a retrouvé son marqueur `INSERT_PERMISSIONS` | Vérifier `source/android/AndroidManifest.xml` — découverte n°2 |
| `Failed to read Key … password … end of file reached` | `--key-pass file:` fait relire le fichier de mot de passe par `apksigner` | Ne pas passer `--key-pass` : sur un magasin PKCS12, le mot de passe de clé est celui du magasin |
| `ninja: no work to do` sur la cible `apk` | `add_executable` au lieu de `qt_add_executable` | Utiliser `qt_add_executable`, sinon Qt ne crée pas les cibles de déploiement |
| **L'app s'installe, démarre, puis se ferme aussitôt** | Les plugins QML ne sont pas embarqués : `qmlimportscanner` n'a trouvé aucun QML | `QT_QML_ROOT_PATH` doit pointer sur le module `ui` — découverte n°4. Symptôme exact dans le journal : `module "QtQuick.Controls" plugin … not found` |
| `no member named 'adjacent' in 'std::ranges::views'` | libc++ **18** (NDK r27c) est plus ancienne que libstdc++ 16 | Éviter les fonctionnalités de bibliothèque absentes de libc++ 18 : `views::adjacent`, `span::at`, `views::zip`… |
| `no member named 'at' in 'std::span<float>'` | `std::span::at` est **C++26**, absent de libc++ 18 | Passer par un petit helper local vérifié, comme dans `ToneSynthesizer` |
| `adb` introuvable ou cassé | `platform-tools` du SDK absent du `PATH` | `source scripts/setup_env.sh` : il le place en tête |

