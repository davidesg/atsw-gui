#!/bin/bash
# atsw_gui --ver (docs/ESTUDIO-terminal-mcp.md §7.4): la ventana suelta que
# abre el asistente. Sobre el ejemplo de Jenkins y Alavi (docs/ejemplos), en
# una copia:
#   - cada ventana se abre, el modulo de cierre de la CI (.github/cierre.c) la
#     cierra como lo haria el usuario, y el proceso sale solo y con 0;
#   - es de SOLO LECTURA: el manifiesto queda igual, byte a byte;
#   - lo que no se puede abrir se dice antes de abrir nada: estado 2 si la
#     orden esta mal escrita, 3 si lo pedido no esta.
#   sh tests/ver.sh       (desde gui/atsw, con atsw_gui hecho y una pantalla)
TOP=$(cd "$(dirname "$0")/.." && pwd)
R=$(cd "$TOP/../.." && pwd)
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) EXE=.exe; MOD=cierre.dll ;; *) EXE=; MOD=cierre.so ;; esac
G="$TOP/atsw_gui$EXE"
W=$(mktemp -d 2>/dev/null || mktemp -d -t atswver)
trap 'rm -rf "$W"' EXIT INT TERM
fallos=0
ok()  { echo "  ok    $1"; }
mal() { echo "  FALLA $1"; fallos=$((fallos + 1)); }

cp -R "$R/docs/ejemplos/jenkins_alavi" "$W/p"
Y="$W/p/proyecto.yaml"
cc -shared -fPIC -O2 -o "$W/$MOD" "$R/.github/cierre.c" $(pkg-config --cflags --libs gtk+-3.0) \
    > "$W/cierre.log" 2>&1 || { echo "FAIL: no se pudo compilar el modulo de cierre"; cat "$W/cierre.log"; exit 1; }
M="$W/$MOD"; case "$(uname -s)" in MINGW*|MSYS*) M=$(cygpath -m "$M") ;; esac
antes=$(cksum < "$Y")

echo "atsw_gui --ver"
for v in "serie MINK" "media-dt MINK" "histograma MUSKRAT" "residuos MINK/m01" "diagnosis MUSKRAT"; do
    set -- $v
    GTK_MODULES="$M" CIERRE=4 "$G" --proyecto "$Y" --ver "$1" "$2" > "$W/$1.log" 2>&1 &
    pid=$!; t=0
    while kill -0 "$pid" 2>/dev/null && [ $t -lt 60 ]; do sleep 1; t=$((t + 1)); done
    if kill -0 "$pid" 2>/dev/null; then
        kill "$pid"; mal "--ver $1 $2: no sale al cerrar su ventana"
    else
        wait "$pid"; rc=$?
        if [ $rc -eq 0 ] && grep -q "cierre: cierro" "$W/$1.log"; then
            ok "--ver $1 $2: se abre su ventana, se cierra y sale con 0"
        else
            mal "--ver $1 $2: rc=$rc; $(tr '\n' ' ' < "$W/$1.log")"
        fi
    fi
done
[ "$antes" = "$(cksum < "$Y")" ] && ok "solo lectura: el manifiesto queda igual" \
                                  || mal "el manifiesto ha cambiado"

"$G" --proyecto "$Y" --ver otra MINK > /dev/null 2>&1; rc=$?
[ $rc -eq 2 ] && ok "una ventana que no existe: estado 2" || mal "una ventana que no existe: rc=$rc"
"$G" --ver serie MINK > /dev/null 2>&1; rc=$?
[ $rc -eq 2 ] && ok "sin --proyecto: estado 2" || mal "sin --proyecto: rc=$rc"
"$G" --proyecto "$Y" --ver serie OTRA > /dev/null 2>&1; rc=$?
[ $rc -eq 3 ] && ok "una serie que no está: estado 3" || mal "una serie que no está: rc=$rc"
"$G" --proyecto "$Y" --ver residuos MINK/m09 > /dev/null 2>&1; rc=$?
[ $rc -eq 3 ] && ok "un modelo que no está: estado 3" || mal "un modelo que no está: rc=$rc"

[ $fallos -eq 0 ] && echo "atsw_gui --ver: todo bien" || echo "atsw_gui --ver: $fallos fallos"
exit $fallos
