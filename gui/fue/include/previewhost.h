/*
 * previewhost.h -- what the host program gives to the graph window.
 *
 * src/preview.c is a copy of the one in fug (gui/src/preview.c): same file,
 * same drawing. What each program has to supply lives here, as plothost.h
 * does for fugplot.c. When the two are factored into a library this is the
 * header that stays different.
 */

#ifndef PREVIEWHOST_H
#define PREVIEWHOST_H

#include <gtk/gtk.h>
#include "fue_context.h"
#include "utils.h"            /* getExt(): la extension de un nombre       */

/* The state of the application: the window is opened from it and it is
 * handed back when the user asks for the viewer of the system.           */
typedef FueContext PreviewApp;

/* Open path with the viewer of the system (the "External Viewer" button) */
void preview_open_external(PreviewApp *app, const gchar *path);

/* A line for the status bar of the application */
void preview_show_status(PreviewApp *app, const gchar *format, ...) G_GNUC_PRINTF(2, 3);

#endif /* PREVIEWHOST_H */
