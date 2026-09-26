# 🌿 Musichien — Workflow Git

> Objectif : un historique **lisible**, où chaque commit raconte une intention, et où l'on peut
> toujours répondre à « pourquoi ce code est-il comme ça ? » deux ans plus tard.

---

## 1. Les branches

| Branche | Rôle | Règles |
|---|---|---|
| **`main`** | **Stable.** Ce qui est publié, ce qui tourne. | On n'y commite **jamais** directement. Elle ne reçoit que des fusions depuis `develop`, sur un jalon terminé. |
| **`develop`** | **Intégration.** La branche de travail quotidienne. | Doit toujours **compiler et passer les tests**. |
| **`feature/<sujet>`** | Une fonctionnalité. | Créée depuis `develop`, fusionnée dans `develop`. |
| **`fix/<sujet>`** | Une correction. | Idem. |
| **`chore/<sujet>`** | Outillage, documentation, refactor sans changement de comportement. | Idem. |
| **`release/<version>`** | *(plus tard)* Préparation d'une version. | Créée depuis `develop`, fusionnée dans `main` **et** `develop`. |

### Nommage des branches

`<type>/<sujet-court-en-kebab-case>`, en anglais, sans numéro de ticket (projet solo) :

```
feature/interval-recognition
feature/spaced-repetition
fix/qml-resource-prefix
chore/clang-tidy-naming-rules
```

> **Pourquoi deux branches permanentes pour un projet solo ?** Parce que `main` doit rester un endroit
> **sûr** : on peut y revenir, y compiler, y tagger, sans se demander si le travail du jour a cassé
> quelque chose. C'est peu de discipline pour beaucoup de sérénité.

### Cycle de vie d'une fonctionnalité

```bash
source scripts/setup_env.sh                 # toujours en premier

git checkout develop
git pull
git checkout -b feature/interval-recognition

# ... travailler, en committant souvent et petit ...

scripts/format_code.sh                      # formater AVANT de committer
scripts/check_code.sh                       # vérifier
cmake --build --preset "Build Clang-Debug Musichien"
ctest --preset "CTest Clang-Debug Musichien"

git add --patch                             # relire ce qu'on ajoute
git commit -m "feat(domain): identify simple intervals from a note sequence"

git checkout develop
git merge --no-ff feature/interval-recognition
git branch -d feature/interval-recognition
git push
```

---

## 2. Les messages de commit — Conventional Commits

Format : `<type>(<portée>): <description à l'impératif présent, en anglais>`

| Type | Quand |
|---|---|
| `feat` | Une fonctionnalité nouvelle pour la personne qui utilise l'app |
| `fix` | Une correction de bug |
| `refactor` | Réécriture sans changement de comportement |
| `test` | Ajout ou correction de tests uniquement |
| `docs` | Documentation uniquement |
| `build` | CMake, presets, dépendances, superbuild |
| `chore` | Outillage, scripts, configuration de l'éditeur |
| `style` | **Formatage uniquement** — jamais de logique mélangée |

**Portées usuelles** : `domain`, `ui`, `application`, `audio`, `persistence`, `cmake`, `scripts`, `docs`.

```
feat(domain): add interval identification for a note sequence
fix(ui): correct the resource prefix so the QML scene loads
refactor(domain): normalise intervals in the constructor
test(domain): cover descending intervals and the unison
build(cmake): pin GoogleTest 1.18.0 through the superbuild
docs: document the coding conventions
style: apply clang-format
```

> **Règle absolue** : **un commit ne mélange jamais formatage et logique.** Un `git blame` qui pointe
> vers un commit de formatage au lieu du vrai changement est un `git blame` inutile.

---

## 3. Ce qui ne rentre **jamais** dans Git

Voir `.gitignore`. En résumé :

- les arbres de build (`Musichien-build/`), `compile_commands.json`, `CMakeUserPresets.json`
- le code généré par Qt (`moc_*`, `ui_*`, `qrc_*`, `qmlcache/`)
- **les keystores Android** (`*.keystore`, `*.jks`) : un fichier perdu = une identité d'application perdue
- les fichiers d'éditeur propres à une machine

---

## 4. Étiquettes (tags)

Format : **`vMAJEUR.MINEUR.CORRECTIF`**, par exemple `v0.1.0`.

| Quand | Quoi |
|---|---|
| Une version **publiée** (APK distribué) | `v0.1.0`, `v0.2.0`… |
| Une étape notable, non publiée | *(pas de tag : un commit sur `develop` suffit)* |

Le numéro de version de l'application vient de l'appel `project()` du `CMakeLists.txt` racine : **une
seule source de vérité**.

```bash
git tag -a v0.1.0 -m "First playable screen"
git push origin v0.1.0
```

---

## 5. Vérifications avant chaque commit

```bash
scripts/format_code.sh     # met en forme C++ et QML
scripts/check_code.sh      # échoue si quelque chose ne respecte pas les conventions
ctest --preset "CTest Clang-Debug Musichien"
```

> **Le critère minimal** : `develop` doit **toujours** compiler et passer ses tests. Un commit qui
> casse le build sur `develop` doit être corrigé ou annulé immédiatement — c'est ce qui permet de
> `git bisect` plus tard sans se battre contre du bruit.
