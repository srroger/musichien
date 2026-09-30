#!/usr/bin/env python3
# =====================================================================================================================
# Musichien - renders the dog's bark
#
# Le petit wouf du chien qui raconte une anecdote. Contrairement aux autres sons du projet, celui-ci est SYNTHETISE : une
# banque General MIDI n'a pas de chien, et un echantillon trouve sur le web viendrait avec une licence qu'on ne peut pas
# verifier. Celui-ci est donc fait de bouts d'arithmetique, et il a le meme role que la synthese de secours de
# l'application : il est la quand rien de mieux n'existe.
#
# La recette est celle d'une voix, pas d'un instrument : une SOURCE (un bourdon d'harmoniques, la vibration) traversant
# un FILTRE (deux formants, la bouche). Pour un « ou », le premier formant est bas et le second proche : c'est ce qui fait
# entendre une bouche arrondie plutot qu'un « a ». Le glissement descendant d'une demi-octave fait le reste - un chien ne
# tient pas une note, il la lache.
#
# Usage:
#   ./scripts/render_dog_bark.py
# =====================================================================================================================

import struct
import wave
from pathlib import Path

import numpy as np

SAMPLE_RATE = 48000
OUTPUT_PATH = Path(__file__).resolve().parent.parent / "assets" / "soundfonts" / "dog_bark.wav"

DURATION_SECONDS = 0.26
PEAK = 0.7


def resonator(samples: np.ndarray, frequency: float, bandwidth: float) -> np.ndarray:
    """Un formant : le filtre a deux poles qui donne sa forme a une voyelle."""
    radius = np.exp(-np.pi * bandwidth / SAMPLE_RATE)
    angle = 2.0 * np.pi * frequency / SAMPLE_RATE

    a1 = 2.0 * radius * np.cos(angle)
    a2 = -(radius * radius)

    output = np.zeros_like(samples)
    previous = 0.0
    before_previous = 0.0

    for index, sample in enumerate(samples):
        value = sample + (a1 * previous) + (a2 * before_previous)
        output[index] = value
        before_previous = previous
        previous = value

    return output


def main() -> None:
    time = np.linspace(0.0, DURATION_SECONDS, int(SAMPLE_RATE * DURATION_SECONDS), endpoint=False)

    # La hauteur tombe doucement - 430 a 270 Hz - et legerement vite au debut : c'est ce qui fait « wou » plutot que
    # « iiiiii ».
    pitch = 270.0 + (160.0 * np.exp(-3.0 * time))
    phase = 2.0 * np.pi * np.cumsum(pitch) / SAMPLE_RATE

    # La SOURCE : un bourdon d'harmoniques decroissantes, comme des cordes vocales. Le 1/n est ce qui donne le timbre
    # rond d'une voix plutot que le tranchant d'une onde carree.
    source = np.zeros_like(time)

    for harmonic in range(1, 16):
        source += (1.0 / harmonic) * np.sin(harmonic * phase)

    # Un souffle bref au tout debut : le petit « w » d'attaque, qui est un bruit, pas une hauteur.
    noise = np.random.default_rng(20260930).normal(0.0, 1.0, time.size)
    noise = np.convolve(noise, np.ones(16) / 16.0, mode="same")
    source = source + (0.35 * noise * np.exp(-time / 0.020))

    # Le FILTRE : deux formants de « ou », plus une bande haute tres sourde pour que le chien ne soit pas une flute.
    filtered = resonator(source, 320.0, 90.0) + (0.5 * resonator(source, 850.0, 140.0))

    # L'ENVELOPPE : une attaque de 12 ms et une chute courte. Un aboiement ne dure pas, et c'est la chute qui le rend
    # mignon plutot qu'agressif.
    envelope = np.minimum(time / 0.012, 1.0) * np.exp(-time / 0.085)

    # Et la FIN est coupee en douceur : sans ce fondu, la chute du signal s'entend comme un clic.
    fade_length = 400
    envelope[-fade_length:] *= np.linspace(1.0, 0.0, fade_length)

    bark = filtered * envelope

    bark -= bark.mean()
    bark /= np.abs(bark).max()
    bark *= PEAK

    samples = np.clip(bark * 32767.0, -32768.0, 32767.0).astype("<i2")

    with wave.open(str(OUTPUT_PATH), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(SAMPLE_RATE)
        output.writeframes(struct.pack(f"<{samples.size}h", *samples))

    print(f"{OUTPUT_PATH.name}: {DURATION_SECONDS:.2f}s, {SAMPLE_RATE} Hz, peak {PEAK}")


if __name__ == "__main__":
    main()
