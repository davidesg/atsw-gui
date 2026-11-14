#!/bin/bash
# LA MADRE QUE SE CAE Y LA QUE SE DESPIDE (lib/sitio, AUDITORIA-procesos B4).
# Un shell hace de madre: lanza fue_gui con «--madre $$» y se va. Si se va SIN
# DESPEDIRSE (se cayo), fue_gui no se cierra --para que se guarde lo que
# haya-- y lo dice; si se despide (atsw-madre-PID.adios en el temporal, lo que
# deja la madre al cerrarse bien), fue_gui se cierra con ella y sale con 0.
#
# Solo en Linux y macOS: en Windows el $$ del shell no es el pid de Windows
# que vigila el hijo.
case "$(uname -s)" in MINGW*|MSYS*) echo "(en Windows no se prueba aqui)"; exit 0 ;; esac
R=$(pwd)
G="$R/gui/fue/bin/fue_gui"
T=${TMPDIR:-/tmp}
[ -x "$G" ] || { echo "::error::fue_gui no esta construido"; exit 1; }
mkdir -p capturas
fallos=0

# 1. SIN DESPEDIDA: la madre se va y el hijo se queda.
bash -c "\"$G\" --madre \$\$ > capturas/madre-caida.log 2>&1 & echo \$! > \"$T/hijo.pid\"; sleep 3"
pid=$(cat "$T/hijo.pid")
sleep 5
if kill -0 "$pid" 2>/dev/null; then
    echo "  ok    sin despedida, la madre se cayo: fue_gui sigue abierto"
    kill "$pid" 2>/dev/null; wait "$pid" 2>/dev/null
else
    echo "::error::sin despedida, fue_gui se cerro con la madre: lo no guardado se perderia"
    fallos=$((fallos + 1))
fi

# 2. CON DESPEDIDA: la madre se cierra bien y el hijo con ella.
bash -c "\"$G\" --madre \$\$ > capturas/madre-despide.log 2>&1 & echo \$! > \"$T/hijo.pid\"; sleep 3; echo adios > \"$T/atsw-madre-\$\$.adios\""
pid=$(cat "$T/hijo.pid")
t=0
while kill -0 "$pid" 2>/dev/null && [ $t -lt 15 ]; do sleep 1; t=$((t + 1)); done
if kill -0 "$pid" 2>/dev/null; then
    echo "::error::con despedida, fue_gui no se cerro con la madre (sigue a los 15 s)"
    kill "$pid" 2>/dev/null
    fallos=$((fallos + 1))
else
    echo "  ok    con despedida, fue_gui se cierra con la madre"
fi
rm -f "$T"/atsw-madre-*.adios "$T/hijo.pid"

[ $fallos -eq 0 ] && echo "la madre que se cae y la que se despide: bien"
exit $fallos
