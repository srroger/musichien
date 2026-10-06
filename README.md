# 🐶 Musichien

> **Une application mobile libre, hors-ligne et privée qui utilise les mécaniques de motivation des
> jeux de rôle et des jeux de combat pour donner envie de travailler son oreille musicale, quelques
> secondes par jour.**

Musichien est un projet **personnel**. Il n'est pas destiné à être monétisé, ne contient ni publicité,
ni traçage, ni compte utilisateur, et **ne peut techniquement pas communiquer sur le réseau**.

---

## 📜 La promesse

| Règle | Comment elle est garantie |
|---|---|
| **Hors-ligne** | L'application ne déclare **aucune permission `INTERNET`**, et le build **refuse** de produire un paquet qui en demanderait une. Sans cette déclaration, Android maintient le processus hors du groupe `inet` : ce n'est pas une promesse, c'est une impossibilité technique. |
| **Permissions maîtrisées** | Deux permissions seulement sont acceptées, et **chacune doit se justifier** : `POST_NOTIFICATIONS` (afficher le rappel) et `RECORD_AUDIO` (le pilier chant). Toute autre fait **échouer le build**. La liste vit dans `scripts/build_android.sh`, chaque entrée avec sa raison d'être. |
| **Aucun traçage** | Aucun SDK Google, Firebase, analytics, publicité ou achat intégré. Jamais. |
| **Données locales** | Tout vit dans le dossier privé de l'application, avec export/import JSON pour ne jamais être prisonnier. |
| **Libre** | Licence **GPL-3.0** : toute version dérivée doit rester libre. |
| **Simple et durable** | Pas de serveur, pas de backend, pas de coût. Une seule personne doit pouvoir maintenir ce projet dans cinq ans. |

---

## 🚀 Démarrage rapide

```bash
# 1. Installer les dépendances épinglées (Qt, outillage, GoogleTest)
scripts/install_dependencies.sh --with-superbuild

# 2. Charger l'environnement (obligatoire, dans chaque nouveau terminal)
source scripts/setup_env.sh

# 3. Configurer, compiler, tester, lancer
cmake --preset "Clang-Debug Musichien"
cmake --build --preset "Build Clang-Debug Musichien"
ctest --preset "CTest Clang-Debug Musichien"
../Musichien-build/Clang-Debug/bin/musichien

# Ou, plus simplement, ouvrir l'éditeur avec l'environnement garanti :
scripts/start_code_oss.sh

# 4. Pour le téléphone : compile, VÉRIFIE l'absence de permission réseau, signe, puis installe
scripts/install_dependencies.sh --with-android   # une seule fois
scripts/build_android.sh

# 5. Depuis un Mac : installer Qt (bureau + iOS), puis produire une IPA
scripts/install_dependencies.sh --with-ios --with-superbuild   # une seule fois
source scripts/setup_env.sh
scripts/build_ios.sh                             # archive Xcode + .ipa signée
```

Procédure complète, prérequis et dépannage : `docs/BUILD_AND_SETUP.md`.
Mise en place pas à pas sur un Mac (Xcode, Homebrew, Qt, IPA) : voir la section « macOS » ci-dessous.

---

## 🍎 macOS : mise en place pas à pas

Objectif : cloner le dépôt, installer les dépendances, compiler, puis produire une **IPA** pour
l'iPhone. Rien de spécifique au Mac du côté du code : seuls quelques outils changent.

### 1. Outils de base

```bash
# Xcode (App Store) : indispensable, il fournit clang, les SDK Apple et l'outillage iOS
xcode-select --install          # à lancer si Xcode n'est pas déjà installé

# Homebrew (https://brew.sh) s'il n'est pas déjà là
brew install llvm cmake ninja git pipx
```

- `llvm` apporte `clang`, `clang-format` et `clang-tidy` (le clang d'Apple ne les fournit pas).
  `scripts/setup_env.sh` met son dossier `bin` sur le `PATH` : le build est alors le même qu'ailleurs.
- `pipx` ne sert qu'à installer `aqtinstall` automatiquement. Facultatif si tu fournis Qt à la main (§3).

### 2. Cloner et créer le dossier des dépendances

```bash
git clone <url-du-depot> Musichien
cd Musichien

# Les dépendances épinglées vivent À CÔTÉ du projet, jamais dans le système.
mkdir -p ../Roger-externals
```

`../Roger-externals` est le chemin par défaut. Pour un autre emplacement :
`export MUSICHIEN_EXTERNAL_DIR=/chemin/vers/mes-externals` avant tout.

### 3. Installer Qt — deux méthodes

**Méthode A — automatique (recommandée).** Le script télécharge Qt depuis le miroir officiel, **sans
compte Qt**, et remplit `../Roger-externals/Qt/<version>/` :

```bash
scripts/install_dependencies.sh --with-ios --with-superbuild
```

Il installe deux variantes : `macos` (bureau) et `ios` (iPhone/iPad). Avec `--with-android` en plus, il
ajoute aussi la variante Android.

**Méthode B — Qt déjà installé à la main.** Si Qt vient de l'installeur officiel
(`~/Qt/<version>/macos`), copie-le dans la disposition attendue, puis saute l'étape Qt du script avec
`--without-qt` :

```bash
VERSION=6.12.0                 # ta version, ≥ 6.11
mkdir -p ../Roger-externals/Qt/${VERSION}
cp -R ~/Qt/${VERSION}/macos ../Roger-externals/Qt/${VERSION}/macos
cp -R ~/Qt/${VERSION}/ios   ../Roger-externals/Qt/${VERSION}/ios    # seulement pour iOS

scripts/install_dependencies.sh --without-qt --with-superbuild
```

> [!important] La disposition des dossiers compte
> `setup_env.sh` cherche `Qt/<version>/macos/lib/cmake/Qt6/Qt6Config.cmake` — le nom du dossier interne
> doit être exactement `macos` (ou `gcc_64`, ou `clang_64`), et `Qt/<version>/ios/…` pour iOS. Un dossier
> nommé autrement, ou posé directement à la racine de `Roger-externals`, n'est **pas** détecté.
>
> La version doit être **≥ 6.11** (voir « Pourquoi Qt 6.11 » dans `docs/BUILD_AND_SETUP.md`) et les
> modules **qtmultimedia**, **qtshadertools** et **qt5compat** doivent être installés. L'installeur Qt
> les propose à la sélection des composants — ne les oublie pas, sinon `find_package(Qt6 …)` échoue.

---

### 4. Charger l'environnement

```bash
source scripts/setup_env.sh
```

À refaire **dans chaque nouveau terminal**. Le script affiche ce qu'il a trouvé (`MUSICHIEN_QT_DIR`,
`MUSICHIEN_QT_IOS_DIR`, `CLANG_DIR`…). S'il prévient qu'aucun Qt épinglé n'est trouvé, **ne continue
pas** : reviens au §3.

### 5. Compiler et tester sur le Mac

```bash
cmake --preset "macOS-Release Musichien"
cmake --build --preset "Build macOS-Release Musichien"
```

L'application se trouve dans `../Musichien-build/macOS-Release/bin/musichien.app`.

### 6. Produire une IPA pour l'iPhone

```bash
export MUSICHIEN_IOS_TEAM_ID=ABCDE12345     # ton identifiant d'équipe Apple (une fois)
scripts/build_ios.sh
```

Le script configure (générateur Xcode), **archive** l'application, puis **exporte** une `.ipa` signée
dans `../Musichien-build/iOS-Release/ipa/`. Options : `--no-export` (s'arrêter à l'archive) et
`--simulator` (build de simulateur, sans signature). Détails : `docs/BUILD_AND_SETUP.md` §6.

### Récapitulatif

| Étape | Commande |
|---|---|
| Outils | `xcode-select --install` puis `brew install llvm cmake ninja git pipx` |
| Dépendances | `scripts/install_dependencies.sh --with-ios --with-superbuild` |
| Qt déjà installé | copier `~/Qt/<v>/{macos,ios}` → `../Roger-externals/Qt/<v>/`, puis ajouter `--without-qt` |
| Environnement | `source scripts/setup_env.sh` |
| Bureau | `cmake --preset "macOS-Release Musichien"` |
| iPhone | `export MUSICHIEN_IOS_TEAM_ID=…` puis `scripts/build_ios.sh` |

---

## 🧱 Dépendances

### Outils pris sur la machine (génériques)

Le même jeu de scripts tourne sur **Linux (Arch)** et sur **macOS** : `scripts/platform.sh` détecte
l'hôte, et `scripts/install_dependencies.sh` utilise `pacman` ou **Homebrew** en conséquence. Sur un Mac,
**Xcode 16+** est requis pour iOS (le bureau ne l'exige pas).

| Outil | Version minimum | Rôle |
|---|---|---|
| **Clang** | 17 | Compilateur de référence du projet |
| **Clang tools** | 17 | `clang-format`, `clang-tidy`, `clangd` |
| **CMake** | 3.26 | Système de build (testé avec 4.4) |
| **Ninja** | 1.11 | Générateur de build |
| **Git** | 2.40 | Versionnage |
| **JDK** | **21** | Requis par la chaîne Android de Qt. **Pas 26** : trop récent, AGP casse. |

### Dépendances **épinglées** (jamais prises sur la machine)

Elles vivent dans `MUSICHIEN_EXTERNAL_DIR` (par défaut `../Roger-externals`) et sont installées par
`scripts/install_dependencies.sh`. **Le build ne dépend donc pas des versions installées sur le PC.**

| Dépendance | Version | Installée par | Rôle |
|---|---|---|---|
| **Qt** | **≥ 6.11 obligatoire** | `aqtinstall` (sans compte Qt) | Framework applicatif, interface QML |
| **Android SDK + NDK** | API **36**, NDK **r27c** | `scripts/install_dependencies.sh --with-android` | Compilation pour le téléphone. Le NDK est **celui avec lequel Qt a été compilé** |

| **GoogleTest / GoogleMock** | **1.18.0** | superbuild (depuis les sources) | Tests unitaires |
| **nlohmann/json** | **3.12.0** | superbuild (en-têtes seuls) | Fichier de sauvegarde et fichiers de contenu |

Les versions épinglées sont déclarées à **un seul endroit** : `cmake/musichienVersions.cmake`.

> **Pourquoi Qt n'est pas compilé depuis les sources ?** Les binaires officiels Qt sont reproductibles
> et identiques sur toutes les machines, alors qu'une compilation de Qt prend des heures et devrait
> être refaite pour chaque cible. Choix assumé, argumenté dans `docs/BUILD_AND_SETUP.md`.

---

## 🗂️ Structure du dépôt

```
Musichien/
├── CMakeLists.txt            # point d'entrée : modes normal et superbuild
├── CMakePresets.json         # presets Clang-Debug / RelWithDebInfo / Release / Superbuild
├── cmake/                    # fonctions et configuration partagées
├── superbuild/               # règles de compilation des dépendances sources
├── scripts/                  # environnement, installation, formatage, vérification, IDE
├── docs/                     # documentation du projet
└── source/
    ├── domain/               # ♥ le cœur métier : aucune dépendance à Qt, testable seul
    ├── ui/                   # les fichiers QML et les ressources
    ├── android/              # AndroidManifest.xml : ce qui garantit l'absence de permission réseau
    └── application/          # le point d'entrée : assemble, ne décide rien
```

**Règle des dépendances** — la flèche pointe toujours vers le domaine :

```
application ──► ui ──► domain
```

---

## 📐 Conventions de code

Toutes les règles sont **imposées automatiquement** par l'outillage, jamais laissées à la mémoire :

- paramètres de fonction préfixés **`p_`**, membres de classe préfixés **`m_`**
- `camelCase`, `CamelCase` pour les types, `UPPER_CASE` pour les constantes
- noms **parlants**, sans abréviation (`int i` est refusé par le linter)
- C++26, style moderne : ranges, concepts, `std::expected`, boucles sur plage
- mise en forme automatique (`clang-format`, `qmlformat`, `EditorConfig`)
- **aucune logique métier dans le QML**
- code et commentaires **en anglais** ; textes affichés en français via `qsTr()`

Documentation complète : **`docs/CODE_CONVENTIONS.md`**. Vérification : `scripts/check_code.sh`.

---

## 🌿 Git

| Branche | Rôle |
|---|---|
| **`main`** | Stable. Ne reçoit que des versions qui compilent et dont les tests passent. |
| **`develop`** | Intégration. Branche de travail quotidienne. |
| `feature/…` `fix/…` `chore/…` | Une branche par sujet, créée depuis `develop`. |

Workflow détaillé : `docs/GIT_WORKFLOW.md`.

---

## 📌 État du projet

- [x] Conception et décisions d'architecture
- [x] Socle de build (CMake, superbuild, presets)
- [x] Domaine : notes, intervalles, accords, modes — **testé** (353 tests)
- [x] Première interface QML (écran de garde)
- [x] Moteur audio (synthèse pure, testée sans carte son)
- [x] **La boucle de jeu** : intervalles nommés, sens, chant jugé en cents, rythme, accords
- [x] **Gamification** : XP, niveaux, étoiles, série, profil et statistiques
- [x] **Un accordeur complet** (30 à 4500 Hz, tempéraments et diapason)
- [x] **Un métronome battu à l'échantillon**, et une batterie **enregistrée**
- [x] **Le pilier Harmonie a commencé** : les sept modes s'écoutent sur un bourdon **enregistré** (cordes, chœur, nappe), et l'exercice du dégradé fait **comparer** deux modes puis les **nommer**
- [x] Notifications locales, sans réseau
- [x] Déploiement Android (APK `arm64-v8a`, **vérifiée sans aucune permission système**)
- [ ] Les cinq mondes de l'aventure, la révision espacée, les boss

---

## ⚖️ Licence

**GNU General Public License, version 3 ou ultérieure** — texte complet dans [`LICENSE`](LICENSE).

```
Musichien — apprendre l'oreille musicale en jouant
Copyright (C) 2026 Roger Srey (srroger)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.
```

Ce projet **doit rester libre** : aucune version dérivée ne peut être fermée.

### Pourquoi GPL-3.0 et pas MIT ?

Deux raisons, dans cet ordre :

1. **C'est cohérent avec le projet.** Musichien est personnel, sans monétisation, et doit le rester.
   Le GPL garantit la **réciprocité** : personne ne pourra jamais « fermer » ce logiciel.
2. **C'est la seule licence propre avec Qt.** Qt est utilisé sous licence GPL. Sur Android, Qt est lié
   d'une manière qui rend la conformité LGPLv3 **juridiquement contestée** (l'utilisateur ne peut pas
   remplacer une bibliothèque à l'intérieur d'un APK signé). **GPL-3.0 lève toute ambiguïté**, et
   débloque en plus les modules Qt réservés au GPL (Qt Quick 3D, Qt Virtual Keyboard, Qt Lottie…).


