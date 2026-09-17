#ifndef PREVIEW_H
#define PREVIEW_H

#include <gtk/gtk.h>
#include "gui.h"

/* Show an EPS or PDF file made by fug in a graph window (the same window is
 * reused, and reloaded, when the file is shown again). Returns FALSE if the
 * file was not drawn by fug 1.14 or later: then use an external viewer. */
gboolean preview_show(AppWidgets *app, const gchar *path);

/* Number of pages of the graph window of path (0 if there is none) */
guint preview_n_pages(const gchar *path);

/* Save the graph window of path as filename; the format (PDF, EPS, PNG or
 * SVG) is given by its extension. PNG, SVG and EPS keep the page shown. */
gboolean preview_save_as(const gchar *path, const gchar *filename, GError **error);

#endif
