/*
 * sitio.h -- donde esta el programa que se esta ejecutando.
 *
 * Los GUIs buscan a sus hermanos (los otros GUIs, los motores) AL LADO de si
 * mismos antes que en el PATH: un programa lanzado desde el arbol de
 * compilacion tiene que lanzar a los de SU arbol (ver gui/atsw/src/gestos.c).
 *
 * Para eso hace falta saber donde esta uno, y eso no es portable: se leia
 * /proc/self/exe, que solo existe en Linux. En macOS y Windows la busqueda
 * de al lado no se hacia nunca y mandaba el PATH. Aqui esta una sola vez, con
 * las tres plataformas.
 */

#ifndef ATSW_SITIO_H
#define ATSW_SITIO_H

#include <glib.h>

/* Directorio del ejecutable en curso (g_free), o NULL si no se sabe. */
gchar *sitio_mi_dir( void );

/* El nombre del ejecutable de un programa: "fue" -> "fue.exe" en Windows.
   Devuelve una cadena nueva (g_free). */
gchar *sitio_exe( const char *programa );

/* Busca programa en los sitios dados, relativos al directorio del
   ejecutable en curso; cada sitio es un formato con un %s para el nombre
   (con .exe en Windows). NULL al final de la lista. Si no esta en ninguno,
   el PATH. Devuelve una ruta nueva (g_free) o NULL. */
gchar *sitio_busca( const char *programa, const char *const *sitios );

#endif
