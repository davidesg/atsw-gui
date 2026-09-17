#ifndef FUG_RUN_H
#define FUG_RUN_H

#include <gtk/gtk.h>
#include "gui.h"

void fug_run_init(const char *argv0);
const gchar *fug_program(void);

gchar *get_workspace(AppWidgets *app);
void set_workspace(AppWidgets *app, const gchar *folder);

GPtrArray *fug_args_new(const gchar *handler);
void fug_args_add(GPtrArray *args, const gchar *format, ...) G_GNUC_PRINTF(2, 3);
gboolean run_fug(AppWidgets *app, const gchar *workspace, GPtrArray *args);

gboolean open_viewer(AppWidgets *app, const gchar *workspace, const gchar *filename);
gboolean open_external(AppWidgets *app, const gchar *path);

void show_error(AppWidgets *app, const gchar *format, ...) G_GNUC_PRINTF(2, 3);
void show_status(AppWidgets *app, const gchar *format, ...) G_GNUC_PRINTF(2, 3);

#endif
