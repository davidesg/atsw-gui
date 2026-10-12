#!/bin/bash
# EL VIDEO DEL MANUAL, con subtitulos, desde el programa de verdad:
#
#   bash docs/manual/video/video.sh
#
# Corre los guiones de video/escenas/ con capturas.sh en modo grabacion
# (VIDEO_DIR): cada escena sale en un .mp4 con sus rotulos en un .ass. Aqui
# se queman los subtitulos en cada trozo y se juntan en
# docs/manual/video/ATSW-ipc-wti.mp4. Necesita ffmpeg con libass y libx264.
set -euo pipefail

R=$(cd "$(dirname "$0")/../../.." && pwd)
V=$R/docs/manual/video
D=$(mktemp -d "${TMPDIR:-/tmp}/atsw-video.XXXXXX")
trap 'rm -rf "$D"' EXIT

# Las fotos de las escenas de video no se quieren: a un directorio aparte.
FOTOS_VIDEO=$D/fotos
mkdir -p "$FOTOS_VIDEO"
ESCENAS=$V/escenas VIDEO_DIR=$D/trozos PASO_MS=${PASO_MS:-450} FOTOS=$FOTOS_VIDEO \
    LIMITE=${LIMITE:-900} bash "$R/docs/manual/capturas/capturas.sh"

: > "$D/lista.txt"
for t in "$D"/trozos/*.mp4; do
    [ -s "$t" ] || continue
    s=$D/$(basename "$t" .mp4)-sub.mp4
    # ass= quiere la ruta escapada a su manera: se trabaja desde el directorio.
    ( cd "$D/trozos" && ffmpeg -loglevel error -y -i "$(basename "$t")" \
          -vf "ass=$(basename "$t").ass" -c:v libx264 -preset slow -crf 22 \
          -pix_fmt yuv420p -r 25 "$s" )
    echo "file '$s'" >> "$D/lista.txt"
done
ffmpeg -loglevel error -y -f concat -safe 0 -i "$D/lista.txt" -c copy "$V/ATSW-ipc-wti.mp4"
echo "hecho: $V/ATSW-ipc-wti.mp4 ($(du -h "$V/ATSW-ipc-wti.mp4" | cut -f1))"
