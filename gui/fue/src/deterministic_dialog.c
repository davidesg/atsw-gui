/* deterministic_dialog.c */
#include "deterministic_dialog.h"
#include "fue_globals.h"       /* for global It, NdetVar, Ts, Tm, etc. */
#include "utils.h"              /* for reload_number_int */
#include <string.h>

/* Global variables from the core (declared in fue_globals.h) */
extern struct intervention It[50];
extern int NdetVar;
extern struct Tseries Ts;
extern struct Tusmodel Tm;
extern int ma_order_inc, ar_order_inc, new_det;

/* Forward declarations of static helper functions */
static void fill_intervention_from_dialog(int idx, FueContext *ctx);
static void adjust_ma_rows(FueContext *ctx, int new_order);
static void adjust_ar_rows(FueContext *ctx, int new_order);
static void on_int_dialog_response(GtkDialog *dialog, gint response_id, FueContext *ctx);

/* ========================================================================= */
/* Create the deterministic dialog (once) and store it in ctx->esp_int_dialog */
/* ========================================================================= */
 void ensure_dialog_created(FueContext *ctx) {
    if (ctx->esp_int_dialog) return;

    ctx->esp_int_dialog = gtk_dialog_new_with_buttons("Deterministic Component",
                                                      GTK_WINDOW(ctx->main_window),
                                                      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                      "_Cancel", GTK_RESPONSE_CANCEL,
                                                      "_OK", GTK_RESPONSE_OK,
                                                      NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(ctx->esp_int_dialog), GTK_RESPONSE_OK);
    gtk_window_set_default_size(GTK_WINDOW(ctx->esp_int_dialog), 550, 550);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(ctx->esp_int_dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 10);

    g_signal_connect(ctx->esp_int_dialog, "response",
                     G_CALLBACK(on_int_dialog_response), ctx);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_container_add(GTK_CONTAINER(content), grid);
    gtk_widget_set_vexpand(grid, TRUE);
    gtk_widget_set_hexpand(grid, TRUE);

    int row = 0;

    /* Tipo */
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Type:"), 0, row, 1, 1);
    GtkWidget *type_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Impulse");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Compensated Impulse");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Step");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Ramp");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Linear Trend");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Easter Effect");
    g_signal_connect(type_combo, "changed", G_CALLBACK(on_type_int_combobox_changed), ctx);
    g_object_set_data(G_OBJECT(ctx->esp_int_dialog), "type_combo", type_combo);
    gtk_grid_attach(GTK_GRID(grid), type_combo, 1, row, 1, 1);
    row++;

    /* Season */
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Season:"), 0, row, 1, 1);
    GtkWidget *period_spin = gtk_spin_button_new_with_range(1, 12, 1);
    g_object_set_data(G_OBJECT(ctx->esp_int_dialog), "period_spin", period_spin);
    gtk_grid_attach(GTK_GRID(grid), period_spin, 1, row, 1, 1);
    row++;

    /* Year */
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Year:"), 0, row, 1, 1);
    GtkWidget *year_spin = gtk_spin_button_new_with_range(1000, 3000, 1);
    g_object_set_data(G_OBJECT(ctx->esp_int_dialog), "year_spin", year_spin);
    gtk_grid_attach(GTK_GRID(grid), year_spin, 1, row, 1, 1);
    row++;

    /* MA Frame */
    GtkWidget *ma_frame = gtk_frame_new("MA Operator Specification");
    gtk_grid_attach(GTK_GRID(grid), ma_frame, 0, row, 2, 1);
    gtk_widget_set_vexpand(ma_frame, TRUE);   /* Permitir expansión vertical */

    GtkWidget *ma_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(ma_vbox), 5);
    gtk_container_add(GTK_CONTAINER(ma_frame), ma_vbox);
    gtk_widget_set_vexpand(ma_vbox, TRUE);    /* También expandir el vbox interno */

    GtkWidget *ma_order_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(ma_vbox), ma_order_hbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ma_order_hbox), gtk_label_new("Order:"), FALSE, FALSE, 0);
    GtkWidget *ma_order_spin = gtk_spin_button_new_with_range(0, 20, 1);
    /* Conectar señal - AHORA DESCOMENTADA */
    g_signal_connect(ma_order_spin, "value-changed",
                     G_CALLBACK(on_ma_order_int_spinbutton_value_changed), ctx);
    gtk_box_pack_start(GTK_BOX(ma_order_hbox), ma_order_spin, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(ctx->esp_int_dialog), "ma_order_spin", ma_order_spin);

    /* Encabezado para los parámetros MA */
    GtkWidget *ma_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_set_homogeneous(GTK_BOX(ma_header), TRUE);
    gtk_box_pack_start(GTK_BOX(ma_vbox), ma_header, FALSE, FALSE, 0);
    GtkWidget *ma_header_label1 = gtk_label_new("Omegas:");
    gtk_box_pack_start(GTK_BOX(ma_header), ma_header_label1, TRUE, TRUE, 0);
    GtkWidget *ma_header_label2 = gtk_label_new("Preliminary Estimates:");
    gtk_box_pack_start(GTK_BOX(ma_header), ma_header_label2, TRUE, TRUE, 0);
    GtkWidget *ma_header_label3 = gtk_label_new("Check to Fix");
    gtk_box_pack_start(GTK_BOX(ma_header), ma_header_label3, TRUE, FALSE, 0);
    gtk_widget_show_all(ma_header);


    GtkWidget *ma_params_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(ma_vbox), ma_params_vbox, FALSE, FALSE, 0);
    gtk_widget_set_vexpand(ma_params_vbox, TRUE);   /* <--- AÑADIDO */
    g_object_set_data(G_OBJECT(ctx->esp_int_dialog), "ma_int_vbox", ma_params_vbox);

    /* Fila 0 MA */
    GtkWidget *ma_row0 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_set_homogeneous(GTK_BOX(ma_row0), TRUE);
    gtk_box_pack_start(GTK_BOX(ma_params_vbox), ma_row0, FALSE, FALSE, 0);
    GtkWidget *label0 = gtk_label_new("0");
    gtk_box_pack_start(GTK_BOX(ma_row0), label0, TRUE, TRUE, 0);
    GtkWidget *ma_spin0 = gtk_spin_button_new_with_range(-1e6, 1e6, 1);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(ma_spin0), 6);
    gtk_box_pack_start(GTK_BOX(ma_row0), ma_spin0, TRUE, TRUE, 0);
    GtkWidget *ma_check0 = gtk_check_button_new();
    gtk_box_pack_start(GTK_BOX(ma_row0), ma_check0, TRUE, FALSE, 0);
    g_object_set_data(G_OBJECT(ctx->esp_int_dialog), "ma_spinbutton_0", ma_spin0);
    g_object_set_data(G_OBJECT(ctx->esp_int_dialog), "ma_checkbutton_0", ma_check0);
    g_object_set_data(G_OBJECT(ctx->esp_int_dialog), "ma_hbox_0", ma_row0);
    /* No duplicar la conexión aquí */


    /* Contenedor dinámico para filas adicionales (dentro de ma_params_vbox) */
    GtkWidget *ma_int_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(ma_params_vbox), ma_int_vbox, FALSE, FALSE, 0);
    gtk_widget_set_vexpand(ma_int_vbox, TRUE);      /* <--- AÑADIDO */
    g_object_set_data(G_OBJECT(ctx->esp_int_dialog), "ma_int_vbox", ma_int_vbox);
    row++;

    /* AR Frame */
    GtkWidget *ar_frame = gtk_frame_new("AR Operator Specification");
    gtk_grid_attach(GTK_GRID(grid), ar_frame, 0, row, 2, 1);
    gtk_widget_set_vexpand(ar_frame, TRUE);
    g_object_set_data(G_OBJECT(ctx->esp_int_dialog), "ar_int_frame", ar_frame);

    GtkWidget *ar_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(ar_vbox), 5);
    gtk_container_add(GTK_CONTAINER(ar_frame), ar_vbox);
    gtk_widget_set_vexpand(ar_vbox, TRUE);

    GtkWidget *ar_order_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(ar_vbox), ar_order_hbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ar_order_hbox), gtk_label_new("Order:"), FALSE, FALSE, 0);
    GtkWidget *ar_order_spin = gtk_spin_button_new_with_range(0, 20, 1);
    /* Conectar señal - AHORA DESCOMENTADA */
    g_signal_connect(ar_order_spin, "value-changed",
                     G_CALLBACK(on_ar_order_int_spinbutton_value_changed), ctx);
    gtk_box_pack_start(GTK_BOX(ar_order_hbox), ar_order_spin, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(ctx->esp_int_dialog), "ar_order_spin", ar_order_spin);


    /* Encabezado para los parámetros AR */
    GtkWidget *ar_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_set_homogeneous(GTK_BOX(ar_header), TRUE);
    gtk_box_pack_start(GTK_BOX(ar_vbox), ar_header, FALSE, FALSE, 0);
    GtkWidget *ar_header_label1 = gtk_label_new("Deltas:");
    gtk_box_pack_start(GTK_BOX(ar_header), ar_header_label1, TRUE, TRUE, 0);
    GtkWidget *ar_header_label2 = gtk_label_new("Preliminary Estimates:");
    gtk_box_pack_start(GTK_BOX(ar_header), ar_header_label2, TRUE, TRUE, 0);
    GtkWidget *ar_header_label3 = gtk_label_new("Check to Fix");
    gtk_box_pack_start(GTK_BOX(ar_header), ar_header_label3, TRUE, FALSE, 0);
    gtk_widget_show_all(ar_header);

    GtkWidget *ar_params_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(ar_vbox), ar_params_vbox, FALSE, FALSE, 0);
    gtk_widget_set_vexpand(ar_params_vbox, TRUE);   /* <--- AÑADIDO */
    g_object_set_data(G_OBJECT(ctx->esp_int_dialog), "ar_int_vbox", ar_params_vbox);
    /* No duplicar la conexión aquí */

    /* Contenedor dinámico para filas AR */
    GtkWidget *ar_int_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(ar_params_vbox), ar_int_vbox, FALSE, FALSE, 0);
    gtk_widget_set_vexpand(ar_int_vbox, TRUE);      /* <--- AÑADIDO */
    g_object_set_data(G_OBJECT(ctx->esp_int_dialog), "ar_int_vbox", ar_int_vbox);

    gtk_widget_show_all(ctx->esp_int_dialog);
}


/* ========================================================================= */
/* Manejador de respuesta del diálogo */

 static void on_int_dialog_response(GtkDialog *dialog, gint response_id, FueContext *ctx) {
    if (response_id == GTK_RESPONSE_OK) {
        if (new_det == 0) save_new_int(ctx);
        else if (new_det == 1) edit_int(ctx);
        else if (new_det == 2) save_insert_int(ctx);
        else save_new_int(ctx);
    }
    gtk_widget_hide(GTK_WIDGET(dialog));
}


/* ========================================================================= */
/* Funciones de ajuste dinámico para MA y AR */

static void adjust_ma_rows(FueContext *ctx, int new_order) {
    GtkWidget *ma_vbox = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), "ma_int_vbox");
    if (!ma_vbox) return;

    GList *children = gtk_container_get_children(GTK_CONTAINER(ma_vbox));
    int current_rows = g_list_length(children);
    g_list_free(children);

    if (new_order > current_rows) {
        /* Añadir filas desde current_rows+1 hasta new_order */
        for (int i = current_rows + 1; i <= new_order; i++) {
            GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
            gtk_box_set_homogeneous(GTK_BOX(row), TRUE);
            gtk_box_pack_start(GTK_BOX(ma_vbox), row, FALSE, FALSE, 0);
            gtk_widget_show(row);

            char label_buf[8];
            snprintf(label_buf, sizeof(label_buf), "%d", i);
            GtkWidget *label = gtk_label_new(label_buf);
            gtk_box_pack_start(GTK_BOX(row), label, TRUE, TRUE, 0);
            gtk_widget_show(label);

            GtkWidget *spin = gtk_spin_button_new_with_range(-1e6, 1e6, 1);
            gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 6);
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), 0.0);        // <-- valor inicial 0
            gtk_box_pack_start(GTK_BOX(row), spin, TRUE, TRUE, 0);
            gtk_widget_show(spin);

            GtkWidget *check = gtk_check_button_new();
            gtk_box_pack_start(GTK_BOX(row), check, TRUE, FALSE, 0);
            gtk_widget_show(check);

            char name[32];
            sprintf(name, "ma_spinbutton_%d", i);
            g_object_set_data(G_OBJECT(ctx->esp_int_dialog), name, spin);
            sprintf(name, "ma_checkbutton_%d", i);
            g_object_set_data(G_OBJECT(ctx->esp_int_dialog), name, check);
            sprintf(name, "ma_hbox_%d", i);
            g_object_set_data(G_OBJECT(ctx->esp_int_dialog), name, row);
        }
    } else if (new_order < current_rows) {
        /* Eliminar filas desde current_rows hasta new_order+1 */
        for (int i = current_rows; i > new_order; i--) {
            char name[32];
            sprintf(name, "ma_hbox_%d", i);
            GtkWidget *row = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), name);
            if (row) {
                gtk_widget_destroy(row);
                g_object_set_data(G_OBJECT(ctx->esp_int_dialog), name, NULL);
                g_object_set_data(G_OBJECT(ctx->esp_int_dialog), g_strdup_printf("ma_spinbutton_%d", i), NULL);
                g_object_set_data(G_OBJECT(ctx->esp_int_dialog), g_strdup_printf("ma_checkbutton_%d", i), NULL);
            }
        }
    }
}

void on_ma_order_int_spinbutton_value_changed(GtkSpinButton *spin, FueContext *ctx) {
    int order = gtk_spin_button_get_value_as_int(spin);
    adjust_ma_rows(ctx, order);
    ctx->ma_order_inc = order;
}

static void adjust_ar_rows(FueContext *ctx, int new_order) {
    GtkWidget *ar_vbox = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), "ar_int_vbox");
    if (!ar_vbox) return;

    GList *children = gtk_container_get_children(GTK_CONTAINER(ar_vbox));
    int current_rows = g_list_length(children);
    g_list_free(children);

    if (new_order > current_rows) {
        for (int i = current_rows + 1; i <= new_order; i++) {
            GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
            gtk_box_set_homogeneous(GTK_BOX(row), TRUE);
            gtk_box_pack_start(GTK_BOX(ar_vbox), row, FALSE, FALSE, 0);
            gtk_widget_show(row);

            char label_buf[8];
            snprintf(label_buf, sizeof(label_buf), "%d", i);
            GtkWidget *label = gtk_label_new(label_buf);
            gtk_box_pack_start(GTK_BOX(row), label, TRUE, TRUE, 0);
            gtk_widget_show(label);

            GtkWidget *spin = gtk_spin_button_new_with_range(-1e6, 1e6, 1);
            gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 6);
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), 0.0);
            gtk_box_pack_start(GTK_BOX(row), spin, TRUE, TRUE, 0);
            gtk_widget_show(spin);

            GtkWidget *check = gtk_check_button_new();
            gtk_box_pack_start(GTK_BOX(row), check, TRUE, FALSE, 0);
            gtk_widget_show(check);

            char name[32];
            sprintf(name, "ar_spinbutton_%d", i);
            g_object_set_data(G_OBJECT(ctx->esp_int_dialog), name, spin);
            sprintf(name, "ar_checkbutton_%d", i);
            g_object_set_data(G_OBJECT(ctx->esp_int_dialog), name, check);
            sprintf(name, "ar_hbox_%d", i);
            g_object_set_data(G_OBJECT(ctx->esp_int_dialog), name, row);
        }
    } else if (new_order < current_rows) {
        for (int i = current_rows; i > new_order; i--) {
            char name[32];
            sprintf(name, "ar_hbox_%d", i);
            GtkWidget *row = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), name);
            if (row) {
                gtk_widget_destroy(row);
                g_object_set_data(G_OBJECT(ctx->esp_int_dialog), name, NULL);
                g_object_set_data(G_OBJECT(ctx->esp_int_dialog), g_strdup_printf("ar_spinbutton_%d", i), NULL);
                g_object_set_data(G_OBJECT(ctx->esp_int_dialog), g_strdup_printf("ar_checkbutton_%d", i), NULL);
            }
        }
    }
}

void on_ar_order_int_spinbutton_value_changed(GtkSpinButton *spin, FueContext *ctx) {
    int order = gtk_spin_button_get_value_as_int(spin);
    adjust_ar_rows(ctx, order);
    ctx->ar_order_inc = order;
}



/* ========================================================================= */
/* Public function: initialize dialog for a new intervention */
/* ========================================================================= */
 void new_int(FueContext *ctx) {
    GtkWidget *type_combo = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), "type_combo");
    GtkWidget *period_spin = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), "period_spin");
    GtkWidget *year_spin = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), "year_spin");
    GtkWidget *ma_order_spin = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), "ma_order_spin");
    GtkWidget *ar_order_spin = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), "ar_order_spin");
    GtkWidget *ma_spin0 = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), "ma_spinbutton_0");
    GtkWidget *ma_check0 = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), "ma_checkbutton_0");

    gtk_combo_box_set_active(GTK_COMBO_BOX(type_combo), 0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(period_spin), 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(year_spin), 2000);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ma_order_spin), 0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ar_order_spin), 0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ma_spin0), 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ma_check0), FALSE);

    /* Forzar limpieza de filas (aunque el valor sea 0) */
    adjust_ma_rows(ctx, 0);
    adjust_ar_rows(ctx, 0);

    ctx->ma_order_inc = 0;
    ctx->ar_order_inc = 0;
}
/* ========================================================================= */
/* Helper: fill an intervention structure from dialog values */
/* ========================================================================= */
static void fill_intervention_from_dialog(int idx, FueContext *ctx) {
    GtkWidget *type_combo = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), "type_combo");
    GtkWidget *period_spin = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), "period_spin");
    GtkWidget *year_spin = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), "year_spin");
    GtkWidget *ma_order_spin = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), "ma_order_spin");
    GtkWidget *ar_order_spin = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), "ar_order_spin");
    GtkWidget *ma_spin0 = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), "ma_spinbutton_0");
    GtkWidget *ma_check0 = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), "ma_checkbutton_0");

    int type = gtk_combo_box_get_active(GTK_COMBO_BOX(type_combo));
    int period = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(period_spin));
    int year = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(year_spin));
    int ma_order = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ma_order_spin));
    int ar_order = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ar_order_spin));

    It[idx].type = type;
    It[idx].period = period;
    It[idx].year = year;
    It[idx].ma_order = ma_order;
    It[idx].ar_order = ar_order;

    /* Free old vectors if any */
    if (It[idx].ma_parameter) free_vector(It[idx].ma_parameter, 0, It[idx].ma_order);
    if (It[idx].ma_fixed) free_ivector(It[idx].ma_fixed, 0, It[idx].ma_order);
    It[idx].ma_parameter = vector(0, ma_order + 1);
    It[idx].ma_fixed = ivector(0, ma_order + 1);
    It[idx].ma_parameter[0] = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ma_spin0));
    It[idx].ma_fixed[0] = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ma_check0)) ? 0 : 1;

    for (int i = 1; i <= ma_order; i++) {
        char name[32];
        sprintf(name, "ma_spinbutton_%d", i);
        GtkWidget *spin = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), name);
        sprintf(name, "ma_checkbutton_%d", i);
        GtkWidget *chk = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), name);
        It[idx].ma_parameter[i] = gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin));
        It[idx].ma_fixed[i] = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chk)) ? 0 : 1;
    }

    if (ar_order > 0) {
        if (It[idx].ar_parameter) free_vector(It[idx].ar_parameter, 1, It[idx].ar_order);
        if (It[idx].ar_fixed) free_ivector(It[idx].ar_fixed, 1, It[idx].ar_order);
        It[idx].ar_parameter = vector(1, ar_order);
        It[idx].ar_fixed = ivector(1, ar_order);
        for (int i = 1; i <= ar_order; i++) {
            char name[32];
            sprintf(name, "ar_spinbutton_%d", i);
            GtkWidget *spin = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), name);
            sprintf(name, "ar_checkbutton_%d", i);
            GtkWidget *chk = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), name);
            It[idx].ar_parameter[i] = gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin));
            It[idx].ar_fixed[i] = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chk)) ? 0 : 1;
        }
    } else {
        It[idx].ar_parameter = NULL;
        It[idx].ar_fixed = NULL;
    }
}

/* ========================================================================= */
/* Save a new intervention (appended at the end) */
/* ========================================================================= */
void save_new_int(FueContext *ctx) {
    fill_intervention_from_dialog(NdetVar, ctx);

    /* Add to tree view */
    GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(ctx->int_treeview)));
    GtkTreeIter iter;
    gtk_list_store_append(store, &iter);
    const char *type_name = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(
        g_object_get_data(G_OBJECT(ctx->esp_int_dialog), "type_combo")));
    gtk_list_store_set(store, &iter,
                       COL_NUM, NdetVar + 1,
                       COL_NAME, type_name,
                       COL_SEASON, It[NdetVar].period,
                       COL_YEAR, It[NdetVar].year,
                       COL_MAOR, It[NdetVar].ma_order,
                       COL_AROR, It[NdetVar].ar_order,
                       -1);
    g_free((gchar*)type_name);
    NdetVar++;
}

/* ========================================================================= */
/* Edit an existing intervention (index from tree selection) */
/* ========================================================================= */
void edit_int(FueContext *ctx) {
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(ctx->int_treeview));
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (!gtk_tree_selection_get_selected(sel, &model, &iter)) return;

    int number;
    gtk_tree_model_get(model, &iter, COL_NUM, &number, -1);
    fill_intervention_from_dialog(number - 1, ctx);

    const char *type_name = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(
        g_object_get_data(G_OBJECT(ctx->esp_int_dialog), "type_combo")));
    gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                       COL_NAME, type_name,
                       COL_SEASON, It[number-1].period,
                       COL_YEAR, It[number-1].year,
                       COL_MAOR, It[number-1].ma_order,
                       COL_AROR, It[number-1].ar_order,
                       -1);
    g_free((gchar*)type_name);
}

/* ========================================================================= */
/* Insert a new intervention before the selected row */
/* ========================================================================= */
void save_insert_int(FueContext *ctx) {
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(ctx->int_treeview));
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (!gtk_tree_selection_get_selected(sel, &model, &iter)) return;

    int number;
    gtk_tree_model_get(model, &iter, COL_NUM, &number, -1);
    gtk_list_store_insert(GTK_LIST_STORE(model), &iter, number);
    gtk_tree_selection_select_iter(sel, &iter);
    reload_number_int(model, iter);   /* renumber all rows */

    /* Shift existing interventions down */
    down_ints(number, NdetVar);
    NdetVar++;

    /* Fill the new intervention at position number-1 */
    fill_intervention_from_dialog(number - 1, ctx);

    /* Update the tree view row */
    const char *type_name = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(
        g_object_get_data(G_OBJECT(ctx->esp_int_dialog), "type_combo")));
        gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                   COL_NAME, type_name,
                   COL_SEASON, It[number-1].period,
                   COL_YEAR, It[number-1].year,
                   COL_MAOR, It[number-1].ma_order,
                   COL_AROR, It[number-1].ar_order,
                   -1);
    g_free((gchar*)type_name);
}

/* ========================================================================= */
/* Reload the dialog with values from an existing intervention (for editing) */
/* ========================================================================= */
 void reload_int(FueContext *ctx, GtkTreeModel *model, GtkTreeIter iter) {
    int number, maor, aror, season, year;
    gtk_tree_model_get(model, &iter,
                       COL_NUM, &number,
                       COL_SEASON, &season,
                       COL_YEAR, &year,
                       COL_MAOR, &maor,
                       COL_AROR, &aror,
                       -1);

    GtkWidget *type_combo = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), "type_combo");
    GtkWidget *period_spin = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), "period_spin");
    GtkWidget *year_spin = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), "year_spin");
    GtkWidget *ma_order_spin = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), "ma_order_spin");
    GtkWidget *ar_order_spin = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), "ar_order_spin");
    GtkWidget *ma_spin0 = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), "ma_spinbutton_0");
    GtkWidget *ma_check0 = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), "ma_checkbutton_0");

    int type = It[number-1].type;
    gtk_combo_box_set_active(GTK_COMBO_BOX(type_combo), type);

    if (type < 6) {
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(period_spin), It[number-1].period);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(year_spin), It[number-1].year);
        gtk_widget_set_sensitive(year_spin, TRUE);
        gtk_widget_set_sensitive(ma_order_spin, TRUE);
    } else {
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(period_spin), It[number-1].freq);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(year_spin), It[number-1].year);
        gtk_widget_set_sensitive(year_spin, FALSE);
        gtk_widget_set_sensitive(ma_order_spin, FALSE);
    }

    /* MA order and parameters */
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ma_order_spin), It[number-1].ma_order);
    adjust_ma_rows(ctx, It[number-1].ma_order);   /* Forzar creación de filas */
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ma_spin0), It[number-1].ma_parameter[0]);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ma_check0), It[number-1].ma_fixed[0] == 0);
    for (int i = 1; i <= It[number-1].ma_order; i++) {
        char name[32];
        sprintf(name, "ma_spinbutton_%d", i);
        GtkWidget *spin = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), name);
        sprintf(name, "ma_checkbutton_%d", i);
        GtkWidget *chk = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), name);
        if (spin && chk) {
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), It[number-1].ma_parameter[i]);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk), It[number-1].ma_fixed[i] == 0);
        }
    }

    /* AR order and parameters */
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ar_order_spin), It[number-1].ar_order);
    adjust_ar_rows(ctx, It[number-1].ar_order);   /* Forzar creación de filas */
    for (int i = 1; i <= It[number-1].ar_order; i++) {
        char name[32];
        sprintf(name, "ar_spinbutton_%d", i);
        GtkWidget *spin = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), name);
        sprintf(name, "ar_checkbutton_%d", i);
        GtkWidget *chk = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), name);
        if (spin && chk) {
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), It[number-1].ar_parameter[i]);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk), It[number-1].ar_fixed[i] == 0);
        }
    }
}

/* ========================================================================= */
/* Type combo changed: handle sensitivity of year and MA order */
/* ========================================================================= */
void on_type_int_combobox_changed(GtkComboBox *combo, FueContext *ctx) {
    int type = gtk_combo_box_get_active(combo);
    GtkWidget *year_spin = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), "year_spin");
    GtkWidget *ma_order_spin = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), "ma_order_spin");
    GtkWidget *ar_frame = g_object_get_data(G_OBJECT(ctx->esp_int_dialog), "ar_int_frame");
    if (type >= 6) {
        gtk_widget_set_sensitive(year_spin, FALSE);
        gtk_widget_set_sensitive(ma_order_spin, FALSE);
        if (ar_frame) gtk_widget_set_sensitive(ar_frame, FALSE);
    } else {
        gtk_widget_set_sensitive(year_spin, TRUE);
        gtk_widget_set_sensitive(ma_order_spin, TRUE);
        if (ar_frame) gtk_widget_set_sensitive(ar_frame, TRUE);
    }
}

/* ========================================================================= */
/* Add seasonals button callback */
/* ========================================================================= */
void on_add_seasonals(GtkButton *button, FueContext *ctx) {
    if (Ts.freq == 1) {
        gtk_label_set_text(GTK_LABEL(ctx->status_label),
                           "Seasonals only available for quarterly/monthly data.");
        return;
    }
    int half = Ts.freq / 2;
    for (int i = 1; i < half; i++) {
        /* Cosine */
        It[NdetVar].type = 6;
        It[NdetVar].freq = i;
        It[NdetVar].ma_order = 0;
        It[NdetVar].ma_parameter = vector(0, 1);
        It[NdetVar].ma_parameter[0] = 0;
        It[NdetVar].ma_fixed = ivector(0, 1);
        It[NdetVar].ma_fixed[0] = 1;
        It[NdetVar].ar_order = 0;
        GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(ctx->int_treeview)));
        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           COL_NUM, NdetVar+1,
                           COL_NAME, "Cos",
                           COL_SEASON, i,
                           COL_MAOR, 0,
                           -1);
        NdetVar++;

        /* Sine */
        It[NdetVar].type = 7;
        It[NdetVar].freq = i;
        It[NdetVar].ma_order = 0;
        It[NdetVar].ma_parameter = vector(0, 1);
        It[NdetVar].ma_parameter[0] = 0;
        It[NdetVar].ma_fixed = ivector(0, 1);
        It[NdetVar].ma_fixed[0] = 1;
        It[NdetVar].ar_order = 0;
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           COL_NUM, NdetVar+1,
                           COL_NAME, "Sin",
                           COL_SEASON, i,
                           COL_MAOR, 0,
                           -1);
        NdetVar++;
    }
    /* Alter */
    It[NdetVar].type = 8;
    It[NdetVar].freq = half;
    It[NdetVar].ma_order = 0;
    It[NdetVar].ma_parameter = vector(0, 1);
    It[NdetVar].ma_parameter[0] = 0;
    It[NdetVar].ma_fixed = ivector(0, 1);
    It[NdetVar].ma_fixed[0] = 1;
    It[NdetVar].ar_order = 0;
    GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(ctx->int_treeview)));
    GtkTreeIter iter;
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       COL_NUM, NdetVar+1,
                       COL_NAME, "Alter",
                       COL_SEASON, half,
                       COL_MAOR, 0,
                       -1);
    NdetVar++;
}

/* ========================================================================= */
/* Dialog OK and Cancel handlers (legacy, kept for compatibility) */
/* ========================================================================= */
void on_ok_int_button_clicked(GtkButton *button, FueContext *ctx) {
    if (new_det == 0) save_new_int(ctx);
    else if (new_det == 1) edit_int(ctx);
    else if (new_det == 2) save_insert_int(ctx);
    else save_new_int(ctx);
    gtk_widget_hide(ctx->esp_int_dialog);
}

void on_cancel_int_button_clicked(GtkButton *button, FueContext *ctx) {
    gtk_widget_hide(ctx->esp_int_dialog);
}

/* ========================================================================= */
/* Remove intervention button callback */
/* ========================================================================= */
void on_remove_int(GtkButton *button, FueContext *ctx) {
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(ctx->int_treeview));
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
        int number;
        gtk_tree_model_get(model, &iter, COL_NUM, &number, -1);
        gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
        NdetVar--;
        up_ints(number-1, NdetVar);
        reload_number_int(model, iter);
    }
}

/* ========================================================================= */
/* Edit intervention button callback */
/* ========================================================================= */
void on_edit_int(GtkButton *button, FueContext *ctx) {
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(ctx->int_treeview));
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
        ensure_dialog_created(ctx);
        reload_int(ctx, model, iter);
        new_det = 1;
        gtk_widget_show(ctx->esp_int_dialog);
    }
}

/* ========================================================================= */
/* Insert intervention button callback */
/* ========================================================================= */
void on_insert_int(GtkButton *button, FueContext *ctx) {
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(ctx->int_treeview));
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
        ensure_dialog_created(ctx);
        new_int(ctx);
        new_det = 2;
        gtk_widget_show(ctx->esp_int_dialog);
    }
}

/* ========================================================================= */
/* Add deterministic button callback */
/* ========================================================================= */
void on_add_deterministic(GtkButton *button, FueContext *ctx) {
    ensure_dialog_created(ctx);
    new_int(ctx);
    new_det = 0;
    gtk_widget_show(ctx->esp_int_dialog);
}

/* ========================================================================= */
/* Tree view row activation callback (double-click) */
/* ========================================================================= */
void on_int_treeview_row_activated(GtkTreeView *tree_view, GtkTreePath *path,
                                   GtkTreeViewColumn *col, FueContext *ctx) {
    GtkTreeSelection *sel = gtk_tree_view_get_selection(tree_view);
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
        ensure_dialog_created(ctx);
        reload_int(ctx, model, iter);
        new_det = 1;
        gtk_widget_show(ctx->esp_int_dialog);
    }
}

/* ========================================================================= */
/* Up/down functions for intervention list */
/* ========================================================================= */
void up_ints(int number, int end) {
    for (int i = number; i < end; i++) {
        It[i] = It[i+1];
    }
}

void down_ints(int number, int end) {
    for (int i = end; i > number; i--) {
        It[i] = It[i-1];
    }
}
