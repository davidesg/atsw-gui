#include "forecast_tab.h"
#include <glib/gstdio.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* Funciones auxiliares */
//static void load_file_to_editor(FueContext *ctx, const char *filename);
static void save_editor_to_file(FueContext *ctx, const char *filename);
//static void set_current_inp_from_path(FueContext *ctx, const char *inp_path);
static gboolean run_fuf_process(const char *base_name, const char *workdir);
static void open_pdf_file(const char *pdf_path);

/* Callbacks */
static void on_forecast_load_clicked(GtkButton *btn, FueContext *ctx);
static void on_forecast_save_as_clicked(GtkButton *btn, FueContext *ctx);
static void on_forecast_run_clicked(GtkButton *btn, FueContext *ctx);
static void on_forecast_view_pdf_clicked(GtkButton *btn, FueContext *ctx);

/* ------------------------------------------------------------------------- */
/* Creación del tab completo                                                */
/* ------------------------------------------------------------------------- */
GtkWidget* create_forecast_tab(FueContext *ctx) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);

    /* Barra superior */
    GtkWidget *top_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

    ctx->forecast_file_chooser = gtk_file_chooser_button_new("Select .inp or .pre file",
                                                              GTK_FILE_CHOOSER_ACTION_OPEN);
    gtk_box_pack_start(GTK_BOX(top_bar), ctx->forecast_file_chooser, TRUE, TRUE, 0);

    ctx->forecast_load_button = gtk_button_new_with_label("Load");
    g_signal_connect(ctx->forecast_load_button, "clicked",
                     G_CALLBACK(on_forecast_load_clicked), ctx);
    gtk_box_pack_start(GTK_BOX(top_bar), ctx->forecast_load_button, FALSE, FALSE, 0);

    ctx->forecast_save_inp_button = gtk_button_new_with_label("Save as .inp");
    g_signal_connect(ctx->forecast_save_inp_button, "clicked",
                     G_CALLBACK(on_forecast_save_as_clicked), ctx);
    gtk_box_pack_start(GTK_BOX(top_bar), ctx->forecast_save_inp_button, FALSE, FALSE, 0);

    ctx->forecast_run_button = gtk_button_new_with_label("Run FUF");
    g_signal_connect(ctx->forecast_run_button, "clicked",
                     G_CALLBACK(on_forecast_run_clicked), ctx);
    gtk_box_pack_start(GTK_BOX(top_bar), ctx->forecast_run_button, FALSE, FALSE, 0);

    ctx->forecast_view_pdf_button = gtk_button_new_with_label("View PDF");
    g_signal_connect(ctx->forecast_view_pdf_button, "clicked",
                     G_CALLBACK(on_forecast_view_pdf_clicked), ctx);
    gtk_box_pack_start(GTK_BOX(top_bar), ctx->forecast_view_pdf_button, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), top_bar, FALSE, FALSE, 0);

    /* Editor de texto */
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    ctx->forecast_editor = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(ctx->forecast_editor), GTK_WRAP_NONE);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(ctx->forecast_editor), TRUE);
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        "textview { font-family: monospace; }", -1, NULL);
    gtk_style_context_add_provider(gtk_widget_get_style_context(ctx->forecast_editor),
                                   GTK_STYLE_PROVIDER(provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
    gtk_container_add(GTK_CONTAINER(scrolled), ctx->forecast_editor);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);

    /* Barra de estado */
    ctx->forecast_status_label = gtk_label_new("Ready");
    gtk_label_set_xalign(GTK_LABEL(ctx->forecast_status_label), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), ctx->forecast_status_label, FALSE, FALSE, 0);

    return vbox;
}


/* ------------------------------------------------------------------------- */
/* Carga un archivo en el editor (mismo formato que la consola principal)   */
/* ------------------------------------------------------------------------- */
void load_file_to_editor(FueContext *ctx, const char *filename) {
    gchar *content = NULL;
    gsize len = 0;
    GError *error = NULL;

    if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
        gchar *msg = g_strdup_printf("File not found: %s", filename);
        gtk_label_set_text(GTK_LABEL(ctx->forecast_status_label), msg);
        g_free(msg);
        return;
    }

    if (g_file_get_contents(filename, &content, &len, &error)) {
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctx->forecast_editor));
        /* Verificar si el contenido es UTF-8 válido */
        if (g_utf8_validate(content, len, NULL)) {
            gtk_text_buffer_set_text(buffer, content, len);
        } else {
            /* Reemplazar caracteres no válidos por '?' */
            gchar *valid_utf8 = g_utf8_make_valid(content, len);
            gtk_text_buffer_set_text(buffer, valid_utf8, -1);
            g_free(valid_utf8);
            gtk_label_set_text(GTK_LABEL(ctx->forecast_status_label),
                               "Warning: File contains invalid UTF-8 characters (replaced).");
        }
        g_free(content);
        gchar *msg = g_strdup_printf("Loaded: %s", filename);
        gtk_label_set_text(GTK_LABEL(ctx->forecast_status_label), msg);
        g_free(msg);
    } else {
        gchar *msg = g_strdup_printf("Error loading file: %s", error->message);
        gtk_label_set_text(GTK_LABEL(ctx->forecast_status_label), msg);
        g_free(msg);
        g_error_free(error);
        return;
    }

    /* Si es un .inp, actualizar estado interno */
    if (g_str_has_suffix(filename, ".inp")) {
        set_current_inp_from_path(ctx, filename);
    } else {
        if (ctx->forecast_loaded_path) g_free(ctx->forecast_loaded_path);
        ctx->forecast_loaded_path = g_strdup(filename);
    }
}

/* ------------------------------------------------------------------------- */
/* Guarda el contenido actual del editor en un archivo (sin preguntar)      */
/* ------------------------------------------------------------------------- */
static void save_editor_to_file(FueContext *ctx, const char *filename) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctx->forecast_editor));
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    gchar *content = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    FILE *f = NULL;

#ifdef _WIN32
    wchar_t *wpath = g_utf8_to_utf16(filename, -1, NULL, NULL, NULL);
    if (wpath) {
        f = _wfopen(wpath, L"w");
        g_free(wpath);
    }
#else
    f = fopen(filename, "w");
#endif

    if (f) {
        fwrite(content, 1, strlen(content), f);
        fclose(f);
        gchar *msg = g_strdup_printf("Saved: %s", filename);
        gtk_label_set_text(GTK_LABEL(ctx->forecast_status_label), msg);
        g_free(msg);
        if (g_str_has_suffix(filename, ".inp")) {
            set_current_inp_from_path(ctx, filename);
        }
    } else {
        gchar *msg = g_strdup_printf("Error saving: %s", strerror(errno));
        gtk_label_set_text(GTK_LABEL(ctx->forecast_status_label), msg);
        g_free(msg);
    }
    g_free(content);
}

/* ------------------------------------------------------------------------- */
/* Establece la ruta del .inp actual y extrae el directorio y nombre base   */
/* ------------------------------------------------------------------------- */
void set_current_inp_from_path(FueContext *ctx, const char *inp_path) {
    if (!inp_path) return;
    if (ctx->forecast_current_inp_path) g_free(ctx->forecast_current_inp_path);
    ctx->forecast_current_inp_path = g_strdup(inp_path);
    if (ctx->forecast_current_base) g_free(ctx->forecast_current_base);
    char *base = g_path_get_basename(inp_path);
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';
    ctx->forecast_current_base = g_strdup(base);
    g_free(base);
    gchar *msg = g_strdup_printf("Active model: %s", ctx->forecast_current_base);
    gtk_label_set_text(GTK_LABEL(ctx->forecast_status_label), msg);
    g_free(msg);
}

/* ------------------------------------------------------------------------- */
/* Ejecuta el proceso fuf (multiplataforma)                                 */
/* ------------------------------------------------------------------------- */
static gboolean run_fuf_process(const char *base_name, const char *workdir) {
    if (!base_name || !workdir) return FALSE;

#ifdef _WIN32
    char *full_path = g_find_program_in_path("fuf.exe");
    if (!full_path) {
        /* Buscar en el mismo directorio que el ejecutable principal */
        char *exe_path = g_win32_get_package_installation_directory_of_module(NULL);
        if (exe_path) {
            full_path = g_build_filename(exe_path, "fuf.exe", NULL);
            g_free(exe_path);
            if (!g_file_test(full_path, G_FILE_TEST_EXISTS)) {
                g_free(full_path);
                full_path = NULL;
            }
        }
    }
    if (!full_path) {
        g_print("fuf.exe not found.\n");
        return FALSE;
    }

    char *cmd_line = g_strdup_printf("\"%s\" \"%s\"", full_path, base_name);
    g_free(full_path);

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {0};
    wchar_t *wcmd = g_utf8_to_utf16(cmd_line, -1, NULL, NULL, NULL);
    wchar_t *wdir = g_utf8_to_utf16(workdir, -1, NULL, NULL, NULL);
    g_free(cmd_line);

    BOOL success = CreateProcessW(NULL, wcmd, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                                  NULL, wdir, &si, &pi);
    g_free(wcmd);
    g_free(wdir);
    if (success) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exit_code = 0;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return exit_code == 0;
    }
    return FALSE;
#else
    char *full_path = g_find_program_in_path("fuf");
    if (!full_path) full_path = g_strdup("./fuf");
    char *argv[] = { full_path, (char *)base_name, NULL };
    GError *error = NULL;
    gint exit_status = 0;
    gboolean ok = g_spawn_sync(workdir, argv, NULL,
                               G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                               NULL, NULL, NULL, NULL, &exit_status, &error);
    g_free(full_path);
    if (error) g_error_free(error);
    return ok && exit_status == 0;
#endif
}

/* ------------------------------------------------------------------------- */
/* Abre el PDF con el visor predeterminado                                   */
/* ------------------------------------------------------------------------- */
static void open_pdf_file(const char *pdf_path) {
#ifdef _WIN32
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "start \"\" \"%s\"", pdf_path);
    (void)system(cmd);
#else
    char *argv[] = { "xdg-open", (char *)pdf_path, NULL };
    g_spawn_async(NULL, argv, NULL,
                  G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                  NULL, NULL, NULL, NULL);
#endif
}

/* ------------------------------------------------------------------------- */
/* Callback: Cargar archivo seleccionado                                     */
/* ------------------------------------------------------------------------- */
static void on_forecast_load_clicked(GtkButton *btn, FueContext *ctx) {
    char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(ctx->forecast_file_chooser));
    if (!filename) {
        gtk_label_set_text(GTK_LABEL(ctx->forecast_status_label), "No file selected.");
        return;
    }
    load_file_to_editor(ctx, filename);
    g_free(filename);
}

/* ------------------------------------------------------------------------- */
/* Callback: Guardar como .inp (diálogo estándar)                            */
/* ------------------------------------------------------------------------- */
static void on_forecast_save_as_clicked(GtkButton *btn, FueContext *ctx) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Save as .inp",
                                                    GTK_WINDOW(ctx->main_window),
                                                    GTK_FILE_CHOOSER_ACTION_SAVE,
                                                    "_Cancel", GTK_RESPONSE_CANCEL,
                                                    "_Save", GTK_RESPONSE_ACCEPT,
                                                    NULL);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "FUE input files (*.inp)");
    gtk_file_filter_add_pattern(filter, "*.inp");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    if (ctx->forecast_current_base) {
        char *suggested = g_strdup_printf("%s.inp", ctx->forecast_current_base);
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), suggested);
        g_free(suggested);
    } else {
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "forecast.inp");
    }

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (filename) {
            save_editor_to_file(ctx, filename);
            g_free(filename);
        }
    }
    gtk_widget_destroy(dialog);
}

/* ------------------------------------------------------------------------- */
/* Callback: Ejecutar fuf                                                    */
/* ------------------------------------------------------------------------- */
static void on_forecast_run_clicked(GtkButton *btn, FueContext *ctx) {
    if (!ctx->forecast_current_inp_path) {
        gtk_label_set_text(GTK_LABEL(ctx->forecast_status_label),
                           "No .inp file active. Please load or save a .inp first.");
        return;
    }

    char *workdir = g_path_get_dirname(ctx->forecast_current_inp_path);
    if (!workdir) {
        gtk_label_set_text(GTK_LABEL(ctx->forecast_status_label),
                           "Could not determine workspace from .inp path.");
        return;
    }

    if (!g_file_test(ctx->forecast_current_inp_path, G_FILE_TEST_EXISTS)) {
        gtk_label_set_text(GTK_LABEL(ctx->forecast_status_label),
                           "INP file not found. Please save again.");
        g_free(workdir);
        return;
    }

    gtk_label_set_text(GTK_LABEL(ctx->forecast_status_label), "Running fuf...");

    gboolean process_ok = run_fuf_process(ctx->forecast_current_base, workdir);

    /* Pequeña pausa para que el sistema escriba los archivos */
    g_usleep(500000);

    /* Construir correctamente las rutas de salida */
    char *out_filename = g_strdup_printf("%s.out", ctx->forecast_current_base);
    char *out_path = g_build_filename(workdir, out_filename, NULL);
    g_free(out_filename);

    char *pdf_filename = g_strdup_printf("%s.pdf", ctx->forecast_current_base);
    char *pdf_path = g_build_filename(workdir, pdf_filename, NULL);
    g_free(pdf_filename);

    if (g_file_test(out_path, G_FILE_TEST_EXISTS)) {
        gtk_label_set_text(GTK_LABEL(ctx->forecast_status_label), "fuf finished successfully.");
        load_file_to_editor(ctx, out_path);
    } else {
        if (process_ok)
            gtk_label_set_text(GTK_LABEL(ctx->forecast_status_label),
                               "fuf finished but .out file not found.");
        else
            gtk_label_set_text(GTK_LABEL(ctx->forecast_status_label), "fuf failed.");
    }

    g_free(out_path);
    g_free(pdf_path);
    g_free(workdir);
}

/* ------------------------------------------------------------------------- */
/* Callback: Ver PDF                                                         */
/* ------------------------------------------------------------------------- */
static void on_forecast_view_pdf_clicked(GtkButton *btn, FueContext *ctx) {
    if (!ctx->forecast_current_inp_path) {
        gtk_label_set_text(GTK_LABEL(ctx->forecast_status_label),
                           "No .inp file active. Please load or save a .inp first.");
        return;
    }

    char *workdir = g_path_get_dirname(ctx->forecast_current_inp_path);
    if (!workdir) {
        gtk_label_set_text(GTK_LABEL(ctx->forecast_status_label),
                           "Could not determine workspace.");
        return;
    }

    char *pdf_filename = g_strdup_printf("%s.pdf", ctx->forecast_current_base);
    char *pdf_path = g_build_filename(workdir, pdf_filename, NULL);
    g_free(pdf_filename);

    if (g_file_test(pdf_path, G_FILE_TEST_EXISTS)) {
        open_pdf_file(pdf_path);
        gtk_label_set_text(GTK_LABEL(ctx->forecast_status_label), "Opening PDF.");
    } else {
        gtk_label_set_text(GTK_LABEL(ctx->forecast_status_label),
                           "PDF file not found. Run fuf first.");
    }

    g_free(pdf_path);
    g_free(workdir);
}

