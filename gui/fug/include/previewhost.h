/*
 * previewhost.h -- what fug gives to the graph window.
 *
 * lib/preview/preview.c is shared with the GUI of fue: same file, same
 * drawing, same zoom and glass. What each program has to supply lives here,
 * as plothost.h does for fugplot.c.
 *
 * For fug the three are trivial: its own AppWidgets is the context, and
 * show_status() and open_external() already exist with the right shape --
 * only the names differ.
 */

#ifndef PREVIEWHOST_H
#define PREVIEWHOST_H

#include <gtk/gtk.h>
#include "gui.h"              /* AppWidgets                                */
#include "fug_run.h"          /* show_status(), open_external()            */
#include "data_load.h"       /* getExt(): la extension de un nombre       */

typedef AppWidgets PreviewApp;

#define preview_show_status    show_status
#define preview_open_external  open_external

#endif /* PREVIEWHOST_H */
