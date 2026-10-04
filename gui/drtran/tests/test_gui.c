/* test_gui.c -- drtran_gui de verdad, conducido desde el codigo.
 *
 * Se levanta la ventana principal (sin ensenarla), se pulsan SUS botones --los
 * de verdad, buscados por su rotulo-- y se mira lo que el usuario acabaria
 * viendo: la barra de estado, los veredictos, las filas de las listas, el .out
 * en su pestana, los ficheros que deja en el disco y si los graficos dibujan
 * algo. El motor es el drtran que haya en el PATH: estimar es lanzarlo.
 *
 *   test_gui <datos> <trabajo>
 *
 *   datos    engines/drtran/tests/data (SYN_Y.pre, SYN_X.pre)
 *   trabajo  un directorio vacio; dentro va la cache del programa
 *
 * LOS DIALOGOS MODALES BLOQUEAN: gtk_dialog_run() no vuelve hasta que alguien
 * contesta. Aqui contesta un vigia (un g_timeout) que busca el dialogo entre
 * las ventanas de primer nivel, apunta lo que dice y hace lo que el plan de la
 * prueba le mande: elegir ficheros, mover un spin, aceptar o cancelar. Un
 * dialogo que nadie esperaba se cancela y se cuenta como fallo, para que la
 * prueba no se quede colgada esperando una mano que no existe.
 *
 * Sin servidor grafico no falla: avisa con una linea que empieza por "no hay"
 * y se va con 0. Cada fallo es una linea que empieza por "FAIL:".
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include "gui.h"

/* main.c entra compilado con -Dmain=drtran_main: asi se prueban sus opciones
 * sin otro binario. Las que no arrancan la ventana vuelven enseguida.      */
int drtran_main(int argc, char **argv);

static int fails = 0;

static void check(int ok, const char *what, const char *saw)
{
    if (ok) return;
    fails++;
    printf("FAIL: %s; se vio: \"%s\"\n", what, saw ? saw : "(nada)");
    fflush(stdout);
}

static void pump(int ms)
{
    gint64 until = g_get_monotonic_time() + (gint64) ms * 1000;

    while (g_get_monotonic_time() < until) {
        while (gtk_events_pending()) gtk_main_iteration_do(FALSE);
        g_usleep(2000);
    }
}

/* ------------------------------------------------------------------------ */
/* Buscar widgets como los buscaria el usuario: por lo que pone              */
/* ------------------------------------------------------------------------ */

typedef struct { const char *txt; GType tipo; GtkWidget *hallado; GPtrArray *todos; } Busca;

static void busca_rec(GtkWidget *w, gpointer data)
{
    Busca *b = data;

    if (b->txt && GTK_IS_BUTTON(w) && !b->hallado) {
        const char *l = gtk_button_get_label(GTK_BUTTON(w));
        if (l && strcmp(l, b->txt) == 0) b->hallado = w;
    }
    if (b->todos && G_TYPE_CHECK_INSTANCE_TYPE(w, b->tipo))
        g_ptr_array_add(b->todos, w);
    /* forall, no foreach: forall entra tambien en los hijos internos -- y en
     * una GtkWindow, en sus POPOVERS, que es donde estan los paneles.      */
    if (GTK_IS_CONTAINER(w))
        gtk_container_forall(GTK_CONTAINER(w), busca_rec, data);
}

static GtkWidget *boton(GtkWidget *raiz, const char *txt)
{
    Busca b = { txt, 0, NULL, NULL };

    busca_rec(raiz, &b);
    if (!b.hallado) check(0, "no encuentro el boton", txt);
    return b.hallado;
}

static GPtrArray *todos(GtkWidget *raiz, GType tipo)
{
    Busca b = { NULL, tipo, NULL, g_ptr_array_new() };

    busca_rec(raiz, &b);
    return b.todos;
}

static void pulsa(GtkWidget *b)
{
    if (b) gtk_button_clicked(GTK_BUTTON(b));
    pump(30);
}

/* Todo el texto de las etiquetas de un arbol de widgets, en una cadena. */
static gchar *texto_de(GtkWidget *raiz)
{
    GPtrArray *ls = todos(raiz, GTK_TYPE_LABEL);
    GString   *t = g_string_new(NULL);
    guint      i;

    for (i = 0; i < ls->len; i++) {
        const char *s = gtk_label_get_text(GTK_LABEL(g_ptr_array_index(ls, i)));
        if (s && *s) { g_string_append(t, s); g_string_append_c(t, '\n'); }
    }
    g_ptr_array_free(ls, TRUE);
    return g_string_free(t, FALSE);
}

static const char *etiqueta(GtkWidget *l)
{
    const char *s = gtk_label_get_text(GTK_LABEL(l));
    return s ? s : "";
}

static int contiene(const char *s, const char *que)
{
    return s && strstr(s, que) != NULL;
}

/* EL PANEL QUE ACABA DE ABRIR UN BOTON. Los paneles cuelgan de la ventana
 * (no de la pagina) y no se guardan en ningun sitio, asi que se buscan por
 * su ancla. Se lee el que esta visible y se cierra, para que el siguiente
 * clic no encuentre el viejo.                                            */
static gchar *panel_de(GtkWidget *ventana, GtkWidget *ancla)
{
    GPtrArray *ps = todos(ventana, GTK_TYPE_POPOVER);
    gchar     *txt = NULL;
    guint      i;

    for (i = 0; i < ps->len; i++) {
        GtkWidget *p = g_ptr_array_index(ps, i);

        if (gtk_popover_get_relative_to(GTK_POPOVER(p)) != ancla) continue;
        if (!gtk_widget_get_visible(p)) continue;
        g_free(txt);
        txt = texto_de(p);
        gtk_widget_hide(p);
    }
    g_ptr_array_free(ps, TRUE);
    return txt;
}

/* Pulsar un boton que abre un panel y devolver lo que dice. */
static gchar *pulsa_panel(GtkWidget *ventana, GtkWidget *raiz, const char *rotulo)
{
    GtkWidget *b = boton(raiz, rotulo);
    gchar     *t;

    if (!b) return g_strdup("");
    pulsa(b);
    t = panel_de(ventana, b);
    if (!t) {
        check(0, "el boton tiene que abrir un panel", rotulo);
        t = g_strdup("");
    }
    return t;
}

/* --- las listas --------------------------------------------------------- */

static int filas(GtkWidget *tv)
{
    return gtk_tree_model_iter_n_children(gtk_tree_view_get_model(GTK_TREE_VIEW(tv)), NULL);
}

/* La celda (fila, col) como texto, sea cadena o entero. Nueva; g_free. */
static gchar *celda(GtkWidget *tv, int fila, int col)
{
    GtkTreeModel *mod = gtk_tree_view_get_model(GTK_TREE_VIEW(tv));
    GtkTreeIter   it;
    GValue        v = G_VALUE_INIT, s = G_VALUE_INIT;
    gchar        *r;

    if (!gtk_tree_model_iter_nth_child(mod, &it, NULL, fila)) return g_strdup("");
    gtk_tree_model_get_value(mod, &it, col, &v);
    g_value_init(&s, G_TYPE_STRING);
    r = g_value_transform(&v, &s) && g_value_get_string(&s)
        ? g_strdup(g_value_get_string(&s)) : g_strdup("");
    g_value_unset(&v); g_value_unset(&s);
    return r;
}

static void marca(GtkWidget *tv, int fila)
{
    GtkTreePath *p = gtk_tree_path_new_from_indices(fila, -1);

    gtk_tree_selection_select_path(gtk_tree_view_get_selection(GTK_TREE_VIEW(tv)), p);
    gtk_tree_path_free(p);
    pump(20);
}

static gchar *buffer_de(GtkWidget *tv)
{
    GtkTextBuffer *b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));
    GtkTextIter    a, z;

    gtk_text_buffer_get_bounds(b, &a, &z);
    return gtk_text_buffer_get_text(b, &a, &z, FALSE);
}

static gchar *lee_fichero(const char *p)
{
    gchar *t = NULL;

    if (!g_file_get_contents(p, &t, NULL, NULL)) return g_strdup("");
    return t;
}

/* ------------------------------------------------------------------------ */
/* El vigia de los dialogos                                                  */
/* ------------------------------------------------------------------------ */

/* Lo que hay que hacer con el SIGUIENTE dialogo. */
typedef struct {
    int         resp;            /* la respuesta final                       */
    /* dialogo de ficheros */
    const char *ficheros[1];     /* abrir: el que se marca                   */
    int         nfich;           /* cuantos tienen que quedar marcados       */
    const char *carpeta;         /* guardar: donde; abrir: Ctrl+A en ella    */
    const char *nombre;          /*          y con que nombre                */
    /* dialogo propio */
    int         combo[4];        /* -1 = no tocar                            */
    int         spin[4];         /* -1 = no tocar                            */
    const char *entrada;         /* la primera GtkEntry que no es un spin    */
    const char *conmuta[4];      /* casillas o botones que se pulsan, por su rotulo */
    int         tic;
} Plan;

static Plan    *plan = NULL;
static GString *dlg_txt = NULL;   /* lo que decia el ultimo dialogo visto   */
static int      dlg_vistos = 0;

static Plan plan_vacio(int resp)
{
    Plan p;
    int  i;

    memset(&p, 0, sizeof p);
    p.resp = resp;
    for (i = 0; i < 4; i++) p.combo[i] = p.spin[i] = -1;
    return p;
}

static gboolean aplica_ficheros(GtkDialog *d, Plan *p)
{
    GtkFileChooser *fc = GTK_FILE_CHOOSER(d);

    if (p->tic++ == 0) {
        if (p->resp != GTK_RESPONSE_ACCEPT) { gtk_dialog_response(d, p->resp); return TRUE; }
        if (p->nombre) {
            gtk_file_chooser_set_current_folder(fc, p->carpeta);
            gtk_file_chooser_set_current_name(fc, p->nombre);
        } else if (p->carpeta)
            gtk_file_chooser_set_current_folder(fc, p->carpeta);
        else
            gtk_file_chooser_select_filename(fc, p->ficheros[0]);
        return FALSE;
    }
    /* El selector carga la carpeta EN DIFERIDO y marca los ficheros cuando
     * acaba: contestar antes devolveria una seleccion vacia. Se espera a
     * ver marcados los que tocan.                                       */
    if (!p->nombre) {
        GSList *s;
        int     n;

        /* VARIOS a la vez, como el usuario: Ctrl+A en la carpeta. Marcarlos
         * uno a uno no sirve -- cada marca mueve el cursor y borra la
         * anterior --, y el consejo del propio boton es justo este.      */
        if (p->carpeta) gtk_file_chooser_select_all(fc);
        s = gtk_file_chooser_get_filenames(fc);
        n = (int) g_slist_length(s);
        g_slist_free_full(s, g_free);
        if (n < p->nfich && p->tic < 200) return FALSE;
    } else if (p->tic < 4) return FALSE;

    gtk_dialog_response(d, p->resp);
    return TRUE;
}

/* EL ORDEN EN PANTALLA, no el de la lista de hijos. Una GtkGrid devuelve
 * sus hijos al reves de como se pusieron, asi que "el primer spin" de la
 * lista era el ultimo de la rejilla: se ordena por fila y columna.       */
static int pos_en_rejilla(GtkWidget *w)
{
    GtkWidget *g = gtk_widget_get_parent(w);
    int        top = 0, left = 0;

    if (!g || !GTK_IS_GRID(g)) return 0;
    gtk_container_child_get(GTK_CONTAINER(g), w, "top-attach", &top,
                            "left-attach", &left, NULL);
    return top * 100 + left;
}

static gint por_posicion(gconstpointer a, gconstpointer b)
{
    return pos_en_rejilla(*(GtkWidget **) a) - pos_en_rejilla(*(GtkWidget **) b);
}

static gboolean aplica_propio(GtkDialog *d, Plan *p)
{
    GtkWidget *c = gtk_dialog_get_content_area(d);
    GPtrArray *cb = todos(c, GTK_TYPE_COMBO_BOX);
    GPtrArray *sp = todos(c, GTK_TYPE_SPIN_BUTTON);
    GPtrArray *en = todos(c, GTK_TYPE_ENTRY);
    guint      i;
    int        k;

    g_ptr_array_sort(cb, por_posicion);
    g_ptr_array_sort(sp, por_posicion);
    for (i = 0; i < cb->len && i < 4; i++)
        if (p->combo[i] >= 0)
            gtk_combo_box_set_active(GTK_COMBO_BOX(g_ptr_array_index(cb, i)), p->combo[i]);
    for (i = 0; i < sp->len && i < 4; i++)
        if (p->spin[i] >= 0)
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_ptr_array_index(sp, i)), p->spin[i]);
    if (p->entrada)
        for (i = 0; i < en->len; i++)
            if (!GTK_IS_SPIN_BUTTON(g_ptr_array_index(en, i))) {
                gtk_entry_set_text(GTK_ENTRY(g_ptr_array_index(en, i)), p->entrada);
                break;
            }
    for (k = 0; k < 4 && p->conmuta[k]; k++) {
        Busca b = { p->conmuta[k], 0, NULL, NULL };

        busca_rec(c, &b);
        if (b.hallado) gtk_button_clicked(GTK_BUTTON(b.hallado));
        else check(0, "en el dialogo no esta", p->conmuta[k]);
    }
    g_ptr_array_free(cb, TRUE); g_ptr_array_free(sp, TRUE); g_ptr_array_free(en, TRUE);
    gtk_dialog_response(d, p->resp);
    return TRUE;
}

static gboolean vigia(gpointer data)
{
    GList *tl = gtk_window_list_toplevels(), *l;

    for (l = tl; l; l = l->next) {
        GtkWidget *w = l->data;
        gboolean   hecho;

        if (!GTK_IS_DIALOG(w) || !gtk_widget_get_visible(w)) continue;
        if (g_object_get_data(G_OBJECT(w), "contestado")) continue;

        if (!g_object_get_data(G_OBJECT(w), "visto")) {
            gchar *t = texto_de(w);

            g_object_set_data(G_OBJECT(w), "visto", GINT_TO_POINTER(1));
            dlg_vistos++;
            g_string_printf(dlg_txt, "%s\n%s",
                            gtk_window_get_title(GTK_WINDOW(w)) ?
                            gtk_window_get_title(GTK_WINDOW(w)) : "", t);
            g_free(t);
        }

        if (!plan) {
            check(0, "un dialogo que nadie esperaba (se cancela)", dlg_txt->str);
            gtk_dialog_response(GTK_DIALOG(w), GTK_RESPONSE_CANCEL);
            hecho = TRUE;
        } else if (GTK_IS_FILE_CHOOSER(w))
            hecho = aplica_ficheros(GTK_DIALOG(w), plan);
        else
            hecho = aplica_propio(GTK_DIALOG(w), plan);

        if (hecho) {
            g_object_set_data(G_OBJECT(w), "contestado", GINT_TO_POINTER(1));
            plan = NULL;
        }
    }
    g_list_free(tl);
    return G_SOURCE_CONTINUE;
}

/* Pulsar un boton que abre un dialogo, con el plan de lo que hay que hacer
 * en el. Devuelve TRUE si el dialogo salio.                             */
static gboolean pulsa_dialogo(GtkWidget *b, Plan *p)
{
    int antes = dlg_vistos;

    plan = p;
    pulsa(b);
    pump(30);
    if (plan == p) { plan = NULL; }
    return dlg_vistos > antes;
}

/* ------------------------------------------------------------------------ */
/* Lo que hay que esperar                                                    */
/* ------------------------------------------------------------------------ */

static gboolean espera_motor(Mtram *m, int segundos)
{
    gint64 hasta = g_get_monotonic_time() + (gint64) segundos * G_USEC_PER_SEC;

    pump(20);
    while (m->est.corriendo && g_get_monotonic_time() < hasta) pump(20);
    pump(50);
    return !m->est.corriendo;
}

/* LA TINTA DE UN GRAFICO. Se busca la ventana de graficos por su titulo (el
 * nombre del fichero), se le pide a SU area de dibujo que se pinte sobre una
 * imagen, y se cuentan los pixeles oscuros: una pagina en blanco no tiene
 * ninguno, un grafico con ejes y rotulos tiene miles. -1 si no hay ventana. */
static int tinta(const char *titulo)
{
    GList     *tl = gtk_window_list_toplevels(), *l;
    GtkWidget *win = NULL;
    int        n = -1;

    for (l = tl; l; l = l->next)
        if (GTK_IS_WINDOW(l->data) && gtk_widget_get_visible(l->data) &&
            g_strcmp0(gtk_window_get_title(GTK_WINDOW(l->data)), titulo) == 0)
            win = l->data;
    g_list_free(tl);
    if (!win) return -1;

    pump(150);
    {
    GPtrArray *as = todos(win, GTK_TYPE_DRAWING_AREA);

    if (as->len) {
        GtkWidget       *a = g_ptr_array_index(as, 0);
        int              w = gtk_widget_get_allocated_width(a);
        int              h = gtk_widget_get_allocated_height(a);
        cairo_surface_t *s;
        cairo_t         *cr;
        int              x, y, st;
        unsigned char   *px;

        if (w < 10 || h < 10) { w = 600; h = 400; gtk_widget_set_size_request(a, w, h); pump(100); }
        s  = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
        cr = cairo_create(s);
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_paint(cr);
        gtk_widget_draw(a, cr);
        cairo_destroy(cr);
        cairo_surface_flush(s);

        px = cairo_image_surface_get_data(s);
        st = cairo_image_surface_get_stride(s);
        n = 0;
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                guint32 v = *(guint32 *) (px + y * st + 4 * x);
                int r = (v >> 16) & 255, g = (v >> 8) & 255, b = v & 255;
                if (r + g + b < 200) n++;
            }
        cairo_surface_destroy(s);
    }
    g_ptr_array_free(as, TRUE);
    }
    /* Se esconde, no se destruye: lib/preview la reutiliza por ruta. */
    gtk_widget_hide(win);
    return n;
}

static void check_tinta(const char *titulo, const char *que)
{
    int  n = tinta(titulo);
    char b[64];

    snprintf(b, sizeof b, "%d pixeles oscuros en «%s»", n, titulo);
    check(n > 200, que, b);
    printf("grafico         : %s, %d pixeles de tinta\n", titulo, n);
}

/* ------------------------------------------------------------------------ */

static gchar *cache_de(const char *f)
{
    return g_build_filename(g_get_user_cache_dir(), GUI_CACHE, f, NULL);
}

static Mtram *ventana_nueva(GtkApplication *app)
{
    Mtram *m = g_new0(Mtram, 1);

    mtram_window_new(app, m);
    /* Los widgets se "ensenan" --hace falta para que los cuadernos cambien de
     * pestana-- pero la ventana NO: no aparece nada en la pantalla.     */
    gtk_widget_show_all(gtk_bin_get_child(GTK_BIN(m->ventana_p)));
    return m;
}

static GtkWidget *pagina(Mtram *m, int pg)
{
    return gtk_notebook_get_nth_page(GTK_NOTEBOOK(m->libro), pg);
}

static void escribe(const char *p, const char *txt)
{
    if (!g_file_set_contents(p, txt, -1, NULL)) check(0, "no pude escribir", p);
}

static void copia(const char *de, const char *a)
{
    gchar *t = NULL;
    gsize  n = 0;

    if (!g_file_get_contents(de, &t, &n, NULL)) { check(0, "no encuentro", de); return; }
    g_file_set_contents(a, t, (gssize) n, NULL);
    g_free(t);
}

/* ======================================================================== */
/* 0 · Las opciones de la linea de ordenes                                   */
/* ======================================================================== */

static void prueba_opciones(const char *trab)
{
    char  *a_help[]  = { "drtran_gui", "--help", NULL };
    char  *a_mal[]   = { "drtran_gui", "--no-existe", NULL };
    gchar *roto      = g_build_filename(trab, "roto.yaml", NULL);
    char  *a_roto[]  = { "drtran_gui", "--proyecto", roto, NULL };
    int    rc;

    /* Un manifiesto ROTO no se pisa: el programa lo dice y no arranca. */
    escribe(roto, "schema_version: 1\nmodelos:\n  - id: [sin cerrar\n\tbasura:: :\n");

    rc = drtran_main(2, a_help);
    check(rc == 0, "--help sale con 0", NULL);
    rc = drtran_main(2, a_mal);
    check(rc == 2, "una opcion desconocida sale con 2", NULL);
    rc = drtran_main(3, a_roto);
    {
    char b[32];
    snprintf(b, sizeof b, "rc=%d", rc);
    check(rc == 3, "--proyecto con un manifiesto roto sale con 3 sin arrancar", b);
    }
    g_free(roto);
}

/* ======================================================================== */

int main(int argc, char **argv)
{
    GtkApplication *app;
    Mtram          *m;
    const char     *datos, *trab0;
    gchar          *trab, *dd, *cache, *syn_y, *syn_x, *malo, *s, *t;
    GtkWidget      *pg;

    if (argc < 3) { fprintf(stderr, "uso: test_gui <datos> <trabajo>\n"); return 2; }
    datos = argv[1];
    trab0 = argv[2];

    /* La cache del programa, DENTRO del directorio de trabajo: modelo.out,
     * residuos.txt y los EPS van ahi y no a la del usuario. GLib mira esta
     * variable en las tres plataformas, y hay que ponerla antes de que nadie
     * pregunte por la cache: la recuerda.                               */
    trab  = g_canonicalize_filename(trab0, NULL);
    cache = g_build_filename(trab, "cache", NULL);
    g_mkdir_with_parents(cache, 0700);
    g_setenv("XDG_CACHE_HOME", cache, TRUE);
    g_chdir(trab);

    /* Los datos, copiados: el dialogo de Abrir los elige alli, y la prueba
     * de un .pre que desaparece no puede tocar los del repositorio.      */
    dd = g_build_filename(trab, "datos", NULL);
    g_mkdir_with_parents(dd, 0700);
    syn_y = g_build_filename(dd, "SYN_Y.pre", NULL);
    syn_x = g_build_filename(dd, "SYN_X.pre", NULL);
    /* El malo, en OTRA carpeta: en la de los buenos se cogeria con Ctrl+A. */
    malo  = g_build_filename(trab, "malos", "malo.pre", NULL);
    s = g_path_get_dirname(malo); g_mkdir_with_parents(s, 0700); g_free(s);
    s = g_build_filename(datos, "SYN_Y.pre", NULL); copia(s, syn_y); g_free(s);
    s = g_build_filename(datos, "SYN_X.pre", NULL); copia(s, syn_x); g_free(s);
    escribe(malo, "esto no es un .pre\n12\nni de lejos\n");

    prueba_opciones(trab);

    if (!gtk_init_check(&argc, &argv)) {
        printf("no hay servidor grafico: la prueba de la ventana no se corre\n");
        return 0;
    }
    if (!g_find_program_in_path("drtran"))
        check(0, "drtran tiene que estar en el PATH para esta prueba", g_getenv("PATH"));

    dlg_txt = g_string_new(NULL);
    g_timeout_add(25, vigia, NULL);

    app = gtk_application_new("org.atsw.drtran.test", G_APPLICATION_NON_UNIQUE);
    g_application_register(G_APPLICATION(app), NULL, NULL);
    m = ventana_nueva(app);

    check(contiene(etiqueta(m->estado), ".pre"), "al abrir, la barra pide los .pre",
          etiqueta(m->estado));
    check(!contiene(etiqueta(m->est.motor), "NO est"),
          "Estimacion dice que drtran esta en el PATH", etiqueta(m->est.motor));

    /* ==================================================================== */
    /* 1 · Series                                                            */
    /* ==================================================================== */
    pg = pagina(m, PG_SERIES);
    {
    Plan p = plan_vacio(GTK_RESPONSE_ACCEPT);

    /* Un fichero que no es un .pre: un mensaje, no un cuelgue. */
    p.ficheros[0] = malo; p.nfich = 1;
    check(pulsa_dialogo(boton(pg, "Abrir…"), &p), "«Abrir…» abre el selector", NULL);
    check(m->c.n == 0, "un .pre malo no se carga", etiqueta(m->estado));
    check(contiene(etiqueta(m->estado), "No pude con malo.pre"),
          "la barra dice que no pudo con el .pre malo, y cual", etiqueta(m->estado));
    printf("pre malo        : %s\n", etiqueta(m->estado));

    /* Cancelar no carga nada. */
    p = plan_vacio(GTK_RESPONSE_CANCEL);
    pulsa_dialogo(boton(pg, "Abrir…"), &p);
    check(m->c.n == 0, "cancelar el selector no carga nada", NULL);

    /* Los dos de golpe, como con Ctrl+A. */
    p = plan_vacio(GTK_RESPONSE_ACCEPT);
    p.carpeta = dd; p.nfich = 2;
    pulsa_dialogo(boton(pg, "Abrir…"), &p);
    }
    check(m->c.n == 2, "se cargan los dos .pre de golpe", etiqueta(m->estado));
    if (m->c.n != 2) { printf("\n%d fallos\n", fails); return 1; }

    /* Por orden alfabetico la salida es SYN_X: se dice. */
    check(contiene(etiqueta(m->estado), "SALIDA es «SYN_X»"),
          "la barra dice cual quedo de salida", etiqueta(m->estado));
    check(filas(m->lista) == 2, "la lista tiene dos filas", NULL);
    s = celda(m->lista, 0, 1);
    check(g_strcmp0(s, "SYN_X") == 0, "la primera fila es SYN_X", s); g_free(s);
    s = celda(m->lista, 0, 0);
    check(g_strcmp0(s, "1 Y") == 0, "y lleva la Y de salida", s); g_free(s);
    check(contiene(etiqueta(m->ver_ventana), "todas completas"),
          "veredicto de la ventana comun", etiqueta(m->ver_ventana));
    check(contiene(etiqueta(m->ver_oper), "iguales"),
          "veredicto de los operadores", etiqueta(m->ver_oper));
    printf("ventana         : %s\n", etiqueta(m->ver_ventana));

    /* «Salida» sin marcar nada: se pide que se marque. */
    gtk_tree_selection_unselect_all(gtk_tree_view_get_selection(GTK_TREE_VIEW(m->lista)));
    pulsa(boton(pg, "Salida"));
    check(contiene(etiqueta(m->estado), "Marca primero"),
          "«Salida» sin fila marcada lo pide", etiqueta(m->estado));

    /* SYN_Y a la salida, con el boton. */
    marca(m->lista, 1);
    pulsa(boton(pg, "Salida"));
    s = celda(m->lista, 0, 1);
    check(g_strcmp0(s, "SYN_Y") == 0, "«Salida» sube la marcada a la primera fila", s);
    g_free(s);
    check(contiene(etiqueta(m->estado), "SYN_Y es ahora la salida"),
          "y la barra lo dice", etiqueta(m->estado));

    /* Las flechas: abajo y otra vez arriba. */
    marca(m->lista, 0);
    pulsa(boton(pg, "↓"));
    s = celda(m->lista, 1, 1);
    check(g_strcmp0(s, "SYN_Y") == 0, "↓ baja la serie una plaza", s); g_free(s);
    pulsa(boton(pg, "↑"));      /* marca_fila la dejo marcada donde quedo */
    s = celda(m->lista, 0, 1);
    check(g_strcmp0(s, "SYN_Y") == 0, "↑ la sube otra vez", s); g_free(s);

    t = pulsa_panel(m->ventana_p, pg, "Ventana…");
    check(contiene(t, "Ventana común") && contiene(t, "SYN_X"),
          "«Ventana…» desglosa la ventana comun por serie", t);
    g_free(t);
    t = pulsa_panel(m->ventana_p, pg, "Operadores…");
    check(contiene(t, "SYN_Y") && contiene(t, "d=1"),
          "«Operadores…» da el polinomio de cada serie", t);
    g_free(t);

    /* ==================================================================== */
    /* 2 · Identificacion                                                    */
    /* ==================================================================== */
    pg = pagina(m, PG_IDENT);
    check(m->id.nent == 1 && filas(m->id.lista) == 1,
          "una fila por entrada candidata", NULL);
    check(m->id.u[0].vale, "la CCF preblanqueada se calcula", NULL);
    s = celda(m->id.lista, 0, 0);
    check(contiene(s, "SYN_X"), "la fila es la de SYN_X", s); g_free(s);
    check(contiene(etiqueta(m->id.ver_tran), "b="),
          "el veredicto propone (b, s)", etiqueta(m->id.ver_tran));
    check(contiene(etiqueta(m->id.ver_exo), "Exogeneidad"),
          "y dice la exogeneidad", etiqueta(m->id.ver_exo));
    printf("identificacion  : %s\n", etiqueta(m->id.ver_tran));

    /* Los retardos son un control de verdad: llegan a la CCF. */
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(m->id.s_lags), 12);
    pump(30);
    check(m->id.u[0].nlags == 12, "el spin de retardos recalcula con 12", NULL);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(m->id.s_lags), 0);
    pump(30);
    check(m->id.u[0].nlags != 12, "y con 0 vuelve a los del motor", NULL);

    /* El grafico: se escribe el EPS y se ve, con tinta. */
    pulsa(boton(pg, "CCF…"));
    s = cache_de("ccf.eps");
    check(g_file_test(s, G_FILE_TEST_EXISTS), "«CCF…» escribe el EPS en la cache", s);
    g_free(s);
    check_tinta("ccf.eps", "la CCF se dibuja en su ventana");

    t = pulsa_panel(m->ventana_p, pg, "Ecuación…");
    check(contiene(t, "SYN_Y") && contiene(t, "SYN_X"),
          "«Ecuación…» escribe la ecuacion con las dos series", t);
    g_free(t);

    /* EL GESTO QUE PUEBLA LA RED. */
    {
    int bb = m->id.u[0].b;

    pulsa(boton(pg, "Añadir a la red"));
    if (bb >= 0) {
        check(m->red.n == 1, "«Añadir a la red» mete el enlace", etiqueta(m->estado));
        check(contiene(etiqueta(m->estado), "SYN_Y ← SYN_X"),
              "y la barra lo dice", etiqueta(m->estado));
        /* Otra vez: no se duplica, se actualiza. */
        pulsa(boton(pg, "Añadir a la red"));
        check(m->red.n == 1 && contiene(etiqueta(m->estado), "ya estaba"),
              "reidentificar no duplica el enlace", etiqueta(m->estado));
    } else
        check(0, "la CCF de SYN tendria que proponer transferencia",
              etiqueta(m->id.ver_tran));
    }

    /* ==================================================================== */
    /* 3 · Red                                                               */
    /* ==================================================================== */
    pg = pagina(m, PG_RED);
    check(filas(m->red.lista) == 1, "la red ensena el enlace", NULL);
    check(contiene(etiqueta(m->red.ver_topo), "Acíclica"),
          "veredicto: aciclica", etiqueta(m->red.ver_topo));
    check(contiene(etiqueta(m->red.ver_forma), "ESTRELLA"),
          "y es una estrella", etiqueta(m->red.ver_forma));

    /* Editar: s = 1 en el dialogo. */
    marca(m->red.lista, 0);
    {
    Plan p = plan_vacio(GTK_RESPONSE_ACCEPT);
    int  b0 = m->red.lnk[0].b;

    p.spin[0] = b0; p.spin[1] = 0; p.spin[2] = 1;
    check(pulsa_dialogo(boton(pg, "Editar…"), &p), "«Editar…» abre el dialogo", NULL);
    check(m->red.lnk[0].s == 1, "el dialogo cambia s", NULL);
    s = celda(m->red.lista, 0, 5);
    check(g_strcmp0(s, "1") == 0, "y la lista lo ensena", s); g_free(s);
    s = celda(m->red.lista, 0, 6);
    check(g_strcmp0(s, "2") == 0, "con dos parametros (s+1+r)", s); g_free(s);
    }

    /* Nuevo, cancelado: nada cambia. */
    {
    Plan p = plan_vacio(GTK_RESPONSE_CANCEL);

    pulsa_dialogo(boton(pg, "Nuevo…"), &p);
    check(m->red.n == 1, "cancelar «Nuevo…» no anade nada", NULL);
    }

    /* Nuevo, una serie que se alimenta a si misma: se dice. */
    {
    Plan p = plan_vacio(GTK_RESPONSE_ACCEPT);

    p.combo[0] = 0; p.combo[1] = 0;
    pulsa_dialogo(boton(pg, "Nuevo…"), &p);
    check(m->red.n == 1, "un enlace de una serie consigo misma no entra", NULL);
    check(contiene(etiqueta(m->estado), "a sí misma"),
          "y la barra dice por que", etiqueta(m->estado));
    }

    /* Nuevo, SYN_X <- SYN_Y: cierra un ciclo. */
    {
    Plan p = plan_vacio(GTK_RESPONSE_ACCEPT);

    p.combo[0] = 1; p.combo[1] = 0;
    pulsa_dialogo(boton(pg, "Nuevo…"), &p);
    check(m->red.n == 2, "«Nuevo…» anade el enlace de vuelta", NULL);
    check(contiene(etiqueta(m->red.ver_topo), "CICLO"),
          "el veredicto dice que hay un ciclo", etiqueta(m->red.ver_topo));
    s = celda(m->red.lista, 1, 7);
    check(contiene(s, "ciclo"), "la fila dice que cierra el ciclo", s); g_free(s);
    t = pulsa_panel(m->ventana_p, pg, "Orden…");
    check(contiene(t, "NO HAY ORDEN"), "«Orden…» dice que no hay orden", t);
    g_free(t);
    }

    /* EL MOTOR SE NIEGA A UN CICLO, y la pantalla lo cuenta en vez de
     * romperse: es el camino de un motor que falla.                     */
    check(estima_lanzar(m), "con un ciclo el motor arranca igual (es el quien se niega)", NULL);
    check(espera_motor(m, 60), "y acaba", NULL);
    check(contiene(etiqueta(m->est.ver_fin), "No llegó a estimar") &&
          contiene(etiqueta(m->est.ver_fin), "salió con"),
          "un motor que falla da un veredicto rojo con su estado de salida",
          etiqueta(m->est.ver_fin));
    s = buffer_de(m->est.salida);
    check(contiene(s, "CYCLE"), "y la consola ensena lo que dijo el motor", s);
    g_free(s);
    check(gtk_widget_get_sensitive(m->est.boton), "«Estimar» vuelve a estar activo", NULL);
    check(!m->dia.vale, "Diagnosis no inventa una diagnosis de una corrida fallida",
          etiqueta(m->dia.ver_global));
    printf("motor que falla : %s\n", etiqueta(m->est.ver_fin));

    /* Se quita el enlace de vuelta. */
    marca(m->red.lista, 1);
    pulsa(boton(pg, "Quitar"));
    check(m->red.n == 1 && contiene(etiqueta(m->red.ver_topo), "Acíclica"),
          "«Quitar» se lleva el enlace marcado", etiqueta(m->red.ver_topo));

    /* Guardar y volver a abrir el .dag. */
    {
    Plan   p = plan_vacio(GTK_RESPONSE_ACCEPT);
    gchar *dag = g_build_filename(trab, "guardada.dag", NULL);
    gchar *mal = g_build_filename(trab, "mala.dag", NULL);

    p.carpeta = trab; p.nombre = "guardada.dag";
    pulsa_dialogo(boton(pg, "Guardar…"), &p);
    s = lee_fichero(dag);
    check(contiene(s, "SYN_Y") && contiene(s, "SYN_X"),
          "«Guardar…» escribe el .dag con los nombres", s);
    g_free(s);
    check(contiene(etiqueta(m->estado), "-n guardada.dag"),
          "y la barra dice como lo lee el motor", etiqueta(m->estado));

    /* Un .dag que nombra una serie que no esta: se dice cual y donde. */
    escribe(mal, "SYN_Y <- NADIE 1 0 0\n");
    p = plan_vacio(GTK_RESPONSE_ACCEPT);
    p.ficheros[0] = mal; p.nfich = 1;
    pulsa_dialogo(boton(pg, "Abrir…"), &p);
    check(m->red.n == 1, "un .dag malo no toca la red", NULL);
    check(contiene(etiqueta(m->estado), "NADIE"),
          "la barra nombra lo que no entiende", etiqueta(m->estado));

    p = plan_vacio(GTK_RESPONSE_ACCEPT);
    p.ficheros[0] = dag; p.nfich = 1;
    pulsa_dialogo(boton(pg, "Abrir…"), &p);
    check(m->red.n == 1 && m->red.lnk[0].s == 1 &&
          contiene(etiqueta(m->estado), "1 enlace leído"),
          "«Abrir…» relee el .dag guardado", etiqueta(m->estado));
    g_free(dag); g_free(mal);
    }

    t = pulsa_panel(m->ventana_p, pg, "Series…");
    check(contiene(t, "entrada pura") && contiene(t, "salida final"),
          "«Series…» da el papel de cada serie", t);
    g_free(t);

    /* ==================================================================== */
    /* 4 · Modelo                                                            */
    /* ==================================================================== */
    pg = pagina(m, PG_MODELO);
    check(m->mod.vale, "la tabla de parametros se construye", NULL);
    check(contiene(etiqueta(m->mod.ver_cuenta), "de transferencia"),
          "el veredicto cuenta lo que se decide aqui", etiqueta(m->mod.ver_cuenta));
    printf("modelo          : %s\n", etiqueta(m->mod.ver_cuenta));

    /* Sin marcar nada, «Fijar…» lo pide. */
    gtk_tree_selection_unselect_all(gtk_tree_view_get_selection(GTK_TREE_VIEW(m->mod.lista)));
    pulsa(boton(pg, "Libre"));
    check(contiene(etiqueta(m->estado), "Marca primero"),
          "«Libre» sin parametro marcado lo pide", etiqueta(m->estado));

    /* Un omega, marcado en el arbol, fijado a 0.5 y liberado otra vez. */
    {
    GtkTreeModel *mod = gtk_tree_view_get_model(GTK_TREE_VIEW(m->mod.lista));
    GtkTreeIter   it, hallado;
    gboolean      hay = FALSE;
    int           k = 0;
    GPtrArray    *pila = g_ptr_array_new();

    /* Recorrido a mano del arbol: gtk_tree_model_foreach no deja parar. */
    if (gtk_tree_model_get_iter_first(mod, &it))
        g_ptr_array_add(pila, gtk_tree_iter_copy(&it));
    while (pila->len && !hay) {
        GtkTreeIter *i = g_ptr_array_remove_index(pila, pila->len - 1), c;
        int          idx = 0;

        gtk_tree_model_get(mod, i, 3 /* M_IDX */, &idx, -1);
        if (idx > 0 && !strncmp(m->mod.st.name[idx], "omega", 5)) {
            hallado = *i; hay = TRUE; k = idx;
        }
        if (gtk_tree_model_iter_children(mod, &c, i))
            g_ptr_array_add(pila, gtk_tree_iter_copy(&c));
        c = *i;
        if (gtk_tree_model_iter_next(mod, &c))
            g_ptr_array_add(pila, gtk_tree_iter_copy(&c));
        gtk_tree_iter_free(i);
    }
    g_ptr_array_free(pila, TRUE);

    check(hay, "el arbol del modelo tiene los omega del enlace", NULL);
    if (hay) {
        Plan p = plan_vacio(GTK_RESPONSE_ACCEPT);

        gtk_tree_selection_select_iter(
            gtk_tree_view_get_selection(GTK_TREE_VIEW(m->mod.lista)), &hallado);
        p.entrada = "0.5";
        check(pulsa_dialogo(boton(pg, "Fijar…"), &p), "«Fijar…» abre el dialogo", NULL);
        check(m->mod.st.kind[k] == SLOT_FIXED && m->mod.st.value[k] == 0.5,
              "el omega queda fijo en 0.5", etiqueta(m->estado));
        check(contiene(etiqueta(m->estado), "= 0.5, fijo"),
              "y la barra lo dice", etiqueta(m->estado));

        /* El arbol se rehizo: se vuelve a marcar el mismo slot. */
        {
        GPtrArray *pl = g_ptr_array_new();
        gboolean   ok = FALSE;

        if (gtk_tree_model_get_iter_first(mod, &it))
            g_ptr_array_add(pl, gtk_tree_iter_copy(&it));
        while (pl->len) {
            GtkTreeIter *i = g_ptr_array_remove_index(pl, pl->len - 1), c;
            int          idx = 0;

            gtk_tree_model_get(mod, i, 3, &idx, -1);
            if (idx == k && !ok) {
                gtk_tree_selection_select_iter(
                    gtk_tree_view_get_selection(GTK_TREE_VIEW(m->mod.lista)), i);
                ok = TRUE;
            }
            if (gtk_tree_model_iter_children(mod, &c, i))
                g_ptr_array_add(pl, gtk_tree_iter_copy(&c));
            c = *i;
            if (gtk_tree_model_iter_next(mod, &c))
                g_ptr_array_add(pl, gtk_tree_iter_copy(&c));
            gtk_tree_iter_free(i);
        }
        g_ptr_array_free(pl, TRUE);
        }
        pulsa(boton(pg, "Libre"));
        check(m->mod.st.kind[k] == SLOT_FREE, "«Libre» lo vuelve a soltar",
              etiqueta(m->estado));
    }
    }

    /* Sigma: se libera la covarianza de las dos innovaciones. */
    {
    Plan p = plan_vacio(GTK_RESPONSE_ACCEPT);
    int  q = slots_find(&m->mod.st, "q[2,1]");

    p.conmuta[0] = "0";        /* la casilla de q[2,1], que nace en 0 */
    check(q > 0, "hay una q[2,1] en la tabla", NULL);
    check(pulsa_dialogo(boton(pg, "Covarianzas…"), &p),
          "«Covarianzas…» abre la matriz", NULL);
    q = slots_find(&m->mod.st, "q[2,1]");
    check(q > 0 && m->mod.st.kind[q] == SLOT_FREE,
          "pulsar la casilla libera q[2,1]", etiqueta(m->estado));
    check(contiene(etiqueta(m->estado), "1 covarianza cambiada"),
          "y la barra lo cuenta", etiqueta(m->estado));
    }

    t = pulsa_panel(m->ventana_p, pg, "Avisos…");
    check(t[0] != 0, "«Avisos…» dice algo", t);
    g_free(t);
    t = pulsa_panel(m->ventana_p, pg, "Empotrado…");
    check(t[0] != 0, "«Empotrado…» dice algo", t);
    g_free(t);

    /* Guardar el .cns, y un .cns que nombra un parametro que no existe. */
    {
    Plan   p = plan_vacio(GTK_RESPONSE_ACCEPT);
    gchar *cns = g_build_filename(trab, "guardado.cns", NULL);
    gchar *mal = g_build_filename(trab, "malo.cns", NULL);
    int    q;

    p.carpeta = trab; p.nombre = "guardado.cns";
    pulsa_dialogo(boton(pg, "Guardar…"), &p);
    s = lee_fichero(cns);
    check(contiene(s, "q[2,1]"), "«Guardar…» escribe la covarianza liberada en el .cns", s);
    g_free(s);

    /* La primera linea es buena y la segunda no: lo que se prueba es que
     * no quede MEDIA aplicada -- q[2,1] tiene que seguir libre, no fija en
     * el 0.3 de una lectura que fallo.                                  */
    escribe(mal, "q[2,1] = 0.3\nomega_inventado = free\n");
    p = plan_vacio(GTK_RESPONSE_ACCEPT);
    p.ficheros[0] = mal; p.nfich = 1;
    pulsa_dialogo(boton(pg, "Abrir…"), &p);
    check(contiene(etiqueta(m->estado), "omega_inventado"),
          "un .cns malo: la barra nombra el parametro que no existe", etiqueta(m->estado));
    q = slots_find(&m->mod.st, "q[2,1]");
    {
    char b[80];

    snprintf(b, sizeof b, "q[2,1]: tipo %d, valor %g", q > 0 ? m->mod.st.kind[q] : -1,
             q > 0 ? (double) m->mod.st.value[q] : 0.0);
    check(q > 0 && m->mod.st.kind[q] == SLOT_FREE,
          "y la tabla queda como estaba, sin media restriccion aplicada", b);
    }

    p = plan_vacio(GTK_RESPONSE_ACCEPT);
    p.ficheros[0] = cns; p.nfich = 1;
    pulsa_dialogo(boton(pg, "Abrir…"), &p);
    q = slots_find(&m->mod.st, "q[2,1]");
    check(q > 0 && m->mod.st.kind[q] == SLOT_FREE,
          "«Abrir…» el .cns guardado devuelve q[2,1] libre", etiqueta(m->estado));
    g_free(cns); g_free(mal);
    }

    /* ==================================================================== */
    /* 5 · Estimacion                                                        */
    /* ==================================================================== */
    pg = pagina(m, PG_ESTIMA);
    check(contiene(etiqueta(m->est.ver_que), "2 series") &&
          contiene(etiqueta(m->est.ver_que), "1 enlace"),
          "el veredicto dice que se va a estimar", etiqueta(m->est.ver_que));
    s = g_strdup(gtk_entry_get_text(GTK_ENTRY(m->est.orden)));
    check(contiene(s, "-n red.dag") && contiene(s, "-c modelo.cns") &&
          contiene(s, "-e residuos.txt"),
          "la orden lleva la red, las restricciones y los residuos", s);
    g_free(s);

    /* Diagonal es un modo: quita la red de la orden. */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m->est.c_diag), TRUE);
    pump(20);
    s = g_strdup(gtk_entry_get_text(GTK_ENTRY(m->est.orden)));
    check(contiene(s, " -0") && !contiene(s, "-n "), "Diagonal pone -0 y quita la red", s);
    g_free(s);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m->est.c_diag), FALSE);
    pump(20);

    /* Opciones: la traza. */
    {
    Plan p = plan_vacio(GTK_RESPONSE_ACCEPT);

    p.conmuta[0] = "-v   traza del optimizador";
    check(pulsa_dialogo(boton(pg, "Opciones…"), &p), "«Opciones…» abre el dialogo", NULL);
    check(m->est.traza, "la casilla de la traza se recoge", NULL);
    check(contiene(gtk_entry_get_text(GTK_ENTRY(m->est.orden)), " -v"),
          "y la orden lleva -v", gtk_entry_get_text(GTK_ENTRY(m->est.orden)));
    p = plan_vacio(GTK_RESPONSE_CANCEL);
    p.conmuta[0] = "-v   traza del optimizador";
    pulsa_dialogo(boton(pg, "Opciones…"), &p);
    check(m->est.traza, "cancelar el dialogo no cambia nada", NULL);
    p = plan_vacio(GTK_RESPONSE_ACCEPT);
    p.conmuta[0] = "-v   traza del optimizador";
    pulsa_dialogo(boton(pg, "Opciones…"), &p);
    check(!m->est.traza, "y se quita igual que se puso", NULL);
    }

    /* ESTIMAR, con el motor de verdad. */
    pulsa(boton(pg, "Estimar"));
    check(espera_motor(m, 90), "la estimacion acaba", etiqueta(m->est.titulo));
    check(contiene(etiqueta(m->est.titulo), "terminado"),
          "el titulo dice que drtran termino", etiqueta(m->est.titulo));
    check((contiene(etiqueta(m->est.ver_fin), "CONVERGE") ||
           contiene(etiqueta(m->est.ver_fin), "óptimo")) &&
          contiene(etiqueta(m->est.ver_fin), "logL"),
          "el desenlace tiene nombre y verosimilitud", etiqueta(m->est.ver_fin));
    printf("estimacion      : %s\n", etiqueta(m->est.ver_fin));
    check(gtk_notebook_get_current_page(GTK_NOTEBOOK(m->est.libreta)) == 1,
          "salta a la pestana del informe", NULL);
    s = buffer_de(m->est.informe);
    check(contiene(s, "Log-likelihood"), "el .out esta en su pestana", NULL);
    g_free(s);
    s = g_strdup(gtk_entry_get_text(GTK_ENTRY(m->est.orden)));
    check(contiene(s, "drtran ") && contiene(s, "SYN_Y.pre"),
          "la orden ejecutada queda a la vista, copiable", s);
    g_free(s);
    {
    const char *f[] = { "modelo.out", "residuos.txt", "red.dag", "modelo.cns", NULL };
    int         i;

    for (i = 0; f[i]; i++) {
        s = cache_de(f[i]);
        check(g_file_test(s, G_FILE_TEST_EXISTS), "la corrida deja su fichero en la cache", s);
        g_free(s);
    }
    }
    t = pulsa_panel(m->ventana_p, pg, "Criterio de parada…");
    check(contiene(t, "500 iteraciones"), "«Criterio de parada…» explica el desenlace", t);
    g_free(t);

    /* ==================================================================== */
    /* 6 · Diagnosis                                                         */
    /* ==================================================================== */
    pg = pagina(m, PG_DIAG);
    check(m->dia.vale, "la diagnosis se lee del .out", etiqueta(m->dia.ver_global));
    check(contiene(etiqueta(m->dia.ver_global), "Hosking"),
          "el veredicto global es el de Hosking", etiqueta(m->dia.ver_global));
    printf("diagnosis       : %s\n", etiqueta(m->dia.ver_global));
    printf("                  %s\n", etiqueta(m->dia.ver_ojo));
    check(filas(m->dia.l_exo) >= 1, "la tabla de exogeneidad tiene filas", NULL);
    check(filas(m->dia.l_ade) >= 1, "la de adecuacion tambien", NULL);
    check(filas(m->dia.l_res) >= 1, "la de residuos tambien", NULL);
    check(filas(m->dia.l_par) >= 3, "y la de parametros", NULL);
    check(m->dia.hay_res, "los residuos de esta corrida se leen", NULL);
    s = buffer_de(m->dia.t_out);
    check(contiene(s, "Log-likelihood"), "la pestana Salida tiene el .out", NULL);
    g_free(s);

    check(contiene(etiqueta(m->dia.l_ecu), "SYN_Y"), "empieza por la ecuacion de la salida",
          etiqueta(m->dia.l_ecu));
    pulsa(boton(pg, "▶"));
    check(contiene(etiqueta(m->dia.l_ecu), "SYN_X"), "▶ pasa a la siguiente ecuacion",
          etiqueta(m->dia.l_ecu));
    pulsa(boton(pg, "◀"));
    check(contiene(etiqueta(m->dia.l_ecu), "SYN_Y"), "◀ vuelve", etiqueta(m->dia.l_ecu));

    /* Los graficos de los residuos: el menu, y cada entrada dibuja. */
    pulsa(boton(pg, "Gráficos…"));
    {
    GList     *tl = gtk_window_list_toplevels(), *l;
    GtkWidget *menu = NULL;
    const char *que[] = { "Histograma", "Serie y ACF / PACF",
                          "CCF de los residuos con SYN_X", NULL };
    const char *tit[] = { "hist_res.eps", "res.eps", "ccf_res.eps" };
    int         i;

    for (l = tl; l; l = l->next) {
        GtkWidget *h = GTK_IS_WINDOW(l->data) ? gtk_bin_get_child(GTK_BIN(l->data)) : NULL;
        /* Sin exigir que este a la vista: con la ventana sin mapear, GTK
         * puede no llegar a desplegarlo, y lo que se prueba son sus
         * entradas.                                                    */
        if (h && GTK_IS_MENU(h)) menu = h;
    }
    g_list_free(tl);
    check(menu != NULL, "«Gráficos…» abre su menu", NULL);
    for (i = 0; menu && que[i]; i++) {
        GPtrArray *its = todos(menu, GTK_TYPE_MENU_ITEM);
        guint      j;
        gboolean   ok = FALSE;

        for (j = 0; j < its->len; j++) {
            GtkWidget *mi = g_ptr_array_index(its, j);
            if (g_strcmp0(gtk_menu_item_get_label(GTK_MENU_ITEM(mi)), que[i]) == 0) {
                gtk_menu_item_activate(GTK_MENU_ITEM(mi));
                ok = TRUE;
            }
        }
        g_ptr_array_free(its, TRUE);
        check(ok, "el menu de graficos tiene la entrada", que[i]);
        pump(50);
        if (ok) check_tinta(tit[i], que[i]);
    }
    if (menu) gtk_menu_popdown(GTK_MENU(menu));
    }

    /* Exportar la tabla de parametros, y la pestana que no es tabla. */
    {
    Plan   p = plan_vacio(GTK_RESPONSE_ACCEPT);
    gchar *csv = g_build_filename(trab, "parametros.csv", NULL);

    gtk_notebook_set_current_page(GTK_NOTEBOOK(m->dia.libreta), 4);
    p.carpeta = trab; p.nombre = "parametros.csv";
    pulsa_dialogo(boton(pg, "Exportar…"), &p);
    s = lee_fichero(csv);
    check(contiene(s, "omega") || contiene(s, "phi"),
          "«Exportar…» escribe la tabla de parametros", s);
    check(contiene(s, "modelo.out") || contiene(s, "SYN_Y"),
          "con la procedencia dentro", s);
    g_free(s); g_free(csv);

    gtk_notebook_set_current_page(GTK_NOTEBOOK(m->dia.libreta), 5);
    pulsa(boton(pg, "Exportar…"));
    check(contiene(etiqueta(m->estado), "ya ES un fichero"),
          "la pestana Salida no se exporta: se dice donde esta", etiqueta(m->estado));
    }

    /* EL BASELINE, desde aqui: pone el modo diagonal, lanza, fija y lo deja. */
    pulsa(boton(pg, "Calcular baseline"));
    check(espera_motor(m, 90), "el baseline acaba", NULL);
    check(m->dia.hay_base, "el baseline queda fijado", etiqueta(m->estado));
    check(!m->est.diagonal &&
          !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(m->est.c_diag)),
          "y el modo diagonal queda como estaba", NULL);
    /* La barra NO se comprueba: diagnosis_desde escribe «Baseline listo» y
     * on_done lo pisa enseguida con el mensaje del motor (estima.c, on_done:
     * el preview_show_status del final). Ver el informe.               */

    /* Y la estimacion con transferencia, otra vez: el LR contra el diagonal. */
    check(estima_lanzar(m), "se puede volver a estimar", NULL);
    check(espera_motor(m, 90), "y acaba", NULL);
    check(contiene(etiqueta(m->dia.l_lr), "LR"),
          "la pestana Ajuste da el LR contra el baseline", etiqueta(m->dia.l_lr));
    check(filas(m->dia.l_aju) >= 1, "y la tabla de ajuste tiene filas", NULL);
    printf("ajuste          : %s\n", etiqueta(m->dia.l_lr));

    /* ==================================================================== */
    /* 7 · Prevision                                                         */
    /* ==================================================================== */
    pg = pagina(m, PG_PREV);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m->prev.c_prever), TRUE);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(m->prev.s_hor), 6);
    pump(20);
    check(contiene(gtk_entry_get_text(GTK_ENTRY(m->est.orden)), "-f 6"),
          "prever pone -f en la orden al momento", gtk_entry_get_text(GTK_ENTRY(m->est.orden)));

    pulsa(m->prev.b_calc);
    check(espera_motor(m, 90), "la prevision acaba", NULL);
    check(m->prev.vale, "la prevision se lee del .out", etiqueta(m->prev.texto));
    check(contiene(etiqueta(m->prev.texto), "origen"),
          "y se ensena con su origen", etiqueta(m->prev.texto));
    check(m->prev.f.ns > 0 && m->prev.f.s[0].nprev == 6,
          "con los 6 periodos pedidos", etiqueta(m->prev.texto));

    /* Evaluar sin ventana: se dice, no se lanza. */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m->prev.c_eval), TRUE);
    pump(20);
    pulsa(m->prev.b_calc);
    check(!m->est.corriendo && contiene(etiqueta(m->estado), "ventana"),
          "evaluar sin ventana lo pide en vez de lanzar", etiqueta(m->estado));

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(m->prev.s_win), 300);
    pump(20);
    check(contiene(gtk_entry_get_text(GTK_ENTRY(m->est.orden)), "-estwin 300"),
          "la ventana va a la orden", gtk_entry_get_text(GTK_ENTRY(m->est.orden)));
    pulsa(m->prev.b_calc);
    check(espera_motor(m, 120), "la evaluacion acaba", NULL);
    check(m->prev.vale && m->prev.f.tiene_ev, "hay evaluacion fuera de muestra",
          etiqueta(m->prev.texto));
    check(filas(m->prev.lista) == 6, "una fila de error por horizonte", NULL);
    s = celda(m->prev.lista, 0, 3);
    check(s[0] && g_ascii_strtod(s, NULL) > 0.0, "con su RMSE", s); g_free(s);
    s = cache_de("evaluacion.csv");
    check(g_file_test(s, G_FILE_TEST_EXISTS), "y el CSV de la evaluacion en la cache", s);
    g_free(s);

    pulsa(boton(pg, "Guardar esta evaluación"));
    check(m->prev.tiene_ref, "«Guardar esta evaluación» la guarda", etiqueta(m->estado));
    pulsa(m->prev.b_calc);
    check(espera_motor(m, 120), "la segunda evaluacion acaba", NULL);
    s = celda(m->prev.lista, 0, 5);
    check(contiene(s, "igual"), "el mismo modelo, comparado con lo guardado, da igual", s);
    g_free(s);
    pulsa(boton(pg, "Olvidarla"));
    check(!m->prev.tiene_ref, "«Olvidarla» la olvida", NULL);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m->prev.c_eval), FALSE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m->prev.c_prever), FALSE);
    pump(20);

    /* ==================================================================== */
    /* Lo que pasa cuando el motor no esta                                   */
    /* ==================================================================== */
    {
    gchar *path = g_strdup(g_getenv("PATH"));
    gchar *vacio = g_build_filename(trab, "vacio", NULL);

    g_mkdir_with_parents(vacio, 0700);
    g_setenv("PATH", vacio, TRUE);
    mtram_refresca(m);
    check(contiene(etiqueta(m->est.motor), "NO est"),
          "sin drtran en el PATH, Estimacion lo dice", etiqueta(m->est.motor));
    check(!estima_lanzar(m), "y no finge que arranca", NULL);
    check(!m->est.corriendo && contiene(etiqueta(m->est.ver_fin), "No pude lanzar"),
          "el veredicto dice que no pudo lanzarlo", etiqueta(m->est.ver_fin));
    check(gtk_widget_get_sensitive(m->est.boton), "y «Estimar» sigue activo", NULL);
    g_setenv("PATH", path, TRUE);
    mtram_refresca(m);
    g_free(path); g_free(vacio);
    }

    /* Quitar una serie se lleva sus enlaces. */
    pg = pagina(m, PG_SERIES);
    marca(m->lista, 1);
    pulsa(boton(pg, "Quitar"));
    check(m->c.n == 1 && m->red.n == 0, "«Quitar» se lleva la serie y su enlace", NULL);
    check(contiene(etiqueta(m->estado), "1 enlace"), "y lo dice", etiqueta(m->estado));
    check(contiene(etiqueta(m->ver_ventana), "al menos dos"),
          "con una sola serie se piden dos", etiqueta(m->ver_ventana));

    /* ==================================================================== */
    /* El proyecto: cada estimacion es una corrida con su nombre y su linaje */
    /* ==================================================================== */
    {
    Mtram *p2 = g_new0(Mtram, 1);
    char   why[512] = "";
    gchar *dir = g_build_filename(trab, "proy", NULL);
    gchar *yaml, *out1 = NULL, *out2 = NULL;
    Serie *a, *b;

    g_mkdir_with_parents(dir, 0700);
    /* RELATIVO, como lo teclea el usuario en --proyecto: el directorio de
     * trabajo del programa es el de la prueba.                          */
    yaml = g_build_filename("proy", "p.yaml", NULL);
    check(mtram_proyecto_abre(p2, yaml, why, sizeof why),
          "--proyecto con un fichero que no existe empieza uno", why);
    mtram_window_new(app, p2);
    gtk_widget_show_all(gtk_bin_get_child(GTK_BIN(p2->ventana_p)));

    a = serie_cargar(syn_y, why, sizeof why);
    b = serie_cargar(syn_x, why, sizeof why);
    check(a && b, "se cargan los .pre en la ventana del proyecto", why);
    if (a && b) {
        p2->c.s[0] = a; p2->c.s[1] = b; p2->c.n = 2;
        mtram_refresca(p2);
        pulsa(boton(pagina(p2, PG_RED), "Estrella"));
        check(p2->red.n == 1, "«Estrella» pone la entrada contra la salida",
              etiqueta(p2->estado));

        /* Los residuos de la ventana SIN proyecto se borran: si la diagnosis
         * del proyecto los leyera --los de otro modelo-- la comprobacion de
         * abajo no lo notaria.                                           */
        s = cache_de("residuos.txt"); g_remove(s); g_free(s);

        check(estima_lanzar(p2), "la primera corrida arranca", etiqueta(p2->estado));
        check(espera_motor(p2, 90), "y acaba", NULL);
        out1 = g_strdup(p2->est.out_path);
        check(contiene(out1, "SYN_Y_") && !contiene(out1, "modelo.out"),
              "con proyecto, el .out lleva el nombre de la corrida", out1);
        check(out1 && g_file_test(out1, G_FILE_TEST_EXISTS),
              "y existe donde el GUI lo busca", out1);
        check(p2->dia.vale, "su diagnosis se lee", etiqueta(p2->dia.ver_global));
        check(p2->dia.hay_res, "y sus residuos, los de ESTA corrida", NULL);
        check(contiene(etiqueta(p2->est.ver_fin), "logL"),
              "el desenlace se cuenta igual", etiqueta(p2->est.ver_fin));

        check(estima_lanzar(p2), "la segunda corrida arranca", NULL);
        check(espera_motor(p2, 90), "y acaba", NULL);
        out2 = g_strdup(p2->est.out_path);
        check(g_strcmp0(out1, out2) != 0,
              "la segunda NO pisa a la primera: caben dos modelos", out2);
        check(out1 && g_file_test(out1, G_FILE_TEST_EXISTS),
              "el .out de la primera sigue ahi", out1);

        s = lee_fichero(yaml);
        check(contiene(s, p2->corrida) && contiene(s, p2->previa),
              "el manifiesto registra la corrida", s);
        check(p2->proy && p2->proy->nm >= 2, "con las dos corridas", s);
        if (p2->proy && p2->proy->nm >= 2)
            check(g_strcmp0(p2->proy->m[1].padre, p2->proy->m[0].id) == 0,
                  "y la segunda cuelga de la primera (el linaje)", s);
        printf("proyecto        : %s\n", out2 ? out2 : "(nada)");
        g_free(s);
    }
    g_free(dir); g_free(yaml); g_free(out1); g_free(out2);
    }

    printf("dialogos        : %d contestados\n", dlg_vistos);
    printf("\n%d fallos\n", fails);
    return fails == 0 ? 0 : 1;
}
