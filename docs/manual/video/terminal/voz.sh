#!/bin/bash
# LA VOZ DE LA PARTE DE LA TERMINAL, sobre una toma ya grabada (graba.sh):
#
#   VOZ_AZURE=es-ES-AlvaroNeural bash docs/manual/video/terminal/voz.sh [TOMA]
#       -> docs/manual/video/ATSW-asistente-terminal.mp4
#
# TOMA es por defecto la que deja graba.sh, sin rotulos. A una toma que ya
# los lleve quemados (el borrador) se le tapan con TAPA=906: la franja de
# abajo, debajo de la terminal, se pinta con el fondo.
#
# Las frases (narracion.txt) se sintetizan con voz.py, como las de las
# escenas, y narra.py las pone en la toma: si la voz necesita mas tiempo que
# la toma, la imagen se congela en el ancla. La narracion describe lo que
# contesto el LLM en ESA toma: otra toma, otra narracion.
set -euo pipefail
ulimit -f 2097152
R=$(cd "$(dirname "$0")/../../../.." && pwd)
V=$R/docs/manual/video
TOMA=${1:-$V/ATSW-asistente-terminal-toma.mp4}
SALIDA=${SALIDA:-$V/ATSW-asistente-terminal.mp4}
[ -s "$TOMA" ] || { echo "no hay toma: $TOMA (graba.sh)" >&2; exit 1; }
[ "$(realpath "$TOMA")" != "$(realpath -m "$SALIDA")" ] || { echo "la toma y la salida son el mismo fichero" >&2; exit 1; }
[ -n "${VOZ_AZURE:-}" ] || [ -n "${VOZ_MODELO:-}" ] || { echo "falta VOZ_AZURE o VOZ_MODELO (voz.py)" >&2; exit 1; }
if [ -n "${VOZ_AZURE:-}" ]; then
    VOZ_DIR=$V/voz/azure-$VOZ_AZURE${VOZ_VELOCIDAD:+-$VOZ_VELOCIDAD}
else
    VOZ_DIR=$V/voz/$(basename "$VOZ_MODELO" .onnx)
fi
python3 "$V/voz.py" "$VOZ_DIR" "$V/terminal/narracion.txt"
D=$(mktemp -d "${TMPDIR:-/tmp}/atsw-narra.XXXXXX")
trap 'rm -rf "$D"' EXIT
python3 "$V/terminal/narra.py" "$TOMA" "$V/terminal/narracion.txt" "$VOZ_DIR" "$D/v.mp4" ${TAPA:+--tapa "$TAPA"}
mv "$D/v.mp4" "$SALIDA"
echo "hecho: $SALIDA ($(du -h "$SALIDA" | cut -f1))"
