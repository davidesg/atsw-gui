#!/bin/bash
# EL PAQUETE DE macOS: ATSW.app y un .dmg para instalarla, con el arbol ya
# construido contra Homebrew (gtk+3 gsl librsvg adwaita-icon-theme) y con
# dylibbundler instalado:
#
#   bash packaging/macos/bundle.sh [version]
#
# Deja dist/ATSW.app y dist/ATSW-<version>-macos-<arq><SUFIJO>.dmg.
#
# BIBLIOTECAS=<prefijo> toma las bibliotecas de ahi en vez de Homebrew: es
# como se hace el paquete para macOS 11 con las de conda-forge, que se
# compilan para macOS viejos (las de Homebrew piden el macOS del runner).
#
#   Contents/MacOS/ATSW      el lanzador (lanzador.c): pone el entorno de
#                            GTK y el PATH, y arranca la madre
#   Contents/Resources/bin   los motores y los GUIs, todos juntos
#   Contents/Resources/lib   las bibliotecas de Homebrew, reescritas a
#                            @executable_path/../lib, y los cargadores de
#                            gdk-pixbuf
#   Contents/Resources/share esquemas de GSettings compilados e iconos
#
# Se firma AD HOC (sin cuenta de desarrollador de Apple). En Apple Silicon un
# binario sin firma no arranca, y reescribir las rutas de las bibliotecas
# rompe la firma que traian. Ad hoc basta para arrancar; Gatekeeper sigue
# avisando la primera vez (ver LEEME.txt).
set -euo pipefail

R=$(cd "$(dirname "$0")/../.." && pwd)
V=${1:-0.0}
B=${BIBLIOTECAS:-$(brew --prefix)}
ARQ=$(uname -m)
APP=$R/dist/ATSW.app
C=$APP/Contents
RES=$C/Resources

. "$R/packaging/programas.sh"

rm -rf "$R/dist"
mkdir -p "$C/MacOS" "$RES/bin" "$RES/lib" "$RES/share/glib-2.0/schemas" "$RES/share/icons"

for p in $PROGRAMAS; do
    [ -f "$R/$p" ] || { echo "falta $p: ¿esta construido?" >&2; exit 1; }
    cp "$R/$p" "$RES/bin/"
done

cc -O2 -Wall -o "$C/MacOS/ATSW" "$R/packaging/macos/lanzador.c"

# Los cargadores de gdk-pixbuf: png y jpeg suelen venir dentro, pero los
# iconos de Adwaita son SVG y ese cargador es de librsvg.
PB=lib/gdk-pixbuf-2.0/2.10.0
mkdir -p "$RES/$PB/loaders"
# Los que dice la cache de Homebrew, esten donde esten: el de SVG lo pone
# librsvg en SU directorio (y como .dylib), no en el de gdk-pixbuf.
# (Las rutas de conda-forge son relativas a su prefijo.)
for l in $(sed -nE 's/^"([^"]*\.(so|dylib))"$/\1/p' "$B/$PB/loaders.cache"); do
    case "$l" in /*) ;; *) l=$B/$l ;; esac
    cp -L "$l" "$RES/$PB/loaders/"
    # Su propio nombre (LC_ID_DYLIB) apunta a Homebrew, y dylibbundler solo
    # arregla las dependencias, no el nombre.
    chmod u+w "$RES/$PB/loaders/$(basename "$l")"
    install_name_tool -id "@executable_path/../$PB/loaders/$(basename "$l")" \
        "$RES/$PB/loaders/$(basename "$l")"
done
# La cache, como plantilla: la ruta de cada cargador pasa a @PREFIJO@, y el
# lanzador escribe la de verdad al arrancar (lanzador.c).
sed -E 's|^"[^"]*/loaders/|"@PREFIJO@/'"$PB"'/loaders/|' "$B/$PB/loaders.cache" \
    > "$RES/$PB/loaders.cache.in"
grep '^"' "$RES/$PB/loaders.cache.in"

# Las bibliotecas: dylibbundler copia a lib/ todas las que piden los
# ejecutables y los cargadores (y las que piden esas), y les reescribe las
# rutas. -s para las que se piden por @rpath.
#
# Las de conda-forge ya se piden por @rpath/: se quedan asi, y cada
# ejecutable lleva un rpath a ../lib. Reescribirlas a @executable_path/...
# alarga los nombres, y en sus cabeceras no cabe («larger updated load
# commands do not fit»).
if [ -n "${BIBLIOTECAS:-}" ]; then PREF=@rpath/; else PREF=@executable_path/../lib/; fi
xs=()
for f in "$RES"/bin/* "$RES/$PB"/loaders/*; do xs+=(-x "$f"); done
dylibbundler -of -b -cd "${xs[@]}" -d "$RES/lib" -p "$PREF" \
    -s "$B/lib" </dev/null
if [ "$PREF" = @rpath/ ]; then
    for f in "$RES"/bin/*; do
        # Fuera los rpath que traiga (los de la construccion), y el nuestro.
        for rp in $(otool -l "$f" | awk '/LC_RPATH/ { f = 1 } f && $1 == "path" { print $2; f = 0 }'); do
            install_name_tool -delete_rpath "$rp" "$f"
        done
        install_name_tool -add_rpath @executable_path/../lib "$f"
    done
fi

# Que no quede NADA apuntando a Homebrew: en la maquina de construccion eso
# funciona y en la del usuario no.
if find "$RES" -type f \( -perm -u+x -o -name '*.dylib' -o -name '*.so' \) \
       -exec otool -L {} \; 2>/dev/null | grep -E "^\s+($B|/usr/local/|/opt/homebrew)"; then
    echo "quedan bibliotecas de $B o de Homebrew sin meter en la .app" >&2; exit 1
fi

cp "$B"/share/glib-2.0/schemas/org.gtk.*.gschema.xml "$RES/share/glib-2.0/schemas/"
glib-compile-schemas "$RES/share/glib-2.0/schemas"
cp -R "$B/share/icons/Adwaita" "$B/share/icons/hicolor" "$RES/share/icons/"

# Ningun enlace simbolico dentro de la .app: los de Homebrew apuntan fuera
# (o a nada), y con uno roto la firma no se verifica («No such file or
# directory»). Cada enlace pasa a ser una copia de lo que apunta, o fuera.
find "$APP" -type l | while read -r l; do
    t=$(readlink -f "$l" || true)
    rm "$l"
    if [ -n "$t" ] && [ -f "$t" ]; then cp "$t" "$l"; fi
done

# La version minima de macOS: la mas alta que pida un binario de la .app.
# Las bibliotecas de Homebrew piden la del runner donde se construyo; las de
# conda-forge, una vieja. Por debajo de esa no arranca. Los binarios viejos
# lo dicen en LC_VERSION_MIN_MACOSX («version») y no en LC_BUILD_VERSION.
minos() {
    otool -l "$1" 2>/dev/null | awk '
        /LC_BUILD_VERSION|LC_VERSION_MIN_MACOSX/ { f = 1 }
        f && ($1 == "minos" || $1 == "version") { print $2; exit }'
}
MINOS=$(find "$APP" -type f \( -perm -u+x -o -name '*.dylib' -o -name '*.so' \) |
        while read -r f; do minos "$f"; done | sort -V | tail -1)
echo "macOS minimo: ${MINOS:=11.0}"
if [ -n "${MACOS_MAXIMO:-}" ] &&
   [ "$(printf '%s\n' "$MINOS" "$MACOS_MAXIMO" | sort -V | tail -1)" != "$MACOS_MAXIMO" ]; then
    echo "piden macOS $MINOS y tiene que valer en $MACOS_MAXIMO:" >&2
    find "$APP" -type f \( -perm -u+x -o -name '*.dylib' -o -name '*.so' \) |
        while read -r f; do echo "$(minos "$f") $f"; done | sort -V | tail -5 >&2
    exit 1
fi

cat > "$C/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key>               <string>ATSW</string>
  <key>CFBundleDisplayName</key>        <string>ATSW</string>
  <key>CFBundleIdentifier</key>         <string>io.github.davidesg.atsw</string>
  <key>CFBundleExecutable</key>         <string>ATSW</string>
  <key>CFBundlePackageType</key>        <string>APPL</string>
  <key>CFBundleVersion</key>            <string>$V</string>
  <key>CFBundleShortVersionString</key> <string>$V</string>
  <key>CFBundleDevelopmentRegion</key>  <string>es</string>
  <key>LSMinimumSystemVersion</key>     <string>$MINOS</string>
  <key>NSHighResolutionCapable</key>    <true/>
</dict>
</plist>
PLIST

# La firma: de dentro afuera, cada binario y despues la .app entera.
find "$RES" -type f \( -name '*.dylib' -o -name '*.so' \) -exec codesign --force --sign - {} \;
codesign --force --sign - "$RES"/bin/*
codesign --force --sign - "$APP"
codesign --verify --strict --verbose=4 "$APP"

cp "$R/packaging/LEEME.txt" "$R/dist/LEEME.txt"

# El .dmg: la .app, un enlace a /Applications para arrastrarla, y el LEEME.
DMG=$R/dist/dmg
mkdir -p "$DMG"
cp -R "$APP" "$DMG/"
ln -s /Applications "$DMG/Applications"
cp "$R/packaging/LEEME.txt" "$DMG/"
[ -f "$R/docs/manual/MANUAL.pdf" ] && cp "$R/docs/manual/MANUAL.pdf" "$DMG/Manual de ATSW.pdf"
# Y sus datos, para seguir el caso con el programa abierto.
mkdir -p "$DMG/Datos del manual"
cp "$R"/docs/manual/datos/ipc_wti.{csv,xlsx} "$DMG/Datos del manual/"
# hdiutil falla a veces en los Mac de GitHub con «Resource busy», sin que
# pase nada: se reintenta.
for i in 1 2 3 4 5; do
    hdiutil create -volname ATSW -srcfolder "$DMG" -ov -format UDZO \
        "$R/dist/ATSW-$V-macos-$ARQ${SUFIJO:-}.dmg" && break
    [ $i -lt 5 ] || exit 1
    echo "hdiutil fallo; otra vez dentro de $((i * 5)) s" >&2; sleep $((i * 5))
done
rm -rf "$DMG"

echo "hecho: $(du -sh "$APP" | cut -f1) la .app, $(du -sh "$R"/dist/*.dmg | cut -f1) el .dmg"
