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

```bash
sudo pacman -S clang clang-tools cmake ninja git jdk21-openjdk python-pipx
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

| 4 | Construit **GoogleTest 1.18.0** et **nlohmann/json 3.12.0** via le superbuild |

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

## 6. Android *(en préparation)*

### ⚠️ Découverte : le dépôt Android de Qt a **déménagé**

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

### Étapes restantes

```bash
# 1. Outillage hôte + Qt pour Android (android-tools, android-udev, jdk21-openjdk)
scripts/install_dependencies.sh --with-android

# 2. L'émulateur a besoin du groupe kvm, puis d'une reconnexion de session
sudo gpasswd -a "$USER" kvm

# 3. Android SDK (cmdline-tools) + le NDK exigé par Qt : la version exacte se lit DANS
#    l'installation Qt pour Android, une fois l'étape 1 faite.

# 4. Sur le téléphone : Paramètres → À propos → Numéro de build (7 tapes)
#                      Options développeur → Débogage USB
#    Puis accepter la fenêtre d'autorisation qui s'affiche.
adb devices                                  # doit lister l'appareil
```

> [!warning] À vérifier impérativement avant toute distribution
> **L'APK ne doit contenir aucune permission `INTERNET`.** C'est la promesse centrale du projet
> (voir la charte). Le manifeste généré par Qt doit donc être **relu** à ce moment-là, et non supposé
> correct.


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

