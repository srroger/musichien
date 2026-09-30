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
# Le script en ecrit QUATRE, et l'application les fait tourner : le chien a des humeurs, et deux parties de suite ne
# s'ouvrent pas sur la meme phrase.
#
# Usage:
#   ./scripts/render_dog_bark.py
# =====================================================================================================================

import struct
import wave
from pathlib import Path

import numpy as np

SAMPLE_RATE = 48000
OUTPUT_DIRECTORY = Path(__file__).resolve().parent.parent / "assets" / "soundfonts"
PEAK = 0.7

# QUATRE aboiements, pas un seul. Le chien ne repete pas la meme phrase : il a des humeurs, et quatre woufs qui tournent
# suffisent a ce qu'on ne l'entende jamais deux fois de suite dire exactement la meme chose. Ils sont tous batis sur la
# meme recette - c'est ce qui fait qu'on reconnait LE chien, et non quatre chiens differents.
#
# Les quatre tiennent dans une fourchette etroite, et c'est voulu : l'humain a dit que le premier etait « tout doux »,
# donc les autres ne doivent pas etre plus mordants, seulement un peu autres. Un chien qui aboie trop fort n'est plus
# mignon, il est derangeant.
#
#   name            start  end   formants        duration  decay  breath  echo
#                   (Hz)   (Hz)  F1 / F2         (s)       (s)    (x)     (s, 0 = aucun)
VARIANTS = [
    {
        "name": "dog_bark",
        "start_pitch": 430.0,
        "end_pitch": 270.0,
        "first_formant": 320.0,
        "second_formant": 850.0,
        "duration": 0.26,
        "decay": 0.085,
        "breath": 0.35,
        "second_bark_at": 0.0,
    },
    {
        # Un peu plus clair, un peu plus court : le « wif » d'un chien content, qui monte la gamme et se depêche.
        "name": "dog_bark_2",
        "start_pitch": 490.0,
        "end_pitch": 320.0,
        "first_formant": 380.0,
        "second_formant": 1000.0,
        "duration": 0.22,
        "decay": 0.070,
        "breath": 0.45,
        "second_bark_at": 0.0,
    },
    {
        # Plus grave et plus long : il a une histoire a raconter, alors il prend son temps. La chute est deux fois plus
        # lente, ce qui suffit a changer completement le caractere sans toucher a la hauteur.
        "name": "dog_bark_3",
        "start_pitch": 360.0,
        "end_pitch": 225.0,
        "first_formant": 285.0,
        "second_formant": 760.0,
        "duration": 0.33,
        "decay": 0.120,
        "breath": 0.25,
        "second_bark_at": 0.0,
    },
    {
        # Et celui qui aboie DEUX fois : une petite reprise d'amplitude a mi-parcours, comme un chien qui insiste. C'est
        # le plus vivant des quatre, et il ne dure pas plus longtemps pour autant.
        "name": "dog_bark_4",
        "start_pitch": 450.0,
        "end_pitch": 290.0,
        "first_formant": 340.0,
        "second_formant": 900.0,
        "duration": 0.34,
        "decay": 0.075,
        "breath": 0.40,
        "second_bark_at": 0.16,
    },
]


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


def render_bark(variant: dict) -> np.ndarray:
    """Fabrique UN aboiement, et rend ses echantillons prets a etre ecrits dans un fichier."""
    time = np.linspace(0.0, variant["duration"], int(SAMPLE_RATE * variant["duration"]), endpoint=False)

    # La hauteur tombe doucement - de « start » a « end » - et legerement vite au debut : c'est ce qui fait « wou »
    # plutot que « iiiiii ». L'ecart entre les deux hauteurs est ce qui donne a chaque variante sa voix.
    span = variant["start_pitch"] - variant["end_pitch"]
    pitch = variant["end_pitch"] + (span * np.exp(-3.0 * time))
    phase = 2.0 * np.pi * np.cumsum(pitch) / SAMPLE_RATE

    # La SOURCE : un bourdon d'harmoniques decroissantes, comme des cordes vocales. Le 1/n est ce qui donne le timbre
    # rond d'une voix plutot que le tranchant d'une onde carree.
    source = np.zeros_like(time)

    for harmonic in range(1, 16):
        source += (1.0 / harmonic) * np.sin(harmonic * phase)

    # Un souffle bref au tout debut : le petit « w » d'attaque, qui est un bruit, pas une hauteur.
    noise = np.random.default_rng(20260930).normal(0.0, 1.0, time.size)
    noise = np.convolve(noise, np.ones(16) / 16.0, mode="same")
    source = source + (variant["breath"] * noise * np.exp(-time / 0.020))

    # Le FILTRE : deux formants de « ou », plus une bande haute tres sourde pour que le chien ne soit pas une flute.
    filtered = resonator(source, variant["first_formant"], 90.0) + (
        0.5 * resonator(source, variant["second_formant"], 140.0)
    )

    # L'ENVELOPPE : une attaque de 12 ms et une chute courte. Un aboiement ne dure pas, et c'est la chute qui le rend
    # mignon plutot qu'agressif.
    envelope = np.minimum(time / 0.012, 1.0) * np.exp(-time / variant["decay"])

    # La quatrieme variante aboie DEUX fois : une seconde attaque, plus courte et un peu moins forte, relance
    # l'enveloppe a mi-parcours. On prend le maximum des deux, jamais la somme : additionner ferait clipper le milieu.
    if variant["second_bark_at"] > 0.0:
        after_second = np.maximum(time - variant["second_bark_at"], 0.0)
        second_bark = 0.8 * np.minimum(after_second / 0.010, 1.0) * np.exp(-after_second / 0.070)
        envelope = np.maximum(envelope, second_bark)

    # Et la FIN est coupee en douceur : sans ce fondu, la chute du signal s'entend comme un clic.
    fade_length = 400
    envelope[-fade_length:] *= np.linspace(1.0, 0.0, fade_length)

    bark = filtered * envelope

    bark -= bark.mean()
    bark /= np.abs(bark).max()
    bark *= PEAK

    return np.clip(bark * 32767.0, -32768.0, 32767.0).astype("<i2")


def main() -> None:
    for variant in VARIANTS:
        samples = render_bark(variant)
        output_path = OUTPUT_DIRECTORY / f"{variant['name']}.wav"

        with wave.open(str(output_path), "wb") as output:
            output.setnchannels(1)
            output.setsampwidth(2)
            output.setframerate(SAMPLE_RATE)
            output.writeframes(struct.pack(f"<{samples.size}h", *samples))

        print(f"{output_path.name}: {variant['duration']:.2f}s, {SAMPLE_RATE} Hz, peak {PEAK}")


if __name__ == "__main__":
    main()

