/*
 * previewhost.h -- lo que mtram le da a la ventana de graficos.
 *
 * lib/preview dibuja en pantalla, con cairo, EL MISMO fichero que se va al
 * papel: interpreta el flujo de contenido que escribe fugdraw. Asi que lo que
 * el analista ve es, letra por letra, lo que va a imprimir. Por eso mtram no
 * tiene un dibujante propio de la CCF: escribe el EPS con lib/ccfplot y se lo
 * pasa a esta ventana.
 *
 * El contrato es corto a proposito: un tipo y dos funciones.
 */

#ifndef MTRAM_PREVIEWHOST_H
#define MTRAM_PREVIEWHOST_H

#include <gtk/gtk.h>

#include "gui.h"
#include "utils.h"            /* getExt(): la extension de un nombre */

typedef Mtram PreviewApp;

void preview_open_external(PreviewApp *app, const gchar *path);
void preview_show_status(PreviewApp *app, const gchar *format, ...)
     G_GNUC_PRINTF(2, 3);

/* «Al cuaderno» en la ventana de graficos, con proyecto y caso (fase P5 de
 * docs/TODO.md): la entrada sale como las de fue, con el caso y la
 * corrida donde fue pone la serie y el modelo (proyecto_gui.c).        */
#define PREVIEW_CUADERNO
gboolean preview_cuaderno_puede(PreviewApp *app);
void     preview_cuaderno(PreviewApp *app, const gchar *path);

#endif /* MTRAM_PREVIEWHOST_H */
