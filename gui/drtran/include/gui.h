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
#include "engine.h"
#include "outdiag.h"
#include "outfcst.h"

#define IDENT_MAX_LAGS 64

/* La pantalla de identificacion.
 *
 * UNA FILA POR ENTRADA CANDIDATA, no por enlace. Identificar es precisamente
 * decidir CUALES merecen estar en la red, asi que se calcula la CCF
 * preblanqueada de la salida contra TODAS, y se comparan de un vistazo. Con un
 * combo habia que ir una por una recordando de memoria lo que decia la
 * anterior -- y eso es justo la decision que se toma aqui.               */
typedef struct {
    int      serie;           /* 1..n-1: la entrada, en indice de Conjunto  */
    gboolean vale;
    int      nlags, n;
    double   ccf[2 * IDENT_MAX_LAGS + 1];   /* k = i - nlags               */
    double   nu [2 * IDENT_MAX_LAGS + 1];
    double   banda;           /* 2/sqrt(n)                                  */
    double   Q;               /* el portmanteau de Hosking                  */
    int      df;

    /* lo que se lee del grafico */
    int      b, s, kmax, ultimo, neg;
} IdentUno;

typedef struct {
    GtkWidget *lista;
    GtkWidget *ver_tran;      /* veredicto: la transferencia                */
    GtkWidget *ver_exo;       /* veredicto: la exogeneidad                  */
    GtkWidget *s_lags;

    IdentUno   u[GUI_MAX_SER];
    int        nent;          /* cuantas entradas candidatas                */
    int        marcada;       /* indice en u[], -1 si ninguna               */
    int        nlags;         /* el que se pide; 0 = el que elige el motor   */
} Ident;

/* La pantalla de la red: el .dag. El motor resuelve el sistema por recursion
 * en orden topologico, asi que la red decide si el modelo se puede estimar.
 * Un ciclo no es un error de sintaxis: es un sistema simultaneo, y ese es de
 * drvarma, no de aqui.                                                    */
typedef struct {
    GtkWidget *lista;         /* los enlaces                                */
    GtkWidget *ver_topo;      /* veredicto: aciclica y su orden, o el ciclo */
    GtkWidget *ver_forma;     /* veredicto: red o estrella, y las sueltas   */

    NetLink    lnk[NET_MAX_LINK];
    int        n;
    gchar     *path;          /* de donde se leyo, o donde se guardo        */
} Red;

/* La pantalla del modelo: el .cns. La tabla de slots ES la forma del modelo
 * --tantos omega como diga s, tantos phi como el .pre deje libres-- asi que la
 * pantalla no es un editor de texto: es una vista de esa tabla.          */
typedef struct {
    GtkWidget *lista;         /* los parametros, en un ARBOL por grupos     */
    GtkWidget *ver_cuenta;    /* veredicto: cuantos hay y cuantos libres    */
    GtkWidget *ver_ojo;       /* veredicto: LO QUE HAY QUE MIRAR            */
    GtkWidget *c_solo;        /* solo lo restringido                        */

    gboolean   solo;          /* la casilla                                 */

    SlotTable  st;
    gboolean   vale;
    int        perdidas;      /* restricciones que no cupieron al rehacer   */
    gboolean   orden_cambio;  /* las series se movieron: los nombres ya no
                                 significan lo mismo                        */
    gchar     *path;
} Modelo;

/* La pantalla de estimacion: la unica que LANZA EL MOTOR como programa. */
typedef struct {
    GtkWidget *boton;         /* Estimar                                    */
    GtkWidget *b_parar;       /* Detener                                    */
    GtkWidget *orden;         /* la orden, en una linea COPIABLE            */
    GtkWidget *titulo;        /* el estado, encima de la salida             */
    GtkWidget *barra;         /* en PULSO: no se puede saber el avance      */
    GtkWidget *salida;        /* lo que dice el motor, EN VIVO              */
    GtkWidget *ver_fin;       /* veredicto: como acabo                      */
    GtkWidget *ver_que;       /* veredicto: que se estima                   */
    GtkWidget *motor;         /* que drtran, y de cuando                    */
    GtkWidget *c_diag;        /* Diagonal: es un MODO, no una opcion        */

    EngineJob *trabajo;       /* la corrida en marcha, para poder pararla   */
    guint      pulso;         /* el temporizador de la barra                */
    gboolean   corriendo;
    gboolean   diagonal;      /* -0 : sin transferencia (homologacion)      */
    gboolean   cast_resta;    /* -S : el cast antiguo                       */
    gboolean   traza;         /* -v                                         */

    /* Lo que el estimador MANTIENE del .pre en vez de reestimarlo al juntar
     * las ecuaciones. Vive aqui y no en Modelo porque dice COMO SE ESTIMA,
     * no QUE ES EL MODELO -- igual que el cast. Pero cambia la cuenta de
     * parametros, asi que la pagina Modelo la lee de aqui.             */
    gboolean   fix_N;         /* -N  el ARMA del ruido de la SALIDA         */
    gboolean   fix_X;         /* -X  el ARMA de las ENTRADAS                */
    gboolean   fix_D;         /* -D  los deterministas de la salida         */
    gboolean   fix_E;         /* -E  los deterministas de las entradas      */
    gboolean   fix_M;         /* -M  las medias                             */
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
    GtkWidget *ver_ventana;   /* veredicto: la ventana comun, UNA linea     */
    GtkWidget *ver_oper;      /* veredicto: los operadores, UNA linea       */
    GtkWidget *estado;        /* la barra de abajo                          */
    Conjunto   c;             /* las series cargadas                        */
    gboolean   recolocando;   /* repintando la lista: no leer su orden      */
    Red        red;           /* la pantalla de la red                      */
    Ident      id;            /* la pantalla de identificacion              */
    Modelo     mod;           /* la pantalla del modelo                     */
    Estima     est;           /* la pantalla de estimacion                  */
    Diag       dia;           /* la pantalla de diagnosis                   */
    Prev       prev;          /* la pantalla de prevision                   */
} Mtram;

GtkWidget *mtram_window_new(GtkApplication *app, Mtram *m);

/* ------------------------------------------------------------------------ */
/* Presentacion, compartida por las paginas                                  */
/*                                                                           */
/* Un VEREDICTO es una linea de altura FIJA con un punto de color. Existe    */
/* porque los marcos que crecian con los datos dejaban la lista en dos filas */
/* --y en Red, ademas, daban saltos mientras se editaba--. El detalle se     */
/* pide con un panel; el veredicto esta siempre. Ver DISENO-interfaz.md.     */
/* ------------------------------------------------------------------------ */

#define MT_VERDE  "#1a7f37"
#define MT_AMBAR  "#9a6700"
#define MT_ROJO   "#b3261e"

void mtram_verdicto(GtkWidget *w, const char *color, const char *fmt, ...)
     G_GNUC_PRINTF(3, 4);

GtkWidget *mtram_popover(GtkWidget *ancla, const char *txt);
void       mtram_popover_mostrar(GtkWidget *ancla, const char *txt);

/* Para un panel que no es solo texto. *caja recibe la caja que hay que llenar;
 * al acabar, mtram_popover_popup(pop) -- y NO gtk_popover_popup a secas: ver
 * el porque en main_window.c.                                            */
GtkWidget *mtram_popover_caja(GtkWidget *ancla, GtkWidget **caja);
void       mtram_popover_popup(GtkWidget *pop);

/* Texto de ANCHO FIJO. Casi todo lo que va en estos paneles son tablas hechas
 * con espacios: con fuente proporcional se descuadran y dejan de leerse.  */
GtkWidget *mtram_mono(const char *txt);

/* red.c */
GtkWidget *red_pagina_new(Mtram *m);
void       red_refresca(Mtram *m);
/* El dialogo de un enlace, para poder tocar (b,r,s) desde la pagina Modelo:
 * la estructura de la transferencia se decide alli tanto como aqui.     */
gboolean   red_edita_enlace(Mtram *m, int k);

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
