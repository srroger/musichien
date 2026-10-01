#!/usr/bin/env python3
# =====================================================================================================================
# Musichien - renders the melody instrument samples
#
# Cinq notes par instrument, rendues une fois pour toutes depuis une banque General MIDI libre - la meme que le piano, la
# guitare, le saxo, la batterie et les bourdons. Voir assets/soundfonts/README.md pour la licence et la provenance, qui
# comptent autant que celle du code.
#
# ---------------------------------------------------------------------------------------------------------------------
# Pourquoi ces timbres-la, et pourquoi DOUX
#
# Roger, le 01/10/2026 : « pour les timbres, j'utilise que la guitare et le piano, les autres sont trop agressifs.
# N'hesite pas a rajouter des timbres doux. On veut se faire plaisir. »
#
# Un timbre AGRESSIF coute deux fois : une seconde mineure deja dissonante devient boueuse, et le joueur ferme
# l'application le soir. La liste ci-dessous est donc choisie pour sa DOUCEUR avant tout : une flute, un ensemble a cordes,
# une clarinette, un marimba et une harpe n'ont ni attaque dure ni harmonique qui gronde.
#
# ---------------------------------------------------------------------------------------------------------------------
# Ajouter ou retirer un instrument, sans rien casser
#
# L'APPLICATION ne connait que des NOMS : elle charge assets/soundfonts/<nom>_c4.wav et compagnie, avec la liste des notes
# ecrite dans le code (c2 a c6). Un instrument de plus, c'est donc quatre lignes a ajouter, et nulle part ailleurs :
#
#   1. une ligne dans INSTRUMENTS, ici, et le script ecrit ses cinq fichiers ;
#   2. son nom dans INSTRUMENT_NAMES (source/domain/audio/SampledInstrument.h) ;
#   3. son nom dans la liste du cablage (source/application/main.cpp) ;
#   4. ses cinq fichiers declares dans source/ui/resources.qrc.
#
# Et pour en RETIRER un : les memes quatre lignes, et ses fichiers disparaissent du paquet. Rien d'autre ne le mentionne -
# c'est ce qui permet de faire grossir ou maigrir l'application sans toucher a une seule ligne de musique.
#
# Usage:
#   ./scripts/render_instrument_samples.py /tmp/MuseScore_General.sf3
# =====================================================================================================================

import struct
import subprocess
import sys
import tempfile
import wave
from pathlib import Path

import numpy as np

SAMPLE_RATE = 48000
OUTPUT_DIRECTORY = Path(__file__).resolve().parent.parent / "assets" / "soundfonts"

# La duree CONSERVEE : plus longue que la plus longue note du jeu (une phrase tenue a tempo lent), et pas plus.
#
# La troncature n'est pas une precaution de poids, meme si elle sert aussi a ca : une harpe rendue par la banque dure onze
# secondes, pendant lesquelles elle reverbere. Le jeu, lui, coupe la note a la duree qu'il a demandee - tout ce qui depasse
# est du poids mort, et le projet est deja passe de 24 a 6 Mo en le comprenant.
KEPT_SECONDS = 2.2

# Le fondu de fin, plus long que celui des percussions : une note TENUE qui s'arrete net s'entend, et c'est un clic que
# personne n'a demande.
FADE_SECONDS = 0.12

PEAK = 0.85
VELOCITY = 96

# Les notes racines, et les noms de fichiers : ceux de TOUS les instruments du projet, sans exception.
ROOT_MIDI_NUMBERS = (36, 48, 60, 72, 84)
NOTE_NAMES = ("c2", "c3", "c4", "c5", "c6")

# Les nouveaux timbres, et leur programme General MIDI.
#
# Les programmes sont ceux de la norme : 73 la flute, 48 l'ensemble a cordes, 71 la clarinette, 12 le marimba, 46 la harpe.
INSTRUMENTS = {
    "flute": 73,
    "cordes": 48,
    "clarinette": 71,
    "marimba": 12,
    "harpe": 46,
}


def variable_length(value: int) -> bytes:
    """Un nombre a longueur variable, comme le veut le format MIDI."""
    chunks = [value & 0x7F]
    value >>= 7
    while value:
        chunks.append((value & 0x7F) | 0x80)
        value >>= 7
    return bytes(reversed(chunks))


def program_midi(program: int, note: int, duration_seconds: float, path: Path) -> None:
    """Un fichier MIDI type 0 : le programme voulu, la note tenue, puis son relachement.

    Le canal est le 1 (index 0), et non le 10 : la percussion est le seul endroit de la norme ou le numero de note designe
    un instrument. Ici, la note designe une hauteur.
    """
    ticks_per_beat = 480
    duration_ticks = int(ticks_per_beat * duration_seconds * 2)

    track = b""
    track += variable_length(0) + bytes([0xC0, program])
    track += variable_length(0) + bytes([0x90, note, VELOCITY])
    track += variable_length(duration_ticks) + bytes([0x80, note, 0])
    track += variable_length(0) + b"\xFF\x2F\x00"

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

    # Une stereo est MOYENNEE plutot qu'amputee d'un canal : garder un seul canal jetterait la moitie du son.
    return frames.mean(axis=1)


def write_wave(path: Path, samples: np.ndarray) -> None:
    clipped = np.clip(samples, -1.0, 1.0)
    data = (clipped * 32767.0).astype("<i2")

    with wave.open(str(path), "wb") as target:
        target.setnchannels(1)
        target.setsampwidth(2)
        target.setframerate(SAMPLE_RATE)
        target.writeframes(data.tobytes())


def main() -> None:
    if len(sys.argv) < 2:
        raise SystemExit(f"usage: {sys.argv[0]} <soundfont.sf3|sf2>")

    bank = Path(sys.argv[1])

    if not bank.exists():
        raise SystemExit(f"{bank}: introuvable")

    with tempfile.TemporaryDirectory() as directory:
        workDirectory = Path(directory)

        for instrumentName, program in INSTRUMENTS.items():
            for midiNumber, noteName in zip(ROOT_MIDI_NUMBERS, NOTE_NAMES):
                midi_path = workDirectory / "note.mid"
                raw_path = workDirectory / "note.wav"

                program_midi(program, midiNumber, KEPT_SECONDS, midi_path)

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

                samples = read_wave(raw_path)

                # TRONQUE a la duree du jeu, puis fondu : une harpe de onze secondes pese dix fois ce qu'elle apporte.
                kept = int(KEPT_SECONDS * SAMPLE_RATE)
                samples = samples[:kept]

                fade_length = min(len(samples), int(FADE_SECONDS * SAMPLE_RATE))

                if fade_length > 0:
                    samples[-fade_length:] *= np.linspace(1.0, 0.0, fade_length)

                peak = float(np.max(np.abs(samples))) if len(samples) else 0.0

                if peak > 0.0:
                    samples *= PEAK / peak

                output_path = OUTPUT_DIRECTORY / f"{instrumentName}_{noteName}.wav"
                write_wave(output_path, samples)

                print(f"{output_path.name}: {len(samples) / SAMPLE_RATE:.2f}s")


if __name__ == "__main__":
    main()
