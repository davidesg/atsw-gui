#!/bin/bash
# EL PAQUETE DE WINDOWS, desde una consola de MSYS2 (UCRT64) y con el arbol
# ya construido:
#
#   bash packaging/windows/bundle.sh [destino]      (por defecto dist/ATSW)
#
# Deja la disposicion de un prefijo de MSYS2, que es la que GTK sabe usar sin
# variables de entorno: GTK y GLib toman como prefijo el directorio PADRE del
# de sus DLL, y desde ahi encuentran share/ y lib/.
#
#   ATSW/bin     los .exe, sus DLL y los ayudantes de g_spawn
#   ATSW/lib     los cargadores de imagenes de gdk-pixbuf (los iconos SVG)
#   ATSW/share   los esquemas de GSettings compilados (sin ellos el selector
#                de ficheros aborta) y los temas de iconos
#
# De ahi sale el zip portatil y, con Inno Setup, el instalador (atsw.iss).
set -euo pipefail

R=$(cd "$(dirname "$0")/../.." && pwd)
D=${1:-$R/dist/ATSW}
P=${MSYSTEM_PREFIX:?hay que correrlo en una consola UCRT64 de MSYS2}

. "$R/packaging/programas.sh"

rm -rf "$D"
mkdir -p "$D/bin" "$D/lib" "$D/share/glib-2.0/schemas" "$D/share/icons"

for p in $PROGRAMAS; do
    [ -f "$R/$p.exe" ] || { echo "falta $p.exe: ¿esta construido?" >&2; exit 1; }
    cp "$R/$p.exe" "$D/bin/"
done

# Los ayudantes de g_spawn: sin ellos GLib no puede lanzar procesos en
# Windows, y la madre no abriria nada ni los GUIs correrian los motores.
cp "$P"/bin/gspawn-win64-helper*.exe "$D/bin/"

# Los cargadores de gdk-pixbuf, y la cache con rutas RELATIVAS al prefijo:
# gdk-pixbuf las resuelve contra su propio prefijo en Windows, asi que el
# paquete se puede mover de sitio.
PB=lib/gdk-pixbuf-2.0/2.10.0
mkdir -p "$D/$PB/loaders"
cp "$P/$PB"/loaders/*.dll "$D/$PB/loaders/"
"$P/bin/gdk-pixbuf-query-loaders.exe" "$D/$PB"/loaders/*.dll \
    | sed -e 's|^"[^"]*\(lib[/\\]\+gdk-pixbuf-2\.0[/\\]\+\)|"\1|' \
    > "$D/$PB/loaders.cache"
grep '^"' "$D/$PB/loaders.cache"

# LAS DLL: las que piden los .exe y los cargadores, y las que piden esas, hasta
# que no aparezca ninguna nueva. Solo las de MSYS2; las de Windows ya estan.
dlls() {
    ldd "$@" 2>/dev/null | awk -v p="$P/bin/" 'index($3, p) == 1 { print $3 }' | sort -u
}
objetos=("$D"/bin/*.exe "$D/$PB"/loaders/*.dll)
while :; do
    nuevas=0
    for f in $(dlls "${objetos[@]}"); do
        b=$(basename "$f")
        if [ ! -f "$D/bin/$b" ]; then
            cp "$f" "$D/bin/"; objetos+=("$D/bin/$b"); nuevas=1
        fi
    done
    [ $nuevas -eq 0 ] && break
done

# Los esquemas: solo los de GTK, compilados aqui.
cp "$P"/share/glib-2.0/schemas/org.gtk.*.gschema.xml "$D/share/glib-2.0/schemas/"
glib-compile-schemas "$D/share/glib-2.0/schemas"

# Los iconos: Adwaita (los de los botones) y hicolor (el tema de reserva).
cp -r "$P/share/icons/Adwaita" "$P/share/icons/hicolor" "$D/share/icons/"

# Un tema claro de Windows, sin decoraciones de cliente raras.
mkdir -p "$D/etc/gtk-3.0"
cat > "$D/etc/gtk-3.0/settings.ini" <<'INI'
[Settings]
gtk-theme-name=Adwaita
gtk-icon-theme-name=Adwaita
gtk-font-name=Segoe UI 9
INI

cp "$R/packaging/LEEME.txt" "$D/LEEME.txt"
# El manual del alumno, si esta construido (docs/manual/construir.py).
[ -f "$R/docs/manual/MANUAL.pdf" ] && cp "$R/docs/manual/MANUAL.pdf" "$D/Manual de ATSW.pdf"
# Y sus datos, para seguir el caso con el programa abierto.
mkdir -p "$D/Datos del manual"
cp "$R"/docs/manual/datos/ipc_wti.{csv,xlsx} "$D/Datos del manual/"

echo "paquete en $D: $(ls "$D/bin" | wc -l) ficheros en bin/, $(du -sh "$D" | cut -f1)"
