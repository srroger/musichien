#!/usr/bin/env python3
# =====================================================================================================================
# Musichien - renders the drum kit samples
#
# The drum kit is PLAYED, not modelled. Like the piano, the guitar and the saxophone, the sounds are rendered once and
# for all from a free General MIDI bank, then committed as small mono files. See assets/soundfonts/README.md for the
# licence and the provenance, which matter as much as the licence of the code.
#
# The first version of the kit was SYNTHESISED - a falling sine for the kick, shaped noise for the snare. Roger heard
# it at once: "je les trouve un peu faible et un peu moche". He was right, and the reason is the same one that made
# the piano samples necessary: a real drum is a skin, a shell and a stick, and no amount of arithmetic makes a model
# of those. The synthesiser keeps its role as the FALLBACK when a sample is missing.
#
# Usage:
#   ./scripts/render_drum_samples.py /tmp/MuseScore_General.sf3
#
# The bank is NOT shipped: it weighs 40 MB, it is downloaded once, and the application never needs it.
# =====================================================================================================================

import struct
import subprocess
import sys
import tempfile
import wave
from pathlib import Path

import numpy as np

# General MIDI percussion, played on channel 10 - which is channel index 9 in the file.
#
# The note numbers ARE the instrument: this is the one place in MIDI where a pitch selects a drum rather than a
# frequency. 36 is the acoustic bass drum, 38 the acoustic snare, 42 the closed hi-hat, 47 the low-mid tom.
DRUMS = {
    "kick": (36, 0.9),
    "snare": (38, 0.9),
    "hihat": (42, 0.30),
    "tom": (47, 1.0),
}

# Le metronome : deux blocs de bois, l'aigu sur le premier temps et le grave sur les autres.
#
# Roger : "le son du temps fort, ok il est bien, mais les 2 autres font un peu moche". Un bloc de bois est LA sonorite
# du metronome - courte, sans hauteur musicale qui traine, et l'ecart aigu/grave dit ou est le premier temps sans qu'on
# ait a compter.
METRONOME = {
    "click_high": (76, 0.12),    # Hi Wood Block
    "click_low": (77, 0.12),     # Low Wood Block
}

VELOCITY = 110
OUTPUT_DIRECTORY = Path(__file__).resolve().parent.parent / "assets" / "soundfonts"
SAMPLE_RATE = 48000
PEAK = 0.9


def single_note_midi(note: int, duration_seconds: float, path: Path) -> None:
    """A type 0 MIDI file holding one percussion note, and nothing else."""
    ticks_per_beat = 480
    duration_ticks = int(ticks_per_beat * duration_seconds * 2)  # two beats a second

    def variable_length(value: int) -> bytes:
        chunks = [value & 0x7F]
        value >>= 7
        while value:
            chunks.append((value & 0x7F) | 0x80)
            value >>= 7
        return bytes(reversed(chunks))

    track = b""
    track += variable_length(0) + bytes([0x90 | 9, note, VELOCITY])      # note on, channel 10
    track += variable_length(duration_ticks) + bytes([0x80 | 9, note, 0])  # note off
    track += variable_length(0) + b"\xFF\x2F\x00"                          # end of track

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

    # A stereo rendering is averaged rather than half discarded: keeping one channel would drop half the sound, and a
    # drum is not panned in this application.
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
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)

    bank = Path(sys.argv[1])
    if not bank.is_file():
        raise SystemExit(f"sound bank not found: {bank}")

    OUTPUT_DIRECTORY.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory() as work_directory:
        work = Path(work_directory)

        for name, (note, kept_seconds) in (DRUMS | METRONOME).items():
            midi_path = work / f"{name}.mid"
            raw_path = work / f"{name}_raw.wav"
            output_path = OUTPUT_DIRECTORY / f"drum_{name}.wav"

            single_note_midi(note, kept_seconds, midi_path)

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

            # Trimmed to the length the drum really rings, then faded: a buffer that stops dead adds a click of its
            # own, which is the artefact the fade exists to remove.
            kept = int(kept_seconds * SAMPLE_RATE)
            samples = samples[:kept]

            fade_length = min(len(samples), int(0.03 * SAMPLE_RATE))
            if fade_length > 0:
                samples[-fade_length:] *= np.linspace(1.0, 0.0, fade_length)

            # A drum is SHORT: at equal peak it sounds quieter than a held note, so it is normalised as high as it can
            # go without clipping.
            peak = float(np.max(np.abs(samples))) if len(samples) else 0.0
            if peak > 0.0:
                samples *= PEAK / peak

            write_wave(output_path, samples)

            print(f"{output_path.name}: {kept / SAMPLE_RATE:.2f} s, {output_path.stat().st_size} bytes")


if __name__ == "__main__":
    main()
