/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * main.c
 * Copyright (C) David Guerrero 2010 <warriord@rocketmail.com>
 *
 * Rewritten to use pure GTK without Glade.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <locale.h>

#include <gtk/gtk.h>

//#include "config.h"
#include "callbacks.h"
#include "proyecto.h"
#include "gui.h"
#include "fug_run.h"

/* For testing propose use the local (not installed) glade file */
/* #define GLADE_FILE PACKAGE_DATA_DIR"/gtk_fmg/glade/gtk_fmg.glade" */

/* Forward declarations for helper functions */
static void create_main_window(AppWidgets *app);
static void create_mdt_dialog(AppWidgets *app);
static void create_acf_dialog(AppWidgets *app);
static void create_plot_dialog(AppWidgets *app);
static void create_output_dialog(AppWidgets *app);
static void create_iden_dialog(AppWidgets *app);

/* --proyecto FICHERO: el espacio de trabajo sale de la RAIZ del proyecto.
 *
 * Es lo minimo que fug necesita de la interfaz madre. Hoy no guarda nada entre
 * ejecuciones, asi que cada arranque empieza preguntando donde esta todo.
 * Sin la opcion funciona como siempre.                                 */
static char g_raiz[1024];

static int lee_opciones(int argc, char *argv[])
{
    const char *proy = NULL;
    Proyecto   *p;
    PrError     e;
    int         i;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--proyecto") && i + 1 < argc) proy = argv[++i];
        else if (!strncmp(argv[i], "--proyecto=", 11)) proy = argv[i] + 11;
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            printf("uso: %s [--proyecto FICHERO]\n\n"
                   "  --proyecto F  el espacio de trabajo sale de la raiz\n"
                   "                del proyecto F.\n", argv[0]);
            return 1;
        }
    }
    if (!proy) return 0;

    p = calloc(1, sizeof *p);
    if (!p) return 0;
    if (pr_leer(proy, p, &e) != 0) {
        char why[512];

        /* Un manifiesto roto se dice y se para. */
        pr_error_es(&e, why, sizeof why);
        fprintf(stderr, "%s: %s\n", proy, why);
        free(p);
        return 2;
    }
    pr_ruta(p, "", NULL, NULL, g_raiz, sizeof g_raiz);
    if (!g_raiz[0]) {
        char *d = g_path_get_dirname(proy);

        snprintf(g_raiz, sizeof g_raiz, "%s", d);
        g_free(d);
    }
    free(p);
    return 0;
}

int main(int argc, char *argv[])
{
    int opt = lee_opciones(argc, argv);

    if (opt == 1) return 0;
    if (opt == 2) return 3;

#ifdef ENABLE_NLS
    bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);
#endif

    gtk_init(&argc, &argv);
    fug_run_init(argv[0]);

    /* Change decimal separator to point (POSIX): fug reads "%lf" */
    setlocale(LC_NUMERIC, "C");

    AppWidgets *app = g_slice_new0(AppWidgets);

    /* Create all windows and dialogs */
    create_main_window(app);
    create_mdt_dialog(app);
    create_acf_dialog(app);
    create_plot_dialog(app);
    create_output_dialog(app);
    create_iden_dialog(app);

    /* Closing an option dialog (Esc or window manager) only hides it */
    g_signal_connect(app->mdt_window, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
    g_signal_connect(app->acf_dialog, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
    g_signal_connect(app->stand_plot_dialog, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
    g_signal_connect(app->output_dialog, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
    g_signal_connect(app->iden_dialog, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);

    /* Connect signals for main window */
    g_signal_connect(app->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(app->save_button, "clicked", G_CALLBACK(on_save_button_clicked), app);
    g_signal_connect(app->refresh_button, "clicked", G_CALLBACK(on_refresh_button_clicked), app);
    g_signal_connect(app->help_button, "clicked", G_CALLBACK(on_help_button_clicked), app);
    g_signal_connect(app->quit_button, "clicked", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(app->see_input_button, "clicked", G_CALLBACK(on_see_input_button_clicked), app);
    g_signal_connect(app->see_output_button, "clicked", G_CALLBACK(on_see_output_button_clicked), app);
    g_signal_connect(app->plot_button, "clicked", G_CALLBACK(on_plot_button_clicked), app);
    g_signal_connect(app->acf_button, "clicked", G_CALLBACK(on_acf_button_clicked), app);
    g_signal_connect(app->hist_button, "clicked", G_CALLBACK(on_hist_button_clicked), app);
    g_signal_connect(app->mdt_button, "clicked", G_CALLBACK(on_mdt_button_clicked), app);
    g_signal_connect(app->iden_button, "clicked", G_CALLBACK(on_iden_button_clicked), app);
    g_signal_connect(app->freq_data_combobox, "changed", G_CALLBACK(on_freq_data_combobox_changed), app);
    g_signal_connect(app->series_name_entry, "changed", G_CALLBACK(on_series_name_entry_changed), app);
    g_signal_connect(app->data_filechooserbutton, "file-set", G_CALLBACK(on_data_filechooserbutton_file_set), app);

    /* Set initial values */
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->freq_data_combobox), 0);
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->option_output_dialog_combobox), 0);

    gtk_widget_show_all(app->window);
    /* Con --proyecto, el espacio de trabajo ya se sabe al arrancar. */
    if (g_raiz[0]) set_workspace(app, g_raiz);

    gtk_main();

    g_slice_free(AppWidgets, app);
    return 0;
}

/* ---------------------------------------------------------------------- */
/* Helper functions to build the interface                                */
/* ---------------------------------------------------------------------- */

static void create_main_window(AppWidgets *app)
{
    /* Main window */
    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->window), "FUG: Load and Save Data");
    gtk_window_set_default_size(GTK_WINDOW(app->window), 500, 400);
    gtk_container_set_border_width(GTK_CONTAINER(app->window), 6);

    GtkWidget *vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(app->window), vbox1);

    /* Series name row */
    GtkWidget *hbox6 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox6, FALSE, FALSE, 0);

    GtkWidget *label11 = gtk_label_new("Series Name: ");
    gtk_box_pack_start(GTK_BOX(hbox6), label11, FALSE, FALSE, 0);
    app->series_name_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox6), app->series_name_entry, TRUE, TRUE, 0);

    /* Data/Workspace/Input table */
    GtkWidget *vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(vbox1), vbox2, FALSE, FALSE, 0);

    /* Row 0: labels; row 1: data file | workspace | input name */
    GtkWidget *table1 = gtk_grid_new();
    gtk_grid_set_column_homogeneous(GTK_GRID(table1), TRUE);
    gtk_container_add(GTK_CONTAINER(vbox2), table1);
    gtk_grid_set_column_spacing(GTK_GRID(table1), 6);
    gtk_grid_set_row_spacing(GTK_GRID(table1), 2);

    GtkWidget *label10 = gtk_label_new("Data File:");
    gtk_widget_set_halign(label10, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(table1), label10, 0, 0, 1, 1);
    app->data_filechooserbutton = gtk_file_chooser_button_new("Select data file (.txt, .dat, .csv or .inp)",
                                                              GTK_FILE_CHOOSER_ACTION_OPEN);
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Data files (*.txt, *.dat, *.csv, *.inp)");
    gtk_file_filter_add_pattern(filter, "*.txt");
    gtk_file_filter_add_pattern(filter, "*.dat");
    gtk_file_filter_add_pattern(filter, "*.csv");
    gtk_file_filter_add_pattern(filter, "*.inp");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(app->data_filechooserbutton), filter);
    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "All files");
    gtk_file_filter_add_pattern(filter, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(app->data_filechooserbutton), filter);
    gtk_grid_attach(GTK_GRID(table1), app->data_filechooserbutton, 0, 1, 1, 1);

    GtkWidget *label12 = gtk_label_new("Workspace:");
    gtk_widget_set_halign(label12, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(table1), label12, 1, 0, 1, 1);
    app->workspace_filechooserbutton = gtk_file_chooser_button_new("Select workspace folder",
                                                                   GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    gtk_grid_attach(GTK_GRID(table1), app->workspace_filechooserbutton, 1, 1, 1, 1);

    GtkWidget *label13 = gtk_label_new("Input Name (without spaces):");
    gtk_widget_set_halign(label13, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(table1), label13, 2, 0, 1, 1);
    app->path_data_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(table1), app->path_data_entry, 2, 1, 1, 1);

    /* Frequency row */
    GtkWidget *hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 0);
    GtkWidget *label2 = gtk_label_new("Frequency of Time Series: ");
    gtk_box_pack_start(GTK_BOX(hbox1), label2, FALSE, FALSE, 0);
    app->freq_data_combobox = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->freq_data_combobox), "1  (annual or numbered)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->freq_data_combobox), "4  (quarterly)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->freq_data_combobox), "12 (monthly)");
    gtk_box_pack_start(GTK_BOX(hbox1), app->freq_data_combobox, FALSE, FALSE, 0);

    /* Observations row */
    GtkWidget *hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox2, FALSE, FALSE, 0);
    GtkWidget *label3 = gtk_label_new("Number of Observations:");
    gtk_box_pack_start(GTK_BOX(hbox2), label3, FALSE, FALSE, 0);
    app->n_load_spinbutton = gtk_spin_button_new_with_range(1, 99999, 1);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(app->n_load_spinbutton), TRUE);
    gtk_box_pack_start(GTK_BOX(hbox2), app->n_load_spinbutton, FALSE, FALSE, 0);

    app->season_label = gtk_label_new("Starting Season:");
    gtk_box_pack_start(GTK_BOX(hbox2), app->season_label, FALSE, FALSE, 0);
    app->first_period_spinbutton = gtk_spin_button_new_with_range(0, 100, 1);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(app->first_period_spinbutton), TRUE);
    gtk_box_pack_start(GTK_BOX(hbox2), app->first_period_spinbutton, FALSE, FALSE, 0);

    app->year_label = gtk_label_new("First Year or Number:");
    gtk_box_pack_start(GTK_BOX(hbox2), app->year_label, FALSE, FALSE, 0);
    app->first_year_spinbutton = gtk_spin_button_new_with_range(1, 4000, 1);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(app->first_year_spinbutton), TRUE);
    gtk_box_pack_start(GTK_BOX(hbox2), app->first_year_spinbutton, FALSE, FALSE, 0);

    /* Separator */
    GtkWidget *hseparator5 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox1), hseparator5, FALSE, FALSE, 0);

    /* Box-Cox row */
    GtkWidget *hbox3 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox3, FALSE, FALSE, 0);
    GtkWidget *label6 = gtk_label_new("Box-Cox Lambda:");
    gtk_box_pack_start(GTK_BOX(hbox3), label6, FALSE, FALSE, 0);
    app->box_cox_lambda_spinbutton = gtk_spin_button_new_with_range(-5, 5, 0.1);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(app->box_cox_lambda_spinbutton), 6);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->box_cox_lambda_spinbutton), 1.0);
    gtk_box_pack_start(GTK_BOX(hbox3), app->box_cox_lambda_spinbutton, FALSE, FALSE, 0);
    GtkWidget *label7 = gtk_label_new("Box-Cox m:");
    gtk_box_pack_start(GTK_BOX(hbox3), label7, FALSE, FALSE, 0);
    app->box_cox_m_spinbutton = gtk_spin_button_new_with_range(0, 1000000, 0.1);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(app->box_cox_m_spinbutton), 6);
    gtk_box_pack_start(GTK_BOX(hbox3), app->box_cox_m_spinbutton, FALSE, FALSE, 0);

    /* Differences row */
    GtkWidget *hbox4 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox4, FALSE, FALSE, 0);
    GtkWidget *label8 = gtk_label_new("Regular Differences:");
    gtk_box_pack_start(GTK_BOX(hbox4), label8, FALSE, FALSE, 0);
    app->nrdiff_spinbutton = gtk_spin_button_new_with_range(0, 100, 1);
    gtk_box_pack_start(GTK_BOX(hbox4), app->nrdiff_spinbutton, FALSE, FALSE, 0);
    GtkWidget *label9 = gtk_label_new("Annual Differences:");
    gtk_box_pack_start(GTK_BOX(hbox4), label9, FALSE, FALSE, 0);
    app->nadiff_spinbutton = gtk_spin_button_new_with_range(0, 100, 1);
    gtk_box_pack_start(GTK_BOX(hbox4), app->nadiff_spinbutton, FALSE, FALSE, 0);

    /* Separator */
    GtkWidget *hseparator6 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox1), hseparator6, FALSE, FALSE, 0);

    /* Notebook */
    GtkWidget *notebook1 = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(vbox1), notebook1, TRUE, TRUE, 0);

    /* Tab 1: Character Files */
    GtkWidget *hbuttonbox3 = gtk_hbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox3), GTK_BUTTONBOX_START);
    app->see_input_button = gtk_button_new_with_label("Input");
    app->see_output_button = gtk_button_new_with_label("Output");
    gtk_container_add(GTK_CONTAINER(hbuttonbox3), app->see_input_button);
    gtk_container_add(GTK_CONTAINER(hbuttonbox3), app->see_output_button);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook1), hbuttonbox3, gtk_label_new("Character Files"));

    /* Tab 2: High Resolution Output Single Options */
    GtkWidget *hbuttonbox4 = gtk_hbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox4), GTK_BUTTONBOX_END);
    app->plot_button = gtk_button_new_with_label("Ts Data Plot");
    app->acf_button = gtk_button_new_with_label("Acf/Pacf Plots");
    app->hist_button = gtk_button_new_with_label("Histogram");
    app->mdt_button = gtk_button_new_with_label("Mean_Std. Dev. Plot");
    gtk_container_add(GTK_CONTAINER(hbuttonbox4), app->plot_button);
    gtk_container_add(GTK_CONTAINER(hbuttonbox4), app->acf_button);
    gtk_container_add(GTK_CONTAINER(hbuttonbox4), app->hist_button);
    gtk_container_add(GTK_CONTAINER(hbuttonbox4), app->mdt_button);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook1), hbuttonbox4, gtk_label_new("High Resolution Output Single Options"));

    /* Tab 3: High Resolution Output Set Options */
    GtkWidget *hbuttonbox5 = gtk_hbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox5), GTK_BUTTONBOX_START);
    app->iden_button = gtk_button_new_with_label("ID Options Set");
    gtk_container_add(GTK_CONTAINER(hbuttonbox5), app->iden_button);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook1), hbuttonbox5, gtk_label_new("High Resolution Output Set Options"));

    /* Bottom button bar */
    GtkWidget *vbox3 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(vbox1), vbox3, FALSE, FALSE, 0);

    GtkWidget *hbuttonbox2 = gtk_hbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox2), GTK_BUTTONBOX_END);
    app->help_button = gtk_button_new_with_mnemonic("_Help");
    app->refresh_button = gtk_button_new_with_mnemonic("_Refresh");
    app->quit_button = gtk_button_new_with_mnemonic("_Quit");
    app->save_button = gtk_button_new_with_mnemonic("_Save");
    gtk_container_add(GTK_CONTAINER(hbuttonbox2), app->help_button);
    gtk_container_add(GTK_CONTAINER(hbuttonbox2), app->refresh_button);
    gtk_container_add(GTK_CONTAINER(hbuttonbox2), app->quit_button);
    gtk_container_add(GTK_CONTAINER(hbuttonbox2), app->save_button);
    gtk_box_pack_start(GTK_BOX(vbox3), hbuttonbox2, FALSE, FALSE, 0);

    app->main_statusbar = gtk_statusbar_new();
    gtk_box_pack_start(GTK_BOX(vbox3), app->main_statusbar, FALSE, FALSE, 0);
}

static void create_mdt_dialog(AppWidgets *app)
{
    app->mdt_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->mdt_window), "Mean-Std. Dev Plot Option");
    gtk_window_set_modal(GTK_WINDOW(app->mdt_window), TRUE);
    gtk_window_set_position(GTK_WINDOW(app->mdt_window), GTK_WIN_POS_CENTER_ON_PARENT);
    gtk_window_set_default_size(GTK_WINDOW(app->mdt_window), 350, 200);
    gtk_window_set_transient_for(GTK_WINDOW(app->mdt_window), GTK_WINDOW(app->window));
    gtk_window_set_deletable(GTK_WINDOW(app->mdt_window), FALSE);

    GtkWidget *vbox4 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_add(GTK_CONTAINER(app->mdt_window), vbox4);

    GtkWidget *hbox5 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(vbox4), hbox5, FALSE, FALSE, 0);
    GtkWidget *label15 = gtk_label_new("Observations per Group:");
    gtk_box_pack_start(GTK_BOX(hbox5), label15, FALSE, FALSE, 0);
    app->mdt_entry_spinbutton = gtk_spin_button_new_with_range(1, 100, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->mdt_entry_spinbutton), 5);
    gtk_box_pack_start(GTK_BOX(hbox5), app->mdt_entry_spinbutton, FALSE, FALSE, 0);

        app->default_mdt_checkbutton = gtk_check_button_new_with_label("Default Options");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->default_mdt_checkbutton), TRUE);
    gtk_widget_set_halign(app->default_mdt_checkbutton, GTK_ALIGN_END);
    gtk_box_pack_start(GTK_BOX(vbox4), app->default_mdt_checkbutton, FALSE, FALSE, 0);

    GtkWidget *hseparator7 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox4), hseparator7, FALSE, FALSE, 0);

    GtkWidget *hbuttonbox1 = gtk_hbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox1), GTK_BUTTONBOX_END);
    GtkWidget *help_mdt = gtk_button_new_with_mnemonic("_Help");
    GtkWidget *cancel_mdt = gtk_button_new_with_mnemonic("_Cancel");
    GtkWidget *ok_mdt = gtk_button_new_with_mnemonic("_OK");
    gtk_container_add(GTK_CONTAINER(hbuttonbox1), help_mdt);
    gtk_container_add(GTK_CONTAINER(hbuttonbox1), cancel_mdt);
    gtk_container_add(GTK_CONTAINER(hbuttonbox1), ok_mdt);
    gtk_box_pack_start(GTK_BOX(vbox4), hbuttonbox1, FALSE, FALSE, 0);

    /* Connect signals */
    g_signal_connect(app->default_mdt_checkbutton, "toggled", G_CALLBACK(on_default_mdt_checkbutton_toggled), app);
    g_signal_connect(help_mdt, "clicked", G_CALLBACK(on_help_mdt_dialog_button_clicked), app);
    g_signal_connect(cancel_mdt, "clicked", G_CALLBACK(on_cancel_mdt_dialog_button_clicked), app);
    g_signal_connect(ok_mdt, "clicked", G_CALLBACK(on_ok_mdt_dialog_button_clicked), app);
}

static void create_acf_dialog(AppWidgets *app)
{
    app->acf_dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(app->acf_dialog), "Acf/Pacf Plots Options");
    gtk_window_set_modal(GTK_WINDOW(app->acf_dialog), TRUE);
    gtk_window_set_position(GTK_WINDOW(app->acf_dialog), GTK_WIN_POS_CENTER_ON_PARENT);
    gtk_window_set_default_size(GTK_WINDOW(app->acf_dialog), 250, 200);
    gtk_window_set_transient_for(GTK_WINDOW(app->acf_dialog), GTK_WINDOW(app->window));
    gtk_window_set_deletable(GTK_WINDOW(app->acf_dialog), FALSE);

    GtkWidget *vbox = gtk_dialog_get_content_area(GTK_DIALOG(app->acf_dialog));

    GtkWidget *options_acf_notebook = gtk_notebook_new();
    app->options_acf_notebook = options_acf_notebook;
    gtk_box_pack_start(GTK_BOX(vbox), options_acf_notebook, TRUE, TRUE, 0);

    /* Page 1: Lags */
    GtkWidget *hbox8 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *label26 = gtk_label_new("Number of Lags:");
    gtk_box_pack_start(GTK_BOX(hbox8), label26, FALSE, FALSE, 0);
    app->lags_acf_spinbutton = gtk_spin_button_new_with_range(0, 100, 1);
    gtk_box_pack_start(GTK_BOX(hbox8), app->lags_acf_spinbutton, FALSE, FALSE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(options_acf_notebook), hbox8, gtk_label_new("Lags"));

    /* Page 2: Degrees of freedom */
    GtkWidget *vbox5 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *hbox9 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *label27 = gtk_label_new("Order  p + q:");
    gtk_box_pack_start(GTK_BOX(hbox9), label27, FALSE, FALSE, 0);
    app->nparma_acf_spinbutton = gtk_spin_button_new_with_range(0, 100, 1);
    gtk_box_pack_start(GTK_BOX(hbox9), app->nparma_acf_spinbutton, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox5), hbox9, FALSE, FALSE, 0);
    GtkWidget *label28 = gtk_label_new("Where:\np = total number of AR parameters estimated\nq = total number of MA parameters estimated");
    gtk_box_pack_start(GTK_BOX(vbox5), label28, FALSE, FALSE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(options_acf_notebook), vbox5, gtk_label_new("Degrees of freedom"));

    /* Page 3: Bands */
    GtkWidget *vbox6 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *hbox10 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *label29 = gtk_label_new("Scale (0 = automatic):");
    gtk_box_pack_start(GTK_BOX(hbox10), label29, FALSE, FALSE, 0);
    app->cbands_acf_spinbutton = gtk_spin_button_new_with_range(0, 1, 0.1);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(app->cbands_acf_spinbutton), 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->cbands_acf_spinbutton), 1);
    gtk_box_pack_start(GTK_BOX(hbox10), app->cbands_acf_spinbutton, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox6), hbox10, FALSE, FALSE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(options_acf_notebook), vbox6, gtk_label_new("Bands"));

    app->default_acf_checkbutton = gtk_check_button_new_with_label("Default Options");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->default_acf_checkbutton), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), app->default_acf_checkbutton, FALSE, FALSE, 0);

    GtkWidget *action_area = gtk_dialog_get_action_area(GTK_DIALOG(app->acf_dialog));
    gtk_button_box_set_layout(GTK_BUTTON_BOX(action_area), GTK_BUTTONBOX_END);
    GtkWidget *cancel_acf = gtk_button_new_with_mnemonic("_Cancel");
    GtkWidget *ok_acf = gtk_button_new_with_mnemonic("_OK");
    gtk_container_add(GTK_CONTAINER(action_area), cancel_acf);
    gtk_container_add(GTK_CONTAINER(action_area), ok_acf);
    gtk_widget_set_can_default(ok_acf, TRUE);
    gtk_window_set_default(GTK_WINDOW(app->acf_dialog), ok_acf);

    g_signal_connect(app->default_acf_checkbutton, "toggled", G_CALLBACK(on_default_acf_checkbutton_toggled), app);
    g_signal_connect(cancel_acf, "clicked", G_CALLBACK(on_cancel_acf_button_clicked), app);
    g_signal_connect(ok_acf, "clicked", G_CALLBACK(on_ok_acf_button_clicked), app);
}

static void create_plot_dialog(AppWidgets *app)
{
    app->stand_plot_dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(app->stand_plot_dialog), "TS Data Plot Options");
    gtk_window_set_modal(GTK_WINDOW(app->stand_plot_dialog), TRUE);
    gtk_window_set_position(GTK_WINDOW(app->stand_plot_dialog), GTK_WIN_POS_CENTER_ON_PARENT);
    gtk_window_set_default_size(GTK_WINDOW(app->stand_plot_dialog), 300, 300);
    gtk_window_set_transient_for(GTK_WINDOW(app->stand_plot_dialog), GTK_WINDOW(app->window));
    gtk_window_set_deletable(GTK_WINDOW(app->stand_plot_dialog), FALSE);

    GtkWidget *vbox = gtk_dialog_get_content_area(GTK_DIALOG(app->stand_plot_dialog));

    GtkWidget *vbox7 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_pack_start(GTK_BOX(vbox), vbox7, TRUE, TRUE, 0);

    GtkWidget *options_plot_notebook = gtk_notebook_new();
    app->options_plot_notebook = options_plot_notebook;
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(options_plot_notebook), GTK_POS_LEFT);
    gtk_box_pack_start(GTK_BOX(vbox7), options_plot_notebook, TRUE, TRUE, 0);

    /* Page 1: Data Plot */
    GtkWidget *vbox8 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    app->with_acf_plot_checkbutton = gtk_check_button_new_with_label("Exclude Acf/Pacf Plots");
    gtk_box_pack_start(GTK_BOX(vbox8), app->with_acf_plot_checkbutton, FALSE, FALSE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(options_plot_notebook), vbox8, gtk_label_new("Data Plot"));

    /* Page 2: Acf/Pacf */
    GtkWidget *table2 = gtk_grid_new();
    GtkWidget *label18 = gtk_label_new("Number of Lags:");
    gtk_grid_attach(GTK_GRID(table2), label18, 0, 0, 1, 1);
    app->lags_acf_plot_spinbutton = gtk_spin_button_new_with_range(1, 100, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->lags_acf_plot_spinbutton), 9);
    gtk_grid_attach(GTK_GRID(table2), app->lags_acf_plot_spinbutton, 1, 0, 1, 1);
    GtkWidget *label19 = gtk_label_new("Degrees of Freedom:");
    gtk_grid_attach(GTK_GRID(table2), label19, 0, 1, 1, 1);
    app->nparma_acf_plot_spinbutton = gtk_spin_button_new_with_range(0, 100, 1);
    gtk_grid_attach(GTK_GRID(table2), app->nparma_acf_plot_spinbutton, 1, 1, 1, 1);
    GtkWidget *label20 = gtk_label_new("Bands (0 = automatic):");
    gtk_grid_attach(GTK_GRID(table2), label20, 0, 2, 1, 1);
    app->cbands_acf_plot_spinbutton = gtk_spin_button_new_with_range(0, 1, 0.1);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(app->cbands_acf_plot_spinbutton), 1);
    gtk_grid_attach(GTK_GRID(table2), app->cbands_acf_plot_spinbutton, 1, 2, 1, 1);
    gtk_notebook_append_page(GTK_NOTEBOOK(options_plot_notebook), table2, gtk_label_new("Acf/Pacf"));

    app->default_plot_checkbutton = gtk_check_button_new_with_label("Default Options");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->default_plot_checkbutton), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox7), app->default_plot_checkbutton, FALSE, FALSE, 0);

    GtkWidget *action_area = gtk_dialog_get_action_area(GTK_DIALOG(app->stand_plot_dialog));
    gtk_button_box_set_layout(GTK_BUTTON_BOX(action_area), GTK_BUTTONBOX_END);
    GtkWidget *cancel_plot = gtk_button_new_with_mnemonic("_Cancel");
    GtkWidget *ok_plot = gtk_button_new_with_mnemonic("_OK");
    gtk_container_add(GTK_CONTAINER(action_area), cancel_plot);
    gtk_container_add(GTK_CONTAINER(action_area), ok_plot);
    gtk_widget_set_can_default(ok_plot, TRUE);
    gtk_window_set_default(GTK_WINDOW(app->stand_plot_dialog), ok_plot);

    g_signal_connect(app->default_plot_checkbutton, "toggled", G_CALLBACK(on_default_plot_checkbutton_toggled), app);
    g_signal_connect(cancel_plot, "clicked", G_CALLBACK(on_cancel_plot_button_clicked), app);
    g_signal_connect(ok_plot, "clicked", G_CALLBACK(on_ok_plot_button_clicked), app);
}

static void create_output_dialog(AppWidgets *app)
{
    app->output_dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(app->output_dialog), "FUG Output");
    gtk_window_set_modal(GTK_WINDOW(app->output_dialog), TRUE);
    gtk_window_set_position(GTK_WINDOW(app->output_dialog), GTK_WIN_POS_CENTER_ON_PARENT);
    gtk_window_set_default_size(GTK_WINDOW(app->output_dialog), 300, 300);
    gtk_window_set_transient_for(GTK_WINDOW(app->output_dialog), GTK_WINDOW(app->window));
    gtk_window_set_deletable(GTK_WINDOW(app->output_dialog), FALSE);

    GtkWidget *vbox = gtk_dialog_get_content_area(GTK_DIALOG(app->output_dialog));

    GtkWidget *vbox9 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_pack_start(GTK_BOX(vbox), vbox9, TRUE, TRUE, 0);

    GtkWidget *hbox11 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *label22 = gtk_label_new("Select Option:");
    gtk_box_pack_start(GTK_BOX(hbox11), label22, FALSE, FALSE, 0);
    app->option_output_dialog_combobox = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->option_output_dialog_combobox), "Active");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->option_output_dialog_combobox), "One");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->option_output_dialog_combobox), "Set");
    gtk_box_pack_start(GTK_BOX(hbox11), app->option_output_dialog_combobox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox9), hbox11, FALSE, FALSE, 0);

    GtkWidget *lags_output_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *label31 = gtk_label_new("acf/pacf lags (0 = default):");
    gtk_box_pack_start(GTK_BOX(lags_output_hbox), label31, FALSE, FALSE, 0);
    app->lags_output_spinbutton = gtk_spin_button_new_with_range(0, 100, 1);
    gtk_box_pack_start(GTK_BOX(lags_output_hbox), app->lags_output_spinbutton, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox9), lags_output_hbox, FALSE, FALSE, 0);

    GtkWidget *hseparator4 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox9), hseparator4, FALSE, FALSE, 0);

    GtkWidget *label33 = gtk_label_new("Mean Deviation Graph apply only for the series in level");
    gtk_box_pack_start(GTK_BOX(vbox9), label33, FALSE, FALSE, 0);

    GtkWidget *nog_output_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *label32 = gtk_label_new("Observation per group:");
    gtk_box_pack_start(GTK_BOX(nog_output_hbox), label32, FALSE, FALSE, 0);
    app->nog_output_spinbutton = gtk_spin_button_new_with_range(1, 100, 1);
    gtk_box_pack_start(GTK_BOX(nog_output_hbox), app->nog_output_spinbutton, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox9), nog_output_hbox, FALSE, FALSE, 0);

    GtkWidget *hseparator8 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox9), hseparator8, FALSE, FALSE, 0);

    GtkWidget *hbox15 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *label45 = gtk_label_new("Max. Regular Differences:");
    gtk_box_pack_start(GTK_BOX(hbox15), label45, FALSE, FALSE, 0);
    app->nrdiff_output_spinbutton = gtk_spin_button_new_with_range(0, 3, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->nrdiff_output_spinbutton), 2);
    gtk_box_pack_start(GTK_BOX(hbox15), app->nrdiff_output_spinbutton, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox9), hbox15, FALSE, FALSE, 0);

    GtkWidget *hbox16 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *label46 = gtk_label_new("Max. Annual Differences:");
    gtk_box_pack_start(GTK_BOX(hbox16), label46, FALSE, FALSE, 0);
    app->nadiff_output_spinbutton = gtk_spin_button_new_with_range(0, 1, 1);
    gtk_box_pack_start(GTK_BOX(hbox16), app->nadiff_output_spinbutton, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox9), hbox16, FALSE, FALSE, 0);

    GtkWidget *action_area = gtk_dialog_get_action_area(GTK_DIALOG(app->output_dialog));
    gtk_button_box_set_layout(GTK_BUTTON_BOX(action_area), GTK_BUTTONBOX_END);
    GtkWidget *cancel_output = gtk_button_new_with_mnemonic("_Cancel");
    GtkWidget *ok_output = gtk_button_new_with_mnemonic("_OK");
    gtk_container_add(GTK_CONTAINER(action_area), cancel_output);
    gtk_container_add(GTK_CONTAINER(action_area), ok_output);
    gtk_widget_set_can_default(ok_output, TRUE);
    gtk_window_set_default(GTK_WINDOW(app->output_dialog), ok_output);

    g_signal_connect(app->option_output_dialog_combobox, "changed", G_CALLBACK(on_option_output_dialog_combobox_changed), app);
    g_signal_connect(cancel_output, "clicked", G_CALLBACK(on_cancel_output_dialog_button_clicked), app);
    g_signal_connect(ok_output, "clicked", G_CALLBACK(on_ok_output_dialog_button_clicked), app);
}

static void create_iden_dialog(AppWidgets *app)
{
    app->iden_dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(app->iden_dialog), "ID Options Set");
    gtk_window_set_modal(GTK_WINDOW(app->iden_dialog), TRUE);
    gtk_window_set_position(GTK_WINDOW(app->iden_dialog), GTK_WIN_POS_CENTER_ON_PARENT);
    gtk_window_set_default_size(GTK_WINDOW(app->iden_dialog), 350, 300);
    gtk_window_set_transient_for(GTK_WINDOW(app->iden_dialog), GTK_WINDOW(app->window));
    gtk_window_set_deletable(GTK_WINDOW(app->iden_dialog), FALSE);

    GtkWidget *vbox = gtk_dialog_get_content_area(GTK_DIALOG(app->iden_dialog));

    GtkWidget *vbox10 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_pack_start(GTK_BOX(vbox), vbox10, TRUE, TRUE, 0);

    GtkWidget *options_iden_notebook = gtk_notebook_new();
    app->options_iden_notebook = options_iden_notebook;
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(options_iden_notebook), GTK_POS_LEFT);
    gtk_box_pack_start(GTK_BOX(vbox10), options_iden_notebook, TRUE, TRUE, 0);

    /* Page 1: Differences */
    GtkWidget *table4 = gtk_grid_new();
    GtkWidget *label38 = gtk_label_new("Max. Regular Differences:");
    gtk_grid_attach(GTK_GRID(table4), label38, 0, 0, 1, 1);
    app->nrdiff_iden_spinbutton = gtk_spin_button_new_with_range(0, 2, 1);
    gtk_grid_attach(GTK_GRID(table4), app->nrdiff_iden_spinbutton, 1, 0, 1, 1);
    GtkWidget *label39 = gtk_label_new("Max. Annual Differences:");
    gtk_grid_attach(GTK_GRID(table4), label39, 0, 1, 1, 1);
    app->nadiff_iden_spinbutton = gtk_spin_button_new_with_range(0, 1, 1);
    gtk_grid_attach(GTK_GRID(table4), app->nadiff_iden_spinbutton, 1, 1, 1, 1);
    gtk_notebook_append_page(GTK_NOTEBOOK(options_iden_notebook), table4, gtk_label_new("Differences"));

    /* Page 2: Acf/Pacf Plots */
    GtkWidget *hbox13 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *label40 = gtk_label_new("lags (0 = default):");
    gtk_box_pack_start(GTK_BOX(hbox13), label40, FALSE, FALSE, 0);
    app->lags_acf_iden_spinbutton = gtk_spin_button_new_with_range(0, 100, 1);
    gtk_box_pack_start(GTK_BOX(hbox13), app->lags_acf_iden_spinbutton, FALSE, FALSE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(options_iden_notebook), hbox13, gtk_label_new("Acf/Pacf Plots"));

    /* Page 3: Mean-Std. Dev. Plot */
    GtkWidget *vbox11 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    app->with_mdt_iden_checkbutton = gtk_check_button_new_with_label("Exclude Mean-Std. Dev Plot");
    gtk_widget_set_halign(app->with_mdt_iden_checkbutton, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox11), app->with_mdt_iden_checkbutton, FALSE, FALSE, 0);
    GtkWidget *hbox12 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *label41 = gtk_label_new("Number observations per group:");
    gtk_box_pack_start(GTK_BOX(hbox12), label41, FALSE, FALSE, 0);
    app->nog_iden_spinbutton = gtk_spin_button_new_with_range(1, 100, 1);
    gtk_box_pack_start(GTK_BOX(hbox12), app->nog_iden_spinbutton, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox11), hbox12, FALSE, FALSE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(options_iden_notebook), vbox11, gtk_label_new("Mean-Std. Dev. Plot"));

    /* Page 4: Original Series */
    app->no_level_iden_checkbutton = gtk_check_button_new_with_label("Exclude Original Series and the\ncorresponding Mean-Std.\nDev. Plot");
    gtk_widget_set_halign(app->no_level_iden_checkbutton, GTK_ALIGN_START);
    gtk_widget_set_valign(app->no_level_iden_checkbutton, GTK_ALIGN_CENTER);
    gtk_notebook_append_page(GTK_NOTEBOOK(options_iden_notebook),
                             app->no_level_iden_checkbutton,
                             gtk_label_new("Original Series"));

    app->default_iden_checkbutton = gtk_check_button_new_with_label("Default Options");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->default_iden_checkbutton), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox10), app->default_iden_checkbutton, FALSE, FALSE, 0);

    GtkWidget *action_area = gtk_dialog_get_action_area(GTK_DIALOG(app->iden_dialog));
    gtk_button_box_set_layout(GTK_BUTTON_BOX(action_area), GTK_BUTTONBOX_END);
    GtkWidget *cancel_iden = gtk_button_new_with_mnemonic("_Cancel");
    GtkWidget *ok_iden = gtk_button_new_with_mnemonic("_OK");
    gtk_container_add(GTK_CONTAINER(action_area), cancel_iden);
    gtk_container_add(GTK_CONTAINER(action_area), ok_iden);
    gtk_widget_set_can_default(ok_iden, TRUE);
    gtk_window_set_default(GTK_WINDOW(app->iden_dialog), ok_iden);

    g_signal_connect(app->default_iden_checkbutton, "toggled", G_CALLBACK(on_default_iden_checkbutton_toggled), app);
    g_signal_connect(app->with_mdt_iden_checkbutton, "toggled", G_CALLBACK(on_with_mdt_iden_checkbutton_toggled), app);
    g_signal_connect(cancel_iden, "clicked", G_CALLBACK(on_cancel_iden_dialog_button_clicked), app);
    g_signal_connect(ok_iden, "clicked", G_CALLBACK(on_ok_iden_dialog_button_clicked), app);
}
