#!/bin/bash
# LA PARTE DE LA TERMINAL del video del asistente, grabada de verdad:
#
#   bash docs/manual/video/terminal/graba.sh        -> docs/manual/video/ATSW-asistente-terminal.mp4
#
# Sale el borrador, con los rotulos de terminal.guion y mudo. La voz va
# despues, con voz.sh, sobre la toma que se quede (narracion.txt).
#
# 1. proyecto.sh rehace el proyecto de partida en /tmp/curso-terminal
#    (instala el asistente: necesita la red).
# 2. capturas.sh graba la madre con madre.txt; la escena lanza graba.py,
#    que abre una terminal en el MISMO broadwayd, corre en ella el cliente
#    (claude, o CLIENTE) y graba lo que se ve con terminal.guion.
# 3. Aqui se juntan: la madre a la izquierda y la terminal a la derecha,
#    alineadas por la hora de reloj, con los rotulos de la terminal.
#
# El cliente usa la cuenta de quien graba. Lo que no debe salir en la
# grabacion se dice en VIGILA, separado por comas (la direccion de la
# cuenta, el nombre de la carpeta del usuario): graba.py para si la terminal
# lo ensena. No va en ningun fichero del repositorio.
set -euo pipefail
ulimit -f 2097152
R=$(cd "$(dirname "$0")/../../../.." && pwd)
V=$R/docs/manual/video
TV=$V/terminal
SALIDA=${SALIDA:-$V/ATSW-asistente-terminal.mp4}
TERMINAL_CLIENTE=${CLIENTE:-$(command -v claude || true)}
[ -x "$TERMINAL_CLIENTE" ] || { echo "no encuentro el cliente (claude): CLIENTE=/ruta/al/cliente" >&2; exit 1; }
export TERMINAL_CLIENTE VIGILA=${VIGILA:-$HOME}
CURSO=/tmp/curso-terminal

[ -n "${REUSA_PROYECTO:-}" ] || bash "$TV/proyecto.sh" >/dev/null
P=$CURSO/asistente
G=$CURSO/terminal
rm -rf "$G"; mkdir -p "$G"
D=$(mktemp -d "${TMPDIR:-/tmp}/atsw-terminal.XXXXXX")
trap 'rm -rf "$D"' EXIT

CURSO=$CURSO PROYECTO_NOMBRE=asistente REUSA=1 CONSERVA=1 ESCENAS=$TV \
    VIDEO_DIR=$D/trozos FOTOS=$D PASO_MS=${PASO_MS:-450} PANTALLA=${PANTALLA:-50} \
    bash "$R/docs/manual/capturas/capturas.sh" "$TV/madre.txt"

[ "$(cat "$G/terminal.mp4.fin" 2>/dev/null)" = ok ] ||
    { echo "la terminal no acabo bien: $G/terminal.mp4.log y $G/graba.err" >&2; exit 1; }

# LA ALINEACION. La madre cuenta su tiempo en cuadros (escenas.c) y la
# terminal en reloj; el rotulo SINCRONIA dice en que segundo del video de la
# madre se lanzo la terminal, y madre.t0 a que hora fue. La terminal empieza
# a grabar despues (su calentamiento): su .t0 dice cuando.
M=$D/trozos/madre.mp4
sinc=$(sed -n 's/^Dialogue: 0,\([^,]*\),.*,SINCRONIA$/\1/p' "$M.ass" |
       awk -F: '{ print $1 * 3600 + $2 * 60 + $3 }')
desde=$(awk -v s="$sinc" -v a="$(cat "$G/madre.t0")" -v b="$(cat "$G/terminal.mp4.t0")" \
        'BEGIN { printf "%.2f", s + (b - a) }')
dur=$(ffprobe -v error -show_entries format=duration -of csv=p=0 "$G/terminal.mp4")

# EL LIENZO: 1920x1080 como las escenas. La madre (en su lienzo, a 1257x880
# desde x=331) recortada y a 1000 de ancho; la terminal, 880x880 a la derecha.
# La toma, sin rotulos, queda en $TOMA: la voz se le pone despues (voz.sh),
# sobre lo que el LLM contesto de verdad. El borrador lleva los rotulos de
# terminal.guion quemados.
TOMA=${TOMA:-$V/ATSW-asistente-terminal-toma.mp4}
( cd "$D" && ffmpeg -loglevel error -y -ss "$desde" -i "$M" -i "$G/terminal.mp4" \
    -f lavfi -i "color=c=0x1e3a66:s=1920x1080:r=10:d=$dur" \
    -filter_complex "[0:v]crop=1290:905:316:12,scale=1000:-2,tpad=stop_mode=clone:stop_duration=$dur[m];\
[2:v][m]overlay=10:(930-h)/2:shortest=1[a];[a][1:v]overlay=1025:25:shortest=1[v]" \
    -map "[v]" -t "$dur" -c:v libx264 -preset slow -crf 18 -pix_fmt yuv420p -r 25 "$TOMA" )
cp "$G/terminal.mp4.ass" "$D/t.ass"
( cd "$D" && ffmpeg -loglevel error -y -i "$TOMA" -f lavfi -t "$dur" -i anullsrc=r=48000:cl=mono \
    -vf ass=t.ass -map 0:v -map 1:a -t "$dur" -c:v libx264 -preset slow -crf 22 -pix_fmt yuv420p \
    -r 25 -c:a aac -b:a 96k -ar 48000 -ac 1 "$SALIDA" )
echo "hecho: $SALIDA ($(du -h "$SALIDA" | cut -f1)); la toma, sin rotulos: $TOMA"
