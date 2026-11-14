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
#include "forecast_tab.h"
#include "proyecto.h"

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

/* UN ASIGNADOR QUE LIBERA SU CAMPO ANTES DE LEER SU ARGUMENTO ES UNA TRAMPA.
 *
 * set_current_inp_from_path() hacia justo eso, y hay quien le pasa el propio
 * campo: «Run FUF» recarga la entrada con que previo. Lo que se leia despues
 * era memoria muerta, y en la barra salia «Active model:» seguido de
 * simbolos raros -- el nombre, sacado de un hueco ya liberado.
 *
 * La trampa es del asignador, no de quien llama.                       */
static void test_inp_path_desde_si_mismo(FueContext *ctx) {
    set_current_inp_from_path(ctx, "/tmp/a/forecast_X.inp");
    check(ctx->forecast_current_base
          && strcmp(ctx->forecast_current_base, "forecast_X") == 0,
          "el nombre base sale de la ruta", ctx->forecast_current_base);

    /* Y ahora con SU PROPIO campo, que es lo que hace Run FUF. */
    set_current_inp_from_path(ctx, ctx->forecast_current_inp_path);
    check(ctx->forecast_current_base
          && strcmp(ctx->forecast_current_base, "forecast_X") == 0,
          "pasarle su propio campo NO puede leer memoria liberada",
          ctx->forecast_current_base);
    check(ctx->forecast_current_inp_path
          && strcmp(ctx->forecast_current_inp_path, "/tmp/a/forecast_X.inp") == 0,
          "y la ruta sigue siendo la que era", ctx->forecast_current_inp_path);
}

/* GUARDAR SOBRE UN MODELO ESTIMADO DERIVA (docs/AUDITORIA-procesos.md, C3):
 * un proyecto con IPC/m00 (los datos) y IPC/m01 estimado; se abre m01, se
 * pulsa Guardar, y m01.inp no se toca: lo guardado es m02, hijo suyo.   */
Proyecto *fue_proyecto(void);
void      fue_pon_proyecto(Proyecto *p);

static void test_guardar_deriva(FueContext *ctx, const char *dir, const char *model) {
    gchar   *proy = g_build_filename(dir, "proy_guardar", NULL);
    gchar   *work = g_build_filename(proy, "IPC", "work", NULL);
    gchar   *src  = g_strdup_printf("%s/%s.inp", dir, model);
    gchar   *yaml = g_build_filename(proy, "proyecto.yaml", NULL);
    gchar   *m01  = g_build_filename(work, "IPC_m01.inp", NULL);
    gchar   *m02  = g_build_filename(work, "IPC_m02.inp", NULL);
    gchar   *out  = g_build_filename(work, "IPC_m01.out", NULL);
    gchar   *c = NULL, *antes = NULL, *despues = NULL;
    gsize    n = 0;
    Proyecto *p = g_new0(Proyecto, 1), *d = g_new0(Proyecto, 1), *viejo = fue_proyecto();
    PrError  e;
    char     id[PR_ID];
    const char *st;

    /* EL MANIFIESTO ANTES QUE LOS FICHEROS: un ID nuevo salta los que ya
       estan en disco (docs/DISENO-proyecto.md §5), y con IPC_m01.inp puesto
       de antemano el hijo de m00 seria m02.                          */
    g_mkdir_with_parents(proy, 0755);
    pr_nuevo(p, "g", "guardar", ".");
    snprintf(p->path, sizeof p->path, "%s", yaml);
    pr_serie_add(p, "IPC", &e);
    pr_deriva_rol(p, "IPC", "", NULL, PR_DATOS, id, sizeof id, NULL, 0, &e);
    pr_deriva(p, "IPC", "", "m00", id, sizeof id, NULL, 0, &e);
    check(!strcmp(id, "m01"), "el hijo de m00 es m01", id);
    check(pr_escribir(p, yaml, &e) == 0, "el proyecto de la prueba se escribe", yaml);
    fue_pon_proyecto(p);

    g_mkdir_with_parents(work, 0755);
    g_file_get_contents(src, &c, &n, NULL);
    g_file_set_contents(m01, c, (gssize) n, NULL);
    g_free(c);
    c = g_build_filename(work, "IPC_m00.inp", NULL);
    g_file_get_contents(src, &antes, &n, NULL);
    g_file_set_contents(c, antes, (gssize) n, NULL);
    g_free(c);
    g_file_set_contents(out, "un .out: m01 esta estimado\n", -1, NULL);


    load_input_fue(m01);
    update_ui_from_model(ctx);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(ctx->workspace_file_chooser), work);
    pump(300);
    gtk_entry_set_text(GTK_ENTRY(ctx->input_name_entry), "IPC_m01");
    g_file_get_contents(m01, &antes, NULL, NULL);

    on_save_inp(NULL, ctx);
    st = gtk_label_get_text(GTK_LABEL(ctx->status_label));
    g_file_get_contents(m01, &despues, NULL, NULL);
    check(antes && despues && !strcmp(antes, despues),
          "Guardar sobre m01, ya estimado, no toca m01.inp", st);
    check(g_file_test(m02, G_FILE_TEST_EXISTS), "lo guardado es m02.inp", st);
    check(st && strstr(st, "m02") != NULL, "y la barra lo dice", st);
    check(pr_leer(yaml, d, &e) == 0 && pr_modelo_idx(d, "IPC", "", "m02") >= 0 &&
          !strcmp(d->m[pr_modelo_idx(d, "IPC", "", "m02")].padre, "m01"),
          "el manifiesto tiene m02, hijo de m01", yaml);

    fue_pon_proyecto(viejo);
    g_free(antes); g_free(despues); g_free(d);
    g_free(proy); g_free(work); g_free(src); g_free(yaml);
    g_free(m01); g_free(m02); g_free(out);
}

int main(int argc, char **argv) {
    FueContext *ctx;
    GtkApplication *app;
    const char *dir, *model;
    const char *status, *bar_text;
    gchar *tip;
    gchar *inp;

    if (argc < 3) { fprintf(stderr, "usage: test_gui <dir> <model>\n"); return 2; }
    dir   = argv[1];
    model = argv[2];

    if (!gtk_init_check(&argc, &argv)) {
        printf("no hay servidor grafico: la prueba de la ventana no se corre\n");
        return 0;
    }

    ctx = g_new0(FueContext, 1);
    app = gtk_application_new("org.atsw.fue.test", G_APPLICATION_FLAGS_NONE);
    g_application_register(G_APPLICATION(app), NULL, NULL);
    ctx->main_window = create_main_window(app, ctx);
    /* Se "ensenan" los widgets -- hace falta para que el cuaderno pueda
     * cambiar de pestana -- pero la ventana NO se muestra, asi que no
     * aparece nada en la pantalla.                                       */
    gtk_widget_show_all(gtk_bin_get_child(GTK_BIN(ctx->main_window)));

    check(ctx->progress != NULL, "la barra de avance tiene que existir", NULL);
    check(ctx->status_label != NULL, "la barra de estado tiene que existir", NULL);
    if (ctx->progress == NULL || ctx->status_label == NULL) {
        printf("\n%d fallos\n", fails);
        return 1;
    }

    test_inp_path_desde_si_mismo(ctx);

    /* El modelo, como lo carga el usuario */
    inp = g_build_filename(dir, model, NULL);
    inp = (g_str_has_suffix(model, ".inp")) ? inp
          : (g_free(inp), g_strdup_printf("%s/%s.inp", dir, model));
    load_input_fue(inp);
    update_ui_from_model(ctx);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(ctx->workspace_file_chooser), dir);
    gtk_entry_set_text(GTK_ENTRY(ctx->input_name_entry), model);
    g_free(inp);

    /* LA ESQUINA DICE QUE MODELO HAY, Y LO DICE AL CARGARLO.
     *
     * Decia el anterior --o "(none)"-- hasta que se guardaba el .inp: el
     * rotulo se actualizaba en UN sitio, el de guardar, en vez de escuchar a
     * la entrada de la que sale. Se vio al abrir un modelo desde la madre,
     * que es un camino que nunca pasa por guardar.                     */
    {
    const char *m = gtk_label_get_text(GTK_LABEL(ctx->model_label));

    check(m && strstr(m, model) != NULL,
          "«Model:» dice el modelo en cuanto se carga, sin guardar nada", m);
    }

    /* Y le da a Run */
    on_run_fue(NULL, ctx);
    pump(1500);

    status   = gtk_label_get_text(GTK_LABEL(ctx->status_label));
    bar_text = gtk_progress_bar_get_text(GTK_PROGRESS_BAR(ctx->progress));
    tip      = gtk_widget_get_tooltip_text(ctx->status_label);

    printf("barra de estado : %s\n", status ? status : "(nada)");
    printf("barra de avance : %s  (visible: %s, %.0f%%)\n",
           bar_text ? bar_text : "(nada)",
           gtk_widget_get_visible(ctx->progress) ? "si" : "NO",
           100.0 * gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(ctx->progress)));
    printf("globo           : %s\n", tip ? g_strdelimit(tip, "\n", ' ') : "(nada)");
    printf("pestana         : %d (la consola es la %d)\n",
           gtk_notebook_get_current_page(GTK_NOTEBOOK(ctx->notebook)), ctx->console_page);

    check(gtk_widget_get_visible(ctx->progress),
          "la barra de avance tiene que quedarse a la vista", bar_text);
    check(bar_text != NULL && strstr(bar_text, "it.") != NULL,
          "y decir cuantas iteraciones hubo", bar_text);
    check(status != NULL && strstr(status, "ha terminado") != NULL,
          "la barra de estado tiene que decir que fue acabo", status);
    check(status != NULL && strstr(status, "convergió") != NULL,
          "y si convergio", status);
    check(status != NULL && strlen(status) < 60,
          "y ser corta, para no estirar la ventana", status);
    check(tip != NULL && strstr(tip, "GRADIENT") != NULL,
          "el texto entero del motor tiene que quedar en el globo", tip);
    check(gtk_notebook_get_current_page(GTK_NOTEBOOK(ctx->notebook)) == ctx->console_page,
          "y saltar a la consola, donde esta todo lo que escribio", status);
    check(!ctx->running, "y no quedarse pensando que sigue corriendo", status);
    g_free(tip);

    test_guardar_deriva(ctx, dir, model);

    printf("\n%d fallos\n", fails);
    return fails == 0 ? 0 : 1;
}
