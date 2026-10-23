/* engine.c -- running fue and fuf from the GUI. See include/engine.h. */

#include "engine.h"
#include "coste.h"

#include <stdarg.h>
#include <string.h>

#include <glib/gstdio.h>

#ifdef G_OS_WIN32
#  include <windows.h>
#else
#  include <sys/wait.h>
#endif

/* The first line of text that says something, so that the status bar carries
 * what the engine itself complained about and not its banner.             */
/* LA PRIMERA LINEA DE LO QUE DIJO EL MOTOR, Y EN UTF-8 PASE LO QUE PASE.
 *
 * Este trozo acaba DENTRO del mensaje, y el mensaje acaba en una etiqueta de
 * GTK. Los motores no prometen UTF-8: escriben lo que les llega --un nombre
 * de serie con los bytes con que se escribio, la salida de pdflatex o de
 * gnuplot-- y con un byte invalido gtk_label_set_text pinta SIMBOLOS RAROS y
 * avisa por stderr. En un programa lanzado por la madre ese aviso se pierde,
 * asi que lo unico que le queda al analista es la basura en pantalla.
 *
 * Se arregla AQUI y no en cada etiqueta: la biblioteca devuelve QUE PASO, y
 * un mensaje que no se puede enseñar no es un mensaje. Son veintiocho sitios
 * solo en la pestaña de prevision.                                       */
static gchar *first_line(const char *text) {
    const char *p = text, *end;
    gchar      *cruda, *limpia;

    if (!text) return NULL;
    while (*p == '\n' || *p == '\r' || *p == ' ' || *p == '\t') p++;
    if (!*p) return NULL;
    end = strpbrk(p, "\r\n");
    if (!end) end = p + strlen(p);
    while (end > p && (end[-1] == ' ' || end[-1] == '\t')) end--;
    if (end <= p) return NULL;

    cruda  = g_strndup(p, (gsize)(end - p));
    limpia = g_utf8_make_valid(cruda, -1);
    g_free(cruda);
    return limpia;
}

static gchar *status_message(const char *program, int status, int signal_no,
                             const char *err, const char *out) {
    const char *what;
    gchar      *line, *msg;

    /* The ladder engines give 4 and 5 their own meaning. drtran and drvarma
       return 4 when the series cannot be crossed: different frequency, last
       date or length (BUG-2, lib/fuepre). That is an input to rebuild in art,
       not a run-time failure. drvarma returns 5 when the diagonal gate fails:
       the joint cast does not reproduce the univariate models, and nothing
       estimated on top of it can be trusted.                                */
    gboolean ladder = program && (strstr(program, "drtran") || strstr(program, "drvarma"));

    if (ladder && status == 4)
        what = "las series no comparten la misma ventana (frecuencia, última "
               "fecha o longitud): rehaz los .pre en art sobre una sola ventana";
    else if (ladder && status == 5 && strstr(program, "drvarma"))
        what = "falló la puerta diagonal: el modelo conjunto no reproduce los "
               "univariantes";
    else
    switch (status) {
    case 0:  what = "ha terminado";                                        break;
    case 1:  what = "no pudo leer el fichero de entrada";                  break;
    case 2:  what = "el fichero de entrada no es válido; no se escribió nada"; break;
    case 3:  what = "no pudo estimar el modelo; los resultados llevan los "
                    "valores iniciales";                                   break;
    case 4:  what = "se paró por un error durante la ejecución";           break;
    case ENGINE_SIGNAL:
#ifdef G_OS_WIN32
        /* En Windows no hay senales: una caida es una excepcion, y su codigo
         * (0xC0000005, acceso indebido) es lo que sirve para buscarla.   */
        return g_strdup_printf("%s se cayó (excepción de Windows 0x%08X, como una señal).",
                               program, (unsigned) signal_no);
#else
        return g_strdup_printf("%s murió por la señal %d.", program, signal_no);
#endif
    default:
        return g_strdup_printf("%s acabó con el estado %d.", program, status);
    }

    line = first_line(err);
    if (!line && status != 0) line = first_line(out);
    if (line && status != 0) {
        msg = g_strdup_printf("%s: %s \xe2\x80\x94 %s", program, what, line);
        g_free(line);
        return msg;
    }
    g_free(line);
    return g_strdup_printf("%s %s.", program, what);
}

/* EL ESTADO DE SALIDA, en un solo sitio. En POSIX es un wait status; en
 * Windows, GLib da el codigo de salida tal cual, y una caida llega como una
 * excepcion NTSTATUS (0xC0000005...), que se trata como la senal de POSIX.
 * Estaba repetido en el camino sincrono y en el asincrono, y en Windows
 * ninguno reconocia una caida.                                          */
static void decode_status(gint wait_status, int *status, int *signal_no) {
#ifdef G_OS_WIN32
    if ((guint) wait_status >= 0xC0000000u) {
        *status    = ENGINE_SIGNAL;
        *signal_no = wait_status;
    } else
        *status = wait_status;
#else
    if (WIFEXITED(wait_status))
        *status = WEXITSTATUS(wait_status);
    else if (WIFSIGNALED(wait_status)) {
        *status    = ENGINE_SIGNAL;
        *signal_no = WTERMSIG(wait_status);
    }
#endif
}

static EngineFinder finder = NULL;

void engine_set_finder(EngineFinder f) { finder = f; }

static gchar *find_program(const char *program) {
    return finder ? finder(program) : g_find_program_in_path(program);
}

/* Se declara antes: la usan el camino sincrono y el asincrono */
static gchar *status_message(const char *program, int status, int signal_no,
                             const char *err, const char *out);

/* Se declara antes: engine_run corre por el mismo camino que engine_start */
static EngineJob *lanza(const char *workdir, const char *program,
                        const char *const *argv, GMainContext *ctx,
                        gboolean sin_entrada, EngineProgress progress,
                        EngineSalida salida, EngineDone done, gpointer data,
                        GError **error);

typedef struct { EngineResult r; gboolean fin; } Espera;

static void espera_fin(const EngineResult *r, gpointer data) {
    Espera *e = data;

    e->r.status    = r->status;
    e->r.signal_no = r->signal_no;
    e->r.output    = g_strdup(r->output);
    e->r.message   = g_strdup(r->message);
    e->fin         = TRUE;
}

EngineResult engine_run(const char *workdir, const char *program, ...) {
    EngineResult r = { ENGINE_NORUN, 0, NULL, NULL };
    GPtrArray   *args;
    va_list      ap;
    const char  *a;

    args = g_ptr_array_new_with_free_func(g_free);
    va_start(ap, program);
    while ((a = va_arg(ap, const char *)) != NULL)
        g_ptr_array_add(args, g_strdup(a));
    va_end(ap);
    g_ptr_array_add(args, NULL);

    /* UN SOLO CAMINO EN LAS TRES PLATAFORMAS, Y AHORA TAMBIEN PARA LA ESPERA.
     * Windows tenia uno propio, con CreateProcessW, que solo recogia el
     * codigo de salida; despues fue g_spawn_sync. Pero g_spawn_sync recoge
     * al hijo por dentro y no deja preguntarle cuanto costo: en Windows el
     * tiempo de CPU sale de GetProcessTimes SOBRE SU MANEJADOR, y ese
     * manejador g_spawn_sync no lo enseña (lib/coste). Asi que la espera es
     * la corrida asincrona de siempre, en un contexto PROPIO que solo se
     * itera aqui: la interfaz no se mueve mientras --como antes--, y lo que
     * se mide es lo mismo en los dos caminos.
     *
     * Con la entrada CERRADA, como hacia g_spawn_sync: un motor que pidiera
     * algo por teclado se quedaria esperando una respuesta que no llega. */
    {
    GMainContext *ctx = g_main_context_new();
    GError       *error = NULL;
    Espera        e;

    memset(&e, 0, sizeof e);
    if (lanza(workdir, program, (const char *const *) args->pdata, ctx, TRUE,
              NULL, NULL, espera_fin, &e, &error) == NULL) {
        r.message = g_strdup_printf("%s no se pudo lanzar: %s", program,
                                    error ? error->message : "error desconocido");
        if (error) g_error_free(error);
        g_main_context_unref(ctx);
        g_ptr_array_free(args, TRUE);
        return r;
    }
    while (!e.fin)
        g_main_context_iteration(ctx, TRUE);
    g_main_context_unref(ctx);
    r = e.r;
    }

    g_ptr_array_free(args, TRUE);
    return r;
}

gboolean engine_wrote_results(const EngineResult *r) {
    return r && (r->status == 0 || r->status == 3);
}

void engine_result_clear(EngineResult *r) {
    if (!r) return;
    g_free(r->output);
    g_free(r->message);
    r->output = r->message = NULL;
}

/* ------------------------------------------------------------------------ */
/* Sin bloquear la interfaz, y con el avance                                  */
/* ------------------------------------------------------------------------ */

typedef struct {
    gchar          *program;
    EngineProgress  progress;
    EngineSalida    salida;     /* lo que va llegando, segun llega          */
    EngineDone      done;
    gpointer        data;
    GString        *out, *err;
    gsize           scanned;    /* hasta donde se busco la iteracion        */
    int             status, signal_no;
    guint           pending;    /* tuberias abiertas + el hijo              */
    GPid            pid;        /* para poder pararlo                       */
    gboolean        vivo;
    GMainContext   *ctx;        /* donde se vigila; NULL, el de siempre     */
    CosteCorrida   *coste;      /* lo que cuesta; NULL sin contexto         */
} Run;

/* El optimizador escribe "%4d F: %0.10f" por iteracion, todas seguidas y sin
 * salto de linea. Se buscan las que ya estan enteras -- el numero termina
 * cuando llega algo que no es digito ni punto -- y se deja el resto para la
 * proxima vez.                                                             */
static void scan_iterations(Run *run) {
    const char *base = run->out->str;
    gsize       len  = run->out->len;

    for (;;) {
        const char *at = strstr(base + run->scanned, " F: ");
        const char *p, *q;
        char       *end;
        double      value;
        int         iteration;

        if (at == NULL) {
            /* se guarda lo justo por si la marca llega partida en dos */
            if (len > 4) run->scanned = len - 4;
            return;
        }
        p = at + 4;
        q = p;
        while (q < base + len && (g_ascii_isdigit(*q) || *q == '.' ||
                                  *q == '-' || *q == '+' || *q == 'e' || *q == 'E'))
            q++;
        if (q >= base + len)            /* el numero aun no ha llegado entero */
            return;
        value = g_ascii_strtod(p, &end);
        if (end == p) { run->scanned = (gsize)(p - base); continue; }

        /* el numero de iteracion, justo antes de la marca */
        q = at;
        while (q > base && g_ascii_isdigit(q[-1])) q--;
        iteration = (q < at) ? atoi(q) : -1;

        run->scanned = (gsize)(end - base);
        if (iteration >= 0 && run->progress != NULL)
            run->progress(iteration, value, run->data);
    }
}

static void run_finish(Run *run) {
    EngineResult r = { run->status, run->signal_no, NULL, NULL };

    r.message = status_message(run->program, run->status, run->signal_no,
                               run->err->str, run->out->str);
    r.output  = g_strconcat(run->err->str,
                            (*run->err->str && *run->out->str) ? "\n" : "",
                            run->out->str, NULL);
    if (r.output && !*r.output) { g_free(r.output); r.output = NULL; }
    /* EL COSTE ANTES QUE EL AVISO: quien recibe done() puede querer leer
       costes.csv, y la linea de esta corrida tiene que estar ya.        */
    coste_termina(run->coste, engine_wrote_results(&r), run->out->str);
    run->coste = NULL;
    if (run->done != NULL)
        run->done(&r, run->data);
    engine_result_clear(&r);

    g_string_free(run->out, TRUE);
    g_string_free(run->err, TRUE);
    g_free(run->program);
    g_free(run);
}

typedef struct { Run *run; gboolean is_err; } Pipe;

static gboolean on_pipe(GIOChannel *channel, GIOCondition cond, gpointer data) {
    Pipe    *pipe = data;
    Run     *run  = pipe->run;
    GString *to   = pipe->is_err ? run->err : run->out;
    gchar    buffer[4096];
    gsize    n = 0;
    GIOStatus st;

    while ((st = g_io_channel_read_chars(channel, buffer, sizeof(buffer), &n, NULL))
           == G_IO_STATUS_NORMAL && n > 0) {
        g_string_append_len(to, buffer, (gssize) n);
        if (to == run->out) {
            scan_iterations(run);
            /* EN VIVO. Sin esto el que llama solo ve la salida al acabar, y
             * en una corrida larga se queda mirando una caja vacia.      */
            if (run->salida) run->salida(buffer, n, run->data);
        }
    }
    /* EL FINAL DE LA TUBERIA ES EL EOF, no solo el HUP. En Linux, cuando el
     * motor cierra su salida, poll() marca POLLHUP; en macOS (y los BSD) la
     * tuberia cerrada se anuncia como LEGIBLE y la lectura da EOF, sin HUP.
     * Se miraba solo cond: aqui se devolvia TRUE, el vigilante volvia a
     * saltar sin fin y la corrida no acababa nunca -- fue_gui se quedaba
     * colgado en Run. Lo vio la CI de macOS (test_engine_progress).     */
    if ((cond & (G_IO_HUP | G_IO_ERR)) == 0
        && st != G_IO_STATUS_EOF && st != G_IO_STATUS_ERROR)
        return TRUE;

    g_io_channel_shutdown(channel, FALSE, NULL);
    g_io_channel_unref(channel);
    g_free(pipe);
    if (--run->pending == 0) run_finish(run);
    return FALSE;
}

static void on_child(GPid pid, gint wait_status, gpointer data) {
    Run *run = data;

    decode_status(wait_status, &run->status, &run->signal_no);
    /* ANTES DE CERRARLO: en Windows el tiempo de CPU se le pregunta al
       manejador del proceso, y despues de g_spawn_close_pid ya no hay. */
    coste_hijo_acabo(run->coste, pid);
    g_spawn_close_pid(pid);
    run->vivo = FALSE;
    if (--run->pending == 0) run_finish(run);
}

static void watch_pipe(gint fd, Run *run, gboolean is_err) {
#ifdef G_OS_WIN32
    GIOChannel *channel = g_io_channel_win32_new_fd(fd);
#else
    GIOChannel *channel = g_io_channel_unix_new(fd);
#endif
    Pipe    *pipe = g_new0(Pipe, 1);
    GSource *src;

    pipe->run    = run;
    pipe->is_err = is_err;
    g_io_channel_set_encoding(channel, NULL, NULL);      /* bytes, sin UTF-8 */
    g_io_channel_set_flags(channel, G_IO_FLAG_NONBLOCK, NULL);
    g_io_channel_set_close_on_unref(channel, TRUE);
    /* En el contexto de la corrida: el de siempre para engine_start, uno
       propio para la espera de engine_run.                              */
    src = g_io_create_watch(channel, G_IO_IN | G_IO_HUP | G_IO_ERR);
    g_source_set_callback(src, (GSourceFunc) (void (*)(void)) on_pipe, pipe, NULL);
    g_source_attach(src, run->ctx);
    g_source_unref(src);
}

/* Parar una corrida. Es lo que faltaba: el pid estaba aqui dentro y no salia,
 * asi que una estimacion larga --o una evaluacion recursiva, que son muchas
 * seguidas-- no se podia abortar.
 *
 * Se manda SIGTERM y se deja que el ciclo normal recoja al hijo: el resultado
 * llega por done() como cualquier otro final, con status ENGINE_SIGNAL. No se
 * inventa un camino aparte para la cancelacion.                          */
void engine_stop(EngineJob *job) {
    Run *run = (Run *) job;

#ifndef G_OS_WIN32
    if (run && run->vivo) kill((pid_t) run->pid, SIGTERM);
#else
    if (run && run->vivo) TerminateProcess((HANDLE) run->pid, 1);
#endif
}

EngineJob *engine_start(const char *workdir, const char *program,
                        const char *const *argv,
                        EngineProgress progress, EngineSalida salida,
                        EngineDone done, gpointer data);

gboolean engine_run_async(const char *workdir, const char *program,
                          const char *const *argv,
                          EngineProgress progress, EngineDone done,
                          gpointer data) {
    return engine_start(workdir, program, argv, progress, NULL, done,
                        data) != NULL;
}

EngineJob *engine_start(const char *workdir, const char *program,
                        const char *const *argv,
                        EngineProgress progress, EngineSalida salida,
                        EngineDone done, gpointer data) {
    GError    *error = NULL;
    EngineJob *job = lanza(workdir, program, argv, NULL, FALSE, progress,
                           salida, done, data, &error);

    if (error) g_error_free(error);
    return job;
}

/* EL CAMINO DE LAS DOS: engine_start con el contexto de siempre y la entrada
 * heredada (como ha sido siempre), engine_run con uno propio y la entrada
 * cerrada. Aqui se pone en marcha la medida del coste.               */
static EngineJob *lanza(const char *workdir, const char *program,
                        const char *const *argv, GMainContext *ctx,
                        gboolean sin_entrada, EngineProgress progress,
                        EngineSalida salida, EngineDone done, gpointer data,
                        GError **error) {
    GPtrArray    *args = g_ptr_array_new_with_free_func(g_free);
    gchar        *path = find_program(program);
    GPid          pid;
    gint          in_fd = -1, out_fd = -1, err_fd = -1;
    Run          *run;
    CosteCorrida *coste;
    GSource      *src;
    int           i;

    g_ptr_array_add(args, path ? path : g_strdup(program));
    for (i = 0; argv != NULL && argv[i] != NULL; i++)
        g_ptr_array_add(args, g_strdup(argv[i]));
    g_ptr_array_add(args, NULL);

    coste = coste_empieza(workdir, program, argv);
    if (!g_spawn_async_with_pipes(workdir, (gchar **) args->pdata, NULL,
                                  G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                                  NULL, NULL, &pid,
                                  sin_entrada ? &in_fd : NULL,
                                  &out_fd, &err_fd, error)) {
        coste_descarta(coste);
        g_ptr_array_free(args, TRUE);
        return NULL;
    }
    g_ptr_array_free(args, TRUE);
    /* La entrada, cerrada en el acto: el motor lee fin de fichero. */
    if (in_fd >= 0) g_close(in_fd, NULL);

    run = g_new0(Run, 1);
    run->program  = g_strdup(program);
    run->progress = progress;
    run->salida   = salida;
    run->done     = done;
    run->data     = data;
    run->out      = g_string_new(NULL);
    run->err      = g_string_new(NULL);
    run->pending  = 3;                       /* las dos tuberias y el hijo  */
    run->pid      = pid;
    run->vivo     = TRUE;
    run->ctx      = ctx;
    run->coste    = coste;
    watch_pipe(out_fd, run, FALSE);
    watch_pipe(err_fd, run, TRUE);
    src = g_child_watch_source_new(pid);
    g_source_set_callback(src, (GSourceFunc) (void (*)(void)) on_child, run, NULL);
    g_source_attach(src, ctx);
    g_source_unref(src);
    return (EngineJob *) run;
}
