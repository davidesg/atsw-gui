/* test_drvarma_gui.c -- drvarma_gui de verdad, conducido desde el codigo.
 *
 * Se levanta la ventana principal (sin ensenarla) y se maneja como lo haria
 * el usuario: el selector de ficheros, el dialogo de propiedades, las
 * pestanas del cuaderno, los botones. Luego se mira lo que el usuario acaba
 * viendo: la barra de estado, el visor de texto y los ficheros que deja el
 * motor junto a los datos. El motor es el drvarma de verdad, buscado en el
 * PATH como lo busca el GUI; para el camino del motor que falla se usa un
 * sustituto compilado (fake_drvarma.c), que en Windows un guion de shell no
 * se puede lanzar.
 *
 *   test_drvarma_gui <carpeta con IPC.txt> <motor falso>
 *
 * drvarma_gui.c es un solo fuente con todo static, asi que se incluye aqui
 * entero (su main renombrado a drvarma_gui_main): es la unica forma de llegar a sus
 * callbacks y a sus widgets sin tocarlo.
 *
 * Sin servidor grafico no falla: dice "no hay ..." y sale con 0. Cada fallo
 * es una linea que empieza por "FAIL:" y el programa sale con 1.
 */

#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>

/* La ventana principal no se ensena: create_ui() termina con
 * gtk_widget_show_all(main_window), y eso la pondria en la pantalla de quien
 * corre las pruebas. Se ensenan sus hijos (hace falta para que los widgets
 * esten "visibles" y el cuaderno funcione), pero no la ventana. Los dialogos
 * solo llaman a show_all sobre su contenido, asi que no cambian.         */
static void prueba_show_all(GtkWidget *w)
{
    if (GTK_IS_APPLICATION_WINDOW(w))
        gtk_widget_show_all(gtk_bin_get_child(GTK_BIN(w)));
    else
        gtk_widget_show_all(w);
}
#define gtk_widget_show_all prueba_show_all
#define main drvarma_gui_main
#include "../../gui/drvarma_gui.c"
#undef main
#undef gtk_widget_show_all

/* ------------------------------------------------------------------ */
/* Contabilidad                                                        */
/* ------------------------------------------------------------------ */

static int fails = 0;
static int checks = 0;

static void check(int ok, const char *what, const char *saw)
{
    checks++;
    if (ok) return;
    fails++;
    printf("FAIL: %s; se vio: \"%s\"\n", what, saw ? saw : "(nada)");
    fflush(stdout);
}

/* Un aviso o un critical de GTK es un fallo: casi siempre es un widget mal
 * usado (un rango al reves, un widget destruido dos veces) que en otra
 * plataforma acaba en cuelgue.                                          */
static void on_log(const gchar *dom, GLogLevelFlags lvl, const gchar *msg, gpointer d)
{
    (void)d;
    if (lvl & (G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING | G_LOG_LEVEL_ERROR)) {
        fails++;
        printf("FAIL: aviso de %s: %s\n", dom ? dom : "?", msg);
        fflush(stdout);
    }
}

/* Si algo se cuelga (un dialogo que nadie contesta, un motor que no acaba)
 * mejor un FAIL que una CI parada hasta el limite de horas. Va en un hilo
 * porque g_spawn_sync bloquea el bucle principal y un g_timeout no saltaria. */
static gpointer vigia(gpointer d)
{
    (void)d;
    g_usleep(170 * G_USEC_PER_SEC);
    printf("FAIL: la prueba no acabo en 170 s (algo se colgo)\n");
    fflush(stdout);
    _Exit(1);
    return NULL;
}

static void pump(int ms)
{
    gint64 until = g_get_monotonic_time() + (gint64)ms * 1000;
    do {
        while (gtk_events_pending()) gtk_main_iteration_do(FALSE);
        g_usleep(2000);
    } while (g_get_monotonic_time() < until);
}

/* ------------------------------------------------------------------ */
/* Buscar widgets como los ve el usuario: por su rotulo                */
/* ------------------------------------------------------------------ */

typedef struct { GType type; const char *label; GtkWidget *found; } Busca;

static void busca_cb(GtkWidget *w, gpointer p)
{
    Busca *b = p;
    if (b->found) return;
    if (G_TYPE_CHECK_INSTANCE_TYPE(w, b->type)) {
        const char *l = NULL;
        if (GTK_IS_BUTTON(w)) l = gtk_button_get_label(GTK_BUTTON(w));
        else if (GTK_IS_LABEL(w)) l = gtk_label_get_text(GTK_LABEL(w));
        if (!b->label || (l && strcmp(l, b->label) == 0)) { b->found = w; return; }
    }
    if (GTK_IS_CONTAINER(w))
        gtk_container_forall(GTK_CONTAINER(w), busca_cb, b);
}

static GtkWidget *busca(GtkWidget *root, GType type, const char *label)
{
    Busca b = { type, label, NULL };
    if (root) busca_cb(root, &b);
    return b.found;
}

static GtkWidget *boton(GtkWidget *root, const char *label)
{
    GtkWidget *w = busca(root, GTK_TYPE_BUTTON, label);
    check(w != NULL, "existe el boton", label);
    return w;
}

/* La pagina del cuaderno que lleva esa pestana */
static GtkWidget *pagina(const char *tab)
{
    GtkWidget *nb = busca(main_window, GTK_TYPE_NOTEBOOK, NULL);
    if (!nb) return NULL;
    for (int i = 0; i < gtk_notebook_get_n_pages(GTK_NOTEBOOK(nb)); i++) {
        GtkWidget *p = gtk_notebook_get_nth_page(GTK_NOTEBOOK(nb), i);
        const char *t = gtk_notebook_get_tab_label_text(GTK_NOTEBOOK(nb), p);
        if (t && strcmp(t, tab) == 0) {
            gtk_notebook_set_current_page(GTK_NOTEBOOK(nb), i);
            return p;
        }
    }
    check(0, "existe la pestana", tab);
    return NULL;
}

/* El widget a la derecha del rotulo `label` en una rejilla: asi estan
 * puestos todos los spins del GUI ("AR order (p):" [spin]).             */
static GtkWidget *junto_a(GtkWidget *root, const char *label)
{
    GtkWidget *l = busca(root, GTK_TYPE_LABEL, label);
    GtkWidget *grid = l ? gtk_widget_get_parent(l) : NULL;
    gint left = 0, top = 0;
    GtkWidget *w = NULL;

    if (grid && GTK_IS_GRID(grid)) {
        gtk_container_child_get(GTK_CONTAINER(grid), l, "left-attach", &left,
                                "top-attach", &top, NULL);
        w = gtk_grid_get_child_at(GTK_GRID(grid), left + 1, top);
    }
    check(w != NULL, "hay un control junto al rotulo", label);
    return w;
}

static void pon_spin(GtkWidget *root, const char *label, double v)
{
    GtkWidget *s = junto_a(root, label);
    if (s && GTK_IS_SPIN_BUTTON(s)) gtk_spin_button_set_value(GTK_SPIN_BUTTON(s), v);
}

/* Una casilla se marca o desmarca con un clic, como el usuario */
static void marca(GtkWidget *root, const char *label, gboolean on)
{
    GtkWidget *c = boton(root, label);
    if (c && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(c)) != on)
        gtk_button_clicked(GTK_BUTTON(c));
}

static void clic(const char *label)
{
    GtkWidget *b = boton(main_window, label);
    if (!b) return;
    /* Un boton apagado el usuario no lo puede pulsar: si lo esta, es un fallo
     * y no se pulsa (gtk_button_clicked lo haria igual).                  */
    check(gtk_widget_is_sensitive(b), "el boton se puede pulsar", label);
    if (gtk_widget_is_sensitive(b)) gtk_button_clicked(GTK_BUTTON(b));
    pump(20);
}

static const char *estado(void)
{
    return gtk_label_get_text(GTK_LABEL(status_label));
}

/* Lo que hay en el visor (se libera con g_free) */
static gchar *visor(void)
{
    GtkTextBuffer *b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    GtkTextIter s, e;
    gtk_text_buffer_get_bounds(b, &s, &e);
    return gtk_text_buffer_get_text(b, &s, &e, FALSE);
}

static void visor_contiene(const char *needle, const char *what)
{
    gchar *t = visor();
    char saw[200];
    g_snprintf(saw, sizeof saw, "%.180s", t);
    check(strstr(t, needle) != NULL, what, saw);
    g_free(t);
}

static void estado_es(const char *want, const char *what)
{
    check(strcmp(estado(), want) == 0, what, estado());
}

static void estado_contiene(const char *want, const char *what)
{
    check(strstr(estado(), want) != NULL, what, estado());
}

/* ------------------------------------------------------------------ */
/* Dialogos modales: un temporizador los contesta                      */
/* ------------------------------------------------------------------ */

/* gtk_dialog_run() abre un bucle anidado y no vuelve hasta que alguien
 * contesta. Antes de pulsar el boton que lo abre se deja aqui QUE hacer con
 * el; el temporizador busca el dialogo entre las ventanas de nivel superior y
 * lo hace. Un dialogo que aparece sin esperarlo se cancela (para no colgar)
 * y se cuenta como fallo.                                               */
typedef void (*Accion)(GtkWidget *dlg);
static Accion pendientes[4];
static int n_pend = 0;
static char ultimo_titulo[256];

static void espera_dialogo(Accion a) { pendientes[n_pend++] = a; }

static void dialogos_contestados(const char *what)
{
    check(n_pend == 0, "aparecio el dialogo que se esperaba", what);
    n_pend = 0;
}

static GtkWidget *dialogo_abierto(void)
{
    GList *all = gtk_window_list_toplevels(), *l;
    GtkWidget *d = NULL;
    for (l = all; l; l = l->next)
        if (GTK_IS_DIALOG(l->data) && gtk_widget_get_visible(l->data)
            && l->data != (gpointer)main_window)
            d = l->data;
    g_list_free(all);
    return d;
}

static gboolean contestador(gpointer u)
{
    static gboolean dentro = FALSE;
    GtkWidget *d;
    (void)u;
    if (dentro) return G_SOURCE_CONTINUE;
    d = dialogo_abierto();
    if (!d) return G_SOURCE_CONTINUE;
    dentro = TRUE;
    g_strlcpy(ultimo_titulo, gtk_window_get_title(GTK_WINDOW(d)) ?
              gtk_window_get_title(GTK_WINDOW(d)) : "", sizeof ultimo_titulo);
    if (n_pend > 0) {
        Accion a = pendientes[0];
        memmove(pendientes, pendientes + 1, sizeof(Accion) * (size_t)(--n_pend));
        a(d);
    } else {
        check(0, "no se esperaba ningun dialogo", ultimo_titulo);
        gtk_dialog_response(GTK_DIALOG(d), GTK_RESPONSE_CANCEL);
    }
    dentro = FALSE;
    return G_SOURCE_CONTINUE;
}

/* --- El selector de ficheros ---------------------------------------- */

static gchar *fichero_a_elegir = NULL;

/* Se pone el fichero y se espera a que el selector lo tenga de verdad
 * (carga la carpeta en segundo plano) antes de pulsar "Open".          */
static void elige_fichero(GtkWidget *d)
{
    GtkFileChooser *fc = GTK_FILE_CHOOSER(d);
    gchar *got = NULL;
    gint64 hasta = g_get_monotonic_time() + 5 * G_USEC_PER_SEC;

    check(GTK_IS_FILE_CHOOSER(d), "el dialogo es un selector de ficheros", ultimo_titulo);
    if (!GTK_IS_FILE_CHOOSER(d)) { gtk_dialog_response(GTK_DIALOG(d), GTK_RESPONSE_CANCEL); return; }
    gtk_file_chooser_set_filename(fc, fichero_a_elegir);
    while (g_get_monotonic_time() < hasta) {
        g_free(got);
        got = gtk_file_chooser_get_filename(fc);
        if (got) break;
        while (gtk_events_pending()) gtk_main_iteration_do(FALSE);
        g_usleep(5000);
    }
    check(got != NULL, "el selector de ficheros tomo el fichero", fichero_a_elegir);
    g_free(got);
    gtk_dialog_response(GTK_DIALOG(d), GTK_RESPONSE_ACCEPT);
}

/* --- El dialogo de propiedades de los datos -------------------------- */

static int props_ok = 1;      /* OK o Cancel */
static int props_n_obs = 216; /* para comprobar la fecha final */

static void contesta_propiedades(GtkWidget *d)
{
    GtkWidget *c = gtk_dialog_get_content_area(GTK_DIALOG(d));
    GtkWidget *freq = busca(c, GTK_TYPE_COMBO_BOX_TEXT, NULL);
    GtkWidget *year = junto_a(c, "Start year:");
    GtkWidget *sub = junto_a(c, "Start month/quarter:");
    GtkWidget *endl = NULL;
    GList *kids, *l;

    check(strcmp(ultimo_titulo, "Data Properties") == 0, "tras cargar sale \"Data Properties\"",
          ultimo_titulo);
    if (!freq || !year || !sub) { gtk_dialog_response(GTK_DIALOG(d), GTK_RESPONSE_CANCEL); return; }

    /* el rotulo de la fecha final */
    kids = gtk_container_get_children(GTK_CONTAINER(gtk_widget_get_parent(year)));
    for (l = kids; l; l = l->next)
        if (GTK_IS_LABEL(l->data) &&
            g_str_has_prefix(gtk_label_get_text(GTK_LABEL(l->data)), "End date"))
            endl = l->data;
    g_list_free(kids);
    check(endl != NULL, "el dialogo dice la fecha final", NULL);

    /* Trimestral desde 2002: la fecha final se recalcula al momento. */
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(year), 2002);
    gtk_combo_box_set_active(GTK_COMBO_BOX(freq), 1);
    if (endl && props_n_obs == 216)
        check(strcmp(gtk_label_get_text(GTK_LABEL(endl)), "End date: Q4 2055") == 0,
              "216 trimestres desde Q1 2002 acaban en Q4 2055",
              gtk_label_get_text(GTK_LABEL(endl)));
    /* Anual: el subperiodo se esconde */
    gtk_combo_box_set_active(GTK_COMBO_BOX(freq), 0);
    check(!gtk_widget_get_visible(sub), "en anual el subperiodo se esconde", NULL);
    /* Mensual desde 1/2002: 216 meses acaban en 12/2019 */
    gtk_combo_box_set_active(GTK_COMBO_BOX(freq), 2);
    check(gtk_widget_get_visible(sub), "en mensual el subperiodo vuelve", NULL);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(sub), 1);
    if (endl && props_n_obs == 216)
        check(strcmp(gtk_label_get_text(GTK_LABEL(endl)), "End date: 12/2019") == 0,
              "216 meses desde 1/2002 acaban en 12/2019", gtk_label_get_text(GTK_LABEL(endl)));
    gtk_dialog_response(GTK_DIALOG(d), props_ok ? GTK_RESPONSE_OK : GTK_RESPONSE_CANCEL);
}

/* --- Los dialogos de parametros (Johansen, VECM, orden del VAR) ------ */

static const char *dlg_titulo_esperado;
static int dlg_lag = 2, dlg_rank = 1, dlg_det = 1;
static int dlg_ok = 1;

static void contesta_parametros(GtkWidget *d)
{
    GtkWidget *c = gtk_dialog_get_content_area(GTK_DIALOG(d));
    GtkWidget *lag = busca(c, GTK_TYPE_SPIN_BUTTON, NULL);
    GtkWidget *det = busca(c, GTK_TYPE_COMBO_BOX_TEXT, NULL);
    GtkWidget *l;

    check(strcmp(ultimo_titulo, dlg_titulo_esperado) == 0, "el dialogo es el del boton",
          ultimo_titulo);
    if (lag) gtk_spin_button_set_value(GTK_SPIN_BUTTON(lag), dlg_lag);
    if (det) gtk_combo_box_set_active(GTK_COMBO_BOX(det), dlg_det);
    l = busca(c, GTK_TYPE_LABEL, "Cointegration rank (r):");
    if (l) {
        GtkWidget *r = junto_a(c, "Cointegration rank (r):");
        if (r) gtk_spin_button_set_value(GTK_SPIN_BUTTON(r), dlg_rank);
    }
    gtk_dialog_response(GTK_DIALOG(d), dlg_ok ? GTK_RESPONSE_OK : GTK_RESPONSE_CANCEL);
}

/* ------------------------------------------------------------------ */
/* Ficheros                                                            */
/* ------------------------------------------------------------------ */

static gchar *work = NULL;

static gchar *en_work(const char *name) { return g_build_filename(work, name, NULL); }

static gchar *lee(const char *name)
{
    gchar *p = en_work(name), *c = NULL;
    if (!g_file_get_contents(p, &c, NULL, NULL)) c = NULL;
    g_free(p);
    return c;
}

static void escribe(const char *name, const char *text)
{
    gchar *p = en_work(name);
    g_file_set_contents(p, text, -1, NULL);
    g_free(p);
}

static void borra(const char *name)
{
    gchar *p = en_work(name);
    g_remove(p);
    g_free(p);
}

static void fichero_contiene(const char *name, const char *needle, const char *what)
{
    gchar *c = lee(name);
    char saw[200];
    g_snprintf(saw, sizeof saw, "%s: %.150s", name, c ? c : "(no existe)");
    check(c && strstr(c, needle), what, saw);
    g_free(c);
}

/* Carga un fichero con el boton y el selector, contestando las propiedades */
static void carga(const char *name, int con_propiedades, int ok)
{
    g_free(fichero_a_elegir);
    fichero_a_elegir = en_work(name);
    espera_dialogo(elige_fichero);
    if (con_propiedades) {
        props_ok = ok;
        espera_dialogo(contesta_propiedades);
    }
    clic("Load Data File");
    dialogos_contestados(name);
}

/* ------------------------------------------------------------------ */
/* Las pruebas                                                         */
/* ------------------------------------------------------------------ */

static void prueba_sin_datos(void)
{
    gchar *t;
    t = g_strdup(gtk_window_get_title(GTK_WINDOW(main_window)));
    check(strstr(t, "DRVARMA") != NULL, "el titulo dice DRVARMA", t);
    g_free(t);
    estado_es("No data loaded.", "al arrancar no hay datos");
    check(!gtk_widget_is_sensitive(btn_run), "sin datos Run esta apagado", NULL);
    check(!gtk_widget_is_sensitive(boton(main_window, "Run & Forecast")),
          "sin datos Run & Forecast esta apagado", NULL);

    /* Los botones encendidos, sin datos, avisan en vez de romperse */
    clic("Johansen Test");   estado_es("Please set data properties first.", "Johansen sin datos");
    clic("Estimate VECM");   estado_es("Please set data properties first.", "VECM sin datos");
    clic("Select VAR Order");estado_es("Please set data properties first.", "orden VAR sin datos");
    pagina("Seasonal Adjustment");
    clic("Test Seasonality");estado_es("No data or properties not set.", "test estacional sin datos");
    clic("View Output");     estado_es("No output file available.", "ver .out sin datos");
    clic("View .inp");       estado_es("No input file generated yet.", "ver .inp sin datos");
}

static void prueba_carga(void)
{
    /* Un fichero con texto y otro de una sola fila: aviso, sin propiedades */
    carga("malo.txt", 0, 0);
    estado_contiene("Error reading file", "un fichero no numerico se rechaza");
    carga("una_fila.txt", 0, 0);
    estado_contiene("Error reading file", "un fichero de una fila se rechaza");
    /* El bueno, pero cancelando las propiedades: se queda sin poder correr */
    carga("IPC.txt", 1, 0);
    estado_es("Loaded 3 series, 216 observations", "carga de IPC.txt");
    check(!gtk_widget_is_sensitive(btn_run), "sin propiedades Run sigue apagado", NULL);
    clic("Johansen Test");
    estado_es("Please set data properties first.", "sin propiedades Johansen avisa");

    /* Y ahora aceptando */
    carga("IPC.txt", 1, 1);
    estado_es("Data properties set. Ready.", "propiedades puestas");
    check(gtk_widget_is_sensitive(btn_run), "con propiedades Run se enciende", NULL);
    check(gtk_widget_is_sensitive(boton(main_window, "Run & Forecast")),
          "con propiedades Run & Forecast se enciende", NULL);
    check(label_freq_info_seasonal &&
          strcmp(gtk_label_get_text(GTK_LABEL(label_freq_info_seasonal)),
                 "(Data frequency: 12 period(s) per year)") == 0,
          "la pestana estacional dice la frecuencia",
          label_freq_info_seasonal ? gtk_label_get_text(GTK_LABEL(label_freq_info_seasonal)) : NULL);

    /* Un fichero malo DESPUES de uno bueno no puede dejar el GUI a medias:
     * los datos buenos siguen ahi y se puede seguir trabajando con ellos. */
    carga("malo.txt", 0, 0);
    estado_contiene("Error reading file", "un fichero malo tras uno bueno se rechaza");
    pagina("Seasonal Adjustment");
    clic("Test Seasonality");
    visor_contiene("y3: ", "tras el fichero malo siguen las 3 series de IPC");
}

static void prueba_run_basico(void)
{
    gchar *inp;
    int filas = 0;

    borra("IPC.inp"); borra("IPC.out");
    /* El motor por defecto se busca en el PATH, como en una instalacion */
    clic("Run DRVARMA");
    estado_es("DRVARMA finished successfully.", "Run con el motor del PATH");
    fichero_contiene("IPC.inp", " 3 216 1 2002", "el .inp lleva series, obs y fecha");
    fichero_contiene("IPC.inp", " y1 y2 y3", "el .inp lleva los nombres");
    fichero_contiene("IPC.inp", "** Box-Cox lambda, regular differences, annual differences:\n 1 0 0",
                     "sin transformar: lambda 1, d=0, D=0");
    inp = lee("IPC.inp");
    if (inp) {
        char *p = strstr(inp, "** Data:\n");
        if (p) for (p += 9; *p; p++) if (*p == '\n') filas++;
    }
    check(filas == 216, "el .inp lleva las 216 filas", inp ? "" : "(no existe)");
    g_free(inp);
    fichero_contiene("IPC.out", "Model: VARMA(1,1)", "el motor estimo el VARMA(1,1)");

    clic("View Output");
    estado_es("Output displayed.", "ver la salida");
    visor_contiene("Model: VARMA(1,1)", "el visor ensena el .out");
    clic("View .inp");
    estado_es("Input file displayed.", "ver el .inp");
    visor_contiene("** Data:", "el visor ensena el .inp");
}

static void prueba_opciones_modelo(void)
{
    GtkWidget *pg;

    pg = pagina("Model");
    pon_spin(pg, "AR order (p):", 1);
    pon_spin(pg, "MA order (q):", 0);
    marca(pg, "Include mean (-mean)", TRUE);
    marca(pg, "Two-step initialisation (-twostep)", TRUE);
    pg = pagina("Matrix Structure");
    marca(pg, "Diagonal AR (-diagar)", TRUE);
    marca(pg, "Diagonal covariance (-diagcov)", TRUE);
    pg = pagina("Estimation");
    gtk_button_clicked(GTK_BUTTON(boton(pg, "Approximate (2)")));
    pg = pagina("Volatility");
    marca(pg, "Exponential volatility (-volexp)", TRUE);
    pon_spin(pg, "  Alpha:", 0.1);
    marca(pg, "Moving volatility (-volmov)", TRUE);
    pg = pagina("Data Transformation");
    marca(pg, "Apply log transform", TRUE);
    pon_spin(pg, "Regular differencing (d):", 1);

    borra("IPC.out"); borra("IPC.volexp"); borra("IPC.volmov");
    clic("Run DRVARMA");
    estado_es("DRVARMA finished successfully.", "Run con todas las opciones");
    fichero_contiene("IPC.inp", "differences:\n 0 1 0", "log y d=1 van a la cabecera del .inp");
    fichero_contiene("IPC.out", "Model: VARMA(1,0)", "p y q llegan al motor");
    fichero_contiene("IPC.out", "Include mean     : yes", "-mean llega al motor");
    fichero_contiene("IPC.out", "Two-step init    : yes", "-twostep llega al motor");
    fichero_contiene("IPC.out", "Diagonal AR      : yes", "-diagar llega al motor");
    fichero_contiene("IPC.out", "Diagonal MA      : no", "-diagma no se pidio");
    fichero_contiene("IPC.out", "Diagonal Cov     : yes", "-diagcov llega al motor");
    fichero_contiene("IPC.out", "Estimation method: 2", "-m 2 llega al motor");
    fichero_contiene("IPC.out", "Box-Cox lambda   : 0", "el log llega al motor");
    fichero_contiene("IPC.out", "d=1, D=0", "d=1 llega al motor");
    clic("View Output");
    visor_contiene("Estimated Parameters", "el visor trae la tabla de estimaciones");
    {
        gchar *v = lee("IPC.volexp"), *m = lee("IPC.volmov");
        check(v != NULL && *v, "-volexp deja IPC.volexp", NULL);
        check(m != NULL && *m, "-volmov deja IPC.volmov", NULL);
        g_free(v); g_free(m);
    }

    /* Se quitan las opciones caras; se quedan log y d=1 */
    pg = pagina("Model");
    marca(pg, "Include mean (-mean)", FALSE);
    marca(pg, "Two-step initialisation (-twostep)", FALSE);
    pg = pagina("Matrix Structure");
    marca(pg, "Diagonal AR (-diagar)", FALSE);
    marca(pg, "Diagonal covariance (-diagcov)", FALSE);
    pg = pagina("Estimation");
    gtk_button_clicked(GTK_BUTTON(boton(pg, "Exact (1)")));
    pg = pagina("Volatility");
    marca(pg, "Exponential volatility (-volexp)", FALSE);
    marca(pg, "Moving volatility (-volmov)", FALSE);
    check(opts.method == 1 && !opts.include_mean && !opts.use_exp_vol && !opts.use_mov_vol,
          "las casillas desmarcadas quitan sus opciones", NULL);
}

static void prueba_estacionalidad(void)
{
    GtkWidget *pg = pagina("Seasonal Adjustment");
    GtkWidget *force = boton(pg, "Force all series");
    GtkWidget *autom = boton(pg, "Only if significant (p < 0.05)");
    gchar *t;
    int n = 0;

    pon_spin(pg, "Seasonal period (s):", 12);
    clic("Test Seasonality");
    estado_es("Seasonality test completed.", "test de estacionalidad");
    visor_contiene("=== SEASONALITY TEST RESULTS ===", "el visor trae el informe estacional");
    visor_contiene("Period s=12, differencing d=1", "el test usa s y d de la pantalla");
    t = visor();
    /* " -> " y no "SEASONAL", que tambien esta dentro de "NOT SEASONAL" */
    for (char *p = t; (p = strstr(p, " -> ")) != NULL; p++) n++;
    check(n == 3, "un veredicto por serie", t);
    g_free(t);

    /* Los modos solo se pueden tocar con el ajuste activado */
    check(force && !gtk_widget_is_sensitive(force), "sin ajuste los modos estan apagados", NULL);
    marca(pg, "Apply seasonal adjustment", TRUE);
    check(force && gtk_widget_is_sensitive(force), "con ajuste los modos se encienden", NULL);

    if (force) gtk_button_clicked(GTK_BUTTON(force));
    clic("Run DRVARMA");
    estado_es("DRVARMA finished successfully.", "Run con ajuste estacional forzado");
    visor_contiene("Seasonal adjustment (levels, s=12, mode=force all)",
                   "el visor dice el ajuste aplicado");
    t = visor();
    n = 0;
    for (char *p = t; (p = strstr(p, "(adjusted)")) != NULL; p++) n++;
    check(n == 3, "forzado: las tres series se ajustan", t);
    g_free(t);
    fichero_contiene("IPC.inp", "* series seasonally adjusted (harmonic), s=12",
                     "el .inp dice que va desestacionalizado");
    {
        /* la primera fila ya no es la del fichero: se le quito la estacionalidad */
        gchar *c = lee("IPC.inp");
        char *p = c ? strstr(c, "** Data:\n") : NULL;
        double y1 = p ? g_ascii_strtod(p + 9, NULL) : 0;
        check(p && fabs(y1 - 69.53) > 1e-4, "los datos del .inp estan ajustados", p);
        g_free(c);
    }

    if (autom) gtk_button_clicked(GTK_BUTTON(autom));
    clic("Run DRVARMA");
    estado_es("DRVARMA finished successfully.", "Run con ajuste estacional automatico");
    visor_contiene("mode=auto (p<0.05)", "el modo automatico llega al ajuste");
    marca(pg, "Apply seasonal adjustment", FALSE);
}

static void prueba_prediccion(void)
{
    GtkWidget *pg = pagina("Forecast");
    gchar *t, *s1, *s2;
    int n = 0;

    marca(pg, "Enable forecast (-forecast)", TRUE);
    pon_spin(pg, "Horizon (L):", 12);
    pon_spin(pg, "Seasonal period (s):", 12);
    borra("IPC.forecast");
    clic("Run & Forecast");
    estado_es("Forecast displayed.", "Run & Forecast ensena la prediccion");
    visor_contiene("Forecasts from VARMA(1,0)", "el visor trae el .forecast");
    visor_contiene("horizon=12", "el horizonte llega al motor");
    /* doce fechas para la serie 1, de 1/2020 a 12/2020 */
    t = visor();
    s1 = strstr(t, "Series 1");
    s2 = s1 ? strstr(s1, "Series 2") : NULL;
    if (s1 && s2) for (char *p = s1; p < s2 && (p = strstr(p, "/2020")) && p < s2; p++) n++;
    check(n == 12, "12 predicciones para la serie 1, todas en 2020", s1);
    g_free(t);
    marca(pg, "Enable forecast (-forecast)", FALSE);
}

static void prueba_cointegracion(void)
{
    gchar *antes, *despues;

    dlg_titulo_esperado = "Johansen Cointegration Test";
    dlg_lag = 2; dlg_det = 2; dlg_ok = 1;
    espera_dialogo(contesta_parametros);
    clic("Johansen Test");
    dialogos_contestados("Johansen");
    estado_es("Johansen test completed.", "Johansen");
    visor_contiene("--- Preprocessing report ---", "Johansen dice el preproceso");
    visor_contiene("Johansen Cointegration Test Results", "Johansen trae sus resultados");
    visor_contiene("H0: r <= 2", "una hipotesis por serie");

    /* Cancelar no toca nada */
    antes = visor();
    dlg_ok = 0;
    espera_dialogo(contesta_parametros);
    clic("Johansen Test");
    dialogos_contestados("Johansen cancelado");
    despues = visor();
    check(strcmp(antes, despues) == 0, "cancelar Johansen deja el visor como estaba", NULL);
    g_free(antes); g_free(despues);

    dlg_titulo_esperado = "Estimate VECM";
    dlg_lag = 2; dlg_rank = 1; dlg_det = 1; dlg_ok = 1;
    espera_dialogo(contesta_parametros);
    clic("Estimate VECM");
    dialogos_contestados("VECM");
    estado_es("VECM estimation completed.", "VECM");
    visor_contiene("VECTOR ERROR CORRECTION MODEL", "el VECM trae su informe");
    visor_contiene("Variables: 3  Lag order (levels): 2  Cointegration rank: 1",
                   "p y r del dialogo llegan al VECM");

    dlg_titulo_esperado = "Select VAR Order";
    dlg_lag = 4; dlg_ok = 1;
    espera_dialogo(contesta_parametros);
    clic("Select VAR Order");
    dialogos_contestados("orden VAR");
    estado_es("VAR order selection completed.", "orden del VAR");
    visor_contiene("Maximum lag considered: 4", "el retardo maximo del dialogo");
    visor_contiene("\n4\t", "una fila por retardo hasta 4");
    visor_contiene("Optimal order (BIC): ", "el orden elegido");
}

static void prueba_motor_falla(const char *falso)
{
    GtkWidget *e = drv_path_entry;

    /* "Browse..." con el selector: se elige un motor que falla */
    g_free(fichero_a_elegir);
    fichero_a_elegir = g_strdup(falso);
    espera_dialogo(elige_fichero);
    clic("Browse...");
    dialogos_contestados("Browse");
    {
        /* Se comparan ficheros, no cadenas: en Windows la misma ruta puede
         * venir con / o con \ segun quien la escriba.                     */
        GFile *a = g_file_new_for_path(gtk_entry_get_text(GTK_ENTRY(e)));
        GFile *b = g_file_new_for_path(falso);
        check(g_file_equal(a, b), "Browse pone el motor elegido en la entrada",
              gtk_entry_get_text(GTK_ENTRY(e)));
        g_object_unref(a); g_object_unref(b);
    }
    clic("Run DRVARMA");
    check(strstr(estado(), "success") == NULL && strstr(estado(), "3") != NULL,
          "un motor que sale con 3 no es un exito, y se dice el codigo", estado());

    /* Lo que se escribe en la entrada es el motor que se usa */
    gtk_entry_set_text(GTK_ENTRY(e), "no_existe_drvarma");
    clic("Run DRVARMA");
    estado_es("Executable not found: no_existe_drvarma", "un motor que no existe se dice");

#ifdef G_OS_WIN32
    gtk_entry_set_text(GTK_ENTRY(e), "drvarma.exe");
#else
    gtk_entry_set_text(GTK_ENTRY(e), "drvarma");
#endif
    clic("Run DRVARMA");
    estado_es("DRVARMA finished successfully.", "de vuelta al motor del PATH");
}

static void prueba_pocos_datos(void)
{
    /* Tres observaciones: todo lo que necesita mas tiene que avisar */
    props_n_obs = 3;
    carga("corto.txt", 1, 1);
    estado_es("Data properties set. Ready.", "carga de un fichero corto");

    pagina("Seasonal Adjustment");
    clic("Test Seasonality");
    visor_contiene("y1: insufficient data after differencing.", "test estacional con 3 datos");

    dlg_titulo_esperado = "Select VAR Order";
    dlg_lag = 8; dlg_ok = 1;
    espera_dialogo(contesta_parametros);
    clic("Select VAR Order");
    dialogos_contestados("orden VAR corto");
    estado_es("Not enough observations after preprocessing.", "orden VAR con 3 datos");

    dlg_titulo_esperado = "Johansen Cointegration Test";
    dlg_lag = 2; dlg_det = 1;
    espera_dialogo(contesta_parametros);
    clic("Johansen Test");
    dialogos_contestados("Johansen corto");
    estado_es("Preprocessing failed or insufficient observations.", "Johansen con 3 datos");

    dlg_titulo_esperado = "Estimate VECM";
    dlg_lag = 2; dlg_rank = 1; dlg_det = 1;
    espera_dialogo(contesta_parametros);
    clic("Estimate VECM");
    dialogos_contestados("VECM corto");
    estado_es("Preprocessing failed or insufficient observations.", "VECM con 3 datos");

    /* Log de un valor no positivo: se dice, no se escribe un .inp roto */
    props_n_obs = 0;
    carga("negativo.txt", 1, 1);
    pagina("Seasonal Adjustment");
    clic("Test Seasonality");
    visor_contiene("ERROR: Cannot apply log transform.", "log de un negativo en el test");
}

static gboolean pulsa_quit(gpointer d)
{
    (void)d;
    gtk_button_clicked(GTK_BUTTON(boton(main_window, "Quit")));
    return G_SOURCE_REMOVE;
}

static gboolean quit_no_salio(gpointer d)
{
    (void)d;
    check(0, "Quit cierra el bucle principal", NULL);
    gtk_main_quit();
    return G_SOURCE_REMOVE;
}

/* Copia el fichero de datos del repositorio a la carpeta de trabajo: el GUI
 * escribe el .inp y el motor el .out junto a los datos, y el arbol de fuentes
 * no se toca.                                                            */
static int copia(const char *dir, const char *from, const char *to)
{
    gchar *src = g_build_filename(dir, from, NULL), *c = NULL;
    gsize n = 0;
    int ok = g_file_get_contents(src, &c, &n, NULL);
    if (ok) {
        gchar *dst = en_work(to);
        ok = g_file_set_contents(dst, c, (gssize)n, NULL);
        g_free(dst);
    }
    g_free(src); g_free(c);
    return ok;
}

int main(int argc, char **argv)
{
    GtkApplication *app;
    GError *err = NULL;
    const char *dominios[] = { "Gtk", "Gdk", "GLib", "GLib-GObject", "GLib-GIO", "Pango" };
    gchar *falso;

    if (argc < 3) { fprintf(stderr, "uso: test_drvarma_gui <datos> <motor falso>\n"); return 2; }
    if (!gtk_init_check(&argc, &argv)) {
        printf("no hay servidor grafico: la prueba de drvarma_gui no se corre\n");
        return 0;
    }
    /* El selector de ficheros apuntaria lo que se abre en los recientes del
     * usuario que corre la prueba: no se le ensucia.                    */
    g_object_set(gtk_settings_get_default(), "gtk-recent-files-enabled", FALSE, NULL);
    /* El GUI arranca en locale C (punto decimal en el .inp); se hace igual */
    setlocale(LC_ALL, "C");
    for (guint i = 0; i < G_N_ELEMENTS(dominios); i++)
        g_log_set_handler(dominios[i], G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL, on_log, NULL);
    g_thread_unref(g_thread_new("vigia", vigia, NULL));

    work = g_dir_make_tmp("drvarma_gui_XXXXXX", &err);
    if (!work) { printf("FAIL: no se pudo crear la carpeta de trabajo: %s\n", err->message); return 1; }
    if (!copia(argv[1], "IPC.txt", "IPC.txt")) {
        printf("FAIL: no se pudo leer %s/IPC.txt\n", argv[1]);
        return 1;
    }
    escribe("malo.txt", "1.0 2.0\nuno dos\n3.0 4.0\n");
    escribe("una_fila.txt", "1.0 2.0 3.0\n");
    escribe("corto.txt", "1 2\n3 4\n5 7\n");
    escribe("negativo.txt", "1 2\n-3 4\n5 7\n6 8\n");
    falso = g_canonicalize_filename(argv[2], NULL);

    /* Se trabaja desde la carpeta temporal: asi el motor no puede salir del
     * directorio actual por casualidad, solo del PATH o de lo que se elija. */
    if (g_chdir(work) != 0) { printf("FAIL: no se pudo entrar en %s\n", work); return 1; }

    app = gtk_application_new("org.atsw.drvarma.test", G_APPLICATION_NON_UNIQUE);
    g_application_register(G_APPLICATION(app), NULL, NULL);
    create_ui(app, NULL);
    g_timeout_add(50, contestador, NULL);
    pump(50);

    prueba_sin_datos();
    prueba_carga();
    prueba_run_basico();
    prueba_opciones_modelo();
    prueba_estacionalidad();
    prueba_prediccion();
    prueba_cointegracion();
    prueba_motor_falla(falso);
    prueba_pocos_datos();

    /* Quit sale del bucle principal */
    g_idle_add(pulsa_quit, NULL);
    g_timeout_add_seconds(10, quit_no_salio, NULL);
    gtk_main();

    printf("drvarma_gui: %d comprobaciones, %d fallos\n", checks, fails);
    /* La carpeta de trabajo se borra si todo fue bien; si no, se deja para
     * mirar que escribieron el GUI y el motor.                          */
    g_chdir(g_get_tmp_dir());
    if (fails) printf("(los ficheros quedan en %s)\n", work);
    else {
        GDir *dir = g_dir_open(work, 0, NULL);
        const gchar *n;
        while (dir && (n = g_dir_read_name(dir)) != NULL) borra(n);
        if (dir) g_dir_close(dir);
        g_rmdir(work);
    }
    g_free(falso);
    return fails ? 1 : 0;
}
