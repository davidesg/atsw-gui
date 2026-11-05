/*
 * sim.h -- el contexto de sim_gui, el GUI de los sistemas simultaneos.
 *
 * docs/DISENO-sim.md. Un solo GUI para VARMA (drvarma) y VEC (drvec), como
 * sima: VARMA o VEC se decide dentro del analisis, no al entrar. En la
 * primera version, solo VARMA.
 *
 * Siete paginas, como drtran_gui, y en el mismo orden donde son hermanas:
 *
 *   1 Series  2 Puerta  3 Identificacion  4 Modelo  5 Estimacion
 *   6 Diagnosis  7 Prevision
 *
 * La nueva es la 2, la PUERTA: el sistema diagonal tiene que reproducir los
 * univariantes, o nada de lo que se construya encima se puede creer.
 *
 * Parte de ficheros de fue YA HECHOS, no de datos crudos: el modelo de cada
 * serie se hace en US (fue); aqui se cruzan. sim_gui no transforma datos.
 */

#ifndef SIM_GUI_H
#define SIM_GUI_H

#include <gtk/gtk.h>

#include "sistema.h"
#include "outvarma.h"
#include "proyecto.h"
#include "engine.h"

/* El directorio de trabajo sin proyecto, bajo la cache del usuario. */
#define SIM_CACHE "sim_gui"

#define SIM_VERDE  "#1a7f37"
#define SIM_AMBAR  "#9a6700"
#define SIM_ROJO   "#b3261e"

enum { PG_SERIES, PG_PUERTA, PG_IDENT, PG_MODELO, PG_ESTIMA, PG_DIAG, PG_PREV, PG_N };

/* La pagina 1: las series, en el orden del sistema. */
typedef struct {
    GtkWidget *lista;
    GtkWidget *ver_ventana;   /* veredicto: la ventana comun               */
    GtkWidget *ver_orden;     /* veredicto: el orden es el de Cholesky     */
    GtkWidget *b_abrir, *b_quitar, *b_subir, *b_bajar;
} PagSeries;

/* La pagina 2: la puerta diagonal. */
typedef struct {
    GtkWidget *b_pasar;       /* Pasar la puerta                           */
    GtkWidget *orden;         /* la orden, copiable                        */
    GtkWidget *lista;         /* la tabla de la puerta                     */
    GtkWidget *ver_puerta;    /* veredicto: pasa, falla, no se ha pasado   */
    GtkWidget *ver_notas;     /* lo que se movio y por que                 */
    GtkWidget *ver_rango;     /* la pregunta de la cointegracion (N1c)     */

    EngineJob *trabajo;
    gboolean   corriendo;
    gboolean   hecha;         /* hay un .out de la puerta para ESTE sistema */
    int        rc;            /* como salio drvarma                        */
    gchar     *out;           /* el .out de la puerta                      */
    gchar     *firma;         /* las rutas del sistema cuando se paso      */
    VarmaOut  *v;             /* lo leido (lib/outvarma), en el monton     */
    int        version;       /* sube con cada puerta leida                */
} PagPuerta;

/* La pagina 3: la identificacion (N2, sima identify_cross). */
typedef struct {
    GtkWidget *rejilla;       /* m x m miniaturas de la CCF de los residuos */
    GtkWidget *enlaces;       /* la caja de las casillas propuestas        */
    GtkWidget *ver_pide;      /* lo que falta (la puerta)                  */
    GtkWidget *ver_lectura;   /* retroalimentacion, una direccion, nada    */
    GtkWidget *ver_sigma;     /* la contemporanea: Sigma completa o no     */
    GtkWidget *b_al_modelo;   /* «Al modelo»                               */
    int        k_max;         /* los retardos que se leen como «cortos»    */
    unsigned char marca[SIS_MAX_SER][SIS_MAX_SER];   /* lo propuesto      */
    int        p_prop;        /* el retardo mas largo propuesto            */
    gboolean   sigma_prop;    /* la contemporanea sale de la banda         */
    gboolean   vale;          /* hay CCF que leer                          */
    int        version;       /* la de la puerta que se leyo; -1 ninguna   */
} PagIdent;

/* La pagina 4: el modelo (N3). Lo que se mandara a drvarma en S4.      */
typedef struct {
    GtkWidget *s_p, *s_q;
    GtkWidget *c_diag;        /* Sigma diagonal                            */
    GtkWidget *enlaces;       /* la caja con una casilla por par ordenado  */
    GtkWidget *c_redet, *c_fixarma, *c_m2, *cb_hessian, *cb_lik;
    GtkWidget *orden;         /* la orden, copiable                        */
    GtkWidget *ver_cuenta;    /* cuantos parametros cruzados               */
    GtkWidget *ver_forma;     /* retroalimentacion o red                   */
    gboolean   leyendo;       /* rellenando los controles: no leerlos      */
} PagModelo;

/* La pagina 5: la estimacion (N3). La unica, con la 2, que LANZA el motor. */
typedef struct {
    GtkWidget *b_estimar, *b_parar;
    GtkWidget *orden;         /* la orden de esta corrida, copiable        */
    GtkWidget *consola;       /* lo que dice drvarma, en vivo              */
    GtkWidget *ver_fin;       /* como acabo                                */
    GtkWidget *ver_donde;     /* a donde va la corrida                     */

    EngineJob *trabajo;
    gboolean   corriendo;
    int        rc;
    gchar     *out;           /* el .out de la corrida (con su extension)  */
    char       corrida[PR_ID];   /* su id en el sistema, con proyecto      */
    char       previa[PR_ID];    /* la anterior: el padre de la siguiente  */
    VarmaOut  *v;             /* lo leido; NULL si no hay corrida          */
    int        version;       /* sube con cada estimacion leida            */
} PagEstima;

/* La pagina 6: la diagnosis (N4). Lee, no calcula: lib/outvarma y
 * lib/outdiag sobre el .out de la corrida.                              */
typedef struct {
    GtkWidget *ver_pide;
    GtkWidget *libreta;
    GtkWidget *ver_lr, *ver_hosking, *ver_jb, *ver_raices, *ver_optim;
    GtkWidget *l_par, *l_res;
    GtkWidget *t_sigma;
    GtkWidget *t_out;
    int        version;
} PagDiag;

/* La pagina 7: la prevision y la evaluacion (N5, N6). drvarma preve en la
 * pasada en que estima, asi que prever es volver a correr EL MISMO modelo
 * con -forecast H: va a <modelo>_prev.* y no toca la estimacion. Evaluar
 * son otras dos corridas con -estwin N --con -estwin, drvarma preve desde
 * el final de la ventana, no de la muestra--: el sistema a <modelo>_eval.*
 * y la vara de medir, el sistema diagonal (los univariantes) con la misma
 * ventana, a <modelo>_diag.*                                            */
typedef struct {
    GtkWidget *s_hor, *c_eval, *s_win, *b_calc;
    GtkWidget *ver_pide, *ver_eval;
    GtkWidget *libreta;
    GtkWidget *t_prev, *l_eval, *t_irf, *t_fevd;
    GtkWidget *graficos;      /* un boton por serie: su grafico           */

    EngineJob *trabajo;
    gboolean   corriendo;
    int        paso;          /* 1 prever, 2 evaluar el sistema, 3 el diagonal */
    gchar     *base;          /* <modelo>_prev, sin extension             */
    VarmaOut  *v;             /* la corrida de prevision                  */
    VarmaOut  *ve;            /* la evaluacion del sistema (-estwin)      */
    VarmaOut  *vd;            /* la del diagonal, con la misma ventana    */
    int        version;       /* la estimacion de la que se previo        */
} PagPrev;

typedef struct {
    GtkWidget *ventana;
    GtkWidget *libro;         /* el cuaderno de las paginas                */
    GtkWidget *estado;        /* la barra de abajo                         */
    GtkWidget *l_modelo;      /* a la derecha: el sistema que hay delante  */
    GtkWidget *avance;        /* mientras corre el motor                   */
    GtkToolItem *tb_estimar, *tb_ver, *tb_diag, *tb_prev;

    Sistema    sis;           /* las series, en orden                      */
    PagSeries  ps;
    PagPuerta  pp;
    PagIdent   pi;
    PagModelo  pm;
    SisModelo  mod;           /* el modelo que se va a estimar             */
    PagEstima  pe;
    PagDiag    pd;
    PagPrev    pv;
    gboolean   recolocando;   /* repintando la lista                       */

    /* EL PROYECTO, si se abrio con --proyecto; y el SISTEMA (un caso con
     * motor drvarma), si con --caso. Sin el, el programa trabaja en la
     * cache del usuario con nombres fijos.                              */
    Proyecto  *proy;
    char       caso[PR_ID];
    char       modelo_ini[PR_ID];
    gboolean   desfasado;     /* algun .pre ya no es el del alta           */
} Sim;

/* proyecto.c */
gboolean sim_proyecto_abre( Sim *m, const char *path, char *why, size_t n );
/* Carga el sistema de --caso: sus entradas EN SU ORDEN, comprobando el
 * sha256 de cada .pre. FALSE con el motivo; si un .pre cambio, carga igual
 * y lo dice en why (desfasado queda puesto).                            */
gboolean sim_caso_carga( Sim *m, char *why, size_t n );
/* La ruta de un artefacto: sin caso, en la cache con nombre fijo
 * («puerta» -> <cache>/sim_gui/puerta); con caso, en el del sistema:
 * <raiz>/_casos/<caso>/work/<caso>_puerta. Crea el directorio. g_free. */
gchar   *sim_artefacto( Sim *m, const char *nombre );

/* ventana.c */
GtkWidget *sim_ventana_new( GtkApplication *app, Sim *m );
void       sim_refresca( Sim *m );
void       sim_estado( Sim *m, const char *fmt, ... ) G_GNUC_PRINTF(2, 3);
void       sim_veredicto( GtkWidget *w, const char *color, const char *fmt, ... )
           G_GNUC_PRINTF(3, 4);
GtkWidget *sim_mono( const char *txt );
/* Un criterio de parada de drvarma (qnewtopt.c), en castellano. */
const char *sim_criterio( const char *c );
/* drvarma, buscado al lado de sim_gui (el arbol, el paquete) antes que en
 * el PATH. SIM_DRVARMA en el entorno lo sustituye (la prueba). g_free.   */
gchar     *sim_hermano( const char *programa );

/* series.c */
GtkWidget *series_pagina_new( Sim *m );
void       series_refresca( Sim *m );
/* Anade un fichero sin dialogo (el cuerpo de Abrir). TRUE si entro.     */
gboolean   series_anade( Sim *m, const char *ruta );

/* puerta.c */
GtkWidget *puerta_pagina_new( Sim *m );
void       puerta_refresca( Sim *m );
/* Lanza drvarma con p = q = 0 y Sigma diagonal. TRUE si arranco.        */
gboolean   puerta_pasar( Sim *m );
/* Lo que la puerta dice de ESTE sistema: 1 pasa, 0 no se ha pasado (o el
 * sistema cambio despues), -1 falla.                                    */
int        puerta_estado( Sim *m );

/* identifica.c */
GtkWidget *identifica_pagina_new( Sim *m );
void       identifica_refresca( Sim *m );
/* Pasa lo marcado al modelo (pagina 4): los enlaces, p y Sigma. */
void       identifica_al_modelo( Sim *m );

/* modelo.c */
GtkWidget *modelo_pagina_new( Sim *m );
void       modelo_refresca( Sim *m );
/* La orden del modelo, legible (los ficheros por su nombre). g_free.   */
gchar     *modelo_orden_texto( Sim *m );

/* estima.c */
GtkWidget *estima_pagina_new( Sim *m );
void       estima_refresca( Sim *m );
/* Lanza drvarma con el modelo de la pagina 4. Pide la puerta superada.
 * TRUE si arranco.                                                      */
gboolean   estima_lanzar( Sim *m );
/* Un sistema ya estimado, sin estimar: su .out en las paginas 5 y 6, y su
 * modelo (p, q, Sigma, enlaces) en la 4. La usa la carga de un sistema.  */
gboolean   estima_desde( Sim *m, const char *out );

/* diagnosis.c */
GtkWidget *diagnosis_pagina_new( Sim *m );
void       diagnosis_refresca( Sim *m );

/* prevision.c */
GtkWidget *prevision_pagina_new( Sim *m );
void       prevision_refresca( Sim *m );
/* Prever (y evaluar, si esta marcado) el sistema estimado. TRUE si arranco. */
gboolean   prevision_calcular( Sim *m );
/* El grafico de la prevision de la serie i, escrito y en la ventana de
 * graficos. TRUE si se pudo.                                             */
gboolean   prevision_grafico( Sim *m, int i );

/* pagina.c: «Ver modelo», la pagina de la serie i (0..n-1) del sistema
 * estimado, con el dibujante de fue (lib/equation). */
gchar     *pagina_ruta( Sim *m, int i );
int        pagina_serie( Sim *m, int i, const char *pdf, char *why, size_t n );
gboolean   pagina_ver( Sim *m, int i );
/* Cuantos items cruzados lleva la ecuacion de la serie i (0: es la de fue
 * tal cual); -1 si no hay sistema estimado. Para la prueba.             */
int        pagina_cruzados( Sim *m, int i );

/* puerta.c: una puerta ya pasada, leida de su .out (la carga de un sistema). */
gboolean   puerta_desde( Sim *m, const char *out );

#endif /* SIM_GUI_H */
