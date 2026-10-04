/* test_gui.c -- gtk_fmg conducido desde el codigo, con el motor de verdad.
 *
 * Se arranca el main.c de verdad (tests/main_wrap.c), y con la ventana ya
 * armada se hace lo que haria el usuario: elegir el fichero de datos, tocar
 * los spin, pulsar los botones de las tres pestanas, aceptar los dialogos de
 * opciones. fug corre de verdad. Despues se mira lo que el usuario veria: la
 * barra de estado, los rotulos, los ficheros que quedan en el espacio de
 * trabajo, y LA VENTANA DEL GRAFICO DIBUJADA -- se pinta en una imagen y se
 * cuentan los pixeles con tinta. Una ventana en blanco no es un grafico.
 *
 *   test_gui <dir de trabajo> <ART.inp> <FULL.inp> <libro.xlsx | ->
 *
 * NO SE ENSENA NINGUNA VENTANA. Cinco funciones de GTK se cambian por las de
 * aqui al compilar el GUI (-D, ver run_gui_tests.sh), y por que:
 *
 *   gtk_dialog_run       bloquea esperando una mano. Aqui se apunta lo que
 *                        el dialogo dice y se contesta lo que toque.
 *   gtk_widget_show_all  los dialogos de opciones se abren con esto: se arman
 *   gtk_window_present   sus hijos y se apunta que se abrieron, sin mapearlos.
 *   gtk_show_uri         los visores de fuera (gv, gedit, el del sistema, el
 *   g_spawn_async        Bloc de notas): se apunta QUE se abriria, y nada mas.
 *
 * g_spawn_sync NO se toca: es con lo que se corre fug.
 *
 * Sin servidor grafico no falla: lo dice ("no hay ...") y sale con 0. Cada
 * fallo es una linea que empieza por "FAIL:".
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include "gui.h"
#include "callbacks.h"
#include "fug_run.h"
#include "data_load.h"
#include "preview.h"
#include "proyecto.h"

int gui_main(int argc, char *argv[]);
extern AppWidgets *test_app;

static int fails = 0;
static int checks = 0;

static void check(int ok, const char *what, const char *saw)
{
    checks++;
    if (ok) return;
    fails++;
    printf("FAIL: %s; se vio: \"%s\"\n", what, saw ? saw : "(nada)");
}

static void pump(int ms)
{
    gint64 until = g_get_monotonic_time() + (gint64) ms * 1000;

    do {
        while (gtk_events_pending()) gtk_main_iteration_do(FALSE);
        if (ms > 0) g_usleep(2000);
    } while (g_get_monotonic_time() < until);
}

/* ---------------------------------------------------------------------- */
/* Lo que el usuario veria: los registros                                 */
/* ---------------------------------------------------------------------- */

static const char *g_work, *g_art, *g_full, *g_xlsx;

static GPtrArray *dialogs;      /* lo que dijo cada gtk_dialog_run        */
static GPtrArray *statuses;     /* cada texto que entro en la barra       */
static GPtrArray *externals;    /* lo que se habria abierto fuera         */
static GPtrArray *presented;    /* ventanas de grafico presentadas        */
static GPtrArray *shown;        /* dialogos de opciones abiertos          */

static gint responses[8];       /* lo que se contestara a los dialogos    */
static int  nresponses = 0;

static void answer_next(gint response)
{
    if (nresponses < (int) G_N_ELEMENTS(responses))
        responses[nresponses++] = response;
}

static gboolean log_has(GPtrArray *log, guint from, const char *what)
{
    guint i;

    for (i = from; i < log->len; i++)
        if (strstr(g_ptr_array_index(log, i), what) != NULL)
            return TRUE;
    return FALSE;
}

static const char *log_last(GPtrArray *log, guint from)
{
    return log->len > from ? g_ptr_array_index(log, log->len - 1) : NULL;
}

/* Todo lo que se apunto desde from, en una linea, para decirlo al fallar. */
static const char *log_since(GPtrArray *log, guint from)
{
    static GString *s = NULL;
    guint i;

    if (s == NULL) s = g_string_new(NULL);
    g_string_truncate(s, 0);
    for (i = from; i < log->len; i++) {
        if (s->len) g_string_append(s, " | ");
        g_string_append(s, g_ptr_array_index(log, i));
    }
    g_strdelimit(s->str, "\n", ' ');
    return s->str;
}

static void on_text_pushed(GtkStatusbar *bar, guint ctx, gchar *text, gpointer data)
{
    (void) bar; (void) ctx; (void) data;
    g_ptr_array_add(statuses, g_strdup(text ? text : ""));
}

/* ---------------------------------------------------------------------- */
/* Las cinco funciones de GTK que el GUI llama con estos nombres          */
/* ---------------------------------------------------------------------- */

gint test_dialog_run(GtkDialog *dialog)
{
    gchar *text = NULL, *secondary = NULL;
    gint response;

    if (GTK_IS_MESSAGE_DIALOG(dialog))
        g_object_get(dialog, "text", &text, "secondary-text", &secondary, NULL);
    g_ptr_array_add(dialogs, g_strdup_printf("%s\n%s",
                    text ? text : gtk_window_get_title(GTK_WINDOW(dialog)),
                    secondary ? secondary : ""));
    g_free(text);
    g_free(secondary);

    if (nresponses > 0) {
        response = responses[0];
        memmove(responses, responses + 1, (size_t) --nresponses * sizeof responses[0]);
    } else {
        response = GTK_IS_MESSAGE_DIALOG(dialog) ? GTK_RESPONSE_CLOSE : GTK_RESPONSE_CANCEL;
    }
    return response;
}

void test_show_all(GtkWidget *widget)
{
    /* Una ventana se arma por dentro -- hace falta para que sus botones y
     * sus cuadernos funcionen -- pero ella no se mapea.                  */
    if (GTK_IS_WINDOW(widget)) {
        GtkWidget *child = gtk_bin_get_child(GTK_BIN(widget));

        if (child) gtk_widget_show_all(child);
        if (g_ptr_array_find(shown, widget, NULL) == FALSE)
            g_ptr_array_add(shown, widget);
    } else {
        gtk_widget_show_all(widget);
    }
}

void test_window_present(GtkWindow *window)
{
    const gchar *role = gtk_window_get_role(window);

    if (role && strcmp(role, "atsw-graph") == 0)
        g_ptr_array_add(presented, g_strdup(gtk_window_get_title(window)));
    else if (g_ptr_array_find(shown, window, NULL) == FALSE)
        g_ptr_array_add(shown, window);
}

gboolean test_show_uri(GdkScreen *screen, const gchar *uri, guint32 when, GError **error)
{
    gchar *path = g_filename_from_uri(uri, NULL, NULL);

    (void) screen; (void) when; (void) error;
    g_ptr_array_add(externals, path ? path : g_strdup(uri));
    return TRUE;
}

gboolean test_spawn_async(const gchar *dir, gchar **argv, gchar **envp, GSpawnFlags flags,
                          GSpawnChildSetupFunc setup, gpointer data, GPid *pid, GError **error)
{
    (void) dir; (void) envp; (void) flags; (void) setup; (void) data; (void) pid; (void) error;
    g_ptr_array_add(externals, g_strdup(argv && argv[0] && argv[1] ? argv[1] : "?"));
    return TRUE;
}

/* ---------------------------------------------------------------------- */
/* Ayudas para mirar                                                      */
/* ---------------------------------------------------------------------- */

static gboolean was_shown(GtkWidget *window)
{
    return g_ptr_array_find(shown, window, NULL);
}

static void forget_shown(void)
{
    g_ptr_array_set_size(shown, 0);
}

/* El boton de un dialogo por su rotulo ("_OK", "_Cancel", "_Help").      */
static GtkWidget *find_button(GtkWidget *root, const char *label)
{
    GList *kids, *l;
    GtkWidget *found = NULL;

    if (GTK_IS_BUTTON(root) && !GTK_IS_TOGGLE_BUTTON(root)) {
        const gchar *t = gtk_button_get_label(GTK_BUTTON(root));

        if (t && strcmp(t, label) == 0) return root;
    }
    if (!GTK_IS_CONTAINER(root)) return NULL;
    kids = gtk_container_get_children(GTK_CONTAINER(root));
    for (l = kids; l && !found; l = l->next)
        found = find_button(l->data, label);
    g_list_free(kids);
    return found;
}

static GtkWidget *find_type(GtkWidget *root, GType type)
{
    GList *kids, *l;
    GtkWidget *found = NULL;

    if (G_TYPE_CHECK_INSTANCE_TYPE(root, type)) return root;
    if (!GTK_IS_CONTAINER(root)) return NULL;
    kids = gtk_container_get_children(GTK_CONTAINER(root));
    for (l = kids; l && !found; l = l->next)
        found = find_type(l->data, type);
    g_list_free(kids);
    return found;
}

static void click_in(GtkWidget *dialog, const char *label)
{
    GtkWidget *b = find_button(dialog, label);

    check(b != NULL, "el dialogo tiene su boton", label);
    if (b) gtk_button_clicked(GTK_BUTTON(b));
    pump(0);
}

static void click(GtkWidget *button)
{
    gtk_button_clicked(GTK_BUTTON(button));
    pump(0);
}

static int spin(GtkWidget *w)
{
    return gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w));
}

static void set_spin(GtkWidget *w, double v)
{
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), v);
}

static void set_toggle(GtkWidget *w, gboolean on)
{
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), on);
}

/* Elegir el fichero de datos COMO EL USUARIO: el chooser y su "file-set". */
static void choose_data(AppWidgets *app, const char *path)
{
    gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(app->data_filechooserbutton), path);
    pump(50);
    g_signal_emit_by_name(app->data_filechooserbutton, "file-set");
    pump(0);
}

/* El espacio de trabajo es el directorio que tiene ESE fichero. Comparar
 * las rutas como texto falla por nada (/private/tmp en macOS, las barras
 * en Windows); preguntar si el fichero esta alli no.                     */
static gboolean workspace_has(AppWidgets *app, const char *file)
{
    gchar *ws = get_workspace(app), *p;
    gboolean ok;

    if (ws == NULL) return FALSE;
    p = g_build_filename(ws, file, NULL);
    ok = g_file_test(p, G_FILE_TEST_EXISTS);
    g_free(p);
    g_free(ws);
    return ok;
}

static gchar *ws_path(AppWidgets *app, const char *file)
{
    gchar *ws = get_workspace(app), *p = g_build_filename(ws ? ws : ".", file, NULL);

    g_free(ws);
    return p;
}

static void remove_ws(AppWidgets *app, const char *file)
{
    gchar *p = ws_path(app, file);

    g_remove(p);
    g_free(p);
}

static gchar *slurp(const char *path)
{
    gchar *s = NULL;

    g_file_get_contents(path, &s, NULL, NULL);
    return s;
}

/* LA VENTANA DEL GRAFICO, PINTADA. Se busca por su titulo (el nombre del
 * fichero), se le da tamano sin mapearla y su area de dibujo se pinta en una
 * imagen. Devuelve los pixeles con tinta: oscuros, que no son ni el fondo
 * gris (0.62), ni la sombra (0.40), ni el papel blanco. -1 si no hay ventana. */
static long graph_ink(const char *title, const char *png)
{
    GList *tops = gtk_window_list_toplevels(), *l;
    GtkWidget *win = NULL, *area;
    GtkAllocation a = { 0, 0, 900, 700 };
    GtkRequisition req;
    cairo_surface_t *img;
    cairo_t *cr;
    unsigned char *px;
    long ink = 0, paper = 0;
    int x, y, w, h, stride;

    for (l = tops; l; l = l->next) {
        const gchar *role = gtk_window_get_role(l->data);
        const gchar *t = gtk_window_get_title(l->data);

        if (role && strcmp(role, "atsw-graph") == 0 && t && strcmp(t, title) == 0)
            win = l->data;
    }
    g_list_free(tops);
    if (win == NULL) return -1;
    area = find_type(win, GTK_TYPE_DRAWING_AREA);
    if (area == NULL) return -1;

    gtk_widget_realize(win);
    gtk_widget_get_preferred_size(win, NULL, &req);
    gtk_widget_size_allocate(win, &a);
    gtk_widget_realize(area);
    pump(0);

    w = gtk_widget_get_allocated_width(area);
    h = gtk_widget_get_allocated_height(area);
    if (w < 50 || h < 50) return 0;
    img = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
    cr = cairo_create(img);
    cairo_set_source_rgb(cr, 0, 0, 0);       /* lo que no se pinte, negro */
    cairo_paint(cr);
    /* La ventana no esta mapeada (no se ensena), y gtk_widget_draw no pinta
     * lo que no se ve. La senal "draw" es lo que GTK emite al exponer: es
     * el mismo manejador de la ventana del grafico, con su cache y todo. */
    {
    gboolean done = FALSE;

    g_signal_emit_by_name(area, "draw", cr, &done);
    }
    cairo_destroy(cr);
    cairo_surface_flush(img);

    px = cairo_image_surface_get_data(img);
    stride = cairo_image_surface_get_stride(img);
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            guint32 p = *(guint32 *) (px + y * stride + 4 * x);
            int r = (p >> 16) & 0xff, g = (p >> 8) & 0xff, b = p & 0xff;

            if (r > 250 && g > 250 && b > 250) paper++;
            else if (r < 90 && g < 90 && b < 90) ink++;
            else if (abs(r - g) > 40 || abs(g - b) > 40) ink++;   /* color */
        }
    if (png) cairo_surface_write_to_png(img, png);
    cairo_surface_destroy(img);
    /* Sin papel no hay pagina: todo negro es que no se pinto nada, y la
     * tinta no puede ser media pagina.                                   */
    if (paper < (long) w * h / 10 || ink > (long) w * h / 2) return 0;
    return ink;
}

/* Un boton que corre fug y abre un grafico: el fichero tiene que aparecer
 * en el espacio de trabajo, la ventana del grafico presentarse con ese
 * titulo, y lo pintado tener tinta.                                      */
static void expect_graph(AppWidgets *app, guint pmark, guint smark, const char *file,
                         const char *args)
{
    gchar *p = ws_path(app, file), *what;
    long ink;

    what = g_strdup_printf("fug dejo %s", file);
    check(g_file_test(p, G_FILE_TEST_EXISTS), what, log_since(statuses, smark));
    g_free(what);
    what = g_strdup_printf("la barra dice que fug corrio con \"%s\"", args);
    check(log_has(statuses, smark, args) && log_has(statuses, smark, "Done: "), what,
          log_since(statuses, smark));
    g_free(what);
    what = g_strdup_printf("la ventana del grafico se abre con %s", file);
    check(log_has(presented, pmark, file), what, log_since(presented, pmark));
    g_free(what);
    /* FMG_TEST_PNG=1 deja lo pintado en <trabajo>/<fichero>.png, para
     * mirarlo cuando esto falle en una maquina que no es la de uno.      */
    if (g_getenv("FMG_TEST_PNG")) {
        gchar *base = g_strconcat(file, ".png", NULL), *png = g_build_filename(g_work, base, NULL);

        ink = graph_ink(file, png);
        g_free(png); g_free(base);
    } else {
        ink = graph_ink(file, NULL);
    }
    printf("grafico %s: %ld pixeles con tinta\n", file, ink);
    what = g_strdup_printf("%s esta DIBUJADO en la ventana (pixeles con tinta: %ld)", file, ink);
    check(ink > 300, what, file);
    g_free(what);
    what = g_strdup_printf("Showing %s", file);
    check(log_has(statuses, smark, what), "y la barra dice que lo ensena", log_since(statuses, smark));
    g_free(what);
    g_free(p);
}

/* ---------------------------------------------------------------------- */
/* Las escenas                                                            */
/* ---------------------------------------------------------------------- */

static gchar *g_self;           /* este programa: hace de motor que falla */
static int scene = 0;
enum { SCENE_MAIN = 1, SCENE_PROYECTO, SCENE_ABRIR };

static void write_file(const char *path, const char *text)
{
    if (!g_file_set_contents(path, text, -1, NULL))
        check(0, "se pudo escribir el fichero de prueba", path);
}

static void scene_main(AppWidgets *app)
{
    guint d, s, p, e;
    gchar *path, *before, *after, *t;
    const char *lbl;

    /* --- lo que se ve al arrancar, y la frecuencia ------------------- */
    check(strcmp(gtk_window_get_title(GTK_WINDOW(app->window)), "FUG: Load and Save Data") == 0,
          "el titulo de la ventana", gtk_window_get_title(GTK_WINDOW(app->window)));
    lbl = gtk_label_get_text(GTK_LABEL(app->season_label));
    check(strcmp(lbl, "Displacement:") == 0, "anual: el rotulo dice Displacement", lbl);
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->freq_data_combobox), 1);
    lbl = gtk_label_get_text(GTK_LABEL(app->season_label));
    check(strcmp(lbl, "Starting Quarter:") == 0, "trimestral: Starting Quarter", lbl);
    set_spin(app->first_period_spinbutton, 9);
    check(spin(app->first_period_spinbutton) == 4, "trimestral: el trimestre no pasa de 4", NULL);
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->freq_data_combobox), 2);
    lbl = gtk_label_get_text(GTK_LABEL(app->season_label));
    check(strcmp(lbl, "Starting Month:") == 0, "mensual: Starting Month", lbl);
    lbl = gtk_label_get_text(GTK_LABEL(app->year_label));
    check(strcmp(lbl, "Starting Year:") == 0, "mensual: Starting Year", lbl);
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->freq_data_combobox), 0);

    /* --- el nombre de entrada sale del de la serie, sin espacios ----- */
    gtk_entry_set_text(GTK_ENTRY(app->series_name_entry), "Mi serie-1");
    lbl = gtk_entry_get_text(GTK_ENTRY(app->path_data_entry));
    check(strcmp(lbl, "Miserie1") == 0, "el nombre de entrada es el de la serie sin signos", lbl);

    /* --- Refresh lo deja todo como al principio ---------------------- */
    s = statuses->len;
    set_spin(app->nrdiff_spinbutton, 2);
    click(app->refresh_button);
    check(*gtk_entry_get_text(GTK_ENTRY(app->series_name_entry)) == '\0', "Refresh vacia el nombre",
          gtk_entry_get_text(GTK_ENTRY(app->series_name_entry)));
    check(spin(app->nrdiff_spinbutton) == 0, "Refresh pone las diferencias a 0", NULL);
    check(log_has(statuses, s, "Cleared"), "Refresh lo dice en la barra", log_since(statuses, s));

    /* --- sin datos: se dice, no se cae ------------------------------- */
    d = dialogs->len;
    click(app->save_button);
    check(log_has(dialogs, d, "Only 1 observations"), "Save sin datos avisa", log_since(dialogs, d));
    set_spin(app->n_load_spinbutton, 50);
    d = dialogs->len;
    click(app->save_button);
    check(log_has(dialogs, d, "No data file selected"), "Save sin fichero avisa", log_since(dialogs, d));

    /* --- ficheros malos ---------------------------------------------- */
    path = g_build_filename(g_work, "basura.txt", NULL);
    write_file(path, "hola\nmundo\n");
    d = dialogs->len;
    choose_data(app, path);
    check(log_has(dialogs, d, "Error loading data"), "un texto sin numeros se dice", log_since(dialogs, d));
    g_free(path);
    path = g_build_filename(g_work, "roto.inp", NULL);
    write_file(path, "esto no es un .inp\n");
    d = dialogs->len;
    choose_data(app, path);
    check(log_has(dialogs, d, "Can not read the input file"), "un .inp roto se dice",
          log_since(dialogs, d));
    g_free(path);

    /* --- el .inp de verdad ------------------------------------------- */
    s = statuses->len;
    choose_data(app, g_art);
    lbl = gtk_entry_get_text(GTK_ENTRY(app->series_name_entry));
    check(strcmp(lbl, "IPCM") == 0, "el nombre de la serie sale del .inp", lbl);
    check(strcmp(gtk_entry_get_text(GTK_ENTRY(app->path_data_entry)), "IPCM") == 0,
          "y el nombre de entrada tambien", gtk_entry_get_text(GTK_ENTRY(app->path_data_entry)));
    check(gtk_combo_box_get_active(GTK_COMBO_BOX(app->freq_data_combobox)) == 2,
          "la frecuencia del .inp (mensual) llega al combo", NULL);
    check(spin(app->n_load_spinbutton) == 216, "las 216 observaciones", NULL);
    check(spin(app->first_year_spinbutton) == 2002 && spin(app->first_period_spinbutton) == 1,
          "la fecha de inicio 1/2002", NULL);
    check(log_has(statuses, s, "216 observations"), "la barra dice lo cargado", log_since(statuses, s));
    check(workspace_has(app, "ART.inp"), "el espacio de trabajo pasa a ser el de los datos", NULL);

    /* --- Output / Active sin haber corrido fug: se dice -------------- */
    d = dialogs->len; s = statuses->len;
    remove_ws(app, "IPCM_fug.out");
    forget_shown();
    click(app->see_output_button);
    check(log_has(statuses, s, "Saved "), "Output escribe primero el .inp", log_since(statuses, s));
    check(was_shown(app->output_dialog), "Output abre su dialogo", NULL);
    check(!gtk_widget_get_sensitive(app->lags_output_spinbutton), "con Active, los retardos no se tocan",
          NULL);
    click_in(app->output_dialog, "_OK");
    check(log_has(dialogs, d, "run fug first"), "Active sin .out dice que hay que correr fug",
          log_since(dialogs, d));
    path = ws_path(app, "IPCM.inp");
    t = slurp(path);
    check(t && strstr(t, "IPCM") && strstr(t, "216"), "el .inp escrito lleva la serie", path);
    g_free(t);
    g_free(path);

    s = statuses->len;
    click(app->save_button);
    check(log_has(statuses, s, "Using "), "un .inp con la misma serie se deja como esta",
          log_since(statuses, s));

    /* --- Input: el .inp, en el visor de fuera ------------------------ */
    e = externals->len;
    click(app->see_input_button);
    check(log_has(externals, e, "IPCM.inp"), "Input abre el .inp con un visor", log_since(externals, e));

    /* --- Help: dice que motor se usa --------------------------------- */
    d = dialogs->len;
    click(app->help_button);
    check(log_has(dialogs, d, "FUG engine:") && !log_has(dialogs, d, "NOT FOUND"),
          "Help dice donde esta fug", log_since(dialogs, d));

    /* --- Histogram ---------------------------------------------------- */
    p = presented->len; s = statuses->len;
    remove_ws(app, "hist_d0IPCM.eps");
    click(app->hist_button);
    expect_graph(app, p, s, "hist_d0IPCM.eps", "fug IPCM -d");
    {
    gchar *eps = ws_path(app, "hist_d0IPCM.eps");
    gchar *png = g_build_filename(g_work, "hist.png", NULL);
    GError *err = NULL;

    check(preview_save_as(eps, png, &err) && g_file_test(png, G_FILE_TEST_EXISTS),
          "Save As de la ventana del grafico escribe el PNG", err ? err->message : png);
    g_clear_error(&err);
    g_free(png); g_free(eps);
    }

    /* --- Ts Data Plot: por defecto, sin Acf, y con opciones ----------- */
    forget_shown();
    click(app->plot_button);
    check(was_shown(app->stand_plot_dialog), "Ts Data Plot abre su dialogo", NULL);
    check(!gtk_widget_get_sensitive(app->options_plot_notebook), "con Default, las opciones no se tocan",
          NULL);
    check(spin(app->lags_acf_plot_spinbutton) == default_lags(216, 12), "los retardos por defecto (39)",
          NULL);
    p = presented->len; s = statuses->len;
    remove_ws(app, "d0IPCM.eps");
    click_in(app->stand_plot_dialog, "_OK");
    expect_graph(app, p, s, "d0IPCM.eps", "fug IPCM -c");

    click(app->plot_button);
    set_toggle(app->default_plot_checkbutton, FALSE);
    check(gtk_widget_get_sensitive(app->options_plot_notebook), "sin Default, las opciones se tocan", NULL);
    set_toggle(app->with_acf_plot_checkbutton, TRUE);
    p = presented->len; s = statuses->len;
    click_in(app->stand_plot_dialog, "_OK");
    expect_graph(app, p, s, "d0IPCM.eps", "fug IPCM -a");

    click(app->plot_button);
    set_toggle(app->with_acf_plot_checkbutton, FALSE);
    set_spin(app->lags_acf_plot_spinbutton, 12);
    set_spin(app->nparma_acf_plot_spinbutton, 2);
    set_spin(app->cbands_acf_plot_spinbutton, 0.5);
    p = presented->len; s = statuses->len;
    click_in(app->stand_plot_dialog, "_OK");
    expect_graph(app, p, s, "d0IPCM.eps", "fug IPCM -c -l 12 -g 2 -f 0.5");
    set_toggle(app->default_plot_checkbutton, TRUE);

    /* Cancel no corre nada */
    click(app->plot_button);
    s = statuses->len;
    click_in(app->stand_plot_dialog, "_Cancel");
    check(!log_has(statuses, s, "Running"), "Cancel no corre fug", log_since(statuses, s));

    /* --- Acf/Pacf ------------------------------------------------------ */
    forget_shown();
    click(app->acf_button);
    check(was_shown(app->acf_dialog), "Acf/Pacf abre su dialogo", NULL);
    p = presented->len; s = statuses->len;
    remove_ws(app, "acf_d0IPCM.eps");
    click_in(app->acf_dialog, "_OK");
    expect_graph(app, p, s, "acf_d0IPCM.eps", "fug IPCM -b");
    click(app->acf_button);
    set_toggle(app->default_acf_checkbutton, FALSE);
    check(gtk_widget_get_sensitive(app->options_acf_notebook), "Acf sin Default: opciones activas", NULL);
    set_spin(app->lags_acf_spinbutton, 20);
    set_spin(app->nparma_acf_spinbutton, 1);
    set_spin(app->cbands_acf_spinbutton, 0.5);
    p = presented->len; s = statuses->len;
    click_in(app->acf_dialog, "_OK");
    expect_graph(app, p, s, "acf_d0IPCM.eps", "fug IPCM -b -l 20 -g 1 -f 0.5");
    set_toggle(app->default_acf_checkbutton, TRUE);
    check(spin(app->lags_acf_spinbutton) == 39, "Default vuelve a poner los retardos", NULL);

    /* --- Mean-Std. Dev. ----------------------------------------------- */
    forget_shown();
    click(app->mdt_button);
    check(was_shown(app->mdt_window), "Mean_Std. Dev. abre su dialogo", NULL);
    check(spin(app->mdt_entry_spinbutton) == 12 && !gtk_widget_get_sensitive(app->mdt_entry_spinbutton),
          "12 por grupo (mensual), sin tocar", NULL);
    d = dialogs->len;
    click_in(app->mdt_window, "_Help");
    check(log_has(dialogs, d, "Mean - Standard Deviation"), "su Help explica el grafico",
          log_since(dialogs, d));
    p = presented->len; s = statuses->len;
    remove_ws(app, "m_dt_d0IPCM.eps");
    click_in(app->mdt_window, "_OK");
    expect_graph(app, p, s, "m_dt_d0IPCM.eps", "fug IPCM -e -m 12");
    click(app->mdt_button);
    s = statuses->len;
    click_in(app->mdt_window, "_Cancel");
    check(!log_has(statuses, s, "Running"), "Cancel no corre fug", log_since(statuses, s));

    /* --- ID Options Set: el PDF de varias paginas ---------------------- */
    forget_shown();
    click(app->iden_button);
    check(was_shown(app->iden_dialog), "ID Options Set abre su dialogo", NULL);
    check(spin(app->nrdiff_iden_spinbutton) == 2 && spin(app->nadiff_iden_spinbutton) == 1 &&
          spin(app->nog_iden_spinbutton) == 12, "por defecto d<=2, D<=1, 12 por grupo", NULL);
    check(gtk_widget_get_sensitive(app->nadiff_iden_spinbutton), "mensual: D se puede tocar", NULL);
    p = presented->len; s = statuses->len;
    remove_ws(app, "IPCM_fug.pdf");
    click_in(app->iden_dialog, "_OK");
    expect_graph(app, p, s, "IPCM_fug.pdf", "fug IPCM set 2 1 -c -h -e -m 12");
    path = ws_path(app, "IPCM_fug.pdf");
    t = g_strdup_printf("%u paginas", preview_n_pages(path));
    check(preview_n_pages(path) > 1, "el PDF del conjunto tiene varias paginas", t);
    g_free(t); g_free(path);

    click(app->iden_button);
    set_toggle(app->default_iden_checkbutton, FALSE);
    set_toggle(app->with_mdt_iden_checkbutton, TRUE);
    check(!gtk_widget_get_sensitive(app->nog_iden_spinbutton), "sin el grafico media-dt, no hay grupos",
          NULL);
    set_toggle(app->no_level_iden_checkbutton, TRUE);
    set_spin(app->nrdiff_iden_spinbutton, 1);
    p = presented->len; s = statuses->len;
    click_in(app->iden_dialog, "_OK");
    t = (gchar *) log_last(statuses, s);
    check(log_has(statuses, s, "Done: fug IPCM set 1 1 -c -l 39") && !log_has(statuses, s, " -e") &&
          !log_has(statuses, s, " -h"), "las opciones del conjunto llegan a fug", log_since(statuses, s));
    check(log_has(presented, p, "IPCM_fug.pdf"), "y se ensena el PDF", t);
    set_toggle(app->default_iden_checkbutton, TRUE);

    /* --- Output: Active, One y Set ------------------------------------ */
    e = externals->len;
    click(app->see_output_button);
    click_in(app->output_dialog, "_OK");
    check(log_has(externals, e, "IPCM_fug.out"), "Active abre el .out de la ultima corrida",
          log_since(externals, e));
    click(app->see_output_button);
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->option_output_dialog_combobox), 1);
    check(gtk_widget_get_sensitive(app->lags_output_spinbutton) &&
          gtk_widget_get_sensitive(app->nog_output_spinbutton) &&
          !gtk_widget_get_sensitive(app->nrdiff_output_spinbutton), "One: retardos y grupos, sin d", NULL);
    e = externals->len; s = statuses->len;
    remove_ws(app, "IPCM_fug.out");
    click_in(app->output_dialog, "_OK");
    check(log_has(statuses, s, "Done: fug IPCM -m 12"), "One corre fug", log_since(statuses, s));
    check(log_has(externals, e, "IPCM_fug.out") && workspace_has(app, "IPCM_fug.out"),
          "y abre el .out que hizo", log_since(externals, e));
    click(app->see_output_button);
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->option_output_dialog_combobox), 2);
    check(gtk_widget_get_sensitive(app->nrdiff_output_spinbutton) &&
          gtk_widget_get_sensitive(app->nadiff_output_spinbutton), "Set: d y D se tocan", NULL);
    set_spin(app->nrdiff_output_spinbutton, 2);
    set_spin(app->nadiff_output_spinbutton, 1);
    set_spin(app->lags_output_spinbutton, 24);
    s = statuses->len;
    click_in(app->output_dialog, "_OK");
    check(log_has(statuses, s, "Done: fug IPCM set 2 1 -h -l 24 -m 12"), "Set corre el barrido",
          log_since(statuses, s));
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->option_output_dialog_combobox), 0);

    /* --- la transformacion de la ventana manda en el .inp ------------- */
    set_spin(app->box_cox_lambda_spinbutton, 0);
    set_spin(app->nrdiff_spinbutton, 1);
    p = presented->len; s = statuses->len;
    click(app->hist_button);
    check(log_has(statuses, s, "Saved "), "otra transformacion reescribe el .inp", log_since(statuses, s));
    expect_graph(app, p, s, "hist_d1lnIPCM.eps", "fug IPCM -d");
    set_spin(app->box_cox_lambda_spinbutton, 0.5);
    p = presented->len; s = statuses->len;
    click(app->hist_button);
    expect_graph(app, p, s, "hist_d1l0.5IPCM.eps", "fug IPCM -d");
    set_spin(app->box_cox_lambda_spinbutton, 1);
    set_spin(app->nrdiff_spinbutton, 0);

    /* --- demasiadas diferencias -------------------------------------- */
    set_spin(app->nrdiff_spinbutton, 100);
    set_spin(app->nadiff_spinbutton, 100);
    d = dialogs->len; s = statuses->len;
    click(app->hist_button);
    check(log_has(dialogs, d, "Too few observations"), "demasiadas diferencias se dicen",
          log_since(dialogs, d));
    check(!log_has(statuses, s, "Running"), "y fug no corre", log_since(statuses, s));
    set_spin(app->nrdiff_spinbutton, 0);
    set_spin(app->nadiff_spinbutton, 0);

    /* --- nombres de entrada malos ------------------------------------ */
    gtk_entry_set_text(GTK_ENTRY(app->path_data_entry), "");
    d = dialogs->len;
    click(app->save_button);
    check(log_has(dialogs, d, "Input name is empty"), "nombre de entrada vacio", log_since(dialogs, d));
    gtk_entry_set_text(GTK_ENTRY(app->path_data_entry), "a b");
    d = dialogs->len;
    click(app->save_button);
    check(log_has(dialogs, d, "can not contain spaces"), "nombre de entrada con espacios",
          log_since(dialogs, d));

    /* --- un .inp con modelo de fue: ni se pisa ni se ignora ---------- */
    gtk_entry_set_text(GTK_ENTRY(app->path_data_entry), "FULL");
    path = g_build_filename(g_work, "FULL.inp", NULL);
    before = slurp(path);
    d = dialogs->len;
    click(app->save_button);
    check(log_has(dialogs, d, "otra transformaci"), "el modelo de fue con otra transformacion se dice",
          log_since(dialogs, d));
    set_spin(app->box_cox_lambda_spinbutton, 0);
    set_spin(app->nrdiff_spinbutton, 1);
    set_spin(app->nadiff_spinbutton, 1);
    s = statuses->len;
    click(app->save_button);
    check(log_has(statuses, s, "Using "), "con la transformacion del modelo, se usa tal cual",
          log_since(statuses, s));
    after = slurp(path);
    check(before && after && strcmp(before, after) == 0, "y el .inp con modelo queda intacto", path);
    g_free(after);
    set_spin(app->box_cox_lambda_spinbutton, 1);
    set_spin(app->nrdiff_spinbutton, 0);
    set_spin(app->nadiff_spinbutton, 0);

    /* --- un .inp ilegible: se pregunta ------------------------------- */
    {
    gchar *x = g_build_filename(g_work, "XX.inp", NULL);
    gchar *was;

    write_file(x, "basura\n");
    gtk_entry_set_text(GTK_ENTRY(app->path_data_entry), "XX");
    d = dialogs->len; s = statuses->len;
    answer_next(GTK_RESPONSE_CANCEL);
    click(app->save_button);
    was = slurp(x);
    check(log_has(dialogs, d, "can not be read"), "un .inp ilegible pregunta antes de pisarlo",
          log_since(dialogs, d));
    check(log_has(statuses, s, "was not replaced") && was && strcmp(was, "basura\n") == 0,
          "Cancel lo deja como estaba", log_since(statuses, s));
    g_free(was);
    s = statuses->len;
    answer_next(GTK_RESPONSE_ACCEPT);
    click(app->save_button);
    was = slurp(x);
    check(log_has(statuses, s, "Saved ") && was && strstr(was, "IPCM"), "Replace lo reescribe",
          log_since(statuses, s));
    g_free(was);
    g_free(x);
    }
    gtk_entry_set_text(GTK_ENTRY(app->path_data_entry), "IPCM");

    /* --- el motor no esta ------------------------------------------- */
    t = g_build_filename(g_work, "no", "existe", "fug", NULL);
    g_setenv("FUG", t, TRUE);
    g_free(t);
    d = dialogs->len; s = statuses->len; p = presented->len;
    click(app->hist_button);
    check(log_has(dialogs, d, "Can not run the FUG engine"), "sin motor se dice", log_since(dialogs, d));
    check(log_has(statuses, s, "Error running fug"), "y la barra lo dice", log_since(statuses, s));
    check(presented->len == p, "y no se abre ningun grafico", log_since(presented, p));
    check(gtk_widget_get_sensitive(app->window), "y la ventana vuelve a responder", NULL);

    /* --- el motor falla: se ensena lo que dijo ------------------------ */
    g_setenv("FUG", g_self, TRUE);
    g_setenv("FUG_TEST_FAKE", "1", TRUE);
    d = dialogs->len; s = statuses->len; p = presented->len;
    click(app->hist_button);
    g_unsetenv("FUG_TEST_FAKE");
    g_unsetenv("FUG");
    check(log_has(dialogs, d, "fug failed") && log_has(dialogs, d, "fallo simulado"),
          "si fug falla, se dice y se ensena lo que escribio", log_since(dialogs, d));
    check(log_has(statuses, s, "fug failed"), "y la barra lo dice", log_since(statuses, s));
    check(presented->len == p, "y no se abre ningun grafico", log_since(presented, p));

    /* --- datos en texto: un valor por linea --------------------------- */
    {
    GString *txt = g_string_new(NULL), *csv = g_string_new("# freq 4\n# start 2/1990\nPIB;OTRA\n");
    gchar *tp = g_build_filename(g_work, "serie.txt", NULL);
    gchar *cp = g_build_filename(g_work, "datos.csv", NULL);
    int i;

    for (i = 0; i < 120; i++) g_string_append_printf(txt, "%.1f\n", Ts.data[i]);
    for (i = 0; i < 60; i++)
        g_string_append_printf(csv, "%d,%d;%d\n", 100 + i % 7 + i, i % 10, 3 * i);
    write_file(tp, txt->str);
    write_file(cp, csv->str);

    click(app->refresh_button);
    s = statuses->len;
    choose_data(app, tp);
    check(spin(app->n_load_spinbutton) == 120, "el texto da 120 valores", log_since(statuses, s));
    lbl = gtk_entry_get_text(GTK_ENTRY(app->series_name_entry));
    check(strcmp(lbl, "serie") == 0, "sin nombre, la serie se llama como el fichero", lbl);
    check(log_has(statuses, s, "Read 120 values"), "la barra dice lo leido", log_since(statuses, s));
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->freq_data_combobox), 2);
    set_spin(app->first_period_spinbutton, 3);
    set_spin(app->first_year_spinbutton, 1990);
    s = statuses->len;
    click(app->save_button);
    check(log_has(statuses, s, "Saved ") && workspace_has(app, "serie.inp"), "se escribe serie.inp",
          log_since(statuses, s));
    check(Ts.freq == 12 && Ts.begtime == 3 && Ts.begyear == 1990 && Ts.nobs == 120,
          "con la frecuencia y la fecha que puso el usuario", NULL);
    set_spin(app->n_load_spinbutton, 100);
    click(app->save_button);
    check(Ts.nobs == 100, "con menos observaciones, se toman las primeras", NULL);
    set_spin(app->n_load_spinbutton, 500);
    d = dialogs->len;
    click(app->save_button);
    check(log_has(dialogs, d, "Not enough observations"), "pedir mas de las que hay se dice",
          log_since(dialogs, d));

    /* grupos demasiado grandes para el grafico media-dt */
    set_spin(app->n_load_spinbutton, 20);
    click(app->mdt_button);
    set_toggle(app->default_mdt_checkbutton, FALSE);
    set_spin(app->mdt_entry_spinbutton, 15);
    d = dialogs->len;
    click_in(app->mdt_window, "_OK");
    check(log_has(dialogs, d, "Too few groups"), "20 observaciones en grupos de 15 se dice",
          log_since(dialogs, d));
    set_toggle(app->default_mdt_checkbutton, TRUE);

    /* --- un CSV de dos columnas, con frecuencia y fecha --------------- */
    click(app->refresh_button);
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->freq_data_combobox), 0);
    choose_data(app, cp);
    check(spin(app->n_load_spinbutton) == 60, "el CSV da 60 filas (no 120: no se aplanan columnas)",
          NULL);
    s = statuses->len;
    click(app->save_button);
    check(log_has(statuses, s, "2 columns in the file: using the first (PIB)"),
          "las columnas que fug descarta se dicen", log_since(statuses, s));
    check(gtk_combo_box_get_active(GTK_COMBO_BOX(app->freq_data_combobox)) == 1 && Ts.freq == 4,
          "la frecuencia sale del fichero (# freq 4)", NULL);
    check(spin(app->first_year_spinbutton) == 1990 && Ts.begtime == 2, "y la fecha (# start 2/1990)",
          NULL);
    check(Ts.data[0] > 99.9 && Ts.data[0] < 100.1, "con la coma decimal leida como tal", NULL);

    /* y otra serie contra el .inp con modelo: se pregunta */
    gtk_entry_set_text(GTK_ENTRY(app->path_data_entry), "FULL");
    d = dialogs->len; s = statuses->len;
    answer_next(GTK_RESPONSE_CANCEL);
    click(app->save_button);
    after = slurp(path);
    check(log_has(dialogs, d, "fue model of another series"), "un modelo de otra serie: se pregunta",
          log_since(dialogs, d));
    check(log_has(statuses, s, "was not replaced") && before && after && strcmp(before, after) == 0,
          "Cancel conserva el modelo", log_since(statuses, s));
    g_free(after);

    g_string_free(txt, TRUE); g_string_free(csv, TRUE);
    g_free(tp); g_free(cp);
    }
    g_free(before);
    g_free(path);

    /* --- un libro .xlsx ------------------------------------------------ */
    if (g_xlsx && strcmp(g_xlsx, "-") != 0) {
        click(app->refresh_button);
        s = statuses->len;
        choose_data(app, g_xlsx);
        check(log_has(statuses, s, "Read ") && spin(app->n_load_spinbutton) >= 10,
              "el .xlsx se lee", log_since(statuses, s));
        s = statuses->len;
        click(app->save_button);
        check(log_has(statuses, s, "Saved "), "y se escribe su .inp", log_since(statuses, s));
        p = presented->len; s = statuses->len;
        click(app->hist_button);
        check(log_has(presented, p, "hist_"), "y fug lo dibuja", log_since(statuses, s));
    }
}

/* --proyecto: el espacio de trabajo es la raiz que declara el manifiesto */
static void scene_proyecto(AppWidgets *app)
{
    check(workspace_has(app, "MARCA"), "--proyecto: el espacio de trabajo es la raiz del proyecto", NULL);
    check(*gtk_entry_get_text(GTK_ENTRY(app->series_name_entry)) == '\0', "y no hay serie cargada", NULL);
}

/* --proyecto y un fichero: lo que manda la madre se carga al arrancar */
static void scene_abrir(AppWidgets *app)
{
    const char *lbl = gtk_entry_get_text(GTK_ENTRY(app->series_name_entry));
    guint p = presented->len, s = statuses->len;

    check(strcmp(lbl, "IPCM") == 0, "el fichero de la orden se carga al arrancar", lbl);
    check(spin(app->n_load_spinbutton) == 216, "con sus 216 observaciones", NULL);
    check(workspace_has(app, "MARCA"), "y el espacio de trabajo sigue en el proyecto", NULL);
    click(app->hist_button);
    check(log_has(presented, p, "hist_d0IPCM.eps") && workspace_has(app, "hist_d0IPCM.eps"),
          "y desde ahi se dibuja como siempre", log_since(statuses, s));
}

static gboolean quit_soon(gpointer data)
{
    click(((AppWidgets *) data)->quit_button);
    return G_SOURCE_REMOVE;
}

static gboolean quit_failed(gpointer data)
{
    (void) data;
    check(0, "Quit tiene que cerrar el bucle de la ventana", NULL);
    gtk_main_quit();
    return G_SOURCE_REMOVE;
}

/* gui_main llega aqui con todo armado, en vez de a gtk_main. */
void test_gtk_main(void)
{
    AppWidgets *app = test_app;
    guint guard;

    g_signal_connect(app->main_statusbar, "text-pushed", G_CALLBACK(on_text_pushed), NULL);
    switch (scene) {
    case SCENE_MAIN:     scene_main(app);     break;
    case SCENE_PROYECTO: scene_proyecto(app); break;
    case SCENE_ABRIR:    scene_abrir(app);    break;
    }

    /* Y al final, Quit: el bucle de verdad tiene que acabar. */
    g_idle_add(quit_soon, app);
    guard = g_timeout_add(10000, quit_failed, NULL);
    gtk_main();
    g_source_remove(guard);
}

int main(int argc, char **argv)
{
    gchar *a, *yaml, *dir, *datos, *t;
    Proyecto *pr;
    PrError pe;
    char *av[5];
    int rc;

    /* HACE DE MOTOR QUE FALLA: run_fug tiene que ensenar lo que dijo. Es
     * este mismo programa porque un guion de sh no se puede lanzar como
     * programa en Windows.                                               */
    if (g_getenv("FUG_TEST_FAKE")) {
        printf("salida simulada\n");
        fprintf(stderr, "fallo simulado del motor\n");
        return 3;
    }
    if (argc < 5) {
        fprintf(stderr, "uso: test_gui <dir> <ART.inp> <FULL.inp> <libro.xlsx|->\n");
        return 2;
    }
    g_work = argv[1]; g_art = argv[2]; g_full = argv[3]; g_xlsx = argv[4];
    g_self = g_path_is_absolute(argv[0]) ? g_strdup(argv[0])
                                         : g_build_filename(g_get_current_dir(), argv[0], NULL);
#ifdef G_OS_WIN32
    if (!g_str_has_suffix(g_self, ".exe")) {
        t = g_strconcat(g_self, ".exe", NULL);
        g_free(g_self);
        g_self = t;
    }
#endif
    (void) g_full;

    if (!gtk_init_check(&argc, &argv)) {
        printf("no hay servidor grafico: la prueba de gtk_fmg no se corre\n");
        return 0;
    }
    dialogs = g_ptr_array_new_with_free_func(g_free);
    statuses = g_ptr_array_new_with_free_func(g_free);
    externals = g_ptr_array_new_with_free_func(g_free);
    presented = g_ptr_array_new_with_free_func(g_free);
    shown = g_ptr_array_new();

    /* --- las opciones de la orden, antes de abrir nada ---------------- */
    av[0] = "gtk_fmg"; av[1] = "--help"; av[2] = NULL;
    rc = gui_main(2, av);
    check(rc == 0 && test_app == NULL, "--help sale con 0 sin abrir la ventana", NULL);
    a = g_build_filename(g_work, "no-existe.yaml", NULL);
    av[1] = "--proyecto"; av[2] = a; av[3] = NULL;
    rc = gui_main(3, av);
    check(rc == 3 && test_app == NULL, "un manifiesto que no esta: se dice y sale con 3", NULL);
    g_free(a);

    /* --- la ventana, de punta a punta --------------------------------- */
    scene = SCENE_MAIN;
    av[1] = NULL;
    gui_main(1, av);
    check(test_app != NULL, "gui_main arma la ventana", NULL);

    /* --- --proyecto, con la raiz en otra carpeta ---------------------- */
    dir = g_build_filename(g_work, "proy", NULL);
    datos = g_build_filename(dir, "datos", NULL);
    g_mkdir_with_parents(datos, 0755);
    t = g_build_filename(datos, "MARCA", NULL);
    write_file(t, "aqui\n");
    g_free(t);
    yaml = g_build_filename(dir, "proyecto.yaml", NULL);
    pr = g_new0(Proyecto, 1);
    pr_nuevo(pr, "prueba", "Prueba de gtk_fmg", "datos");
    check(pr_escribir(pr, yaml, &pe) == 0, "se escribe el manifiesto de prueba", yaml);
    g_free(pr);

    scene = SCENE_PROYECTO;
    test_app = NULL;
    av[1] = "--proyecto"; av[2] = yaml; av[3] = NULL;
    gui_main(3, av);

    /* --- --proyecto y un fichero, como lo manda la madre -------------- */
    {
    gchar *art2 = g_build_filename(datos, "ART.inp", NULL), *s = slurp(g_art);

    write_file(art2, s ? s : "");
    g_free(s);
    scene = SCENE_ABRIR;
    test_app = NULL;
    av[1] = "--proyecto"; av[2] = yaml; av[3] = art2; av[4] = NULL;
    gui_main(4, av);
    g_free(art2);
    }

    printf("%d comprobaciones, %d fallos\n", checks, fails);
    return fails == 0 ? 0 : 1;
}
