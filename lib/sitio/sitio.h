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

/* --- LOS HIJOS SE VAN CON LA MADRE ---------------------------------------
 *
 * La madre lanza fue_gui, drtran_gui y gtk_fmg con «--madre PID». Si la madre
 * se cierra y ellos siguen abiertos, siguen escribiendo en el proyecto --un
 * modelo nuevo, un caso, el manifiesto-- sin nadie que lo vea ni lo ordene.
 * Así que cada hijo vigila a su madre y se cierra cuando ella ya no está.
 *
 * Por qué así y no de otra forma: la madre no tiene con qué pedirle a otro
 * proceso que se cierre ordenadamente en las tres plataformas (una señal en
 * Windows no lo es), pero el hijo sí puede preguntar si su madre vive.
 *
 * sitio_mi_pid       el pid de este proceso, para el «--madre PID».
 * sitio_vigila_madre  mira cada segundo si el proceso pid sigue vivo; si no,
 *                     llama a al_irse(datos) una vez. pid NULL o vacío: no
 *                     vigila (el programa lanzado a mano). Devuelve 1 si
 *                     vigila.                                            */
long sitio_mi_pid( void );
int  sitio_vigila_madre( const char *pid, GSourceFunc al_irse, gpointer datos );

#endif
