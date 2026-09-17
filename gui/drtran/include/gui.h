/*
 * gui.h -- el contexto de mtram.
 */

#ifndef DRTRAN_GUI_H
#define DRTRAN_GUI_H

#include <gtk/gtk.h>
#include "series.h"
#include "netfile.h"

#define IDENT_MAX_LAGS 64

/* La pantalla de identificacion de un enlace: la CCF preblanqueada de la
 * salida contra UNA de las entradas, y lo que se lee de ella.            */
typedef struct {
    GtkWidget *combo;         /* contra que entrada                         */
    GtkWidget *lectura;       /* (b, r, s) y la exogeneidad, en palabras    */
    GtkWidget *ecuacion;      /* la ecuacion del modelo                     */

    int        entrada;       /* indice en Conjunto: 1..n-1                 */
    gboolean   vale;          /* hay CCF calculada                          */
    int        nlags, n;
    double     ccf[2 * IDENT_MAX_LAGS + 1];   /* k = i - nlags              */
    double     nu [2 * IDENT_MAX_LAGS + 1];
    double     banda;         /* 2/sqrt(n)                                  */
    double     Q;             /* el portmanteau de Hosking                  */
    int        df;
} Ident;

/* La pantalla de la red: el .dag. El motor resuelve el sistema por recursion
 * en orden topologico, asi que la red decide si el modelo se puede estimar.
 * Un ciclo no es un error de sintaxis: es un sistema simultaneo, y ese es de
 * drvarma, no de aqui.                                                    */
typedef struct {
    GtkWidget *lista;         /* los enlaces                                */
    GtkWidget *veredicto;     /* aciclica y su orden, o el ciclo por su nombre */

    NetLink    lnk[NET_MAX_LINK];
    int        n;
    gchar     *path;          /* de donde se leyo, o donde se guardo        */
} Red;

typedef struct {
    GtkWidget *ventana_p;     /* la ventana principal                       */
    GtkWidget *lista;         /* las series, en orden                       */
    GtkWidget *ventana;       /* la ventana muestral comun                  */
    GtkWidget *compat;        /* la compatibilidad de operadores            */
    GtkWidget *estado;        /* la barra de abajo                          */
    Conjunto   c;             /* las series cargadas                        */
    Red        red;           /* la pantalla de la red                      */
    Ident      id;            /* la pantalla de identificacion              */
} Mtram;

GtkWidget *mtram_window_new(GtkApplication *app, Mtram *m);

/* red.c */
GtkWidget *red_pagina_new(Mtram *m);
void       red_refresca(Mtram *m);

/* identifica.c */
GtkWidget *identifica_pagina_new(Mtram *m);
void       identifica_refresca(Mtram *m);

#endif
