/*
 * gui.h -- el contexto de mtram.
 */

#ifndef DRTRAN_GUI_H
#define DRTRAN_GUI_H

#include <gtk/gtk.h>
#include "series.h"
#include "netfile.h"
#include "slots.h"
#include "verdict.h"
#include "outdiag.h"
#include "outfcst.h"

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

/* La pantalla del modelo: el .cns. La tabla de slots ES la forma del modelo
 * --tantos omega como diga s, tantos phi como el .pre deje libres-- asi que la
 * pantalla no es un editor de texto: es una vista de esa tabla.          */
typedef struct {
    GtkWidget *lista;         /* los parametros, uno por linea              */
    GtkWidget *cuenta;        /* cuantos hay, cuantos libres, y los avisos  */

    SlotTable  st;
    gboolean   vale;
    gchar     *path;
} Modelo;

/* La pantalla de estimacion: la unica que LANZA EL MOTOR como programa. */
typedef struct {
    GtkWidget *boton;         /* Estimar                                    */
    GtkWidget *que;           /* que se va a estimar, antes de estimarlo    */
    GtkWidget *orden;         /* la orden entera, copiable                  */
    GtkWidget *desenlace;     /* como acabo el optimizador, con nombre      */
    GtkWidget *salida;        /* lo que dijo el motor, entero               */
    GtkWidget *c_diag, *c_resta, *c_traza;

    gboolean   corriendo;
    gboolean   diagonal;      /* -0 : sin transferencia (homologacion)      */
    gboolean   cast_resta;    /* -S : el cast antiguo                       */
    gboolean   traza;         /* -v                                         */
    gchar     *out_path;
    VerdictInfo v;
} Estima;

/* La pantalla de diagnosis. Los residuos son los de ESTA corrida: TASTE tenia
 * una sola ranura 'RESIDUOS' (TASTECTV.PAS:475) y por eso no se podian comparar
 * dos modelos. Aqui cada .out trae los suyos.                            */
typedef struct {
    GtkWidget *lista;         /* los contrastes, uno por linea              */
    GtkWidget *veredicto;     /* que hay que hacer con esto                 */

    Diagnosis  d;
    gboolean   vale;
    gchar     *path;          /* el .out del que salio                      */
} Diag;

/* La pantalla de prevision y evaluacion.
 *
 * Las bandas de la prevision son TEORICAS; la evaluacion fuera de muestra es
 * EMPIRICA. La pantalla existe para no dejar confundirlas -- y para poder
 * comparar dos modelos, que es lo que TASTE no podia hacer.            */
typedef struct {
    GtkWidget *lista;         /* el error por horizonte                     */
    GtkWidget *texto;         /* la prevision y lo que significa            */
    GtkWidget *c_prever, *c_eval, *s_hor, *s_win;

    Forecast   f;
    gboolean   vale;

    gboolean   prever, evaluar;
    int        horizonte;     /* -f L                                       */
    int        ventana;       /* -estwin E, 0 = sin ventana                 */

    OfEval     ref;           /* la evaluacion guardada, para comparar      */
    gboolean   tiene_ref;
    char       ref_que[80];
} Prev;

typedef struct {
    GtkWidget *ventana_p;     /* la ventana principal                       */
    GtkWidget *lista;         /* las series, en orden                       */
    GtkWidget *ventana;       /* la ventana muestral comun                  */
    GtkWidget *compat;        /* la compatibilidad de operadores            */
    GtkWidget *estado;        /* la barra de abajo                          */
    Conjunto   c;             /* las series cargadas                        */
    Red        red;           /* la pantalla de la red                      */
    Ident      id;            /* la pantalla de identificacion              */
    Modelo     mod;           /* la pantalla del modelo                     */
    Estima     est;           /* la pantalla de estimacion                  */
    Diag       dia;           /* la pantalla de diagnosis                   */
    Prev       prev;          /* la pantalla de prevision                   */
} Mtram;

GtkWidget *mtram_window_new(GtkApplication *app, Mtram *m);

/* red.c */
GtkWidget *red_pagina_new(Mtram *m);
void       red_refresca(Mtram *m);

/* modelo.c */
GtkWidget *modelo_pagina_new(Mtram *m);
void       modelo_refresca(Mtram *m);

/* estima.c */
GtkWidget *estima_pagina_new(Mtram *m);
void       estima_refresca(Mtram *m);

/* prevision.c */
GtkWidget *prevision_pagina_new(Mtram *m);
void       prevision_refresca(Mtram *m);
void       prevision_desde(Mtram *m, const char *path);

/* diagnosis.c */
GtkWidget *diagnosis_pagina_new(Mtram *m);
void       diagnosis_refresca(Mtram *m);
void       diagnosis_desde(Mtram *m, const char *path);

/* identifica.c */
GtkWidget *identifica_pagina_new(Mtram *m);
void       identifica_refresca(Mtram *m);

#endif
