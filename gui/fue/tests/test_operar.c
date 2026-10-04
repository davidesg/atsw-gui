/* test_operar.c -- fue_gui manejado entero desde el codigo, con los motores
 * de verdad.
 *
 * test_gui.c comprueba UNA cosa: que Run acaba diciendo lo que tiene que
 * decir. Esto recorre lo demas que el analista toca, por el mismo sitio por
 * donde lo toca --los botones de la barra, los de cada pestaña, los dialogos
 * de operadores y de deterministas, el selector de ficheros-- y mira lo que
 * queda a la vista: la barra de estado, las filas de los arboles, el texto
 * de la consola y de la prevision, los ficheros que se escriben al lado del
 * modelo y si las ventanas de graficos llegan a pintar algo.
 *
 *   test_operar <directorio de datos> <directorio de trabajo>
 *
 * El directorio de trabajo se da vacio; se llena aqui. Los motores (fue y
 * fuf) se buscan en el PATH, como los busca el programa.
 *
 * LOS DIALOGOS MODALES SE CONTESTAN SOLOS. gtk_dialog_run() no vuelve hasta
 * que alguien responde, y aqui no hay nadie: un temporizador mira las
 * ventanas abiertas, apunta lo que dice cada aviso --para poder comprobarlo
 * despues-- y lo cierra; a un selector de ficheros le pone el que toca.
 *
 * Sin servidor grafico no falla: avisa con una linea "no hay..." y se va.
 */

#include <stdio.h>
#include <string.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include "fue_context.h"
#include "main_window.h"
#include "fue_globals.h"
#include "data_handling.h"
#include "model_spec.h"
#include "file_io.h"
#include "forecast_tab.h"
#include "inpcheck.h"
#include "preview.h"
#include "proyecto.h"

/* gui/fue/src/arranque.c y analisis.c: no tienen cabecera propia. */
const char *fue_abrir(void);
void        fue_pon_abrir(const char *s);
const char *fue_raiz_proyecto(void);
void        fue_pon_raiz_proyecto(const char *s);
void        fue_pon_proyecto(Proyecto *p);
Proyecto   *fue_proyecto(void);
void        fue_on_diagnosis(GtkWidget *w, FueContext *ctx);

static int fails = 0;
static int checks = 0;

static void check(int ok, const char *what, const char *saw) {
    checks++;
    if (ok) return;
    fails++;
    printf("FAIL: %s; se vio: \"%s\"\n", what, saw ? saw : "(nada)");
    fflush(stdout);
}

static void pump(int ms) {
    gint64 until = g_get_monotonic_time() + (gint64) ms * 1000;

    while (g_get_monotonic_time() < until) {
        while (gtk_events_pending()) gtk_main_iteration_do(FALSE);
        g_usleep(2000);
    }
}

/* ------------------------------------------------------------------------ */
/* Quien contesta a los dialogos modales                                     */
/* ------------------------------------------------------------------------ */

/* Lo que dijeron los avisos desde la ultima vez que se miro. */
static GString *avisos;
/* El fichero que hay que elegir en el proximo selector; NULL = Cancelar. */
static gchar *elegir;
static int    elegir_espera;
static GtkWidget *contestado;     /* el selector al que ya se le dijo que si */

static void texto_de_aviso(GtkMessageDialog *d) {
    gchar *t = NULL, *t2 = NULL;

    g_object_get(d, "text", &t, "secondary-text", &t2, NULL);
    g_string_append_printf(avisos, "%s | %s\n", t ? t : "", t2 ? t2 : "");
    g_free(t);
    g_free(t2);
}

/* Dos carpetas son la misma si lo son en el disco, se escriban como se
 * escriban.                                                            */
static gboolean same_dir(const char *a, const char *b) {
    GFile *fa = g_file_new_for_path(a), *fb = g_file_new_for_path(b);
    gboolean r = g_file_equal(fa, fb);

    g_object_unref(fa);
    g_object_unref(fb);
    return r;
}

static gboolean contesta(gpointer unused) {
    GList *l, *top = gtk_window_list_toplevels();

    (void) unused;
    for (l = top; l; l = l->next) {
        GtkWidget *w = l->data;

        if (!gtk_widget_get_visible(w)) continue;
        if (GTK_IS_MESSAGE_DIALOG(w)) {
            texto_de_aviso(GTK_MESSAGE_DIALOG(w));
            gtk_dialog_response(GTK_DIALOG(w), GTK_RESPONSE_CLOSE);
            break;
        }
        if (GTK_IS_FILE_CHOOSER_DIALOG(w)) {
            GtkFileChooser *fc = GTK_FILE_CHOOSER(w);

            /* YA CONTESTADO: al guardar, el selector se queda la respuesta,
             * mira en el disco si el fichero existe --sin bloquear-- y
             * entonces se la da a si mismo. Mientras, sigue abierto; si se
             * le volviera a contestar, seria con un Cancelar.           */
            if (w == contestado) continue;
            if (!elegir) {
                gtk_dialog_response(GTK_DIALOG(w), GTK_RESPONSE_CANCEL);
                break;
            }
            /* El selector carga la carpeta DESPUES, asi que se le pone el
             * fichero y se espera a que lo tenga de verdad -- comparando
             * el nombre y la carpeta, que en Windows se escriben con la
             * otra barra. Si no llega, se acepta igual y la prueba dira
             * lo que paso.                                              */
            {
            gchar *f = gtk_file_chooser_get_filename(fc);
            gchar *quiero = g_path_get_basename(elegir), *dquiero = g_path_get_dirname(elegir);
            gchar *b1 = f ? g_path_get_basename(f) : NULL, *d1 = f ? g_path_get_dirname(f) : NULL;
            gboolean ya = b1 && strcmp(b1, quiero) == 0 && d1 && same_dir(d1, dquiero);

            if (!ya && elegir_espera++ % 10 == 0) {
                if (gtk_file_chooser_get_action(fc) == GTK_FILE_CHOOSER_ACTION_SAVE) {
                    gtk_file_chooser_set_current_folder(fc, dquiero);
                    gtk_file_chooser_set_current_name(fc, quiero);
                } else
                    gtk_file_chooser_set_filename(fc, elegir);
            }
            g_free(f); g_free(b1); g_free(d1); g_free(quiero); g_free(dquiero);
            if (ya || elegir_espera > 150) {
                elegir_espera = 0;
                g_clear_pointer(&elegir, g_free);
                contestado = w;
                g_object_add_weak_pointer(G_OBJECT(w), (gpointer *) &contestado);
                gtk_dialog_response(GTK_DIALOG(w), GTK_RESPONSE_ACCEPT);
            }
            }
            break;
        }
    }
    g_list_free(top);
    return G_SOURCE_CONTINUE;
}

/* Lo que dijeron los avisos, y se vacia. g_free. */
static gchar *avisos_vistos(void) {
    gchar *s = g_strdup(avisos->str);

    g_string_truncate(avisos, 0);
    return s;
}

/* ------------------------------------------------------------------------ */
/* Buscar lo que el analista ve                                              */
/* ------------------------------------------------------------------------ */

typedef gboolean (*Pred)(GtkWidget *w, gpointer d);

typedef struct { Pred p; gpointer d; GtkWidget *hallado; } Busqueda;

static void busca_en(GtkWidget *w, gpointer data) {
    Busqueda *b = data;

    if (b->hallado) return;
    if (b->p(w, b->d)) { b->hallado = w; return; }
    if (GTK_IS_CONTAINER(w))
        gtk_container_forall(GTK_CONTAINER(w), busca_en, b);
}

static GtkWidget *busca(GtkWidget *raiz, Pred p, gpointer d) {
    Busqueda b = { p, d, NULL };

    busca_en(raiz, &b);
    return b.hallado;
}

/* Un boton de una pestaña: su rotulo y el tipo de operador que lleva
 * colgado (0 = ninguno, que es como van los de los deterministas).      */
typedef struct { const char *rotulo; int op; } QueBoton;

static gboolean es_boton(GtkWidget *w, gpointer d) {
    QueBoton *q = d;
    const char *l;

    if (!GTK_IS_BUTTON(w) || GTK_IS_TOOL_BUTTON(w)) return FALSE;
    l = gtk_button_get_label(GTK_BUTTON(w));
    return l && strcmp(l, q->rotulo) == 0
        && GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w), "op_type")) == q->op;
}

static void pulsa(FueContext *ctx, const char *rotulo, int op) {
    QueBoton q = { rotulo, op };
    GtkWidget *b = busca(ctx->main_window, es_boton, &q);
    gchar *s = g_strdup_printf("el boton «%s» (operador %d) existe", rotulo, op);

    check(b != NULL, s, NULL);
    g_free(s);
    if (b) gtk_button_clicked(GTK_BUTTON(b));
    pump(30);
}

static gboolean es_de_barra(GtkWidget *w, gpointer d) {
    const char *l;

    if (!GTK_IS_TOOL_BUTTON(w)) return FALSE;
    l = gtk_tool_button_get_label(GTK_TOOL_BUTTON(w));
    return l && strcmp(l, (const char *) d) == 0;
}

/* Los de la barra de arriba: New, Open, Save, Run, Forecast...           */
static void barra(FueContext *ctx, const char *rotulo) {
    GtkWidget *b = busca(ctx->main_window, es_de_barra, (gpointer) rotulo);
    gchar *s = g_strdup_printf("el boton «%s» de la barra existe", rotulo);

    check(b != NULL, s, NULL);
    g_free(s);
    if (b) g_signal_emit_by_name(b, "clicked");
    pump(30);
}

static gboolean es_area(GtkWidget *w, gpointer d) {
    (void) d;
    return GTK_IS_DRAWING_AREA(w);
}

static gboolean es_arbol(GtkWidget *w, gpointer d) {
    (void) d;
    return GTK_IS_TREE_VIEW(w);
}

static const char *estado(FueContext *ctx) {
    return gtk_label_get_text(GTK_LABEL(ctx->status_label));
}

static const char *estado_prev(FueContext *ctx) {
    return gtk_label_get_text(GTK_LABEL(ctx->forecast_status_label));
}

static int filas(GtkWidget *tv) {
    return gtk_tree_model_iter_n_children(gtk_tree_view_get_model(GTK_TREE_VIEW(tv)), NULL);
}

static void selecciona(GtkWidget *tv, int fila) {
    GtkTreePath *p = gtk_tree_path_new_from_indices(fila, -1);

    gtk_tree_selection_select_path(gtk_tree_view_get_selection(GTK_TREE_VIEW(tv)), p);
    gtk_tree_path_free(p);
}

/* La columna de texto c de la fila i de un arbol. g_free. */
static gchar *celda(GtkWidget *tv, int i, int c) {
    GtkTreeModel *m = gtk_tree_view_get_model(GTK_TREE_VIEW(tv));
    GtkTreeIter it;
    gchar *s = NULL;

    if (gtk_tree_model_iter_nth_child(m, &it, NULL, i))
        gtk_tree_model_get(m, &it, c, &s, -1);
    return s;
}

static guint entero(GtkWidget *tv, int i, int c) {
    GtkTreeModel *m = gtk_tree_view_get_model(GTK_TREE_VIEW(tv));
    GtkTreeIter it;
    guint v = 0;

    if (gtk_tree_model_iter_nth_child(m, &it, NULL, i))
        gtk_tree_model_get(m, &it, c, &v, -1);
    return v;
}

/* Todo el texto de una vista. g_free. */
static gchar *texto(GtkWidget *tv) {
    GtkTextBuffer *b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));
    GtkTextIter a, z;

    gtk_text_buffer_get_bounds(b, &a, &z);
    return gtk_text_buffer_get_text(b, &a, &z, FALSE);
}

static gchar *lee(const char *path) {
    gchar *s = NULL;

    g_file_get_contents(path, &s, NULL, NULL);
    return s;
}

/* Lo que ensena una vista es el fichero entero. Se compara con el fichero
 * hecho UTF-8 valido, que es lo que hace la vista: los motores escriben
 * algun byte latin-1 (el 0xBD de fuf) y GTK no los admite tal cual.    */
static gboolean vista_es_fichero(GtkWidget *vista, const char *path) {
    gchar *f = lee(path), *v = texto(vista), *fv = f ? g_utf8_make_valid(f, -1) : NULL;
    gboolean r;

    /* El contenido, no el fin de linea: en Windows lo que guarda el GUI
     * lleva \r\n y la vista no.                                       */
    for (gchar **q = (gchar *[]){ fv, v, NULL }; *q; q++) {
        gchar *c = *q, *w = *q;
        for (; *c; c++) if (*c != '\r') *w++ = *c;
        *w = '\0';
    }
    r = fv && v && *v && strcmp(fv, v) == 0;

    g_free(f); g_free(v); g_free(fv);
    return r;
}

static void copia(const char *de, const char *a) {
    gchar *s = NULL;
    gsize n = 0;

    if (g_file_get_contents(de, &s, &n, NULL))
        g_file_set_contents(a, s, n, NULL);
    else
        printf("FAIL: no se pudo leer el dato %s\n", de), fails++;
    g_free(s);
}

static gboolean contiene(const char *s, const char *t) {
    return s && t && strstr(s, t) != NULL;
}

/* El nombre de lo que tiene un selector, sin carpeta: las carpetas se
 * escriben distinto en cada sistema y lo que importa es el fichero.   */
static gchar *nombre_en(GtkWidget *chooser) {
    gchar *f = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
    gchar *b = f ? g_path_get_basename(f) : NULL;

    g_free(f);
    return b;
}

/* ------------------------------------------------------------------------ */
/* ¿Se ha pintado algo?                                                      */
/* ------------------------------------------------------------------------ */

/* Los pixeles oscuros de una superficie: el papel es blanco y el fondo de
 * la ventana es gris claro, asi que lo que baja de 100 es tinta.        */
static int tinta_de(cairo_surface_t *s) {
    int w = cairo_image_surface_get_width(s), h = cairo_image_surface_get_height(s);
    int st = cairo_image_surface_get_stride(s), x, y, n = 0;
    unsigned char *p;

    cairo_surface_flush(s);
    p = cairo_image_surface_get_data(s);
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            guint32 px = *(guint32 *) (p + y * st + 4 * x);
            int r = (px >> 16) & 0xff, g = (px >> 8) & 0xff, b = px & 0xff;

            if ((r + g + b) / 3 < 100) n++;
        }
    return n;
}

static int tinta_widget(GtkWidget *w) {
    int ancho = gtk_widget_get_allocated_width(w), alto = gtk_widget_get_allocated_height(w);
    cairo_surface_t *s;
    cairo_t *cr;
    int n;

    if (ancho < 2 || alto < 2) return -1;
    s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, ancho, alto);
    cr = cairo_create(s);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    gtk_widget_draw(w, cr);
    cairo_destroy(cr);
    n = tinta_de(s);
    cairo_surface_destroy(s);
    return n;
}

/* La ventana de graficos de un fichero: lib/preview la marca con el papel
 * "atsw-graph" y le pone de titulo el nombre del fichero.              */
static GtkWidget *ventana_de(const char *titulo_con) {
    GList *l, *top = gtk_window_list_toplevels();
    GtkWidget *r = NULL;

    for (l = top; l && !r; l = l->next) {
        const char *t = gtk_window_get_title(GTK_WINDOW(l->data));

        if (gtk_widget_get_visible(l->data) && t && strstr(t, titulo_con))
            r = l->data;
    }
    g_list_free(top);
    return r;
}

/* Que la ventana del grafico este abierta y que su dibujo tenga tinta; y
 * se cierra, como haria el analista.                                   */
static void grafico_pintado(const char *titulo, const char *que) {
    GtkWidget *v, *area;
    gchar *s;
    int n = -1;

    pump(400);
    v = ventana_de(titulo);
    s = g_strdup_printf("%s: se abre la ventana del grafico «%s»", que, titulo);
    check(v != NULL, s, NULL);
    g_free(s);
    if (!v) return;
    check(g_strcmp0(gtk_window_get_role(GTK_WINDOW(v)), "atsw-graph") == 0,
          "y es la ventana de graficos de lib/preview", gtk_window_get_role(GTK_WINDOW(v)));
    area = busca(v, es_area, NULL);
    if (area) n = tinta_widget(area);
    {
    gchar *vio = g_strdup_printf("%d pixeles con tinta", n);

    s = g_strdup_printf("%s: el grafico tiene algo dibujado", que);
    check(n > 200, s, vio);
    g_free(s); g_free(vio);
    }
    gtk_widget_destroy(v);
    pump(30);
}

/* ------------------------------------------------------------------------ */
/* Las pruebas, en el orden en que las haria alguien                         */
/* ------------------------------------------------------------------------ */

static const char *DATOS, *TRABAJO;
static GtkApplication *app;

static gchar *prepara(const char *sub, const char *const *ficheros) {
    gchar *dir = g_build_filename(TRABAJO, sub, NULL);

    g_mkdir_with_parents(dir, 0755);
    for (; *ficheros; ficheros++) {
        gchar *de = g_build_filename(DATOS, *ficheros, NULL);
        gchar *a  = g_build_filename(dir, *ficheros, NULL);

        copia(de, a);
        g_free(de); g_free(a);
    }
    return dir;
}

/* Correr fue con Run y esperar a que acabe, como espera el analista.   */
static void corre_fue(FueContext *ctx) {
    gint64 hasta;

    barra(ctx, "Run");
    hasta = g_get_monotonic_time() + 30 * G_USEC_PER_SEC;
    while (ctx->running && g_get_monotonic_time() < hasta) pump(20);
    pump(50);
    check(!ctx->running, "fue tiene que acabar (30 s)", estado(ctx));
}

/* 1. EL ARRANQUE: lo que se ve con la ventana recien abierta, y abrir el
 *    fichero que manda la madre.                                       */
static void prueba_arranque(FueContext *ctx) {
    static const char *f[] = { "D1.inp", NULL };
    gchar *dir = prepara("arranque", f), *inp = g_build_filename(dir, "D1.inp", NULL);
    gchar *s;

    check(g_strcmp0(gtk_label_get_text(GTK_LABEL(ctx->model_label)), "(none)") == 0,
          "al abrir, «Model:» dice (none)", gtk_label_get_text(GTK_LABEL(ctx->model_label)));
    s = texto(ctx->console_text_view);
    check(g_str_has_prefix(s, "No output yet"), "la consola empieza diciendo que no hay nada", s);
    g_free(s);
    check(g_strcmp0(estado_prev(ctx), "Ready") == 0, "la pestaña de prevision empieza en Ready",
          estado_prev(ctx));
    /* NOTA: recien creada, la ventana tiene Diagnosis/Anomalos/Ganancia
       ENCENDIDOS y sin globo -- fue_analisis_refresca() solo se llama al
       cambiar el nombre de entrada, no al crearla (main_window.c). No se
       comprueba aqui: se comprueba en cuanto hay un fichero, abajo.
       Pulsarlo sin proyecto lo dice en la barra, y no se cae.          */
    fue_on_diagnosis(NULL, ctx);
    check(contiene(estado(ctx), "proyecto"), "Diagnosis sin proyecto lo dice en la barra", estado(ctx));

    /* Lo que deja dicho la linea de ordenes. */
    check(fue_abrir() == NULL, "sin fichero en la linea de ordenes, no hay que abrir nada", fue_abrir());
    fue_pon_abrir(inp);
    check(g_strcmp0(fue_abrir(), inp) == 0, "el fichero mandado se recuerda", fue_abrir());
    fue_abre_al_arrancar(ctx, fue_abrir());
    pump(50);
    fue_pon_abrir(NULL);

    check(g_strcmp0(estado(ctx), "Model loaded from file.") == 0, "abrir D1.inp lo dice", estado(ctx));
    check(g_strcmp0(gtk_entry_get_text(GTK_ENTRY(ctx->series_name_entry)), "D") == 0,
          "el nombre de la serie sale del fichero", gtk_entry_get_text(GTK_ENTRY(ctx->series_name_entry)));
    check(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ctx->n_obs_spin)) == 216,
          "216 observaciones", NULL);
    check(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ctx->start_year_spin)) == 2002,
          "empieza en 2002", NULL);
    check(gtk_combo_box_get_active(GTK_COMBO_BOX(ctx->freq_combo)) == 2, "y es mensual", NULL);
    check(g_strcmp0(gtk_entry_get_text(GTK_ENTRY(ctx->input_name_entry)), "D1") == 0,
          "el nombre de entrada es el del fichero", gtk_entry_get_text(GTK_ENTRY(ctx->input_name_entry)));
    check(g_strcmp0(gtk_label_get_text(GTK_LABEL(ctx->model_label)), "D1.inp") == 0,
          "«Model:» dice D1.inp", gtk_label_get_text(GTK_LABEL(ctx->model_label)));
    s = nombre_en(ctx->workspace_file_chooser);
    check(g_strcmp0(s, "arranque") == 0, "el espacio de trabajo es la carpeta del fichero", s);
    g_free(s);
    /* Sin proyecto, la diagnosis no tiene de que modelo hablar: apagada, y
       el globo dice por que.                                          */
    check(!gtk_widget_get_sensitive(ctx->btn_diagnosis) && !gtk_widget_get_sensitive(ctx->btn_anomalos)
          && !gtk_widget_get_sensitive(ctx->btn_ganancia),
          "sin proyecto, Diagnosis/Anomalos/Ganancia apagados", NULL);
    s = gtk_widget_get_tooltip_text(ctx->btn_diagnosis);
    check(contiene(s, "proyecto"), "y el globo dice que no es un modelo del proyecto", s);
    g_free(s);
    /* Lo que el fichero trae, en sus pestañas: once deterministas (cinco
       pares de armonicos y el alterno) y un AR(3) regular.            */
    check(filas(ctx->int_treeview) == 11, "los 11 deterministas en su arbol", NULL);
    s = celda(ctx->int_treeview, 10, COL_NAME);
    check(g_strcmp0(s, "Alter") == 0, "el ultimo es el alterno", s);
    g_free(s);
    check(filas(ctx->arr_treeview) == 1 && entero(ctx->arr_treeview, 0, 2) == 3,
          "un AR regular de orden 3", NULL);
    check(filas(ctx->mar_treeview) == 0 && filas(ctx->maa_treeview) == 0
          && filas(ctx->ara_treeview) == 0, "y nada mas en la parte estocastica", NULL);

    /* --proyecto: la ventana nace con el espacio de trabajo puesto. Se
       levanta una segunda ventana solo para mirarlo, y se tira.       */
    fue_pon_raiz_proyecto(dir);
    check(g_strcmp0(fue_raiz_proyecto(), dir) == 0, "la raiz del proyecto se recuerda", fue_raiz_proyecto());
    {
    FueContext *c2 = g_new0(FueContext, 1);
    GtkWidget *w2 = create_main_window(app, c2);

    s = nombre_en(c2->workspace_file_chooser);
    check(g_strcmp0(s, "arranque") == 0, "con --proyecto, el espacio de trabajo es su raiz", s);
    g_free(s);
    gtk_widget_destroy(w2);
    g_free(c2);
    }
    fue_pon_raiz_proyecto(NULL);
    check(fue_raiz_proyecto() == NULL, "y sin el, no hay raiz", fue_raiz_proyecto());
    g_free(inp); g_free(dir);
}

/* 2. FICHEROS MALOS: se dice por que, y lo que habia se queda.          */
static void prueba_ficheros_malos(FueContext *ctx) {
    static const char *f[] = { NULL };
    gchar *dir = prepara("malos", f);
    gchar *inp = g_build_filename(dir, "roto.inp", NULL);
    gchar *txt = g_build_filename(dir, "roto.txt", NULL);
    gchar *a;

    g_file_set_contents(inp, "esto no es un modelo\n12\n", -1, NULL);
    g_file_set_contents(txt, "uno\ndos\ntres\n", -1, NULL);

    g_free(avisos_vistos());
    fue_abre_al_arrancar(ctx, inp);
    pump(50);
    a = avisos_vistos();
    check(contiene(a, "roto.inp was not loaded"), "un .inp roto: el aviso dice que no se cargo", a);
    check(strlen(a) > strlen("roto.inp was not loaded | \n"), "y por que", a);
    g_free(a);
    check(g_strcmp0(estado(ctx), "File not loaded.") == 0, "y la barra lo repite", estado(ctx));
    check(filas(ctx->int_treeview) == 11 && NdetVar == 11,
          "y el modelo que habia sigue ahi", NULL);

    fue_abre_al_arrancar(ctx, txt);
    pump(50);
    check(g_str_has_prefix(estado(ctx), "Data not loaded"), "unos datos que no son numeros: no se cargan",
          estado(ctx));
    check(strlen(estado(ctx)) > strlen("Data not loaded: "), "y se dice el motivo", estado(ctx));
    g_free(inp); g_free(txt); g_free(dir);
}

/* 3. UN MODELO DESDE LOS DATOS, construido con los dialogos, guardado y
 *    estimado.                                                          */
static void op_dialogo(FueContext *ctx, double v1, int orden, gboolean fijo, gint resp) {
    GObject *d = G_OBJECT(ctx->esp_op_dialog);
    GtkWidget *orden_spin, *s1, *c1;

    check(d && gtk_widget_get_visible(ctx->esp_op_dialog), "el dialogo del operador se abre", NULL);
    if (!d) return;
    orden_spin = g_object_get_data(d, "order_spin");
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(orden_spin), orden);
    s1 = g_object_get_data(d, "ar_spinbutton_1");
    c1 = g_object_get_data(d, "ar_checkbutton_1");
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(s1), v1);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(c1), fijo);
    if (orden > 1) {
        GtkWidget *s2 = g_object_get_data(d, "ar_spinbutton_2");

        check(s2 != NULL, "subir el orden pone una fila mas en el dialogo", NULL);
        if (s2) gtk_spin_button_set_value(GTK_SPIN_BUTTON(s2), 0.1);
    }
    gtk_dialog_response(GTK_DIALOG(ctx->esp_op_dialog), resp);
    pump(30);
    check(!gtk_widget_get_visible(ctx->esp_op_dialog), "y se cierra al contestar", NULL);
}

static void det_dialogo(FueContext *ctx, int tipo, int periodo, int anno, gint resp) {
    GObject *d = G_OBJECT(ctx->esp_int_dialog);

    check(d && gtk_widget_get_visible(ctx->esp_int_dialog), "el dialogo del determinista se abre", NULL);
    if (!d) return;
    gtk_combo_box_set_active(GTK_COMBO_BOX(g_object_get_data(d, "type_combo")), tipo);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(d, "period_spin")), periodo);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(d, "year_spin")), anno);
    gtk_dialog_response(GTK_DIALOG(ctx->esp_int_dialog), resp);
    pump(30);
    check(!gtk_widget_get_visible(ctx->esp_int_dialog), "y se cierra al contestar", NULL);
}

static void prueba_construir(FueContext *ctx) {
    static const char *f[] = { "D.txt", NULL };
    gchar *dir = prepara("datos", f);
    gchar *txt = g_build_filename(dir, "D.txt", NULL);
    gchar *inp = g_build_filename(dir, "D.inp", NULL);
    gchar *out = g_build_filename(dir, "D.out", NULL);
    gchar *s;

    /* Los datos no traen fecha: la pone el analista ANTES de cargarlos. */
    gtk_combo_box_set_active(GTK_COMBO_BOX(ctx->freq_combo), 2);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->start_year_spin), 2002);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->start_period_spin), 1);

    /* «Open» de la barra, con el selector de ficheros de verdad. */
    elegir = g_strdup(txt);
    barra(ctx, "Open");
    pump(50);
    check(g_strcmp0(estado(ctx), "Data loaded successfully.") == 0, "Open con D.txt carga los datos",
          estado(ctx));
    check(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ctx->n_obs_spin)) == 216,
          "las 216 observaciones del fichero", NULL);
    check(g_strcmp0(gtk_entry_get_text(GTK_ENTRY(ctx->input_name_entry)), "D") == 0,
          "el nombre de entrada sale del fichero", gtk_entry_get_text(GTK_ENTRY(ctx->input_name_entry)));
    check(filas(ctx->int_treeview) == 0 && filas(ctx->arr_treeview) == 0,
          "datos nuevos, modelo vacio: los arboles se vacian", NULL);

    /* La frecuencia manda en los factores de la diferencia anual. */
    gtk_combo_box_set_active(GTK_COMBO_BOX(ctx->freq_combo), 0);
    check(!gtk_widget_get_sensitive(ctx->f0_check), "anual: sin factores de la diferencia anual", NULL);
    gtk_combo_box_set_active(GTK_COMBO_BOX(ctx->freq_combo), 1);
    check(gtk_widget_get_sensitive(ctx->f2_check) && !gtk_widget_get_sensitive(ctx->f4_check),
          "trimestral: f=0..2 si, f=4 no", NULL);
    gtk_combo_box_set_active(GTK_COMBO_BOX(ctx->freq_combo), 2);
    check(gtk_widget_get_sensitive(ctx->f6_check) && Ts.freq == 12, "mensual: todos", NULL);

    /* Box-Cox y diferencias: log, (1-B)(1-B^12). */
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->boxcox_lambda_spin), 0.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->nrdiff_spin), 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->nadiff_spin), 1);

    /* Un MA regular y uno anual, por sus dialogos. */
    pulsa(ctx, "Add", 3);
    op_dialogo(ctx, -0.4, 1, FALSE, GTK_RESPONSE_OK);
    check(filas(ctx->mar_treeview) == 1 && NopMar == 1, "OK añade el MA regular", NULL);
    pulsa(ctx, "Add", 4);
    op_dialogo(ctx, -0.5, 1, FALSE, GTK_RESPONSE_OK);
    check(filas(ctx->maa_treeview) == 1 && NopMaa == 1, "y el MA anual", NULL);

    /* Cancelar no añade nada, aunque se haya tocado el orden. */
    pulsa(ctx, "Add", 1);
    op_dialogo(ctx, 0.3, 2, FALSE, GTK_RESPONSE_CANCEL);
    check(filas(ctx->arr_treeview) == 0 && NopArr == 0, "Cancel no añade el AR", NULL);

    /* Añadir un AR(2) con un parametro fijo y quitarlo. */
    pulsa(ctx, "Add", 1);
    op_dialogo(ctx, 0.3, 2, TRUE, GTK_RESPONSE_OK);
    check(filas(ctx->arr_treeview) == 1 && entero(ctx->arr_treeview, 0, 2) == 2,
          "un AR regular de orden 2", NULL);
    s = celda(ctx->arr_treeview, 0, 1);
    check(g_strcmp0(s, "1") == 0, "con una restriccion (el fijo)", s);
    g_free(s);
    selecciona(ctx->arr_treeview, 0);
    pulsa(ctx, "Remove", 1);
    check(filas(ctx->arr_treeview) == 0 && NopArr == 0, "Remove lo quita", NULL);

    /* Editar el MA regular: el dialogo vuelve con lo que tenia. */
    selecciona(ctx->mar_treeview, 0);
    pulsa(ctx, "Edit", 3);
    {
    GtkWidget *s1 = g_object_get_data(G_OBJECT(ctx->esp_op_dialog), "ar_spinbutton_1");
    double v = gtk_spin_button_get_value(GTK_SPIN_BUTTON(s1));
    gchar *vio = g_strdup_printf("%g", v);

    check(v > -0.41 && v < -0.39, "Edit recarga el valor que tenia el operador", vio);
    g_free(vio);
    }
    op_dialogo(ctx, -0.3, 1, FALSE, GTK_RESPONSE_OK);
    check(filas(ctx->mar_treeview) == 1 && Mar[0].op_parameter[1] > -0.31 && Mar[0].op_parameter[1] < -0.29,
          "y OK lo cambia, sin añadir otro", NULL);

    /* Insertar delante del MA regular, y quitarlo: el que habia vuelve a
       ser el primero, con su valor.                                    */
    selecciona(ctx->mar_treeview, 0);
    pulsa(ctx, "Insert", 3);
    op_dialogo(ctx, 0.2, 1, FALSE, GTK_RESPONSE_OK);
    check(filas(ctx->mar_treeview) == 2 && NopMar == 2, "Insert pone un operador mas", NULL);
    check(entero(ctx->mar_treeview, 0, 0) == 1 && entero(ctx->mar_treeview, 1, 0) == 2,
          "y los arboles se renumeran", NULL);
    check(Mar[0].op_parameter[1] > 0.19 && Mar[1].op_parameter[1] < -0.29,
          "el insertado va DELANTE del elegido", NULL);
    selecciona(ctx->mar_treeview, 0);
    pulsa(ctx, "Remove", 3);
    check(filas(ctx->mar_treeview) == 1 && NopMar == 1 && Mar[0].op_parameter[1] < -0.29,
          "quitarlo deja el que habia", NULL);

    /* Los de frecuencia fija: añadir, editar y quitar. */
    pulsa(ctx, "Add", 5);
    {
    GObject *d = G_OBJECT(ctx->esp_fix_dialog);

    check(d && gtk_widget_get_visible(ctx->esp_fix_dialog), "el dialogo de frecuencia fija se abre", NULL);
    if (d) {
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(d, "freq_spin")), 3);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(d, "param_spin")), -0.5);
        gtk_dialog_response(GTK_DIALOG(ctx->esp_fix_dialog), GTK_RESPONSE_OK);
        pump(30);
    }
    }
    check(filas(ctx->ar_fix_treeview) == 1 && NumAr2f == 1 && entero(ctx->ar_fix_treeview, 0, 2) == 3,
          "un AR(2) de frecuencia 3", NULL);
    selecciona(ctx->ar_fix_treeview, 0);
    pulsa(ctx, "Edit", 5);
    {
    GObject *d = G_OBJECT(ctx->esp_fix_dialog);
    double v = gtk_spin_button_get_value(GTK_SPIN_BUTTON(g_object_get_data(d, "param_spin")));

    check(v > -0.51 && v < -0.49, "Edit recarga el parametro de frecuencia fija", NULL);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(d, "freq_spin")), 4);
    gtk_dialog_response(GTK_DIALOG(ctx->esp_fix_dialog), GTK_RESPONSE_OK);
    pump(30);
    }
    check(entero(ctx->ar_fix_treeview, 0, 2) == 4 && Ar2f[0].freq == 4.0, "y OK lo cambia", NULL);
    selecciona(ctx->ar_fix_treeview, 0);
    pulsa(ctx, "Remove", 5);
    check(filas(ctx->ar_fix_treeview) == 0 && NumAr2f == 0, "Remove lo quita", NULL);

    /* Deterministas: un escalon y un impulso, editar, insertar, quitar. */
    pulsa(ctx, "Add", 0);
    det_dialogo(ctx, 2, 1, 2010, GTK_RESPONSE_OK);           /* Step 1/2010   */
    pulsa(ctx, "Add", 0);
    det_dialogo(ctx, 0, 6, 2008, GTK_RESPONSE_OK);           /* Impulse 6/2008 */
    check(filas(ctx->int_treeview) == 2 && NdetVar == 2, "dos deterministas", NULL);
    s = celda(ctx->int_treeview, 1, COL_NAME);
    check(g_strcmp0(s, "Impulse") == 0 && entero(ctx->int_treeview, 1, COL_SEASON) == 6
          && entero(ctx->int_treeview, 1, COL_YEAR) == 2008, "el segundo es el impulso de 6/2008", s);
    g_free(s);
    pulsa(ctx, "Add", 0);
    det_dialogo(ctx, 0, 1, 2005, GTK_RESPONSE_CANCEL);
    check(filas(ctx->int_treeview) == 2 && NdetVar == 2, "Cancel no añade el determinista", NULL);

    selecciona(ctx->int_treeview, 1);
    pulsa(ctx, "Edit", 0);
    {
    GObject *d = G_OBJECT(ctx->esp_int_dialog);
    int p = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(g_object_get_data(d, "period_spin")));

    check(p == 6, "Edit recarga la fecha del determinista", NULL);
    }
    det_dialogo(ctx, 0, 7, 2008, GTK_RESPONSE_OK);
    check(filas(ctx->int_treeview) == 2 && It[1].period == 7 && entero(ctx->int_treeview, 1, COL_SEASON) == 7,
          "y OK lo cambia, en el arbol y en el modelo", NULL);

    /* Insertar: lo que dice el arbol y lo que se guarda tiene que ser lo
       mismo, en el mismo orden.                                       */
    selecciona(ctx->int_treeview, 0);
    pulsa(ctx, "Insert", 0);
    det_dialogo(ctx, 3, 1, 2015, GTK_RESPONSE_OK);           /* Ramp 1/2015 */
    check(filas(ctx->int_treeview) == 3 && NdetVar == 3, "Insert pone un determinista mas", NULL);
    {
    int i, iguales = 1;
    GString *vio = g_string_new(NULL);

    for (i = 0; i < 3; i++) {
        guint per = entero(ctx->int_treeview, i, COL_SEASON), an = entero(ctx->int_treeview, i, COL_YEAR);

        g_string_append_printf(vio, "[arbol %u/%u, modelo %d/%d] ", per, an, It[i].period, It[i].year);
        if ((int) per != It[i].period || (int) an != It[i].year) iguales = 0;
    }
    check(iguales, "el arbol y el modelo tienen los deterministas en el mismo orden", vio->str);
    check(It[0].type == 3 && It[1].type == 2 && It[2].type == 0,
          "el insertado va DELANTE del elegido, como los operadores", vio->str);
    g_string_free(vio, TRUE);
    }
    selecciona(ctx->int_treeview, 0);
    pulsa(ctx, "Remove", 0);
    check(filas(ctx->int_treeview) == 2 && NdetVar == 2 && It[0].type == 2 && It[1].type == 0,
          "Remove quita el insertado y deja los otros dos", NULL);

    /* Guardar: el .inp sale al lado de los datos, lo acepta el comprobador
       del motor y se vuelve a leer igual.                              */
    barra(ctx, "Save");
    check(g_strcmp0(estado(ctx), "INP file saved.") == 0, "Save lo dice", estado(ctx));
    check(g_file_test(inp, G_FILE_TEST_EXISTS), "y D.inp esta al lado de los datos", inp);
    {
    char why[512] = "";

    check(inp_check_fue(inp, why, sizeof why) == 0, "el .inp guardado lo acepta el comprobador de fue", why);
    }
    load_input_fue(inp);
    check(NopMar == 1 && NopMaa == 1 && NdetVar == 2 && Tm.nrdiff == 1 && Tm.nadiff == 1
          && Tm.boxlam == 0.0 && Ts.nobs == 216,
          "releido, es el mismo modelo", NULL);
    update_ui_from_model(ctx);

    /* Y se estima. */
    corre_fue(ctx);
    check(contiene(estado(ctx), "fue finished"), "Run: fue acaba con el modelo construido", estado(ctx));
    check(g_file_test(out, G_FILE_TEST_EXISTS), "y deja D.out", out);
    check(vista_es_fichero(ctx->console_text_view, out), "la consola ensena el .out entero", NULL);
    check(gtk_notebook_get_current_page(GTK_NOTEBOOK(ctx->notebook)) == ctx->console_page,
          "y se salta a la consola", NULL);

    /* Ver la salida: la ventana de graficos con el PDF que hizo fue. */
    barra(ctx, "View Output");
    check(g_strcmp0(estado(ctx), "Graph window.") == 0, "View Output abre la ventana de graficos", estado(ctx));
    grafico_pintado("D.pdf", "View Output");

    /* Guardar el grafico como imagen, como el boton Save As de la ventana. */
    {
    gchar *pdf = g_build_filename(dir, "D.pdf", NULL), *png = g_build_filename(dir, "D_grafico.png", NULL);
    GError *e = NULL;
    GdkPixbuf *pb;

    preview_show(ctx, pdf);
    check(preview_save_as(pdf, png, &e), "el grafico se guarda como PNG", e ? e->message : NULL);
    g_clear_error(&e);
    pb = gdk_pixbuf_new_from_file(png, NULL);
    check(pb != NULL && gdk_pixbuf_get_width(pb) > 100, "y el PNG se puede leer", png);
    if (pb) {
        cairo_surface_t *sf = gdk_cairo_surface_create_from_pixbuf(pb, 1, NULL);

        check(tinta_de(sf) > 200, "y tiene el grafico dibujado", NULL);
        cairo_surface_destroy(sf);
        g_object_unref(pb);
    }
    {
    GtkWidget *v = ventana_de("D.pdf");

    if (v) gtk_widget_destroy(v);
    }
    g_free(pdf); g_free(png);
    }
    g_free(txt); g_free(inp); g_free(out); g_free(dir);
}

/* 4. LO QUE PUEDE SALIR MAL AL ESTIMAR: el motor no esta, o no le gusta
 *    el modelo. Se dice, y la ventana sigue viva.                       */
static void prueba_fallos_motor(FueContext *ctx) {
    gchar *dir = g_build_filename(TRABAJO, "fallos", NULL);
    gchar *vacio = g_build_filename(TRABAJO, "sin_motores", NULL);
    gchar *path = g_strdup(g_getenv("PATH"));

    g_mkdir_with_parents(dir, 0755);
    g_mkdir_with_parents(vacio, 0755);

    /* Sin fue en el PATH. */
    g_setenv("PATH", vacio, TRUE);
    corre_fue(ctx);
    g_setenv("PATH", path, TRUE);
    check(contiene(estado(ctx), "could not be run"), "sin fue en el PATH: lo dice", estado(ctx));
    check(gtk_widget_get_visible(ctx->progress), "y la barra de avance no se queda a medias", NULL);

    /* New: todo vacio. */
    barra(ctx, "New");
    check(g_strcmp0(estado(ctx), "New model created.") == 0, "New lo dice", estado(ctx));
    check(filas(ctx->int_treeview) == 0 && filas(ctx->mar_treeview) == 0 && filas(ctx->maa_treeview) == 0
          && NdetVar == 0 && NopMar == 0, "y vacia el modelo y los arboles", NULL);
    check(gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->boxcox_lambda_spin)) == 1.0
          && gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ctx->nrdiff_spin)) == 0,
          "y Box-Cox y diferencias vuelven a lo de partida", NULL);

    /* Los estacionales: los once de una serie mensual, y ninguno en una
       anual (lo dice).                                                 */
    pulsa(ctx, "Add Seasonals", 0);
    check(filas(ctx->int_treeview) == 11 && NdetVar == 11, "Add Seasonals pone 11 en una mensual", NULL);
    barra(ctx, "New");
    gtk_combo_box_set_active(GTK_COMBO_BOX(ctx->freq_combo), 0);
    pulsa(ctx, "Add Seasonals", 0);
    check(filas(ctx->int_treeview) == 0 && contiene(estado(ctx), "only available"),
          "y ninguno en una anual, diciendolo", estado(ctx));

    /* El nombre de la serie da el de entrada, sin lo que no cabe en un
       nombre de fichero.                                               */
    gtk_entry_set_text(GTK_ENTRY(ctx->series_name_entry), "mi serie; rara");
    check(g_strcmp0(gtk_entry_get_text(GTK_ENTRY(ctx->input_name_entry)), "miserierara") == 0,
          "el nombre de entrada sigue al de la serie, limpio",
          gtk_entry_get_text(GTK_ENTRY(ctx->input_name_entry)));

    /* Guardar sin nombre no escribe nada y lo dice. */
    gtk_entry_set_text(GTK_ENTRY(ctx->input_name_entry), "");
    barra(ctx, "Save");
    check(g_strcmp0(estado(ctx), "Please enter an input name.") == 0, "Save sin nombre lo pide", estado(ctx));

    /* Un modelo sin datos: fue lo rechaza y la barra lo dice, con lo que
       escribio el motor debajo.                                        */
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(ctx->workspace_file_chooser), dir);
    gtk_entry_set_text(GTK_ENTRY(ctx->input_name_entry), "vacio");
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctx->text_view)), "", -1);
    corre_fue(ctx);
    check(!contiene(estado(ctx), "finished") && contiene(estado(ctx), "fue"),
          "un modelo sin datos: fue no acaba bien y la barra lo dice", estado(ctx));
    {
    gchar *t = texto(ctx->text_view);

    check(t && *t, "y lo que escribio el motor queda a la vista", t);
    g_free(t);
    }
    {
    gchar *s = gtk_widget_get_tooltip_text(ctx->status_label);

    check(s && *s, "con el texto entero en el globo", s);
    g_free(s);
    }

    /* Ver la salida sin PDF. */
    barra(ctx, "View Output");
    check(contiene(estado(ctx), "PDF file not found"), "View Output sin PDF lo dice", estado(ctx));

    /* Forecast desde la barra sin modelo estimado: fue -f falla y se dice. */
    barra(ctx, "Forecast");
    check(!contiene(estado(ctx), "successfully"), "Forecast sin modelo valido no dice que acabo bien",
          estado(ctx));
    gtk_entry_set_text(GTK_ENTRY(ctx->input_name_entry), "");
    barra(ctx, "Forecast");
    check(g_strcmp0(estado(ctx), "No model name provided.") == 0, "Forecast sin nombre lo pide", estado(ctx));

    g_free(path); g_free(vacio); g_free(dir);
}

/* 5. LA PREVISION: el boton de la barra (el ciclo entero) y la pestaña
 *    (cargar, guardar como, correr fuf, ver el PDF).                    */
static void prueba_prevision(FueContext *ctx) {
    static const char *f[] = { "D1.inp", NULL };
    gchar *dir = prepara("prevision", f);
    gchar *inp = g_build_filename(dir, "D1.inp", NULL);
    gchar *finp = g_build_filename(dir, "forecast_D1.inp", NULL);
    gchar *fout = g_build_filename(dir, "forecast_D1.out", NULL);
    gchar *mio = g_build_filename(dir, "mio.inp", NULL);
    gchar *mio_out = g_build_filename(dir, "mio.out", NULL);
    gchar *malo = g_build_filename(dir, "malo.inp", NULL);
    gchar *raro = g_build_filename(dir, "raro.txt", NULL);
    gchar *s;

    /* Antes de nada, los botones sin entrada activa: lo dicen. */
    g_clear_pointer(&ctx->forecast_current_inp_path, g_free);
    g_clear_pointer(&ctx->forecast_current_base, g_free);
    gtk_button_clicked(GTK_BUTTON(ctx->forecast_run_button));
    check(contiene(estado_prev(ctx), "No .inp file active"), "Run FUF sin entrada lo dice", estado_prev(ctx));
    gtk_button_clicked(GTK_BUTTON(ctx->forecast_view_pdf_button));
    check(contiene(estado_prev(ctx), "No .inp file active"), "View PDF sin entrada lo dice", estado_prev(ctx));
    gtk_button_clicked(GTK_BUTTON(ctx->forecast_load_button));
    check(g_strcmp0(estado_prev(ctx), "No file selected.") == 0, "Load sin fichero lo dice", estado_prev(ctx));

    /* El ciclo entero, desde la barra: estimar y prever. */
    fue_abre_al_arrancar(ctx, inp);
    corre_fue(ctx);
    check(contiene(estado(ctx), "fue finished"), "D1 se estima", estado(ctx));
    barra(ctx, "Forecast");
    check(g_strcmp0(estado(ctx), "Forecast finished successfully.") == 0, "Forecast hace el ciclo entero",
          estado(ctx));
    check(g_file_test(finp, G_FILE_TEST_EXISTS) && g_file_test(fout, G_FILE_TEST_EXISTS),
          "fue -f y fuf dejan forecast_D1.inp y .out", NULL);
    {
    GtkWidget *hoja = gtk_widget_get_parent(ctx->forecast_notebook);
    int p = gtk_notebook_page_num(GTK_NOTEBOOK(ctx->notebook), hoja);

    check(p >= 0 && gtk_notebook_get_current_page(GTK_NOTEBOOK(ctx->notebook)) == p,
          "y se salta a la pestaña de prevision", NULL);
    }
    check(gtk_notebook_get_current_page(GTK_NOTEBOOK(ctx->forecast_notebook)) == 1,
          "con el informe delante", NULL);
    s = texto(ctx->forecast_editor);
    check(contiene(s, "Forecast horizon"), "la entrada de fuf, en su hoja", s);
    g_free(s);
    check(vista_es_fichero(ctx->forecast_out_view, fout), "y el informe, entero, en la suya", NULL);
    s = nombre_en(ctx->forecast_file_chooser);
    check(g_strcmp0(s, "forecast_D1.inp") == 0, "el selector dice que entrada es", s);
    g_free(s);

    /* La pestaña: cargar el MODELO genera su entrada de prevision. */
    g_remove(finp);
    gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(ctx->forecast_file_chooser), inp);
    gtk_button_clicked(GTK_BUTTON(ctx->forecast_load_button));
    pump(30);
    check(g_file_test(finp, G_FILE_TEST_EXISTS), "Load con un modelo genera su forecast_*.inp", finp);
    s = texto(ctx->forecast_editor);
    check(contiene(s, "Forecast horizon"), "y la carga en el editor", s);
    g_free(s);
    check(g_strcmp0(ctx->forecast_current_base, "forecast_D1") == 0, "y queda activa",
          ctx->forecast_current_base);

    /* Run FUF: rehace el informe. */
    g_remove(fout);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctx->forecast_out_view)), "", -1);
    gtk_button_clicked(GTK_BUTTON(ctx->forecast_run_button));
    pump(30);
    check(g_file_test(fout, G_FILE_TEST_EXISTS), "Run FUF escribe forecast_D1.out", fout);
    check(vista_es_fichero(ctx->forecast_out_view, fout), "y lo ensena entero", NULL);
    s = texto(ctx->forecast_editor);
    check(contiene(s, "Forecast horizon"), "sin tapar la entrada", s);
    g_free(s);

    /* View PDF: el grafico de la prevision. */
    gtk_button_clicked(GTK_BUTTON(ctx->forecast_view_pdf_button));
    check(g_strcmp0(estado_prev(ctx), "Graph window.") == 0, "View PDF abre la ventana de graficos",
          estado_prev(ctx));
    grafico_pintado("forecast_D1.pdf", "View PDF");

    /* Guardar como: lo que hay en el editor, con otro nombre; y se prevé
       con el.                                                          */
    elegir = g_strdup(mio);
    gtk_button_clicked(GTK_BUTTON(ctx->forecast_save_inp_button));
    pump(50);
    check(g_file_test(mio, G_FILE_TEST_EXISTS), "Save as .inp escribe el fichero elegido", mio);
    check(vista_es_fichero(ctx->forecast_editor, mio), "con lo que hay en el editor", NULL);
    check(g_strcmp0(ctx->forecast_current_base, "mio") == 0, "y pasa a ser la entrada activa",
          ctx->forecast_current_base);
    gtk_button_clicked(GTK_BUTTON(ctx->forecast_run_button));
    pump(30);
    check(g_file_test(mio_out, G_FILE_TEST_EXISTS), "y Run FUF prevé con ella", mio_out);

    /* Guardar como, cancelado: no escribe nada. */
    elegir = NULL;
    gtk_button_clicked(GTK_BUTTON(ctx->forecast_save_inp_button));
    pump(30);
    check(g_strcmp0(ctx->forecast_current_base, "mio") == 0, "Cancelar el guardar no cambia nada",
          ctx->forecast_current_base);

    /* Una entrada que fuf no acepta: lo dice, con lo que escribio. */
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctx->forecast_editor)),
                             "*\n*\n*\n*\n\n** basura\n 12\n", -1);
    elegir = g_strdup(malo);
    gtk_button_clicked(GTK_BUTTON(ctx->forecast_save_inp_button));
    pump(50);
    gtk_button_clicked(GTK_BUTTON(ctx->forecast_run_button));
    pump(30);
    check(contiene(estado_prev(ctx), "fuf") && !g_str_has_prefix(estado_prev(ctx), "Loaded"),
          "una entrada rota: fuf falla y la pestaña lo dice", estado_prev(ctx));

    /* Ni modelo ni entrada de prevision: se dice en la barra, sin modal. */
    g_file_set_contents(raro, "hola\n", -1, NULL);
    g_free(avisos_vistos());
    gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(ctx->forecast_file_chooser), raro);
    gtk_button_clicked(GTK_BUTTON(ctx->forecast_load_button));
    pump(30);
    check(g_str_has_prefix(estado_prev(ctx), "Not a model nor a forecast input"),
          "Load con un fichero cualquiera lo dice", estado_prev(ctx));
    s = avisos_vistos();
    check(*s == '\0', "y sin dialogo modal", s);
    g_free(s);

    g_free(dir); g_free(inp); g_free(finp); g_free(fout); g_free(mio); g_free(mio_out);
    g_free(malo); g_free(raro);
}

/* 6. LAS VENTANAS DE ANALISIS, dentro de un proyecto: apagadas hasta que
 *    hay un .out al dia, y despues abren lo suyo.                       */
static void prueba_analisis(FueContext *ctx) {
    static Proyecto p;
    PrError e;
    char id[PR_ID] = "", ruta[PR_RUTA] = "";
    gchar *raiz = g_build_filename(TRABAJO, "proyecto", NULL);
    gchar *yaml = g_build_filename(raiz, "proyecto.yaml", NULL);
    gchar *s;

    g_mkdir_with_parents(raiz, 0755);
    pr_nuevo(&p, "P", "prueba", raiz);
    check(pr_serie_add(&p, "D", &e) == 0, "el proyecto da de alta la serie", NULL);
    check(pr_deriva(&p, "D", "", NULL, id, sizeof id, ruta, sizeof ruta, &e) == 0,
          "y deriva su primer modelo", NULL);
    {
    gchar *d = g_path_get_dirname(ruta), *de = g_build_filename(DATOS, "D1.inp", NULL);

    g_mkdir_with_parents(d, 0755);
    copia(de, ruta);
    g_free(d); g_free(de);
    }
    check(pr_escribir(&p, yaml, &e) == 0, "el manifiesto se escribe", yaml);
    {
    static Proyecto leido;

    check(pr_leer(yaml, &leido, &e) == 0, "y se vuelve a leer", yaml);
    fue_pon_proyecto(&leido);
    }

    fue_abre_al_arrancar(ctx, ruta);
    pump(50);
    check(!gtk_widget_get_sensitive(ctx->btn_diagnosis),
          "un modelo del proyecto sin estimar: Diagnosis apagado", NULL);
    s = gtk_widget_get_tooltip_text(ctx->btn_diagnosis);
    check(s && !contiene(s, "no es un modelo de este proyecto"),
          "y el globo ya no dice que no sea del proyecto", s);
    g_free(s);

    corre_fue(ctx);
    check(contiene(estado(ctx), "fue finished"), "se estima", estado(ctx));
    check(gtk_widget_get_sensitive(ctx->btn_diagnosis) && gtk_widget_get_sensitive(ctx->btn_anomalos)
          && gtk_widget_get_sensitive(ctx->btn_ganancia),
          "con el .out al dia, los tres se encienden", NULL);

    barra(ctx, "Diagnosis");
    pump(200);
    {
    GtkWidget *v = ventana_de("Diagnosis");

    check(v != NULL, "Diagnosis abre su ventana", estado(ctx));
    if (v) {
        GtkWidget *t = busca(v, es_arbol, NULL);

        check(t && filas(t) > 0, "con el dictamen en la lista", NULL);
        gtk_widget_destroy(v);
    }
    }
    barra(ctx, "Ganancia");
    pump(200);
    {
    GtkWidget *v = ventana_de("Ganancia");

    check(v != NULL, "Ganancia abre su ventana", estado(ctx));
    if (v) gtk_widget_destroy(v);
    }
    barra(ctx, "Anomalos");
    grafico_pintado("anomalos", "Anomalos");

    fue_pon_proyecto(NULL);
    update_model_label(ctx);
    check(!gtk_widget_get_sensitive(ctx->btn_diagnosis), "sin proyecto, se vuelven a apagar", NULL);
    g_free(raiz); g_free(yaml);
}

/* 7. CAMBIAR DE MODELO Y SEGUIR TRABAJANDO. Abrir uno con deterministas,
 *    luego otro con menos, y añadir uno: el hueco que se rellena no puede
 *    ser memoria que ya se libero.                                     */
static void prueba_cambiar_de_modelo(FueContext *ctx) {
    static const char *f[] = { "D1.inp", "D.txt", NULL };
    gchar *dir = prepara("cambio", f);
    gchar *d1 = g_build_filename(dir, "D1.inp", NULL), *dat = g_build_filename(dir, "D.txt", NULL);
    gchar *sin = g_build_filename(dir, "sin.inp", NULL), *out = g_build_filename(dir, "sin.out", NULL);

    fue_abre_al_arrancar(ctx, d1);              /* 11 deterministas     */
    fue_abre_al_arrancar(ctx, dat);             /* datos: ninguno       */
    gtk_entry_set_text(GTK_ENTRY(ctx->input_name_entry), "sin");
    barra(ctx, "Save");
    fue_abre_al_arrancar(ctx, sin);             /* un .inp sin ninguno  */
    check(NdetVar == 0 && filas(ctx->int_treeview) == 0, "el segundo modelo no trae deterministas", NULL);
    pulsa(ctx, "Add", 0);
    det_dialogo(ctx, 0, 6, 2008, GTK_RESPONSE_OK);
    pulsa(ctx, "Add", 0);
    det_dialogo(ctx, 2, 1, 2010, GTK_RESPONSE_OK);
    selecciona(ctx->int_treeview, 0);
    pulsa(ctx, "Remove", 0);
    pulsa(ctx, "Add", 0);
    det_dialogo(ctx, 0, 3, 2012, GTK_RESPONSE_OK);
    check(NdetVar == 2 && It[0].type == 2 && It[1].type == 0 && It[1].period == 3,
          "quitar y añadir deja lo que dice el arbol", NULL);
    corre_fue(ctx);
    check(contiene(estado(ctx), "fue finished"), "y el modelo se estima", estado(ctx));
    /* Y volver a abrir otro: es donde se liberaba dos veces. */
    fue_abre_al_arrancar(ctx, d1);
    check(NdetVar == 11, "volver a abrir D1 funciona", NULL);
    g_free(d1); g_free(dat); g_free(sin); g_free(out); g_free(dir);
}

int main(int argc, char **argv) {
    FueContext *ctx;

    if (argc < 3) { fprintf(stderr, "uso: test_operar <datos> <trabajo>\n"); return 2; }
    /* RUTAS ABSOLUTAS: los selectores de GTK no entienden una relativa
       (set_current_folder falla en silencio) y el programa nunca recibe
       otra cosa de ellos.                                              */
    {
    gchar *aqui = g_get_current_dir();

    DATOS   = g_path_is_absolute(argv[1]) ? argv[1] : g_build_filename(aqui, argv[1], NULL);
    TRABAJO = g_path_is_absolute(argv[2]) ? argv[2] : g_build_filename(aqui, argv[2], NULL);
    g_free(aqui);
    }

    /* Que nada de esto toque la casa del que lo corre: la cache de las
       ventanas de analisis y los recientes del selector de ficheros. */
    {
    gchar *c = g_build_filename(TRABAJO, "xdg", NULL);

    g_setenv("XDG_CACHE_HOME", c, TRUE);
    g_setenv("XDG_DATA_HOME", c, TRUE);
    g_setenv("XDG_CONFIG_HOME", c, TRUE);
    g_setenv("GSETTINGS_BACKEND", "memory", TRUE);
    g_free(c);
    }

    if (!gtk_init_check(&argc, &argv)) {
        printf("no hay servidor grafico: la prueba de manejo no se corre\n");
        return 0;
    }
    avisos = g_string_new(NULL);
    g_timeout_add(20, contesta, NULL);

    ctx = g_new0(FueContext, 1);
    app = gtk_application_new("org.atsw.fue.operar", G_APPLICATION_NON_UNIQUE);
    g_application_register(G_APPLICATION(app), NULL, NULL);
    ctx->main_window = create_main_window(app, ctx);
    /* Los widgets se ensenan --el cuaderno solo cambia a paginas visibles--
       pero la ventana no: no aparece nada salvo los dialogos y los
       graficos, que se abren solos y se cierran aqui mismo.           */
    gtk_widget_show_all(gtk_bin_get_child(GTK_BIN(ctx->main_window)));

    prueba_arranque(ctx);           printf("arranque           : hecho\n");
    prueba_ficheros_malos(ctx);     printf("ficheros malos     : hecho\n");
    prueba_construir(ctx);          printf("construir y estimar: hecho\n");
    prueba_fallos_motor(ctx);       printf("fallos del motor   : hecho\n");
    prueba_prevision(ctx);          printf("prevision          : hecho\n");
    prueba_analisis(ctx);           printf("analisis           : hecho\n");
    prueba_cambiar_de_modelo(ctx);  printf("cambiar de modelo  : hecho\n");

    printf("\n%d comprobaciones, %d fallos\n", checks, fails);
    return fails == 0 ? 0 : 1;
}
