/*
 * barra.h -- la barra de herramientas de los GUIs de ATSW.
 *
 * La estreno fue_gui (#40): iconos propios con su texto debajo, botones
 * partidos cuyo clic hace lo habitual y cuya flecha abre el resto, y
 * Ctrl+tecla. Vive aqui para que drtran_gui tenga LA MISMA, no una copia
 * (docs/DISENO-barra-drtran.md).
 *
 * datos es lo que recibe cada cb: el contexto del programa.
 */

#ifndef AT_BARRA_H
#define AT_BARRA_H

#include <gtk/gtk.h>
#include "iconos.h"

/* Un boton con icono y rotulo al final de la barra; globo es el porque. */
GtkToolItem *at_boton( GtkWidget *barra, const char *rotulo, const char *icono,
                       const char *respaldo, const char *globo, GCallback cb,
                       gpointer datos );

/* El boton partido: el clic hace cb, la flecha abre menu. */
GtkToolItem *at_partido( GtkWidget *barra, const char *rotulo, const char *icono,
                         const char *respaldo, GtkWidget *menu, GCallback cb,
                         gpointer datos );

/* Una entrada de menu con su icono, al final de menu. ref da el color. */
GtkWidget *at_entrada( GtkWidget *menu, const char *texto, const char *icono,
                       const char *respaldo, GtkWidget *ref, GCallback cb,
                       gpointer datos );

/* Ctrl+tecla pulsa el boton, si esta encendido. */
void at_atajo( GtkAccelGroup *g, guint tecla, GtkToolItem *b );

#endif
