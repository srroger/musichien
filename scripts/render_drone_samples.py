#!/usr/bin/env python3
# =====================================================================================================================
# Musichien - renders the sustained drone samples
#
# Le BOURDON est joue, et non modelise. Comme le piano, la guitare, le saxophone et la batterie, il est rendu une fois
# pour toutes depuis une banque General MIDI libre, puis commite en petits fichiers mono. Voir
# assets/soundfonts/README.md pour la licence et la provenance, qui comptent autant que celle du code.
#
# ---------------------------------------------------------------------------------------------------------------------
# Pourquoi ce script existe, et ce qu'il a fallu entendre pour y venir
#
# Le premier bourdon etait SYNTHETISE : une corde frappee, puis une dents de scie, puis un son d'orgue a sept
# harmoniques. Trois versions, trois verdicts d'oreille, et le dernier resume les deux premiers :
#
#   * une corde frappee DECROIT. A 1,85 s elle etait trente decibels sous son attaque : ce n'etait plus un bourdon,
#     c'etait un souvenir de bourdon. Roger : « on ne l'entend pas assez longtemps » ;
#   * une dents de scie contient des harmoniques jusqu'a Nyquist, qui se replient : Roger : « on dirait un vieux son
#     NES qui gresille » ;
#   * un orgue additif, enfin, TIENT et ne gresille plus - mais reste « un peu moche ». Roger : « une bonne qualite de
#     son est essentielle pour rendre l'experience agreable », et il a autorise le telechargement.
#
# Un bourdon est un son TENU, LONG, et DOUX, tenu par un veritable instrument : aucune arithmetique ne fait un
# ensemble a cordes. La lecon est exactement celle de la batterie, et elle s'ecrit deux fois parce qu'elle a ete apprise
# deux fois.
#
# ---------------------------------------------------------------------------------------------------------------------
# Trois candidats, et c'est l'oreille qui tranche
#
# Les trois programmes sont ceux qu'ecoute un bourdon modal : des cordes (tenues, riches, stables), un chœur (plus
# aerien), et une nappe (plus synthetique). Le meilleur n'est pas decidable ici : il se choisit en ecoutant.
#
# Usage:
#   ./scripts/render_drone_samples.py /tmp/MuseScore_General.sf3 [dossier_de_sortie]
#
# Le dossier de sortie par defaut est celui de l'atelier (~/Musichien-atelier/drones), et NON assets/soundfonts : un
# candidat s'ecoute avant d'entrer dans le depot. Quand Roger en a choisi un, ses fichiers passent dans
# assets/soundfonts et sont declares dans source/ui/resources.qrc.
#
# La banque n'est PAS livree : elle pese 40 Mo, elle se telecharge une fois, et l'application n'en a jamais besoin.
# =====================================================================================================================

import struct
import subprocess
import sys
import tempfile
import wave
from pathlib import Path

import numpy as np

# General MIDI : le numero de programme choisit l'instrument. Les numeros sont ceux du standard, bases sur zero.
CANDIDATES = {
    "strings": 48,      # String Ensemble 1
    "choir": 52,        # Choir Aahs
    "pad": 89,          # Pad 2 (warm)
}

# La QUINTE du bourdon : la tonique, et sa quinte juste au-dessus. Roger a tranche, deux fois de suite : une note seule
# dit « ceci est la tonique », une quinte dit « ceci est le CENTRE » - et ce n'est pas la meme information a entendre.
DRONE_NOTES = {
    "d2": 38,
    "a2": 45,
}

VELOCITY = 100
SAMPLE_RATE = 48000
DRONE_SECONDS = 12.0
PEAK = 0.9


def variable_length(value: int) -> bytes:
    """Un nombre a longueur variable, comme le veut le format MIDI."""
    chunks = [value & 0x7F]
    value >>= 7
    while value:
        chunks.append((value & 0x7F) | 0x80)
        value >>= 7
    return bytes(reversed(chunks))


def program_midi(program: int, notes: list[int], duration_seconds: float, path: Path) -> None:
    """Un fichier MIDI type 0 : le programme voulu en tete, les notes tenues ensemble, puis leur relachement.

    Le canal est le 1 (index 0), et non le 10 : la percussion est le seul endroit de la norme ou le numero de note
    designe un instrument. Ici, la note designe une hauteur, et c'est la hauteur qu'on veut.
    """
    ticks_per_beat = 480
    duration_ticks = int(ticks_per_beat * duration_seconds * 2)  # deux temps par seconde, comme la batterie

    track = b""
    track += variable_length(0) + bytes([0xC0, program])  # program change

    for note in notes:
        track += variable_length(0) + bytes([0x90, note, VELOCITY])

    for index, note in enumerate(notes):
        # Le PREMIER relachement porte la duree, les suivants tombent au meme instant : c'est ce qui tient les notes
        # ensemble au lieu de les echelonner.
        delay = duration_ticks if index == 0 else 0
        track += variable_length(delay) + bytes([0x80, note, 0])

    track += variable_length(0) + b"\xFF\x2F\x00"  # end of track

    header = b"MThd" + struct.pack(">IHHH", 6, 0, 1, ticks_per_beat)
    chunk = b"MTrk" + struct.pack(">I", len(track)) + track

    path.write_bytes(header + chunk)


def read_wave(path: Path) -> np.ndarray:
    with wave.open(str(path), "rb") as source:
        channel_count = source.getnchannels()
        sample_width = source.getsampwidth()
        if sample_width != 2:
            raise SystemExit(f"{path}: expected 16 bit samples, found {sample_width * 8} bit")

        frames = np.frombuffer(source.readframes(source.getnframes()), dtype="<i2")

    frames = frames.reshape(-1, channel_count).astype(np.float64) / 32768.0

    # Une stéréo est MOYENNÉE plutôt qu'amputée d'un canal : garder un seul canal jetterait la moitié du son, et un
    # bourdon n'est pas panoramisé dans cette application.
    return frames.mean(axis=1)


def write_wave(path: Path, samples: np.ndarray) -> None:
    clipped = np.clip(samples, -1.0, 1.0)
    data = (clipped * 32767.0).astype("<i2")

    with wave.open(str(path), "wb") as target:
        target.setnchannels(1)
        target.setsampwidth(2)
        target.setframerate(SAMPLE_RATE)
        target.writeframes(data.tobytes())


def prepare(samples: np.ndarray, kept_seconds: float) -> np.ndarray:
    """Garde la duree voulue, adoucit les deux bouts, et amene la crete au niveau demande.

    Le fondu de FIN est long, et ce n'est pas un detail de confort : un bourdon s'entend dans une phrase qui va
    jusqu'au bout de son silence, et une fin coupee net serait un clic au milieu de la musique. Le fondu d'ATTAQUE
    est court, parce qu'un ensemble a cordes met un dixieme de seconde a s'installer et que c'est justement ce
    naturel qu'on est venu chercher.
    """
    kept = samples[: int(kept_seconds * SAMPLE_RATE)]

    attack_length = min(len(kept), int(0.10 * SAMPLE_RATE))
    if attack_length > 0:
        kept[:attack_length] *= np.linspace(0.0, 1.0, attack_length)

    release_length = min(len(kept), int(1.50 * SAMPLE_RATE))
    if release_length > 0:
        kept[-release_length:] *= np.linspace(1.0, 0.0, release_length)

    peak = float(np.max(np.abs(kept))) if len(kept) else 0.0
    if peak > 0.0:
        kept *= PEAK / peak

    return kept


def render(bank: Path, work: Path, candidate: str, program: int, notes: list[int], name: str,
           output_directory: Path) -> None:
    midi_path = work / f"{name}.mid"
    raw_path = work / f"{name}_raw.wav"
    output_path = output_directory / f"{candidate}_{name}.wav"

    program_midi(program, notes, DRONE_SECONDS, midi_path)

    subprocess.run(
        [
            "fluidsynth", "-ni",
            "-F", str(raw_path),
            "-T", "wav",
            "-r", str(SAMPLE_RATE),
            "-g", "0.9",
            str(bank),
            str(midi_path),
        ],
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    samples = prepare(read_wave(raw_path), DRONE_SECONDS)
    write_wave(output_path, samples)

    print(f"{output_path.name}: {len(samples) / SAMPLE_RATE:.2f} s, {output_path.stat().st_size} bytes")


def main() -> None:
    if len(sys.argv) not in (2, 3):
        raise SystemExit(__doc__)

    bank = Path(sys.argv[1])
    if not bank.is_file():
        raise SystemExit(f"sound bank not found: {bank}")

    output_directory = Path(sys.argv[2]) if len(sys.argv) == 3 else Path.home() / "Musichien-atelier" / "drones"
    output_directory.mkdir(parents=True, exist_ok=True)

    quinte = list(DRONE_NOTES.values())

    with tempfile.TemporaryDirectory() as work_directory:
        work = Path(work_directory)

        for candidate, program in CANDIDATES.items():
            # 1. La QUINTE tenue : ce qu'on ecoute pour juger le timbre, et rien d'autre.
            render(bank, work, candidate, program, quinte, "quinte_d2a2", output_directory)

            # 2. Les deux notes separees : elles serviront a TRANSPOSER le bourdon dans une autre tonique, car une
            #    seule note peut etre deplacee de quelques demi-tons sans qu'on l'entende.
            for note_name, midi_number in DRONE_NOTES.items():
                render(bank, work, candidate, program, [midi_number], note_name, output_directory)


if __name__ == "__main__":
    main()

