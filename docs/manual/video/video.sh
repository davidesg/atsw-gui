#!/bin/bash
# EL VIDEO DEL MANUAL, con subtitulos, desde el programa de verdad:
#
#   bash docs/manual/video/video.sh            el caso del manual (IPC y WTI)
#   bash docs/manual/video/video.sh es-core    el univariante con intervenciones
#
# Sin argumento, los guiones de video/escenas/ y ATSW-ipc-wti.mp4. Con un
# nombre, los de video/escenas-NOMBRE/, su proyecto en /tmp/curso-NOMBRE/NOMBRE
# y ATSW-NOMBRE.mp4.
#
# Corre los guiones de video/escenas/ con capturas.sh en modo grabacion
# (VIDEO_DIR): cada escena sale en un .mp4 con sus rotulos en un .ass. Aqui
# se queman los subtitulos en cada trozo y se juntan en
# docs/manual/video/ATSW-ipc-wti.mp4 (o ATSW-NOMBRE.mp4). Necesita ffmpeg con libass y libx264.
#
# CON VOZ, de Piper o de Azure (ver voz.py):
#
#   VOZ_MODELO=/ruta/es_ES-sharvard-medium.onnx [PIPER=/ruta/piper] bash video.sh
#   VOZ_AZURE=es-ES-ElviraNeural bash video.sh     (clave y region: voz.py)
#
# Cada rotulo se sintetiza antes de grabar; al grabar, el rotulo siguiente
# espera a que acabe su frase (escenas.c), y aqui se mezclan las frases en
# cada trozo en el segundo en que empezo su rotulo. Sin VOZ_MODELO, el video
# sale mudo, como antes. Las frases se guardan en video/voz/ (fuera de git):
# cambiar un rotulo rehace solo la suya.
set -euo pipefail

# Ningun fichero de mas de 2 GB: un ffmpeg que no para no llena el disco.
ulimit -f 2097152
R=$(cd "$(dirname "$0")/../../.." && pwd)
V=$R/docs/manual/video
CASO=${1:-}
if [ -z "$CASO" ]; then
    ESC=$V/escenas SALIDA=$V/ATSW-ipc-wti.mp4
else
    ESC=$V/escenas-$CASO SALIDA=$V/ATSW-$CASO.mp4
    [ -d "$ESC" ] || { echo "no hay guiones en $ESC" >&2; exit 1; }
    export CURSO=/tmp/curso-$CASO PROYECTO_NOMBRE=$CASO
fi
D=$(mktemp -d "${TMPDIR:-/tmp}/atsw-video.XXXXXX")
trap 'rm -rf "$D"' EXIT

# Las fotos de las escenas de video no se quieren: a un directorio aparte.
FOTOS_VIDEO=$D/fotos
mkdir -p "$FOTOS_VIDEO"
if [ -n "${VOZ_AZURE:-}" ] || [ -n "${VOZ_MODELO:-}" ]; then
    # Una carpeta por voz: cambiar de voz no reutiliza las frases de la otra.
    if [ -n "${VOZ_AZURE:-}" ]; then
        VOZ_DIR=$V/voz/azure-$VOZ_AZURE${VOZ_VELOCIDAD:+-$VOZ_VELOCIDAD}
    else
        VOZ_DIR=$V/voz/$(basename "$VOZ_MODELO" .onnx)
    fi
    python3 "$V/voz.py" "$VOZ_DIR" "$ESC"/*.txt
    export VOZ_DIR
fi
ESCENAS=$ESC VIDEO_DIR=$D/trozos PASO_MS=${PASO_MS:-450} FOTOS=$FOTOS_VIDEO \
    LIMITE=${LIMITE:-900} bash "$R/docs/manual/capturas/capturas.sh"

: > "$D/lista.txt"
for t in "$D"/trozos/*.mp4; do
    [ -s "$t" ] || continue
    s=$D/$(basename "$t" .mp4)-sub.mp4
    # El sonido: las frases de su .voz, cada una en su segundo, o silencio.
    # Todos los trozos llevan pista de audio igual, para poder juntarlos.
    # La pista se corta a la duracion del trozo (dur), y no con -shortest:
    # con -filter_complex, -shortest no paraba una pista sin fin y ffmpeg
    # escribio hasta llenar el disco.
    dur=$(ffprobe -v error -show_entries format=duration -of csv=p=0 "$t")
    [ -n "$dur" ] || { echo "no se la duracion de $t" >&2; exit 1; }
    ent=() fil= mez= n=0
    if [ -s "$t.voz" ]; then
        while read -r seg wav; do
            n=$((n + 1))
            ent+=(-i "$wav")
            ms=$(awk -v s="$seg" 'BEGIN { printf "%d", s * 1000 }')
            fil+="[$n:a]aresample=48000,adelay=${ms}:all=1[v$n];"
            mez+="[v$n]"
        done < "$t.voz"
        fil+="${mez}amix=inputs=$n:normalize=0:dropout_transition=0,apad=whole_dur=$dur,atrim=0:$dur[a]"
    else
        ent+=(-f lavfi -t "$dur" -i anullsrc=r=48000:cl=mono)
        fil="[1:a]anull[a]"
    fi
    # ass= quiere la ruta escapada a su manera: se trabaja desde el directorio.
    ( cd "$D/trozos" && ffmpeg -loglevel error -y -i "$(basename "$t")" "${ent[@]}" \
          -filter_complex "[0:v]ass=$(basename "$t").ass[v];$fil" \
          -map "[v]" -map "[a]" -t "$dur" \
          -c:v libx264 -preset slow -crf 22 -pix_fmt yuv420p -r 25 \
          -c:a aac -b:a 96k -ar 48000 -ac 1 "$s" )
    echo "file '$s'" >> "$D/lista.txt"
done
ffmpeg -loglevel error -y -f concat -safe 0 -i "$D/lista.txt" -c copy "$SALIDA"
echo "hecho: $SALIDA ($(du -h "$SALIDA" | cut -f1))"
