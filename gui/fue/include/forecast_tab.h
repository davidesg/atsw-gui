#ifndef FORECAST_TAB_H
#define FORECAST_TAB_H

#include <gtk/gtk.h>
#include "fue_context.h"

GtkWidget* create_forecast_tab(FueContext *ctx);

/* La entrada y el informe, cada uno en su hoja. Cualquiera de los dos puede
   ser NULL: se carga lo que haya.                                        */
void forecast_muestra(FueContext *ctx, const char *inp, const char *out);
void set_current_inp_from_path(FueContext *ctx, const char *inp_path);

#endif
