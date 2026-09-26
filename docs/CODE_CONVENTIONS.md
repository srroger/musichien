---
type: référence
status: actif
domaine: musique
créé: 2026-09-26
aliases:
  - Conventions de code Musichien
tags:
  - musichien
  - conventions
  - cpp
  - qml
  - style
---

# 📐 Musichien — Conventions de code

> [!abstract] Pourquoi ce document
> Objectif : un code **uniforme**, **sans conflit Git parasite**, et **lisible par n'importe qui**.
> La règle d'or : **on ne compte pas sur la mémoire humaine — on configure les outils pour imposer les règles.**

> [!important] Ces règles sont **automatiquement vérifiées**
> - `.clang-format` → mise en forme (espaces, retours à la ligne)
> - `.clang-tidy` → **préfixes `m_` / `p_`**, camelCase, longueur des noms
> - `.editorconfig` → espaces/tabulations identiques dans tous les éditeurs
> - `qmlformat` / `qmllint` → mise en forme et vérification du QML
>
> Vérifié : `clang-tidy` suggère réellement `p_value` et `m_counter`, et refuse `int i`.

---

## 1. Langue

| Élément | Langue |
|---|---|
| **Code** : noms de classes, fonctions, variables, membres, fichiers | 🇬🇧 **Anglais** |
| **Commentaires** dans le code | 🇬🇧 **Anglais** |
| **Messages de commit Git** | 🇬🇧 **Anglais** |
| **Documentation** (`docs/`, ce fichier) | 🇫🇷 Français |
| **Textes affichés dans l'application** | 🇫🇷 **Français** (via fichiers de traduction Qt) |

> [!tip] Raison
> Un code en anglais est **partageable et universel**. Un projet libre qui ne serait lu que par des francophones se coupe de 95 % de ses relecteurs potentiels.
> Les textes utilisateur passent par `tr(...)` et les fichiers `.ts` : **aucune chaîne française en dur dans le code**.

---

## 2. 🏷️ Nommage — les règles

### 2.1 Tableau de référence

| Élément | Règle | Exemple |
|---|---|---|
| Classe / `struct` / `union` / `enum` | `CamelCase` | `IntervalTrainer`, `AudioEngine` |
| Alias de type / `typedef` | `CamelCase` | `NoteId`, `ExerciseList` |
| **Fonction / méthode** | `camelBack` | `computeScore()`, `loadFromFile()` |
| Espace de noms | `lower_case` | `musichien::domain` |
| Variable locale | `camelBack` | `remainingAttempts` |
| Variable globale | `camelBack` | `applicationInstance` |
| **Membre de classe (privé/protégé)** | **`m_`** + `camelBack` | `m_currentLevel`, `m_audioEngine` |
| **Membre public** | `camelBack` **sans préfixe** | `noteIndex`, `isCorrect` |
| **Paramètre de fonction** | **`p_`** + `camelBack` | `p_interval`, `p_playerScore` |
| Constante / `constexpr` | `UPPER_CASE` | `MAX_INTERVAL_SEMITONES` |
| Énumérateur | `CamelCase` | `IntervalQuality::Major` |
| Macro | `UPPER_CASE` | `MUSICHIEN_ASSERT` |
| Paramètre de template | `CamelCase` | `template < typename ValueType >` |

### 2.2 Les deux règles signature du projet

```cpp
// ---------------------------------------------------------------------------------------------------------------
// m_ for class data members, p_ for function parameters
// ---------------------------------------------------------------------------------------------------------------
class IntervalTrainer
{
public:
    explicit IntervalTrainer( const AudioEngine & p_audioEngine )
        : m_audioEngine{ p_audioEngine }
    {
    }

    PlaybackResult playExercise( const Exercise & p_exercise, int p_repeatCount )
    {
        PlaybackResult playbackResult = m_audioEngine.play( p_exercise.firstNote );

        for ( const Note & note : p_exercise.remainingNotes )
        {
            m_playedNotes.push_back( note );
        }

        return playbackResult;
    }

private:
    const AudioEngine & m_audioEngine;
    std::vector< Note >  m_playedNotes;
};
```

> [!warning] Exception documentée : les membres publics
> Les membres publics **ne sont pas préfixés**. Raison : les `struct` de données simples et les types exposés au QML se lisent mieux sans préfixe, et un préfixe y perdrait son sens de « état interne ».
> **Si un membre doit être préfixé, c'est le signe qu'il devrait être privé.**

### 2.3 Noms parlants — pas d'abréviations

> [!danger] Interdit
> `i`, `j`, `it`, `str`, `tmp`, `val`, `idx`, `ptr`, `res`, `cnt`, `nb`, `cfg`, `msg`, `ctx`…

> [!success] Attendu
> Même dans une boucle, **le nom doit dire l'intention**.

```cpp
// ❌ Refusé par clang-tidy (readability-identifier-length) et refusé en revue
for ( int i = 0; i < notes.size(); ++i ) { }
for ( auto & it : exercises ) { }
std::string str = toString( interval );

// ✅ Attendu
for ( int notePosition = 0; notePosition < notes.size(); ++notePosition ) { }
for ( Exercise & exercise : exercises ) { }
std::string intervalName = toString( interval );
```

**Abréviations évidentes tolérées** : unités (`ms`, `hz`, `bpm`, `db`, `x`, `y`), et noms d'API externes imposés par un framework.

> [!note] Limite honnête du linter
> `readability-identifier-length` vérifie la **longueur** (≥ 3 caractères), pas le **sens**. Le respect de l'intention reste une **règle de revue humaine**.

---

## 3. 🎨 QML — adaptation des règles

Les règles C++ s'appliquent au QML **avec les exceptions propres au langage**.

| Élément QML | Règle | Exemple |
|---|---|---|
| **Composant** (nom de fichier) | `CamelCase`, **fichier commençant par une majuscule** | `TrainerScreen.qml` |
| `id` d'un item | `camelBack` | `id: submitButton` |
| **Paramètre de fonction QML** | **`p_`** + `camelBack` | `function playInterval( p_noteA, p_noteB )` |
| `property` d'un item | `camelBack`, **sans préfixe** | `property int remainingAttempts` |
| `signal` | `camelBack` (convention Qt) | `signal exerciseFinished( int p_score )` |
| Fonction JavaScript locale | `camelBack` | `function formatDuration( p_seconds )` |

```qml
// TrainerScreen.qml
import QtQuick
import QtQuick.Controls

Item
{
    id: trainerScreen

    property int remainingAttempts: 3
    property string currentExerciseName: ""

    signal exerciseFinished( int p_finalScore )

    // Function parameters follow the p_ rule, exactly like in C++.
    function presentExercise( p_exerciseName, p_attempts )
    {
        currentExerciseName = p_exerciseName;
        remainingAttempts = p_attempts;
    }

    Button
    {
        id: submitButton
        text: qsTr( "Valider" )          // French text always goes through qsTr()
        enabled: trainerScreen.remainingAttempts > 0
        onClicked: trainerScreen.exerciseFinished( 42 )
    }
}
```

> [!important] Interdiction absolue dans le QML
> **Aucune logique métier dans les fichiers `.qml`.** Le QML **affiche** et **délègue**.
> Tout calcul (score, progression, génération d'exercice) vit dans le **C++**, exposé au QML via `Q_PROPERTY`, `Q_INVOKABLE` et des modèles.
> Raison : c'est ce qui rend la logique **testable sans interface graphique** (voir `docs/ARCHITECTURE.md`).

**Mise en forme** : `qmlformat` (fourni par Qt) + `qmllint` pour la vérification statique.

---

## 4. 🚀 C++26 moderne — le style visé

> [!note] Intention
> Écrire du code **moderne par défaut**, pour s'habituer à de belles syntaxes. Ce n'est pas du zèle : les algorithmes de la bibliothèque standard sont **plus lisibles et moins bugués** qu'une boucle écrite à la main.

### 4.1 Oublier les boucles classiques

```cpp
// ❌ À éviter
for ( int index = 0; index < notes.size(); ++index )
{
    applyTransposition( notes[ index ] );
}

// ✅ Boucle sur plage
for ( Note & note : notes )
{
    applyTransposition( note );
}

// ✅ Mieux encore : un algorithme
std::ranges::for_each( notes, applyTransposition );

// ✅ Ranges paresseux et composables
auto ascendingIntervals = notes
                        | std::views::filter( isAscending )
                        | std::views::transform( intervalInSemitones )
                        | std::ranges::to< std::vector >();
```

### 4.2 Le vocabulaire moderne attendu

| Outil | Usage |
|---|---|
| `std::ranges::*` | Algorithmes : `find_if`, `sort`, `transform`, `any_of`, `count_if` |
| `std::views::*` | Pipelines paresseux : `filter`, `transform`, `take`, `drop`, `join` |
| `concepts` | Contraindre les templates et **remplacer les `enable_if`** |
| `std::optional` | Valeur **absente** (ce n'est pas une erreur) |
| `std::expected` (C++23) | Erreur **attendue** — remplace les codes retour et les exceptions de contrôle |
| `constexpr` / `consteval` | Tout ce qui peut être calculé à la compilation **doit** l'être |
| Liaisons structurées | `for ( const auto & [ chapterId, chapter ] : m_chapters )` |
| `std::span` | Vue non propriétaire sur un tableau (remplace `T * p_data, size_t p_size`) |
| `std::string_view` | Paramètre chaîne non propriétaire |
| Désignateurs (`Designated initializers`) | `Exercise{ .seconds = 3, .repetitions = 5 }` |
| `[[nodiscard]]` | Sur **tout** ce qui renvoie un résultat à ne pas ignorer |
| `auto` avec parcimonie | Uniquement quand le type est **évident ou très verbeux** |

> [!warning] Sur la réflexion (P2996)
> La réflexion est bien au programme de **C++26**, mais **son support compilateur reste expérimental** à ce jour. Elle sera activée **dès qu'elle sera utilisable en production**, pas avant — se priver d'un outil utile serait dommage, mais fonder l'architecture sur une fonctionnalité instable serait une faute. Le suivi est noté dans `docs/ARCHITECTURE.md`.

### 4.3 Règles de conception SOLID

| Principe | Traduction concrète dans Musichien |
|---|---|
| **S** — Responsabilité unique | Une classe = une raison de changer. `ExerciseGenerator` génère, il ne joue pas de son. |
| **O** — Ouvert / fermé | Ajouter un type d'exercice = **ajouter un fichier de données**, pas modifier le générateur |
| **L** — Substitution | Toute implémentation d'une interface est interchangeable sans surprise |
| **I** — Ségrégation d'interfaces | Interfaces **petites et ciblées**, jamais une « interface à tout faire » |
| **D** — Inversion de dépendance | Le domaine **ne connaît ni Qt, ni le système de fichiers, ni le matériel** |

**Patterns attendus** — à utiliser quand ils apportent quelque chose, jamais par décoration : *Strategy* (génération d'exercices), *Repository* (persistance), *Adapter* (audio : `QAudioSink` derrière une interface du domaine), *Factory* (création d'exercices), *Observer* (signaux Qt, uniquement en frontière).

> [!tip] Règle des dépendances
> **La flèche des dépendances pointe toujours vers le domaine.**
> `ui` → `application` → `domain` ← `infrastructure`
> Le `domain` ne dépend de **rien** d'externe. C'est ce qui le rend testable et portable.

---

## 5. 🧹 Mise en forme automatique

> [!success] Principe
> **Aucun débat sur les espaces. Aucun ajustement « à la main ».** L'outil décide, tout le monde obéit.

| Outil | Fichier | Ce qu'il garantit |
|---|---|---|
| `clang-format` | `.clang-format` | Indentation 4 espaces, jamais de tabulation, `ColumnLimit: 0` (aucun retour à la ligne superflu), **1 ligne vide maximum**, style `foo( bar )` hérité d'ADAM |
| `clang-tidy` | `.clang-tidy` | Préfixes `m_`/`p_`, camelCase, longueur des noms, `const`-correctness, modernisation (`modernize-*`), performances |
| `EditorConfig` | `.editorconfig` | Fin de ligne LF, newline final, pas d'espace en fin de ligne, UTF-8 |
| `qmlformat` / `qmllint` | — | Mise en forme et vérification statique du QML |
| `.gitattributes` | — | **LF stocké et coché partout** → aucun diff fantôme entre machines |

```bash
scripts/format_code.sh    # formate C++ et QML, en place
scripts/check_code.sh     # vérifie sans modifier, échoue en cas d'écart
```

> [!warning] Convention
> **Un commit ne doit jamais mélanger formatage et logique.** On formate **avant** de committer, ou dans un commit dédié `style:`. C'est ce qui garde un historique lisible et des `git blame` utiles.

---

## 6. 📝 Commits et branches

Voir `docs/GIT_WORKFLOW.md`. En résumé : **Conventional Commits** en anglais (`feat:`, `fix:`, `refactor:`, `test:`, `docs:`, `chore:`), branches `feature/…` / `fix/…` / `chore/…` créées depuis `develop`, et **jamais** de commit direct sur `main`.


