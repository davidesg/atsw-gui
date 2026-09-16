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
static gchar *first_line(const char *text) {
    const char *p = text, *end;

    if (!text) return NULL;
    while (*p == '\n' || *p == '\r' || *p == ' ' || *p == '\t') p++;
    if (!*p) return NULL;
    end = strpbrk(p, "\r\n");
    if (!end) end = p + strlen(p);
    while (end > p && (end[-1] == ' ' || end[-1] == '\t')) end--;
    return (end > p) ? g_strndup(p, (gsize)(end - p)) : NULL;
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
