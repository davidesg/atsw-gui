/* test_gui.c -- el programa de verdad, conducido desde el codigo.
 *
 * Levanta la ventana principal (sin ensenarla), pone el directorio de
 * trabajo y el modelo, llama al boton de Run y espera. Luego dice lo que
 * quedo a la vista: la barra de avance y la barra de estado. Es la unica
 * manera de comprobar sin manos lo que el usuario acaba viendo.
 *
 *   test_gui <directorio> <modelo>
 *
 * Necesita un servidor grafico; si no lo hay, no falla: avisa y se va.
 */

#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>

#include "fue_context.h"
#include "main_window.h"
#include "fue_globals.h"
#include "data_handling.h"
#include "model_spec.h"
#include "file_io.h"

static int fails = 0;

static void check(int ok, const char *what, const char *saw) {
    if (ok) return;
    fails++;
    printf("FAIL: %s; se vio: \"%s\"\n", what, saw ? saw : "(nada)");
}

/* Deja correr el bucle principal hasta que no quede nada por hacer, o
 * hasta que pasen ms milisegundos.                                        */
static void pump(int ms) {
    gint64 until = g_get_monotonic_time() + ms * 1000;

    while (g_get_monotonic_time() < until) {
        while (gtk_events_pending()) gtk_main_iteration_do(FALSE);
        g_usleep(2000);
    }
}

int main(int argc, char **argv) {
    FueContext *ctx;
    const char *dir, *model;
    const char *status, *bar_text;
    gchar *inp;

    if (argc < 3) { fprintf(stderr, "usage: test_gui <dir> <model>\n"); return 2; }
    dir   = argv[1];
    model = argv[2];

    if (!gtk_init_check(&argc, &argv)) {
        printf("no hay servidor grafico: la prueba de la ventana no se corre\n");
        return 0;
    }

    ctx = g_new0(FueContext, 1);
    ctx->main_window = create_main_window(NULL, ctx);
    /* No se ensena: no hace falta para que los widgets funcionen y asi no
     * aparece una ventana por sorpresa.                                   */

    check(ctx->progress != NULL, "la barra de avance tiene que existir", NULL);
    check(ctx->status_label != NULL, "la barra de estado tiene que existir", NULL);
    if (ctx->progress == NULL || ctx->status_label == NULL) {
        printf("\n%d fallos\n", fails);
        return 1;
    }

    /* El modelo, como lo carga el usuario */
    inp = g_build_filename(dir, model, NULL);
    inp = (g_str_has_suffix(model, ".inp")) ? inp
          : (g_free(inp), g_strdup_printf("%s/%s.inp", dir, model));
    load_input_fue(inp);
    update_ui_from_model(ctx);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(ctx->workspace_file_chooser), dir);
    gtk_entry_set_text(GTK_ENTRY(ctx->input_name_entry), model);
    g_free(inp);

    /* Y le da a Run */
    on_run_fue(NULL, ctx);
    pump(1500);

    status   = gtk_label_get_text(GTK_LABEL(ctx->status_label));
    bar_text = gtk_progress_bar_get_text(GTK_PROGRESS_BAR(ctx->progress));

    printf("barra de estado : %s\n", status ? status : "(nada)");
    printf("barra de avance : %s  (visible: %s, %.0f%%)\n",
           bar_text ? bar_text : "(nada)",
           gtk_widget_get_visible(ctx->progress) ? "si" : "NO",
           100.0 * gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(ctx->progress)));

    check(gtk_widget_get_visible(ctx->progress),
          "la barra de avance tiene que quedarse a la vista", bar_text);
    check(bar_text != NULL && strstr(bar_text, "iteration") != NULL,
          "y decir cuantas iteraciones hubo", bar_text);
    check(status != NULL && strstr(status, "finished") != NULL,
          "la barra de estado tiene que decir que fue acabo", status);
    check(status != NULL && strstr(status, "CONVERGENCE") != NULL,
          "y como convergio", status);
    check(!ctx->running, "y no quedarse pensando que sigue corriendo", status);

    printf("\n%d fallos\n", fails);
    return fails == 0 ? 0 : 1;
}
