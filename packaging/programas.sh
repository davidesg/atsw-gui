# Los programas que van en el paquete, como rutas del arbol de construccion
# sin la extension (.exe en Windows). Lo leen los guiones de empaquetado de
# cada plataforma; la lista vive aqui y en ningun otro sitio.
#
# Todos van al MISMO directorio. Asi se encuentran entre ellos: la madre y el
# identificador buscan a sus hermanos «./%s» al lado de si mismos (lib/sitio),
# y lo que se busca por el PATH lo resuelve cada plataforma -- en Windows
# g_find_program_in_path mira primero el directorio del propio .exe, y en
# macOS el lanzador de la .app pone ese directorio el primero del PATH.
#
# engines/fug/gtk_fmg NO: es el GUI viejo de GTK 2. El de verdad es gui/fug.
PROGRAMAS="
engines/fue/bin/fue
engines/fuf/bin/fuf
engines/fug/fug
engines/drtran/bin/drtran
engines/drvarma/bin/drvarma
engines/drvec/bin/drvec
engines/art/bin/art
gui/atsw/atsw_gui
gui/fue/bin/fue_gui
gui/fug/gtk_fmg
gui/drtran/drtran_gui
engines/drvarma/bin/drvarma_gui
"
