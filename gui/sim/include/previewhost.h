/*
 * previewhost.h -- lo que sim_gui le da a la ventana de graficos (lib/preview).
 *
 * lib/preview dibuja en pantalla EL MISMO fichero que va al papel: el EPS que
 * escribe lib/ccfplot. Como en drtran_gui, el contrato es un tipo y dos
 * funciones (identifica.c).
 */
#ifndef SIM_PREVIEWHOST_H
#define SIM_PREVIEWHOST_H

#include <gtk/gtk.h>
#include "sim.h"
#include "utils.h"

typedef Sim PreviewApp;

void preview_open_external( PreviewApp *app, const gchar *path );
void preview_show_status( PreviewApp *app, const gchar *format, ... ) G_GNUC_PRINTF(2, 3);

/* «Al cuaderno» en la ventana de graficos, con proyecto y caso (fase P5 de
 * docs/TODO.md): la entrada sale como las de fue y las de drtran_gui, con
 * el caso y la corrida (proyecto.c).                                    */
#define PREVIEW_CUADERNO
gboolean preview_cuaderno_puede( PreviewApp *app );
void     preview_cuaderno( PreviewApp *app, const gchar *path );

#endif
