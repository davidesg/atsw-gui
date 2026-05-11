#ifndef FUE_CONTEXT_H
#define FUE_CONTEXT_H

#include <gtk/gtk.h>

typedef struct {
    /* Main window widgets */
    GtkWidget *main_window;
    GtkWidget *status_label;
    GtkWidget *text_view;
    GtkWidget *btn_run;
    GtkWidget *btn_save;
    GtkWidget *btn_execute;

    /* Data specification widgets */
    GtkWidget *series_name_entry;
    GtkWidget *freq_combo;
    GtkWidget *n_obs_spin;
    GtkWidget *start_period_spin;
    GtkWidget *start_year_spin;
    GtkWidget *data_file_chooser;
    GtkWidget *workspace_file_chooser;
    GtkWidget *input_name_entry;

    /* Box-Cox and differences */
    GtkWidget *boxcox_lambda_spin;
    GtkWidget *boxcox_m_spin;
    GtkWidget *nrdiff_spin;
    GtkWidget *nadiff_spin;
    GtkWidget *f0_check, *f1_check, *f2_check, *f3_check, *f4_check, *f5_check, *f6_check;

    /* Deterministic component treeview */
    GtkWidget *int_treeview;

    /* Stochastic component treeviews */
    GtkWidget *arr_treeview;
    GtkWidget *ara_treeview;
    GtkWidget *mar_treeview;
    GtkWidget *maa_treeview;
    GtkWidget *ar_fix_treeview;
    GtkWidget *ma_fix_treeview;

    /* Mean and rescaling */
    GtkWidget *mean_spin;
    GtkWidget *mean_check;
    GtkWidget *refactor_spin;

    /* Internal state flags (from callbacks.c) */
    int ma_order_inc;
    int ar_order_inc;
    int new_det;
    int type_op;
    int new_op;

    int edit_index;
    int insert_position;

    /* Dialog windows */
    GtkWidget *esp_int_dialog;
    GtkWidget *esp_op_dialog;
    GtkWidget *esp_fix_dialog;

    /* Forecast tab widgets */
    GtkWidget *forecast_file_chooser;
    GtkWidget *forecast_load_button;
    GtkWidget *forecast_save_inp_button;
    GtkWidget *forecast_run_button;
    GtkWidget *forecast_view_pdf_button;
    GtkWidget *forecast_editor;
    GtkWidget *forecast_status_label;
    char *forecast_current_base;          /* nombre base sin extensión */
    char *forecast_current_inp_path;      /* ruta completa del último .inp guardado */
    char *forecast_loaded_path;           /* ruta del último archivo cargado (puede ser .pre) */
    /* Console tab widgets */
    GtkWidget *console_text_view;
    GtkWidget *edit_inp_button;
    GtkWidget *save_inp_button;
    GtkWidget *model_label;

} FueContext;

#endif
