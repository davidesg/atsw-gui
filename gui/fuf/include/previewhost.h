/*
 * previewhost.h -- lo que fuf_gui le da a la ventana de gráficos.
 *
 * El contrato de lib/preview es corto a propósito: UN TIPO Y DOS FUNCIONES, y
 * cada programa que quiera enseñar un gráfico pone las suyas. Por eso la
 * ventana de gráficos es la misma en la madre, en drtran_gui y aquí sin que
 * ninguno de los tres sepa de los otros.
 */

#ifndef FUF_PREVIEWHOST_H
#define FUF_PREVIEWHOST_H

#include <gtk/gtk.h>

typedef struct Fuf PreviewApp;

void preview_open_external(PreviewApp *app, const gchar *path);
void preview_show_status(PreviewApp *app, const gchar *format, ...)
     G_GNUC_PRINTF(2, 3);

/* getExt(): lib/preview la usa para decidir el formato al guardar. Viene de
 * lib/utils/ext.c, que no arrastra nada.                               */
const char *getExt(const char *fspec);

#endif /* FUF_PREVIEWHOST_H */
