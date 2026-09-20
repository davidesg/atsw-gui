#ifndef DATA_HANDLING_H
#define DATA_HANDLING_H

#include <gtk/gtk.h>
#include "fue_context.h"

/* why[nwhy] recibe el motivo si devuelve FALSE. Antes no habia motivo:
 * la barra decia "Failed to load data file." y nada mas.          */
gboolean load_data_file(const char *filename, FueContext *ctx,
                        char *why, size_t nwhy);

/* Abre un fichero que viene de fuera (la interfaz madre) al arrancar. Usa el
 * mismo camino que el chooser del analista: no hay una segunda forma de
 * cargar.                                                               */
void fue_abre_al_arrancar(FueContext *ctx, const char *path);
void on_data_file_selected(GtkFileChooserButton *button, FueContext *ctx);
void set_model_fue(FueContext *ctx);
void on_load_series(GtkToolButton *btn, FueContext *ctx);


#endif
