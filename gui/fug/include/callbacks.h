#ifndef CALLBACKS_H
#define CALLBACKS_H

#include <gtk/gtk.h>
#include "gui.h"

gboolean get_main(AppWidgets *app);
gboolean SaveInpFile(AppWidgets *app);

void on_save_button_clicked(GtkButton *button, gpointer user_data);
void on_refresh_button_clicked(GtkButton *button, gpointer user_data);
void on_help_button_clicked(GtkButton *button, gpointer user_data);
void on_plot_button_clicked(GtkButton *button, gpointer user_data);
void on_hist_button_clicked(GtkButton *button, gpointer user_data);
void on_mdt_button_clicked(GtkButton *button, gpointer user_data);
void on_ok_mdt_dialog_button_clicked(GtkButton *button, gpointer user_data);
void on_help_mdt_dialog_button_clicked(GtkButton *button, gpointer user_data);
void on_cancel_mdt_dialog_button_clicked(GtkButton *button, gpointer user_data);
void on_iden_button_clicked(GtkButton *button, gpointer user_data);
void on_see_input_button_clicked(GtkButton *button, gpointer user_data);
void on_see_output_button_clicked(GtkButton *button, gpointer user_data);
void on_ok_acf_button_clicked(GtkButton *button, gpointer user_data);
void on_cancel_acf_button_clicked(GtkButton *button, gpointer user_data);
void on_acf_button_clicked(GtkButton *button, gpointer user_data);
void on_cancel_plot_button_clicked(GtkButton *button, gpointer user_data);
void on_ok_plot_button_clicked(GtkButton *button, gpointer user_data);
void on_ok_output_dialog_button_clicked(GtkButton *button, gpointer user_data);
void on_cancel_output_dialog_button_clicked(GtkButton *button, gpointer user_data);
void on_default_plot_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data);
void on_default_acf_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data);
void on_option_output_dialog_combobox_changed(GtkComboBox *combo_box, gpointer user_data);
void on_freq_data_combobox_changed(GtkComboBox *combo_box, gpointer user_data);
void on_series_name_entry_changed(GtkEntry *entry, gpointer user_data);
void no_file_message(const gchar *filename, gpointer user_data);
void on_default_mdt_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data);
void on_default_iden_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data);
void on_cancel_iden_dialog_button_clicked(GtkButton *button, gpointer user_data);
void on_ok_iden_dialog_button_clicked(GtkButton *button, gpointer user_data);
void on_with_mdt_iden_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data);
void on_data_filechooserbutton_file_set(GtkButton *button, gpointer user_data);

#endif
