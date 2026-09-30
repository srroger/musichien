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

## 🥁 La batterie et le métronome (même banque, même chaîne)

| Son | Note MIDI (percussion) | Durée gardée | Fichier |
|---|---|---|---|
| **Grosse caisse** | 36 (Acoustic Bass Drum) | 0,9 s | `drum_kick.wav` |
| **Caisse claire** | 38 (Acoustic Snare) | 0,9 s | `drum_snare.wav` |
| **Charleston** | 42 (Closed Hi-Hat) | 0,30 s | `drum_hihat.wav` |
| **Tom** | 47 (Low-Mid Tom) | 1,0 s | `drum_tom.wav` |
| **Métronome — temps fort** | 76 (Hi Wood Block) | 0,12 s | `drum_click_high.wav` |
| **Métronome — temps faibles** | 77 (Low Wood Block) | 0,12 s | `drum_click_low.wav` |

**Six fichiers, environ 320 Ko.** La percussion est le seul endroit de la norme MIDI où **le numéro de note désigne
un instrument** : elle se joue sur le canal 10, et elle **ne se transpose pas** (c'est pour ça que le lecteur les
joue telles quelles, sans passer par l'échantillonneur).

> [!note] Pourquoi la batterie est échantillonnée elle aussi
> La première version était **synthétisée** — une chute de sinus pour la grosse caisse, du bruit façonné pour la caisse
> claire. Roger l'a entendue tout de suite : « je les trouve un peu faible et un peu moche ». Et la raison est
> exactement celle qui avait rendu les échantillons de piano nécessaires : une vraie percussion, c'est **une peau, une
> coque et une baguette**, et aucune quantité d'arithmétique n'en fait un modèle. Le synthétiseur garde son rôle de
> **repli** quand un fichier manque.

> [!note] Pourquoi deux blocs de bois pour le métronome
> Roger trouvait le temps fort bien, mais les autres « un peu moches ». Un **bloc de bois** est la sonorité du
> métronome : courte, sans hauteur musicale qui traîne, et l'écart aigu/grave dit **où est le premier temps** sans
> qu'on ait à compter.

### Régénérer la batterie

```bash
# la banque (40 Mo, à jeter après usage : elle n'est PAS livrée)
curl -sL -o /tmp/MuseScore_General.sf3 \
  'https://ftp.osuosl.org/pub/musescore/soundfont/MuseScore_General/MuseScore_General.sf3'

# les six fichiers, d'un coup : le script écrit le MIDI, appelle fluidsynth, et met en forme
scripts/render_drum_samples.py /tmp/MuseScore_General.sf3
```

Le script est **dans le dépôt** (`scripts/render_drum_samples.py`), et pas seulement dans l'historique : un son dont
on ne peut pas refaire la recette est un son qu'on ne pourra plus justifier dans deux ans.

> [!note] Les trois instruments ne sont pas là pour faire joli
> C'est le **4ᵉ axe du projet** qui commence : « pas seulement le piano ». Une oreille qui n'a entendu un
> intervalle qu'au piano ne l'a pas entendu — chaque instrument a ses harmoniques, et donc sa **couleur**.
> Le saxophone est le plus utile des trois pour ça : son timbre est **riche et impair**, et la même quinte y
> sonne tout autrement qu'au piano.


---

## 🎻 Les bourdons (même banque, autre usage)

| Timbre | Programme General MIDI | Notes | Fichiers |
|---|---|---|---|
| **Cordes** | String Ensemble 1 | ré 2, la 2 | `drone_strings_d2.wav`, `drone_strings_a2.wav` |
| **Chœur** | Choir Aahs | ré 2, la 2 | `drone_choir_d2.wav`, `drone_choir_a2.wav` |
| **Nappe** | Pad 2 (warm) | ré 2, la 2 | `drone_pad_d2.wav`, `drone_pad_a2.wav` |

**Six fichiers, 12 s chacun, mono, 48 kHz, 16 bits — 6,9 Mo.** Ils ne servent pas à jouer une mélodie : ils tiennent
**sous** une gamme, et c'est ce qui donne un CENTRE à un mode. Sans bourdon, sept notes ne sont que sept notes — et
« écoute cette gamme et nomme le mode » n'est pas une question difficile, c'est une question sans réponse. Voir
[[27 - Les modes - la couleur et le cercle des quintes]] §4.

> [!note] Pourquoi une quinte, et pourquoi deux notes séparées
> Le bourdon est **la tonique et sa quinte**, tenues ensemble : une note seule dit « ceci est la tonique », une quinte
> dit « ceci est le centre ». Les deux notes sont enregistrées **séparément** pour que le bourdon puisse être
> **transposé** : le domaine tire sa tonique entre 35 et 41, et un échantillon déplacé de plus de trois demi-tons
> s'entend comme un ralentissement.

### Régénérer les bourdons

```bash
# la même banque que la batterie et les instruments
scripts/render_drone_samples.py /tmp/MuseScore_General.sf3
```

Le dossier de sortie par défaut est celui de l'atelier (`~/Musichien-atelier/drones`), et **pas** `assets/soundfonts` :
un candidat s'écoute **avant** d'entrer dans le dépôt. Quand un timbre est retenu, ses fichiers passent dans
`assets/soundfonts/` et sont déclarés dans `source/ui/resources.qrc`.

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
