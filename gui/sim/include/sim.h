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
} PagPuerta;

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

#endif /* SIM_GUI_H */
