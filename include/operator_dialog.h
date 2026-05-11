#ifndef OPERATOR_DIALOG_H
#define OPERATOR_DIALOG_H

#include <gtk/gtk.h>
#include "fue_context.h"

/* Callbacks for operator buttons (in the main window) */
void on_add_operator(GtkButton *button, FueContext *ctx);
void on_edit_operator(GtkButton *button, FueContext *ctx);
void on_remove_operator(GtkButton *button, FueContext *ctx);
void on_insert_operator(GtkButton *button, FueContext *ctx);

/* Callbacks for fixed-frequency operator buttons */
void on_add_fixed(GtkButton *button, FueContext *ctx);
void on_edit_fixed(GtkButton *button, FueContext *ctx);
void on_remove_fixed(GtkButton *button, FueContext *ctx);
void on_insert_fixed(GtkButton *button, FueContext *ctx);

/* Tree view activation callbacks (from main window) */
void on_arr_treeview_row_activated(GtkTreeView *treeview, GtkTreePath *path,
                                   GtkTreeViewColumn *col, FueContext *ctx);
void on_ara_treeview_row_activated(GtkTreeView *treeview, GtkTreePath *path,
                                   GtkTreeViewColumn *col, FueContext *ctx);
void on_mar_treeview_row_activated(GtkTreeView *treeview, GtkTreePath *path,
                                   GtkTreeViewColumn *col, FueContext *ctx);
void on_maa_treeview_row_activated(GtkTreeView *treeview, GtkTreePath *path,
                                   GtkTreeViewColumn *col, FueContext *ctx);
void on_ar_fix_treeview_row_activated(GtkTreeView *treeview, GtkTreePath *path,
                                      GtkTreeViewColumn *col, FueContext *ctx);
void on_ma_fix_treeview_row_activated(GtkTreeView *treeview, GtkTreePath *path,
                                      GtkTreeViewColumn *col, FueContext *ctx);

/* Functions to create and manage the dialogs */
void ensure_operator_dialog(FueContext *ctx);
void ensure_fixed_dialog(FueContext *ctx);

/* En operator_dialog.h, después de las otras declaraciones */
void reload_op(FueContext *ctx, int type_op, int index);
void reload_ar2f(FueContext *ctx, int type_op, int index);

#endif
