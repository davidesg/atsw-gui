/* engine.h -- running fue and fuf from the GUI.
 *
 * The engines are run directly, never through a shell: a series name with a
 * space, a quote or a semicolon used to be handed to /bin/sh, which either
 * broke the command or ran something else.
 *
 * They also say how they ended, and the GUI passes that on instead of the
 * bare "failed" it used to show:
 *
 *   0  the results are written
 *   1  the command line or the input file could not be read
 *   2  the input file is not valid; nothing was written
 *   3  the model could not be estimated; the results carry the initial values
 *   4  the engine stopped on a run-time error
 *
 * Anything else (a signal, an unknown status) is reported as it comes.
 *
 * LO QUE CUESTA CADA CORRIDA: si el que lanza puso antes un contexto
 * (coste_contexto, lib/coste), la corrida deja su linea en el costes.csv del
 * proyecto, con el tiempo de CPU y de reloj. engine_run, engine_run_async y
 * engine_start pasan por el mismo sitio; sin contexto no se escribe nada.
 */

#ifndef ENGINE_H
#define ENGINE_H

#include <glib.h>

typedef struct {
    int    status;     /* the exit status; ENGINE_NORUN or ENGINE_SIGNAL    */
    int    signal_no;  /* the signal, when status is ENGINE_SIGNAL          */
    gchar *output;     /* what it wrote (stdout and stderr), or NULL        */
    gchar *message;    /* one line for the status bar; never NULL           */
} EngineResult;

#define ENGINE_NORUN  (-1)   /* the program could not be started            */
#define ENGINE_SIGNAL (-2)   /* it died on a signal                         */

/* Run <program> in workdir with the arguments that follow (a NULL-terminated
 * list) and wait for it. The program is looked up in the PATH, or with the
 * finder given to engine_set_finder.                                       */
EngineResult engine_run(const char *workdir, const char *program, ...) G_GNUC_NULL_TERMINATED;

/* TRUE when the run left its files behind: it finished (0), or it could not
 * estimate the model and wrote the results with the initial values (3).    */
gboolean engine_wrote_results(const EngineResult *r);

void engine_result_clear(EngineResult *r);

/* ------------------------------------------------------------------------ */
/* Sin bloquear la interfaz, y con el avance                                 */
/*                                                                           */
/* El optimizador de fue y de fuf escribe una linea por iteracion -- sin      */
/* salto de linea, todas seguidas -- con el numero y el valor de la funcion.  */
/* Desde fue 1.14 y fuf 1.09 sale sin buffer, asi que llega segun ocurre.     */
/* ------------------------------------------------------------------------ */

/* Una iteracion del optimizador */
typedef void (*EngineProgress)(int iteration, double objective, gpointer data);

/* El final: r vale solo mientras dura la llamada */
typedef void (*EngineDone)(const EngineResult *r, gpointer data);

/* Lanza <program> en workdir con argv (terminado en NULL, sin el nombre del
 * programa) y vuelve enseguida. progress se llama por cada iteracion que el
 * motor cuenta y done cuando acaba. FALSE si no se pudo lanzar, y entonces
 * done no se llama.                                                        */
gboolean engine_run_async(const char *workdir, const char *program,
                          const char *const *argv,
                          EngineProgress progress, EngineDone done,
                          gpointer data);

/* ------------------------------------------------------------------------ */
/* Lo mismo, pero EN VIVO y con la posibilidad de pararlo                    */
/*                                                                           */
/* engine_run_async entrega la salida entera al final. En una corrida larga  */
/* --o en una evaluacion recursiva, que son muchas estimaciones seguidas--   */
/* eso deja al que llama mirando una caja vacia, y sin forma de abortar.     */
/* ------------------------------------------------------------------------ */

/* Un trozo de la salida, segun llega. No termina en cero: len manda. */
typedef void (*EngineSalida)(const char *txt, gsize len, gpointer data);

typedef struct EngineJob EngineJob;      /* opaco */

/* Devuelve el trabajo, o NULL si no se pudo lanzar (y entonces done no se
 * llama). salida y progress pueden ser NULL.                            */
EngineJob *engine_start(const char *workdir, const char *program,
                        const char *const *argv,
                        EngineProgress progress, EngineSalida salida,
                        EngineDone done, gpointer data);

/* Pide al proceso que se pare. El final llega por done() como cualquier otro,
 * con status ENGINE_SIGNAL: no hay un camino aparte para la cancelacion.  */
void engine_stop(EngineJob *job);

/* PARAR TODOS LOS QUE QUEDEN VIVOS, al irse el programa (BUG-0001 de
 * drtran_gui). Manda SIGTERM (TerminateProcess en Windows) a cada corrida que
 * no ha acabado, espera un momento a que salgan y remata con SIGKILL a la que
 * no. No llama a done(): el que se va ya no tiene ventana a la que avisar.
 * Se registra sola con atexit al lanzar la primera corrida; cada GUI la llama
 * ademas en el "shutdown" de su aplicacion y antes de irse con la madre.  */
void engine_stop_all(void);

/* WHERE THE PROGRAM IS. By default, the PATH. A GUI run from the build tree
 * must run the engines of ITS tree, not an old install in /usr/local: there
 * was a fue of September there, from before the equation lost its italics,
 * and fue_gui ran it. finder gets the bare name ("fue") and returns a new
 * path (g_free) or NULL; NULL as finder goes back to the PATH. The messages
 * still name the program, not the path.                                    */
typedef gchar *(*EngineFinder)(const char *program);
void engine_set_finder(EngineFinder finder);

#endif /* ENGINE_H */
