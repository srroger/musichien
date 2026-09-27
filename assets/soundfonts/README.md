# Échantillons d'instruments

Des notes de **vrai piano**, de **vraie guitare** et de **vrai saxophone**, rendues une fois pour toutes depuis
une banque libre. Elles remplaceront la synthèse numérique pour les exercices — la synthèse reste pour le bruit
d'erreur et le repli.

| Instrument | Notes | Fichiers |
|---|---|---|
| **Piano** | do 2 à do 6 (MIDI 36, 48, 60, 72, 84) | `piano_c2…c6.wav` |
| **Guitare** (nylon) | do 2 à do 6 | `guitare_c2…c6.wav` |
| **Saxophone** (alto) | do 2 à do 5 — **pas de do 6** : un sax alto s'arrête au la aigu, et la banque refuse la note | `saxo_c2…c5.wav` |

**Quatorze fichiers, 2,5 s chacun, mono, 48 kHz, 16 bits signé — 3,4 Mo au total**, dans le format exact que
le moteur audio ouvre déjà.

> [!note] Les trois instruments ne sont pas là pour faire joli
> C'est le **4ᵉ axe du projet** qui commence : « pas seulement le piano ». Une oreille qui n'a entendu un
> intervalle qu'au piano ne l'a pas entendu — chaque instrument a ses harmoniques, et donc sa **couleur**.
> Le saxophone est le plus utile des trois pour ça : son timbre est **riche et impair**, et la même quinte y
> sonne tout autrement qu'au piano.


---

## 1. 📜 D'où ils viennent, et ce qu'on a le droit d'en faire

| Quoi | Détail |
|---|---|
| **Source** | **MuseScore_General**, la banque libre livrée par MuseScore |
| **Où** | `https://ftp.osuosl.org/pub/musescore/soundfont/MuseScore_General/MuseScore_General.sf3` (40 Mo) |
| **Licence** | **LGPL** (c'est ce que le fichier déclare lui-même à l'ouverture) |
| **Ce qu'on en fait** | on en **rend** cinq notes, qu'on embarque dans l'application |
| **Obligation** | **l'attribution** ci-dessus, et le fait que le projet est **libre et public** (`docs/`, [[03 - Charte - Local, libre et privé]]) — ce qui est exactement le cas |

> [!warning] C'est pour ça que ce fichier existe
> Un son sans provenance écrite est un son qu'on ne pourra plus justifier dans deux ans. **La licence d'un son
> compte autant que celle du code**, et elle se perd encore plus vite.

---

## 2. 🔁 Comment les régénérer, à l'identique

Tout tient en **une chaîne d'outils de développement** — rien de tout cela n'est embarqué dans l'application.

```bash
# 1. la banque (40 Mo, à jeter après usage : elle n'est PAS livrée)
curl -sL -o /tmp/MuseScore_General.sf3 \
  'https://ftp.osuosl.org/pub/musescore/soundfont/MuseScore_General/MuseScore_General.sf3'

# 2. cinq fichiers MIDI d'une seule note, puis leur rendu en WAV
#    (voir l'historique du dépôt : ~35 lignes de Python)
fluidsynth -ni -F /tmp/c4_raw.wav -T wav -r 48000 -g 0.9 /tmp/MuseScore_General.sf3 /tmp/c4.mid

# 3. mixage en mono, 2,5 s gardées, fondu de 50 ms, crête à 0,9
```

Pourquoi cette chaîne plutôt qu'une banque embarquée :

| | Banque embarquée | Échantillons (ce choix) |
|---|---|---|
| **Poids** | 40 à 215 Mo dans l'APK | **1,2 Mo** |
| **Dépendance** | un lecteur de SoundFont (TinySoundFont, 2 000 lignes) | **aucune** |
| **Travail Android** | un moteur audio de plus à porter | **rien** |
| **Qualité** | maximale (velocity, pédale, plusieurs couches) | très bonne pour **un** timbre |

> [!tip] La porte reste ouverte
> Le jour où le **pilier « Timbre »** demandera une guitare, une flûte et un violon ([[20 - Récapitulatif -
> Ce que Musichien veut être]]), on ajoutera **trois ou quatre échantillons de plus** — soit 1 Mo. La banque
> complète n'aura probablement jamais besoin d'être embarquée.

---

## 3. 🧱 Ce que le moteur en fera

1. **charger** les cinq fichiers depuis les ressources (`:/assets/soundfonts/`) ;
2. **choisir** l'échantillon le plus proche de la note demandée, et **transposer** par lecture à vitesse
   variable — au plus **trois demi-tons** d'écart, ce qui reste inaudible comme artefact ;
3. **envelopper** la fin (la note est coupée par l'horloge, pas par la corde) ;
4. **normaliser sur l'énergie**, exactement comme la synthèse le fait déjà : c'est ce qui garantit qu'une
   note, un accord et une gamme s'entendent **au même niveau**, ce qui est la moitié du travail d'oreille.

Et la synthèse numérique **reste** : elle est le **repli** si les échantillons manquent, et c'est elle qui
porte le **bruit d'erreur** — qui doit justement **ne pas** être beau.
