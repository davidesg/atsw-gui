/* engine.c -- running fue and fuf from the GUI. See include/engine.h. */

#include "engine.h"

#include <stdarg.h>
#include <string.h>

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

    switch (status) {
    case 0:  what = "finished";                                            break;
    case 1:  what = "could not read the input file";                       break;
    case 2:  what = "the input file is not valid, nothing was written";    break;
    case 3:  what = "could not estimate the model; the results carry the "
                    "initial values";                                      break;
    case 4:  what = "stopped on a run-time error";                         break;
    case ENGINE_SIGNAL:
        return g_strdup_printf("%s died on signal %d.", program, signal_no);
    default:
        return g_strdup_printf("%s ended with status %d.", program, status);
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

/* Se declara antes: la usan el camino sincrono y el asincrono */
static gchar *status_message(const char *program, int status, int signal_no,
                             const char *err, const char *out);

EngineResult engine_run(const char *workdir, const char *program, ...) {
    EngineResult r = { ENGINE_NORUN, 0, NULL, NULL };
    GPtrArray   *args;
    va_list      ap;
    const char  *a;
    gchar       *path;

    path = g_find_program_in_path(program);
    args = g_ptr_array_new_with_free_func(g_free);
    g_ptr_array_add(args, path ? path : g_strdup(program));
    va_start(ap, program);
    while ((a = va_arg(ap, const char *)) != NULL)
        g_ptr_array_add(args, g_strdup(a));
    va_end(ap);
    g_ptr_array_add(args, NULL);

#ifdef G_OS_WIN32
    {
    /* On Windows the engines are console programs: CreateProcessW with
     * CREATE_NO_WINDOW keeps a console from flashing over the GUI. The
     * output is not captured here, only the exit status.                  */
    GString             *cmd = g_string_new(NULL);
    STARTUPINFOW         si  = { sizeof(si) };
    PROCESS_INFORMATION  pi  = { 0 };
    wchar_t             *wcmd, *wdir;
    guint                i;

    for (i = 0; i + 1 < args->len; i++) {
        if (i) g_string_append_c(cmd, ' ');
        g_string_append_printf(cmd, "\"%s\"", (const char *)args->pdata[i]);
    }
    wcmd = g_utf8_to_utf16(cmd->str, -1, NULL, NULL, NULL);
    wdir = workdir ? g_utf8_to_utf16(workdir, -1, NULL, NULL, NULL) : NULL;
    g_string_free(cmd, TRUE);

    if (CreateProcessW(NULL, wcmd, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                       NULL, wdir, &si, &pi)) {
        DWORD code = 0;
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        r.status = (int)code;
    }
    g_free(wcmd);
    g_free(wdir);
    if (r.status == ENGINE_NORUN)
        r.message = g_strdup_printf("%s could not be run.", program);
    else
        r.message = status_message(program, r.status, 0, NULL, NULL);
    }
#else
    {
    gchar  *out = NULL, *err = NULL;
    GError *error = NULL;
    gint    wait_status = 0;

    if (!g_spawn_sync(workdir, (gchar **)args->pdata, NULL, G_SPAWN_SEARCH_PATH,
                      NULL, NULL, &out, &err, &wait_status, &error)) {
        r.message = g_strdup_printf("%s could not be run: %s", program,
                                    error ? error->message : "unknown error");
        if (error) g_error_free(error);
        g_free(out);
        g_free(err);
        g_ptr_array_free(args, TRUE);
        return r;
    }
    if (WIFEXITED(wait_status))
        r.status = WEXITSTATUS(wait_status);
    else if (WIFSIGNALED(wait_status)) {
        r.status    = ENGINE_SIGNAL;
        r.signal_no = WTERMSIG(wait_status);
    }
    r.message = status_message(program, r.status, r.signal_no, err, out);
    /* stderr first: that is where the engines put what went wrong */
    r.output  = g_strconcat(err ? err : "", (err && *err && out && *out) ? "\n" : "",
                            out ? out : "", NULL);
    if (r.output && !*r.output) { g_free(r.output); r.output = NULL; }
    g_free(out);
    g_free(err);
    }
#endif

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

    while (g_io_channel_read_chars(channel, buffer, sizeof(buffer), &n, NULL)
           == G_IO_STATUS_NORMAL && n > 0) {
        g_string_append_len(to, buffer, (gssize) n);
        if (to == run->out) {
            scan_iterations(run);
            /* EN VIVO. Sin esto el que llama solo ve la salida al acabar, y
             * en una corrida larga se queda mirando una caja vacia.      */
            if (run->salida) run->salida(buffer, n, run->data);
        }
    }
    if ((cond & (G_IO_HUP | G_IO_ERR)) == 0)
        return TRUE;

    g_io_channel_shutdown(channel, FALSE, NULL);
    g_io_channel_unref(channel);
    g_free(pipe);
    if (--run->pending == 0) run_finish(run);
    return FALSE;
}

static void on_child(GPid pid, gint wait_status, gpointer data) {
    Run *run = data;

#ifdef G_OS_WIN32
    run->status = wait_status;
#else
    if (WIFEXITED(wait_status))
        run->status = WEXITSTATUS(wait_status);
    else if (WIFSIGNALED(wait_status)) {
        run->status    = ENGINE_SIGNAL;
        run->signal_no = WTERMSIG(wait_status);
    }
#endif
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
    Pipe *pipe = g_new0(Pipe, 1);

    pipe->run    = run;
    pipe->is_err = is_err;
    g_io_channel_set_encoding(channel, NULL, NULL);      /* bytes, sin UTF-8 */
    g_io_channel_set_flags(channel, G_IO_FLAG_NONBLOCK, NULL);
    g_io_channel_set_close_on_unref(channel, TRUE);
    g_io_add_watch(channel, G_IO_IN | G_IO_HUP | G_IO_ERR, on_pipe, pipe);
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
    GPtrArray  *args = g_ptr_array_new_with_free_func(g_free);
    gchar      *path = g_find_program_in_path(program);
    GError     *error = NULL;
    GPid        pid;
    gint        out_fd = -1, err_fd = -1;
    Run        *run;
    int         i;

    g_ptr_array_add(args, path ? path : g_strdup(program));
    for (i = 0; argv != NULL && argv[i] != NULL; i++)
        g_ptr_array_add(args, g_strdup(argv[i]));
    g_ptr_array_add(args, NULL);

    if (!g_spawn_async_with_pipes(workdir, (gchar **) args->pdata, NULL,
                                  G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                                  NULL, NULL, &pid, NULL, &out_fd, &err_fd, &error)) {
        if (error) g_error_free(error);
        g_ptr_array_free(args, TRUE);
        return NULL;
    }
    g_ptr_array_free(args, TRUE);

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
    watch_pipe(out_fd, run, FALSE);
    watch_pipe(err_fd, run, TRUE);
    g_child_watch_add(pid, on_child, run);
    return (EngineJob *) run;
}
