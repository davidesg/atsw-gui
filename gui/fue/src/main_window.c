/* main_window.c – builds the main UI */
#include "main_window.h"

/* main.c: la raiz del --proyecto, o NULL. */
const char *fue_raiz_proyecto(void);
#include "fue_globals.h"       /* needed for tree view column constants and structures */
#include "data_handling.h"
#include "model_spec.h"
#include "file_io.h"
#include "deterministic_dialog.h"
#include "operator_dialog.h"
#include "forecast_tab.h"
#include "barra.h"

/* Forward declarations of helper functions for building tabs */
static GtkWidget* create_data_input_tab(FueContext *ctx);
static GtkWidget* create_boxcox_tab(FueContext *ctx);
static GtkWidget* create_deterministic_tab(FueContext *ctx);
static GtkWidget* create_stochastic_tab(FueContext *ctx);
static GtkWidget* create_operator_tab(FueContext *ctx, const char *tv_name,
                                      const char *title, GtkWidget **treeview_ptr,
                                      int op_type);
static GtkWidget* create_fixed_tab(FueContext *ctx, const char *tv_name,
                                   const char *title, GtkWidget **treeview_ptr,
                                   int op_type);
static GtkWidget* create_console_tab(FueContext *ctx);
static void create_intervention_tree_view(GtkTreeView *treeview);
static void create_operator_tree_view(GtkTreeView *treeview);
static void create_fixed_tree_view(GtkTreeView *treeview);
//static void update_model_label(FueContext *ctx);

/* gui/fue/src/analisis.c: la diagnosis y los anomalos de ESTE modelo. */
void fue_on_diagnosis(GtkWidget *w, FueContext *ctx);
void fue_on_anomalos(GtkWidget *w, FueContext *ctx);
void fue_on_ganancia(GtkWidget *w, FueContext *ctx);
void fue_on_identifica(GtkWidget *w, FueContext *ctx);
void fue_on_integracion(GtkWidget *w, FueContext *ctx);
void fue_on_estacionalidad(GtkWidget *w, FueContext *ctx);
void fue_on_factores(GtkWidget *w, FueContext *ctx);
void fue_on_exportar(GtkWidget *w, FueContext *ctx);
void fue_analisis_refresca(FueContext *ctx);
static void on_fixed_treeview_row_activated(GtkTreeView *treeview, GtkTreePath *path,
                                            GtkTreeViewColumn *col, FueContext *ctx);
static void on_operator_treeview_row_activated(GtkTreeView *treeview, GtkTreePath *path,
                                               GtkTreeViewColumn *col, FueContext *ctx) ;


/* LA BARRA: lib/barra (la misma que drtran_gui). ---------------------- */

static gboolean sal(GtkAccelGroup *g, GObject *o, guint k, GdkModifierType m,
                    gpointer ctx) {
    (void) g; (void) o; (void) k; (void) m;
    on_quit(NULL, ctx);
    return TRUE;
}

/* PREVER: lo que hacian los botones de la pestaña, desde el menu. La
 * pestaña sigue siendo donde se ajustan el horizonte y el origen.     */
static void prever_pdf(GtkWidget *w, FueContext *ctx) {
    (void) w;
    gtk_button_clicked(GTK_BUTTON(ctx->forecast_view_pdf_button));
}

static void prever_abrir(GtkWidget *w, FueContext *ctx) {
    (void) w;
    gtk_button_clicked(GTK_BUTTON(ctx->forecast_load_button));
}

static void prever_pestana(GtkWidget *w, FueContext *ctx) {
    (void) w;
    gtk_notebook_set_current_page(GTK_NOTEBOOK(ctx->notebook), ctx->forecast_page);
}

GtkWidget* create_main_window(GtkApplication *app, FueContext *ctx) {
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "FUE – Time Series Modeling");
    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);

    ctx->main_window = window;

    /* Main vertical box - sin espaciado, controlamos con márgenes */
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    /* --- La barra (sin expandir) ---
     *
     * EL CICLO DE TRABAJO, CON TEXTO E ICONOS PROPIOS (gui/fue/src/iconos.c).
     * Antes eran catorce iconos del tema sin texto: tres lupas (el PDF, los
     * anomalos, el identificador), la «i» de informacion para la diagnosis
     * y la flecha de recargar para la ganancia. Ahora nueve botones, cada
     * uno con su nombre, y lo que no cabe va en el menu de tres de ellos:
     * Diagnosis, Contrastes y Prever son botones partidos --el clic hace lo
     * habitual y la flecha abre el resto--. Los nombres son los del menu del
     * boton derecho de la madre: una accion, un nombre en todo ATSW.     */
    GtkWidget *toolbar = gtk_toolbar_new();
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH);
    /* EN LA VENTANA ANTES DE LOS BOTONES: los iconos toman el color del
       texto de la barra, y suelta, sin ventana, el tema no le da el suyo
       (salian casi blancos).                                          */
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
    GtkAccelGroup *atajos = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(window), atajos);

    GtkToolItem *new_btn = at_boton(toolbar, "Nuevo", "nuevo", "document-new",
        "Un modelo nuevo (Ctrl+N)", G_CALLBACK(on_new_file), ctx);
    at_atajo(atajos, GDK_KEY_n, new_btn);
    GtkToolItem *open_btn = at_boton(toolbar, "Abrir", "abrir", "document-open",
        "Abrir unos datos o un modelo, .inp o .pre (Ctrl+O)", G_CALLBACK(on_load_series), ctx);
    at_atajo(atajos, GDK_KEY_o, open_btn);
    GtkToolItem *save_btn = at_boton(toolbar, "Guardar", "guardar", "document-save",
        "Guardar el modelo como .inp (Ctrl+S)", G_CALLBACK(on_save_inp), ctx);
    at_atajo(atajos, GDK_KEY_s, save_btn);
    ctx->btn_save = GTK_WIDGET(save_btn);

    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);

    GtkToolItem *run_btn = at_boton(toolbar, "Estimar", "estimar", "media-playback-start",
        "Estimar el modelo con fue (Ctrl+E)", G_CALLBACK(on_run_fue), ctx);
    at_atajo(atajos, GDK_KEY_e, run_btn);
    ctx->btn_execute = GTK_WIDGET(run_btn);
    GtkToolItem *ver_btn = at_boton(toolbar, "Ver modelo", "ver-modelo", "x-office-document",
        "La página de fue: los residuos con su ACF y su PACF y el modelo escrito "
        "debajo, con sus desviaciones típicas", G_CALLBACK(on_view_output), ctx);
    (void) ver_btn;

    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);

    /* DIAGNOSIS, CONTRASTES Y PREVER, con su menu. Las ventanas son LAS
     * MISMAS que las de la madre --lib/analisis--, no una copia, y se
     * encienden solas cuando hay un .out AL DIA: ver analisis.c.       */
    {
    GtkWidget *m = gtk_menu_new();

    ctx->btn_diagnosis = GTK_WIDGET(at_partido(toolbar, "Diagnosis", "diagnosis",
        "dialog-information", m, G_CALLBACK(fue_on_diagnosis), ctx));
    at_atajo(atajos, GDK_KEY_d, GTK_TOOL_ITEM(ctx->btn_diagnosis));
    ctx->mi_diagnosis = at_entrada(m, "Diagnosis…", "diagnosis", "dialog-information",
                                toolbar, G_CALLBACK(fue_on_diagnosis), ctx);
    ctx->btn_anomalos = at_entrada(m, "Anómalos…", "anomalos", "edit-find",
                                toolbar, G_CALLBACK(fue_on_anomalos), ctx);
    ctx->btn_ganancia = at_entrada(m, "Ganancia de las intervenciones…", "ganancia",
                                "view-refresh", toolbar, G_CALLBACK(fue_on_ganancia), ctx);
    ctx->btn_identifica = at_entrada(m, "Identificar los residuos…", "identificar",
                                  "edit-find-replace", toolbar,
                                  G_CALLBACK(fue_on_identifica), ctx);
    ctx->btn_factores = at_entrada(m, "Factorización del AR…", "factores", NULL,
                                toolbar, G_CALLBACK(fue_on_factores), ctx);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), gtk_separator_menu_item_new());
    ctx->btn_exportar = at_entrada(m, "Exportar la diagnosis…", "exportar", "document-save-as",
                                toolbar, G_CALLBACK(fue_on_exportar), ctx);
    gtk_widget_show_all(m);
    }
    {
    GtkWidget *m = gtk_menu_new();

    ctx->btn_contrastes = GTK_WIDGET(at_partido(toolbar, "Contrastes", "integracion",
        "view-list", m, G_CALLBACK(fue_on_integracion), ctx));
    ctx->btn_integracion = at_entrada(m, "Orden de integración…", "integracion", "view-list",
                                   toolbar, G_CALLBACK(fue_on_integracion), ctx);
    ctx->btn_estacional = at_entrada(m, "Estacionalidad (MEG)…", "estacionalidad",
                                  "x-office-calendar", toolbar,
                                  G_CALLBACK(fue_on_estacionalidad), ctx);
    gtk_widget_show_all(m);
    }
    {
    GtkWidget *m = gtk_menu_new();
    GtkToolItem *p = at_partido(toolbar, "Prever", "prever", "go-jump", m,
                             G_CALLBACK(on_forecast_button_clicked), ctx);

    gtk_widget_set_tooltip_text(GTK_WIDGET(p), "Prever con fuf desde el modelo "
        "estimado: fue -f escribe la entrada de la previsión y fuf la calcula");
    at_entrada(m, "Prever con fuf…", "prever", "go-jump", toolbar,
            G_CALLBACK(on_forecast_button_clicked), ctx);
    at_entrada(m, "Ver la última previsión (PDF)", NULL, NULL, toolbar,
            G_CALLBACK(prever_pdf), ctx);
    at_entrada(m, "Abrir una previsión guardada…", NULL, NULL, toolbar,
            G_CALLBACK(prever_abrir), ctx);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), gtk_separator_menu_item_new());
    at_entrada(m, "Ir a la pestaña de la previsión", NULL, NULL, toolbar,
            G_CALLBACK(prever_pestana), ctx);
    gtk_widget_show_all(m);
    }

    /* SALIR, sin boton: la equis de la ventana y Ctrl+Q. Una equis roja
     * en mitad de la barra, junto a las herramientas, era un riesgo.   */
    {
    GClosure *c = g_cclosure_new(G_CALLBACK(sal), ctx, NULL);

    gtk_accel_group_connect(atajos, GDK_KEY_q, GDK_CONTROL_MASK, 0, c);
    }

    /* --- Model specification notebook --- */
    GtkWidget *model_notebook = gtk_notebook_new();
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(model_notebook), GTK_POS_TOP);
    gtk_widget_set_vexpand(model_notebook, TRUE);

    /* Añadir pestañas */
    GtkWidget *data_input_tab = create_data_input_tab(ctx);
    gtk_notebook_append_page(GTK_NOTEBOOK(model_notebook), data_input_tab, gtk_label_new("Data Input"));

    GtkWidget *boxcox_tab = create_boxcox_tab(ctx);
    gtk_notebook_append_page(GTK_NOTEBOOK(model_notebook), boxcox_tab, gtk_label_new("Box-Cox & Differences"));

    GtkWidget *det_tab = create_deterministic_tab(ctx);
    gtk_notebook_append_page(GTK_NOTEBOOK(model_notebook), det_tab, gtk_label_new("Deterministic Component"));

    GtkWidget *stoch_tab = create_stochastic_tab(ctx);
    gtk_notebook_append_page(GTK_NOTEBOOK(model_notebook), stoch_tab, gtk_label_new("Stochastic Component"));

    GtkWidget *console_tab = create_console_tab(ctx);
    ctx->console_page =
    gtk_notebook_append_page(GTK_NOTEBOOK(model_notebook), console_tab, gtk_label_new("Console"));
    ctx->notebook = model_notebook;

    GtkWidget *forecast_tab = create_forecast_tab(ctx);
    ctx->forecast_page =
    gtk_notebook_append_page(GTK_NOTEBOOK(model_notebook), forecast_tab, gtk_label_new("Forecast"));

    /* --- Paned vertical para separar notebook (arriba) y área de salida (abajo) --- */
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start(GTK_BOX(vbox), paned, TRUE, TRUE, 0);
    gtk_widget_set_vexpand(paned, TRUE);

    /* Parte superior: notebook (puede crecer, no se encoge) */
    gtk_paned_pack1(GTK_PANED(paned), model_notebook, TRUE, FALSE);

    /* --- Área de salida (text view) dentro de un scrolled window --- */
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    ctx->text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(ctx->text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(ctx->text_view), GTK_WRAP_NONE);
    {
        GtkCssProvider *tv_css = gtk_css_provider_new();
        gtk_css_provider_load_from_data(tv_css, "textview { font-family: monospace; }", -1, NULL);
        gtk_style_context_add_provider(gtk_widget_get_style_context(ctx->text_view),
                                       GTK_STYLE_PROVIDER(tv_css),
                                       GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(tv_css);
    }
    gtk_container_add(GTK_CONTAINER(scrolled), ctx->text_view);

    /* Parte inferior: área de salida (no toma espacio extra, puede encogerse) */
    gtk_paned_pack2(GTK_PANED(paned), scrolled, FALSE, TRUE);

    /* Posición inicial del divisor: 550px (aprox 78% de 700) */
    gtk_paned_set_position(GTK_PANED(paned), 550);

    /* --- Barra de estado inferior compacta --- */
    GtkWidget *status_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_box_pack_start(GTK_BOX(vbox), status_bar, FALSE, FALSE, 0);
    gtk_widget_set_margin_top(status_bar, 0);
    gtk_widget_set_margin_bottom(status_bar, 0);
    gtk_widget_set_size_request(status_bar, -1, 24);

    ctx->status_label = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(ctx->status_label), 0.0);
    /* Que un mensaje largo no estire la ventana: se corta con puntos
     * suspensivos y el texto entero queda en el globo.                   */
    gtk_label_set_ellipsize(GTK_LABEL(ctx->status_label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_size_request(ctx->status_label, 120, -1);
    gtk_box_pack_start(GTK_BOX(status_bar), ctx->status_label, TRUE, TRUE, 0);

    /* El avance del motor: se ve solo mientras corre */
    ctx->progress = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(ctx->progress), TRUE);
    gtk_widget_set_size_request(ctx->progress, 110, -1);
    gtk_widget_set_valign(ctx->progress, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(status_bar), ctx->progress, FALSE, FALSE, 4);
    gtk_widget_set_no_show_all(ctx->progress, TRUE);

    GtkWidget *model_label_title = gtk_label_new("Model: ");
    gtk_box_pack_start(GTK_BOX(status_bar), model_label_title, FALSE, FALSE, 0);
    ctx->model_label = gtk_label_new("(none)");
    gtk_label_set_xalign(GTK_LABEL(ctx->model_label), 0.0);
    gtk_box_pack_start(GTK_BOX(status_bar), ctx->model_label, FALSE, FALSE, 0);

    return window;
}

/* Nueva pestaña para la especificación de datos (incluye rescaling factor) */
static GtkWidget* create_data_input_tab(FueContext *ctx) {
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);

    int row = 0;

    /* Fila 1: Series name */
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Series Name:"), 0, row, 1, 1);
    ctx->series_name_entry = gtk_entry_new();
    g_signal_connect(ctx->series_name_entry, "changed", G_CALLBACK(on_series_name_changed), ctx);
    gtk_grid_attach(GTK_GRID(grid), ctx->series_name_entry, 1, row, 1, 1);
    row++;

    /* Fila 2: Frequency */
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Frequency:"), 0, row, 1, 1);
    ctx->freq_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ctx->freq_combo), NULL, "1 (Annual or numbered)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ctx->freq_combo), NULL, "4 (Quarterly)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ctx->freq_combo), NULL, "12 (Monthly)");
    gtk_combo_box_set_active(GTK_COMBO_BOX(ctx->freq_combo), 2);
    g_signal_connect(ctx->freq_combo, "changed", G_CALLBACK(on_freq_changed), ctx);
    gtk_grid_attach(GTK_GRID(grid), ctx->freq_combo, 1, row, 1, 1);
    row++;

    /* Fila 3: Observations, start period, start year */
    GtkWidget *hbox_obs = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_grid_attach(GTK_GRID(grid), hbox_obs, 1, row, 1, 1);
    GtkWidget *label_nobs = gtk_label_new("Observations:");
    gtk_box_pack_start(GTK_BOX(hbox_obs), label_nobs, FALSE, FALSE, 0);
    ctx->n_obs_spin = gtk_spin_button_new_with_range(1, 10000, 1);
    gtk_box_pack_start(GTK_BOX(hbox_obs), ctx->n_obs_spin, FALSE, FALSE, 0);
    GtkWidget *label_start_period = gtk_label_new("Start Period:");
    gtk_box_pack_start(GTK_BOX(hbox_obs), label_start_period, FALSE, FALSE, 0);
    ctx->start_period_spin = gtk_spin_button_new_with_range(1, 12, 1);
    gtk_box_pack_start(GTK_BOX(hbox_obs), ctx->start_period_spin, FALSE, FALSE, 0);
    GtkWidget *label_start_year = gtk_label_new("Start Year:");
    gtk_box_pack_start(GTK_BOX(hbox_obs), label_start_year, FALSE, FALSE, 0);
    ctx->start_year_spin = gtk_spin_button_new_with_range(1000, 3000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->start_year_spin), 2000);
    gtk_box_pack_start(GTK_BOX(hbox_obs), ctx->start_year_spin, FALSE, FALSE, 0);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Time range:"), 0, row, 1, 1);
    row++;

    /* Fila 4: Data File, Workspace, Input Name en una misma fila */
    GtkWidget *hbox_files = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_grid_attach(GTK_GRID(grid), hbox_files, 0, row, 2, 1);
    GtkWidget *label_data = gtk_label_new("Data File:");
    gtk_box_pack_start(GTK_BOX(hbox_files), label_data, FALSE, FALSE, 0);
    ctx->data_file_chooser = gtk_file_chooser_button_new("Select data file", GTK_FILE_CHOOSER_ACTION_OPEN);
    g_signal_connect(ctx->data_file_chooser, "file-set", G_CALLBACK(on_data_file_selected), ctx);
    gtk_box_pack_start(GTK_BOX(hbox_files), ctx->data_file_chooser, TRUE, TRUE, 0);

    GtkWidget *label_ws = gtk_label_new("Workspace:");
    gtk_box_pack_start(GTK_BOX(hbox_files), label_ws, FALSE, FALSE, 0);
    ctx->workspace_file_chooser = gtk_file_chooser_button_new("Select workspace", GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    /* Con --proyecto, el espacio de trabajo ya se sabe al arrancar. */
    {
    const char *raiz = fue_raiz_proyecto();

    if (raiz)
        gtk_file_chooser_set_current_folder(
            GTK_FILE_CHOOSER(ctx->workspace_file_chooser), raiz);
    }
    gtk_box_pack_start(GTK_BOX(hbox_files), ctx->workspace_file_chooser, TRUE, TRUE, 0);

    GtkWidget *label_inp = gtk_label_new("Input Name:");
    gtk_box_pack_start(GTK_BOX(hbox_files), label_inp, FALSE, FALSE, 0);
    ctx->input_name_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox_files), ctx->input_name_entry, TRUE, TRUE, 0);
    /* LA ESQUINA SIGUE A LA ENTRADA, no a un momento concreto.
     *
     * "Model:" se actualizaba SOLO al guardar el .inp, asi que al cargar un
     * modelo --por el selector o mandado por la madre-- seguia diciendo el
     * anterior, o "(none)". Un rotulo que dice lo que hay tiene que
     * escucharlo, no que lo avisen: si depende de que alguien se acuerde de
     * llamarlo, es cuestion de tiempo que un camino nuevo se olvide. Y eso
     * es lo que paso con el camino que abre la madre.                  */
    g_signal_connect_swapped(ctx->input_name_entry, "changed",
                             G_CALLBACK(update_model_label), ctx);
    row++;

    /* Fila 5: Rescaling Factor (movido aquí) */
    GtkWidget *hbox_rescaling = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_grid_attach(GTK_GRID(grid), hbox_rescaling, 0, row, 2, 1);
    GtkWidget *rescaling_label = gtk_label_new("Rescaling Factor:");
    gtk_box_pack_start(GTK_BOX(hbox_rescaling), rescaling_label, FALSE, FALSE, 0);
    ctx->refactor_spin = gtk_spin_button_new_with_range(1, 1000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->refactor_spin), 1);
    g_signal_connect(ctx->refactor_spin, "value-changed", G_CALLBACK(on_refactor_changed), ctx);
    gtk_box_pack_start(GTK_BOX(hbox_rescaling), ctx->refactor_spin, FALSE, FALSE, 0);
    row++;

    return grid;
}

/* Helper for the Box-Cox tab (simplified) */
static GtkWidget* create_boxcox_tab(FueContext *ctx) {
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);

    int row = 0;
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Box-Cox Lambda:"), 0, row, 1, 1);
    ctx->boxcox_lambda_spin = gtk_spin_button_new_with_range(-20, 20, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->boxcox_lambda_spin), 1);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(ctx->boxcox_lambda_spin), 2);
    gtk_grid_attach(GTK_GRID(grid), ctx->boxcox_lambda_spin, 1, row++, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Box-Cox m:"), 0, row, 1, 1);
    ctx->boxcox_m_spin = gtk_spin_button_new_with_range(0, 100, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->boxcox_m_spin), 1);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(ctx->boxcox_m_spin), 2);
    gtk_grid_attach(GTK_GRID(grid), ctx->boxcox_m_spin, 1, row++, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Regular Differences:"), 0, row, 1, 1);
    ctx->nrdiff_spin = gtk_spin_button_new_with_range(0, 10, 1);
    gtk_grid_attach(GTK_GRID(grid), ctx->nrdiff_spin, 1, row++, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Annual Differences:"), 0, row, 1, 1);
    ctx->nadiff_spin = gtk_spin_button_new_with_range(0, 10, 1);
    gtk_grid_attach(GTK_GRID(grid), ctx->nadiff_spin, 1, row++, 1, 1);

    /* Individual factors (for monthly/quarterly) */
    GtkWidget *expander = gtk_expander_new("Individual factors of the annual difference");
    GtkWidget *hbox_factors = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    ctx->f0_check = gtk_check_button_new_with_label("f=0");
    ctx->f1_check = gtk_check_button_new_with_label("f=1");
    ctx->f2_check = gtk_check_button_new_with_label("f=2");
    ctx->f3_check = gtk_check_button_new_with_label("f=3");
    ctx->f4_check = gtk_check_button_new_with_label("f=4");
    ctx->f5_check = gtk_check_button_new_with_label("f=5");
    ctx->f6_check = gtk_check_button_new_with_label("f=6");
    gtk_box_pack_start(GTK_BOX(hbox_factors), ctx->f0_check, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_factors), ctx->f1_check, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_factors), ctx->f2_check, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_factors), ctx->f3_check, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_factors), ctx->f4_check, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_factors), ctx->f5_check, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_factors), ctx->f6_check, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(expander), hbox_factors);
    gtk_grid_attach(GTK_GRID(grid), expander, 0, row++, 2, 1);

    return grid;
}

/* Helper for the deterministic component tab */
static GtkWidget* create_deterministic_tab(FueContext *ctx) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);

    /* TreeView */
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    ctx->int_treeview = gtk_tree_view_new();
    create_intervention_tree_view(GTK_TREE_VIEW(ctx->int_treeview));
    g_signal_connect(ctx->int_treeview, "row-activated",
                     G_CALLBACK(on_int_treeview_row_activated), ctx);
    gtk_container_add(GTK_CONTAINER(scrolled), ctx->int_treeview);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);

    /* Buttons */
    GtkWidget *button_box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_END);
    GtkWidget *btn_add = gtk_button_new_with_label("Add");
    g_signal_connect(btn_add, "clicked", G_CALLBACK(on_add_deterministic), ctx);
    gtk_container_add(GTK_CONTAINER(button_box), btn_add);
    GtkWidget *btn_insert = gtk_button_new_with_label("Insert");
    g_signal_connect(btn_insert, "clicked", G_CALLBACK(on_insert_int), ctx);
    gtk_container_add(GTK_CONTAINER(button_box), btn_insert);
    GtkWidget *btn_edit = gtk_button_new_with_label("Edit");
    g_signal_connect(btn_edit, "clicked", G_CALLBACK(on_edit_int), ctx);
    gtk_container_add(GTK_CONTAINER(button_box), btn_edit);
    GtkWidget *btn_remove = gtk_button_new_with_label("Remove");
    g_signal_connect(btn_remove, "clicked", G_CALLBACK(on_remove_int), ctx);
    gtk_container_add(GTK_CONTAINER(button_box), btn_remove);
    GtkWidget *btn_seasonals = gtk_button_new_with_label("Add Seasonals");
    g_signal_connect(btn_seasonals, "clicked", G_CALLBACK(on_add_seasonals), ctx);
    gtk_container_add(GTK_CONTAINER(button_box), btn_seasonals);
    gtk_box_pack_start(GTK_BOX(vbox), button_box, FALSE, FALSE, 0);

    /* Mean parameter */
    GtkWidget *mean_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), mean_box, FALSE, FALSE, 0);
    ctx->mean_check = gtk_check_button_new_with_label("Include Mean Parameter:");
    gtk_box_pack_start(GTK_BOX(mean_box), ctx->mean_check, FALSE, FALSE, 0);
    ctx->mean_spin = gtk_spin_button_new_with_range(-100000, 100000, 1);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(ctx->mean_spin), 2);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->mean_spin), 0.0);
    gtk_box_pack_start(GTK_BOX(mean_box), ctx->mean_spin, FALSE, FALSE, 0);

    return vbox;
}

/* Helper for the stochastic component tab (sub-notebook) */
static GtkWidget* create_stochastic_tab(FueContext *ctx) {
    GtkWidget *notebook = gtk_notebook_new();
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_LEFT);

    GtkWidget *arr_tab = create_operator_tab(ctx, "arr_treeview", "AR Regular", &ctx->arr_treeview, 1);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), arr_tab, gtk_label_new("AR Regular"));

    GtkWidget *ara_tab = create_operator_tab(ctx, "ara_treeview", "AR Annual", &ctx->ara_treeview, 2);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), ara_tab, gtk_label_new("AR Annual"));

    GtkWidget *mar_tab = create_operator_tab(ctx, "mar_treeview", "MA Regular", &ctx->mar_treeview, 3);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), mar_tab, gtk_label_new("MA Regular"));

    GtkWidget *maa_tab = create_operator_tab(ctx, "maa_treeview", "MA Annual", &ctx->maa_treeview, 4);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), maa_tab, gtk_label_new("MA Annual"));

    GtkWidget *ar_fix_tab = create_fixed_tab(ctx, "ar_fix_treeview", "AR Fixed Frequency", &ctx->ar_fix_treeview, 5);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), ar_fix_tab, gtk_label_new("AR Fixed Frequency"));

    GtkWidget *ma_fix_tab = create_fixed_tab(ctx, "ma_fix_treeview", "MA Fixed Frequency", &ctx->ma_fix_treeview, 6);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), ma_fix_tab, gtk_label_new("MA Fixed Frequency"));

    return notebook;
}

/* Helper to create a generic operator tab (for AR/MA regular/annual) */
static GtkWidget* create_operator_tab(FueContext *ctx, const char *tv_name,
                                      const char *title, GtkWidget **treeview_ptr,
                                      int op_type) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);

    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    *treeview_ptr = gtk_tree_view_new();
    create_operator_tree_view(GTK_TREE_VIEW(*treeview_ptr));
    g_signal_connect(*treeview_ptr, "row-activated",
                     G_CALLBACK(on_operator_treeview_row_activated), ctx);
    gtk_container_add(GTK_CONTAINER(scrolled), *treeview_ptr);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);

    GtkWidget *button_box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_END);

    GtkWidget *btn_add = gtk_button_new_with_label("Add");
    g_object_set_data(G_OBJECT(btn_add), "op_type", GINT_TO_POINTER(op_type));
    g_signal_connect(btn_add, "clicked", G_CALLBACK(on_add_operator), ctx);
    gtk_container_add(GTK_CONTAINER(button_box), btn_add);

    GtkWidget *btn_insert = gtk_button_new_with_label("Insert");
    g_object_set_data(G_OBJECT(btn_insert), "op_type", GINT_TO_POINTER(op_type));
    g_object_set_data(G_OBJECT(btn_insert), "treeview", *treeview_ptr);
    g_signal_connect(btn_insert, "clicked", G_CALLBACK(on_insert_operator), ctx);
    gtk_container_add(GTK_CONTAINER(button_box), btn_insert);

    GtkWidget *btn_edit = gtk_button_new_with_label("Edit");
    g_object_set_data(G_OBJECT(btn_edit), "op_type", GINT_TO_POINTER(op_type));
    g_object_set_data(G_OBJECT(btn_edit), "treeview", *treeview_ptr);
    g_signal_connect(btn_edit, "clicked", G_CALLBACK(on_edit_operator), ctx);
    gtk_container_add(GTK_CONTAINER(button_box), btn_edit);

    GtkWidget *btn_remove = gtk_button_new_with_label("Remove");
    g_object_set_data(G_OBJECT(btn_remove), "op_type", GINT_TO_POINTER(op_type));
    g_object_set_data(G_OBJECT(btn_remove), "treeview", *treeview_ptr);
    g_signal_connect(btn_remove, "clicked", G_CALLBACK(on_remove_operator), ctx);
    gtk_container_add(GTK_CONTAINER(button_box), btn_remove);

    gtk_box_pack_start(GTK_BOX(vbox), button_box, FALSE, FALSE, 0);
    return vbox;
}

/* Helper for fixed-frequency operator tab */
static GtkWidget* create_fixed_tab(FueContext *ctx, const char *tv_name,
                                   const char *title, GtkWidget **treeview_ptr,
                                   int op_type) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);

    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    *treeview_ptr = gtk_tree_view_new();
    create_fixed_tree_view(GTK_TREE_VIEW(*treeview_ptr));
    g_signal_connect(*treeview_ptr, "row-activated",
                     G_CALLBACK(on_fixed_treeview_row_activated), ctx);
    gtk_container_add(GTK_CONTAINER(scrolled), *treeview_ptr);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);

    GtkWidget *button_box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_END);

    GtkWidget *btn_add = gtk_button_new_with_label("Add");
    g_object_set_data(G_OBJECT(btn_add), "op_type", GINT_TO_POINTER(op_type));
    g_signal_connect(btn_add, "clicked", G_CALLBACK(on_add_fixed), ctx);
    gtk_container_add(GTK_CONTAINER(button_box), btn_add);

    GtkWidget *btn_insert = gtk_button_new_with_label("Insert");
    g_object_set_data(G_OBJECT(btn_insert), "op_type", GINT_TO_POINTER(op_type));
    g_object_set_data(G_OBJECT(btn_insert), "treeview", *treeview_ptr);
    g_signal_connect(btn_insert, "clicked", G_CALLBACK(on_insert_fixed), ctx);
    gtk_container_add(GTK_CONTAINER(button_box), btn_insert);

    GtkWidget *btn_edit = gtk_button_new_with_label("Edit");
    g_object_set_data(G_OBJECT(btn_edit), "op_type", GINT_TO_POINTER(op_type));
    g_object_set_data(G_OBJECT(btn_edit), "treeview", *treeview_ptr);
    g_signal_connect(btn_edit, "clicked", G_CALLBACK(on_edit_fixed), ctx);
    gtk_container_add(GTK_CONTAINER(button_box), btn_edit);

    GtkWidget *btn_remove = gtk_button_new_with_label("Remove");
    g_object_set_data(G_OBJECT(btn_remove), "op_type", GINT_TO_POINTER(op_type));
    g_object_set_data(G_OBJECT(btn_remove), "treeview", *treeview_ptr);
    g_signal_connect(btn_remove, "clicked", G_CALLBACK(on_remove_fixed), ctx);
    gtk_container_add(GTK_CONTAINER(button_box), btn_remove);

    gtk_box_pack_start(GTK_BOX(vbox), button_box, FALSE, FALSE, 0);
    return vbox;
}

/* Console tab (con fuente monoespaciada y sin ajuste de línea) */
static GtkWidget* create_console_tab(FueContext *ctx) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);

    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    ctx->console_text_view = gtk_text_view_new();

    gtk_text_view_set_editable(GTK_TEXT_VIEW(ctx->console_text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(ctx->console_text_view), GTK_WRAP_NONE);
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        "textview { font-family: monospace; }", -1, NULL);
    gtk_style_context_add_provider(gtk_widget_get_style_context(ctx->console_text_view),
                                   GTK_STYLE_PROVIDER(provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);

    {
        GtkTextBuffer *console_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctx->console_text_view));
        gtk_text_buffer_set_text(console_buf,
            "No output yet. Save a model and click Run to see results here.", -1);
    }
    gtk_container_add(GTK_CONTAINER(scrolled), ctx->console_text_view);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);

    GtkWidget *button_box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_END);

    /* AQUI ESTABAN «Edit .inp», «Edit .pre» y «Save .inp», y se han ido.
     *
     * El editor del fichero vive ahora en la madre, sobre un NODO del
     * proyecto: gui/atsw/src/editor.c. Es la mudanza que docs/DISENO-editor
     * §7 dejaba pendiente desde la fase 2 -- entonces el editor trabajaba
     * sobre «<area de trabajo>/<modelo>», y abrir cualquier otro chocaba con
     * que la raiz tiene que estar declarada. Ya lo esta.
     *
     * Aqui sobraban por dos razones y la segunda es la de peso:
     *
     *   - eran una TERCERA puerta al mismo fichero, al lado del formulario
     *     de estas pestañas;
     *   - y su ambito era «el modelo del area de trabajo», que no es un
     *     nodo de nada. Desde que existe el manifiesto, editar un .inp que
     *     ya se estimo puede DERIVAR uno nuevo en vez de dejar su .out
     *     describiendo otra cosa. Eso aqui no se podia hacer.
     *
     * La consola se queda: es donde sale lo que escribe el motor.      */
    gtk_widget_destroy(button_box);
    return vbox;
}

/* Callbacks para doble clic en los árboles de operadores */
static void on_operator_treeview_row_activated(GtkTreeView *treeview, GtkTreePath *path,
                                               GtkTreeViewColumn *col, FueContext *ctx) {
    int type_op = 0;
    if (treeview == GTK_TREE_VIEW(ctx->arr_treeview)) type_op = 1;
    else if (treeview == GTK_TREE_VIEW(ctx->ara_treeview)) type_op = 2;
    else if (treeview == GTK_TREE_VIEW(ctx->mar_treeview)) type_op = 3;
    else if (treeview == GTK_TREE_VIEW(ctx->maa_treeview)) type_op = 4;
    else return;

    GtkTreeSelection *sel = gtk_tree_view_get_selection(treeview);
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
        int number;
        gtk_tree_model_get(model, &iter, 0, &number, -1);
        ctx->edit_index = number - 1;
        ctx->type_op = type_op;
        ensure_operator_dialog(ctx);
        reload_op(ctx, ctx->type_op, ctx->edit_index);
        ctx->new_op = 1;
        gtk_widget_show(ctx->esp_op_dialog);
    }
}

static void on_fixed_treeview_row_activated(GtkTreeView *treeview, GtkTreePath *path,
                                            GtkTreeViewColumn *col, FueContext *ctx) {
    int type_op = 0;
    if (treeview == GTK_TREE_VIEW(ctx->ar_fix_treeview)) type_op = 5;
    else if (treeview == GTK_TREE_VIEW(ctx->ma_fix_treeview)) type_op = 6;
    else return;

    GtkTreeSelection *sel = gtk_tree_view_get_selection(treeview);
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
        int number;
        gtk_tree_model_get(model, &iter, 0, &number, -1);
        ctx->edit_index = number - 1;
        ctx->type_op = type_op;
        ensure_fixed_dialog(ctx);
        reload_ar2f(ctx, ctx->type_op, ctx->edit_index);
        ctx->new_op = 1;
        gtk_widget_show(ctx->esp_fix_dialog);
    }
}

/* Funciones auxiliares para la creación de los árboles */
static void create_intervention_tree_view(GtkTreeView *treeview) {
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *col;
    GtkListStore *store;

    store = gtk_list_store_new(NUM_COLS, G_TYPE_UINT, G_TYPE_STRING,
                               G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT);
    gtk_tree_view_set_model(treeview, GTK_TREE_MODEL(store));
    g_object_unref(store);

    renderer = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes("Number", renderer, "text", COL_NUM, NULL);
    gtk_tree_view_append_column(treeview, col);

    renderer = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes("Type", renderer, "text", COL_NAME, NULL);
    gtk_tree_view_append_column(treeview, col);

    renderer = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes("Season/Freq", renderer, "text", COL_SEASON, NULL);
    gtk_tree_view_append_column(treeview, col);

    renderer = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes("Year/Number", renderer, "text", COL_YEAR, NULL);
    gtk_tree_view_append_column(treeview, col);

    renderer = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes("MA Order", renderer, "text", COL_MAOR, NULL);
    gtk_tree_view_append_column(treeview, col);

    renderer = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes("AR Order", renderer, "text", COL_AROR, NULL);
    gtk_tree_view_append_column(treeview, col);
}

static void create_operator_tree_view(GtkTreeView *treeview) {
    GtkCellRenderer *renderer;
    GtkListStore *store;

    store = gtk_list_store_new(3, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_UINT);
    gtk_tree_view_set_model(treeview, GTK_TREE_MODEL(store));
    g_object_unref(store);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes(treeview, -1, "Number", renderer, "text", 0, NULL);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes(treeview, -1, "Restriction", renderer, "text", 1, NULL);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes(treeview, -1, "Order", renderer, "text", 2, NULL);
}

static void create_fixed_tree_view(GtkTreeView *treeview) {
    GtkCellRenderer *renderer;
    GtkListStore *store;

    store = gtk_list_store_new(3, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_UINT);
    gtk_tree_view_set_model(treeview, GTK_TREE_MODEL(store));
    g_object_unref(store);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes(treeview, -1, "Number", renderer, "text", 0, NULL);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes(treeview, -1, "Restriction", renderer, "text", 1, NULL);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes(treeview, -1, "Frequency", renderer, "text", 2, NULL);
}



/* Función para actualizar la etiqueta del modelo en la barra de estado */
void update_model_label(FueContext *ctx) {
    const char *input_name = gtk_entry_get_text(GTK_ENTRY(ctx->input_name_entry));
    if (input_name && strlen(input_name) > 0) {
        char *model_str = g_strdup_printf("%s.inp", input_name);
        gtk_label_set_text(GTK_LABEL(ctx->model_label), model_str);
        g_free(model_str);
    } else {
        gtk_label_set_text(GTK_LABEL(ctx->model_label), "(none)");
    }
    /* Cambio el fichero: los botones de analisis se vuelven a preguntar si
       tienen su insumo. Ver gui/fue/src/analisis.c.                    */
    fue_analisis_refresca(ctx);
}
