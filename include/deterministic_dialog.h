/* deterministic_dialog.h */
#ifndef DETERMINISTIC_DIALOG_H
#define DETERMINISTIC_DIALOG_H

#include <gtk/gtk.h>
#include "fue_context.h"

void on_add_deterministic(GtkButton *button, FueContext *ctx);
void on_edit_int(GtkButton *button, FueContext *ctx);
void on_remove_int(GtkButton *button, FueContext *ctx);
void on_insert_int(GtkButton *button, FueContext *ctx);
void on_add_seasonals(GtkButton *button, FueContext *ctx);
void on_int_treeview_row_activated(GtkTreeView *treeview, GtkTreePath *path,
                                   GtkTreeViewColumn *col, FueContext *ctx);
void new_int(FueContext *ctx);
void save_new_int(FueContext *ctx);
void save_insert_int(FueContext *ctx);
void edit_int(FueContext *ctx);
void reload_int(FueContext *ctx, GtkTreeModel *model, GtkTreeIter iter);
void up_ints(int number, int end);
void down_ints(int number, int end);
//void reload_number_int(GtkTreeModel *model, GtkTreeIter iter);

/* These functions are called from within the dialog */
void on_ok_int_button_clicked(GtkButton *button, FueContext *ctx);
void on_cancel_int_button_clicked(GtkButton *button, FueContext *ctx);
void on_ma_order_int_spinbutton_value_changed(GtkSpinButton *spin, FueContext *ctx);
void on_ar_order_int_spinbutton_value_changed(GtkSpinButton *spin, FueContext *ctx);
void on_type_int_combobox_changed(GtkComboBox *combo, FueContext *ctx);

#endif
