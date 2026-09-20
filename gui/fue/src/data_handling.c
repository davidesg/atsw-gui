#include "data_handling.h"
#include "fue_globals.h"
#include "file_io.h"
#include "model_spec.h"
#include "utils.h"
#include "inpcheck.h"
#include "datos.h"
#include <glib/gstdio.h>
#include <ctype.h>
#include <string.h>

/* LOS DATOS ENTRAN POR lib/datos, QUE ES LA UNICA PUERTA.
 *
 * Aqui habia un cuarto lector del mismo formato, y los cuatro discrepaban: el
 * de fug APLANA todas las columnas en un vector y este se queda con la
 * primera, asi que el MISMO FICHERO daba dos series distintas
 * (INVENTARIO-madre.md §1). La decision --la 1 es la serie, las demas
 * regresores-- esta razonada en DISENO-madre.md §4 y vive en lib/datos.
 *
 * Lo que se gana aqui, ademas de dejar de discrepar:
 *
 *   una cabecera ya no hace fallar la carga -- y da NOMBRE a la serie;
 *   un CSV de verdad se lee, con sus comas;
 *   la coma decimal se acepta;
 *   LA FRECUENCIA Y LA FECHA SALEN DEL FICHERO si el fichero las trae, en vez
 *   de salir siempre del combo (que por defecto dice 12 y año 2000);
 *   y cuando falla, SE DICE POR QUE. Antes era "Failed to load data file."
 *
 * why[nwhy] recibe el motivo si devuelve FALSE.                          */
gboolean load_data_file(const char *filename, FueContext *ctx,
                        char *why, size_t nwhy) {
    DtDatos *d;
    DtError  e;
    int      i, j;

    if (why && nwhy) why[0] = '\0';

    d = g_new0(DtDatos, 1);            /* 2 MB: en la pila no cabe */
    if (dt_leer(filename, d, &e) != 0) {
        if (why) dt_error_en(&e, why, nwhy);
        g_free(d);
        return FALSE;
    }

    /* El limite es del GUI, no del formato: Data[] es un vector fijo de 2000.
     * Antes no se comprobaba y se escribia fuera.                        */
    if (d->nobs > (int)(sizeof(Data) / sizeof(Data[0]))) {
        if (why) snprintf(why, nwhy,
            "%d observations: this window holds %d at most",
            d->nobs, (int)(sizeof(Data) / sizeof(Data[0])));
        g_free(d);
        return FALSE;
    }

    DataMat = matrix(0, d->ncol, 1, d->nobs);
    Ts.nobs = d->nobs;

    /* LA FRECUENCIA Y LA FECHA, DEL FICHERO SI LAS TRAE. Si no las trae, del
     * combo -- pero entonces las pone el analista y lo sabe.             */
    if (d->freq > 0) {
        Ts.freq = d->freq;
        gtk_combo_box_set_active(GTK_COMBO_BOX(ctx->freq_combo),
                                 d->freq == 1 ? 0 : d->freq == 4 ? 1 : 2);
    } else {
        int freq_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(ctx->freq_combo));

        Ts.freq = (freq_idx == 0) ? 1 : (freq_idx == 1) ? 4 : 12;
    }
    if (d->anio > 0) {
        Ts.begyear = d->anio;
        Ts.begtime = d->per > 0 ? d->per : 1;
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->start_year_spin), Ts.begyear);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->start_period_spin), Ts.begtime);
    } else {
        Ts.begyear = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ctx->start_year_spin));
        Ts.begtime = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ctx->start_period_spin));
    }
    ts_set_name(NULL);

    /* La columna 1 es la SERIE; las demas, regresores. DataMat va 1..ncol. */
    for (i = 1; i <= d->nobs; i++) {
        for (j = 1; j <= d->ncol; j++) DataMat[j][i] = d->v[j - 1][i - 1];
        Data[i - 1] = d->v[0][i - 1];
    }

    Ts.data = vector(1, Ts.nobs);
    for (i = 1; i <= Ts.nobs; i++) Ts.data[i] = d->v[0][i - 1];

    /* EL NOMBRE, DE LA CABECERA si la hay. El fichero sabe como se llama la
     * serie mejor que su nombre de fichero.                              */
    if (d->tiene_cabecera && d->nombre[0][0])
        gtk_entry_set_text(GTK_ENTRY(ctx->series_name_entry), d->nombre[0]);
    else {
        char *basename = g_path_get_basename(filename);
        char *dot = strrchr(basename, '.');

        if (dot) *dot = '\0';
        gtk_entry_set_text(GTK_ENTRY(ctx->series_name_entry), basename);
        g_free(basename);
    }
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->n_obs_spin), Ts.nobs);

    g_free(d);
    return TRUE;
}

void on_data_file_selected(GtkFileChooserButton *button, FueContext *ctx) {
    char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(button));
    if (!filename) return;

    const char *ext = getExt(filename);
    if (g_strcmp0(ext, ".inp") == 0 || g_strcmp0(ext, ".pre") == 0) {
        /* Se mira antes de leerlo: load_input_fue() se fia del fichero, y
         * uno de previsiones le mete el horizonte donde espera el numero de
         * variables deterministas -- seguia leyendo hasta llevarse el
         * monton por delante. El aviso es el mismo que da el motor.      */
        if (!inp_ok_to_load(ctx->main_window, filename, 0)) {
            gtk_label_set_text(GTK_LABEL(ctx->status_label), "File not loaded.");
            gtk_file_chooser_unselect_all(GTK_FILE_CHOOSER(button));
            g_free(filename);
            return;
        }
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
        char why[256];

        if (load_data_file(filename, ctx, why, sizeof why)) {
            ts_set_name(gtk_entry_get_text(GTK_ENTRY(ctx->series_name_entry)));
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
            /* CON EL MOTIVO. Antes decia "Failed to load data file." y punto:
             * el analista no sabia si era la linea 3 o el fichero entero. */
            char msg[320];

            snprintf(msg, sizeof msg, "Data not loaded%s%s",
                     why[0] ? ": " : ".", why);
            gtk_label_set_text(GTK_LABEL(ctx->status_label), msg);
        }
    }
    g_free(filename);
}

/* Aqui habia una copia COMENTADA de on_data_file_selected, obsoleta: la viva
 * ya trae la comprobacion del .inp y el aviso con motivo. Se borro al adoptar
 * lib/datos, porque un comentario nuevo dentro de ella cerraba el bloque -- C
 * no anida comentarios -- y el resto pasaba a compilarse.                 */

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
