#!/bin/bash
# PRUEBA DE HUMO DE LOS GUIs: cada uno se lanza, tiene que seguir vivo al cabo
# de ESPERA segundos (no se ha caido al arrancar) y se le hace una captura de
# pantalla, que queda en capturas/ para mirarla. Luego SE CIERRA COMO LO HARIA
# EL USUARIO -- el modulo cierre.c cierra sus ventanas -- y tiene que salir
# solo y con 0: ni caerse al cerrar ni quedarse colgado. Antes se le mataba
# con kill y el cierre no se probaba en ninguna plataforma; en macOS, cerrar
# fue_gui sacaba un aviso de fallo (TODO.md, 13-nov-2026).
#
# Lo que esto NO prueba es que los botones hagan lo que deben: para eso estan
# las pruebas conducidas desde el codigo de cada GUI. Esto caza lo que ellas
# no ven: que el ejecutable de verdad arranque en esa plataforma (DLLs,
# temas, iconos, el backend grafico).
ESPERA=${ESPERA:-8}
R=$(pwd)
mkdir -p capturas
export PATH="$R/engines/fue/bin:$R/engines/fuf/bin:$R/engines/fug:$R/engines/drtran/bin:$R/engines/drvarma/bin:$PATH"

# En Windows, GLib busca los esquemas de GSettings y los iconos junto al
# ejecutable (../share). En el arbol de construccion no estan, y fue_gui se
# caia al abrir el selector de ficheros ("No GSettings schemas are
# installed"). Un paquete para Windows tiene que llevar esa carpeta; aqui se
# apunta a la de MSYS2.
case "$(uname -s)" in
    MINGW*|MSYS*)
        pre=${MSYSTEM_PREFIX:-/ucrt64}
        export XDG_DATA_DIRS="$(cygpath -m "$pre/share")"
        export GSETTINGS_SCHEMA_DIR="$(cygpath -m "$pre/share/glib-2.0/schemas")"
        echo "GSETTINGS_SCHEMA_DIR=$GSETTINGS_SCHEMA_DIR"
        [ -f "$pre/share/glib-2.0/schemas/gschemas.compiled" ] ||
            echo "::warning::no hay gschemas.compiled en $pre/share/glib-2.0/schemas" ;;
esac

case "$(uname -s)" in
    MINGW*|MSYS*) EXE=.exe ;;
    *)            EXE=   ;;
esac

captura() {
    case "$(uname -s)" in
        Darwin) screencapture -x "$1" ;;
        MINGW*|MSYS*)
            powershell -NoProfile -Command "
              Add-Type -AssemblyName System.Windows.Forms,System.Drawing;
              \$b=[System.Windows.Forms.Screen]::PrimaryScreen.Bounds;
              \$i=New-Object System.Drawing.Bitmap \$b.Width,\$b.Height;
              \$g=[System.Drawing.Graphics]::FromImage(\$i);
              \$g.CopyFromScreen(\$b.Location,[System.Drawing.Point]::Empty,\$b.Size);
              \$i.Save('$(cygpath -w "$1")')" ;;
        *) import -window root "$1" ;;
    esac
}

# EL MODULO QUE CIERRA (cierre.c), compilado para esta plataforma.
case "$(uname -s)" in
    MINGW*|MSYS*) MOD="$R/capturas/cierre.dll" ;;
    *)            MOD="$R/capturas/cierre.so" ;;
esac
if ! cc -shared -fPIC -O2 -o "$MOD" "$R/.github/cierre.c" \
        $(pkg-config --cflags --libs gtk+-3.0) > capturas/cierre.log 2>&1; then
    echo "::error::no se pudo compilar el modulo de cierre"; sed 's/^/  | /' capturas/cierre.log
    exit 1
fi
case "$(uname -s)" in MINGW*|MSYS*) MODG=$(cygpath -m "$MOD") ;; *) MODG=$MOD ;; esac
CIERRE=$((ESPERA + 2))
HASTA=${HASTA:-40}          # segundos para salir solo, desde que se lanza

fallan=()
for g in gui/fue/bin/fue_gui gui/fug/gtk_fmg gui/drtran/drtran_gui \
         gui/atsw/atsw_gui gui/sim/sim_gui; do
    nombre=$(basename "$g")
    echo "== $nombre"
    if [ ! -f "$g$EXE" ]; then
        echo "::error::$nombre: no esta construido"; fallan+=("$nombre"); continue
    fi
    ( cd "$(dirname "$g")" && GTK_MODULES="$MODG" CIERRE=$CIERRE exec "./$nombre$EXE" ) \
        > "capturas/$nombre.log" 2>&1 &
    pid=$!
    sleep "$ESPERA"
    if kill -0 "$pid" 2>/dev/null; then
        captura "capturas/$nombre.png" || echo "  (sin captura)"
        echo "  arranca y sigue vivo a los $ESPERA s"
        # EL CIERRE: el modulo cierra sus ventanas a los $CIERRE s
        t=$ESPERA
        while kill -0 "$pid" 2>/dev/null && [ "$t" -lt "$HASTA" ]; do sleep 1; t=$((t + 1)); done
        if kill -0 "$pid" 2>/dev/null; then
            echo "::error::$nombre: no sale al cerrar sus ventanas (sigue vivo a los $HASTA s: colgado)"
            sed 's/^/  | /' "capturas/$nombre.log" | tail -20
            kill "$pid" 2>/dev/null
            case "$(uname -s)" in MINGW*|MSYS*) taskkill //F //IM "$nombre$EXE" > /dev/null 2>&1 ;; esac
            wait "$pid" 2>/dev/null
            fallan+=("$nombre")
        else
            wait "$pid"; rc=$?
            if [ "$rc" -eq 0 ]; then
                echo "  se cierra solo y sale con 0"
            else
                echo "::error::$nombre: sale mal al cerrar (rc=$rc$( [ "$rc" -gt 128 ] && echo ", señal $((rc - 128))"))"
                sed 's/^/  | /' "capturas/$nombre.log" | tail -30
                fallan+=("$nombre")
            fi
            if grep -q "CRITICAL" "capturas/$nombre.log"; then
                echo "::warning::$nombre: avisos CRITICAL de GLib/GTK (capturas/$nombre.log)"
                grep "CRITICAL" "capturas/$nombre.log" | head -5 | sed 's/^/  | /'
            fi
        fi
    else
        wait "$pid"; rc=$?
        echo "::error::$nombre: se cerro al arrancar (rc=$rc)"
        sed 's/^/  | /' "capturas/$nombre.log" | head -30
        fallan+=("$nombre")
    fi
done

echo
if [ ${#fallan[@]} -eq 0 ]; then echo "los GUIs arrancan y se cierran bien"
else echo "fallan al arrancar o al cerrar: ${fallan[*]}"; exit 1; fi
