#!/bin/bash
# EL PAQUETE .deb (Ubuntu 22.04 o posterior, Debian 12 o posterior):
#
#   bash packaging/linux/deb.sh [version]
#
# Deja dist/atsw_<version>_amd64.deb. Se construye en el Ubuntu MAS VIEJO que
# se quiere cubrir (la CI usa el 22.04): lo compilado ahi arranca en los
# posteriores, al reves no.
#
# GSL va DENTRO de los ejecutables (estatica): cada Ubuntu trae una version
# distinta (libgsl27, libgsl28...) y con ella dentro el paquete no depende de
# cual. GTK, GLib y zlib son del sistema; en Ubuntu 24.04 sus paquetes se
# llaman ...t64, de ahi las alternativas en Depends.
set -euo pipefail

R=$(cd "$(dirname "$0")/../.." && pwd)
V=${1:-0.0}
DIRS="engines/fue engines/fuf engines/fug engines/drtran engines/art
      gui/fue gui/fug gui/drtran gui/atsw gui/atsw-cli"
A=$(gcc -print-multiarch)
GSL="/usr/lib/$A/libgsl.a /usr/lib/$A/libgslcblas.a"
for f in $GSL; do [ -f "$f" ] || { echo "falta $f (libgsl-dev)" >&2; exit 1; }; done

cd "$R"
for d in $DIRS; do
    echo "== $d"
    make -s -C "$d" clean >/dev/null 2>&1 || true
    make -s -C "$d" GSL_LIBS="$GSL"
done

# Que ninguno pida libgsl: si alguno la enlaza dinamica, el paquete no
# arrancaria en un Ubuntu con otra version.
. "$R/packaging/programas.sh"
if for p in $PROGRAMAS; do ldd "$R/$p"; done | grep -q libgsl; then
    echo "algun programa sigue pidiendo libgsl dinamica" >&2; exit 1
fi

P=$R/dist/deb
rm -rf "$P"
sh "$R/packaging/linux/instalar.sh" "$P" /usr
install -d "$P/DEBIAN"
cat > "$P/DEBIAN/control" <<CONTROL
Package: atsw
Version: $V
Architecture: amd64
Section: science
Priority: optional
Depends: libgtk-3-0t64 | libgtk-3-0, libglib2.0-0t64 | libglib2.0-0, zlib1g, libc6 (>= 2.35)
Installed-Size: $(du -sk "$P/usr" | cut -f1)
Maintainer: ATSW <https://github.com/davidesg/atsw-gui/issues>
Homepage: https://davidesg.github.io/atsw-gui/
Description: taller de series temporales (Box-Jenkins-Treadway)
 Modelos ARIMA univariantes, anomalos e intervencion, y funciones de
 transferencia, con estimacion por maxima verosimilitud exacta. Trae la
 ventana principal (atsw), el editor fue, drtran, fug y sus motores, y el
 manual del alumno con sus datos.
CONTROL
mkdir -p "$R/dist"
dpkg-deb --build --root-owner-group "$P" "$R/dist/atsw_${V}_amd64.deb"
rm -rf "$P"
echo "hecho: $(du -sh "$R/dist/atsw_${V}_amd64.deb" | cut -f1)"
