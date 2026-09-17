#ifndef FORECAST_TAB_H
#define FORECAST_TAB_H

#include <gtk/gtk.h>
#include "fue_context.h"

GtkWidget* create_forecast_tab(FueContext *ctx);
void load_file_to_editor(FueContext *ctx, const char *filename);
void set_current_inp_from_path(FueContext *ctx, const char *inp_path);

#endif
