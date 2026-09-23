/*
 * fufgui.h -- la ventana de fuf.
 *
 * POR QUE TIENE VENTANA PROPIA Y NO UNA PESTAÑA.
 *
 * Estaba dentro de fue_gui, y ahi estaba estrecha: una prevision tiene
 * interfaz propia --origen, horizonte, bandas, el grafico y la tabla-- que no
 * cabe comoda al lado de la especificacion de un modelo. Y sobre todo, fuf ES
 * OTRO MOTOR: el taller tiene un GUI por motor y la madre orquesta. Este es
 * el que faltaba.
 *
 * LO QUE NO SE LLEVA CONSIGO ES LA DECISION. Que prever lo dice la madre,
 * porque es ella la que sabe cual es el modelo elegido de cada serie y que
 * ventanas hay declaradas. Aqui se prevé lo que llega por la linea de
 * ordenes, igual que fue_gui estima lo que le mandan.
 */

#ifndef FUFGUI_H
#define FUFGUI_H

#include <gtk/gtk.h>

#include "engine.h"
#include "outfcst.h"
#include "preview.h"
#include "proyecto.h"

#define FG_RUTA 1024

typedef struct Fuf {
    GtkWidget *ventana;
    GtkWidget *l_modelo;       /* que se esta previendo                   */
    GtkWidget *horizonte;      /* cuantos periodos                        */
    GtkWidget *b_prever;
    GtkWidget *b_grafico;
    GtkWidget *salida;         /* el .out ENTERO, que es el informe       */
    GtkWidget *consola;        /* la orden y lo que el motor va diciendo  */
    GtkWidget *estado;

    char       inp[FG_RUTA];   /* el .pre o .inp del modelo               */
    char       dir[FG_RUTA];   /* donde corren los motores: SU directorio */
    char       base[240];      /* el nombre sin extension                 */
    char       prev[256];      /* "forecast_<base>" -- 16 mas, que caben  */

    EngineJob *job;
} Fuf;

GtkWidget *fuf_ventana_nueva( GtkApplication *app, Fuf *f );
void       fuf_di( Fuf *f, const char *fmt, ... ) G_GNUC_PRINTF( 2, 3 );
void       fuf_consola( Fuf *f, const char *fmt, ... ) G_GNUC_PRINTF( 2, 3 );
void       fuf_trae_out( Fuf *f );
int        fuf_pon_horizonte( const char *inp, int h, char *why, size_t n );

#endif
