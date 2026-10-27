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

/* --- LOS MOTORES, SIN CONSOLA EN WINDOWS ---------------------------------
 *
 * Los motores (fue, fug, art...) son programas de consola, y los GUIs se
 * enlazan con -mwindows: no tienen consola. En Windows, un programa de
 * consola lanzado por uno que no la tiene recibe una NUEVA, con su ventana
 * negra, y eso es lo que pasaba con g_spawn: GLib lanza a traves de su
 * gspawn-win64-helper.exe (sin consola), y el motor, hijo de este, abria la
 * suya. Cada redibujo del vistazo de la madre abria y cerraba una consola, y
 * pagaba dos procesos en vez de uno. GLib no tiene una bandera para pedir
 * CREATE_NO_WINDOW, ni en g_spawn ni en GSubprocess.
 *
 * Asi que en Windows se lanza con CreateProcessW y CREATE_NO_WINDOW --sin el
 * proceso intermedio--, y en Linux y macOS es g_spawn, sin cambiar nada: las
 * dos funciones de abajo tienen la firma de las de GLib y alli las llaman
 * tal cual.
 *
 * CREATE_NO_WINDOW SOLO SI EL QUE LANZA NO TIENE CONSOLA, que es el criterio
 * de GLib para elegir su ayudante: atsw-cli, o una bateria corrida desde una
 * terminal, siguen compartiendo la suya con el motor (y el Ctrl-C le llega).
 *
 * En Windows, ademas:
 *   - la entrada que no se pide como tuberia es NUL (no la del que lanza, que
 *     en un GUI no existe); la salida y el error que no se piden, tambien;
 *   - envp tiene que ser NULL (el hijo hereda el entorno) y child_setup no se
 *     usa (tampoco lo usa GLib en Windows);
 *   - el pid es el manejador del proceso, como en GLib: se vigila con
 *     g_child_watch y se cierra con g_spawn_close_pid.                    */
#ifdef G_OS_WIN32
/* Abrir un fichero con el programa que Windows tenga para el (el PDF, con
 * el visor). Era system("start ..."): system pasa por cmd.exe, un programa
 * de consola, y desde un GUI eso asomaba una consola cada vez. Aqui es
 * ShellExecuteW, sin interprete de ordenes. Devuelve TRUE si se abrio.  */
gboolean sitio_abre( const char *ruta );
#endif

#define SITIO_SIN_CONSOLA 1     /* lo mira lib/sitio/test_consola.c */

gboolean sitio_lanza_con_tuberias( const gchar *dir, gchar **argv, gchar **envp,
                                   GSpawnFlags flags,
                                   GSpawnChildSetupFunc child_setup,
                                   gpointer user_data, GPid *child_pid,
                                   gint *standard_input, gint *standard_output,
                                   gint *standard_error, GError **error );

gboolean sitio_lanza_y_espera( const gchar *dir, gchar **argv, gchar **envp,
                               GSpawnFlags flags,
                               GSpawnChildSetupFunc child_setup,
                               gpointer user_data, gchar **standard_output,
                               gchar **standard_error, gint *wait_status,
                               GError **error );

#endif
