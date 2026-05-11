/* main.c – entry point for FUE GUI */
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include "fue_context.h"
#include "main_window.h"
#include "fue_globals.h"       /* provides global structures */
#include "data_handling.h"  /* for load_data_file, etc. */
#include "model_spec.h"
#include "deterministic_dialog.h"
#include "operator_dialog.h"
#include "file_io.h"


#ifdef _WIN32
#include <windows.h>
#endif

static void init_global_flags(FueContext *ctx) {
    /* Pass the flags from the context to the global variables */
    ma_order_inc = ctx->ma_order_inc;
    ar_order_inc = ctx->ar_order_inc;
    new_det = ctx->new_det;
    type_op = ctx->type_op;
    new_op = ctx->new_op;

}


static void activate(GtkApplication *app, gpointer user_data) {
    FueContext *ctx = g_new0(FueContext, 1);

    /* Initialize global flags from context (they start at 0) */
    init_global_flags(ctx);

    /* Build the main window */
    ctx->main_window = create_main_window(app, ctx);
    gtk_widget_show_all(ctx->main_window);

    /* Set up status label and text view (already done in create_main_window) */

    /* Cleanup on window close */
    g_signal_connect(ctx->main_window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
}

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "C");

#ifdef _WIN32
    /* On Windows, set up resource paths for GTK (if needed) */
    wchar_t wpath[MAX_PATH];
    GetModuleFileNameW(NULL, wpath, MAX_PATH);
    char *exe_path = g_utf16_to_utf8(wpath, -1, NULL, NULL, NULL);
    char *exe_dir = g_path_get_dirname(exe_path);
    g_free(exe_path);

    char *data_dir = g_build_filename(exe_dir, "share", NULL);
    char *pixbuf_cache = g_build_filename(exe_dir, "lib", "gdk-pixbuf-2.0", "2.10.0", "loaders.cache", NULL);
    char *schema_dir = g_build_filename(exe_dir, "share", "glib-2.0", "schemas", NULL);

    g_setenv("XDG_DATA_DIRS", data_dir, TRUE);
    g_setenv("GDK_PIXBUF_MODULE_FILE", pixbuf_cache, TRUE);
    g_setenv("GSETTINGS_SCHEMA_DIR", schema_dir, TRUE);
    g_setenv("GTK_THEME", "Windows", TRUE);

    g_free(data_dir);
    g_free(pixbuf_cache);
    g_free(schema_dir);
    g_free(exe_dir);
#endif

    GtkApplication *app = gtk_application_new("org.fue.gui", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}
