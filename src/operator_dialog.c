/* operator_dialog.c – AR/MA operator dialogs (versión corregida) */
#include "operator_dialog.h"
#include "fue_globals.h"
#include "deterministic_dialog.h"   /* for reload_number_int */
#include "utils.h"
#include <math.h>
#include <string.h>

/* Forward declarations */
static struct oper* get_op_array(int type_op, int *count);
static struct freq_fix* get_fix_array(int type_op, int *count);
static GtkTreeView* get_op_treeview(FueContext *ctx, int type_op);
static GtkTreeView* get_fix_treeview(FueContext *ctx, int type_op);
static void new_ar(FueContext *ctx);
static void new_ar2f(FueContext *ctx);
static void save_new_op(FueContext *ctx, int type_op);
static void edit_op(FueContext *ctx, int type_op, int index);
static void save_insert_op(FueContext *ctx, int type_op, int position);

static void save_new_ar2f(FueContext *ctx, int type_op);
static void edit_ar2f(FueContext *ctx, int type_op, int index);
static void save_insert_ar2f(FueContext *ctx, int type_op, int position);

static void on_op_dialog_response(GtkDialog *dialog, gint response_id, FueContext *ctx);
static void on_fix_dialog_response(GtkDialog *dialog, gint response_id, FueContext *ctx);
static void on_ar_order_spinbutton_value_changed(GtkSpinButton *spin, FueContext *ctx);
static int count_restrictions_from_dialog(FueContext *ctx, int order);

/* ========================================================================= */
/* Helper functions to access global arrays based on operator type          */
/* ========================================================================= */
static struct oper* get_op_array(int type_op, int *count) {
    switch (type_op) {
        case 1: *count = NopArr; return Arr;
        case 2: *count = NopAra; return Ara;
        case 3: *count = NopMar; return Mar;
        case 4: *count = NopMaa; return Maa;
        default: *count = 0; return NULL;
    }
}

static struct freq_fix* get_fix_array(int type_op, int *count) {
    if (type_op == 5) { *count = NumAr2f; return Ar2f; }
    if (type_op == 6) { *count = NumMa2f; return Ma2f; }
    *count = 0;
    return NULL;
}

static GtkTreeView* get_op_treeview(FueContext *ctx, int type_op) {
    switch (type_op) {
        case 1: return GTK_TREE_VIEW(ctx->arr_treeview);
        case 2: return GTK_TREE_VIEW(ctx->ara_treeview);
        case 3: return GTK_TREE_VIEW(ctx->mar_treeview);
        case 4: return GTK_TREE_VIEW(ctx->maa_treeview);
        default: return NULL;
    }
}

static GtkTreeView* get_fix_treeview(FueContext *ctx, int type_op) {
    if (type_op == 5) return GTK_TREE_VIEW(ctx->ar_fix_treeview);
    if (type_op == 6) return GTK_TREE_VIEW(ctx->ma_fix_treeview);
    return NULL;
}

/* ========================================================================= */
/* Dialog creation functions                                                */
/* ========================================================================= */
void ensure_operator_dialog(FueContext *ctx) {
    if (ctx->esp_op_dialog) return;

    ctx->esp_op_dialog = gtk_dialog_new_with_buttons("Stochastic Operator",
                                                     GTK_WINDOW(ctx->main_window),
                                                     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                     "_Cancel", GTK_RESPONSE_CANCEL,
                                                     "_OK", GTK_RESPONSE_OK,
                                                     NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(ctx->esp_op_dialog), GTK_RESPONSE_OK);
    gtk_window_set_default_size(GTK_WINDOW(ctx->esp_op_dialog), 450, 350);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(ctx->esp_op_dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 10);

    GtkWidget *frame = gtk_frame_new("Operator Specification");
    gtk_frame_set_label_align(GTK_FRAME(frame), 0.0, 0.5);
    gtk_container_add(GTK_CONTAINER(content), frame);
    gtk_widget_set_vexpand(frame, TRUE);
    gtk_widget_set_hexpand(frame, TRUE);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
    gtk_container_add(GTK_CONTAINER(frame), vbox);
    gtk_widget_set_vexpand(vbox, TRUE);
    gtk_widget_set_hexpand(vbox, TRUE);

    /* Order selection */
    GtkWidget *order_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), order_hbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(order_hbox), gtk_label_new("Order:"), FALSE, FALSE, 0);
    GtkWidget *order_spin = gtk_spin_button_new_with_range(1, 20, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(order_spin), 1);
    g_signal_connect(order_spin, "value-changed", G_CALLBACK(on_ar_order_spinbutton_value_changed), ctx);
    gtk_box_pack_start(GTK_BOX(order_hbox), order_spin, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(ctx->esp_op_dialog), "order_spin", order_spin);

    /* Headers */
    GtkWidget *header_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_set_homogeneous(GTK_BOX(header_hbox), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), header_hbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header_hbox), gtk_label_new("Order"), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(header_hbox), gtk_label_new("Preliminary Estimates"), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(header_hbox), gtk_label_new("Check to Fix"), TRUE, FALSE, 0);

    /* Dynamic area for rows */
    GtkWidget *ar_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_pack_start(GTK_BOX(vbox), ar_vbox, TRUE, TRUE, 0);
    gtk_widget_set_vexpand(ar_vbox, TRUE);
    g_object_set_data(G_OBJECT(ctx->esp_op_dialog), "ar_vbox", ar_vbox);

    /* First row (order 1) */
    GtkWidget *first_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_set_homogeneous(GTK_BOX(first_row), TRUE);
    gtk_box_pack_start(GTK_BOX(ar_vbox), first_row, FALSE, FALSE, 0);
    GtkWidget *label1 = gtk_label_new("1");
    gtk_box_pack_start(GTK_BOX(first_row), label1, TRUE, TRUE, 0);
    GtkWidget *spin1 = gtk_spin_button_new_with_range(-10, 10, 0.1);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin1), 6);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin1), 0.0);
    gtk_box_pack_start(GTK_BOX(first_row), spin1, TRUE, TRUE, 0);
    GtkWidget *check1 = gtk_check_button_new();
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check1), FALSE);
    gtk_box_pack_start(GTK_BOX(first_row), check1, TRUE, FALSE, 0);
    g_object_set_data(G_OBJECT(ctx->esp_op_dialog), "ar_spinbutton_1", spin1);
    g_object_set_data(G_OBJECT(ctx->esp_op_dialog), "ar_checkbutton_1", check1);
    g_object_set_data(G_OBJECT(ctx->esp_op_dialog), "ar_hbox_1", first_row);

    g_signal_connect(ctx->esp_op_dialog, "response", G_CALLBACK(on_op_dialog_response), ctx);
    gtk_widget_show_all(ctx->esp_op_dialog);
}

void ensure_fixed_dialog(FueContext *ctx) {
    if (ctx->esp_fix_dialog) return;

    ctx->esp_fix_dialog = gtk_dialog_new_with_buttons("Frequency Fixed Operator",
                                                      GTK_WINDOW(ctx->main_window),
                                                      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                      "_Cancel", GTK_RESPONSE_CANCEL,
                                                      "_OK", GTK_RESPONSE_OK,
                                                      NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(ctx->esp_fix_dialog), GTK_RESPONSE_OK);
    gtk_window_set_default_size(GTK_WINDOW(ctx->esp_fix_dialog), 400, 200);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(ctx->esp_fix_dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 10);

    GtkWidget *frame = gtk_frame_new("Operator Specification");
    gtk_frame_set_label_align(GTK_FRAME(frame), 0.0, 0.5);
    gtk_container_add(GTK_CONTAINER(content), frame);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

    /* Order (fixed) */
    GtkWidget *order_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), order_hbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(order_hbox), gtk_label_new("Order:"), FALSE, FALSE, 0);
    GtkWidget *order_label = gtk_label_new("2");
    gtk_box_pack_start(GTK_BOX(order_hbox), order_label, FALSE, FALSE, 0);

    /* Frequency */
    GtkWidget *freq_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), freq_hbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(freq_hbox), gtk_label_new("Frequency:"), FALSE, FALSE, 0);
    GtkWidget *freq_spin = gtk_spin_button_new_with_range(1, 12, 1);
    gtk_box_pack_start(GTK_BOX(freq_hbox), freq_spin, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(ctx->esp_fix_dialog), "freq_spin", freq_spin);

    /* Parameter */
    GtkWidget *param_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), param_hbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(param_hbox), gtk_label_new("Pre-estimation Value:"), FALSE, FALSE, 0);
    GtkWidget *param_spin = gtk_spin_button_new_with_range(-1, 0, 0.1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(param_spin), -0.8);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(param_spin), 6);
    gtk_box_pack_start(GTK_BOX(param_hbox), param_spin, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(param_hbox), gtk_label_new("Fixed Option:"), FALSE, FALSE, 0);
    GtkWidget *param_check = gtk_check_button_new();
    gtk_box_pack_start(GTK_BOX(param_hbox), param_check, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(ctx->esp_fix_dialog), "param_spin", param_spin);
    g_object_set_data(G_OBJECT(ctx->esp_fix_dialog), "param_check", param_check);

    g_signal_connect(ctx->esp_fix_dialog, "response", G_CALLBACK(on_fix_dialog_response), ctx);
    gtk_widget_show_all(ctx->esp_fix_dialog);
}

/* ========================================================================= */
/* Dynamic row handling for operator dialog                                 */
/* ========================================================================= */
static void adjust_ar_rows(FueContext *ctx, int new_order) {
    GtkWidget *ar_vbox = g_object_get_data(G_OBJECT(ctx->esp_op_dialog), "ar_vbox");
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

            GtkWidget *spin = gtk_spin_button_new_with_range(-10, 10, 0.1);
            gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 6);
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), 0.0);
            gtk_box_pack_start(GTK_BOX(row), spin, TRUE, TRUE, 0);
            gtk_widget_show(spin);

            GtkWidget *check = gtk_check_button_new();
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), FALSE);
            gtk_box_pack_start(GTK_BOX(row), check, TRUE, FALSE, 0);
            gtk_widget_show(check);

            char name[32];
            sprintf(name, "ar_spinbutton_%d", i);
            g_object_set_data(G_OBJECT(ctx->esp_op_dialog), name, spin);
            sprintf(name, "ar_checkbutton_%d", i);
            g_object_set_data(G_OBJECT(ctx->esp_op_dialog), name, check);
            sprintf(name, "ar_hbox_%d", i);
            g_object_set_data(G_OBJECT(ctx->esp_op_dialog), name, row);
        }
    } else if (new_order < current_rows) {
        for (int i = current_rows; i > new_order; i--) {
            char name[32];
            sprintf(name, "ar_hbox_%d", i);
            GtkWidget *row = g_object_get_data(G_OBJECT(ctx->esp_op_dialog), name);
            if (row) {
                gtk_widget_destroy(row);
                g_object_set_data(G_OBJECT(ctx->esp_op_dialog), name, NULL);
                g_object_set_data(G_OBJECT(ctx->esp_op_dialog), g_strdup_printf("ar_spinbutton_%d", i), NULL);
                g_object_set_data(G_OBJECT(ctx->esp_op_dialog), g_strdup_printf("ar_checkbutton_%d", i), NULL);
            }
        }
    }
}

void on_ar_order_spinbutton_value_changed(GtkSpinButton *spin, FueContext *ctx) {
    int order = gtk_spin_button_get_value_as_int(spin);
    adjust_ar_rows(ctx, order);
}

/* ========================================================================= */
/* Dialog initialization helpers                                            */
/* ========================================================================= */
static void new_ar(FueContext *ctx) {
    GtkWidget *order_spin = g_object_get_data(G_OBJECT(ctx->esp_op_dialog), "order_spin");
    // Bloquear señal para evitar llamadas recursivas
    g_signal_handlers_block_by_func(order_spin, on_ar_order_spinbutton_value_changed, ctx);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(order_spin), 1);
    g_signal_handlers_unblock_by_func(order_spin, on_ar_order_spinbutton_value_changed, ctx);

    // Limpiar filas excepto la primera
    GtkWidget *ar_vbox = g_object_get_data(G_OBJECT(ctx->esp_op_dialog), "ar_vbox");
    GList *children = gtk_container_get_children(GTK_CONTAINER(ar_vbox));
    int i = 1;
    for (GList *l = children; l; l = l->next) {
        if (i > 1) gtk_widget_destroy(GTK_WIDGET(l->data));
        i++;
    }
    g_list_free(children);

    // Resetear primera fila
    GtkWidget *spin1 = g_object_get_data(G_OBJECT(ctx->esp_op_dialog), "ar_spinbutton_1");
    GtkWidget *check1 = g_object_get_data(G_OBJECT(ctx->esp_op_dialog), "ar_checkbutton_1");
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin1), 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check1), FALSE);
}

static void new_ar2f(FueContext *ctx) {
    GtkWidget *freq_spin = g_object_get_data(G_OBJECT(ctx->esp_fix_dialog), "freq_spin");
    GtkWidget *param_spin = g_object_get_data(G_OBJECT(ctx->esp_fix_dialog), "param_spin");
    GtkWidget *param_check = g_object_get_data(G_OBJECT(ctx->esp_fix_dialog), "param_check");
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(freq_spin), 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(param_spin), -0.8);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(param_check), FALSE);
}

/* ========================================================================= */
/* Función auxiliar para contar restricciones desde el diálogo               */
/* ========================================================================= */
static int count_restrictions_from_dialog(FueContext *ctx, int order) {
    int restrictions = 0;
    for (int i = 1; i <= order; i++) {
        char name[32];
        sprintf(name, "ar_checkbutton_%d", i);
        GtkWidget *check = g_object_get_data(G_OBJECT(ctx->esp_op_dialog), name);
        if (check && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check)))
            restrictions++;
    }
    return restrictions;
}

/* ========================================================================= */
/* Save, edit, insert, reload functions for operators                       */
/* ========================================================================= */
static void save_new_op(FueContext *ctx, int type_op) {
    GtkWidget *order_spin = g_object_get_data(G_OBJECT(ctx->esp_op_dialog), "order_spin");
    int order = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(order_spin));
    GtkWidget *spin1 = g_object_get_data(G_OBJECT(ctx->esp_op_dialog), "ar_spinbutton_1");
    GtkWidget *check1 = g_object_get_data(G_OBJECT(ctx->esp_op_dialog), "ar_checkbutton_1");
    double param1 = gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin1));
    int fixed1 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check1)) ? 0 : 1;

    int count;
    struct oper *op = get_op_array(type_op, &count);
    if (!op) return;

    op[count].type = type_op;
    op[count].order = order;
    op[count].op_parameter = vector(1, order);
    op[count].op_fixed = ivector(1, order);
    op[count].op_parameter[1] = param1;
    op[count].op_fixed[1] = fixed1;

    for (int i = 2; i <= order; i++) {
        char name[32];
        sprintf(name, "ar_spinbutton_%d", i);
        GtkWidget *spin = g_object_get_data(G_OBJECT(ctx->esp_op_dialog), name);
        sprintf(name, "ar_checkbutton_%d", i);
        GtkWidget *check = g_object_get_data(G_OBJECT(ctx->esp_op_dialog), name);
        op[count].op_parameter[i] = gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin));
        op[count].op_fixed[i] = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check)) ? 0 : 1;
    }

    int restrictions = count_restrictions_from_dialog(ctx, order);

    GtkTreeView *treeview = get_op_treeview(ctx, type_op);
    if (treeview) {
        GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(treeview));
        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);
        char *restriction_str = g_strdup_printf("%d", restrictions);
        gtk_list_store_set(store, &iter, 0, count+1, 1, restriction_str, 2, order, -1);
        g_free(restriction_str);
    }
    if (type_op == 1) NopArr++;
    else if (type_op == 2) NopAra++;
    else if (type_op == 3) NopMar++;
    else if (type_op == 4) NopMaa++;
}

static void edit_op(FueContext *ctx, int type_op, int index) {
    int count;
    struct oper *op = get_op_array(type_op, &count);
    if (!op || index < 0 || index >= count) return;

    GtkWidget *order_spin = g_object_get_data(G_OBJECT(ctx->esp_op_dialog), "order_spin");
    int order = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(order_spin));
    op[index].order = order;

    // Liberar vectores antiguos y asignar nuevos
    free_vector(op[index].op_parameter, 1, op[index].order);
    free_ivector(op[index].op_fixed, 1, op[index].order);
    op[index].op_parameter = vector(1, order);
    op[index].op_fixed = ivector(1, order);

    GtkWidget *spin1 = g_object_get_data(G_OBJECT(ctx->esp_op_dialog), "ar_spinbutton_1");
    GtkWidget *check1 = g_object_get_data(G_OBJECT(ctx->esp_op_dialog), "ar_checkbutton_1");
    op[index].op_parameter[1] = gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin1));
    op[index].op_fixed[1] = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check1)) ? 0 : 1;

    for (int i = 2; i <= order; i++) {
        char name[32];
        sprintf(name, "ar_spinbutton_%d", i);
        GtkWidget *spin = g_object_get_data(G_OBJECT(ctx->esp_op_dialog), name);
        sprintf(name, "ar_checkbutton_%d", i);
        GtkWidget *check = g_object_get_data(G_OBJECT(ctx->esp_op_dialog), name);
        op[index].op_parameter[i] = gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin));
        op[index].op_fixed[i] = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check)) ? 0 : 1;
    }

    // Calcular restricciones actuales desde el diálogo
    int restrictions = count_restrictions_from_dialog(ctx, order);

    // Actualizar treeview: columna 1 (Restriction) y columna 2 (Order)
    GtkTreeView *treeview = get_op_treeview(ctx, type_op);
    if (treeview) {
        GtkTreeModel *model = gtk_tree_view_get_model(treeview);
        GtkTreeIter iter;
        if (gtk_tree_model_iter_nth_child(model, &iter, NULL, index)) {
            char *restriction_str = g_strdup_printf("%d", restrictions);
            gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                               1, restriction_str,
                               2, order,
                               -1);
            g_free(restriction_str);
        }
    }
}

static void save_insert_op(FueContext *ctx, int type_op, int position) {
    int count;
    struct oper *op = get_op_array(type_op, &count);
    if (!op) return;

    // Desplazar hacia abajo
    for (int i = count; i > position; i--) {
        op[i] = op[i-1];
    }

    GtkWidget *order_spin = g_object_get_data(G_OBJECT(ctx->esp_op_dialog), "order_spin");
    int order = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(order_spin));
    op[position].type = type_op;
    op[position].order = order;
    op[position].op_parameter = vector(1, order);
    op[position].op_fixed = ivector(1, order);

    GtkWidget *spin1 = g_object_get_data(G_OBJECT(ctx->esp_op_dialog), "ar_spinbutton_1");
    GtkWidget *check1 = g_object_get_data(G_OBJECT(ctx->esp_op_dialog), "ar_checkbutton_1");
    op[position].op_parameter[1] = gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin1));
    op[position].op_fixed[1] = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check1)) ? 0 : 1;

    for (int i = 2; i <= order; i++) {
        char name[32];
        sprintf(name, "ar_spinbutton_%d", i);
        GtkWidget *spin = g_object_get_data(G_OBJECT(ctx->esp_op_dialog), name);
        sprintf(name, "ar_checkbutton_%d", i);
        GtkWidget *check = g_object_get_data(G_OBJECT(ctx->esp_op_dialog), name);
        op[position].op_parameter[i] = gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin));
        op[position].op_fixed[i] = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check)) ? 0 : 1;
    }

    // Calcular restricciones desde el diálogo
    int restrictions = count_restrictions_from_dialog(ctx, order);

    // Actualizar treeview: insertar fila y luego renumerar
    GtkTreeView *treeview = get_op_treeview(ctx, type_op);
    if (treeview) {
        GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(treeview));
        GtkTreeIter iter;
        gtk_list_store_insert(store, &iter, position);
        char *restriction_str = g_strdup_printf("%d", restrictions);
        gtk_list_store_set(store, &iter,
                           0, position + 1,          // Número temporal
                           1, restriction_str,
                           2, order,
                           -1);
        g_free(restriction_str);
        reload_number_int(GTK_TREE_MODEL(store), iter);  // Renumera todas las filas
    }
    if (type_op == 1) NopArr++;
    else if (type_op == 2) NopAra++;
    else if (type_op == 3) NopMar++;
    else if (type_op == 4) NopMaa++;
}

void reload_op(FueContext *ctx, int type_op, int index) {
    int count;
    struct oper *op = get_op_array(type_op, &count);
    if (!op || index < 0 || index >= count) return;

    GtkWidget *order_spin = g_object_get_data(G_OBJECT(ctx->esp_op_dialog), "order_spin");
    int order = op[index].order;

    // Bloquear señal para evitar recursión
    g_signal_handlers_block_by_func(order_spin, on_ar_order_spinbutton_value_changed, ctx);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(order_spin), order);
    g_signal_handlers_unblock_by_func(order_spin, on_ar_order_spinbutton_value_changed, ctx);

    // Ajustar filas dinámicas sin disparar señal
    adjust_ar_rows(ctx, order);

    // Establecer valores de la primera fila
    GtkWidget *spin1 = g_object_get_data(G_OBJECT(ctx->esp_op_dialog), "ar_spinbutton_1");
    GtkWidget *check1 = g_object_get_data(G_OBJECT(ctx->esp_op_dialog), "ar_checkbutton_1");
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin1), op[index].op_parameter[1]);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check1), op[index].op_fixed[1] == 0);

    // Establecer valores de filas adicionales
    for (int i = 2; i <= order; i++) {
        char name[32];
        sprintf(name, "ar_spinbutton_%d", i);
        GtkWidget *spin = g_object_get_data(G_OBJECT(ctx->esp_op_dialog), name);
        sprintf(name, "ar_checkbutton_%d", i);
        GtkWidget *check = g_object_get_data(G_OBJECT(ctx->esp_op_dialog), name);
        if (spin && check) {
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), op[index].op_parameter[i]);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), op[index].op_fixed[i] == 0);
        }
    }
}

/* ========================================================================= */
/* Fixed-frequency operators (AR2f, MA2f)                                   */
/* ========================================================================= */
static void save_new_ar2f(FueContext *ctx, int type_op) {
    GtkWidget *freq_spin = g_object_get_data(G_OBJECT(ctx->esp_fix_dialog), "freq_spin");
    GtkWidget *param_spin = g_object_get_data(G_OBJECT(ctx->esp_fix_dialog), "param_spin");
    GtkWidget *param_check = g_object_get_data(G_OBJECT(ctx->esp_fix_dialog), "param_check");
    int freq = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(freq_spin));
    double param = gtk_spin_button_get_value(GTK_SPIN_BUTTON(param_spin));
    int fixed = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(param_check)) ? 0 : 1;

    int count;
    struct freq_fix *fix = get_fix_array(type_op, &count);
    if (!fix) return;

    fix[count].type = type_op;
    fix[count].freq = freq;
    fix[count].op_parameter = param;
    fix[count].op_fixed = fixed;

    GtkTreeView *treeview = get_fix_treeview(ctx, type_op);
    if (treeview) {
        GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(treeview));
        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);
        char *restriction_str = g_strdup_printf("%d", fixed);
        gtk_list_store_set(store, &iter, 0, count+1, 1, restriction_str, 2, freq, -1);
        g_free(restriction_str);
    }
    if (type_op == 5) NumAr2f++;
    else if (type_op == 6) NumMa2f++;
}

static void edit_ar2f(FueContext *ctx, int type_op, int index) {
    int count;
    struct freq_fix *fix = get_fix_array(type_op, &count);
    if (!fix || index < 0 || index >= count) return;

    GtkWidget *freq_spin = g_object_get_data(G_OBJECT(ctx->esp_fix_dialog), "freq_spin");
    GtkWidget *param_spin = g_object_get_data(G_OBJECT(ctx->esp_fix_dialog), "param_spin");
    GtkWidget *param_check = g_object_get_data(G_OBJECT(ctx->esp_fix_dialog), "param_check");
    fix[index].freq = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(freq_spin));
    fix[index].op_parameter = gtk_spin_button_get_value(GTK_SPIN_BUTTON(param_spin));
    fix[index].op_fixed = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(param_check)) ? 0 : 1;

    int fixed = fix[index].op_fixed;
    GtkTreeView *treeview = get_fix_treeview(ctx, type_op);
    if (treeview) {
        GtkTreeModel *model = gtk_tree_view_get_model(treeview);
        GtkTreeIter iter;
        if (gtk_tree_model_iter_nth_child(model, &iter, NULL, index)) {
            char *restriction_str = g_strdup_printf("%d", fixed);
            gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                               1, restriction_str,
                               2, fix[index].freq,
                               -1);
            g_free(restriction_str);
        }
    }
}

static void save_insert_ar2f(FueContext *ctx, int type_op, int position) {
    int count;
    struct freq_fix *fix = get_fix_array(type_op, &count);
    if (!fix) return;

    for (int i = count; i > position; i--) {
        fix[i] = fix[i-1];
    }

    GtkWidget *freq_spin = g_object_get_data(G_OBJECT(ctx->esp_fix_dialog), "freq_spin");
    GtkWidget *param_spin = g_object_get_data(G_OBJECT(ctx->esp_fix_dialog), "param_spin");
    GtkWidget *param_check = g_object_get_data(G_OBJECT(ctx->esp_fix_dialog), "param_check");
    fix[position].type = type_op;
    fix[position].freq = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(freq_spin));
    fix[position].op_parameter = gtk_spin_button_get_value(GTK_SPIN_BUTTON(param_spin));
    fix[position].op_fixed = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(param_check)) ? 0 : 1;

    int fixed = fix[position].op_fixed;
    GtkTreeView *treeview = get_fix_treeview(ctx, type_op);
    if (treeview) {
        GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(treeview));
        GtkTreeIter iter;
        gtk_list_store_insert(store, &iter, position);
        char *restriction_str = g_strdup_printf("%d", fixed);
        gtk_list_store_set(store, &iter,
                           0, position + 1,
                           1, restriction_str,
                           2, fix[position].freq,
                           -1);
        g_free(restriction_str);
        reload_number_int(GTK_TREE_MODEL(store), iter);
    }
    if (type_op == 5) NumAr2f++;
    else if (type_op == 6) NumMa2f++;
}

void reload_ar2f(FueContext *ctx, int type_op, int index) {
    int count;
    struct freq_fix *fix = get_fix_array(type_op, &count);
    if (!fix || index < 0 || index >= count) return;

    GtkWidget *freq_spin = g_object_get_data(G_OBJECT(ctx->esp_fix_dialog), "freq_spin");
    GtkWidget *param_spin = g_object_get_data(G_OBJECT(ctx->esp_fix_dialog), "param_spin");
    GtkWidget *param_check = g_object_get_data(G_OBJECT(ctx->esp_fix_dialog), "param_check");
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(freq_spin), fix[index].freq);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(param_spin), fix[index].op_parameter);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(param_check), fix[index].op_fixed == 0);
}

/* ========================================================================= */
/* Dialog response handlers                                                 */
/* ========================================================================= */
static void on_op_dialog_response(GtkDialog *dialog, gint response_id, FueContext *ctx) {
    if (response_id == GTK_RESPONSE_OK) {
        if (ctx->new_op == 0)
            save_new_op(ctx, ctx->type_op);
        else if (ctx->new_op == 1)
            edit_op(ctx, ctx->type_op, ctx->edit_index);
        else if (ctx->new_op == 2)
            save_insert_op(ctx, ctx->type_op, ctx->insert_position);
        else
            save_new_op(ctx, ctx->type_op);
    }
    gtk_widget_hide(GTK_WIDGET(dialog));
}

static void on_fix_dialog_response(GtkDialog *dialog, gint response_id, FueContext *ctx) {
    if (response_id == GTK_RESPONSE_OK) {
        if (ctx->new_op == 0)
            save_new_ar2f(ctx, ctx->type_op);
        else if (ctx->new_op == 1)
            edit_ar2f(ctx, ctx->type_op, ctx->edit_index);
        else if (ctx->new_op == 2)
            save_insert_ar2f(ctx, ctx->type_op, ctx->insert_position);
        else
            save_new_ar2f(ctx, ctx->type_op);
    }
    gtk_widget_hide(GTK_WIDGET(dialog));
}

/* ========================================================================= */
/* Public callback functions for toolbar buttons (to be called from main_window.c) */
/* ========================================================================= */
void on_add_operator(GtkButton *button, FueContext *ctx) {
    gpointer type = g_object_get_data(G_OBJECT(button), "op_type");
    if (type) {
        ctx->type_op = GPOINTER_TO_INT(type);
        ensure_operator_dialog(ctx);
        new_ar(ctx);
        ctx->new_op = 0;
        gtk_widget_show(ctx->esp_op_dialog);
    }
}

void on_edit_operator(GtkButton *button, FueContext *ctx) {
    GtkTreeView *treeview = g_object_get_data(G_OBJECT(button), "treeview");
    if (!treeview) return;
    GtkTreeSelection *sel = gtk_tree_view_get_selection(treeview);
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
        int number;
        gtk_tree_model_get(model, &iter, 0, &number, -1);
        ctx->edit_index = number - 1;
        gpointer type = g_object_get_data(G_OBJECT(button), "op_type");
        if (type) ctx->type_op = GPOINTER_TO_INT(type);
        ensure_operator_dialog(ctx);
        reload_op(ctx, ctx->type_op, ctx->edit_index);
        ctx->new_op = 1;
        gtk_widget_show(ctx->esp_op_dialog);
    }
}

void on_remove_operator(GtkButton *button, FueContext *ctx) {
    GtkTreeView *treeview = g_object_get_data(G_OBJECT(button), "treeview");
    if (!treeview) return;
    GtkTreeSelection *sel = gtk_tree_view_get_selection(treeview);
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
        int number;
        gtk_tree_model_get(model, &iter, 0, &number, -1);
        gpointer type = g_object_get_data(G_OBJECT(button), "op_type");
        if (!type) return;
        int type_op = GPOINTER_TO_INT(type);
        int count;
        struct oper *op = get_op_array(type_op, &count);
        if (op && number-1 < count) {
            gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
            for (int i = number-1; i < count-1; i++) {
                op[i] = op[i+1];
            }
            if (type_op == 1) NopArr--;
            else if (type_op == 2) NopAra--;
            else if (type_op == 3) NopMar--;
            else if (type_op == 4) NopMaa--;
            reload_number_int(model, iter);
        }
    }
}

void on_insert_operator(GtkButton *button, FueContext *ctx) {
    GtkTreeView *treeview = g_object_get_data(G_OBJECT(button), "treeview");
    if (!treeview) return;
    GtkTreeSelection *sel = gtk_tree_view_get_selection(treeview);
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
        int number;
        gtk_tree_model_get(model, &iter, 0, &number, -1);
        ctx->insert_position = number - 1;
        gpointer type = g_object_get_data(G_OBJECT(button), "op_type");
        if (type) ctx->type_op = GPOINTER_TO_INT(type);
        ensure_operator_dialog(ctx);
        new_ar(ctx);
        ctx->new_op = 2;
        gtk_widget_show(ctx->esp_op_dialog);
    }
}

/* Fixed-frequency callbacks */
void on_add_fixed(GtkButton *button, FueContext *ctx) {
    gpointer type = g_object_get_data(G_OBJECT(button), "op_type");
    if (type) {
        ctx->type_op = GPOINTER_TO_INT(type);
        ensure_fixed_dialog(ctx);
        new_ar2f(ctx);
        ctx->new_op = 0;
        gtk_widget_show(ctx->esp_fix_dialog);
    }
}

void on_edit_fixed(GtkButton *button, FueContext *ctx) {
    GtkTreeView *treeview = g_object_get_data(G_OBJECT(button), "treeview");
    if (!treeview) return;
    GtkTreeSelection *sel = gtk_tree_view_get_selection(treeview);
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
        int number;
        gtk_tree_model_get(model, &iter, 0, &number, -1);
        ctx->edit_index = number - 1;
        gpointer type = g_object_get_data(G_OBJECT(button), "op_type");
        if (type) ctx->type_op = GPOINTER_TO_INT(type);
        ensure_fixed_dialog(ctx);
        reload_ar2f(ctx, ctx->type_op, ctx->edit_index);
        ctx->new_op = 1;
        gtk_widget_show(ctx->esp_fix_dialog);
    }
}

void on_remove_fixed(GtkButton *button, FueContext *ctx) {
    GtkTreeView *treeview = g_object_get_data(G_OBJECT(button), "treeview");
    if (!treeview) return;
    GtkTreeSelection *sel = gtk_tree_view_get_selection(treeview);
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
        int number;
        gtk_tree_model_get(model, &iter, 0, &number, -1);
        gpointer type = g_object_get_data(G_OBJECT(button), "op_type");
        if (!type) return;
        int type_op = GPOINTER_TO_INT(type);
        int count;
        struct freq_fix *fix = get_fix_array(type_op, &count);
        if (fix && number-1 < count) {
            gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
            for (int i = number-1; i < count-1; i++) {
                fix[i] = fix[i+1];
            }
            if (type_op == 5) NumAr2f--;
            else if (type_op == 6) NumMa2f--;
            reload_number_int(model, iter);
        }
    }
}

void on_insert_fixed(GtkButton *button, FueContext *ctx) {
    GtkTreeView *treeview = g_object_get_data(G_OBJECT(button), "treeview");
    if (!treeview) return;
    GtkTreeSelection *sel = gtk_tree_view_get_selection(treeview);
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
        int number;
        gtk_tree_model_get(model, &iter, 0, &number, -1);
        ctx->insert_position = number - 1;
        gpointer type = g_object_get_data(G_OBJECT(button), "op_type");
        if (type) ctx->type_op = GPOINTER_TO_INT(type);
        ensure_fixed_dialog(ctx);
        new_ar2f(ctx);
        ctx->new_op = 2;
        gtk_widget_show(ctx->esp_fix_dialog);
    }
}
