#ifndef PREVIEW_H
#define PREVIEW_H

#include <gtk/gtk.h>
#include "previewhost.h"

/* Show an EPS or PDF file made by the engines in a graph window (the same window is
 * reused, and reloaded, when the file is shown again). Returns FALSE if the
 * file was not drawn by fugdraw: then use an external viewer. */
gboolean preview_show(PreviewApp *app, const gchar *path);

/* Number of pages of the graph window of path (0 if there is none) */
guint preview_n_pages(const gchar *path);

/* UN PIE PARA LA VENTANA DEL GRAFICO: los controles del que la abrio.
 *
 * Existe para el «vistazo» de la interfaz madre: el analista cambia lambda, d
 * y D AL PIE DEL GRAFICO y el dibujo se rehace ahi mismo, sin irse a la
 * herramienta completa. La ventana se reutiliza por ruta --preview_show la
 * recarga-- asi que el pie sobrevive a las recargas y no parpadea.
 *
 * El widget pasa a ser de la ventana. NULL quita el que hubiera. Devuelve
 * FALSE si no hay ventana para esa ruta todavia.                        */
gboolean preview_set_footer(const gchar *path, GtkWidget *footer);

/* Save the graph window of path as filename; the format (PDF, EPS, PNG or
 * SVG) is given by its extension. PNG, SVG and EPS keep the page shown. */
gboolean preview_save_as(const gchar *path, const gchar *filename, GError **error);

#endif
