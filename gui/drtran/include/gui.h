/*
 * gui.h -- el contexto de mtram.
 */

#ifndef DRTRAN_GUI_H
#define DRTRAN_GUI_H

#include <gtk/gtk.h>
#include "series.h"

typedef struct {
    GtkWidget *ventana_p;     /* la ventana principal                       */
    GtkWidget *lista;         /* las series, en orden                       */
    GtkWidget *ventana;       /* la ventana muestral comun                  */
    GtkWidget *compat;        /* la compatibilidad de operadores            */
    GtkWidget *estado;        /* la barra de abajo                          */
    Conjunto   c;             /* las series cargadas                        */
} Mtram;

GtkWidget *mtram_window_new(GtkApplication *app, Mtram *m);

#endif
