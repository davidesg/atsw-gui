#!/bin/sh
# INSTALA EL ARBOL YA CONSTRUIDO en Linux. Lo usan el .deb (deb.sh) y la
# instalacion desde las fuentes (make install):
#
#   sh packaging/linux/instalar.sh DESTDIR PREFIJO
#
#   PREFIJO/lib/atsw/bin/       los motores y los GUIs, todos juntos: asi se
#                               encuentran entre ellos (lib/sitio busca al lado
#                               del ejecutable de verdad, /proc/self/exe)
#   PREFIJO/bin/atsw            enlace a la madre
#   PREFIJO/share/applications  ATSW, fue, drtran y fug en el menu
#   PREFIJO/share/doc/atsw/     el LEEME, el manual y sus datos
set -e
D=$1 P=${2:-/usr/local}
R=$(cd "$(dirname "$0")/../.." && pwd)
. "$R/packaging/programas.sh"

B=$D$P/lib/atsw/bin
install -d "$B" "$D$P/bin" "$D$P/share/applications" \
    "$D$P/share/icons/hicolor/scalable/apps" "$D$P/share/doc/atsw/Datos del manual"
for p in $PROGRAMAS; do
    [ -f "$R/$p" ] || { echo "falta $p: ¿esta construido?" >&2; exit 1; }
    install -m 755 "$R/$p" "$B/"
done
ln -sf ../lib/atsw/bin/atsw_gui "$D$P/bin/atsw"
install -m 644 "$R/packaging/linux/atsw.svg" "$D$P/share/icons/hicolor/scalable/apps/"

# Una entrada de menu por GUI, como los accesos del menu Inicio de Windows.
# EL FICHERO SE LLAMA COMO EL ID DE LA APLICACION (org.atsw.gui...): con
# Wayland, GNOME busca ese .desktop para poner el icono en la barra, y si
# no lo encuentra pone una rueda dentada. StartupWMClass, para X11.
entrada() {  # id programa nombre comentario
    cat > "$D$P/share/applications/$1.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=$3
Comment=$4
Exec=$P/lib/atsw/bin/$2
Icon=atsw
Terminal=false
Categories=Education;Science;Math;
StartupWMClass=$1
DESKTOP
}
entrada org.atsw.gui   atsw_gui   "ATSW"                          "Taller de series temporales: proyectos, series, modelos y casos"
entrada org.fue.gui    fue_gui    "ATSW: editor de modelos (fue)" "Modelos ARIMA univariantes"
entrada org.drtran.gui drtran_gui "ATSW: redes de transferencia (drtran)" "Funciones de transferencia"
entrada org.fug.gui    gtk_fmg    "ATSW: gráficos (fug)"          "Gráficos de series"

install -m 644 "$R/packaging/LEEME.txt" "$D$P/share/doc/atsw/"
[ -f "$R/docs/manual/MANUAL.pdf" ] && install -m 644 "$R/docs/manual/MANUAL.pdf" "$D$P/share/doc/atsw/Manual de ATSW.pdf"
install -m 644 "$R"/docs/manual/datos/ipc_wti.csv "$R"/docs/manual/datos/ipc_wti.xlsx \
    "$R"/docs/manual/datos/es_core.csv "$R"/docs/manual/datos/es_core.xlsx \
    "$D$P/share/doc/atsw/Datos del manual/"
echo "instalado en $D$P"
