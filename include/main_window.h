#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <gtk/gtk.h>
#include "fue_context.h"

GtkWidget* create_main_window(GtkApplication *app, FueContext *ctx);
void update_model_label(FueContext *ctx);
#endif /* MAIN_WINDOW_H */
