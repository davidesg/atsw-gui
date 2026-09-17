#ifndef GUI_H
#define GUI_H

#include <gtk/gtk.h>

typedef struct {
    GtkWidget *window;
    GtkWidget *series_name_entry;
    GtkWidget *data_filechooserbutton;
    GtkWidget *workspace_filechooserbutton;
    GtkWidget *path_data_entry;
    GtkWidget *freq_data_combobox;
    GtkWidget *n_load_spinbutton;
    GtkWidget *first_period_spinbutton;
    GtkWidget *first_year_spinbutton;
    GtkWidget *box_cox_lambda_spinbutton;
    GtkWidget *box_cox_m_spinbutton;
    GtkWidget *nrdiff_spinbutton;
    GtkWidget *nadiff_spinbutton;
    GtkWidget *season_label;
    GtkWidget *year_label;
    GtkWidget *main_statusbar;

    /* Buttons */
    GtkWidget *help_button;
    GtkWidget *refresh_button;
    GtkWidget *quit_button;
    GtkWidget *save_button;
    GtkWidget *see_input_button;
    GtkWidget *see_output_button;
    GtkWidget *plot_button;
    GtkWidget *acf_button;
    GtkWidget *hist_button;
    GtkWidget *mdt_button;
    GtkWidget *iden_button;

    /* Dialogs */
    GtkWidget *mdt_window;
    GtkWidget *acf_dialog;
    GtkWidget *stand_plot_dialog;
    GtkWidget *output_dialog;
    GtkWidget *iden_dialog;

    /* MDT dialog widgets */
    GtkWidget *mdt_entry_spinbutton;
    GtkWidget *default_mdt_checkbutton;

    /* ACF dialog widgets */
    GtkWidget *lags_acf_spinbutton;
    GtkWidget *nparma_acf_spinbutton;
    GtkWidget *cbands_acf_spinbutton;
    GtkWidget *default_acf_checkbutton;

    /* Plot dialog widgets */
    GtkWidget *with_acf_plot_checkbutton;
    GtkWidget *lags_acf_plot_spinbutton;
    GtkWidget *nparma_acf_plot_spinbutton;
    GtkWidget *cbands_acf_plot_spinbutton;
    GtkWidget *default_plot_checkbutton;

    /* Output dialog widgets */
    GtkWidget *option_output_dialog_combobox;
    GtkWidget *lags_output_spinbutton;
    GtkWidget *nog_output_spinbutton;
    GtkWidget *nrdiff_output_spinbutton;
    GtkWidget *nadiff_output_spinbutton;

    /* IDEN dialog widgets */
    GtkWidget *lags_acf_iden_spinbutton;
    GtkWidget *nrdiff_iden_spinbutton;
    GtkWidget *nadiff_iden_spinbutton;
    GtkWidget *nog_iden_spinbutton;
    GtkWidget *with_mdt_iden_checkbutton;
    GtkWidget *no_level_iden_checkbutton;
    GtkWidget *default_iden_checkbutton;

    /* Notebooks of the option dialogs (insensitive with "Default Options") */
    GtkWidget *options_plot_notebook;
    GtkWidget *options_acf_notebook;
    GtkWidget *options_iden_notebook;

} AppWidgets;

#endif
