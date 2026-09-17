/* test_editor.c -- el editor del .inp, conducido desde el codigo.
 *
 * La secuencia que hace el usuario: Edit .inp, tocar el texto, Save .inp, Run.
 * Lo que se comprueba es que lo que edito sigue ahi cuando el motor lo lee.
 *
 *   test_editor <directorio> <modelo>
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
#include "utils.h"

static int fails = 0;

static void check(int ok, const char *what, const char *saw) {
    if (ok) return;
    fails++;
    printf("FAIL: %s; se vio: \"%s\"\n", what, saw ? saw : "(nada)");
}

static void pump(int ms) {
    gint64 until = g_get_monotonic_time() + ms * 1000;

    while (g_get_monotonic_time() < until) {
        while (gtk_events_pending()) gtk_main_iteration_do(FALSE);
        g_usleep(2000);
    }
}

/* El .inp entero, tal como esta en el disco. */
static gchar *leer(const char *path) {
    gchar *s = NULL;

    if (!g_file_get_contents(path, &s, NULL, NULL)) return NULL;
    return s;
}

/* El texto que hay ahora mismo en la consola. */
static gchar *consola(FueContext *ctx) {
    GtkTextBuffer *b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctx->console_text_view));
    GtkTextIter a, z;

    gtk_text_buffer_get_bounds(b, &a, &z);
    return gtk_text_buffer_get_text(b, &a, &z, FALSE);
}

static void pon_consola(FueContext *ctx, const char *texto) {
    GtkTextBuffer *b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctx->console_text_view));

    gtk_text_buffer_set_text(b, texto, -1);
}

int main(int argc, char **argv) {
    FueContext *ctx;
    GtkApplication *app;
    const char *dir, *model;
    gchar *inp, *antes, *editado, *tras_guardar, *tras_correr, *p;

    if (argc < 3) { fprintf(stderr, "uso: test_editor <dir> <modelo>\n"); return 2; }
    dir   = argv[1];
    model = argv[2];

    if (!gtk_init_check(&argc, &argv)) {
        printf("no hay servidor grafico: la prueba del editor no se corre\n");
        return 0;
    }

    ctx = g_new0(FueContext, 1);
    app = gtk_application_new("org.atsw.fue.editor", G_APPLICATION_FLAGS_NONE);
    g_application_register(G_APPLICATION(app), NULL, NULL);
    ctx->main_window = create_main_window(app, ctx);
    gtk_widget_show_all(gtk_bin_get_child(GTK_BIN(ctx->main_window)));

    inp = g_strdup_printf("%s/%s.inp", dir, model);
    load_input_fue(inp);
    update_ui_from_model(ctx);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(ctx->workspace_file_chooser), dir);
    gtk_entry_set_text(GTK_ENTRY(ctx->input_name_entry), model);

    antes = leer(inp);
    check(antes != NULL, "el .inp de partida tiene que estar", inp);
    if (!antes) { printf("\n%d fallos\n", fails); return 1; }

    /* ------------------------------------------------------------------ */
    /* 1. Edit .inp -- el fichero tiene que aparecer en la consola          */
    /* ------------------------------------------------------------------ */
    on_edit_inp_clicked(NULL, ctx);
    pump(200);
    editado = consola(ctx);
    check(editado && strstr(editado, "Frequency") != NULL,
          "Edit .inp tiene que traer el fichero a la consola", editado);
    check(gtk_text_view_get_editable(GTK_TEXT_VIEW(ctx->console_text_view)),
          "y dejarlo editable", NULL);

    /* ------------------------------------------------------------------ */
    /* 2. El usuario toca un valor: la media pasa de fija a libre          */
    /* ------------------------------------------------------------------ */
    {
    gchar **l = g_strsplit(editado, "\n", -1);
    int i, tocada = -1;

    for (i = 0; l[i]; i++)
        if (strstr(l[i], "Mean parameter") && l[i + 1]) {
            g_free(l[i + 1]);
            l[i + 1] = g_strdup("-1.234567 1");
            tocada = i + 1;
            break;
        }
    check(tocada >= 0, "el .inp tiene que tener seccion de media", NULL);
    p = g_strjoinv("\n", l);
    pon_consola(ctx, p);
    g_free(p);
    g_strfreev(l);
    }

    /* ------------------------------------------------------------------ */
    /* 3. Save .inp                                                        */
    /* ------------------------------------------------------------------ */
    on_save_inp_clicked(NULL, ctx);
    pump(200);
    tras_guardar = leer(inp);
    check(tras_guardar && strstr(tras_guardar, "-1.234567 1") != NULL,
          "Save .inp tiene que dejar la edicion EN EL FICHERO", tras_guardar);

    /* ------------------------------------------------------------------ */
    /* 4. Run -- y aqui es donde se pierde                                 */
    /* ------------------------------------------------------------------ */
    on_run_fue(NULL, ctx);
    pump(1500);
    tras_correr = leer(inp);
    check(tras_correr && strstr(tras_correr, "-1.234567") != NULL,
          "y Run NO puede borrarla: el motor tiene que leer lo que se edito",
          tras_correr ? "la edicion desaparecio del .inp" : "(no se pudo leer)");

    /* ------------------------------------------------------------------ */
    /* 5. Y el modelo en memoria tiene que estar de acuerdo con el fichero */
    /* ------------------------------------------------------------------ */
    check(Tm.Imu == 1,
          "tras guardar, el modelo en memoria tiene que haber recargado la "
          "edicion (media libre)", Tm.Imu ? "libre" : "fija");

    /* ------------------------------------------------------------------ */
    /* 6. Y lo que el motor no podria leer, no se guarda -- y el fichero    */
    /*    bueno tiene que sobrevivir al intento                             */
    /* ------------------------------------------------------------------ */
    {
    gchar *bueno, *despues;
    const char *estado;

    bueno = leer(inp);
    on_edit_inp_clicked(NULL, ctx);
    pump(200);
    pon_consola(ctx, "esto no es un fichero de fue, ni de lejos\n");
    on_save_inp_clicked(NULL, ctx);
    pump(200);

    despues = leer(inp);
    check(despues && strcmp(bueno, despues) == 0,
          "un texto invalido NO puede tocar el fichero que habia",
          despues ? "el fichero cambio" : "(no se pudo leer)");

    estado = gtk_label_get_text(GTK_LABEL(ctx->status_label));
    check(estado && strstr(estado, "Not saved") != NULL,
          "y hay que decir que no se guardo", estado);
    check(estado && strstr(estado, "line") != NULL,
          "y por que, con la linea", estado);
    check(gtk_text_view_get_editable(GTK_TEXT_VIEW(ctx->console_text_view)),
          "y dejarlo editable para poder corregirlo", NULL);
    printf("rechazo         : %s\n", estado ? estado : "(nada)");

    g_free(bueno); g_free(despues);
    }

    /* ------------------------------------------------------------------ */
    /* 7. El .pre: se puede editar, y al guardar sale un .inp -- el .pre     */
    /*    no se pisa, porque su promesa es que es el optimo                  */
    /* ------------------------------------------------------------------ */
    {
    gchar *pre = g_strdup_printf("%s/%s.pre", dir, model);
    gchar *pre_antes, *pre_despues, *inp_despues, *texto;
    const char *estado;

    /* el Run de antes dejo un .pre */
    pre_antes = leer(pre);
    check(pre_antes != NULL, "tras Run tiene que haber un .pre", pre);

    if (pre_antes) {
        on_edit_pre_clicked(NULL, ctx);
        pump(200);
        texto = consola(ctx);
        check(texto && strstr(texto, "Frequency") != NULL,
              "Edit .pre tiene que traer el .pre a la consola", texto);
        estado = gtk_label_get_text(GTK_LABEL(ctx->status_label));
        check(estado && strstr(estado, ".inp") != NULL,
              "y avisar de que al guardar sale un .inp", estado);
        printf("aviso del pre   : %s\n", estado ? estado : "(nada)");

        /* se toca un valor y se guarda */
        {
        gchar **l = g_strsplit(texto, "\n", -1);
        int i;
        for (i = 0; l[i]; i++)
            if (strstr(l[i], "Mean parameter") && l[i + 1]) {
                g_free(l[i + 1]); l[i + 1] = g_strdup("-9.876543 1"); break;
            }
        p = g_strjoinv("\n", l);
        pon_consola(ctx, p);
        g_free(p); g_strfreev(l);
        }
        on_save_inp_clicked(NULL, ctx);
        pump(200);

        pre_despues = leer(pre);
        inp_despues = leer(inp);
        check(pre_despues && strcmp(pre_antes, pre_despues) == 0,
              "el .pre NO se toca: sigue siendo el optimo que decia ser",
              pre_despues ? "el .pre cambio" : "(no se pudo leer)");
        check(inp_despues && strstr(inp_despues, "-9.876543") != NULL,
              "y lo editado va al .inp", inp_despues ? "no esta en el .inp" : "(nada)");
        estado = gtk_label_get_text(GTK_LABEL(ctx->status_label));
        check(estado && strstr(estado, "specification") != NULL,
              "y hay que decir que ahora es una especificacion", estado);
        printf("tras guardar    : %s\n", estado ? estado : "(nada)");

        g_free(texto); g_free(pre_despues); g_free(inp_despues);
    }
    g_free(pre_antes); g_free(pre);
    }

    printf("\n%d fallos\n", fails);
    g_free(antes); g_free(editado); g_free(tras_guardar); g_free(tras_correr);
    g_free(inp);
    return fails ? 1 : 0;
}
