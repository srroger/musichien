# 🏛️ Musichien — Architecture

> Principe directeur : **le cœur métier ne dépend de rien.** Tout le reste s'organise autour.

---

## 1. Les quatre couches

```
┌──────────────────────────────────────────────────────────────────────────────┐
│  application          point d'entrée : assemble et câble, ne décide rien     │
│      │                                                                       │
│      ├──────────►  ui          QML et modèles de vue : affiche et délègue    │
│      │                │                                                      │
│      └────────────────┴──────────►  domain    ♥ le cœur musical             │
│                                          ▲                                   │
│  infrastructure (à venir)  ──────────────┘                                   │
│  sauvegarde, audio, temps : implémentent les interfaces du domaine           │
└──────────────────────────────────────────────────────────────────────────────┘
```

| Couche | Contenu | Dépendances autorisées |
|---|---|---|
| **`domain`** | Notes, intervalles, gammes, accords, génération d'exercices, score, révision espacée | **Rien d'externe.** Uniquement la bibliothèque standard C++. |
| **`ui`** | Fichiers QML, modèles de vue exposés au QML | Qt, `domain` |
| **`application`** | `main()`, création du moteur QML, câblage | Qt, `ui`, `domain` |
| **`infrastructure`** *(à venir)* | Persistance JSON, moteur audio, horloge, notifications locales | Qt, `domain` |

### Pourquoi cette règle est la plus importante du projet

> **Le domaine ne sait pas qu'il tourne sur un téléphone.**

Conséquence concrète : toute la musique se teste **sur la machine de développement, en millisecondes,
sans émulateur, sans téléphone, sans carte son.** C'est ce qui rend le projet tenable pour une seule
personne qui ne code pas tous les jours.

C'est aussi ce qui rend un éventuel changement d'interface (ou même de framework) **non destructeur**.

---

## 2. Le sens des dépendances

```
application ──► ui ──► domain
      │                  ▲
      └──► infrastructure┘
```

**La flèche ne va jamais dans l'autre sens.** Le jour où `domain` a besoin de Qt ou d'un fichier,
c'est le signe qu'une abstraction manque : on ajoute une **interface** dans le domaine, et
l'implémentation concrète dans `infrastructure` (inversion de dépendance).

### Exemple : l'audio

```cpp
// Dans le DOMAINE : une interface, aucun détail technique.
class NotePlayer
{
public:
    virtual ~NotePlayer() = default;
    virtual void play( const Note & p_note ) = 0;
};

// Dans l'INFRASTRUCTURE : l'implémentation Qt, seule à connaître QAudioSink.
class QAudioNotePlayer final : public NotePlayer { ... };
```

Le domaine exprime **ce dont il a besoin** ; l'infrastructure fournit **comment**. Les tests du
domaine utilisent un `FakeNotePlayer` qui note ce qu'on lui a demandé de jouer.

---

## 3. Le contenu est de la donnée, pas du code

Décision structurante (voir les notes de conception) : chapitres, épreuves, exercices et récompenses
sont décrits dans des **fichiers de données**, jamais codés en dur.

```
assets/content/
├── worlds.json                   # les mondes narratifs
├── world-01/chapters.json        # les chapitres d'un monde
└── exercises/…                   # les définitions d'exercices
```

**Pourquoi :** ajouter 200 exercices doit être une opération de **contenu**, jamais une recompilation
ni une modification de la logique. Le code contient des **règles**, les fichiers contiennent du
**contenu**.

Corollaire : les **axes de difficulté sont orthogonaux** — `compétence × timbre × direction ×
registre × contexte`. L'application est un **générateur** d'exercices, pas une base de questions
écrite à la main.

---

## 4. La sauvegarde

Un **fichier JSON local**, avec un champ `version` pour pouvoir faire évoluer le format sans rien
perdre. Exportable et importable : l'utilisateur n'est jamais prisonnier de l'application.

```json
{
  "version": 1,
  "avatar": { "rang": 2, "equipement": ["collier_rouge"] },
  "xp": 1240,
  "streak": { "jours": 4, "dernierJour": "2026-09-26", "gels": 1 },
  "chapitres": { "MONDE1.CH01": { "etoiles": 3, "boss": "reussi" } },
  "revisionEspacee": [ { "id": "MONDE1.CH02", "acquis": 0.72, "prochaineRevue": "2026-09-28" } ]
}
```

---

## 5. Notes de suivi technique

| Sujet | État |
|---|---|
| **Qt minimum : 6.11** | ✅ **Vérifié et imposé.** Qt 6.10 et antérieurs ne compilent pas en C++26 (`std::saturate_cast` renommé en `std::saturating_cast`). Refus explicite au moment de la configuration. |
| **Layout audio de sortie** | ✅ Le **layout du périphérique** est utilisé (stéréo en général) et le signal **mono est dupliqué sur tous les canaux**. Demander un flux **mono** faisait sortir le son du **canal gauche uniquement**, selon le back-end. |
| **Format d'échantillon** | ⚠️ **Float 32 bits exigé.** Le repli vers **Int16** n'est pas implémenté : si un périphérique ne propose que de l'Int16, la lecture reste silencieuse avec un message clair. À faire si Android le réclame. |
| **Latence audio** | À mesurer. Aucune optimisation de latence n'a encore été faite : le tampon complet est poussé en une seule écriture, ce qui convient à une note mais pas à un métronome. |
| **Réflexion C++26 (P2996)** | ❌ **indisponible** : `-freflection` inconnu de Clang 22, `<experimental/meta>` absent. À activer dès qu'un support stable arrive. |
| **SoundFonts** | À choisir : c'est la façon économique d'avoir plusieurs instruments crédibles. |
| **Android** | ✅ **Fonctionne de bout en bout.** APK `arm64-v8a` construite, **relue sans aucune permission système**, signée puis installée par `scripts/build_android.sh`. NDK **r27c (27.2.12479018)**, imposé par Qt 6.12 ; **JDK 21** (jamais le JDK 26 du système) ; plateforme **android-36**. |
| **Portabilité de la bibliothèque standard** | ⚠️ **Contrainte de conception, pas un détail.** Le NDK r27c embarque **libc++ 18**, plus ancienne que le **libstdc++ 16** du poste de développement. Le domaine doit donc se limiter aux fonctionnalités présentes dans **les deux** : pas de `std::views::adjacent`, pas de `std::span::at`. C'est le prix à payer pour que **le code testé sur la machine soit exactement celui qui tourne sur le téléphone** — un seul chemin, testé une fois. |
| **`INTERNET` : promesse ou garantie ?** | ✅ **Garantie, et vérifiée deux fois.** Qt réclame `INTERNET` via **deux canaux** distincts (`QT_ANDROID_PERMISSIONS` et les `<module>-android-dependencies.xml` lus par `androiddeployqt`). Les deux sont fermés, et `scripts/build_android.sh` relit les permissions dans l'APK **compilée puis signée**. Sans `android.permission.INTERNET`, Android exclut le processus du groupe `inet` : c'est le noyau qui refuse, pas le code. |


---

## 6. Ce qui n'est **jamais** permis

| Interdit | Raison |
|---|---|
| Dépendance à Qt / au système de fichiers dans `domain` | Détruit la testabilité, qui est la raison d'être de cette architecture |
| Logique métier dans un fichier `.qml` | Non testable, et impossible à réutiliser |
| Chaîne de caractères française en dur dans le code | Les textes passent par `qsTr()` |
| Accès réseau, SDK de traçage, publicité, achat intégré | Aucune permission `INTERNET` déclarée, et `scripts/build_android.sh` **refuse** de produire une APK qui en demanderait une |
| Version codée en dur ailleurs que dans `cmake/musichienVersions.cmake` | Une seule source de vérité |
| Formatage et logique dans le même commit | Rend le `git blame` inutilisable |
