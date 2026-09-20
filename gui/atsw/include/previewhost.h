/*
 * previewhost.h -- lo que la madre le da a la ventana de graficos.
 *
 * El contrato de lib/preview es corto a proposito: UN TIPO Y DOS FUNCIONES, y
 * cada programa que quiera enseñar un grafico pone las suyas. Por eso la
 * ventana de graficos es la misma en drtran_gui y aqui sin que ninguno de los
 * dos sepa del otro.
 *
 * lib/preview dibuja con cairo EL MISMO fichero que se va al papel --interpreta
 * el flujo de contenido que escribe fugdraw-- asi que lo que se ve es, letra
 * por letra, lo que se va a imprimir.
 */

#ifndef ATSW_PREVIEWHOST_H
#define ATSW_PREVIEWHOST_H

#include <gtk/gtk.h>

#include "atsw.h"

typedef Atsw PreviewApp;

void preview_open_external(PreviewApp *app, const gchar *path);
void preview_show_status(PreviewApp *app, const gchar *format, ...)
     G_GNUC_PRINTF(2, 3);

/* getExt(): lib/preview la usa para decidir el formato al guardar. La de
 * lib/utils arrastra GTK y el contexto del GUI de fue, asi que aqui va la
 * misma funcion, que son cuatro lineas.                                 */
const char *getExt(const char *fspec);

#endif /* ATSW_PREVIEWHOST_H */
