/* model_spec.h */
#ifndef MODEL_SPEC_H
#define MODEL_SPEC_H

#include <gtk/gtk.h>
#include "fue_context.h"

void on_series_name_changed(GtkEntry *entry, FueContext *ctx);
void on_freq_changed(GtkComboBox *combo, FueContext *ctx);
void on_refactor_changed(GtkSpinButton *spin, FueContext *ctx);
void update_individual_factors_sensitivity(FueContext *ctx);
void sync_individual_factors_from_ui(FueContext *ctx);
void sync_mean_from_ui(FueContext *ctx);
void sync_differences_from_ui(FueContext *ctx);
void sync_boxcox_from_ui(FueContext *ctx);
void sync_all_from_ui(FueContext *ctx); /* called before saving .inp */
void update_ui_from_model(FueContext *ctx);   /* new function */
void populate_all_treeviews(FueContext *ctx);

#endif
