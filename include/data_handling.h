#ifndef DATA_HANDLING_H
#define DATA_HANDLING_H

#include <gtk/gtk.h>
#include "fue_context.h"

gboolean load_data_file(const char *filename, FueContext *ctx);
void on_data_file_selected(GtkFileChooserButton *button, FueContext *ctx);
void set_model_fue(FueContext *ctx);
void on_load_series(GtkToolButton *btn, FueContext *ctx);


#endif
