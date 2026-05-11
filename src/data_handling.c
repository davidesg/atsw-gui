#include "data_handling.h"
#include "fue_globals.h"
#include "file_io.h"
#include "model_spec.h"
#include "utils.h"
#include <glib/gstdio.h>
#include <ctype.h>
#include <string.h>

gboolean load_data_file(const char *filename, FueContext *ctx) {
    FILE *f = NULL;
#ifdef _WIN32
    wchar_t *wpath = g_utf8_to_utf16(filename, -1, NULL, NULL, NULL);
    if (wpath) {
        f = _wfopen(wpath, L"r");
        g_free(wpath);
    }
#else
    f = fopen(filename, "r");
#endif
    if (!f) return FALSE;

    int n_cols = 0, n_rows = 0;
    char line[8192];
    while (fgets(line, sizeof(line), f)) {
        if (n_rows == 0) {
            char *p = line;
            int in_field = 0;
            while (*p) {
                if (!isspace((unsigned char)*p) && !in_field) {
                    in_field = 1;
                    n_cols++;
                } else if (isspace((unsigned char)*p)) {
                    in_field = 0;
                }
                p++;
            }
        }
        n_rows++;
    }
    if (n_rows < 2 || n_cols == 0) {
        fclose(f);
        return FALSE;
    }

    DataMat = matrix(0, n_cols, 1, n_rows);
    Ts.nobs = n_rows;

    int freq_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(ctx->freq_combo));
    Ts.freq = (freq_idx == 0) ? 1 : (freq_idx == 1) ? 4 : 12;
    Ts.begyear = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ctx->start_year_spin));
    Ts.begtime = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ctx->start_period_spin));
    Ts.name = NULL;

    rewind(f);
    for (int i = 1; i <= n_rows; i++) {
        for (int j = 1; j <= n_cols; j++) {
            if (fscanf(f, "%lf", &DataMat[j][i]) != 1) {
                free_matrix(DataMat, 0, n_cols, 1, n_rows);
                fclose(f);
                return FALSE;
            }
            if (j == 1) Data[i-1] = DataMat[j][i];
        }
    }
    fclose(f);

    Ts.data = vector(1, Ts.nobs);
    for (int i = 1; i <= Ts.nobs; i++) {
        Ts.data[i] = DataMat[1][i];
    }

    char *basename = g_path_get_basename(filename);
    char *dot = strrchr(basename, '.');
    if (dot) *dot = '\0';
    gtk_entry_set_text(GTK_ENTRY(ctx->series_name_entry), basename);
    g_free(basename);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->n_obs_spin), Ts.nobs);

    return TRUE;
}

void on_data_file_selected(GtkFileChooserButton *button, FueContext *ctx) {
    char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(button));
    if (!filename) return;

    const char *ext = getExt(filename);
    if (g_strcmp0(ext, ".inp") == 0 || g_strcmp0(ext, ".pre") == 0) {
        load_input_fue(filename);
        update_ui_from_model(ctx);
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "Model loaded from file.");
        gtk_widget_set_sensitive(ctx->btn_run, TRUE);
        /* También establecer workspace y input name a partir del archivo .inp */
        char *dir = g_path_get_dirname(filename);
        char *basename = g_path_get_basename(filename);
        char *dot = strrchr(basename, '.');
        if (dot) *dot = '\0';
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(ctx->workspace_file_chooser), dir);
        gtk_entry_set_text(GTK_ENTRY(ctx->input_name_entry), basename);
        g_free(basename);
        g_free(dir);
    } else {
        if (load_data_file(filename, ctx)) {
            Ts.name = g_strdup(gtk_entry_get_text(GTK_ENTRY(ctx->series_name_entry)));
            Ts.refactor = 1.0;
            Tm.boxlam = 1.0;
            Tm.boxm = 1.0;
            Tm.nrdiff = 0;
            Tm.nadiff = 0;
            Tm.Imu = 0;
            Tm.mu = 0.0;
            if (Ts.freq > 1) {
                Tm.ifadf = ivector(0, Ts.freq/2);
                for (int i = 0; i <= Ts.freq/2; i++) Tm.ifadf[i] = 0;
            }
            NdetVar = 0;
            NopArr = NopAra = NopMar = NopMaa = 0;
            NumAr2f = NumMa2f = 0;
            update_ui_from_model(ctx);
            gtk_label_set_text(GTK_LABEL(ctx->status_label), "Data loaded successfully.");
            gtk_widget_set_sensitive(ctx->btn_run, TRUE);

            /* Establecer workspace al directorio del archivo de datos */
            char *dir = g_path_get_dirname(filename);
            gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(ctx->workspace_file_chooser), dir);
            g_free(dir);
            /* Establecer input name como el nombre base sin extensión */
            char *basename = g_path_get_basename(filename);
            char *dot = strrchr(basename, '.');
            if (dot) *dot = '\0';
            gtk_entry_set_text(GTK_ENTRY(ctx->input_name_entry), basename);
            g_free(basename);
        } else {
            gtk_label_set_text(GTK_LABEL(ctx->status_label), "Failed to load data file.");
        }
    }
    g_free(filename);
}

/*
void on_data_file_selected(GtkFileChooserButton *button, FueContext *ctx) {
    char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(button));
    if (!filename) return;

    const char *ext = getExt(filename);
    if (g_strcmp0(ext, ".inp") == 0 || g_strcmp0(ext, ".pre") == 0) {
        load_input_fue(filename);
        update_ui_from_model(ctx);
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "Model loaded from file.");
        gtk_widget_set_sensitive(ctx->btn_run, TRUE);
    } else {
        if (load_data_file(filename, ctx)) {
            Ts.name = g_strdup(gtk_entry_get_text(GTK_ENTRY(ctx->series_name_entry)));
            Ts.refactor = 1.0;
            Tm.boxlam = 1.0;
            Tm.boxm = 1.0;
            Tm.nrdiff = 0;
            Tm.nadiff = 0;
            Tm.Imu = 0;
            Tm.mu = 0.0;
            if (Ts.freq > 1) {
                Tm.ifadf = ivector(0, Ts.freq/2);
                for (int i = 0; i <= Ts.freq/2; i++) Tm.ifadf[i] = 0;
            }
            NdetVar = 0;
            NopArr = NopAra = NopMar = NopMaa = 0;
            NumAr2f = NumMa2f = 0;
            update_ui_from_model(ctx);
            gtk_label_set_text(GTK_LABEL(ctx->status_label), "Data loaded successfully.");
            gtk_widget_set_sensitive(ctx->btn_run, TRUE);
        } else {
            gtk_label_set_text(GTK_LABEL(ctx->status_label), "Failed to load data file.");
        }
    }
    g_free(filename);
}

*/

void on_load_series(GtkToolButton *btn, FueContext *ctx) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Open data file",
                                                    GTK_WINDOW(ctx->main_window),
                                                    GTK_FILE_CHOOSER_ACTION_OPEN,
                                                    "_Cancel", GTK_RESPONSE_CANCEL,
                                                    "_Open", GTK_RESPONSE_ACCEPT,
                                                    NULL);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        /* Simulate file selection by setting the data file chooser button */
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(ctx->data_file_chooser), filename);
        /* Then call the existing handler */
        on_data_file_selected(GTK_FILE_CHOOSER_BUTTON(ctx->data_file_chooser), ctx);
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}


void set_model_fue(FueContext *ctx) {
    update_ui_from_model(ctx);
}
