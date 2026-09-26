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
| **Hors-ligne** | L'application ne déclare **aucune permission `INTERNET`**. Sans cette déclaration, Android refuse tout accès réseau : ce n'est pas une promesse, c'est une impossibilité technique. |
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
```

Procédure complète, prérequis et dépannage : `docs/BUILD_AND_SETUP.md`.

---

## 🧱 Dépendances

### Outils pris sur la machine (génériques)

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
- [x] Domaine : notes et intervalles, **testés**
- [x] Première interface QML (écran de garde)
- [x] Moteur audio (synthèse pure, testée sans carte son)
- [ ] Boucle de jeu, progression, gamification
- [x] Déploiement Android (APK `arm64-v8a`, **vérifiée sans aucune permission système**)

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


