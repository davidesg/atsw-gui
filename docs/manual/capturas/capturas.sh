#!/bin/bash
# LAS CAPTURAS DEL MANUAL, otra vez y desde el programa de verdad:
#
#   bash docs/manual/capturas/capturas.sh [escena ...]
#
# Necesita el arbol construido (make) y broadwayd (Debian/Ubuntu: viene con
# libgtk-3-bin). Cada escena es un guion escenas/NN-nombre.txt (las ordenes,
# en escenas.c); su primera linea dice que programa lanza:
#
#   #! atsw_gui --proyecto @PROYECTO@/proyecto.yaml
#
# Las escenas HACEN el caso del manual desde cero, como lo haria un alumno:
# 01 crea el proyecto en @PROYECTO@ (un directorio temporal) con los datos
# de @DATOS@ (docs/manual/datos/), 02 identifica y estima el IPC, y asi. Se
# corren en orden y cada una parte de lo que dejo la anterior: lo que se ve
# en las fotos es lo que el programa calcula hoy.
#
# Las fotos quedan en docs/manual/img/. Todo va por GDK_BACKEND=broadway: las
# ventanas se dibujan fuera de la pantalla.
set -euo pipefail

R=$(cd "$(dirname "$0")/../../.." && pwd)
M=$R/docs/manual
# Un sitio fijo y corto, porque la ruta del proyecto sale en las capturas.
# CURSO y PROYECTO_NOMBRE los cambia el video de otro caso (video.sh es-core).
T=${CURSO:-/tmp/curso-atsw}
P=$T/${PROYECTO_NOMBRE:-ipc-wti}
# Una sola vuelta a la vez: el directorio es fijo, y una vuelta que acaba lo
# borra (el trap de abajo) aunque otra lo este usando.
exec 9>"$T.lock"
flock -n 9 || { echo "ya hay otra vuelta de capturas en marcha" >&2; exit 1; }
# REUSA=1 parte de lo que dejo la vuelta anterior (con CONSERVA=1): para
# escribir una escena sin repetir todas las de antes.
[ -n "${REUSA:-}" ] || { rm -rf "$T"; mkdir -p "$P"; }
# El broadwayd de cada escena se para por su PID: solo el que se lanzo aqui.
BROADWAY_PID=
para_broadway() { [ -z "$BROADWAY_PID" ] || kill "$BROADWAY_PID" 2>/dev/null || true; BROADWAY_PID=; }
trap 'para_broadway; [ -n "${CONSERVA:-}" ] || rm -rf "$T"' EXIT

PANTALLA=${PANTALLA:-7}
mkdir -p "$M/img" "$T/cache"

cc -shared -fPIC -O2 -Wall -o "$T/libescenas.so" "$M/capturas/escenas.c" \
    $(pkg-config --cflags --libs gtk+-3.0)


# Los programas, todos en un directorio como en los paquetes (packaging/):
# asi se encuentran entre ellos igual que en una instalacion, y en las
# capturas no sale la ruta del arbol de construccion.
. "$R/packaging/programas.sh"
mkdir -p "$T/ATSW/bin"
for p in $PROGRAMAS; do cp "$R/$p" "$T/ATSW/bin/"; done
export PATH="$T/ATSW/bin:$PATH"
export GDK_BACKEND=broadway BROADWAY_DISPLAY=":$PANTALLA" GTK_THEME=Adwaita
export XDG_CACHE_HOME=$T/cache XDG_CONFIG_HOME=$T/config XDG_DATA_HOME=$T/data NO_AT_BRIDGE=1 FOTOS=${FOTOS:-$M/img} LC_ALL=${IDIOMA:-C.UTF-8}

# ESCENAS: de donde salen los guiones (el video tiene los suyos). VIDEO_DIR:
# ademas de fotografiar, graba cada escena en VIDEO_DIR/<escena>.mp4 con sus
# subtitulos al lado (.mp4.ass); video.sh los junta.
ESCENAS=${ESCENAS:-$M/capturas/escenas}
escenas=("$@")
[ ${#escenas[@]} -gt 0 ] || escenas=("$ESCENAS"/*.txt)

for e in "${escenas[@]}"; do
    [ -f "$e" ] || e=$ESCENAS/$e
    if [ -n "${VIDEO_DIR:-}" ]; then
        mkdir -p "$VIDEO_DIR"
        export VIDEO="$VIDEO_DIR/$(basename "$e" .txt).mp4"
    fi
    linea=$(head -1 "$e")
    case "$linea" in '#! '*) ;; *) echo "$e: falta la linea #! programa" >&2; exit 1 ;; esac
    cmd=${linea#'#! '}
    cmd=${cmd//@PROYECTO@/$P}
    cmd=${cmd//@DATOS@/$M/datos}
    sed -e "s|@PROYECTO@|$P|g" -e "s|@DATOS@|$M/datos|g" "$e" > "$T/guion.txt"
    echo "== $(basename "$e")"
    # Un broadwayd para cada escena: si un cliente sale con ventanas
    # abiertas, broadwayd a veces se cae (client_free: assertion failed).
    # Y en otra pantalla: la anterior tarda en soltar su socket.
    para_broadway
    PANTALLA=$((PANTALLA + 1))
    export BROADWAY_DISPLAY=":$PANTALLA"
    broadwayd ":$PANTALLA" >>"$T/broadwayd.log" 2>&1 &
    BROADWAY_PID=$!
    sleep 1
    ( cd "$T" && GTK_MODULES=$T/libescenas.so ESCENA=$T/guion.txt \
          timeout "${LIMITE:-300}" "$T/ATSW/bin/"$cmd ) 2>&1 | grep -v "^$" | grep -v "pixman\|preferred_height_for_width\|Negative content\|BUG \*\*\*\|AccelLabel\|queue_draw_area\|Aborted" || true
done
