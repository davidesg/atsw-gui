/*
 * fug_run.c
 *
 * Run the FUG engine and open its results.
 *
 * fug is started directly (no shell) in the workspace folder, so paths
 * with spaces work, and its exit status and messages are reported to the
 * user. The graphs are shown in the graph window of the GUI (preview.c);
 * other files are opened with a viewer, without waiting for it.
 *
 * The engine is, in this order: the FUG environment variable, the fug
 * program next to the GUI (both are built together) or fug in the PATH.
 */

#include <string.h>
#include <gtk/gtk.h>
#include "fug_run.h"
#include "data_load.h"
#include "preview.h"

/* Longest part of the fug console output shown in an error dialog */
#define MAX_REPORT 1500

static gchar *gui_dir = NULL;

/* Folder of the GUI executable, from argv[0] (or /proc/self/exe) */
void fug_run_init(const char *argv0)
{
    gchar *self = g_file_read_link("/proc/self/exe", NULL), *found;

    if (self != NULL) {
        gui_dir = g_path_get_dirname(self);
        g_free(self);
    } else if (argv0 != NULL && (strchr(argv0, '/') != NULL || strchr(argv0, '\\') != NULL)) {
        gui_dir = g_path_get_dirname(argv0);
    } else if (argv0 != NULL && (found = g_find_program_in_path(argv0)) != NULL) {
        gui_dir = g_path_get_dirname(found);
        g_free(found);
    }
}

/* DONDE ESTA EL MOTOR, y el orden importa.
 *
 * Se buscaba al lado del GUI y, si no, en el PATH. Pero EN EL MONOREPO EL
 * MOTOR NO ESTA AL LADO: el GUI vive en gui/fug y el motor en engines/fug, asi
 * que la busqueda de al lado fallaba siempre y mandaba el PATH.
 *
 * Y en el PATH podia haber --habia-- un fug 1.14 de una instalacion vieja, que
 * NO CONOCE -B. Lo peor no es que lo rechace: es que SALE CON CODIGO 0, asi
 * que el GUI creia que habia ido bien y se quedaba esperando unos graficos que
 * nunca se hicieron. Eso es lo que parecia un cuelgue.
 *
 * Orden: la variable FUG manda --es un override explicito--, despues los
 * sitios del arbol de compilacion, y por ultimo el PATH.               */
const gchar *fug_program(void)
{
    static gchar *program = NULL;
    const gchar *fug = g_getenv("FUG");
#ifdef G_OS_WIN32
    static const char *EXE = "fug.exe";
#else
    static const char *EXE = "fug";
#endif
    static const char *sitio[] = {
        "%s",                        /* al lado, como estaba              */
        "../../engines/fug/%s",      /* el monorepo: gui/fug -> engines/fug */
        "../../engines/fug/bin/%s",
        NULL
    };
    int i;

    if (fug && *fug)
        return fug;
    if (program != NULL)
        return program;

    for (i = 0; gui_dir && sitio[i]; i++) {
        gchar *rel = g_strdup_printf(sitio[i], EXE);
        gchar *p   = g_build_filename(gui_dir, rel, NULL);

        g_free(rel);
        if (g_file_test(p, G_FILE_TEST_IS_EXECUTABLE)) { program = p; return program; }
        g_free(p);
    }

    program = g_strdup(EXE);
    return program;
}

void show_error(AppWidgets *app, const gchar *format, ...)
{
    GtkWidget *dialog;
    gchar *message;
    va_list ap;

    va_start(ap, format);
    message = g_strdup_vprintf(format, ap);
    va_end(ap);

    dialog = gtk_message_dialog_new(app ? GTK_WINDOW(app->window) : NULL,
                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", message);
    gtk_window_set_title(GTK_WINDOW(dialog), "FUG");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    g_free(message);
}

void show_status(AppWidgets *app, const gchar *format, ...)
{
    GtkStatusbar *bar;
    guint context;
    gchar *message;
    va_list ap;

    if (app == NULL)
        return;
    bar = GTK_STATUSBAR(app->main_statusbar);
    context = gtk_statusbar_get_context_id(bar, "fug");
    va_start(ap, format);
    message = g_strdup_vprintf(format, ap);
    va_end(ap);

    gtk_statusbar_pop(bar, context);
    gtk_statusbar_push(bar, context, message);
    g_free(message);
}

/* ---------------------------------------------------------------------- */
/* Workspace folder                                                       */
/* ---------------------------------------------------------------------- */

/* Folder where the .inp file is written and fug is run (g_free it). */
gchar *get_workspace(AppWidgets *app)
{
    GtkFileChooser *chooser = GTK_FILE_CHOOSER(app->workspace_filechooserbutton);
    gchar *folder = gtk_file_chooser_get_filename(chooser);

    if (folder == NULL)
        folder = gtk_file_chooser_get_current_folder(chooser);
    return folder;
}

void set_workspace(AppWidgets *app, const gchar *folder)
{
    GtkFileChooser *chooser = GTK_FILE_CHOOSER(app->workspace_filechooserbutton);

    gtk_file_chooser_unselect_all(chooser);
    gtk_file_chooser_set_current_folder(chooser, folder);
}

/* ---------------------------------------------------------------------- */
/* Running fug                                                            */
/* ---------------------------------------------------------------------- */

/* New argument vector "fug <handler> -B lambda m d D"; free it with
 * g_ptr_array_free(args, TRUE). */
GPtrArray *fug_args_new(const gchar *handler)
{
    GPtrArray *args = g_ptr_array_new_with_free_func(g_free);

    gchar buf[G_ASCII_DTOSTR_BUF_SIZE];

    g_ptr_array_add(args, g_strdup(fug_program()));
    g_ptr_array_add(args, g_strdup(handler));
    /* The transformation of the main window: the .inp may be a fue model */
    g_ptr_array_add(args, g_strdup("-B"));
    g_ptr_array_add(args, g_strdup(g_ascii_formatd(buf, sizeof buf, "%.6f", Tm.boxlam)));
    g_ptr_array_add(args, g_strdup(g_ascii_formatd(buf, sizeof buf, "%.6f", Tm.boxm)));
    fug_args_add(args, "%d", Tm.nrdiff);
    fug_args_add(args, "%d", Tm.nadiff);
    return args;
}

void fug_args_add(GPtrArray *args, const gchar *format, ...)
{
    va_list ap;

    va_start(ap, format);
    g_ptr_array_add(args, g_strdup_vprintf(format, ap));
    va_end(ap);
}

static void set_busy(AppWidgets *app, gboolean busy)
{
    GdkWindow *window = gtk_widget_get_window(app->window);

    if (busy) {
        /* En GTK3 el cursor se pide al display, y se libera con g_object_unref */
        GdkCursor *cursor = gdk_cursor_new_for_display(gdk_display_get_default(),
                                                       GDK_WATCH);
        if (window != NULL)
            gdk_window_set_cursor(window, cursor);
        if (cursor != NULL) g_object_unref(cursor);
        gtk_widget_set_sensitive(app->window, FALSE);
    }
    /* Clicks made while fug runs reach an insensitive window: dropped */
    while (gtk_events_pending())
        gtk_main_iteration();
    if (!busy) {
        gtk_widget_set_sensitive(app->window, TRUE);
        if (window != NULL)
            gdk_window_set_cursor(window, NULL);
    }
}

/* Run fug in the workspace folder and wait for it. Errors are reported to
 * the user; returns TRUE when fug finished successfully. */
gboolean run_fug(AppWidgets *app, const gchar *workspace, GPtrArray *args)
{
    gchar *out = NULL, *err = NULL, *cmdline, *report;
    GError *error = NULL;
    gint status;
    gboolean ok;
    gsize len;

    g_ptr_array_add(args, NULL);
    cmdline = g_strjoinv(" ", (gchar **) args->pdata);

    show_status(app, "Running: %s", cmdline);
    set_busy(app, TRUE);
    ok = g_spawn_sync(workspace, (gchar **) args->pdata, NULL, G_SPAWN_SEARCH_PATH,
                      NULL, NULL, &out, &err, &status, &error);
    set_busy(app, FALSE);
    g_ptr_array_remove_index(args, args->len - 1);

    if (!ok) {
        show_error(app, "Can not run the FUG engine \"%s\":\n%s\n\n"
                        "Hint: check that fug is installed and in the PATH "
                        "(or set the FUG environment variable).",
                   fug_program(), error->message);
        show_status(app, "Error running fug");
        g_error_free(error);
    } else if (!g_spawn_check_exit_status(status, &error)) {
        ok = FALSE;
        /* keep only the end of the console output */
        len = strlen(out);
        report = g_strconcat(err, (len > MAX_REPORT) ? "...\n" : "",
                             out + ((len > MAX_REPORT) ? len - MAX_REPORT : 0), NULL);
        show_error(app, "fug failed (%s)\n\nCommand: %s\nFolder: %s\n\n%s",
                   error->message, cmdline, workspace, g_strstrip(report));
        show_status(app, "fug failed: %s", cmdline);
        g_free(report);
        g_error_free(error);
    } else {
        show_status(app, "Done: %s", cmdline);
    }

    g_free(cmdline);
    g_free(out);
    g_free(err);
    return ok;
}

/* ---------------------------------------------------------------------- */
/* Viewers                                                                */
/* ---------------------------------------------------------------------- */

static gboolean is_graph(const gchar *filename)
{
    const gchar *ext = getExt(filename);

    return g_ascii_strcasecmp(ext, ".eps") == 0 || g_ascii_strcasecmp(ext, ".ps") == 0 ||
           g_ascii_strcasecmp(ext, ".pdf") == 0;
}

/* Open a file of the workspace: the graphs of fug in the graph window of
 * the GUI (preview.c), other files with a viewer (open_external). */
gboolean open_viewer(AppWidgets *app, const gchar *workspace, const gchar *filename)
{
    gchar *path = g_build_filename(workspace, filename, NULL);
    gboolean ok;

    if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
        show_error(app, "The file was not created by fug:\n%s", path);
        g_free(path);
        return FALSE;
    }
    ok = is_graph(filename) && preview_show(app, path);
    if (ok)
        show_status(app, "Showing %s", filename);
    else
        ok = open_external(app, path);
    g_free(path);
    return ok;
}

/* Open a file with a viewer, without waiting: gv for EPS/PS/PDF and gedit
 * for text files if they are installed (Windows: Notepad for text files),
 * else the application of the system. */
gboolean open_external(AppWidgets *app, const gchar *path)
{
    gchar *viewer, *folder, *uri, *argv[3];
    GError *error = NULL;
    gboolean ok;

#ifdef G_OS_WIN32
    viewer = is_graph(path) ? NULL : g_find_program_in_path("notepad.exe");
#else
    viewer = g_find_program_in_path(is_graph(path) ? "gv" : "gedit");
#endif
    if (viewer != NULL) {
        folder = g_path_get_dirname(path);
        argv[0] = viewer;
        argv[1] = (gchar *) path;
        argv[2] = NULL;
        ok = g_spawn_async(folder, argv, NULL, 0, NULL, NULL, NULL, &error);
        g_free(folder);
        g_free(viewer);
    } else {
        uri = g_filename_to_uri(path, NULL, &error);
        ok = uri != NULL && gtk_show_uri(app ? gtk_widget_get_screen(app->window) : NULL, uri,
                                         GDK_CURRENT_TIME, &error);
        g_free(uri);
    }
    if (!ok) {
        show_error(app, "Can not open the file:\n%s\n%s", path, error->message);
        g_error_free(error);
    } else {
        show_status(app, "Opened %s", path);
    }
    return ok;
}
