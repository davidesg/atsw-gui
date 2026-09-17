/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * callbacks.c
 * Copyright (C) David Guerrero 2010 <warriord@rocketmail.com>
 *
 * Modified to use pure GTK (no Glade)
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <math.h>
#include <gtk/gtk.h>
#include "callbacks.h"
#include "nlutils.h"
#include "data_load.h"
#include "fug_run.h"

/* Minimum number of observations left after differencing */
#define MIN_OBS 10


/* ---------------------------------------------------------------------- */
/* Helper functions                                                       */
/* ---------------------------------------------------------------------- */
static void set_main(AppWidgets *app)
{
    gtk_entry_set_text(GTK_ENTRY(app->series_name_entry), Ts.name);

    if (Ts.freq == 1)
        gtk_combo_box_set_active(GTK_COMBO_BOX(app->freq_data_combobox), 0);
    else if (Ts.freq == 4)
        gtk_combo_box_set_active(GTK_COMBO_BOX(app->freq_data_combobox), 1);
    else if (Ts.freq == 12)
        gtk_combo_box_set_active(GTK_COMBO_BOX(app->freq_data_combobox), 2);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->n_load_spinbutton), Ts.nobs);
    if (Ts.freq > 1)
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->first_period_spinbutton), Ts.begtime);
    else
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->first_period_spinbutton), Ts.outyear);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->first_year_spinbutton), Ts.begyear);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->box_cox_lambda_spinbutton), Tm.boxlam);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->box_cox_m_spinbutton), Tm.boxm);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->nrdiff_spinbutton), Tm.nrdiff);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->nadiff_spinbutton), Tm.nadiff);
}

/* Input name: name of the .inp file without extension */
static const gchar *get_handler(AppWidgets *app)
{
    return gtk_entry_get_text(GTK_ENTRY(app->path_data_entry));
}

/* Check that enough observations remain after differencing */
static gboolean check_differences(AppWidgets *app, int nrdiff, int nadiff)
{
    int left = Ts.nobs - (nrdiff + Ts.freq * nadiff);

    if (left < MIN_OBS) {
        show_error(app, "Too few observations: %d observations with %d regular and %d annual "
                        "differences leave only %d values (at least %d are needed).",
                   Ts.nobs, nrdiff, nadiff, left, MIN_OBS);
        return FALSE;
    }
    return TRUE;
}

/* ---------------------------------------------------------------------- */
/* Global functions (used by main.c)                                      */
/* ---------------------------------------------------------------------- */

/* Read the main window into Ts and Tm, and the series from the data file.
 * Returns FALSE (after telling the user) if something is wrong. */
/* v rounded to 6 decimals, exactly as fug reads it from "%.6f" */
static double six_decimals(double v)
{
    gchar buf[G_ASCII_DTOSTR_BUF_SIZE];

    return g_ascii_strtod(g_ascii_formatd(buf, sizeof buf, "%.6f", v), NULL);
}

gboolean get_main(AppWidgets *app)
{
    gchar *filename, *name;
    GError *error = NULL;
    double *data;
    int nvalues, nobs, freq_idx;

    nobs = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->n_load_spinbutton));
    if (nobs < MIN_OBS) {
        show_error(app, "Error loading data!\nOnly %d observations.\nAre you sure?", nobs);
        return FALSE;
    }

    filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(app->data_filechooserbutton));
    if (filename == NULL) {
        show_error(app, "Error: No data file selected.");
        return FALSE;
    }

    /* The data are read again each time, so changes to the file are used */
    if (g_ascii_strcasecmp(getExt(filename), ".inp") == 0) {
        struct Tseries ts_saved = Ts;
        struct Tusmodel tm_saved = Tm;

        Ts.name = NULL;
        Ts.data = NULL;
        if (load_input(filename, &error)) {
            data = Ts.data;
            nvalues = Ts.nobs;
            g_free((gchar *) Ts.name);
        } else {
            data = NULL;
        }
        Ts = ts_saved;      /* only the data are taken from the .inp file */
        Tm = tm_saved;
    } else {
        data = read_data_values(filename, &nvalues, &error);
    }
    if (data == NULL) {
        show_error(app, "Error loading data:\n%s", error->message);
        g_error_free(error);
        g_free(filename);
        return FALSE;
    }
    if (nvalues < nobs) {
        show_error(app, "Error reading data: Not enough observations.\n"
                        "The file has %d values and %d were requested:\n%s",
                   nvalues, nobs, filename);
        g_free(data);
        g_free(filename);
        return FALSE;
    }
    g_free(filename);
    set_series_data(data, nobs);

    /* fug reads the series name as a single word */
    name = g_strdup(gtk_entry_get_text(GTK_ENTRY(app->series_name_entry)));
    g_strstrip(name);
    if (*name == '\0') {
        g_free(name);
        name = g_strdup(get_handler(app));
    }
    g_strdelimit(name, " \t\n\r", '-');
    g_free((gchar *) Ts.name);
    Ts.name = name;

    freq_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(app->freq_data_combobox));
    if (freq_idx == 0) Ts.freq = 1;
    else if (freq_idx == 1) Ts.freq = 4;
    else Ts.freq = 12;

    if (Ts.freq > 1) {
        Ts.begtime = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->first_period_spinbutton));
        Ts.outyear = 0;
    } else {
        Ts.begtime = 1;
        Ts.outyear = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->first_period_spinbutton));
    }
    Ts.begyear = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->first_year_spinbutton));

    /* The 6 decimals of the spin buttons, as fug reads them from -B, so the
     * plot file names computed here match the ones of fug */
    Tm.boxlam = six_decimals(gtk_spin_button_get_value(GTK_SPIN_BUTTON(app->box_cox_lambda_spinbutton)));
    Tm.boxm = six_decimals(gtk_spin_button_get_value(GTK_SPIN_BUTTON(app->box_cox_m_spinbutton)));
    Tm.nrdiff = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->nrdiff_spinbutton));
    Tm.nadiff = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->nadiff_spinbutton));

    return check_differences(app, Tm.nrdiff, Tm.nadiff);
}

/* Ask before replacing a fue model (or a file that is not an .inp) */
static gboolean replace_input(AppWidgets *app, const gchar *path, InputStatus status)
{
    GtkWidget *dialog;
    gint response;

    dialog = gtk_message_dialog_new(GTK_WINDOW(app->window),
                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE, "%s",
                                    status == INPUT_MODEL
                                    ? "The input file has a fue model of another series"
                                    : "The input file can not be read");
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
        "%s\n\n%s", path,
        status == INPUT_MODEL
        ? "fug and fue share the .inp file. If it is replaced with the series shown, "
          "the fue model is lost. Choose another input name to keep it."
        : "Replace it with the series shown?");
    gtk_dialog_add_buttons(GTK_DIALOG(dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                           "_Replace", GTK_RESPONSE_ACCEPT, NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
    gtk_window_set_title(GTK_WINDOW(dialog), "FUG");
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    return response == GTK_RESPONSE_ACCEPT;
}

/* Write <workspace>/<input name>.inp from Ts and Tm, as a fue .inp without
 * model. The .inp is shared with fue: a file with the same series is left
 * as it is (fug takes the transformation from its command line, -B), and a
 * fue model of another series is only replaced if the user agrees. */
gboolean SaveInpFile(AppWidgets *app)
{
    const gchar *handler;
    gchar *workspace, *outputf;
    GError *error = NULL;
    InputStatus status;
    gboolean ok;

    handler = get_handler(app);
    if (*handler == '\0') {
        show_error(app, "Error: Input name is empty.");
        return FALSE;
    }
    if (strpbrk(handler, " \t/\\") != NULL) {
        show_error(app, "Error: The input name \"%s\" can not contain spaces or slashes.", handler);
        return FALSE;
    }
    workspace = get_workspace(app);
    if (workspace == NULL) {
        show_error(app, "Error: Workspace folder not selected.");
        return FALSE;
    }

    outputf = g_strconcat(workspace, G_DIR_SEPARATOR_S, handler, ".inp", NULL);
    status = compare_input(outputf);
    if (status == INPUT_SAME) {
        ok = TRUE;
        show_status(app, "Using %s", outputf);
    } else if ((status == INPUT_MODEL || status == INPUT_UNREADABLE) &&
               !replace_input(app, outputf, status)) {
        ok = FALSE;
        show_status(app, "%s was not replaced", outputf);
    } else if (!(ok = save_input(outputf, &error))) {
        show_error(app, "Error with Input Name:\n%s", error->message);
        g_error_free(error);
    } else {
        show_status(app, "Saved %s", outputf);
    }

    g_free(outputf);
    g_free(workspace);
    return ok;
}

/* Read the main window and write the .inp file. Returns the workspace
 * folder (g_free it), or NULL if fug can not be run. */
static gchar *prepare_input(AppWidgets *app)
{
    if (!get_main(app) || !SaveInpFile(app))
        return NULL;
    return get_workspace(app);
}

/* Run fug with args and, if it worked, show the file it created. */
static void run_and_view(AppWidgets *app, GPtrArray *args, const gchar *result)
{
    gchar *workspace = get_workspace(app);

    if (workspace != NULL && run_fug(app, workspace, args))
        open_viewer(app, workspace, result);

    g_free(workspace);
    g_ptr_array_free(args, TRUE);
}

/* Base name of the plots of the current series (g_free it) */
static gchar *plot_name(AppWidgets *app, int nrdiff, int nadiff)
{
    return file_plot(nrdiff, nadiff, Tm.boxlam, Ts.freq, (char *) get_handler(app));
}

/* ---------------------------------------------------------------------- */
/* Callbacks                                                              */
/* ---------------------------------------------------------------------- */
void on_save_button_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *app = (AppWidgets*)user_data;

    (void) button;
    g_free(prepare_input(app));
}

void on_refresh_button_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *app = (AppWidgets*)user_data;

    (void) button;
    gtk_entry_set_text(GTK_ENTRY(app->series_name_entry), "");
    gtk_file_chooser_unselect_all(GTK_FILE_CHOOSER(app->data_filechooserbutton));
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->freq_data_combobox), 0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->n_load_spinbutton), 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->first_period_spinbutton), 0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->first_year_spinbutton), 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->box_cox_lambda_spinbutton), 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->box_cox_m_spinbutton), 0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->nrdiff_spinbutton), 0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->nadiff_spinbutton), 0);
    show_status(app, "Cleared");
}

void on_help_button_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *app = (AppWidgets*)user_data;
    GtkWidget *dialog;
    gchar *path;

    (void) button;
    path = g_path_is_absolute(fug_program()) ? g_strdup(fug_program())
                                             : g_find_program_in_path(fug_program());
    dialog = gtk_message_dialog_new(GTK_WINDOW(app->window),
                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
                                    "FUG: graphics for time series identification");
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
        "1. Choose the data file (.txt/.dat/.csv with one value per line, or a FUG .inp file).\n"
        "2. Choose the workspace folder and the input name: <input name>.inp and all the "
        "results are written in the workspace.\n"
        "3. Set the frequency, starting date, Box-Cox transformation and differences.\n"
        "4. Use the buttons of the tabs to run fug and see the results.\n\n"
        "Plots are shown in the graph window, where they can be saved as PDF, EPS, PNG "
        "or SVG and printed. Text files are opened with gedit (Notepad on Windows).\n\n"
        "FUG engine: %s", path ? path : "fug NOT FOUND (next to gtk_fmg or in the PATH)");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    g_free(path);
}

void on_plot_button_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *app = (AppWidgets*)user_data;
    gchar *workspace;

    (void) button;
    if ((workspace = prepare_input(app)) == NULL)
        return;
    g_free(workspace);

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->default_plot_checkbutton)) == TRUE) {
        gtk_widget_set_sensitive(app->options_plot_notebook, FALSE);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->lags_acf_plot_spinbutton), default_lags(Ts.nobs, Ts.freq));
    }

    gtk_widget_show_all(app->stand_plot_dialog);
    gtk_window_present(GTK_WINDOW(app->stand_plot_dialog));
}

void on_acf_button_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *app = (AppWidgets*)user_data;
    gchar *workspace;

    (void) button;
    if ((workspace = prepare_input(app)) == NULL)
        return;
    g_free(workspace);

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->default_acf_checkbutton)) == TRUE) {
        gtk_widget_set_sensitive(app->options_acf_notebook, FALSE);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->lags_acf_spinbutton), default_lags(Ts.nobs, Ts.freq));
    }

    gtk_widget_show_all(app->acf_dialog);
    gtk_window_present(GTK_WINDOW(app->acf_dialog));
}

void on_hist_button_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *app = (AppWidgets*)user_data;
    gchar *workspace, *name, *plot;
    GPtrArray *args;

    (void) button;
    if ((workspace = prepare_input(app)) == NULL)
        return;
    g_free(workspace);

    name = plot_name(app, Tm.nrdiff, Tm.nadiff);
    plot = g_strdup_printf("hist_%s.eps", name);

    args = fug_args_new(get_handler(app));
    fug_args_add(args, "-d");
    run_and_view(app, args, plot);

    g_free(plot);
    g_free(name);
}

void on_mdt_button_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *app = (AppWidgets*)user_data;
    gchar *workspace;

    (void) button;
    if ((workspace = prepare_input(app)) == NULL)
        return;
    g_free(workspace);

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->default_mdt_checkbutton)) == TRUE) {
        gtk_widget_set_sensitive(app->mdt_entry_spinbutton, FALSE);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->mdt_entry_spinbutton), default_nog(Ts.freq));
    }

    gtk_widget_show_all(app->mdt_window);
    gtk_window_present(GTK_WINDOW(app->mdt_window));
}

void on_ok_mdt_dialog_button_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *app = (AppWidgets*)user_data;
    gchar *name, *graph;
    GPtrArray *args;
    int nog;

    (void) button;
    gtk_widget_hide(app->mdt_window);

    nog = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->mdt_entry_spinbutton));
    if (Ts.nobs / nog < 2) {
        show_error(app, "Too few groups: %d observations in groups of %d.", Ts.nobs, nog);
        return;
    }

    /* The mean-std. dev. plot is always made for the series in level */
    name = plot_name(app, 0, 0);
    graph = g_strdup_printf("m_dt_%s.eps", name);

    args = fug_args_new(get_handler(app));
    fug_args_add(args, "-e");
    fug_args_add(args, "-m");
    fug_args_add(args, "%d", nog);
    run_and_view(app, args, graph);

    g_free(graph);
    g_free(name);
}

void on_help_mdt_dialog_button_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *app = (AppWidgets*)user_data;
    GtkWidget *dialog;

    (void) button;
    dialog = gtk_message_dialog_new(GTK_WINDOW(app->mdt_window),
                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
                                    "Mean - Standard Deviation Plot");
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
        "The series in level (with the Box-Cox transformation) is split in consecutive "
        "groups of the given number of observations. The mean and the standard deviation "
        "of each group are plotted: a relation between them suggests a Box-Cox "
        "transformation.\n\nDefault: 12 observations per group for monthly data, 8 otherwise.");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

void on_cancel_mdt_dialog_button_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *app = (AppWidgets*)user_data;

    (void) button;
    gtk_widget_hide(app->mdt_window);
}

void on_iden_button_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *app = (AppWidgets*)user_data;
    gchar *workspace;

    (void) button;
    if ((workspace = prepare_input(app)) == NULL)
        return;
    g_free(workspace);

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->default_iden_checkbutton)) == TRUE) {
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->nrdiff_iden_spinbutton), 2);
        if (Ts.freq > 1)
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->nadiff_iden_spinbutton), 1);
        else
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->nadiff_iden_spinbutton), 0);

        gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->lags_acf_iden_spinbutton), default_lags(Ts.nobs, Ts.freq));
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->nog_iden_spinbutton), default_nog(Ts.freq));
        gtk_widget_set_sensitive(app->options_iden_notebook, FALSE);
    }
    gtk_widget_set_sensitive(app->nadiff_iden_spinbutton, Ts.freq > 1);

    gtk_widget_show_all(app->iden_dialog);
    gtk_window_present(GTK_WINDOW(app->iden_dialog));
}

void on_see_input_button_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *app = (AppWidgets*)user_data;
    gchar *workspace, *input;

    (void) button;
    if ((workspace = prepare_input(app)) == NULL)
        return;

    input = g_strconcat(get_handler(app), ".inp", NULL);
    open_viewer(app, workspace, input);

    g_free(input);
    g_free(workspace);
}

void on_see_output_button_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *app = (AppWidgets*)user_data;
    gchar *workspace;

    (void) button;
    if ((workspace = prepare_input(app)) == NULL)
        return;
    g_free(workspace);

    if (gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->nog_output_spinbutton)) <= 1)
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->nog_output_spinbutton), default_nog(Ts.freq));
    on_option_output_dialog_combobox_changed(GTK_COMBO_BOX(app->option_output_dialog_combobox), app);

    gtk_widget_show_all(app->output_dialog);
    gtk_window_present(GTK_WINDOW(app->output_dialog));
}

void on_ok_acf_button_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *app = (AppWidgets*)user_data;
    gchar *name, *graph;
    GPtrArray *args;
    double cbands;

    (void) button;
    gtk_widget_hide(app->acf_dialog);

    name = plot_name(app, Tm.nrdiff, Tm.nadiff);
    graph = g_strdup_printf("acf_%s.eps", name);

    args = fug_args_new(get_handler(app));
    fug_args_add(args, "-b");
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->default_acf_checkbutton)) == FALSE) {
        fug_args_add(args, "-l");
        fug_args_add(args, "%d", gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->lags_acf_spinbutton)));
        fug_args_add(args, "-g");
        fug_args_add(args, "%d", gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->nparma_acf_spinbutton)));
        cbands = gtk_spin_button_get_value(GTK_SPIN_BUTTON(app->cbands_acf_spinbutton));
        if (cbands > 0) {
            fug_args_add(args, "-f");
            fug_args_add(args, "%.1f", cbands);
        }
    }
    run_and_view(app, args, graph);

    g_free(graph);
    g_free(name);
}

void on_cancel_acf_button_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *app = (AppWidgets*)user_data;

    (void) button;
    gtk_widget_hide(app->acf_dialog);
}

void on_cancel_plot_button_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *app = (AppWidgets*)user_data;

    (void) button;
    gtk_widget_hide(app->stand_plot_dialog);
}

void on_ok_plot_button_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *app = (AppWidgets*)user_data;
    gchar *name, *graph;
    GPtrArray *args;
    double cbands;

    (void) button;
    gtk_widget_hide(app->stand_plot_dialog);

    name = plot_name(app, Tm.nrdiff, Tm.nadiff);
    graph = g_strdup_printf("%s.eps", name);

    args = fug_args_new(get_handler(app));
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->default_plot_checkbutton)) == TRUE) {
        fug_args_add(args, "-c");
    } else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->with_acf_plot_checkbutton)) == FALSE) {
        fug_args_add(args, "-c");
        fug_args_add(args, "-l");
        fug_args_add(args, "%d", gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->lags_acf_plot_spinbutton)));
        fug_args_add(args, "-g");
        fug_args_add(args, "%d", gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->nparma_acf_plot_spinbutton)));
        cbands = gtk_spin_button_get_value(GTK_SPIN_BUTTON(app->cbands_acf_plot_spinbutton));
        if (cbands > 0) {
            fug_args_add(args, "-f");
            fug_args_add(args, "%.1f", cbands);
        }
    } else {
        fug_args_add(args, "-a");
    }
    run_and_view(app, args, graph);

    g_free(graph);
    g_free(name);
}

void on_ok_output_dialog_button_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *app = (AppWidgets*)user_data;
    gchar *workspace, *file_output, *output;
    GPtrArray *args;
    int lags, nog, nrdiff, nadiff, active;

    (void) button;
    gtk_widget_hide(app->output_dialog);

    lags = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->lags_output_spinbutton));
    nog = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->nog_output_spinbutton));
    nrdiff = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->nrdiff_output_spinbutton));
    nadiff = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->nadiff_output_spinbutton));
    output = g_strconcat(get_handler(app), "_fug.out", NULL);

    active = gtk_combo_box_get_active(GTK_COMBO_BOX(app->option_output_dialog_combobox));
    if (active <= 0) {
        /* Active: the .out file of the last run of fug */
        workspace = get_workspace(app);
        file_output = g_build_filename(workspace, output, NULL);
        if (g_file_test(file_output, G_FILE_TEST_EXISTS) == TRUE)
            open_viewer(app, workspace, output);
        else
            no_file_message(file_output, user_data);
        g_free(file_output);
        g_free(workspace);
    } else if (active == 1 || check_differences(app, nrdiff, Ts.freq > 1 ? nadiff : 0)) {
        args = fug_args_new(get_handler(app));
        if (active == 2) {
            fug_args_add(args, "set");
            fug_args_add(args, "%d", nrdiff);
            fug_args_add(args, "%d", nadiff);
            fug_args_add(args, "-h");
        }
        if (lags > 0) {
            fug_args_add(args, "-l");
            fug_args_add(args, "%d", lags);
        }
        fug_args_add(args, "-m");
        fug_args_add(args, "%d", nog);
        run_and_view(app, args, output);
    }

    g_free(output);
}

void on_cancel_output_dialog_button_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *app = (AppWidgets*)user_data;

    (void) button;
    gtk_widget_hide(app->output_dialog);
}

void on_default_plot_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
    AppWidgets *app = (AppWidgets*)user_data;

    if (gtk_toggle_button_get_active(togglebutton) == TRUE) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->with_acf_plot_checkbutton), FALSE);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->lags_acf_plot_spinbutton), default_lags(Ts.nobs, Ts.freq));
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->cbands_acf_plot_spinbutton), 1.0);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->nparma_acf_plot_spinbutton), 0);
        gtk_widget_set_sensitive(app->options_plot_notebook, FALSE);
    } else {
        gtk_widget_set_sensitive(app->options_plot_notebook, TRUE);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->cbands_acf_plot_spinbutton), 1.0);
    }
}

void on_default_acf_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
    AppWidgets *app = (AppWidgets*)user_data;

    if (gtk_toggle_button_get_active(togglebutton) == TRUE) {
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->lags_acf_spinbutton), default_lags(Ts.nobs, Ts.freq));
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->nparma_acf_spinbutton), 0);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->cbands_acf_spinbutton), 1.0);
        gtk_widget_set_sensitive(app->options_acf_notebook, FALSE);
    } else {
        gtk_widget_set_sensitive(app->options_acf_notebook, TRUE);
    }
}

void on_default_mdt_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
    AppWidgets *app = (AppWidgets*)user_data;

    if (gtk_toggle_button_get_active(togglebutton) == TRUE) {
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->mdt_entry_spinbutton), default_nog(Ts.freq));
        gtk_widget_set_sensitive(app->mdt_entry_spinbutton, FALSE);
    } else {
        gtk_widget_set_sensitive(app->mdt_entry_spinbutton, TRUE);
    }
}

void on_option_output_dialog_combobox_changed(GtkComboBox *combo_box, gpointer user_data)
{
    AppWidgets *app = (AppWidgets*)user_data;
    int active = gtk_combo_box_get_active(combo_box);

    if (active <= 0) {
        gtk_widget_set_sensitive(app->lags_output_spinbutton, FALSE);
        gtk_widget_set_sensitive(app->nog_output_spinbutton, FALSE);
    } else {
        gtk_widget_set_sensitive(app->lags_output_spinbutton, TRUE);
        if (active == 2 || (active == 1 && Tm.nrdiff == 0 && Tm.nadiff == 0))
            gtk_widget_set_sensitive(app->nog_output_spinbutton, TRUE);
        else
            gtk_widget_set_sensitive(app->nog_output_spinbutton, FALSE);
    }
    gtk_widget_set_sensitive(app->nrdiff_output_spinbutton, active == 2);
    gtk_widget_set_sensitive(app->nadiff_output_spinbutton, active == 2 && Ts.freq > 1);
}

void on_freq_data_combobox_changed(GtkComboBox *combo_box, gpointer user_data)
{
    AppWidgets *app = (AppWidgets*)user_data;
    GtkSpinButton *period = GTK_SPIN_BUTTON(app->first_period_spinbutton);

    int active = gtk_combo_box_get_active(combo_box);
    if (active == 0) {
        gtk_spin_button_set_range(period, 0, 100);
        gtk_label_set_text(GTK_LABEL(app->season_label), "Displacement:");
        gtk_label_set_text(GTK_LABEL(app->year_label), "Starting Year or Number:");
    } else {
        gtk_label_set_text(GTK_LABEL(app->year_label), "Starting Year:");
        if (active == 1) {
            gtk_spin_button_set_range(period, 1, 4);
            gtk_label_set_text(GTK_LABEL(app->season_label), "Starting Quarter:");
        } else if (active == 2) {
            gtk_spin_button_set_range(period, 1, 12);
            gtk_label_set_text(GTK_LABEL(app->season_label), "Starting Month:");
        }
    }
}

void on_series_name_entry_changed(GtkEntry *entry, gpointer user_data)
{
    AppWidgets *app = (AppWidgets*)user_data;
    gchar *name2;

    /* Input name: the series name without punctuation or spaces */
    name2 = g_strdup(gtk_entry_get_text(entry));
    char *p1 = name2, *p2 = name2;
    while (*p1) {
        if (g_ascii_isalnum(*p1))
            *p2++ = *p1;
        p1++;
    }
    *p2 = 0;

    gtk_entry_set_text(GTK_ENTRY(app->path_data_entry), name2);
    g_free(name2);
}

void on_data_filechooserbutton_file_set(GtkButton *button, gpointer user_data)
{
    AppWidgets *app = (AppWidgets*)user_data;
    gchar *inputf, *folder, *base;
    GError *error = NULL;
    double *data;
    int nvalues;

    (void) button;
    inputf = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(app->data_filechooserbutton));
    if (inputf == NULL)
        return;

    /* The results are written by default next to the data */
    folder = g_path_get_dirname(inputf);
    set_workspace(app, folder);
    g_free(folder);

    if (g_ascii_strcasecmp(getExt(inputf), ".inp") == 0) {
        if (load_input(inputf, &error)) {
            if (*Ts.name == '\0') {
                g_free((gchar *) Ts.name);
                Ts.name = g_path_get_basename(inputf);
                *strrchr((gchar *) Ts.name, '.') = '\0';
            }
            set_main(app);
            show_status(app, "Loaded %s (%d observations)", inputf, Ts.nobs);
        }
    } else if ((data = read_data_values(inputf, &nvalues, &error)) != NULL) {
        set_series_data(data, nvalues);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->n_load_spinbutton), nvalues);
        if (*gtk_entry_get_text(GTK_ENTRY(app->series_name_entry)) == '\0') {
            base = g_path_get_basename(inputf);
            if (strrchr(base, '.') != NULL)
                *strrchr(base, '.') = '\0';
            gtk_entry_set_text(GTK_ENTRY(app->series_name_entry), base);
            g_free(base);
        }
        show_status(app, "Read %d values from %s", nvalues, inputf);
    }
    if (error != NULL) {
        show_error(app, "Error loading data:\n%s", error->message);
        g_error_free(error);
    }
    g_free(inputf);
}

void on_with_mdt_iden_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
    AppWidgets *app = (AppWidgets*)user_data;
    if (gtk_toggle_button_get_active(togglebutton) == TRUE)
        gtk_widget_set_sensitive(app->nog_iden_spinbutton, FALSE);
    else
        gtk_widget_set_sensitive(app->nog_iden_spinbutton, TRUE);
}

void on_default_iden_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
    AppWidgets *app = (AppWidgets*)user_data;

    if (gtk_toggle_button_get_active(togglebutton) == TRUE) {
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->nrdiff_iden_spinbutton), 2);
        if (Ts.freq > 1)
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->nadiff_iden_spinbutton), 1);
        else
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->nadiff_iden_spinbutton), 0);

        gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->lags_acf_iden_spinbutton), default_lags(Ts.nobs, Ts.freq));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->with_mdt_iden_checkbutton), FALSE);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->nog_iden_spinbutton), default_nog(Ts.freq));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->no_level_iden_checkbutton), FALSE);

        gtk_widget_set_sensitive(app->options_iden_notebook, FALSE);
    } else {
        gtk_widget_set_sensitive(app->options_iden_notebook, TRUE);
    }
}

void on_cancel_iden_dialog_button_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *app = (AppWidgets*)user_data;

    (void) button;
    gtk_widget_hide(app->iden_dialog);
}

void on_ok_iden_dialog_button_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *app = (AppWidgets*)user_data;
    gchar *graph;
    GPtrArray *args;
    int nrdiff, nadiff, lags, nog;

    (void) button;
    gtk_widget_hide(app->iden_dialog);

    nrdiff = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->nrdiff_iden_spinbutton));
    nadiff = (Ts.freq > 1) ? gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->nadiff_iden_spinbutton)) : 0;
    lags = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->lags_acf_iden_spinbutton));
    nog = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->nog_iden_spinbutton));
    if (!check_differences(app, nrdiff, nadiff))
        return;

    args = fug_args_new(get_handler(app));
    fug_args_add(args, "set");
    fug_args_add(args, "%d", nrdiff);
    fug_args_add(args, "%d", nadiff);
    fug_args_add(args, "-c");
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->default_iden_checkbutton)) == TRUE) {
        fug_args_add(args, "-h");
        fug_args_add(args, "-e");
        fug_args_add(args, "-m");
        fug_args_add(args, "%d", nog);
    } else {
        if (lags > 0) {
            fug_args_add(args, "-l");
            fug_args_add(args, "%d", lags);
        }
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->with_mdt_iden_checkbutton)) == FALSE) {
            fug_args_add(args, "-e");
            fug_args_add(args, "-m");
            fug_args_add(args, "%d", nog);
        }
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->no_level_iden_checkbutton)) == FALSE)
            fug_args_add(args, "-h");
    }

    /* fug joins the plots in <input name>.pdf, with landscape pages for
     * monthly or long series */
    graph = g_strconcat(get_handler(app), "_fug.pdf", NULL);
    run_and_view(app, args, graph);
    g_free(graph);
}

void no_file_message(const gchar *filename, gpointer user_data)
{
    AppWidgets *app = (AppWidgets*)user_data;

    show_error(app, "Error: Can not open the file\n%s\n\nHint: run fug first (options One or Set).",
               filename);
}
