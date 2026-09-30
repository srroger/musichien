#!/bin/bash
# =====================================================================================================================
# Musichien - compresse les phrases de l'atelier en OGG Vorbis
#
#   Usage:  scripts/compress_phrases.sh [dossier]
#
# Roger a demande si les WAV pouvaient etre compresses : « la musique n'est pas hyper riche, on ne devrait pas avoir
# besoin d'une resolution de malade ». Il a raison, et l'OGG divise le poids par DIX la ou le WAV a 22050 Hz ne le divise
# que par deux - pour des phrases de quelques secondes, la difference s'entend a peine.
#
# Les WAV ne sont PAS supprimes : c'est l'oreille qui doit dire si la compression tient, et l'index.html essaie l'OGG
# avant de retomber sur le WAV. Le jour ou l'on est sur que ca tient, on supprime les WAV et l'atelier tient dans
# quelques megaoctets.
# =====================================================================================================================

set -euo pipefail

if ! command -v ffmpeg > /dev/null; then
    echo "ffmpeg introuvable : il est necessaire pour encoder en OGG."
    exit 1
fi

DIRECTORY="${1:-${HOME}/Musichien-atelier/phrases}"

if [ ! -d "${DIRECTORY}" ]; then
    echo "dossier introuvable : ${DIRECTORY}"
    exit 1
fi

before=$(du -sk "${DIRECTORY}" | cut -f1)

for file in "${DIRECTORY}"/*.wav; do
    [ -e "${file}" ] || continue

    ffmpeg -y -loglevel error -i "${file}" -c:a libvorbis -q:a 4 "${file%.wav}.ogg"
done

after=$(du -sk "${DIRECTORY}" | cut -f1)

echo "Musichien: ${DIRECTORY}"
echo "  avant : $((before / 1024)) Mo"
echo "  apres : $((after / 1024)) Mo (WAV conserves)"
echo "  ouvrir l'index.html : il essaie l'OGG, et retombe sur le WAV."
